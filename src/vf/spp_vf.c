#include <arpa/inet.h>
#include <getopt.h>

#include <rte_eth_ring.h>
#include <rte_eth_vhost.h>
#include <rte_memzone.h>

#include "spp_vf.h"
#include "ringlatencystats.h"
#include "classifier_mac.h"
#include "spp_forward.h"

/* define */
#define SPP_CORE_STATUS_CHECK_MAX 5
#define SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL 1000000

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/* add below */

	SPP_LONGOPT_RETVAL_CONFIG,
};

/* struct */
struct startup_param {
	uint64_t cpu;
};

struct patch_info {
	int	use_flg;
	int	dpdk_port;
	struct spp_config_mac_table_element *mac_info;
	struct spp_core_port_info *rx_core;
	struct spp_core_port_info *tx_core;
};

struct if_info {
	int num_nic;
	int num_vhost;
	int num_ring;
	struct patch_info nic_patchs[RTE_MAX_ETHPORTS];
	struct patch_info vhost_patchs[RTE_MAX_ETHPORTS];
	struct patch_info ring_patchs[RTE_MAX_ETHPORTS];
};

static struct spp_config_area	g_config;
static struct startup_param	g_startup_param;
static struct if_info		g_if_info;
static struct spp_core_info	g_core_info[SPP_CONFIG_CORE_MAX];

static char config_file_path[PATH_MAX];

/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, APP, "Usage: %s [EAL args] -- [--config CONFIG_FILE_PATH]\n"
			" --config CONFIG_FILE_PATH: specific config file path\n"
			"\n"
			, progname);
}

/*
 * Set RING PMD
 */
static int
add_ring_pmd(int ring_id)
{
	struct rte_ring *ring;
	int ring_port_id;

	/* look up ring, based on user's provided id*/
	ring = rte_ring_lookup(get_rx_queue_name(ring_id));
	if (unlikely(ring == NULL)) {
		RTE_LOG(ERR, APP,
			"Cannot get RX ring - is server process running?\n");
		return -1;
	}

	/* create ring pmd*/
	ring_port_id = rte_eth_from_ring(ring);
	RTE_LOG(DEBUG, APP, "ring port id %d\n", ring_port_id);

	return ring_port_id;
}

/*
 * Set VHOST PMD
 */
static int
add_vhost_pmd(int index)
{
	struct rte_eth_conf port_conf = {
		.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
	};
	struct rte_mempool *mp;
	uint8_t vhost_port_id;
	int nr_queues = 1;
	const char *name;
	char devargs[64];
	char *iface;
	uint16_t q;
	int ret;
#define NR_DESCS 128

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (unlikely(mp == NULL)) {
		RTE_LOG(ERR, APP, "Cannot get mempool for mbufs. (name = %s)\n",
				PKTMBUF_POOL_NAME);
		return -1;
	}

	/* eth_vhost0 index 0 iface /tmp/sock0 on numa 0 */
	name = get_vhost_backend_name(index);
	iface = get_vhost_iface_name(index);

	sprintf(devargs, "%s,iface=%s,queues=%d", name, iface, nr_queues);
	ret = rte_eth_dev_attach(devargs, &vhost_port_id);
	if (unlikely(ret < 0)) {
		RTE_LOG(ERR, APP, "rte_eth_dev_attach error. (ret = %d)\n", ret);
		return ret;
	}

	ret = rte_eth_dev_configure(vhost_port_id, nr_queues, nr_queues,
		&port_conf);
	if (unlikely(ret < 0)) {
		RTE_LOG(ERR, APP, "rte_eth_dev_configure error. (ret = %d)\n",
				ret);
		return ret;
	}

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL, mp);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, APP,
				"rte_eth_rx_queue_setup error. (ret = %d)\n",
				ret);
			return ret;
		}
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, APP,
				"rte_eth_tx_queue_setup error. (ret = %d)\n",
				ret);
			return ret;
		}
	}

	/* Start the Ethernet port. */
	ret = rte_eth_dev_start(vhost_port_id);
	if (unlikely(ret < 0)) {
		RTE_LOG(ERR, APP, "rte_eth_dev_start error. (ret = %d)\n", ret);
		return ret;
	}

	RTE_LOG(DEBUG, APP, "vhost port id %d\n", vhost_port_id);

	return vhost_port_id;
}

/*
 * Check core status
 */
