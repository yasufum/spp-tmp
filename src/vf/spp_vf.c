/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Nippon Telegraph and Telephone Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Nippon Telegraph and Telephone Corporation
 *       nor the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include <rte_eth_ring.h>
#include <rte_eth_vhost.h>
#include <rte_memzone.h>
#include <rte_cycles.h>

#include "spp_vf.h"
#include "ringlatencystats.h"
#include "classifier_mac.h"
#include "spp_forward.h"
#include "command_proc.h"
#include "spp_port.h"

/* Max number of core status check */
#define SPP_CORE_STATUS_CHECK_MAX 5

/* Sampling interval timer for latency evaluation */
#define SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL 1000000

/* Name string for each component */
#define CORE_TYPE_CLASSIFIER_MAC_STR "classifier_mac"
#define CORE_TYPE_MERGE_STR          "merge"
#define CORE_TYPE_FORWARD_STR        "forward"

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/*
	 * Return value definition for getopt_long()
	 * Only for long option
	 */
	SPP_LONGOPT_RETVAL_CLIENT_ID,   /* --client-id    */
	SPP_LONGOPT_RETVAL_VHOST_CLIENT /* --vhost-client */
};

/* Flag of processing type to copy management information */
enum copy_mng_flg {
	COPY_MNG_FLG_NONE,
	COPY_MNG_FLG_UPDCOPY,
	COPY_MNG_FLG_ALLCOPY,
};

/* Manage given options as global variable */
struct startup_param {
	int client_id;          /* Client ID of spp_vf */
	char server_ip[INET_ADDRSTRLEN];
				/* IP address sting of spp controller */
	int server_port;        /* Port Number of spp controller */
	int vhost_client;       /* Flag for --vhost-client option */
};

/* Manage number of interfaces  and port information as global variable */
struct iface_info {
	int num_nic;            /* The number of phy */
	int num_vhost;          /* The number of vhost */
	int num_ring;           /* The number of ring */
	struct spp_port_info nic[RTE_MAX_ETHPORTS];
				/* Port information of phy */
	struct spp_port_info vhost[RTE_MAX_ETHPORTS];
				/* Port information of vhost */
	struct spp_port_info ring[RTE_MAX_ETHPORTS];
				/* Port information of ring */
};

/* Manage component running in core as global variable */
struct core_info {
	volatile enum spp_component_type type;
				/* Component type */
	int num;                /* The number of IDs below */
	int id[RTE_MAX_LCORE];  /* ID list of components executed on cpu core */
};

/* Manage core status and component information as global variable */
struct core_mng_info {
	/* Status of cpu core */
	volatile enum spp_core_status status;

	/* Index number of core information for reference */
	volatile int ref_index;

	/* Index number of core information for updating */
	volatile int upd_index;

	/* Core information of each cpu core */
	struct core_info core[SPP_INFO_AREA_MAX];
};

/* Manage data to be backup */
struct cancel_backup_info {
	/* Backup data of core information */
	struct core_mng_info core[RTE_MAX_LCORE];

	/* Backup data of component information */
	struct spp_component_info component[RTE_MAX_LCORE];

	/* Backup data of interface information */
	struct iface_info interface;
};

/* Declare global variables */
/* Logical core ID for main process */
static unsigned int g_main_lcore_id = 0xffffffff;

/* Execution parameter of spp_vf */
static struct startup_param g_startup_param;

/* Interface management information */
static struct iface_info g_iface_info;

/* Component management information */
static struct spp_component_info g_component_info[RTE_MAX_LCORE];

/* Core management information */
static struct core_mng_info g_core_info[RTE_MAX_LCORE];

/* Array of update indicator for core management information */
static int g_change_core[RTE_MAX_LCORE];

/* Array of update indicator for component management information */
static int g_change_component[RTE_MAX_LCORE];

/* Backup information for cancel command */
static struct cancel_backup_info g_backup_info;

/* Print help message */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, APP, "Usage: %s [EAL args] --"
			" --client-id CLIENT_ID"
			" -s SERVER_IP:SERVER_PORT"
			" [--vhost-client]\n"
			" --client-id CLIENT_ID   : My client ID\n"
			" -s SERVER_IP:SERVER_PORT  : Access information to the server\n"
			" --vhost-client            : Run vhost on client\n"
			, progname);
}

/* Make a hexdump of an array data in every 4 byte */
static void
dump_buff(const char *name, const void *addr, const size_t size)
{
	size_t cnt = 0;
	size_t max = (size / sizeof(unsigned int)) +
			((size % sizeof(unsigned int)) != 0);
	const uint32_t *buff = addr;

	if ((name != NULL) && (name[0] != '\0'))
		RTE_LOG(DEBUG, APP, "dump buff. (%s)\n", name);

	for (cnt = 0; cnt < max; cnt += 16) {
		RTE_LOG(DEBUG, APP, "[%p]"
				" %08x %08x %08x %08x %08x %08x %08x %08x"
				" %08x %08x %08x %08x %08x %08x %08x %08x",
				&buff[cnt],
				buff[cnt+0], buff[cnt+1],
				buff[cnt+2], buff[cnt+3],
				buff[cnt+4], buff[cnt+5],
				buff[cnt+6], buff[cnt+7],
				buff[cnt+8], buff[cnt+9],
				buff[cnt+10], buff[cnt+11],
				buff[cnt+12], buff[cnt+13],
				buff[cnt+14], buff[cnt+15]);
	}
}

static int
add_ring_pmd(int ring_id)
{
	struct rte_ring *ring;
	int ring_port_id;

	/* Lookup ring of given id */
	ring = rte_ring_lookup(get_rx_queue_name(ring_id));
	if (unlikely(ring == NULL)) {
		RTE_LOG(ERR, APP,
			"Cannot get RX ring - is server process running?\n");
		return -1;
	}

	/* Create ring pmd */
	ring_port_id = rte_eth_from_ring(ring);
	RTE_LOG(INFO, APP, "ring port add. (no = %d / port = %d)\n",
			ring_id, ring_port_id);
	return ring_port_id;
}

