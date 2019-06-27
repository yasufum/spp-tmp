/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_memcpy.h>
#include <rte_random.h>
#include <rte_byteorder.h>
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <netinet/in.h>

#include "classifier_mac.h"
#include "spp_vf.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/string_buffer.h"
#include "shared/secondary/json_helper.h"
#include "shared/secondary/spp_worker_th/cmd_res_formatter.h"
#include "shared/secondary/spp_worker_th/vf_deps.h"
#include "shared/secondary/spp_worker_th/spp_port.h"

#define RTE_LOGTYPE_SPP_CLASSIFIER_MAC RTE_LOGTYPE_USER1

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC rte_jhash
#endif

/* number of classifier information (reference/update) */
#define NOF_CLS_INFO 2

/* number of classifier mac table entry */
#define NOF_CLS_TABLE_ENTRIES 128

/* interval that wait until change update index (micro second) */
#define CHANGE_UPDATE_INDEX_WAIT_INTERVAL SPP_CHANGE_UPDATE_INTERVAL

/*
 *  interval that transmit burst packet,
 *  if buffer is not filled (nano second)
 */
#define DRAIN_TX_PACKET_INTERVAL 100

/* VID of VLAN untagged */
#define VLAN_UNTAGGED_VID 0x0fff

/** Value for default MAC address of classifier */
#define CLS_DUMMY_ADDR 0x010000000000

/* classifier management information */
struct management_info {
	/* classifier information */
	struct cls_comp_info cmp_infos[NOF_CLS_INFO];

	/* Reference index number for classifier information */
	volatile int ref_index;

	/* Update index number for classifier information */
	volatile int upd_index;

	/* used flag */
	volatile int is_used;
};

struct management_info g_mng_infos[RTE_MAX_LCORE];

/* uninitialize classifier. */
static void
uninit_classifier(struct management_info *mng_info)
{
	int i;

	mng_info->is_used = 0;

	for (i = 0; i < NOF_CLS_INFO; ++i)
		uninit_component_info(mng_info->cmp_infos + (long)i);

	memset(mng_info, 0, sizeof(struct management_info));
}

/* initialize classifier information. */
void
init_classifier_info(int component_id)
{
	struct management_info *mng_info = NULL;

	mng_info = g_mng_infos + component_id;
	uninit_classifier(mng_info);
}

/*
 * hash table name buffer size
 *[reson for value]
 *	in dpdk's lib/librte_hash/rte_cuckoo_hash.c
 *		snprintf(ring_name, sizeof(ring_name), "HT_%s", params->name);
 *		snprintf(hash_name, sizeof(hash_name), "HT_%s", params->name);
 *	ring_name buffer size is RTE_RING_NAMESIZE
 *	hash_name buffer size is RTE_HASH_NAMESIZE
 */
static const size_t HASH_TABLE_NAME_BUF_SZ =
		((RTE_HASH_NAMESIZE < RTE_RING_NAMESIZE) ?  RTE_HASH_NAMESIZE :
		RTE_RING_NAMESIZE) - 3;

/* mac address string(xx:xx:xx:xx:xx:xx) buffer size */
static const size_t ETHER_ADDR_STR_BUF_SZ =
		ETHER_ADDR_LEN * 2 + (ETHER_ADDR_LEN - 1) + 1;

/* classifier information per lcore */
struct management_info g_mng_infos[RTE_MAX_LCORE];

/**
 * Hash table count used for making a name of hash table
 *
 * This function is required because it is incremented at the time of use,
 * but since we want to start at 0.
 */
static rte_atomic16_t g_hash_table_count = RTE_ATOMIC16_INIT(0xff);

/* get vid from packet */
static inline uint16_t
get_vid(const struct rte_mbuf *pkt)
{
	struct ether_hdr *eth;
	struct vlan_hdr *vh;

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	if (eth->ether_type == rte_cpu_to_be_16(ETHER_TYPE_VLAN)) {
		/* vlan tagged */
		vh = (struct vlan_hdr *)(eth + 1);
		return rte_be_to_cpu_16(vh->vlan_tci) & 0x0fff;
	}

	/* vlan untagged */
	return VLAN_UNTAGGED_VID;
}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG

#define LOG_DBG(name, fmt, ...)                                        \
		RTE_LOG_DP(DEBUG, SPP_CLASSIFIER_MAC,                  \
				"[%s]Log(%s:%d):"fmt,                  \
				name, __func__, __LINE__, __VA_ARGS__)