static int
check_core_status(enum spp_core_status status)
{
	int cnt;
	for (cnt = 0; cnt < SPP_CONFIG_CORE_MAX; cnt++) {
		if (g_core_info[cnt].type == SPP_CONFIG_UNUSE) {
			continue;
		}
		if (g_core_info[cnt].status != status) {
			/* Status mismatch */
			return -1;
		}
	}
	return 0;
}

/*
 * Wait for core status check
 */
static int
check_core_status_wait(enum spp_core_status status)
{
	int cnt = 0;
	for (cnt = 0; cnt < SPP_CORE_STATUS_CHECK_MAX; cnt++) {
		sleep(1);
		int ret = check_core_status(status);
		if (ret == 0) {
			return 0;
		}
	}

	RTE_LOG(ERR, APP, "Status check time out. (status = %d)\n", status);
	return -1;
}

/*
 * Set core status
 */
static void
set_core_status(enum spp_core_status status)
{
	int core_cnt = 0;
	for(core_cnt = 0; core_cnt < SPP_CONFIG_CORE_MAX; core_cnt++) {
		g_core_info[core_cnt].status = status;
	}
}

/*
 * Process stop
 */
static void
stop_process(int signal) {
	if (unlikely(signal != SIGTERM) &&
			unlikely(signal != SIGINT)) {
		/* Other signals */
		return;
	}

	set_core_status(SPP_CORE_STOP_REQUEST);
}

/*
 * 起動パラメータのCPUのbitmapを数値へ変換
 */
static int
parse_cpu_bit(uint64_t *cpu, const char *cpu_bit)
{
	char *endptr = NULL;
	uint64_t temp;

	temp = strtoull(cpu_bit, &endptr, 0);
	if (unlikely(endptr == cpu_bit) || unlikely(*endptr != '\0')) {
		return -1;
	}

	*cpu = temp;
	RTE_LOG(DEBUG, APP, "cpu = %lu", *cpu);
	return 0;
}

/*
 * Parse the dpdk arguments for use in client app.
 */
static int
parse_dpdk_args(int argc, char *argv[])
{
	int cnt;
	int option_index, opt;
	const int argcopt = argc;
	char *argvopt[argcopt];
	const char *progname = argv[0];
	static struct option lgopts[] = { {0} };

	/* getoptを使用するとargvが並び変わるみたいなので、コピーを実施 */
	for (cnt = 0; cnt < argcopt; cnt++) {
		argvopt[cnt] = argv[cnt];
	}

	/* Check DPDK parameter */
	optind = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argvopt, "c:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case 'c':
			/* CPU */
			if (parse_cpu_bit(&g_startup_param.cpu, optarg) != 0) {
				usage(progname);
				return -1;
			}
			break;
		default:
			/* CPU */
			/* DPDKのパラメータは他にもあるので、エラーとはしない */
			break;
		}
	}

	return 0;
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
	int cnt;
	int option_index, opt;
	const int argcopt = argc;
	char *argvopt[argcopt];
	const char *progname = argv[0];
	static struct option lgopts[] = { 
			{ "config", required_argument, NULL, SPP_LONGOPT_RETVAL_CONFIG },
			{ 0 },
	 };

	/* getoptを使用するとargvが並び変わるみたいなので、コピーを実施 */
	for (cnt = 0; cnt < argcopt; cnt++) {
		argvopt[cnt] = argv[cnt];
	}

	/* Check application parameter */
	optind = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argvopt, "", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case SPP_LONGOPT_RETVAL_CONFIG:
			if (optarg[0] == '\0' || strlen(optarg) >= sizeof(config_file_path)) {
				usage(progname);
				return -1;
			}
			strcpy(config_file_path, optarg);
			break;
		default:
			break;
		}
	}

	return 0;
}

/*
 * IF種別＆IF番号のIF情報の領域取得
 */
static struct patch_info *
get_if_area(enum port_type if_type, int if_no)
{
	switch (if_type) {
	case PHY:
		return &g_if_info.nic_patchs[if_no];
		break;
	case VHOST:
		return &g_if_info.vhost_patchs[if_no];
		break;
	case RING:
		return &g_if_info.ring_patchs[if_no];
		break;
	default:
		/* エラー出力は呼び元でチェック */
		return NULL;
		break;
	}
}

/*
 * IF情報初期化
 */
static void
init_if_info(void)
{
	memset(&g_if_info, 0x00, sizeof(g_if_info));
}

/*
 * CORE情報初期化
 */
