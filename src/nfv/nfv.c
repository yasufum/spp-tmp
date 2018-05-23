/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#include <arpa/inet.h>
#include <getopt.h>

#include <rte_eth_ring.h>
#include <rte_eth_vhost.h>
#include <rte_memzone.h>

#include "common.h"

static sig_atomic_t on = 1;

/*
 * our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to.
 */
static uint16_t client_id;
static char *server_ip;
static int server_port;

static enum cmd_type cmd = STOP;

static struct port_map port_map[RTE_MAX_ETHPORTS];

/* the port details */
struct port_info *ports;

static struct port ports_fwd_array[RTE_MAX_ETHPORTS];

/* It is used to convert port name from string type to enum */
struct porttype_map {
	const char     *port_name;
	enum port_type port_type;
};

struct porttype_map portmap[] = {
	{ .port_name = "phy",   .port_type = PHY, },
	{ .port_name = "ring",  .port_type = RING, },
	{ .port_name = "vhost", .port_type = VHOST, },
	{ .port_name = NULL,    .port_type = UNDEF, },
};

/* Return a type of port as a enum member of porttype_map structure. */
static enum port_type get_port_type(char *portname)
{
	for (int i = 0; portmap[i].port_name != NULL; i++) {
		const char *port_name = portmap[i].port_name;
		if (strncmp(portname, port_name, strlen(port_name)) == 0)
			return portmap[i].port_type;
	}
	return UNDEF;
}

/*
 * Split a token into words  with given separator and return the number of
 * splitted words.
 */
static int spp_split(char *splitted_words[], char *token, const char *sep)
{
	int cnt = 0;
	splitted_words[cnt] = strtok(token, sep);
	while (splitted_words[cnt] != NULL) {
		RTE_LOG(DEBUG, APP, "token %d = %s\n",
				cnt, splitted_words[cnt]);
		cnt++;
		splitted_words[cnt] = strtok(NULL, sep);
	}
	return cnt;
}


/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, APP,
		"Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	const char *progname = argv[0];
	static struct option lgopts[] = { {0} };
	int ret;

	while ((opt = getopt_long(argc, argvopt, "n:s:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case 'n':
			if (parse_num_clients(&client_id, optarg) != 0) {
				usage(progname);
				return -1;
			}
			break;
		case 's':
			ret = parse_server(&server_ip, &server_port, optarg);
			if (ret != 0) {
				usage(progname);
				return -1;
			}
			break;
		default:
			usage(progname);
			return -1;
		}
	}

	return 0;
}