static void
log_packet(const char *name, struct rte_mbuf *pkt,
		const char *func_name, int line_num)
{
	struct ether_hdr *eth;
	uint16_t vid;
	char mac_addr_str[2][ETHER_ADDR_STR_BUF_SZ];

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	vid = get_vid(pkt);

	ether_format_addr(mac_addr_str[0], sizeof(mac_addr_str),
			&eth->d_addr);
	ether_format_addr(mac_addr_str[1], sizeof(mac_addr_str),
			&eth->s_addr);

	RTE_LOG_DP(DEBUG, SPP_CLASSIFIER_MAC,
			"[%s]Packet(%s:%d). d_addr=%s, s_addr=%s, "
			"vid=%hu, pktlen=%u\n",
			name,
			func_name,
			line_num,
			mac_addr_str[0],
			mac_addr_str[1],
			vid,
			rte_pktmbuf_pkt_len(pkt));
}

#define LOG_PKT(name, pkt) \
		log_packet(name, pkt, __func__, __LINE__)

static void
log_classification(
		long clsd_idx,
		struct rte_mbuf *pkt,
		struct cls_comp_info *cmp_info,
		struct cls_port_info *clsd_data,
		const char *func_name,
		int line_num)
{
	struct ether_hdr *eth;
	uint16_t vid;
	char mac_addr_str[2][ETHER_ADDR_STR_BUF_SZ];
	char iface_str[STR_LEN_NAME];

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	vid = get_vid(pkt);

	ether_format_addr(mac_addr_str[0], sizeof(mac_addr_str),
			&eth->d_addr);
	ether_format_addr(mac_addr_str[1], sizeof(mac_addr_str),
			&eth->s_addr);

	if (clsd_idx < 0)
		snprintf(iface_str, sizeof(iface_str), "%ld", clsd_idx);
	else
		sppwk_port_uid(iface_str,
				clsd_data[clsd_idx].iface_type,
				clsd_data[clsd_idx].iface_no_global);

	RTE_LOG_DP(DEBUG, SPP_CLASSIFIER_MAC,
			"[%s]Classification(%s:%d). d_addr=%s, "
			"s_addr=%s, vid=%hu, pktlen=%u, tx_iface=%s\n",
			cmp_info->name,
			func_name,
			line_num,
			mac_addr_str[0],
			mac_addr_str[1],
			vid,
			rte_pktmbuf_pkt_len(pkt),
			iface_str);
}

#define LOG_CLS(clsd_idx, pkt, cmp_info, clsd_data)                    \
		log_classification(clsd_idx, pkt, cmp_info, clsd_data, \
				__func__, __LINE__)

/* Log DEBUG message for classified MAC and VLAN info. */
static void
log_entry(
		long clsd_idx,
		uint16_t vid,
		const char *mac_addr_str,
		struct cls_comp_info *cmp_info,
		struct cls_port_info *clsd_data,
		const char *func_name,
		int line_num)
{
	char iface_str[STR_LEN_NAME];

	if (clsd_idx < 0)
		snprintf(iface_str, sizeof(iface_str), "%ld", clsd_idx);
	else
		sppwk_port_uid(iface_str,
				clsd_data[clsd_idx].iface_type,
				clsd_data[clsd_idx].iface_no_global);

	RTE_LOG_DP(DEBUG, SPP_CLASSIFIER_MAC,
			"[%s]Entry(%s:%d). vid=%hu, mac_addr=%s, iface=%s\n",
			cmp_info->name,
			func_name,
			line_num,
			vid,
			mac_addr_str,
			iface_str);
}
#define LOG_ENT(clsd_idx, vid, mac_addr_str, cmp_info, clsd_data)           \
		log_entry(clsd_idx, vid, mac_addr_str, cmp_info, clsd_data, \
				__func__, __LINE__)
#else
#define LOG_DBG(name, fmt, ...)
#define LOG_PKT(name, pkt)
#define LOG_CLS(pkt, clsd_idx, cmp_info, clsd_data)
#define LOG_ENT(clsd_idx, vid, mac_addr_str, cmp_info, clsd_data)
#endif

/* check if management information is used. */
static inline int
is_used_mng_info(const struct management_info *mng_info)
{
	return (mng_info != NULL && mng_info->is_used);
}