static int
add_vhost_pmd(int index, int client)
{
	struct rte_eth_conf port_conf = {
		.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
	};
	struct rte_mempool *mp;
	uint16_t vhost_port_id;
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

	sprintf(devargs, "%s,iface=%s,queues=%d,client=%d",
			name, iface, nr_queues, client);
	ret = rte_eth_dev_attach(devargs, &vhost_port_id);
	if (unlikely(ret < 0)) {
		RTE_LOG(ERR, APP, "rte_eth_dev_attach error. (ret = %d)\n",
				ret);
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

	RTE_LOG(INFO, APP, "vhost port add. (no = %d / port = %d)\n",
			index, vhost_port_id);
	return vhost_port_id;
}

/* Get core status */
enum spp_core_status
spp_get_core_status(unsigned int lcore_id)
{
	return g_core_info[lcore_id].status;
}

/**
 * Check status of all of cores is same as given
 *
 * It returns -1 as status mismatch if status is not same.
 * If core is in use, status will be checked.
 */
static int
check_core_status(enum spp_core_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (g_core_info[lcore_id].status != status) {
			/* Status is mismatched */
			return -1;
		}
	}
	return 0;
}

/**
 * Run check_core_status() for SPP_CORE_STATUS_CHECK_MAX times with
 * interval time (1sec)
 */
static int
check_core_status_wait(enum spp_core_status status)
{
	int cnt = 0;
	for (cnt = 0; cnt < SPP_CORE_STATUS_CHECK_MAX; cnt++) {
		sleep(1);
		int ret = check_core_status(status);
		if (ret == 0)
			return 0;
	}

	RTE_LOG(ERR, APP, "Status check time out. (status = %d)\n", status);
	return -1;
}

/* Set core status */
static void
set_core_status(unsigned int lcore_id,
		enum spp_core_status status)
{
	g_core_info[lcore_id].status = status;
}

/* Set all core to given status */
static void
set_all_core_status(enum spp_core_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		g_core_info[lcore_id].status = status;
	}
}

/**
 * Set all of component status to SPP_CORE_STOP_REQUEST if received signal
 * is SIGTERM or SIGINT
 */
static void
stop_process(int signal) {
	if (unlikely(signal != SIGTERM) &&
			unlikely(signal != SIGINT)) {
		return;
	}

	g_core_info[g_main_lcore_id].status = SPP_CORE_STOP_REQUEST;
	set_all_core_status(SPP_CORE_STOP_REQUEST);
}

/**
 * Convert string of given client id to integer
 *
 * If succeeded, client id of integer is assigned to client_id and
 * return 0. Or return -1 if failed.
 */
static int
parse_app_client_id(const char *client_id_str, int *client_id)
{
	int id = 0;
	char *endptr = NULL;

	id = strtol(client_id_str, &endptr, 0);
	if (unlikely(client_id_str == endptr) || unlikely(*endptr != '\0'))
		return -1;

	if (id >= RTE_MAX_LCORE)
		return -1;

	*client_id = id;
	RTE_LOG(DEBUG, APP, "Set client id = %d\n", *client_id);
	return 0;
}

/* Parse options for server IP and port */
static int
parse_app_server(const char *server_str, char *server_ip, int *server_port)
{
	const char delim[2] = ":";
	unsigned int pos = 0;
	int port = 0;
	char *endptr = NULL;

	pos = strcspn(server_str, delim);
	if (pos >= strlen(server_str))
		return -1;

	port = strtol(&server_str[pos+1], &endptr, 0);
	if (unlikely(&server_str[pos+1] == endptr) || unlikely(*endptr != '\0'))
		return -1;

	memcpy(server_ip, server_str, pos);
	server_ip[pos] = '\0';
	*server_port = port;
	RTE_LOG(DEBUG, APP, "Set server IP   = %s\n", server_ip);
	RTE_LOG(DEBUG, APP, "Set server port = %d\n", *server_port);
	return 0;
}

/* Parse options for client app */
static int
parse_app_args(int argc, char *argv[])
{
	int cnt;
	int proc_flg = 0;
	int server_flg = 0;
	int option_index, opt;
	const int argcopt = argc;
	char *argvopt[argcopt];
	const char *progname = argv[0];
	static struct option lgopts[] = {
			{ "client-id", required_argument, NULL,
					SPP_LONGOPT_RETVAL_CLIENT_ID },
			{ "vhost-client", no_argument, NULL,
					SPP_LONGOPT_RETVAL_VHOST_CLIENT },
			{ 0 },
	};

	/**
	 * Save argv to argvopt to avoid losing the order of options
	 * by getopt_long()
	 */
	for (cnt = 0; cnt < argcopt; cnt++)
		argvopt[cnt] = argv[cnt];

	/* Clear startup parameters */
	memset(&g_startup_param, 0x00, sizeof(g_startup_param));

	/* Check options of application */
	optind = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argvopt, "s:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case SPP_LONGOPT_RETVAL_CLIENT_ID:
			if (parse_app_client_id(optarg,
					&g_startup_param.client_id) != 0) {
				usage(progname);
				return -1;
			}
			proc_flg = 1;
			break;
		case SPP_LONGOPT_RETVAL_VHOST_CLIENT:
			g_startup_param.vhost_client = 1;
			break;
		case 's':
			if (parse_app_server(optarg, g_startup_param.server_ip,
					&g_startup_param.server_port) != 0) {
				usage(progname);
				return -1;
			}
			server_flg = 1;
			break;
		default:
			usage(progname);
			return -1;
		}
	}

	/* Check mandatory parameters */
	if ((proc_flg == 0) || (server_flg == 0)) {
		usage(progname);
		return -1;
	}
	RTE_LOG(INFO, APP,
			"app opts (client_id=%d,server=%s:%d,vhost_client=%d)\n",
			g_startup_param.client_id,
			g_startup_param.server_ip,
			g_startup_param.server_port,
			g_startup_param.vhost_client);
	return 0;
}

/**
 * Return port info of given type and num of interface
 *
 * It returns NULL value if given type is invalid.
 */
static struct spp_port_info *
get_iface_info(enum port_type iface_type, int iface_no)
{
	switch (iface_type) {
	case PHY:
		return &g_iface_info.nic[iface_no];
	case VHOST:
		return &g_iface_info.vhost[iface_no];
	case RING:
		return &g_iface_info.ring[iface_no];
	default:
		return NULL;
	}
}

/* Dump of core information */
static void
dump_core_info(const struct core_mng_info *core_info)
{
	char str[SPP_NAME_STR_LEN];
	const struct core_mng_info *info = NULL;
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		info = &core_info[lcore_id];
		RTE_LOG(DEBUG, APP, "core[%d] status=%d, ref=%d, upd=%d\n",
				lcore_id, info->status,
				info->ref_index, info->upd_index);

		sprintf(str, "core[%d]-0 type=%d, num=%d", lcore_id,
				info->core[0].type, info->core[0].num);
		dump_buff(str, info->core[0].id, sizeof(int)*info->core[0].num);

		sprintf(str, "core[%d]-1 type=%d, num=%d", lcore_id,
				info->core[1].type, info->core[1].num);
		dump_buff(str, info->core[1].id, sizeof(int)*info->core[1].num);
	}
}

