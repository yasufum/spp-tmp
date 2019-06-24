/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include "spp_vf.h"
#include "classifier_mac.h"
#include "forwarder.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/spp_worker_th/cmd_utils.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/spp_worker_th/cmd_runner.h"
#include "shared/secondary/spp_worker_th/cmd_parser.h"
#include "shared/secondary/spp_worker_th/spp_port.h"

/* Declare global variables */
/* Logical core ID for main process */
static unsigned int g_main_lcore_id = 0xffffffff;

/* Execution parameter of spp_vf */
static struct startup_param g_startup_param;

/* Interface management information */
static struct iface_info g_iface_info;

/* Component management information */
static struct sppwk_comp_info g_component_info[RTE_MAX_LCORE];

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
			" -s SERVER_IP:SERVER_PORT  :"
			" Access information to the server\n"
			" --vhost-client            : Run vhost on client\n"
			, progname);
}

/* Parse options for client app */
static int
parse_app_args(int argc, char *argv[])
{
	int cli_id;  /* Client ID. */
	char *ctl_ip;  /* IP address of spp_ctl. */
	int ctl_port;  /* Port num to connect spp_ctl. */
	int ret;
	int cnt;
	int option_index, opt;

	int proc_flg = 0;
	int server_flg = 0;

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
			if (parse_client_id(&cli_id, optarg) != SPP_RET_OK) {
				usage(progname);
				return SPP_RET_NG;
			}
			set_client_id(cli_id);

			proc_flg = 1;
			break;
		case SPP_LONGOPT_RETVAL_VHOST_CLIENT:
			g_enable_vhost_cli = 1;
			break;
		case 's':
			ret = parse_server(&ctl_ip, &ctl_port, optarg);
			set_spp_ctl_ip(ctl_ip);
			set_spp_ctl_port(ctl_port);
			if (ret != SPP_RET_OK) {
				usage(progname);
				return SPP_RET_NG;
			}
			server_flg = 1;
			break;
		default:
			usage(progname);
			return SPP_RET_NG;
		}
	}

	/* Check mandatory parameters */
	if ((proc_flg == 0) || (server_flg == 0)) {
		usage(progname);
		return SPP_RET_NG;
	}
	RTE_LOG(INFO, APP,
			"Parsed app args (client_id=%d,server=%s:%d,"
			"vhost_client=%d)\n",
			cli_id, ctl_ip, ctl_port, g_enable_vhost_cli);
	return SPP_RET_OK;
}

