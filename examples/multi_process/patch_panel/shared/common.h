/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#define MAX_CLIENT  99
#define MSG_SIZE    1000
#define SOCK_RESET  -1
#define PORT_RESET  -99

/*
 * When doing reads from the NIC or the client queues,
 * use this batch size
 */
#define PACKET_READ_SIZE 32
#define MAX_PKT_BURST 32

#define MAX_PARAMETER 10

#define MBUFS_PER_CLIENT 1536
#define MBUFS_PER_PORT 1536
#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512

#define NO_FLAGS 0

/* Command. */
enum cmd_type {
	STOP,
	START,
	FORWARD,
};

/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the clients, so we have a distinct set, on
 * different cache lines for each client to use.
 */

struct stats {
	uint64_t rx;
	uint64_t rx_drop;
	uint64_t tx;
	uint64_t tx_drop;
} __rte_cache_aligned;

struct port_info {
	uint8_t num_ports;
	uint8_t id[RTE_MAX_ETHPORTS];
	struct stats port_stats[RTE_MAX_ETHPORTS];
	struct stats client_stats[MAX_CLIENT];
};

enum port_type {
	PHY,
	RING,
	VHOST,
	UNDEF,
};

struct port_map {
	int id;
	enum port_type port_type;
	struct stats *stats;
	struct stats default_stats;
};

struct port {
	int in_port_id;
	int out_port_id;
	uint16_t (*rx_func)(uint8_t, uint16_t, struct rte_mbuf **, uint16_t);
	uint16_t (*tx_func)(uint8_t, uint16_t, struct rte_mbuf **, uint16_t);
};

/* define common names for structures shared between server and client */
#define MP_CLIENT_RXQ_NAME "eth_ring%u"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define VM_PKTMBUF_POOL_NAME "VM_Proc_pktmbuf_pool"
#define VM_MZ_PORT_INFO "VM_Proc_port_info"
#define MZ_PORT_INFO "MProc_port_info"
#define VHOST_BACKEND_NAME "eth_vhost%u"
#define VHOST_IFACE_NAME "/tmp/sock%u"

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned int id)
{
	/*
	 * buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(MP_CLIENT_RXQ_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_RXQ_NAME, id);
	return buffer;
}

static inline const char *
get_vhost_backend_name(unsigned int id)
{
	/*
	 * buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(VHOST_BACKEND_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, VHOST_BACKEND_NAME, id);
	return buffer;
}

static inline char *
get_vhost_iface_name(unsigned int id)
{
	/*
	 * buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(VHOST_IFACE_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, VHOST_IFACE_NAME, id);
	return buffer;
}

void check_all_ports_link_status(struct port_info *ports, uint8_t port_num,
		uint32_t port_mask);

int init_port(uint8_t port_num, struct rte_mempool *pktmbuf_pool);

int parse_portmask(struct port_info *ports, uint8_t max_ports,
		const char *portmask);
int parse_num_clients(uint8_t *num_clients, const char *clients);
int parse_server(char **server_ip, int *server_port, char *server_addr);

int spp_atoi(const char *str, int *val);

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#endif
