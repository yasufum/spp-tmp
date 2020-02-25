/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "shared/port_manager.h"

struct porttype_map portmap[] = {
	{ .port_name = "phy",   .port_type = PHY, },
	{ .port_name = "ring",  .port_type = RING, },
	{ .port_name = "vhost", .port_type = VHOST, },
	{ .port_name = "pcap", .port_type = PCAP, },
	{ .port_name = "nullpmd", .port_type = NULLPMD, },
	{ .port_name = "tap", .port_type = TAP, },
	{ .port_name = "memif", .port_type = MEMIF, },
	{ .port_name = NULL,    .port_type = UNDEF, },
};

void
forward_array_init_one(unsigned int i, unsigned int j)
{
	ports_fwd_array[i][j].in_port_id = PORT_RESET;
	ports_fwd_array[i][j].in_queue_id = 0;
	ports_fwd_array[i][j].out_port_id = PORT_RESET;
	ports_fwd_array[i][j].out_queue_id = 0;
}

/* initialize forward array with default value */
void
forward_array_init(void)
{
	unsigned int i, j;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		for (j = 0; j < RTE_MAX_QUEUES_PER_PORT; j++)
			forward_array_init_one(i, j);
	}
}

void
forward_array_reset(void)
{
	unsigned int i, j;
	uint16_t max_queue;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		max_queue = get_port_max_queues(i);

		for (j = 0; j < max_queue; j++) {
			if (ports_fwd_array[i][j].in_port_id != PORT_RESET) {
				ports_fwd_array[i][j].out_port_id = PORT_RESET;
				RTE_LOG(INFO, SHARED, "Port ID %d\n", i);
				RTE_LOG(INFO, SHARED, "Queue ID %d\n", j);
				RTE_LOG(INFO, SHARED, "out_port_id %d\n",
					ports_fwd_array[i][j].out_port_id);
			}
		}
	}
}

void
port_map_init_one(unsigned int i)
{
	port_map[i].id = PORT_RESET;
	port_map[i].port_type = UNDEF;
	port_map[i].stats = &port_map[i].default_stats;
	port_map[i].queue_info = NULL;
}

void
port_map_init(void)
{
	unsigned int i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		port_map_init_one(i);
}

/* Return -1 as an error if given patch is invalid */
int
add_patch(uint16_t in_port, uint16_t in_queue,
		uint16_t out_port, uint16_t out_queue)
{
	if (!is_valid_port(in_port, in_queue) ||
		!is_valid_port(out_port, out_queue))
		return -1;

	if (!is_valid_port_rxq(in_port, in_queue) ||
		!is_valid_port_txq(out_port, out_queue))
		return 1;

	/* Populate in port data */
	ports_fwd_array[in_port][in_queue].in_port_id = in_port;
	ports_fwd_array[in_port][in_queue].in_queue_id = in_queue;
	ports_fwd_array[in_port][in_queue].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[in_port][in_queue].tx_func = &rte_eth_tx_burst;
	ports_fwd_array[in_port][in_queue].out_port_id = out_port;
	ports_fwd_array[in_port][in_queue].out_queue_id = out_queue;

	/* Populate out port data */
	ports_fwd_array[out_port][out_queue].in_port_id = out_port;
	ports_fwd_array[out_port][out_queue].in_queue_id = out_queue;
	ports_fwd_array[out_port][out_queue].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[out_port][out_queue].tx_func = &rte_eth_tx_burst;

	RTE_LOG(DEBUG, SHARED, "STATUS: in port %d in queue %d"
		" in_port_id %d in_queue_id %d\n",
		in_port, in_queue,
		ports_fwd_array[in_port][in_queue].in_port_id,
		ports_fwd_array[in_port][in_queue].in_queue_id);
	RTE_LOG(DEBUG, SHARED, "STATUS: in port %d in queue %d"
		" patch out_port_id %d out_queue_id %d\n",
		in_port, in_queue,
		ports_fwd_array[in_port][in_queue].out_port_id,
		ports_fwd_array[in_port][in_queue].out_queue_id);
	RTE_LOG(DEBUG, SHARED, "STATUS: out port %d out queue %d"
		" in_port_id %d in_queue_id %d\n",
		out_port, out_queue,
		ports_fwd_array[out_port][out_queue].in_port_id,
		ports_fwd_array[out_port][out_queue].in_queue_id);

	return 0;
}

