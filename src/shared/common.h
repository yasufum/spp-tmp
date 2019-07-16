/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SHARED_COMMON_H_
#define _SHARED_COMMON_H_

#include <signal.h>
#include <unistd.h>
#include <rte_ethdev_driver.h>

#define IPADDR_LEN 16  /* Length of IP address in string. */

#define MSG_SIZE 2048  /* socket buffer len */

#define SOCK_RESET  -1
#define PORT_RESET  UINT16_MAX

// Maximum number of rings.
#define MAX_CLIENT  99

// The number of tokens in a command line.
#define MAX_PARAMETER 48

#define NO_FLAGS 0

/* Interval time to retry connection. */
#define CONN_RETRY_USEC (1000 * 1000)  /* micro sec */

/*
 * When doing reads from the NIC or the client queues,
 * use this batch size
 */
//#define PACKET_READ_SIZE 32

/*
 * TODO(yasufum) move it from common.h used only for spp_nfv, spp_vf and
 * spp_mirror.
 */
#define MAX_PKT_BURST 32

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512

#define VDEV_PREFIX_RING  "net_ring"
#define VDEV_PREFIX_VHOST "eth_vhost"
#define VDEV_PREFIX_PCAP  "net_pcap"
#define VDEV_PREFIX_TAP   "net_tap"
#define VDEV_PREFIX_NULL  "eth_null"


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
	uint16_t num_ports;
	uint16_t id[RTE_MAX_ETHPORTS];
	struct stats port_stats[RTE_MAX_ETHPORTS];
	struct stats client_stats[MAX_CLIENT];
};

enum port_type {
	PHY,
	RING,
	VHOST,
	PCAP,
	NULLPMD,
	TAP,
	UNDEF,
};

struct port_map {
	int id;
	enum port_type port_type;
	struct stats *stats;
	struct stats default_stats;
};

struct port {
	uint16_t in_port_id;
	uint16_t out_port_id;
	uint16_t (*rx_func)(uint16_t, uint16_t, struct rte_mbuf **, uint16_t);
	uint16_t (*tx_func)(uint16_t, uint16_t, struct rte_mbuf **, uint16_t);
};

/* define common names for structures shared between server and client */
#define MP_CLIENT_RXQ_NAME "eth_ring%u"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"

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

/* Set log level of type RTE_LOGTYPE_USER* to given level. */
int set_user_log_level(int num_user_log, uint32_t log_level);

/* Set log level of type RTE_LOGTYPE_USER* to RTE_LOG_DEBUG. */
int set_user_log_debug(int num_user_log);

int parse_num_clients(uint16_t *num_clients, const char *clients);

int parse_server(char **server_ip, int *server_port, char *server_addr);

/**
 * Get directory name of given proc_name.
 *
 * @param[in] proc_name Name of sec process such as spp_nfv.
 * @param[out] dir_name Directory name.
 * @return 0
 */
int get_sec_dir(char *proc_name, char *dir_name);

extern uint8_t lcore_id_used[RTE_MAX_LCORE];

/**
 * Get IP address of spp_ctl as string.
 *
 * @param[in,out] s_ip IP address of spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int get_spp_ctl_ip(char *s_ip);

/**
 * Set IP address of spp_ctl.
 *
 * @param[in] s_ip IP address of spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_spp_ctl_ip(const char *s_ip);

/**
 * Get port number for connecting to spp_ctl as string.
 *
 * @return Port number, or -1 if failed.
 */
int get_spp_ctl_port(void);

/**
 * Set port number for connecting to spp_ctl.
 *
 * @param[in] s_port Port number for spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_spp_ctl_port(int s_port);

/**
 * Get port type and port ID from ethdev name, such as `eth_vhost1` which
 * can be retrieved with rte_eth_dev_get_name_by_port().
 * In this case of `eth_vhost1`, port type is `VHOST` and port ID is `1`.
 *
 * @return 0 if succeeded, or -1 if failed.
 */
int parse_dev_name(char *dev_name, int *port_type, int *port_id);

#endif