/* create mac classification instance. */
static struct mac_classifier *
create_mac_classification(void)
{
	struct mac_classifier *mac_cls;
	char hash_tab_name[HASH_TABLE_NAME_BUF_SZ];
	struct rte_hash **mac_cls_tab;

	mac_cls = (struct mac_classifier *)rte_zmalloc(
			NULL, sizeof(struct mac_classifier), 0);

	if (unlikely(mac_cls == NULL))
		return NULL;

	mac_cls->nof_cls_ports = 0;
	mac_cls->default_cls_idx = -1;

	mac_cls_tab = &mac_cls->cls_tbl;

	/* make hash table name(require uniqueness between processes) */
	sprintf(hash_tab_name, "cmtab_%07x%02hx",
			getpid(),
			rte_atomic16_add_return(&g_hash_table_count, 1));

	RTE_LOG(INFO, SPP_CLASSIFIER_MAC, "Create table. name=%s, bufsz=%lu\n",
			hash_tab_name, HASH_TABLE_NAME_BUF_SZ);

	/* set hash creating parameters */
	struct rte_hash_parameters hash_params = {
			.name      = hash_tab_name,
			.entries   = NOF_CLS_TABLE_ENTRIES,
			.key_len   = sizeof(struct ether_addr),
			.hash_func = DEFAULT_HASH_FUNC,
			.hash_func_init_val = 0,
			.socket_id = rte_socket_id(),
	};

	/* create classifier mac table (hash table) */
	*mac_cls_tab = rte_hash_create(&hash_params);
	if (unlikely(*mac_cls_tab == NULL)) {
		RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
				"Cannot create mac classification table. "
				"name=%s\n", hash_tab_name);
		rte_free(mac_cls);
		return NULL;
	}

	return mac_cls;
}

/* initialize classifier information. */
static int
init_component_info(struct cls_comp_info *cmp_info,
		const struct sppwk_comp_info *wk_comp_info)
{
	int ret = SPP_RET_NG;
	int i;
	struct mac_classifier *mac_cls;
	struct ether_addr eth_addr;
	char mac_addr_str[ETHER_ADDR_STR_BUF_SZ];
	/* Classifier has one RX port and several TX ports. */
	struct cls_port_info *cls_rx_port_info = &cmp_info->rx_port_i;
	struct cls_port_info *cls_tx_ports_info = cmp_info->tx_ports_i;
	struct sppwk_port_info *tx_port = NULL;
	uint16_t vid;

	/* set rx */
	if (wk_comp_info->nof_rx == 0) {
		cls_rx_port_info->iface_type = UNDEF;
		cls_rx_port_info->iface_no = 0;
		cls_rx_port_info->iface_no_global = 0;
		cls_rx_port_info->ethdev_port_id = 0;
		cls_rx_port_info->nof_pkts = 0;
	} else {
		cls_rx_port_info->iface_type =
			wk_comp_info->rx_ports[0]->iface_type;
		cls_rx_port_info->iface_no = 0;
		cls_rx_port_info->iface_no_global =
			wk_comp_info->rx_ports[0]->iface_no;
		cls_rx_port_info->ethdev_port_id =
			wk_comp_info->rx_ports[0]->ethdev_port_id;
		cls_rx_port_info->nof_pkts = 0;
	}

	/* set tx */
	cmp_info->nof_tx_ports = wk_comp_info->nof_tx;
	cmp_info->mac_addr_entry = 0;
	for (i = 0; i < wk_comp_info->nof_tx; i++) {
		tx_port = wk_comp_info->tx_ports[i];
		vid = tx_port->cls_attrs.vlantag.vid;

		/* store ports information */
		cls_tx_ports_info[i].iface_type = tx_port->iface_type;
		cls_tx_ports_info[i].iface_no = i;
		cls_tx_ports_info[i].iface_no_global = tx_port->iface_no;
		cls_tx_ports_info[i].ethdev_port_id = tx_port->ethdev_port_id;
		cls_tx_ports_info[i].nof_pkts = 0;

		if (tx_port->cls_attrs.mac_addr == 0)
			continue;

		/* if mac classification is NULL, make instance */
		if (unlikely(cmp_info->mac_clfs[vid] == NULL)) {
			RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
					"Mac classification is not registered."
					" create. vid=%hu\n", vid);
			cmp_info->mac_clfs[vid] =
					create_mac_classification();
			if (unlikely(cmp_info->mac_clfs[vid] == NULL))
				return SPP_RET_NG;
		}
		mac_cls = cmp_info->mac_clfs[vid];

		/* store active tx_port that associate with mac address */
		mac_cls->cls_ports[mac_cls->nof_cls_ports++] = i;

		/* mac address entry flag set */
		cmp_info->mac_addr_entry = 1;

		/* store default classified */
		if (unlikely(tx_port->cls_attrs.mac_addr == CLS_DUMMY_ADDR)) {
			mac_cls->default_cls_idx = i;
			RTE_LOG(INFO, SPP_CLASSIFIER_MAC,
					"default classified. vid=%hu, "
					"iface_type=%d, iface_no=%d, "
					"ethdev_port_id=%d\n",
					vid,
					tx_port->iface_type,
					tx_port->iface_no,
					tx_port->ethdev_port_id);
			continue;
		}

		/* add entry to classifier mac table */
		rte_memcpy(&eth_addr, &tx_port->cls_attrs.mac_addr,
				ETHER_ADDR_LEN);
		ether_format_addr(mac_addr_str, sizeof(mac_addr_str),
				&eth_addr);

		ret = rte_hash_add_key_data(mac_cls->cls_tbl,
				(void *)&eth_addr, (void *)(long)i);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
					"Cannot add entry to classifier mac "
					"table. ret=%d, vid=%hu, "
					"mac_addr=%s\n",
					ret, vid, mac_addr_str);
			return SPP_RET_NG;
		}

		RTE_LOG(INFO, SPP_CLASSIFIER_MAC,
				"Add entry to classifier mac table. "
				"vid=%hu, mac_addr=%s, iface_type=%d, "
				"iface_no=%d, ethdev_port_id=%d\n",
				vid,
				mac_addr_str,
				tx_port->iface_type,
				tx_port->iface_no,
				tx_port->ethdev_port_id);
	}

	return SPP_RET_OK;
}

