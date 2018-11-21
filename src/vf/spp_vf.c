/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
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
	int id[RTE_MAX_LCORE];
			/* ID list of components executed on cpu core */
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
			" -s SERVER_IP:SERVER_PORT  :"
			" Access information to the server\n"
			" --vhost-client            : Run vhost on client\n"
			, progname);
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
	if (unlikely(&server_str[pos+1] == endptr) ||
				unlikely(*endptr != '\0'))
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
			"app opts (client_id=%d,server=%s:%d,"
			"vhost_client=%d)\n",
			g_startup_param.client_id,
			g_startup_param.server_ip,
			g_startup_param.server_port,
			g_startup_param.vhost_client);
	return 0;
}

/* Get component type being updated on target core */
enum spp_component_type
spp_get_component_type_update(unsigned int lcore_id)
{
	struct core_mng_info *info = &g_core_info[lcore_id];
	return info->core[info->upd_index].type;
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

		if (spp_check_core_update(lcore_id) == SPP_RET_OK) {
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

	RTE_LOG(DEBUG, APP, "update_classifier_table "
			"( type = mac, mac addr = %s, port = %d:%d )\n",
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
			RTE_LOG(ERR, APP, "VLAN ID is different. "
					"( vid = %d )\n", vid);
			return SPP_RET_NG;
		}
		if ((port_info->class_id.mac_addr != 0) &&
				unlikely(port_info->class_id.mac_addr !=
						mac_addr)) {
			RTE_LOG(ERR, APP, "MAC address is different. "
					"( mac = %s )\n", mac_addr_str);
			return SPP_RET_NG;
		}

		port_info->class_id.vlantag.vid = ETH_VLAN_ID_MAX;
		port_info->class_id.mac_addr    = 0;
		memset(port_info->class_id.mac_addr_str, 0x00,
							SPP_MIN_STR_LEN);
	} else if (action == SPP_CMD_ACTION_ADD) {
		/* Setting */
		if (unlikely(port_info->class_id.vlantag.vid !=
				ETH_VLAN_ID_MAX)) {
			RTE_LOG(ERR, APP, "Port in used. "
					"( port = %d:%d, vlan = %d != %d )\n",
					port->iface_type, port->iface_no,
					port_info->class_id.vlantag.vid, vid);
			return SPP_RET_NG;
		}
		if (unlikely(port_info->class_id.mac_addr != 0)) {
			RTE_LOG(ERR, APP, "Port in used. "
					"( port = %d:%d, mac = %s != %s )\n",
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
		ret_del = del_component_info(component_id,
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
		RTE_LOG(ERR, APP, "Unknown component by port command. "
				"(component = %s)\n", name);
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
				RTE_LOG(ERR, APP,
						"No space of port ability.\n");
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
				RTE_LOG(ERR, APP, "Cannot iterate core "
						"information. "
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
				RTE_LOG(ERR, APP, "Cannot iterate core "
						"information. "
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
spp_get_iface_index(const char *port,
		    enum port_type *iface_type,
		    int *iface_no)
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