static void
init_core_info(void)
{
	memset(&g_core_info, 0x00, sizeof(g_core_info));
	int core_cnt, port_cnt;
	for (core_cnt = 0; core_cnt < SPP_CONFIG_CORE_MAX; core_cnt++) {
		for (port_cnt = 0; port_cnt < RTE_MAX_ETHPORTS; port_cnt++) {
			g_core_info[core_cnt].rx_ports[port_cnt].if_type = UNDEF;
			g_core_info[core_cnt].tx_ports[port_cnt].if_type = UNDEF;
		}
	}
}

/*
 * Configのプロセス情報から管理情報に設定
 */
static int
set_form_proc_info(struct spp_config_area *config)
{
	/* Configのproc_infoから内部管理情報へ設定 */
	int core_cnt, rx_start, rx_cnt, tx_start, tx_cnt;
	enum port_type if_type;
	int if_no;
	uint64_t cpu_bit = 0;
	struct spp_config_functions *core_func = NULL;
	struct spp_core_info *core_info = NULL;
	struct patch_info *patch_info = NULL;
	for (core_cnt = 0; core_cnt < config->proc.num_func; core_cnt++) {
		core_func = &config->proc.functions[core_cnt];
		core_info = &g_core_info[core_func->core_no];

		if (core_func->type == SPP_CONFIG_UNUSE) {
			continue;
		}

		/* Forwardをまとめる事は可、他種別は不可 */
		if ((core_info->type != SPP_CONFIG_UNUSE) &&
				((core_info->type != SPP_CONFIG_FORWARD) ||
				(core_func->type != SPP_CONFIG_FORWARD))) {
			RTE_LOG(ERR, APP, "Core in use. (core = %d, type = %d/%d)\n",
					core_func->core_no,
					core_func->type, core_info->type);
			return -1;
		}

		/* Set CORE type */
		core_info->type = core_func->type;
		cpu_bit |= 1 << core_func->core_no;

		/* Set RX port */
		rx_start = core_info->num_rx_port;
		core_info->num_rx_port += core_func->num_rx_port;
		for (rx_cnt = 0; rx_cnt < core_func->num_rx_port; rx_cnt++) {
			if_type = core_func->rx_ports[rx_cnt].if_type;
			if_no   = core_func->rx_ports[rx_cnt].if_no;

			core_info->rx_ports[rx_start + rx_cnt].if_type = if_type;
			core_info->rx_ports[rx_start + rx_cnt].if_no   = if_no;

			/* IF種別とIF番号に対応するIF情報の領域取得 */
			patch_info = get_if_area(if_type, if_no);

			patch_info->use_flg = 1;
			if (unlikely(patch_info->rx_core != NULL)) {
				RTE_LOG(ERR, APP, "Used RX port (core = %d, if_type = %d, if_no = %d)\n",
						core_func->core_no, if_type, if_no);
				return -1;
			}

			/* IF情報からCORE情報を変更する場合用に設定 */
			patch_info->rx_core = &core_info->rx_ports[rx_start + rx_cnt];
		}

		/* Set TX port */
		tx_start = core_info->num_tx_port;
		core_info->num_tx_port += core_func->num_tx_port;
		for (tx_cnt = 0; tx_cnt < core_func->num_tx_port; tx_cnt++) {
			if_type = core_func->tx_ports[tx_cnt].if_type;
			if_no   = core_func->tx_ports[tx_cnt].if_no;

			core_info->tx_ports[tx_start + tx_cnt].if_type = if_type;
			core_info->tx_ports[tx_start + tx_cnt].if_no   = if_no;

			/* IF種別とIF番号に対応するIF情報の領域取得 */
			patch_info = get_if_area(if_type, if_no);

			patch_info->use_flg = 1;
			if (unlikely(patch_info->tx_core != NULL)) {
				RTE_LOG(ERR, APP, "Used TX port (core = %d, if_type = %d, if_no = %d)\n",
						core_func->core_no, if_type, if_no);
				return -1;
			}

			/* IF情報からCORE情報を変更する場合用に設定 */
			patch_info->tx_core = &core_info->tx_ports[tx_start + tx_cnt];
		}
	}

#if 0 /* bugfix#385 */
	if (unlikely((cpu_bit & g_startup_param.cpu) != cpu_bit)) {
		/* CPU mismatch */
		RTE_LOG(ERR, APP, "CPU mismatch (cpu param = %lx, config = %lx)\n",
				g_startup_param.cpu, cpu_bit);
		return -1;
	}
#endif
	return 0;
}

