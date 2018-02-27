/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Nippon Telegraph and Telephone Corporation
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
 *     * Neither the name of Nippon Telegraph and Telephone Corporation
 *       nor the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
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

#ifndef _RINGLATENCYSTATS_H_
#define _RINGLATENCYSTATS_H_

/**
 * @file
 * SPP RING latency statistics
 *
 * Measure the latency through ring-PMD.
 */

#include <rte_mbuf.h>

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
 * @retval 0: succeeded.
 * @retval -1: failed.
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
 * calculate latency.
 *
 * @note call at dequeue.
 *
 * @param ring_id
 *  The ring id.
 * @param pkts
 *  The address of an array of nb_pkts pointers to rte_mbuf structures
 *  which contain the packets to be measured.
 * @param nb_pkts
 *  The maximum number of packets to be measured.
 */
void spp_ringlatencystats_calculate_latency(int ring_id,
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

#else

#define spp_ringlatencystats_init(arg1, arg2) 0
#define spp_ringlatencystats_uninit()
#define spp_ringlatencystats_add_time_stamp(arg1, arg2, arg3)
#define spp_ringlatencystats_calculate_latency(arg1, arg2, arg3)
#define spp_ringlatencystats_get_count() 0
#define spp_ringlatencystats_get_stats(arg1, arg2)

#endif /* SPP_RINGLATENCYSTATS_ENABLE */

#endif /* _RINGLATENCYSTATS_H_ */
