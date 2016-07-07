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

#include <arpa/inet.h>

#include <rte_eal_memconfig.h>
#include <rte_eth_ring.h>
#include <rte_memzone.h>

#include "args.h"
#include "common.h"
#include "init.h"

static sig_atomic_t on = 1;

static enum cmd_type cmd = STOP;

static struct port_map port_map[RTE_MAX_ETHPORTS];

static struct port ports_fwd_array[RTE_MAX_ETHPORTS];

static void
forward(void)
{
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint16_t buf;
	int in_port;
	int out_port;
	int i;

	/* Go through every possible port numbers */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		struct rte_mbuf *bufs[MAX_PKT_BURST];

		if (ports_fwd_array[i].in_port_id < 0)
			continue;

		if (ports_fwd_array[i].out_port_id < 0)
			continue;

		/* if status active, i count is in port */
		in_port = i;
		out_port = ports_fwd_array[i].out_port_id;

		/* Get burst of RX packets, from first port of pair. */
		/* first port rx, second port tx */
		nb_rx = ports_fwd_array[in_port].rx_func(in_port, 0, bufs,
			MAX_PKT_BURST);
		if (unlikely(nb_rx == 0))
			continue;

		port_map[in_port].stats->rx += nb_rx;

		/* Send burst of TX packets, to second port of pair. */
		nb_tx = ports_fwd_array[out_port].tx_func(out_port, 0, bufs,
			nb_rx);

		port_map[out_port].stats->tx += nb_tx;

		/* Free any unsent packets. */
		if (unlikely(nb_tx < nb_rx)) {
			port_map[out_port].stats->tx_drop += nb_rx - nb_tx;
			for (buf = nb_tx; buf < nb_rx; buf++)
				rte_pktmbuf_free(bufs[buf]);
		}
	}
}

/* main processing loop */
static void
nfv_loop(void)
{
	unsigned int lcore_id = rte_lcore_id();

	RTE_LOG(INFO, APP, "entering main loop on lcore %u\n", lcore_id);

	while (1) {
		if (unlikely(cmd == STOP)) {
			sleep(1);
			continue;
		} else if (cmd == FORWARD) {
			forward();
		}
	}
}

/* leading to nfv processing loop */
static int
main_loop(void *dummy __rte_unused)
{
	nfv_loop();
	return 0;
}

static void
port_map_init_one(unsigned int i)
{
	port_map[i].id = PORT_RESET;
	port_map[i].port_type = UNDEF;
	port_map[i].stats = &port_map[i].default_stats;
}

static void
port_map_init(void)
{
	unsigned int i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		port_map_init_one(i);
}

static void
forward_array_init_one(int i)
{
	ports_fwd_array[i].in_port_id = PORT_RESET;
	ports_fwd_array[i].out_port_id = PORT_RESET;
}

/* initialize forward array with default value */
static void
forward_array_init(void)
{
	unsigned int i;

	/* initialize port forward array */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		forward_array_init_one(i);
}

static int
do_send(int *connected, int *sock, char *str)
{
	int ret;

	ret = send(*sock, str, MSG_SIZE, 0);
	if (ret == -1) {
		RTE_LOG(ERR, APP, "send failed");
		*connected = 0;
		return -1;
	}

	RTE_LOG(INFO, APP, "To Server: %s\n", str);

	return 0;
}

static void
forward_array_reset(void)
{
	unsigned int i;

	/* initialize port forward array */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id > -1) {
			ports_fwd_array[i].out_port_id = PORT_RESET;
			RTE_LOG(INFO, APP, "Port ID %d\n", i);
			RTE_LOG(INFO, APP, "out_port_id %d\n",
				ports_fwd_array[i].out_port_id);
		}
	}
}

/* print forward array active port */
static void
print_active_ports(char *str)
{
	unsigned int i;

	sprintf(str, "%d\n", client_id);
	/* every elements value */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id < 0)
			continue;

		RTE_LOG(INFO, APP, "Port ID %d\n", i);
		RTE_LOG(INFO, APP, "Status %d\n",
			ports_fwd_array[i].in_port_id);

		sprintf(str + strlen(str), "port id: %d,", i);
		if (ports_fwd_array[i].in_port_id >= 0)
			sprintf(str + strlen(str), "on,");
		else
			sprintf(str + strlen(str), "off,");

		switch (port_map[i].port_type) {
		case PHY:
			RTE_LOG(INFO, APP, "Type: PHY\n");
			sprintf(str + strlen(str), "PHY,");
			break;
		case RING:
			RTE_LOG(INFO, APP, "Type: RING\n");
			sprintf(str + strlen(str), "RING(%u),",
				port_map[i].id);
			break;
		case VHOST:
			RTE_LOG(INFO, APP, "Type: VHOST\n");
			sprintf(str + strlen(str), "VHOST(%u),",
				port_map[i].id);
			break;
		case UNDEF:
			RTE_LOG(INFO, APP, "Type: UDF\n");
			sprintf(str + strlen(str), "UDF,");
			break;
		}

		RTE_LOG(INFO, APP, "Out Port ID %d\n",
			ports_fwd_array[i].out_port_id);
		sprintf(str + strlen(str), "outport: %d\n",
			ports_fwd_array[i].out_port_id);
	}
}