/* Dump of component information */
static void
dump_component_info(const struct spp_component_info *component_info)
{
	char str[SPP_NAME_STR_LEN];
	const struct spp_component_info *component = NULL;
	int cnt = 0;
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		component = &component_info[cnt];
		if (component->type == SPP_COMPONENT_UNUSE)
			continue;

		RTE_LOG(DEBUG, APP, "component[%d] name=%s, type=%d, core=%u, index=%d\n",
				cnt, component->name, component->type,
				component->lcore_id, component->component_id);

		sprintf(str, "component[%d] rx=%d", cnt,
				component->num_rx_port);
		dump_buff(str, component->rx_ports,
			sizeof(struct spp_port_info *)*component->num_rx_port);

		sprintf(str, "component[%d] tx=%d", cnt,
				component->num_tx_port);
		dump_buff(str, component->tx_ports,
			sizeof(struct spp_port_info *)*component->num_tx_port);
	}
}

/* Dump of interface information */
static void
dump_interface_info(const struct iface_info *iface_info)
{
	const struct spp_port_info *port = NULL;
	int cnt = 0;
	RTE_LOG(DEBUG, APP, "interface phy=%d, vhost=%d, ring=%d\n",
			iface_info->num_nic,
			iface_info->num_vhost,
			iface_info->num_ring);
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->nic[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, APP, "phy  [%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->dpdk_port,
				port->class_id.vlantag.vid,
				port->class_id.mac_addr,
				port->class_id.mac_addr_str);
	}
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->vhost[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, APP, "vhost[%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->dpdk_port,
				port->class_id.vlantag.vid,
				port->class_id.mac_addr,
				port->class_id.mac_addr_str);
	}
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->ring[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, APP, "ring [%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->dpdk_port,
				port->class_id.vlantag.vid,
				port->class_id.mac_addr,
				port->class_id.mac_addr_str);
	}
}

/* Dump of all management information */
static void
dump_all_mng_info(
		const struct core_mng_info *core,
		const struct spp_component_info *component,
		const struct iface_info *interface)
{
	if (rte_log_get_global_level() < RTE_LOG_DEBUG)
		return;

	dump_core_info(core);
	dump_component_info(component);
	dump_interface_info(interface);
}

/* Copy management information */
static void
copy_mng_info(
		struct core_mng_info *dst_core,
		struct spp_component_info *dst_component,
		struct iface_info *dst_interface,
		const struct core_mng_info *src_core,
		const struct spp_component_info *src_component,
		const struct iface_info *src_interface,
		enum copy_mng_flg flg)
{
	int upd_index = 0;
	unsigned int lcore_id = 0;

	switch (flg) {
	case COPY_MNG_FLG_UPDCOPY:
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			upd_index = src_core[lcore_id].upd_index;
			memcpy(&dst_core[lcore_id].core[upd_index],
					&src_core[lcore_id].core[upd_index],
					sizeof(struct core_info));
		}
		break;
	default:
		/*
		 * Even if the flag is set to None,
		 * copying of core is necessary,
		 * so we will treat all copies as default.
		 */
		memcpy(dst_core, src_core,
				sizeof(struct core_mng_info)*RTE_MAX_LCORE);
		break;
	}

	memcpy(dst_component, src_component,
			sizeof(struct spp_component_info)*RTE_MAX_LCORE);
	memcpy(dst_interface, src_interface,
			sizeof(struct iface_info));
}

/* Backup the management information */
static void
backup_mng_info(struct cancel_backup_info *backup)
{
	dump_all_mng_info(g_core_info, g_component_info, &g_iface_info);
	copy_mng_info(backup->core, backup->component, &backup->interface,
			g_core_info, g_component_info, &g_iface_info,
			COPY_MNG_FLG_ALLCOPY);
	dump_all_mng_info(backup->core, backup->component, &backup->interface);
	memset(g_change_core, 0x00, sizeof(g_change_core));
	memset(g_change_component, 0x00, sizeof(g_change_component));
}

/* Cancel update of management information */
static void
cancel_mng_info(struct cancel_backup_info *backup)
{
	dump_all_mng_info(backup->core, backup->component, &backup->interface);
	copy_mng_info(g_core_info, g_component_info, &g_iface_info,
			backup->core, backup->component, &backup->interface,
			COPY_MNG_FLG_ALLCOPY);
	dump_all_mng_info(g_core_info, g_component_info, &g_iface_info);
	memset(g_change_core, 0x00, sizeof(g_change_core));
	memset(g_change_component, 0x00, sizeof(g_change_component));
}

/**
 * Initialize g_iface_info
 *
 * Clear g_iface_info and set initial value.
 */
static void
init_iface_info(void)
{
	int port_cnt;  /* increment ether ports */
	memset(&g_iface_info, 0x00, sizeof(g_iface_info));
	for (port_cnt = 0; port_cnt < RTE_MAX_ETHPORTS; port_cnt++) {
		g_iface_info.nic[port_cnt].iface_type = UNDEF;
		g_iface_info.nic[port_cnt].iface_no   = port_cnt;
		g_iface_info.nic[port_cnt].dpdk_port  = -1;
		g_iface_info.nic[port_cnt].class_id.vlantag.vid =
				ETH_VLAN_ID_MAX;
		g_iface_info.vhost[port_cnt].iface_type = UNDEF;
		g_iface_info.vhost[port_cnt].iface_no   = port_cnt;
		g_iface_info.vhost[port_cnt].dpdk_port  = -1;
		g_iface_info.vhost[port_cnt].class_id.vlantag.vid =
				ETH_VLAN_ID_MAX;
		g_iface_info.ring[port_cnt].iface_type = UNDEF;
		g_iface_info.ring[port_cnt].iface_no   = port_cnt;
		g_iface_info.ring[port_cnt].dpdk_port  = -1;
		g_iface_info.ring[port_cnt].class_id.vlantag.vid =
				ETH_VLAN_ID_MAX;
	}
}

/**
 * Initialize g_component_info
 */
static void
init_component_info(void)
{
	int cnt;
	memset(&g_component_info, 0x00, sizeof(g_component_info));
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++)
		g_component_info[cnt].component_id = cnt;
	memset(g_change_component, 0x00, sizeof(g_change_component));
}

/**
 * Initialize g_core_info
 */
