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
