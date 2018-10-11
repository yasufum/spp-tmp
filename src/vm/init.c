/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#include <limits.h>

#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include "args.h"
#include "common.h"
#include "init.h"

/* The mbuf pool for packet rx */
static struct rte_mempool *pktmbuf_pool;

/* the port details */
struct port_info *ports;

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(int total_ports)
{
	const unsigned int num_mbufs = total_ports * MBUFS_PER_PORT;

	if (num_mbufs == 0)
		return 0;

	/*
	 * don't pass single-producer/single-consumer flags to mbuf create as
	 * it seems faster to use a cache instead
	 */
	RTE_LOG(INFO, APP,
		"Lookup mbuf pool '%s' [%u mbufs] ...\n", VM_PKTMBUF_POOL_NAME,
		num_mbufs);

	pktmbuf_pool = rte_mempool_lookup(VM_PKTMBUF_POOL_NAME);
	if (pktmbuf_pool == NULL) {
		RTE_LOG(INFO, APP, "Creating mbuf pool '%s' [%u mbufs] ...\n",
			VM_PKTMBUF_POOL_NAME, num_mbufs);

		pktmbuf_pool = rte_mempool_create(VM_PKTMBUF_POOL_NAME,
			num_mbufs, MBUF_SIZE, MBUF_CACHE_SIZE,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
			rte_socket_id(), NO_FLAGS);
	}

	return (pktmbuf_pool == NULL); /* 0  on success */
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
int
init(int argc, char *argv[])
{
	int retval;
	const struct rte_memzone *mz;
	uint16_t count, total_ports;

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0)
		return -1;

	argc -= retval;
	argv += retval;

	/* get total number of ports */
	total_ports = rte_eth_dev_count_avail();

	/* set up array for port data */
	mz = rte_memzone_lookup(MZ_PORT_INFO);
	if (mz == NULL) {
		RTE_LOG(DEBUG, APP, "Cannot get port info structure\n");
		mz = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports),
			rte_socket_id(), NO_FLAGS);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot reserve memzone for port info\n");
		memset(mz->addr, 0, sizeof(*ports));
	}
	ports = mz->addr;

	/* parse additional, application arguments */
	retval = parse_app_args(total_ports, argc, argv);
	if (retval != 0)
		return -1;

	/* initialise mbuf pools */
	retval = init_mbuf_pools(total_ports);
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

	/* now initialise the ports we will use */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		for (count = 0; count < total_ports; count++) {
			retval = init_port(ports->id[count], pktmbuf_pool);
			if (retval != 0)
				rte_exit(EXIT_FAILURE,
					"Cannot initialise port %d\n", count);
		}
	}
	check_all_ports_link_status(ports, total_ports, (~0x0));

	return 0;
}