/*
 * ConfigのMACテーブル情報から管理情報に設定
 */
static int
set_from_classifier_table(struct spp_config_area *config)
{
	/* MAC table */
	enum port_type if_type;
	int if_no = 0;
	int mac_cnt = 0;
	struct spp_config_mac_table_element *mac_table = NULL;
	struct patch_info *patch_info = NULL;
	for (mac_cnt = 0; mac_cnt < config->classifier_table.num_table; mac_cnt++) {
		mac_table = &config->classifier_table.mac_tables[mac_cnt];

		if_type = mac_table->port.if_type;
		if_no   = mac_table->port.if_no;

		/* IF種別とIF番号に対応するIF情報の領域取得 */
		patch_info = get_if_area(if_type, if_no);

		if (unlikely(patch_info->use_flg == 0)) {
			RTE_LOG(ERR, APP, "Not used interface (if_type = %d, if_no = %d)\n",
					if_type, if_no);
			return -1;
		}

		/* CORE情報側にもMACアドレスの情報設定 */
		/* MACアドレスは送信側のみに影響する為、送信側のみ設定 */
		patch_info->mac_info = mac_table;
		if (unlikely(patch_info->tx_core != NULL)) {
			patch_info->tx_core->mac_addr = mac_table->mac_addr;
			strcpy(patch_info->tx_core->mac_addr_str, mac_table->mac_addr_str);
		}
	}
	return 0;
}

/*
 * NIC用の情報設定
 */
static int
set_nic_interface(struct spp_config_area *config __attribute__ ((unused)))
{
	/* NIC Setting */
	g_if_info.num_nic = rte_eth_dev_count();
	if (g_if_info.num_nic > RTE_MAX_ETHPORTS) {
		g_if_info.num_nic = RTE_MAX_ETHPORTS;
	}

	int nic_cnt, nic_num = 0;
	struct patch_info *patch_info = NULL;
	for (nic_cnt = 0; nic_cnt < RTE_MAX_ETHPORTS; nic_cnt++) {
		patch_info = &g_if_info.nic_patchs[nic_cnt];
		/* Set DPDK port */
		patch_info->dpdk_port = nic_cnt;

		if (patch_info->use_flg == 0) {
			/* Not Used */
			continue;
		}

		/* CORE情報側にもDPDKポート番号の情報設定 */
		if (patch_info->rx_core != NULL) {
			patch_info->rx_core->dpdk_port = nic_cnt;
		}
		if (patch_info->tx_core != NULL) {
			patch_info->tx_core->dpdk_port = nic_cnt;
		}

		/* NICの設定数カウント */
		nic_num++;
	}

	if (unlikely(nic_num > g_if_info.num_nic)) {
		RTE_LOG(ERR, APP, "NIC Setting mismatch. (IF = %d, config = %d)\n",
				nic_num, g_if_info.num_nic);
		return -1;
	}
	
	return 0;
}

/*
 * VHOST用の情報設定
 */
static int
set_vhost_interface(struct spp_config_area *config)
{
	/* VHOST Setting */
	int vhost_cnt, vhost_num = 0;
	g_if_info.num_vhost = config->proc.num_vhost;
	struct patch_info *patch_info = NULL;
	for (vhost_cnt = 0; vhost_cnt < RTE_MAX_ETHPORTS; vhost_cnt++) {
		patch_info = &g_if_info.vhost_patchs[vhost_cnt];
		if (patch_info->use_flg == 0) {
			/* Not Used */
			continue;
		}

		/* Set DPDK port */
		int dpdk_port = add_vhost_pmd(vhost_cnt);
		if (unlikely(dpdk_port < 0)) {
			RTE_LOG(ERR, APP, "VHOST add failed. (no = %d)\n",
					vhost_cnt);
			return -1;
		}
		patch_info->dpdk_port = dpdk_port;

		/* CORE情報側にもDPDKポート番号の情報設定 */
		if (patch_info->rx_core != NULL) {
			patch_info->rx_core->dpdk_port = dpdk_port;
		}
		if (patch_info->tx_core != NULL) {
			patch_info->tx_core->dpdk_port = dpdk_port;
		}
		vhost_num++;
	}
	if (unlikely(vhost_num > g_if_info.num_vhost)) {
		RTE_LOG(ERR, APP, "VHOST Setting mismatch. (IF = %d, config = %d)\n",
				vhost_num, g_if_info.num_vhost);
		return -1;
	}
	return 0;
}

