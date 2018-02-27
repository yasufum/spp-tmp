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
#ifndef __SPP_PORT_H__
#define __SPP_PORT_H__

/**
 * @file
 * SPP Port ability
 *
 * Provide about the ability per port.
 */

#include "spp_vf.h"

/** Calculate TCI of VLAN tag. */
#define SPP_VLANTAG_CALC_TCI(id, pcp) (((pcp & 0x07) << 13) | (id & 0x0fff))

/** Type for changing index. */
enum port_ability_chg_index_type {
	/** Type for changing index to reference area. */
	PORT_ABILITY_CHG_INDEX_REF,

	/** Type for changing index to update area. */
	PORT_ABILITY_CHG_INDEX_UPD,
};

/** Initialize port ability. */
void spp_port_ability_init(void);

/**
 * Get information of port ability.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param rxtx
 *  rx/tx identifier of port_id.
 * @param info
 *  Port ability information.
 */
void spp_port_ability_get_info(
		int port_id, enum spp_port_rxtx rxtx,
		struct spp_port_ability **info);

/**
 * Change index of management information.
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param rxtx
 *  rx/tx identifier of port_id.
 * @param type
 *  Type for changing index.
 */
void spp_port_ability_change_index(
		enum port_ability_chg_index_type type,
		int port_id, enum spp_port_rxtx rxtx);

/**
 * Update port capability.
 *
 * @param component_info
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of port ability.
 */
void spp_port_ability_update(const struct spp_component_info *component);

/**
 * Wrapper function for rte_eth_rx_burst().
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The index of the receive queue from which to retrieve input packets.
 *  SPP is fixed at 0.
 * @param rx_pkts
 *  The address of an array of pointers to *rte_mbuf* structures that
 *  must be large enough to store *nb_pkts* pointers in it.
 * @param nb_pkts
 *  The maximum number of packets to retrieve.
 *
 * @return
 *  The number of packets actually retrieved, which is the number
 *  of pointers to *rte_mbuf* structures effectively supplied to the
 *  *rx_pkts* array.
 */
uint16_t spp_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst().
 *
 * @param port_id
 *  The port identifier of the Ethernet device.
 * @param queue_id
 *  The index of the transmit queue through which output packets must be sent.
 *  SPP is fixed at 0.
 * @param tx_pkts
 *  The address of an array of *nb_pkts* pointers to *rte_mbuf* structures
 *  which contain the output packets.
 * @param nb_pkts
 *  The maximum number of packets to transmit.
 *
 * @return
 *  The number of output packets actually stored in transmit descriptors of
 *  the transmit ring. The return value can be less than the value of the
 *  *tx_pkts* parameter when the transmit ring is full or has been filled up.
 */
uint16_t spp_eth_tx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#endif /*  __SPP_PORT_H__ */