/* uninitialize classifier information. */
void
uninit_component_info(struct cls_comp_info *comp_info)
{
	int i;
	for (i = 0; i < NOF_VLAN; ++i)
		free_mac_classifier(comp_info->mac_clfs[i]);
	memset(comp_info, 0, sizeof(struct cls_comp_info));
}

/* transmit packet to one destination. */
static inline void
transmit_packet(struct cls_port_info *clsd_data)
{
	int i;
	uint16_t n_tx;

	/* transmit packets */
	n_tx = spp_eth_tx_burst(clsd_data->ethdev_port_id, 0,
			clsd_data->pkts, clsd_data->nof_pkts);

	/* free cannot transmit packets */
	if (unlikely(n_tx != clsd_data->nof_pkts)) {
		for (i = n_tx; i < clsd_data->nof_pkts; i++)
			rte_pktmbuf_free(clsd_data->pkts[i]);
		RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
				"drop packets(tx). num=%hu, ethdev_port_id=%hu\n",
				(uint16_t)(clsd_data->nof_pkts - n_tx),
				clsd_data->ethdev_port_id);
	}

	clsd_data->nof_pkts = 0;
}

/* transmit packet to one destination. */
static inline void
transmit_all_packet(struct cls_comp_info *cmp_info)
{
	int i;
	struct cls_port_info *clsd_data_tx = cmp_info->tx_ports_i;

	for (i = 0; i < cmp_info->nof_tx_ports; i++) {
		if (unlikely(clsd_data_tx[i].nof_pkts != 0)) {
			RTE_LOG(INFO, SPP_CLASSIFIER_MAC,
					"transmit all packets (drain). "
					"index=%d, "
					"nof_pkts=%hu\n",
					i,
					clsd_data_tx[i].nof_pkts);
			transmit_packet(&clsd_data_tx[i]);
		}
	}
}

/* set mbuf pointer to tx buffer and transmit packet, if buffer is filled */
static inline void
push_packet(struct rte_mbuf *pkt, struct cls_port_info *clsd_data)
{
	clsd_data->pkts[clsd_data->nof_pkts++] = pkt;

	/* transmit packet, if buffer is filled */
	if (unlikely(clsd_data->nof_pkts == MAX_PKT_BURST)) {
		RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
				"transmit packets (buffer is filled). "
				"iface_type=%d, iface_no={%d,%d}, "
				"tx_port=%hu, nof_pkts=%hu\n",
				clsd_data->iface_type,
				clsd_data->iface_no_global,
				clsd_data->iface_no,
				clsd_data->ethdev_port_id,
				clsd_data->nof_pkts);
		transmit_packet(clsd_data);
	}
}

