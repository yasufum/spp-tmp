/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __WK_SPP_PORT_H__
#define __WK_SPP_PORT_H__

#include "cmd_utils.h"

/**
 * Wrapper function for rte_eth_rx_burst() with ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst() with ring latency feature.
 *
 * @param port_id Etherdev ID.
 * @param[in] queue_id TX queue ID, but fixed value 0 in SPP.
 * @param[in] tx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of TX packets.
 * @return Number of TX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#endif /*  __WK_SPP_PORT_H__ */
