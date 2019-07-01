/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#include <rte_mbuf.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "ringlatencystats.h"
#include "cmd_utils.h"
#include "port_capability.h"
#include "../return_codes.h"

#define NS_PER_SEC 1E9

#define RTE_LOGTYPE_SPP_RING_LATENCY_STATS RTE_LOGTYPE_USER1

#ifdef SPP_RINGLATENCYSTATS_ENABLE

/** ring latency statistics information */
struct ring_latency_stats_info {
	uint64_t timer_tsc;     /**< sampling interval counter */
	uint64_t prev_tsc;      /**< previous time */
	struct spp_ringlatencystats_ring_latency_stats stats;
				/**< ring latency statistics list */
};

/** sampling interval */
static uint64_t g_samp_intvl;

/** ring latency statistics information instance */
static struct ring_latency_stats_info *g_stats_info;

/** number of ring latency statistics */
static uint16_t g_stats_count;

/* clock cycles per nano second */
static inline uint64_t
cycles_per_ns(void)
{
	return rte_get_timer_hz() / NS_PER_SEC;
}

int
spp_ringlatencystats_init(uint64_t samp_intvl, uint16_t stats_count)
{
	/* allocate memory for ring latency statistics information */
	g_stats_info = rte_zmalloc(
			"global ring_latency_stats_info",
			sizeof(struct ring_latency_stats_info) * stats_count,
			0);
	if (unlikely(g_stats_info == NULL)) {
		RTE_LOG(ERR, SPP_RING_LATENCY_STATS, "Cannot allocate memory "
				"for ring latency stats info\n");
		return SPP_RET_NG;
	}

	/* store global information for ring latency statistics */
	g_samp_intvl = samp_intvl * cycles_per_ns();
	g_stats_count = stats_count;

	RTE_LOG(DEBUG, SPP_RING_LATENCY_STATS,
			"g_samp_intvl=%lu, g_stats_count=%hu, "
			"cpns=%lu, NS_PER_SEC=%f\n",
			g_samp_intvl, g_stats_count,
			cycles_per_ns(), NS_PER_SEC);

	return SPP_RET_OK;
}

void
spp_ringlatencystats_uninit(void)
{
	/* free memory for ring latency statistics information */
	if (likely(g_stats_info != NULL)) {
		rte_free(g_stats_info);
		g_stats_count = 0;
	}
}

void
spp_ringlatencystats_add_time_stamp(int ring_id,
			struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	unsigned int i;
	uint64_t diff_tsc, now;
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	for (i = 0; i < nb_pkts; i++) {

		/* get tsc now */
		now = rte_rdtsc();

		/* calculate difference from the previous processing time */
		diff_tsc = now - stats_info->prev_tsc;
		stats_info->timer_tsc += diff_tsc;

		/* when it is over sampling interval */
		/* set tsc to mbuf::timestamp */
		if (unlikely(stats_info->timer_tsc >= g_samp_intvl)) {
			RTE_LOG(DEBUG, SPP_RING_LATENCY_STATS,
					"Set timestamp. ring_id=%d, "
					"pkts_index=%u, timestamp=%lu\n",
					ring_id, i, now);
			pkts[i]->timestamp = now;
			stats_info->timer_tsc = 0;
		}

		/* update previous tsc */
		stats_info->prev_tsc = now;
	}
}

void
spp_ringlatencystats_calculate_latency(int ring_id,
			struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	unsigned int i;
	uint64_t now;
	int64_t latency;
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	now = rte_rdtsc();
	for (i = 0; i < nb_pkts; i++) {
		if (likely(pkts[i]->timestamp == 0))
			continue;

		/* when mbuf::timestamp is not zero */
		/* calculate latency */
		latency = (uint64_t)floor((now - pkts[i]->timestamp) /
				cycles_per_ns());
		if (likely(latency < SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT-1))
			stats_info->stats.slot[latency]++;
		else
			stats_info->stats.slot[
					SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT
					-1]++;
	}
}

int
spp_ringlatencystats_get_count(void)
{
	return g_stats_count;
}

void
spp_ringlatencystats_get_stats(int ring_id,
		struct spp_ringlatencystats_ring_latency_stats *stats)
{
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	rte_memcpy(stats, &stats_info->stats,
			sizeof(struct spp_ringlatencystats_ring_latency_stats));
}

/* Print statistics of time for packet processing in ring interface */
void
print_ring_latency_stats(struct iface_info *if_info)
{
	/* Clear screen and move cursor to top left */
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	printf("%s%s", clr, topLeft);

	int ring_cnt, stats_cnt;
	struct spp_ringlatencystats_ring_latency_stats stats[RTE_MAX_ETHPORTS];
	memset(&stats, 0x00, sizeof(stats));

	printf("RING Latency\n");
	printf(" RING");
	for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
		if (if_info->ring[ring_cnt].iface_type == UNDEF)
			continue;

		spp_ringlatencystats_get_stats(ring_cnt, &stats[ring_cnt]);
		printf(", %-18d", ring_cnt);
	}
	printf("\n");

	for (stats_cnt = 0; stats_cnt < SPP_RINGLATENCYSTATS_STATS_SLOT_COUNT;
			stats_cnt++) {
		printf("%3dns", stats_cnt);
		for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
			if (if_info->ring[ring_cnt].iface_type == UNDEF)
				continue;

			printf(", 0x%-16lx", stats[ring_cnt].slot[stats_cnt]);
		}
		printf("\n");
	}
}

/* Wrapper function for rte_eth_rx_burst() with ring latency feature. */
uint16_t
sppwk_eth_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no,
		uint16_t queue_id  __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;

	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);

	/* TODO(yasufum) confirm why it returns SPP_RET_OK. */
	if (unlikely(nb_rx == 0))
		return SPP_RET_OK;

	if (iface_type == RING)
		spp_ringlatencystats_calculate_latency(
				iface_no,
				rx_pkts, nb_pkts);
	return nb_rx;
}

/* Wrapper function for rte_eth_tx_burst() with ring latency feature. */
uint16_t
sppwk_eth_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	nb_tx = rte_eth_tx_burst(port_id, 0, tx_pkts, nb_pkts);

	if (iface_type == RING)
		spp_ringlatencystats_add_time_stamp(
				iface_no,
				tx_pkts, nb_pkts);
	return nb_tx;
}

#endif /* SPP_RINGLATENCYSTATS_ENABLE */