/* get index of general default classified */
static inline int
get_general_default_classified_index(struct cls_comp_info *cmp_info)
{
	struct mac_classifier *mac_cls;

	mac_cls = cmp_info->mac_clfs[VLAN_UNTAGGED_VID];
	if (unlikely(mac_cls == NULL)) {
		LOG_DBG(cmp_info->name, "Untagged's default is not set. "
				"vid=%d\n", (int)VLAN_UNTAGGED_VID);
		return SPP_RET_NG;
	}

	return mac_cls->default_cls_idx;
}

/* handle L2 multicast(include broadcast) packet */
static inline void
handle_l2multicast_packet(struct rte_mbuf *pkt,
		struct cls_comp_info *cmp_info,
		struct cls_port_info *clsd_data)
{
	int i;
	struct mac_classifier *mac_cls;
	uint16_t vid = get_vid(pkt);
	int gen_def_clsd_idx = get_general_default_classified_index(cmp_info);
	int n_act_clsd;

	/* select mac address classification by vid */
	mac_cls = cmp_info->mac_clfs[vid];
	if (unlikely(mac_cls == NULL ||
			mac_cls->nof_cls_ports == 0)) {
		/* specific vlan is not registered
		 * use untagged's default(as general default)
		 */
		if (unlikely(gen_def_clsd_idx < 0)) {
			/* untagged's default is not registered too */
			RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
					"No entry.(l2 multicast packet)\n");
			rte_pktmbuf_free(pkt);
			return;
		}

		/* transmit to untagged's default(as general default) */
		LOG_CLS((long)gen_def_clsd_idx, pkt, cmp_info, clsd_data);
		push_packet(pkt, clsd_data + (long)gen_def_clsd_idx);
		return;
	}

	/* add to mbuf's refcnt */
	n_act_clsd = mac_cls->nof_cls_ports;
	if (gen_def_clsd_idx >= 0 && vid != VLAN_UNTAGGED_VID)
		++n_act_clsd;

	rte_mbuf_refcnt_update(pkt, (int16_t)(n_act_clsd - 1));

	/* transmit to specific segment & general default */
	for (i = 0; i < mac_cls->nof_cls_ports; i++) {
		LOG_CLS((long)mac_cls->cls_ports[i], pkt, cmp_info, clsd_data);
		push_packet(pkt, clsd_data + (long)mac_cls->cls_ports[i]);
	}

	if (gen_def_clsd_idx >= 0 && vid != VLAN_UNTAGGED_VID) {
		LOG_CLS((long)gen_def_clsd_idx, pkt, cmp_info, clsd_data);
		push_packet(pkt, clsd_data + (long)gen_def_clsd_idx);
	}
}

/* select index of classified */
static inline int
select_classified_index(const struct rte_mbuf *pkt,
		struct cls_comp_info *cmp_info)
{
	int ret;
	struct ether_hdr *eth;
	void *lookup_data;
	struct mac_classifier *mac_cls;
	uint16_t vid;

	eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	vid = get_vid(pkt);

	/* select mac address classification by vid */
	mac_cls = cmp_info->mac_clfs[vid];
	if (unlikely(mac_cls == NULL)) {
		LOG_DBG(cmp_info->name, "Mac classification is not "
				"registered. vid=%hu\n", vid);
		return get_general_default_classified_index(cmp_info);
	}

	/* find in table (by destination mac address) */
	ret = rte_hash_lookup_data(mac_cls->cls_tbl,
			(const void *)&eth->d_addr, &lookup_data);
	if (ret >= 0) {
		LOG_DBG(cmp_info->name, "Mac address is registered. "
				"ret=%d, vid=%hu\n", ret, vid);
		return (int)(long)lookup_data;
	}

	LOG_DBG(cmp_info->name,
			"Mac address is not registered. ret=%d, "
			"(EINVAL=%d, ENOENT=%d)\n", ret, EINVAL, ENOENT);

	/* check if packet is l2 multicast */
	if (unlikely(is_multicast_ether_addr(&eth->d_addr)))
		return -2;