static void
forward_array_remove(int port_id)
{
	unsigned int i;

	/* Update ports_fwd_array */
	forward_array_init_one(port_id);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id < 0)
			continue;

		if (ports_fwd_array[i].out_port_id == port_id) {
			ports_fwd_array[i].out_port_id = PORT_RESET;
			break;
		}
	}
}

static int
find_port_id(int id, enum port_type type)
{
	int port_id = PORT_RESET;
	unsigned int i;

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

static int
do_del(char *token_list[], int max_token)
{
	int port_id = PORT_RESET;
	int id;

	if (max_token <= 2)
		return -1;

	if (!strcmp(token_list[1], "ring")) {
		char name[RTE_ETH_NAME_MAX_LEN];

		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		RTE_LOG(DEBUG, APP, "Del ring id %d\n", id);
		port_id = find_port_id(id, RING);
		if (port_id < 0)
			return -1;

		rte_eth_dev_detach(port_id, name);
	}

	forward_array_remove(port_id);
	port_map_init_one(port_id);

	return 0;
}

static int
is_valid_port(int port_id)
{
	if (port_id < 0 || port_id > RTE_MAX_ETHPORTS)
		return 0;

	return port_map[port_id].id != PORT_RESET;
}

static int
add_patch(int in_port, int out_port)
{
	if (!is_valid_port(in_port) || !is_valid_port(out_port))
		return -1;

	/* Populate in port data */
	ports_fwd_array[in_port].in_port_id = in_port;
	ports_fwd_array[in_port].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[in_port].tx_func = &rte_eth_tx_burst;
	ports_fwd_array[in_port].out_port_id = out_port;

	/* Populate out port data */
	ports_fwd_array[out_port].in_port_id = out_port;
	ports_fwd_array[out_port].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[out_port].tx_func = &rte_eth_tx_burst;

	RTE_LOG(DEBUG, APP, "STATUS: in port %d in_port_id %d\n", in_port,
		ports_fwd_array[in_port].in_port_id);
	RTE_LOG(DEBUG, APP, "STATUS: in port %d patch out port id %d\n",
		in_port, ports_fwd_array[in_port].out_port_id);
	RTE_LOG(DEBUG, APP, "STATUS: outport %d in_port_id %d\n", out_port,
		ports_fwd_array[out_port].in_port_id);

	return 0;
}

static struct rte_memzone *
get_memzone_by_addr(const void *addr)
{
	struct rte_memzone *tmp, *mz;
	struct rte_mem_config *mcfg;
	int i;

	mcfg = rte_eal_get_configuration()->mem_config;
	mz = NULL;

	/* find memzone for the ring */
	for (i = 0; i < RTE_MAX_MEMZONE; i++) {
		tmp = &mcfg->memzone[i];

		if (tmp->addr_64 == (uint64_t) addr) {
			mz = tmp;
			break;
		}
	}

	return mz;
}

static int
add_ring_pmd(int ring_id)
{
	const struct rte_memzone *memzone;
	struct rte_ring *ring;
	int ring_port_id;

	/* look up ring, based on user's provided id*/
	ring = rte_ring_lookup(get_rx_queue_name(ring_id));
	if (ring == NULL) {
		RTE_LOG(ERR, APP,
			"Cannot get RX ring - is server process running?\n");
		return -1;
	}

	memzone = get_memzone_by_addr(ring);
	if (memzone == NULL)
		return -1;

	/* create ring pmd*/
	ring->memzone = memzone;
	ring_port_id = rte_eth_from_ring(ring);

	RTE_LOG(DEBUG, APP, "ring port id %d\n", ring_port_id);

	return ring_port_id;
}

static int
do_add(char *token_list[], int max_token)
{
	enum port_type type = UNDEF;
	int port_id = PORT_RESET;
	int id;

	if (max_token <= 2)
		return -1;

	if (!strcmp(token_list[1], "ring")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		type = RING;
		port_id = add_ring_pmd(id);
	}

	if (port_id < 0)
		return -1;

	port_map[port_id].id = id;
	port_map[port_id].port_type = type;
	port_map[port_id].stats = &ports->client_stats[id];

	/* Update ports_fwd_array with port id */
	ports_fwd_array[port_id].in_port_id = port_id;

	return 0;
}

static int
parse_command(char *str)
{
	char *token_list[MAX_PARAMETER] = {NULL};
	int max_token = 0;
	int ret = 0;
	int i;

	if (!str)
		return 0;

	/* tokenize the user commands from controller */
	token_list[max_token] = strtok(str, " ");
	while (token_list[max_token] != NULL) {
		RTE_LOG(DEBUG, APP, "token %d = %s\n", max_token,
			token_list[max_token]);
		max_token++;
		token_list[max_token] = strtok(NULL, " ");
	}

	if (max_token == 0)
		return 0;

	if (!strcmp(token_list[0], "status")) {
		RTE_LOG(DEBUG, APP, "status\n");
		memset(str, '\0', MSG_SIZE);
		if (cmd == FORWARD)
			i = sprintf(str, "Client ID %d Running\n", client_id);
		else
			i = sprintf(str, "Client ID %d Idling\n", client_id);
		print_active_ports(str + i);

	} else if (!strcmp(token_list[0], "_get_client_id")) {
		memset(str, '\0', MSG_SIZE);
		i = sprintf(str, "%d", client_id);

	} else if (!strcmp(token_list[0], "_set_client_id")) {
		int id;

		if (spp_atoi(token_list[1], &id) >= 0)
			client_id = id;

	} else if (!strcmp(token_list[0], "exit")) {
		RTE_LOG(DEBUG, APP, "exit\n");
		RTE_LOG(DEBUG, APP, "stop\n");
		cmd = STOP;
		ret = -1;

	} else if (!strcmp(token_list[0], "stop")) {
		RTE_LOG(DEBUG, APP, "stop\n");
		cmd = STOP;

	} else if (!strcmp(token_list[0], "forward")) {
		RTE_LOG(DEBUG, APP, "forward\n");
		cmd = FORWARD;

	} else if (strncmp(token_list[0], "add", 3) == 0) {
		RTE_LOG(DEBUG, APP, "add\n");
		do_add(token_list, max_token);

	} else if (!strcmp(token_list[0], "patch")) {
		RTE_LOG(DEBUG, APP, "patch\n");

		if (max_token <= 1)
			return 0;

		if (strncmp(token_list[1], "reset", 5) == 0) {
			/* reset forward array*/
			forward_array_reset();
		} else {
			int in_port;
			int out_port;

			if (max_token <= 2)
				return 0;

			if (spp_atoi(token_list[1], &in_port) < 0)
				return 0;

			if (spp_atoi(token_list[2], &out_port) < 0)
				return 0;

			add_patch(in_port, out_port);
		}
	} else if (strncmp(str, "del", 3) == 0) {
		RTE_LOG(DEBUG, APP, "del\n");

		cmd = STOP;
		do_del(token_list, max_token);
	}

	return ret;
}

static int
do_receive(int *connected, int *sock, char *str)
{
	int ret;

	memset(str, '\0', MSG_SIZE);

	ret = recv(*sock, str, MSG_SIZE, 0);
	if (ret <= 0) {
		RTE_LOG(DEBUG, APP, "Receive count: %d\n", ret);
		if (ret < 0)
			RTE_LOG(ERR, APP, "Receive Fail");
		else
			RTE_LOG(INFO, APP, "Receive 0\n");

		RTE_LOG(INFO, APP, "Assume Server closed connection\n");
		close(*sock);
		*sock = SOCK_RESET;
		*connected = 0;
		return -1;
	}

	return 0;
}

static int
do_connection(int *connected, int *sock)
{
	static struct sockaddr_in servaddr;
	int ret = 0;

	if (*connected == 0) {
		if (*sock < 0) {
			RTE_LOG(INFO, APP, "Creating socket...\n");
			*sock = socket(AF_INET, SOCK_STREAM, 0);
			if (*sock < 0)
				rte_exit(EXIT_FAILURE, "socket error\n");

			/* Creation of the socket */
			memset(&servaddr, 0, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_addr.s_addr = inet_addr(server_ip);
			servaddr.sin_port = htons(server_port);
		}

		RTE_LOG(INFO, APP, "Trying to connect ... socket %d\n", *sock);
		ret = connect(*sock, (struct sockaddr *) &servaddr,
			sizeof(servaddr));
		if (ret < 0) {
			RTE_LOG(ERR, APP, "Connection Error");
			return ret;
		}

		RTE_LOG(INFO, APP, "Connected\n");
		*connected = 1;
	}

	return ret;
}

int
main(int argc, char *argv[])
{
	unsigned int lcore_id;
	int sock = SOCK_RESET;
	int connected = 0;
	char str[MSG_SIZE];
	int ret;
	int i;

	/* initialise the system */
	if (init(argc, argv) < 0)
		return -1;

	/* initialize port forward array*/
	forward_array_init();
	port_map_init();

	/* update port_forward_array with active port */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		/* Update ports_fwd_array with phy port*/
		ports_fwd_array[i].in_port_id = i;
		port_map[i].port_type = PHY;
		port_map[i].id = i;
		port_map[i].stats = &ports->port_stats[i];
	}

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(main_loop, NULL, lcore_id);
	}

	while (on) {
		ret = do_connection(&connected, &sock);
		if (ret < 0) {
			sleep(1);
			continue;
		}

		ret = do_receive(&connected, &sock, str);
		if (ret < 0)
			continue;

		RTE_LOG(DEBUG, APP, "Received string: %s\n", str);

		ret = parse_command(str);
		if (ret < 0)
			break;

		/*Send the message back to client*/
		ret = do_send(&connected, &sock, str);
		if (ret < 0)
			continue;
	}

	/* exit */
	close(sock);
	sock = SOCK_RESET;
	RTE_LOG(INFO, APP, "spp_vm exit.\n");
	return 0;
}
