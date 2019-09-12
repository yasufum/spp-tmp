/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _RINGLATENCYSTATS_H_
#define _RINGLATENCYSTATS_H_

/**
 * @file
 * SPP RING latency statistics
 *
 * Measure the latency through ring-PMD.
 */

#include <rte_mbuf.h>
#include "cmd_utils.h"

/** number of slots to save latency. 0ns~99ns and 100ns over */
#define SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT 101

/** ring latency statistics */
struct spp_ringlatencystats_ring_latency_stats {
	/** slots to save latency */
	uint64_t slot[SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT];
};


#ifdef SPP_RINGLATENCYSTATS_ENABLE
/**
 * initialize ring latency statistics.
 *
 * @param samp_intvl
 *  The interval timer(ns) to refer the counter.
 * @param stats_count
 *  The number of ring to be measured.
 *
 * @retval SPPWK_RET_OK: succeeded.
 * @retval SPPWK_RET_NG: failed.
 */
int spp_ringlatencystats_init(uint64_t samp_intvl, uint16_t stats_count);

/**
 *uninitialize ring latency statistics.
 */
void spp_ringlatencystats_uninit(void);

/**
 * add time-stamp to mbuf's member.
 *
 * @note call at enqueue.
 *
 * @param ring_id
 *  The ring id.
 * @param pkts
 *  The address of an array of nb_pkts pointers to rte_mbuf structures
 *  which contain the packets to be measured.
 * @param nb_pkts
 *  The maximum number of packets to be measured.
 */
void spp_ringlatencystats_add_time_stamp(int ring_id,
			struct rte_mbuf **pkts, uint16_t nb_pkts);

/**
 * calculate latency of ring.
 *
 * @note call at dequeue.
 *
 * @param ring_id ring id.
 * @param pkts Pointer to nb_pkts to containing packets.
 * @param nb_pkts Max number of packets to be measured.
 */
void sppwk_calc_ring_latency(int ring_id,
		struct rte_mbuf **pkts, uint16_t nb_pkts);

/**
 * get number of ring latency statistics.
 *
 * @return spp_ringlatencystats_init's parameter "stats_count"
 */
int spp_ringlatencystats_get_count(void);

/**
 *get specific ring latency statistics.
 *
 * @param ring_id
 *  The ring id.
 * @param stats
 *  The statistics values.
 */
void spp_ringlatencystats_get_stats(int ring_id,
		struct spp_ringlatencystats_ring_latency_stats *stats);

/* Print statistics of time for packet processing in ring interface */
void print_ring_latency_stats(struct iface_info *if_info);

/**
 * Wrapper function for rte_eth_rx_burst() with ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
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
uint16_t sppwk_eth_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_rx_burst() with VLAN and ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst() with VLAN and ring latency feature.
 *
 * @param port_id Etherdev ID.
 * @param[in] queue_id TX queue ID, but fixed value 0 in SPP.
 * @param[in] tx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of TX packets.
 * @return Number of TX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#else

#define spp_ringlatencystats_init(arg1, arg2) 0
#define spp_ringlatencystats_uninit()
#define spp_ringlatencystats_add_time_stamp(arg1, arg2, arg3)
#define sppwk_calc_ring_latency(arg1, arg2, arg3)
#define spp_ringlatencystats_get_count() 0
#define spp_ringlatencystats_get_stats(arg1, arg2)
#define print_ringlatencystats_stats(arg)
#define sppwk_eth_ring_stats_rx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_ring_stats_tx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_vlan_ring_stats_rx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_vlan_ring_stats_tx_burst(arg1, arg2, arg3, arg4, arg5, arg6)

#endif /* SPP_RINGLATENCYSTATS_ENABLE */

#endif /* _RINGLATENCYSTATS_H_ */