/*
 * RING用の情報設定
 */
static int
set_ring_interface(struct spp_config_area *config)
{
	/* RING Setting */
	int ring_cnt, ring_num = 0;
	g_if_info.num_ring = config->proc.num_ring;
	struct patch_info *patch_info = NULL;
	for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
		patch_info = &g_if_info.ring_patchs[ring_cnt];
		if (patch_info->use_flg == 0) {
			/* Not Used */
			continue;
		}

		/* Set DPDK port */
		int dpdk_port = add_ring_pmd(ring_cnt);
		if (unlikely(dpdk_port < 0)) {
			RTE_LOG(ERR, APP, "RING add failed. (no = %d)\n",
					ring_cnt);
			return -1;
		}
		patch_info->dpdk_port = dpdk_port;

		/* CORE情報側にもDPDKポート番号の情報設定 */
		if (patch_info->rx_core != NULL) {
			patch_info->rx_core->dpdk_port = dpdk_port;
		}
		if (patch_info->tx_core != NULL) {
			patch_info->tx_core->dpdk_port = dpdk_port;
		}
		ring_num++;
	}
	if (unlikely(ring_num > g_if_info.num_ring)) {
		RTE_LOG(ERR, APP, "RING Setting mismatch. (IF = %d, config = %d)\n",
				ring_num, g_if_info.num_ring);
		return -1;
	}
	return 0;
}

/*
 * 管理データ初期設定
 */
static int
init_manage_data(struct spp_config_area *config)
{
	/* Initialize */
	init_if_info();
	init_core_info();

	/* Set config data */
	int ret_proc = set_form_proc_info(config);
	if (unlikely(ret_proc != 0)) {
		/* 関数内でログ出力済みなので、省略 */
		return -1;
	}
	int ret_classifier = set_from_classifier_table(config);
	if (unlikely(ret_classifier != 0)) {
		/* 関数内でログ出力済みなので、省略 */
		return -1;
	}

	/* Set interface data */
	int ret_nic = set_nic_interface(config);
	if (unlikely(ret_nic != 0)) {
		/* 関数内でログ出力済みなので、省略 */
		return -1;
	}

	int ret_vhost = set_vhost_interface(config);
	if (unlikely(ret_vhost != 0)) {
		/* 関数内でログ出力済みなので、省略 */
		return -1;
	}

	int ret_ring = set_ring_interface(config);
	if (unlikely(ret_ring != 0)) {
		/* 関数内でログ出力済みなので、省略 */
		return -1;
	}

	return 0;
}

#ifdef SPP_RINGLATENCYSTATS_ENABLE /* RING滞留時間 */
static void
print_ring_latency_stats(void)
{
	/* Clear screen and move to top left */
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	printf("%s%s", clr, topLeft);

	/* Print per RING */
	int ring_cnt, stats_cnt;
	struct spp_ringlatencystats_ring_latency_stats stats[RTE_MAX_ETHPORTS];
	memset(&stats, 0x00, sizeof(stats));

	printf("RING Latency\n");
	printf(" RING");
	for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
		if (g_if_info.ring_patchs[ring_cnt].use_flg == 0) {
			continue;
		}
		spp_ringlatencystats_get_stats(ring_cnt, &stats[ring_cnt]);
		printf(", %-18d", ring_cnt);
	}
	printf("\n");

	for (stats_cnt = 0; stats_cnt < SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT; stats_cnt++) {
		printf("%3dns", stats_cnt);
		for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
			if (g_if_info.ring_patchs[ring_cnt].use_flg == 0) {
				continue;
			}

			printf(", 0x%-16lx", stats[ring_cnt].slot[stats_cnt]);
		}
		printf("\n");
	}

	return;
}
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

/*
 * VHOST用ソケットファイル削除
 */
static void
del_vhost_sockfile(struct patch_info *vhost_patchs)
{
	int cnt;
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		if (likely(vhost_patchs[cnt].use_flg == 0)) {
			/* VHOST未使用はスキップ */
			continue;
		}

		/* 使用していたVHOSTについて削除を行う */
		remove(get_vhost_iface_name(cnt));
	}
}

/*
 * main
 */