static void
init_core_info(void)
{
	int cnt = 0;
	memset(&g_core_info, 0x00, sizeof(g_core_info));
	set_all_core_status(SPP_CORE_STOP);
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		g_core_info[cnt].ref_index = 0;
		g_core_info[cnt].upd_index = 1;
	}
	memset(g_change_core, 0x00, sizeof(g_change_core));
}

/**
 * Setup port info of port on host
 */
static int
set_nic_interface(void)
{
	int nic_cnt = 0;

	/* NIC Setting */
	g_iface_info.num_nic = rte_eth_dev_count();
	if (g_iface_info.num_nic > RTE_MAX_ETHPORTS)
		g_iface_info.num_nic = RTE_MAX_ETHPORTS;

	for (nic_cnt = 0; nic_cnt < g_iface_info.num_nic; nic_cnt++) {
		g_iface_info.nic[nic_cnt].iface_type   = PHY;
		g_iface_info.nic[nic_cnt].dpdk_port = nic_cnt;
	}

	return 0;
}

/**
 * Setup management info for spp_vf
 */
static int
init_mng_data(void)
{
	/* Initialize interface and core information */
	init_iface_info();
	init_core_info();
	init_component_info();

	int ret_nic = set_nic_interface();
	if (unlikely(ret_nic != 0))
		return -1;

	return 0;
}

#ifdef SPP_RINGLATENCYSTATS_ENABLE
/**
 * Print statistics of time for packet processing in ring interface
 */
static void
print_ring_latency_stats(void)
{
	/* Clear screen and move cursor to top left */
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	printf("%s%s", clr, topLeft);

	int ring_cnt, stats_cnt;
	struct spp_ringlatencystats_ring_latency_stats stats[RTE_MAX_ETHPORTS];
	memset(&stats, 0x00, sizeof(stats));

	printf("RING Latency\n");
	printf(" RING");
	for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
		if (g_iface_info.ring[ring_cnt].iface_type == UNDEF)
			continue;

		spp_ringlatencystats_get_stats(ring_cnt, &stats[ring_cnt]);
		printf(", %-18d", ring_cnt);
	}
	printf("\n");

	for (stats_cnt = 0; stats_cnt < SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT;
			stats_cnt++) {
		printf("%3dns", stats_cnt);
		for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
			if (g_iface_info.ring[ring_cnt].iface_type == UNDEF)
				continue;

			printf(", 0x%-16lx", stats[ring_cnt].slot[stats_cnt]);
		}
		printf("\n");
	}
}
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

/**
 * Remove sock file
 */
static void
del_vhost_sockfile(struct spp_port_info *vhost)
{
	int cnt;

	/* Do not remove for if it is running in vhost-client mode. */
	if (g_startup_param.vhost_client != 0)
		return;

	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		if (likely(vhost[cnt].iface_type == UNDEF)) {
			/* Skip removing if it is not using vhost */
			continue;
		}

		remove(get_vhost_iface_name(cnt));
	}
}

/* Get component type of target core */
enum spp_component_type
spp_get_component_type(unsigned int lcore_id)
{
	struct core_mng_info *info = &g_core_info[lcore_id];
	return info->core[info->ref_index].type;
}

/* Get component type being updated on target core */
enum spp_component_type
spp_get_component_type_update(unsigned int lcore_id)
{
	struct core_mng_info *info = &g_core_info[lcore_id];
	return info->core[info->upd_index].type;
}

/* Get core ID of target component */
unsigned int
spp_get_component_core(int component_id)
{
	struct spp_component_info *info = &g_component_info[component_id];
	return info->lcore_id;
}

/* Get usage area of target core */
static struct core_info *
get_core_info(unsigned int lcore_id)
{
	struct core_mng_info *info = &g_core_info[lcore_id];
	return &(info->core[info->ref_index]);
}

/* Check core index change */
int
spp_check_core_index(unsigned int lcore_id)
{
	struct core_mng_info *info = &g_core_info[lcore_id];
	return info->ref_index == info->upd_index;
}

/* Main process of slave core */
static int
slave_main(void *arg __attribute__ ((unused)))
{
	int ret = 0;
	int cnt = 0;
	unsigned int lcore_id = rte_lcore_id();
	enum spp_core_status status = SPP_CORE_STOP;
	struct core_mng_info *info = &g_core_info[lcore_id];
	struct core_info *core = get_core_info(lcore_id);

	RTE_LOG(INFO, APP, "Core[%d] Start.\n", lcore_id);
	set_core_status(lcore_id, SPP_CORE_IDLE);

	while ((status = spp_get_core_status(lcore_id)) !=
			SPP_CORE_STOP_REQUEST) {
		if (status != SPP_CORE_FORWARD)
			continue;

		if (spp_check_core_index(lcore_id)) {
			/* Setting with the flush command trigger. */
			info->ref_index = (info->upd_index+1) %
					SPP_INFO_AREA_MAX;
			core = get_core_info(lcore_id);
		}

		for (cnt = 0; cnt < core->num; cnt++) {
			if (spp_get_component_type(lcore_id) ==
					SPP_COMPONENT_CLASSIFIER_MAC) {
				/* Classifier loops inside the function. */
				ret = spp_classifier_mac_do(core->id[cnt]);
				break;
			}

			/*
			 * Forward / Merge returns at once.
			 * It is for processing multiple components.
			 */
			ret = spp_forward(core->id[cnt]);
			if (unlikely(ret != 0))
				break;
		}
		if (unlikely(ret != 0)) {
			RTE_LOG(ERR, APP, "Core[%d] Component Error. (id = %d)\n",
					lcore_id, core->id[cnt]);
			break;
		}
	}

	set_core_status(lcore_id, SPP_CORE_STOP);
	RTE_LOG(INFO, APP, "Core[%d] End.\n", lcore_id);
	return ret;
}

/**
 * Main function
 *
 * Return -1 explicitly if error is occurred.
 */