/*
 * Return actual port ID which is assigned by system internally, or PORT_RESET
 * if port is not found.
 */
uint16_t
find_port_id(int id, enum port_type type)
{
	uint16_t port_id = PORT_RESET;
	uint16_t i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (port_map[i].port_type != type)
			continue;

		if (port_map[i].id == id) {
			port_id = i;
			break;
		}
	}

	return port_id;
}

/* Return 0 if invalid */
int
is_valid_port(uint16_t port_id, uint16_t queue_id)
{
	uint16_t max_queue;

	if (port_id > RTE_MAX_ETHPORTS)
		return 0;

	if (port_map[port_id].id == PORT_RESET)
		return 0;

	max_queue = get_port_max_queues(port_id);
	if (queue_id >= max_queue)
		return 0;

	return 1;
}

/*
 * Check if rxq exceeds the number of queues defined for the port.
 * Return 0 if invalid.
 */
int
is_valid_port_rxq(uint16_t port_id, uint16_t rxq)
{
	uint16_t nof_queues;

	if (port_map[port_id].queue_info != NULL) {
		nof_queues = port_map[port_id].queue_info->rxq;
	} else {
		/* default number of queues is 1 */
		nof_queues = 1;
	}
	if (rxq >= nof_queues)
		return 0;

	return 1;
}

/*
 * Check if txq exceeds the number of queues defined for the port.
 * Return 0 if invalid.
 */
int
is_valid_port_txq(uint16_t port_id, uint16_t txq)
{
	uint16_t nof_queues;

	if (port_map[port_id].queue_info != NULL) {
		nof_queues = port_map[port_id].queue_info->txq;
	} else {
		/* default number of queues is 1 */
		nof_queues = 1;
	}
	if (txq >= nof_queues)
		return 0;

	return 1;
}

void
forward_array_remove(int port_id, uint16_t queue_id)
{
	unsigned int i, j;
	uint16_t max_queue;
	int remove_flg = 0;

	/* Update ports_fwd_array */
	forward_array_init_one(port_id, queue_id);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		max_queue = get_port_max_queues(i);

		for (j = 0; j < max_queue; j++) {
			if (ports_fwd_array[i][j].in_port_id == PORT_RESET &&
				(ports_fwd_array[i][j].out_port_id != port_id ||
				ports_fwd_array[i][j].out_queue_id != queue_id))
				continue;

			ports_fwd_array[i][j].out_port_id = PORT_RESET;
			remove_flg = 1;
			break;
		}

		if (remove_flg)
			break;
	}
}

/* Return a type of port as a enum member of porttype_map structure. */
enum port_type get_port_type(char *portname)
{
	int i;
	for (i = 0; portmap[i].port_name != NULL; i++) {
		const char *port_name = portmap[i].port_name;
		if (strncmp(portname, port_name, strlen(port_name)) == 0)
			return portmap[i].port_type;
	}
	return UNDEF;
}

/* Returns a larger number of queues of RX or TX port as the maximum number */
uint16_t
get_port_max_queues(uint16_t port_id)
{
	uint16_t max_queue = 1; /* default max_queue is 1 */

	if (port_map[port_id].queue_info != NULL) {
		if (port_map[port_id].queue_info->rxq >=
			port_map[port_id].queue_info->txq)
			max_queue = port_map[port_id].queue_info->rxq;
		else
			max_queue = port_map[port_id].queue_info->txq;
	}
	return max_queue;
}