static void
forward(void)
{
	uint16_t nb_rx;
	uint16_t nb_tx;
	int in_port;
	int out_port;
	uint16_t buf;
	int i;

	/* Go through every possible port numbers*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		struct rte_mbuf *bufs[MAX_PKT_BURST];

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		if (ports_fwd_array[i].out_port_id == PORT_RESET)
			continue;

		/* if status active, i count is in port*/
		in_port = i;
		out_port = ports_fwd_array[i].out_port_id;

		/* Get burst of RX packets, from first port of pair. */
		/*first port rx, second port tx*/
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
			/*RTE_LOG(INFO, APP, "Idling\n");*/
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
forward_array_init_one(unsigned int i)
{
	ports_fwd_array[i].in_port_id = PORT_RESET;
	ports_fwd_array[i].out_port_id = PORT_RESET;
}

/* initialize forward array with default value*/
static void
forward_array_init(void)
{
	unsigned int i;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		forward_array_init_one(i);
}

static void
forward_array_reset(void)
{
	unsigned int i;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id != PORT_RESET) {
			ports_fwd_array[i].out_port_id = PORT_RESET;
			RTE_LOG(INFO, APP, "Port ID %d\n", i);
			RTE_LOG(INFO, APP, "out_port_id %d\n",
				ports_fwd_array[i].out_port_id);
		}
	}
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
forward_array_remove(int port_id)
{
	unsigned int i;

	/* Update ports_fwd_array */
	forward_array_init_one(port_id);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id == PORT_RESET)
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

	if (!strcmp(token_list[1], "vhost")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		port_id = find_port_id(id, VHOST);
		if (port_id < 0)
			return -1;

	} else if (!strcmp(token_list[1], "ring")) {
		char name[RTE_ETH_NAME_MAX_LEN];

		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		port_id = find_port_id(id, RING);
		if (port_id < 0)
			return -1;

		rte_eth_dev_detach(port_id, name);

	} else if (!strcmp(token_list[1], "pcap")) {
		char name[RTE_ETH_NAME_MAX_LEN];

		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		port_id = find_port_id(id, PCAP);
		if (port_id < 0)
			return -1;

		rte_eth_dev_detach(port_id, name);

	} else if (!strcmp(token_list[1], "nullpmd")) {
		char name[RTE_ETH_NAME_MAX_LEN];

		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		port_id = find_port_id(id, NULLPMD);
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

static int
add_ring_pmd(int ring_id)
{
	struct rte_ring *ring;
	int ring_port_id;

	/* look up ring, based on user's provided id*/
	ring = rte_ring_lookup(get_rx_queue_name(ring_id));
	if (ring == NULL) {
		RTE_LOG(ERR, APP,
			"Cannot get RX ring - is server process running?\n");
		return -1;
	}

	/* create ring pmd*/
	ring_port_id = rte_eth_from_ring(ring);
	RTE_LOG(DEBUG, APP, "ring port id %d\n", ring_port_id);

	return ring_port_id;
}

static int
add_vhost_pmd(int index)
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
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

	/* eth_vhost0 index 0 iface /tmp/sock0 on numa 0 */
	name = get_vhost_backend_name(index);
	iface = get_vhost_iface_name(index);

	sprintf(devargs, "%s,iface=%s,queues=%d", name, iface, nr_queues);
	ret = rte_eth_dev_attach(devargs, &vhost_port_id);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_configure(vhost_port_id, nr_queues, nr_queues,
		&port_conf);
	if (ret < 0)
		return ret;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL, mp);
		if (ret < 0)
			return ret;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL);
		if (ret < 0)
			return ret;
	}

	/* Start the Ethernet port. */
	ret = rte_eth_dev_start(vhost_port_id);
	if (ret < 0)
		return ret;

	RTE_LOG(DEBUG, APP, "vhost port id %d\n", vhost_port_id);

	return vhost_port_id;
}

static int
add_pcap_pmd(int index)
{
	struct rte_eth_conf port_conf = {
		.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
	};

	struct rte_mempool *mp;
	const char *name;
	char devargs[256];
	uint16_t pcap_pmd_port_id;
	uint16_t nr_queues = 1;

	int ret;

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannon get mempool for mbuf\n");

	name = get_pcap_pmd_name(index);
	sprintf(devargs,
		"%s,rx_pcap=/tmp/rx_%d.pcap,tx_pcap=/tmp/tx_%d.pcap",
		name, index, index);
	ret = rte_eth_dev_attach(devargs, &pcap_pmd_port_id);

	if (ret < 0)
		return ret;

	ret = rte_eth_dev_configure(
			pcap_pmd_port_id, nr_queues, nr_queues, &port_conf);

	if (ret < 0)
		return ret;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	uint16_t q;
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(
				pcap_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(pcap_pmd_port_id),
				NULL, mp);
		if (ret < 0)
			return ret;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(
				pcap_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(pcap_pmd_port_id),
				NULL);
		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(pcap_pmd_port_id);

	if (ret < 0)
		return ret;

	RTE_LOG(DEBUG, APP, "pcap port id %d\n", pcap_pmd_port_id);

	return pcap_pmd_port_id;
}

static int
add_null_pmd(int index)
{
	struct rte_eth_conf port_conf = {
			.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
	};

	struct rte_mempool *mp;
	const char *name;
	char devargs[64];
	uint16_t null_pmd_port_id;
	uint16_t nr_queues = 1;

	int ret;

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannon get mempool for mbuf\n");

	name = get_null_pmd_name(index);
	sprintf(devargs, "%s", name);
	ret = rte_eth_dev_attach(devargs, &null_pmd_port_id);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_configure(
			null_pmd_port_id, nr_queues, nr_queues,
			&port_conf);
	if (ret < 0)
		return ret;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	uint16_t q;
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(
				null_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(
					null_pmd_port_id), NULL, mp);
		if (ret < 0)
			return ret;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(
				null_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(
					null_pmd_port_id),
				NULL);
		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(null_pmd_port_id);
	if (ret < 0)
		return ret;

	RTE_LOG(DEBUG, APP, "null port id %d\n", null_pmd_port_id);

	return null_pmd_port_id;
}