int
main(int argc, char *argv[])
{
	int ret = -1;
#ifdef SPP_DEMONIZE
	/* Daemonize process */
	int ret_daemon = daemon(0, 0);
	if (unlikely(ret_daemon != 0)) {
		RTE_LOG(ERR, APP, "daemonize is failed. (ret = %d)\n",
				ret_daemon);
		return ret_daemon;
	}
#endif

	/* Signal handler registration (SIGTERM/SIGINT) */
	signal(SIGTERM, stop_process);
	signal(SIGINT,  stop_process);

	while (1) {
		int ret_dpdk = rte_eal_init(argc, argv);
		if (unlikely(ret_dpdk < 0))
			break;

		argc -= ret_dpdk;
		argv += ret_dpdk;

		/* Set log level  */
		rte_log_set_global_level(RTE_LOG_LEVEL);

		/* Parse spp_vf specific parameters */
		int ret_parse = parse_app_args(argc, argv);
		if (unlikely(ret_parse != 0))
			break;

		/* Get lcore id of main thread to set its status after */
		g_main_lcore_id = rte_lcore_id();

		int ret_mng = init_mng_data();
		if (unlikely(ret_mng != 0))
			break;

		int ret_classifier_mac_init = spp_classifier_mac_init();
		if (unlikely(ret_classifier_mac_init != 0))
			break;

		spp_forward_init();
		spp_port_ability_init();

		/* Setup connection for accepting commands from controller */
		int ret_command_init = spp_command_proc_init(
				g_startup_param.server_ip,
				g_startup_param.server_port);
		if (unlikely(ret_command_init != 0))
			break;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
		int ret_ringlatency = spp_ringlatencystats_init(
				SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL,
				g_iface_info.num_ring);
		if (unlikely(ret_ringlatency != 0))
			break;
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

		/* Start worker threads of classifier and forwarder */
		unsigned int lcore_id = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			rte_eal_remote_launch(slave_main, NULL, lcore_id);
		}

		/* Set the status of main thread to idle */
		g_core_info[g_main_lcore_id].status = SPP_CORE_IDLE;
		int ret_wait = check_core_status_wait(SPP_CORE_IDLE);
		if (unlikely(ret_wait != 0))
			break;

		/* Start forwarding */
		set_all_core_status(SPP_CORE_FORWARD);
		RTE_LOG(INFO, APP, "My ID %d start handling message\n", 0);
		RTE_LOG(INFO, APP, "[Press Ctrl-C to quit ...]\n");

		/* Backup the management information after initialization */
		backup_mng_info(&g_backup_info);

		/* Enter loop for accepting commands */
		int ret_do = 0;
#ifndef USE_UT_SPP_VF
		while (likely(g_core_info[g_main_lcore_id].status !=
				SPP_CORE_STOP_REQUEST)) {
#else
		{
#endif
			/* Receive command */
			ret_do = spp_command_proc_do();
			if (unlikely(ret_do != 0))
				break;

			sleep(1);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
			print_ring_latency_stats();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
		}

		if (unlikely(ret_do != 0)) {
			set_all_core_status(SPP_CORE_STOP_REQUEST);
			break;
		}

		ret = 0;
		break;
	}

	/* Finalize to exit */
	if (g_main_lcore_id == rte_lcore_id()) {
		g_core_info[g_main_lcore_id].status = SPP_CORE_STOP;
		int ret_core_end = check_core_status_wait(SPP_CORE_STOP);
		if (unlikely(ret_core_end != 0))
			RTE_LOG(ERR, APP, "Core did not stop.\n");

		/*
		 * Remove vhost sock file if it is not running
		 *  in vhost-client mode
		 */
		del_vhost_sockfile(g_iface_info.vhost);
	}

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	spp_ringlatencystats_uninit();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	RTE_LOG(INFO, APP, "spp_vf exit.\n");
	return ret;
}

int
spp_get_client_id(void)
{
	return g_startup_param.client_id;
}

/**
 * Check mac address used on the port for registering or removing
 */
int
spp_check_classid_used_port(
		int vid, uint64_t mac_addr,
		enum port_type iface_type, int iface_no)
{
	struct spp_port_info *port_info = get_iface_info(iface_type, iface_no);
	return ((mac_addr == port_info->class_id.mac_addr) &&
			(vid == port_info->class_id.vlantag.vid));
}

/*
 * Check if port has been added.
 */
int
spp_check_added_port(enum port_type iface_type, int iface_no)
{
	struct spp_port_info *port = get_iface_info(iface_type, iface_no);
	return port->iface_type != UNDEF;
}

/*
 * Check if port has been flushed.
 */
int
spp_check_flush_port(enum port_type iface_type, int iface_no)
{
	struct spp_port_info *port = get_iface_info(iface_type, iface_no);
	return port->dpdk_port >= 0;
}

/*
 * Check if component is using port.
 */
int
spp_check_used_port(
		enum port_type iface_type,
		int iface_no,
		enum spp_port_rxtx rxtx)
{
	int cnt, port_cnt, max = 0;
	struct spp_component_info *component = NULL;
	struct spp_port_info **port_array = NULL;
	struct spp_port_info *port = get_iface_info(iface_type, iface_no);

	if (port == NULL)
		return SPP_RET_NG;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		component = &g_component_info[cnt];
		if (component->type == SPP_COMPONENT_UNUSE)
			continue;

		if (rxtx == SPP_PORT_RXTX_RX) {
			max = component->num_rx_port;
			port_array = component->rx_ports;
		} else if (rxtx == SPP_PORT_RXTX_TX) {
			max = component->num_tx_port;
			port_array = component->tx_ports;
		}
		for (port_cnt = 0; port_cnt < max; port_cnt++) {
			if (unlikely(port_array[port_cnt] == port))
				return cnt;
		}
	}

	return SPP_RET_NG;
}

/*
 * Set port change to component.
 */
static void
set_component_change_port(struct spp_port_info *port, enum spp_port_rxtx rxtx)
{
	int ret = 0;
	if ((rxtx == SPP_PORT_RXTX_RX) || (rxtx == SPP_PORT_RXTX_ALL)) {
		ret = spp_check_used_port(port->iface_type, port->iface_no,
				SPP_PORT_RXTX_RX);
		if (ret >= 0)
			g_change_component[ret] = 1;
	}

	if ((rxtx == SPP_PORT_RXTX_TX) || (rxtx == SPP_PORT_RXTX_ALL)) {
		ret = spp_check_used_port(port->iface_type, port->iface_no,
				SPP_PORT_RXTX_TX);
		if (ret >= 0)
			g_change_component[ret] = 1;
	}
}