	/* if default is not set, use untagged's default */
	if (unlikely(mac_cls->default_cls_idx < 0 &&
			vid != VLAN_UNTAGGED_VID)) {
		LOG_DBG(cmp_info->name, "Vid's default is not set. "
				"use general default. vid=%hu\n", vid);
		return get_general_default_classified_index(cmp_info);
	}

	/* use default */
	LOG_DBG(cmp_info->name, "Use vid's default. vid=%hu\n", vid);
	return mac_cls->default_cls_idx;
}

/*
 * classify packet by destination mac address,
 * and transmit packet (conditional).
 */
static inline void
classify_packet(struct rte_mbuf **rx_pkts, uint16_t n_rx,
		struct cls_comp_info *cmp_info,
		struct cls_port_info *clsd_data)
{
	int i;
	long clsd_idx;

	for (i = 0; i < n_rx; i++) {
		LOG_PKT(cmp_info->name, rx_pkts[i]);

		clsd_idx = select_classified_index(rx_pkts[i], cmp_info);
		LOG_CLS(clsd_idx, rx_pkts[i], cmp_info, clsd_data);

		if (likely(clsd_idx >= 0)) {
			LOG_DBG(cmp_info->name, "as unicast packet. i=%d\n",
					i);
			push_packet(rx_pkts[i], clsd_data + clsd_idx);
		} else if (unlikely(clsd_idx == -1)) {
			LOG_DBG(cmp_info->name, "no destination. "
					"drop packet. i=%d\n", i);
			rte_pktmbuf_free(rx_pkts[i]);
		} else if (unlikely(clsd_idx == -2)) {
			LOG_DBG(cmp_info->name, "as multicast packet. i=%d\n",
					i);
			handle_l2multicast_packet(rx_pkts[i],
					cmp_info, clsd_data);
		}
	}
}

/* change update index at classifier management information */
static inline void
change_classifier_index(struct management_info *mng_info, int id)
{
	if (unlikely(mng_info->ref_index ==
			mng_info->upd_index)) {
		/* Change reference index of port ability. */
		spp_port_ability_change_index(PORT_ABILITY_CHG_INDEX_REF,
									0, 0);

		/* Transmit all packets for switching the using data. */
		transmit_all_packet(mng_info->cmp_infos + mng_info->ref_index);

		RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
				"Core[%u] Change update index.\n", id);
		mng_info->ref_index =
				(mng_info->upd_index + 1) % NOF_CLS_INFO;
	}
}

/* classifier(mac address) initialize globals. */
int
spp_classifier_mac_init(void)
{
	memset(g_mng_infos, 0, sizeof(g_mng_infos));
	return 0;
}

/* classifier(mac address) update component info. */
int
update_classifier(struct sppwk_comp_info *wk_comp_info)
{
	int ret;
	int wk_id = wk_comp_info->comp_id;
	struct management_info *mng_info = g_mng_infos + wk_id;
	struct cls_comp_info *cls_info = NULL;

	RTE_LOG(INFO, SPP_CLASSIFIER_MAC,
			"Start updating classifier, id=%u.\n", wk_id);

	/* TODO(yasufum) rename `infos`. */
	cls_info = mng_info->cmp_infos + mng_info->upd_index;

	/* initialize update side classifier information */
	ret = init_component_info(cls_info, wk_comp_info);
	if (unlikely(ret != SPP_RET_OK)) {
		RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
				"Cannot update classifier, ret=%d.\n", ret);
		return ret;
	}
	memcpy(cls_info->name, wk_comp_info->name, STR_LEN_NAME);

	/* change index of reference side */
	mng_info->upd_index = mng_info->ref_index;
	mng_info->is_used = 1;

	/* wait until no longer access the new update side */
	while (likely(mng_info->ref_index ==
			mng_info->upd_index))
		rte_delay_us_block(CHANGE_UPDATE_INDEX_WAIT_INTERVAL);

	/* uninitialize old */
	uninit_component_info(mng_info->cmp_infos + mng_info->upd_index);

	RTE_LOG(INFO, SPP_CLASSIFIER_MAC,
			"Done update classifier, id=%u.\n", wk_id);

	return SPP_RET_OK;
}