int
#ifndef USE_UT_SPP_VF
main(int argc, char *argv[])
#else /* ifndef USE_UT_SPP_VF */
ut_main(int argc, char *argv[])
#endif  /* ifndef USE_UT_SPP_VF */
{
	int ret = -1;
#ifdef SPP_DEMONIZE
	/* Daemonize process */
	int ret_daemon = daemon(0, 0);
	if (unlikely(ret_daemon != 0)) {
		RTE_LOG(ERR, APP, "daemonize is faild. (ret = %d)\n", ret_daemon);
		return ret_daemon;
	}
#endif

	/* Signal handler registration (SIGTERM/SIGINT) */
	signal(SIGTERM, stop_process);
	signal(SIGINT,  stop_process);

	/* set default config file path */
	strcpy(config_file_path, SPP_CONFIG_FILE_PATH);

	unsigned int main_lcore_id = 0xffffffff;
	while(1) {
		/* Parse dpdk parameters */
		int ret_parse = parse_dpdk_args(argc, argv);
		if (unlikely(ret_parse != 0)) {
			break;
		}

		/* DPDK initialize */
		int ret_dpdk = rte_eal_init(argc, argv);
		if (unlikely(ret_dpdk < 0)) {
			break;
		}

		/* Skip dpdk parameters */
		argc -= ret_dpdk;
		argv += ret_dpdk;

		/* Set log level  */
		rte_log_set_global_level(RTE_LOG_LEVEL);

		/* Parse application parameters */
		ret_parse = parse_app_args(argc, argv);
		if (unlikely(ret_parse != 0)) {
			break;
		}

		RTE_LOG(INFO, APP, "Load config file(%s)\n", config_file_path);

		/* Load config */
		int ret_config = spp_config_load_file(config_file_path, 0, &g_config);
		if (unlikely(ret_config != 0)) {
			break;
		}

		/* Get core id. */
		main_lcore_id = rte_lcore_id();

		/* 起動パラメータとコンフィグチェック */
		/* 各IF情報設定 */
		int ret_manage = init_manage_data(&g_config);
		if (unlikely(ret_manage != 0)) {
			break;
		}

		/* 他機能部初期化 */
#ifdef SPP_RINGLATENCYSTATS_ENABLE /* RING滞留時間 */
		int ret_ringlatency = spp_ringlatencystats_init(
				SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL, g_if_info.num_ring);
		if (unlikely(ret_ringlatency != 0)) {
			break;
		}
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

		/* Start  thread */
		unsigned int lcore_id = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			if (g_core_info[lcore_id].type == SPP_CONFIG_CLASSIFIER_MAC) {
				rte_eal_remote_launch(spp_classifier_mac_do,
						(void *)&g_core_info[lcore_id],
						lcore_id);
			} else {
				rte_eal_remote_launch(spp_forward,
						(void *)&g_core_info[lcore_id],
						lcore_id);
			}
		}

		/* スレッド状態確認 */
		g_core_info[main_lcore_id].status = SPP_CORE_IDLE;
		int ret_wait = check_core_status_wait(SPP_CORE_IDLE);
		if (unlikely(ret_wait != 0)) {
			break;
		}

		/* Start forward */
		set_core_status(SPP_CORE_FORWARD);
		RTE_LOG(INFO, APP, "My ID %d start handling message\n", 0);
		RTE_LOG(INFO, APP, "[Press Ctrl-C to quit ...]\n");

		/* loop */
#ifndef USE_UT_SPP_VF
		while(likely(g_core_info[main_lcore_id].status != SPP_CORE_STOP_REQUEST)) {
#else
		{
#endif
			sleep(1);

#ifdef SPP_RINGLATENCYSTATS_ENABLE /* RING滞留時間 */
			print_ring_latency_stats();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
		}

		/* 正常終了 */
		ret = 0;
		break;
	}

	/* exit */
	if (main_lcore_id == rte_lcore_id())
	{
		g_core_info[main_lcore_id].status = SPP_CORE_STOP;
		int ret_core_end = check_core_status_wait(SPP_CORE_STOP);
		if (unlikely(ret_core_end != 0)) {
			RTE_LOG(ERR, APP, "Core did not stop.\n");
		}

		/* 使用していたVHOSTのソケットファイルを削除 */
		del_vhost_sockfile(g_if_info.vhost_patchs);
	}

	/* 他機能部終了処理 */
#ifdef SPP_RINGLATENCYSTATS_ENABLE /* RING滞留時間 */
	spp_ringlatencystats_uninit();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
	RTE_LOG(INFO, APP, "spp_vf exit.\n");
	return ret;
}