/* Main process of slave core */
static int
slave_main(void *arg __attribute__ ((unused)))
{
	int ret = 0;
	int cnt = 0;
	unsigned int lcore_id = rte_lcore_id();
	enum sppwk_lcore_status status = SPP_CORE_STOP;
	struct core_mng_info *info = &g_core_info[lcore_id];
	struct core_info *core = get_core_info(lcore_id);

	RTE_LOG(INFO, APP, "Core[%d] Start.\n", lcore_id);
	set_core_status(lcore_id, SPP_CORE_IDLE);

	while ((status = spp_get_core_status(lcore_id)) !=
			SPP_CORE_STOP_REQUEST) {
		if (status != SPP_CORE_FORWARD)
			continue;

		if (spp_check_core_update(lcore_id) == SPP_RET_OK) {
			/* Setting with the flush command trigger. */
			info->ref_index = (info->upd_index+1) % TWO_SIDES;
			core = get_core_info(lcore_id);
		}

		/* It is for processing multiple components. */
		for (cnt = 0; cnt < core->num; cnt++) {
			/* Component classification to call a function. */
			if (spp_get_component_type(core->id[cnt]) ==
					SPPWK_TYPE_CLS) {
				/* Component type for classifier. */
				ret = spp_classifier_mac_do(core->id[cnt]);
				if (unlikely(ret != 0))
					break;
			} else {
				/* Component type for forward or merge. */
				ret = forward_packets(core->id[cnt]);
				if (unlikely(ret != 0))
					break;
			}
		}
		if (unlikely(ret != 0)) {
			RTE_LOG(ERR, APP, "Core[%d] Component Error. "
					"(id = %d)\n",
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
 * Return SPP_RET_NG explicitly if error is occurred.
 */
int
main(int argc, char *argv[])
{
	int ret = SPP_RET_NG;
	char ctl_ip[IPADDR_LEN] = { 0 };
	int ctl_port;
	int ret_cmd_init;
	unsigned int lcore_id = 0;

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

		/* Parse spp_vf specific parameters */
		int ret_parse = parse_app_args(argc, argv);
		if (unlikely(ret_parse != SPP_RET_OK))
			break;

		/* Get lcore id of main thread to set its status after */
		g_main_lcore_id = rte_lcore_id();

		if (sppwk_set_mng_data(&g_startup_param, &g_iface_info,
					g_component_info, g_core_info,
					g_change_core, g_change_component,
					&g_backup_info,
					g_main_lcore_id) < SPP_RET_OK) {
			RTE_LOG(ERR, APP,
				"Failed to set management data.\n");
			break;
		}

		int ret_mng = init_mng_data();
		if (unlikely(ret_mng != SPP_RET_OK))
			break;

		int ret_classifier_mac_init = spp_classifier_mac_init();
		if (unlikely(ret_classifier_mac_init != SPP_RET_OK))
			break;

		init_forwarder();
		spp_port_ability_init();

		/* Setup connection for accepting commands from controller */
		get_spp_ctl_ip(ctl_ip);
		ctl_port = get_spp_ctl_port();
		ret_cmd_init = sppwk_cmd_runner_conn(ctl_ip, ctl_port);
		if (unlikely(ret_cmd_init != SPP_RET_OK))
			break;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
		int ret_ringlatency = spp_ringlatencystats_init(
				SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL,
				g_iface_info.num_ring);
		if (unlikely(ret_ringlatency != SPP_RET_OK))
			break;
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

		/* Start worker threads of classifier and forwarder */
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			rte_eal_remote_launch(slave_main, NULL, lcore_id);
		}

		/* Set the status of main thread to idle */
		g_core_info[g_main_lcore_id].status = SPP_CORE_IDLE;
		int ret_wait = check_core_status_wait(SPP_CORE_IDLE);
		if (unlikely(ret_wait != SPP_RET_OK))
			break;

		/* Start forwarding */
		set_all_core_status(SPP_CORE_FORWARD);
		RTE_LOG(INFO, APP, "My ID %d start handling message\n", 0);
		RTE_LOG(INFO, APP, "[Press Ctrl-C to quit ...]\n");

		/* Backup the management information after initialization */
		backup_mng_info(&g_backup_info);

		/* Enter loop for accepting commands */
		int ret_do = SPP_RET_OK;
#ifndef USE_UT_SPP_VF
		while (likely(g_core_info[g_main_lcore_id].status !=
				SPP_CORE_STOP_REQUEST)) {
#else
		{
#endif
			/* Receive command */
			ret_do = sppwk_run_cmd();
			if (unlikely(ret_do != SPP_RET_OK))
				break;

		       /*
			* Wait to avoid CPU overloaded.
			*/
			usleep(100);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
			print_ring_latency_stats();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
		}

		if (unlikely(ret_do != SPP_RET_OK)) {
			set_all_core_status(SPP_CORE_STOP_REQUEST);
			break;
		}

		ret = SPP_RET_OK;
		break;
	}

	/* Finalize to exit */
	if (g_main_lcore_id == rte_lcore_id()) {
		g_core_info[g_main_lcore_id].status = SPP_CORE_STOP;
		int ret_core_end = check_core_status_wait(SPP_CORE_STOP);
		if (unlikely(ret_core_end != SPP_RET_OK))
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
