/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_net_crc.h>

#include "spp_port.h"
#include "shared/secondary/return_codes.h"
#include "ringlatencystats.h"

/* Wrapper function for rte_eth_rx_burst() with ring latency feature. */
uint16_t
sppwk_eth_rx_burst(uint16_t port_id,
		uint16_t queue_id  __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;

	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);

	/* TODO(yasufum) confirm why it returns SPP_RET_OK. */
	if (unlikely(nb_rx == 0))
		return SPP_RET_OK;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	if (g_port_mng_info[port_id].iface_type == RING)
		spp_ringlatencystats_calculate_latency(
				g_port_mng_info[port_id].iface_no,
				rx_pkts, nb_pkts);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	return nb_rx;
}

/* Wrapper function for rte_eth_tx_burst() with ring latency feature. */
uint16_t
sppwk_eth_tx_burst(uint16_t port_id,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	nb_tx = rte_eth_tx_burst(port_id, 0, tx_pkts, nb_pkts);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	if (g_port_mng_info[port_id].iface_type == RING)
		spp_ringlatencystats_add_time_stamp(
				g_port_mng_info[port_id].iface_no,
				tx_pkts, nb_pkts);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	return nb_tx;
}