int
spp_update_classifier_table(
		enum spp_command_action action,
		enum spp_classifier_type type __attribute__ ((unused)),
		int vid,
		const char *mac_addr_str,
		const struct spp_port_index *port)
{
	struct spp_port_info *port_info = NULL;
	int64_t ret_mac = 0;
	uint64_t mac_addr = 0;

	RTE_LOG(DEBUG, APP, "update_classifier_table ( type = mac, mac addr = %s, port = %d:%d )\n",
			mac_addr_str, port->iface_type, port->iface_no);

	ret_mac = spp_change_mac_str_to_int64(mac_addr_str);
	if (unlikely(ret_mac == -1)) {
		RTE_LOG(ERR, APP, "MAC address format error. ( mac = %s )\n",
				mac_addr_str);
		return SPP_RET_NG;
	}
	mac_addr = (uint64_t)ret_mac;

	port_info = get_iface_info(port->iface_type, port->iface_no);
	if (unlikely(port_info == NULL)) {
		RTE_LOG(ERR, APP, "No port. ( port = %d:%d )\n",
				port->iface_type, port->iface_no);
		return SPP_RET_NG;
	}
	if (unlikely(port_info->iface_type == UNDEF)) {
		RTE_LOG(ERR, APP, "Port not added. ( port = %d:%d )\n",
				port->iface_type, port->iface_no);
		return SPP_RET_NG;
	}

	if (action == SPP_CMD_ACTION_DEL) {
		/* Delete */
		if ((port_info->class_id.vlantag.vid != 0) &&
				unlikely(port_info->class_id.vlantag.vid !=
				vid)) {
			RTE_LOG(ERR, APP, "VLAN ID is different. ( vid = %d )\n",
					vid);
			return SPP_RET_NG;
		}
		if ((port_info->class_id.mac_addr != 0) &&
				unlikely(port_info->class_id.mac_addr !=
						mac_addr)) {
			RTE_LOG(ERR, APP, "MAC address is different. ( mac = %s )\n",
					mac_addr_str);
			return SPP_RET_NG;
		}

		port_info->class_id.vlantag.vid = ETH_VLAN_ID_MAX;
		port_info->class_id.mac_addr    = 0;
		memset(port_info->class_id.mac_addr_str, 0x00, SPP_MIN_STR_LEN);
	} else if (action == SPP_CMD_ACTION_ADD) {
		/* Setting */
		if (unlikely(port_info->class_id.vlantag.vid !=
				ETH_VLAN_ID_MAX)) {
			RTE_LOG(ERR, APP, "Port in used. ( port = %d:%d, vlan = %d != %d )\n",
					port->iface_type, port->iface_no,
					port_info->class_id.vlantag.vid, vid);
			return SPP_RET_NG;
		}
		if (unlikely(port_info->class_id.mac_addr != 0)) {
			RTE_LOG(ERR, APP, "Port in used. ( port = %d:%d, mac = %s != %s )\n",
					port->iface_type, port->iface_no,
					port_info->class_id.mac_addr_str,
					mac_addr_str);
			return SPP_RET_NG;
		}

		port_info->class_id.vlantag.vid = vid;
		port_info->class_id.mac_addr    = mac_addr;
		strcpy(port_info->class_id.mac_addr_str, mac_addr_str);
	}

	set_component_change_port(port_info, SPP_PORT_RXTX_TX);
	return SPP_RET_OK;
}

/* Get free component */
static int
get_free_component(void)
{
	int cnt = 0;
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (g_component_info[cnt].type == SPP_COMPONENT_UNUSE)
			return cnt;
	}
	return -1;
}

/* Get name matching component */
int
spp_get_component_id(const char *name)
{
	int cnt = 0;
	if (name[0] == '\0')
		return SPP_RET_NG;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (strcmp(name, g_component_info[cnt].name) == 0)
			return cnt;
	}
	return SPP_RET_NG;
}

static int
get_del_core_element(int info, int num, int *array)
{
	int cnt;
	int match = -1;
	int max = num;

	for (cnt = 0; cnt < max; cnt++) {
		if (info == array[cnt])
			match = cnt;
	}

	if (match < 0)
		return -1;

	/* Last element is excluded from movement. */
	max--;

	for (cnt = match; cnt < max; cnt++)
		array[cnt] = array[cnt+1];

	/* Last element is cleared. */
	array[cnt] = 0;
	return 0;
}

/* Component command to execute it */
int
spp_update_component(
		enum spp_command_action action,
		const char *name,
		unsigned int lcore_id,
		enum spp_component_type type)
{
	int ret = SPP_RET_NG;
	int ret_del = -1;
	int component_id = 0;
	unsigned int tmp_lcore_id = 0;
	struct spp_component_info *component = NULL;
	struct core_info *core = NULL;
	struct core_mng_info *info = NULL;

	switch (action) {
	case SPP_CMD_ACTION_START:
		info = &g_core_info[lcore_id];
		if (info->status == SPP_CORE_UNUSE) {
			RTE_LOG(ERR, APP, "Core unavailable.\n");
			return SPP_RET_NG;
		}

		component_id = spp_get_component_id(name);
		if (component_id >= 0) {
			RTE_LOG(ERR, APP, "Component name in used.\n");
			return SPP_RET_NG;
		}

		component_id = get_free_component();
		if (component_id < 0) {
			RTE_LOG(ERR, APP, "Component upper limit is over.\n");
			return SPP_RET_NG;
		}

		core = &info->core[info->upd_index];
		if ((core->type != SPP_COMPONENT_UNUSE) &&
				(core->type != type)) {
			RTE_LOG(ERR, APP, "Component type is error.\n");
			return SPP_RET_NG;
		}

		component = &g_component_info[component_id];
		memset(component, 0x00, sizeof(struct spp_component_info));
		strcpy(component->name, name);
		component->type         = type;
		component->lcore_id     = lcore_id;
		component->component_id = component_id;

		core->type = type;
		core->id[core->num] = component_id;
		core->num++;
		ret = SPP_RET_OK;
		tmp_lcore_id = lcore_id;
		g_change_component[component_id] = 1;
		break;

	case SPP_CMD_ACTION_STOP:
		component_id = spp_get_component_id(name);
		if (component_id < 0)
			return SPP_RET_OK;

		component = &g_component_info[component_id];
		tmp_lcore_id = component->lcore_id;
		memset(component, 0x00, sizeof(struct spp_component_info));

		info = &g_core_info[tmp_lcore_id];
		core = &info->core[info->upd_index];
		ret_del = get_del_core_element(component_id,
				core->num, core->id);
		if (ret_del >= 0)
			/* If deleted, decrement number. */
			core->num--;

		if (core->num == 0)
			core->type = SPP_COMPONENT_UNUSE;

		ret = SPP_RET_OK;
		g_change_component[component_id] = 0;
		break;

	default:
		break;
	}

	g_change_core[tmp_lcore_id] = 1;
	return ret;
}

static int
check_port_element(
		struct spp_port_info *info,
		int num,
		struct spp_port_info *array[])
{
	int cnt = 0;
	int match = -1;
	for (cnt = 0; cnt < num; cnt++) {
		if (info == array[cnt])
			match = cnt;
	}
	return match;
}