/* classifier(mac address) thread function. */
int
spp_classifier_mac_do(int id)
{
	int i;
	int n_rx;
	struct management_info *mng_info = g_mng_infos + id;
	struct cls_comp_info *cmp_info = NULL;
	struct rte_mbuf *rx_pkts[MAX_PKT_BURST];

	struct cls_port_info *clsd_data_rx = NULL;
	struct cls_port_info *clsd_data_tx = NULL;

	uint64_t cur_tsc, prev_tsc = 0;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * DRAIN_TX_PACKET_INTERVAL;

	/* change index of update classifier management information */
	change_classifier_index(mng_info, id);

	cmp_info = mng_info->cmp_infos + mng_info->ref_index;
	clsd_data_rx = &cmp_info->rx_port_i;
	clsd_data_tx = cmp_info->tx_ports_i;

	/**
	 * decide classifier information of the current cycle If at least,
	 * one rx port, one tx port and one classifier_table exist, then start
	 * classifying. If not, stop classifying.
	 */
	if (!(clsd_data_rx->iface_type != UNDEF &&
			cmp_info->nof_tx_ports >= 1 &&
			cmp_info->mac_addr_entry == 1))
		return SPP_RET_OK;

	/* drain tx packets, if buffer is not filled for interval */
	cur_tsc = rte_rdtsc();
	if (unlikely(cur_tsc - prev_tsc > drain_tsc)) {
		for (i = 0; i < cmp_info->nof_tx_ports; i++) {
			if (likely(clsd_data_tx[i].nof_pkts == 0))
				continue;

			RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
					"transmit packets (drain). index=%d, "
					"nof_pkts=%hu, interval=%lu\n",
					i, clsd_data_tx[i].nof_pkts,
					cur_tsc - prev_tsc);
				transmit_packet(&clsd_data_tx[i]);
		}
		prev_tsc = cur_tsc;
	}

	if (clsd_data_rx->iface_type == UNDEF)
		return SPP_RET_OK;

	/* retrieve packets */
	n_rx = spp_eth_rx_burst(clsd_data_rx->ethdev_port_id, 0, rx_pkts,
			MAX_PKT_BURST);
	if (unlikely(n_rx == 0))
		return SPP_RET_OK;

	/* classify and interval that transmit burst packet */
	classify_packet(rx_pkts, n_rx, cmp_info, clsd_data_tx);

	return SPP_RET_OK;
}

/* classifier iterate component information */
int
get_classifier_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params)
{
	int ret = SPP_RET_NG;
	int i;
	int nof_tx, nof_rx = 0;  /* Num of RX and TX ports. */
	struct management_info *mng_info;
	struct cls_comp_info *cmp_info;
	struct cls_port_info *port_info;
	struct sppwk_port_idx rx_ports[RTE_MAX_ETHPORTS];
	struct sppwk_port_idx tx_ports[RTE_MAX_ETHPORTS];

	mng_info = g_mng_infos + id;
	if (!is_used_mng_info(mng_info)) {
		RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
				"Classifier is not used "
				"(comp_id=%d, lcore_id=%d, type=%d).\n",
				id, lcore_id, SPPWK_TYPE_CLS);
		return SPP_RET_NG;
	}

	cmp_info = mng_info->cmp_infos + mng_info->ref_index;
	port_info = cmp_info->tx_ports_i;

	memset(rx_ports, 0x00, sizeof(rx_ports));
	if (cmp_info->rx_port_i.iface_type != UNDEF) {
		nof_rx = 1;
		rx_ports[0].iface_type = cmp_info->rx_port_i.iface_type;
		rx_ports[0].iface_no = cmp_info->rx_port_i.iface_no_global;
	}

	memset(tx_ports, 0x00, sizeof(tx_ports));
	nof_tx = cmp_info->nof_tx_ports;
	for (i = 0; i < nof_tx; i++) {
		tx_ports[i].iface_type = port_info[i].iface_type;
		tx_ports[i].iface_no = port_info[i].iface_no_global;
	}

	/* Set the information with the function specified by the command. */
	/* TODO(yasufum) rename `element_proc` */
	ret = (*params->element_proc)(
		params, lcore_id, cmp_info->name, SPPWK_TYPE_CLS_STR,
		nof_rx, rx_ports, nof_tx, tx_ports);
	if (unlikely(ret != SPP_RET_OK))
		return SPP_RET_NG;

	return SPP_RET_OK;
}

/**
 * For setting up a response for `status` command, iterate adding each of
 * entries in MAC address table to the result message.
 */
