/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <getopt.h>

#include <rte_memory.h>

#include "shared/common.h"
#include "args.h"
#include "init.h"
#include "primary.h"

/* global var for number of rings - extern in header */
uint16_t num_rings;
char *server_ip;
int server_port;

/* Flag for deciding to forward */
int do_forwarding;

/*
 * Long options mapped to a short option.
 *
 * First long only option value must be >= 256, so that we won't
 * conflict with short options.
 */
enum {
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_OPT_DISP_STATS,
	CMD_OPT_PORT_NUM, /* For `--port-num` */
};

struct option lgopts[] = {
	{"disp-stats", no_argument, NULL, CMD_OPT_DISP_STATS},
	{"port-num", required_argument, NULL, CMD_OPT_PORT_NUM},
	{0}
};

static const char *progname;

/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	RTE_LOG(INFO, PRIMARY,
	    "%s [EAL options] -- -p PORTMASK -n NUM_CLIENTS [-s NUM_SOCKETS]"
		" [--port-num NUM_PORT"
		" rxq NUM_RX_QUEUE txq NUM_TX_QUEUE]...\n"
	    " -p PORTMASK: hexadecimal bitmask of ports to use\n"
	    " -n NUM_RINGS: number of ring ports used from secondaries\n"
		" --port-num NUM_PORT: number of ports for multi-queue setting\n"
		" rxq NUM_RX_QUEUE: number of receive queues\n"
		" txq NUM_TX_QUEUE number of transmit queues\n"
	    , progname);
}

int set_forwarding_flg(int flg)
{
	if (flg == 0 || flg == 1)
		do_forwarding = flg;
	else {
		RTE_LOG(ERR, PRIMARY, "Invalid value for forwarding flg.\n");
		return -1;
	}
	return 0;
}

