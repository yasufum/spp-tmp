/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __PORT_CAPABILITY_H__
#define __PORT_CAPABILITY_H__

/**
 * @file
 * SPP Port ability
 *
 * Provide about the ability per port.
 */

#include "cmd_utils.h"

/** Calculate TCI of VLAN tag. */
#define SPP_VLANTAG_CALC_TCI(id, pcp) (((pcp & 0x07) << 13) | (id & 0x0fff))

/** Type for changing index. */
enum port_ability_chg_index_type {
	PORT_ABILITY_CHG_INDEX_REF,  /** To change index to reference area. */
	PORT_ABILITY_CHG_INDEX_UPD,  /** To change index to update area. */
};

/** Initialize port ability. */
void spp_port_ability_init(void);

/**
 * Get information of port ability.
 *
 * @param port_id Etherdev ID.
 * @param rxtx RX/TX ID of port_id.
 * @param info Port ability information.
 */
void spp_port_ability_get_info(
		int port_id, enum sppwk_port_dir dir,
		struct sppwk_port_attrs **info);

/**
 * Change index of management information.
 *
 * @param port_id Etherdev ID.
 * @param rxtx RX/TX ID of port_id.
 * @param type Type for changing index.
 */
void spp_port_ability_change_index(
		enum port_ability_chg_index_type type,
		int port_id, enum sppwk_port_dir dir);

/**
 * Update port capability.
 *
 * @param component_info
 *  The pointer to struct sppwk_comp_info.@n
 *  The data for updating the internal data of port ability.
 */
void spp_port_ability_update(const struct sppwk_comp_info *component);

/**
 * Wrapper function for rte_eth_rx_burst() with ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_rx_burst(uint16_t port_id, uint16_t queue_id,
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
uint16_t sppwk_eth_vlan_tx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#endif /*  __PORT_CAPABILITY_H__ */