static void
iterate_adding_mac_entry(
		struct spp_iterate_classifier_table_params *params,
		uint16_t vid,
		struct mac_classifier *mac_cls,
		__rte_unused struct cls_comp_info *cmp_info,
		struct cls_port_info *port_info)
{
	int ret;
	const void *key;
	void *data;
	uint32_t next;
	struct sppwk_port_idx port;
	char mac_addr_str[ETHER_ADDR_STR_BUF_SZ];
	enum sppwk_cls_type cls_type;

	cls_type = SPPWK_CLS_TYPE_VLAN;
	if (unlikely(vid == VLAN_UNTAGGED_VID))
		cls_type = SPPWK_CLS_TYPE_MAC;

	if (mac_cls->default_cls_idx >= 0) {
		port.iface_type = (port_info +
				mac_cls->default_cls_idx)->iface_type;
		port.iface_no = (port_info +
				mac_cls->default_cls_idx)->iface_no_global;

		/* Logging DEBUG message. */
		LOG_ENT((long)mac_cls->default_cls_idx, vid,
				SPPWK_TERM_DEFAULT, cmp_info, port_info);
		/**
		 * Append "default" entry. `element_proc` is a funciton
		 * pointer to append_classifier_element_value().
		 */
		(*params->element_proc)(params, cls_type, vid,
				SPPWK_TERM_DEFAULT, &port);
	}

	next = 0;
	while (1) {
		ret = rte_hash_iterate(mac_cls->cls_tbl, &key, &data, &next);

		if (unlikely(ret < 0))
			break;

		ether_format_addr(mac_addr_str, sizeof(mac_addr_str),
				(const struct ether_addr *)key);

		port.iface_type = (port_info + (long)data)->iface_type;
		port.iface_no = (port_info + (long)data)->iface_no_global;

		LOG_ENT((long)data, vid, mac_addr_str, cmp_info, port_info);

		/**
		 * Append each entry of MAC address. `element_proc` is a
		 * funciton pointer to append_classifier_element_value().
		 */
		(*params->element_proc)(params, cls_type, vid,
				mac_addr_str, &port);
	}
}

/**
 * Setup data of classifier table and call iterator function for getting
 * each of entries.
 */
int
add_classifier_table_val(
		struct spp_iterate_classifier_table_params *params)
{
	int i, vlan_id;
	struct management_info *mng_info;
	struct cls_comp_info *cmp_info;
	struct cls_port_info *port_info;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		mng_info = g_mng_infos + i;
		if (!is_used_mng_info(mng_info))
			continue;

		cmp_info = mng_info->cmp_infos + mng_info->ref_index;
		port_info = cmp_info->tx_ports_i;

		RTE_LOG(DEBUG, SPP_CLASSIFIER_MAC,
			"Start iterate classifier table on lcore %u.\n", i);

		for (vlan_id = 0; vlan_id < NOF_VLAN; ++vlan_id) {
			if (cmp_info->mac_clfs[vlan_id] == NULL)
				continue;

			iterate_adding_mac_entry(params, (uint16_t) vlan_id,
					cmp_info->mac_clfs[vlan_id], cmp_info,
					port_info);
		}
	}

	return SPP_RET_OK;
}

/* Iterate classifier_table to create response to status command */
static int
_add_classifier_table(
		struct spp_iterate_classifier_table_params *params)
{
	int ret;

	ret = add_classifier_table_val(params);
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
				"Cannot iterate classifier_mac_table.\n");
		return SPP_RET_NG;
	}

	return SPP_RET_OK;
}

/**
 * Add entries of classifier table in JSON. Before iterating the entries,
 * this function calls several nested functions.
 *   add_classifier_table()  // This function.
 *     -> _add_classifier_table()  // Wrapper and doesn't almost nothing.
 *       -> add_classifier_table_val()  // Setup data and call iterator.
 *         -> iterate_adding_mac_entry()
 */
int
add_classifier_table(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPP_RET_NG;
	struct spp_iterate_classifier_table_params itr_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_CLASSIFIER_MAC,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	itr_params.output = tmp_buff;
	itr_params.element_proc = append_classifier_element_value;

	ret = _add_classifier_table(&itr_params);
	if (unlikely(ret != SPP_RET_OK)) {
		spp_strbuf_free(itr_params.output);
		return SPP_RET_NG;
	}

	ret = append_json_array_brackets(output, name, itr_params.output);
	spp_strbuf_free(itr_params.output);
	return ret;
}