static int
do_add(char *token_list[], int max_token)
{
	enum port_type type = UNDEF;
	int port_id = PORT_RESET;
	int id;

	if (max_token <= 2)
		return -1;

	if (!strcmp(token_list[1], "vhost")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		type = VHOST;
		port_id = add_vhost_pmd(id);

	} else if (!strcmp(token_list[1], "ring")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		type = RING;
		port_id = add_ring_pmd(id);

	} else if (!strcmp(token_list[1], "pcap")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		type = PCAP;
		port_id = add_pcap_pmd(id);

	} else if (!strcmp(token_list[1], "nullpmd")) {
		if (spp_atoi(token_list[2], &id) < 0)
			return 0;

		type = NULLPMD;
		port_id = add_null_pmd(id);
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

	/* tokenize user command from controller */
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
			i = sprintf(str, "status: running\n");
		else
			i = sprintf(str, "status: idling\n");
		print_active_ports(str + i, ports_fwd_array, port_map);

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

	} else if (!strcmp(token_list[0], "add")) {
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

			if (spp_atoi(token_list[1], &in_port) < 0) {
				char *param_list[MAX_PARAMETER] = { 0 };
				int param_count = spp_split(
						param_list, token_list[1], ":");
				if (param_count == 2) {
					int in_port_id;
					if (spp_atoi(
						param_list[1], &in_port_id) < 0)
						return 0;
					in_port = find_port_id(
						in_port_id,
						get_port_type(param_list[0]));
				} else {
					return 0;
				}
			}

			if (spp_atoi(token_list[2], &out_port) < 0) {
				char *param_list[MAX_PARAMETER] = { 0 };
				int param_count = spp_split(
						param_list,
						token_list[2], ":");
				if (param_count == 2) {
					int out_port_id;
					if (spp_atoi(
						param_list[1],
						&out_port_id) < 0)
						return 0;
					out_port = find_port_id(
						out_port_id,
						get_port_type(param_list[0]));
				} else {
					return 0;
				}
			}

			if (in_port < 0 || out_port < 0)
				return 0;

			add_patch(in_port, out_port);
		}

	} else if (!strcmp(token_list[0], "del")) {
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

			/*Create of the tcp socket*/
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

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
main(int argc, char *argv[])
{
	const struct rte_memzone *mz;
	int sock = SOCK_RESET;
	unsigned int lcore_id;
	unsigned int nb_ports;
	int connected = 0;
	char str[MSG_SIZE];
	unsigned int i;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;

	argc -= ret;
	argv += ret;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	/* initialize port forward array*/
	forward_array_init();
	port_map_init();

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* set up array for port data */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		mz = rte_memzone_lookup(MZ_PORT_INFO);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot get port info structure\n");
		ports = mz->addr;
	} else { /* RTE_PROC_PRIMARY */
		mz = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports),
			rte_socket_id(), NO_FLAGS);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot reserve memory zone for port information\n");
		memset(mz->addr, 0, sizeof(*ports));
		ports = mz->addr;
	}

	RTE_LOG(INFO, APP, "Number of Ports: %d\n", nb_ports);

	/* update port_forward_array with active port */
	for (i = 0; i < nb_ports; i++) {
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		/* Update ports_fwd_array with phy port*/
		ports_fwd_array[i].in_port_id = i;
		port_map[i].port_type = PHY;
		port_map[i].id = i;
		port_map[i].stats = &ports->port_stats[i];
	}

	lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(main_loop, NULL, lcore_id);
	}

	RTE_LOG(INFO, APP, "My ID %d start handling message\n", client_id);
	RTE_LOG(INFO, APP, "[Press Ctrl-C to quit ...]\n");

	/* send and receive msg loop */
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
	RTE_LOG(INFO, APP, "spp_nfv exit.\n");
	return 0;
}