int get_forwarding_flg(void)
{
	if (do_forwarding < 0) {
		RTE_LOG(ERR, PRIMARY, "Forwarding flg is not initialized.\n");
		return -1;
	}
	return do_forwarding;
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
int
parse_portmask(struct port_info *ports, uint16_t max_ports,
		const char *portmask)
{
	char *end = NULL;
	unsigned long pm;
	uint16_t count = 0;

	if (portmask == NULL || *portmask == '\0')
		return -1;

	/* convert parameter to a number and verify */
	pm = strtoul(portmask, &end, 16);
	if (end == NULL || *end != '\0' || pm == 0)
		return -1;

	/* loop through bits of the mask and mark ports */
	while (pm != 0) {
		if (pm & 0x01) { /* bit is set in mask, use port */
			if (count >= max_ports)
				RTE_LOG(WARNING, PRIMARY,
					"port %u not present - ignoring\n",
					count);
			else
				ports->id[ports->num_ports++] = count;
		}
		pm = (pm >> 1);
		count++;
	}

	return 0;
}

/**
 * Take the number of clients passed with `-n` option and convert to
 * to a number to store in the num_clients variable.
 */
static int
parse_nof_rings(uint16_t *num_clients, const char *clients)
{
	char *end = NULL;
	unsigned long temp;

	if (clients == NULL || *clients == '\0')
		return -1;

	temp = strtoul(clients, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;

	*num_clients = (uint16_t)temp;
	return 0;
}

/* Extract the number of queues from startup option. */
static int
parse_nof_queues(struct port_queue *arg_queues, const char *str_port_num,
		int option_index, uint16_t max_ports, int argc, char *argv[])
{
	char *end = NULL;
	unsigned long temp;
	uint16_t port_num, rxq, txq;


	if (str_port_num == NULL || *str_port_num == '\0') {
		RTE_LOG(ERR, PRIMARY,
			"PORT_NUM is not specified(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	/* Parameter check of port_num */
	temp = strtoul(str_port_num, &end, 10);
	if (end == NULL || *end != '\0') {
		RTE_LOG(ERR, PRIMARY,
			"PORT_NUM is not a number(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}
	port_num = (uint16_t)temp;

	if (port_num > max_ports) {
		RTE_LOG(ERR, PRIMARY,
			"PORT_NUM exceeds the number of available ports"
			"(%s:%d)\n",
			__func__, __LINE__);
		return 1;
	}

	/* Check if both 'rxq' and 'txq' are inclued in parameter string. */
	if (option_index + 3 > argc) {
		RTE_LOG(ERR, PRIMARY,
			"rxq NUM_RX_QUEUE txq NUM_TX_QUEUE is not specified"
			"(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	if (strcmp(argv[option_index], "rxq")) {
		RTE_LOG(ERR, PRIMARY,
			"rxq is not specified in the --port_num option"
			"(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	/* Parameter check of rxq */
	temp = strtoul(argv[option_index + 1], &end, 10);
	if (end == NULL || *end != '\0' || temp == 0) {
		RTE_LOG(ERR, PRIMARY,
			"NUM_RX_QUEUE is not a number(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}
	rxq = (uint16_t)temp;

	if (strcmp(argv[option_index + 2], "txq")) {
		RTE_LOG(ERR, PRIMARY,
			"txq is not specified in the --port_num option"
			"(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	/* Parameter check of txq */
	temp = strtoul(argv[option_index + 3], &end, 10);
	if (end == NULL || *end != '\0' || temp == 0) {
		RTE_LOG(ERR, PRIMARY,
			"NUM_TX_QUEUE is not a number(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}
	txq = (uint16_t)temp;

	arg_queues[port_num].rxq = rxq;
	arg_queues[port_num].txq = txq;

	return 0;
}

/**
 * Set the number of queues for port_id.
 * If not specified number of queue is set as 1.
 */
static int
set_nof_queues(struct port_info *ports, struct port_queue *arg_queues)
{
	int index;
	uint16_t port_id, rxq, txq;

	for (index = 0; index < ports->num_ports; index++) {
		port_id = ports->id[index];

		if (arg_queues[port_id].rxq == 0 ||
			arg_queues[port_id].txq == 0) {
			rxq = 1;
			txq = 1;
		} else {
			rxq = arg_queues[port_id].rxq;
			txq = arg_queues[port_id].txq;
		}

		ports->queue_info[index].rxq = rxq;
		ports->queue_info[index].txq = txq;
	}
	return 0;
}

/**
 * The application specific arguments follow the DPDK-specific
 * arguments which are stripped by the DPDK init. This function
 * processes these application arguments, printing usage info
 * on error.
 */
int
parse_app_args(uint16_t max_ports, int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	int ret;
	struct port_queue arg_queues[RTE_MAX_ETHPORTS] = { 0 };

	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "n:p:s:", lgopts,
		&option_index)) != EOF) {
		switch (opt) {
		case CMD_OPT_DISP_STATS:
			set_forwarding_flg(0);
			break;
		case 'p':
			if (parse_portmask(ports, max_ports, optarg) != 0) {
				usage();
				return -1;
			}
			break;
		case 'n':
			if (parse_nof_rings(&num_rings, optarg) != 0) {
				usage();
				return -1;
			}
			break;
		case 's':
			ret = parse_server(&server_ip, &server_port, optarg);
			if (ret != 0) {
				usage();
				return -1;
			}
			break;
		case CMD_OPT_PORT_NUM:
			ret = parse_nof_queues(arg_queues, optarg, optind,
					max_ports, argc, argv);
			if (ret != 0) {
				usage();
				return -1;
			}
			break;
		default:
			RTE_LOG(ERR,
				PRIMARY, "ERROR: Unknown option '%c'\n", opt);
			usage();
			return -1;
		}
	}

	if (ports->num_ports == 0 || num_rings == 0) {
		usage();
		return -1;
	}

	ret = set_nof_queues(ports, arg_queues);
	if (ret != 0) {
		usage();
		return -1;
	}

	return 0;
}