static int
get_del_port_element(
		struct spp_port_info *info,
		int num,
		struct spp_port_info *array[])
{
	int cnt = 0;
	int match = -1;
	int max = num;

	match = check_port_element(info, num, array);
	if (match < 0)
		return -1;

	/* Last element is excluded from movement. */
	max--;

	for (cnt = match; cnt < max; cnt++)
		array[cnt] = array[cnt+1];

	/* Last element is cleared. */
	array[cnt] = NULL;
	return 0;
}

/* Port add or del to execute it */
int
spp_update_port(enum spp_command_action action,
		const struct spp_port_index *port,
		enum spp_port_rxtx rxtx,
		const char *name,
		const struct spp_port_ability *ability)
{
	int ret = SPP_RET_NG;
	int ret_check = -1;
	int ret_del = -1;
	int component_id = 0;
	int cnt = 0;
	struct spp_component_info *component = NULL;
	struct spp_port_info *port_info = NULL;
	int *num = NULL;
	struct spp_port_info **ports = NULL;

	component_id = spp_get_component_id(name);
	if (component_id < 0) {
		RTE_LOG(ERR, APP, "Unknown component by port command. (component = %s)\n",
				name);
		return SPP_RET_NG;
	}

	component = &g_component_info[component_id];
	port_info = get_iface_info(port->iface_type, port->iface_no);
	if (rxtx == SPP_PORT_RXTX_RX) {
		num = &component->num_rx_port;
		ports = component->rx_ports;
	} else {
		num = &component->num_tx_port;
		ports = component->tx_ports;
	}

	switch (action) {
	case SPP_CMD_ACTION_ADD:
		ret_check = check_port_element(port_info, *num, ports);
		if (ret_check >= 0)
			return SPP_RET_OK;

		if (*num >= RTE_MAX_ETHPORTS) {
			RTE_LOG(ERR, APP, "Port upper limit is over.\n");
			break;
		}

		if (ability->ope != SPP_PORT_ABILITY_OPE_NONE) {
			while ((cnt < SPP_PORT_ABILITY_MAX) &&
					(port_info->ability[cnt].ope !=
					SPP_PORT_ABILITY_OPE_NONE)) {
				cnt++;
			}
			if (cnt >= SPP_PORT_ABILITY_MAX) {
				RTE_LOG(ERR, APP, "No space of port ability.\n");
				return SPP_RET_NG;
			}
			memcpy(&port_info->ability[cnt], ability,
					sizeof(struct spp_port_ability));
		}

		port_info->iface_type = port->iface_type;
		ports[*num] = port_info;
		(*num)++;

		ret = SPP_RET_OK;
		break;

	case SPP_CMD_ACTION_DEL:
		for (cnt = 0; cnt < SPP_PORT_ABILITY_MAX; cnt++) {
			if (port_info->ability[cnt].ope ==
					SPP_PORT_ABILITY_OPE_NONE)
				continue;

			if (port_info->ability[cnt].rxtx == rxtx)
				memset(&port_info->ability[cnt], 0x00,
					sizeof(struct spp_port_ability));
		}

		ret_del = get_del_port_element(port_info, *num, ports);
		if (ret_del == 0)
			(*num)--; /* If deleted, decrement number. */

		ret = SPP_RET_OK;
		break;
	default:
		break;
	}

	g_change_component[component_id] = 1;
	return ret;
}

/* Flush initial setting of each interface. */
static int
flush_port(void)
{
	int ret = 0;
	int cnt = 0;
	struct spp_port_info *port = NULL;

	/* Initialize added vhost. */
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &g_iface_info.vhost[cnt];
		if ((port->iface_type != UNDEF) && (port->dpdk_port < 0)) {
			ret = add_vhost_pmd(port->iface_no,
					g_startup_param.vhost_client);
			if (ret < 0)
				return SPP_RET_NG;
			port->dpdk_port = ret;
		}
	}

	/* Initialize added ring. */
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &g_iface_info.ring[cnt];
		if ((port->iface_type != UNDEF) && (port->dpdk_port < 0)) {
			ret = add_ring_pmd(port->iface_no);
			if (ret < 0)
				return SPP_RET_NG;
			port->dpdk_port = ret;
		}
	}
	return SPP_RET_OK;
}

/* Flush changed core. */
static void
flush_core(void)
{
	int cnt = 0;
	struct core_mng_info *info = NULL;

	/* Changed core has changed index. */
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (g_change_core[cnt] != 0) {
			info = &g_core_info[cnt];
			info->upd_index = info->ref_index;
		}
	}

	/* Waiting for changed core change. */
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (g_change_core[cnt] != 0) {
			info = &g_core_info[cnt];
			while (likely(info->ref_index == info->upd_index))
				rte_delay_us_block(SPP_CHANGE_UPDATE_INTERVAL);

			memcpy(&info->core[info->upd_index],
					&info->core[info->ref_index],
					sizeof(struct core_info));
		}
	}
}

/* Flush changed component */
static int
flush_component(void)
{
	int ret = 0;
	int cnt = 0;
	struct spp_component_info *component_info = NULL;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (g_change_component[cnt] == 0)
			continue;

		component_info = &g_component_info[cnt];
		spp_port_ability_update(component_info);

		if (component_info->type == SPP_COMPONENT_CLASSIFIER_MAC)
			ret = spp_classifier_mac_update(component_info);
		else
			ret = spp_forward_update(component_info);

		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, APP, "Flush error. ( component = %s, type = %d)\n",
					component_info->name,
					component_info->type);
			return SPP_RET_NG;
		}
	}
	return SPP_RET_OK;
}

/* Flush command to execute it */
int
spp_flush(void)
{
	int ret = -1;

	/* Initial setting of each interface. */
	ret = flush_port();
	if (ret < 0)
		return ret;

	/* Flush of core index. */
	flush_core();

	/* Flush of component */
	ret = flush_component();

	backup_mng_info(&g_backup_info);
	return ret;
}

/* Cancel data that is not flushing */
void
spp_cancel(void)
{
	cancel_mng_info(&g_backup_info);
}

