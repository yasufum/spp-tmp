/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#include <getopt.h>

#include <rte_memory.h>

#include "args.h"
#include "common.h"
#include "init.h"

/* global var for number of clients - extern in header */
uint16_t num_clients;
char *server_ip;
int server_port;

static const char *progname;

/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	RTE_LOG(INFO, APP,
	    "%s [EAL options] -- -p PORTMASK -n NUM_CLIENTS [-s NUM_SOCKETS]\n"
	    " -p PORTMASK: hexadecimal bitmask of ports to use\n"
	    " -n NUM_CLIENTS: number of client processes to use\n"
	    , progname);
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
	struct option lgopts[] = { {0} };
	int ret;

	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "n:p:s:", lgopts,
		&option_index)) != EOF) {
		switch (opt) {
		case 'p':
			if (parse_portmask(ports, max_ports, optarg) != 0) {
				usage();
				return -1;
			}
			break;
		case 'n':
			if (parse_num_clients(&num_clients, optarg) != 0) {
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
		default:
			RTE_LOG(ERR, APP, "ERROR: Unknown option '%c'\n", opt);
			usage();
			return -1;
		}
	}

	if (ports->num_ports == 0 || num_clients == 0) {
		usage();
		return -1;
	}

	return 0;
}