/* Iterate core information */
int
spp_iterate_core_info(struct spp_iterate_core_params *params)
{
	int ret;
	int lcore_id, cnt;
	struct core_info *core = NULL;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (spp_get_core_status(lcore_id) == SPP_CORE_UNUSE)
			continue;

		core = get_core_info(lcore_id);
		if (core->num == 0) {
			ret = (*params->element_proc)(
				params, lcore_id,
				"", SPP_TYPE_UNUSE_STR,
				0, NULL, 0, NULL);
			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, APP, "Cannot iterate core information. "
						"(core = %d, type = %d)\n",
						lcore_id, SPP_COMPONENT_UNUSE);
				return SPP_RET_NG;
			}
			continue;
		}

		for (cnt = 0; cnt < core->num; cnt++) {
			if (core->type == SPP_COMPONENT_CLASSIFIER_MAC) {
				ret = spp_classifier_get_component_status(
						lcore_id,
						core->id[cnt],
						params);
			} else {
				ret = spp_forward_get_component_status(
						lcore_id,
						core->id[cnt],
						params);
			}
			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, APP, "Cannot iterate core information. "
						"(core = %d, type = %d)\n",
						lcore_id, core->type);
				return SPP_RET_NG;
			}
		}
	}

	return SPP_RET_OK;
}

/* Iterate Classifier_table */
int
spp_iterate_classifier_table(
		struct spp_iterate_classifier_table_params *params)
{
	int ret;

	ret = spp_classifier_mac_iterate_table(params);
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, APP, "Cannot iterate classifier_mac_table.\n");
		return SPP_RET_NG;
	}

	return SPP_RET_OK;
}

/* Get the port number of DPDK. */
int
spp_get_dpdk_port(enum port_type iface_type, int iface_no)
{
	switch (iface_type) {
	case PHY:
		return g_iface_info.nic[iface_no].dpdk_port;
	case RING:
		return g_iface_info.ring[iface_no].dpdk_port;
	case VHOST:
		return g_iface_info.vhost[iface_no].dpdk_port;
	default:
		return -1;
	}
}

/**
 * Separate port id of combination of iface type and number and
 * assign to given argument, iface_type and iface_no.
 *
 * For instance, 'ring:0' is separated to 'ring' and '0'.
 */
int
spp_get_iface_index(const char *port, enum port_type *iface_type, int *iface_no)
{
	enum port_type type = UNDEF;
	const char *no_str = NULL;
	char *endptr = NULL;

	/* Find out which type of interface from port */
	if (strncmp(port, SPP_IFTYPE_NIC_STR ":",
			strlen(SPP_IFTYPE_NIC_STR)+1) == 0) {
		/* NIC */
		type = PHY;
		no_str = &port[strlen(SPP_IFTYPE_NIC_STR)+1];
	} else if (strncmp(port, SPP_IFTYPE_VHOST_STR ":",
			strlen(SPP_IFTYPE_VHOST_STR)+1) == 0) {
		/* VHOST */
		type = VHOST;
		no_str = &port[strlen(SPP_IFTYPE_VHOST_STR)+1];
	} else if (strncmp(port, SPP_IFTYPE_RING_STR ":",
			strlen(SPP_IFTYPE_RING_STR)+1) == 0) {
		/* RING */
		type = RING;
		no_str = &port[strlen(SPP_IFTYPE_RING_STR)+1];
	} else {
		/* OTHER */
		RTE_LOG(ERR, APP, "Unknown interface type. (port = %s)\n",
				port);
		return -1;
	}

	/* Change type of number of interface */
	int ret_no = strtol(no_str, &endptr, 0);
	if (unlikely(no_str == endptr) || unlikely(*endptr != '\0')) {
		/* No IF number */
		RTE_LOG(ERR, APP, "No interface number. (port = %s)\n", port);
		return -1;
	}

	*iface_type = type;
	*iface_no = ret_no;

	RTE_LOG(DEBUG, APP, "Port = %s => Type = %d No = %d\n",
			port, *iface_type, *iface_no);
	return 0;
}

/**
 * Generate a formatted string of combination from interface type and
 * number and assign to given 'port'
 */
int spp_format_port_string(char *port, enum port_type iface_type, int iface_no)
{
	const char *iface_type_str;

	switch (iface_type) {
	case PHY:
		iface_type_str = SPP_IFTYPE_NIC_STR;
		break;
	case RING:
		iface_type_str = SPP_IFTYPE_RING_STR;
		break;
	case VHOST:
		iface_type_str = SPP_IFTYPE_VHOST_STR;
		break;
	default:
		return -1;
	}

	sprintf(port, "%s:%d", iface_type_str, iface_no);

	return 0;
}

/**
 * Change mac address of 'aa:bb:cc:dd:ee:ff' to int64 and return it
 */
int64_t
spp_change_mac_str_to_int64(const char *mac)
{
	int64_t ret_mac = 0;
	int64_t token_val = 0;
	int token_cnt = 0;
	char tmp_mac[SPP_MIN_STR_LEN];
	char *str = tmp_mac;
	char *saveptr = NULL;
	char *endptr = NULL;

	RTE_LOG(DEBUG, APP, "MAC address change. (mac = %s)\n", mac);

	strcpy(tmp_mac, mac);
	while (1) {
		/* Split by colon(':') */
		char *ret_tok = strtok_r(str, ":", &saveptr);
		if (unlikely(ret_tok == NULL))
			break;

		/* Check for mal-formatted address */
		if (unlikely(token_cnt >= ETHER_ADDR_LEN)) {
			RTE_LOG(ERR, APP, "MAC address format error. (mac = %s)\n",
					mac);
			return -1;
		}

		/* Convert string to hex value */
		int ret_tol = strtol(ret_tok, &endptr, 16);
		if (unlikely(ret_tok == endptr) || unlikely(*endptr != '\0'))
			break;

		/* Append separated value to the result */
		token_val = (int64_t)ret_tol;
		ret_mac |= token_val << (token_cnt * 8);
		token_cnt++;
		str = NULL;
	}

	RTE_LOG(DEBUG, APP, "MAC address change. (mac = %s => 0x%08lx)\n",
			 mac, ret_mac);
	return ret_mac;
}

/**
 * Return the type of forwarder as a member of enum of spp_component_type
 */
enum spp_component_type
spp_change_component_type(const char *type_str)
{
	if (strncmp(type_str, CORE_TYPE_CLASSIFIER_MAC_STR,
			 strlen(CORE_TYPE_CLASSIFIER_MAC_STR)+1) == 0) {
		/* Classifier */
		return SPP_COMPONENT_CLASSIFIER_MAC;
	} else if (strncmp(type_str, CORE_TYPE_MERGE_STR,
			 strlen(CORE_TYPE_MERGE_STR)+1) == 0) {
		/* Merger */
		return SPP_COMPONENT_MERGE;
	} else if (strncmp(type_str, CORE_TYPE_FORWARD_STR,
			 strlen(CORE_TYPE_FORWARD_STR)+1) == 0) {
		/* Forwarder */
		return SPP_COMPONENT_FORWARD;
	}
	return SPP_COMPONENT_UNUSE;
}
