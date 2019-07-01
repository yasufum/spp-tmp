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

/* Port ability management information */
struct port_abl_info {
	volatile int ref_index; /* Index to reference area. */
	volatile int upd_index; /* Index to update area. */
	struct sppwk_port_attrs port_attrs[TWO_SIDES][PORT_ABL_MAX];
				/* Port attributes for spp_vf. */
};

/* Port ability port information */
struct port_mng_info {
	enum port_type iface_type;  /* Interface type (phy, vhost or so). */
	int iface_no;  /* Interface number. */
	struct port_abl_info rx;  /* Mng data of port ability for RX. */
	struct port_abl_info tx;  /* Mng data of port ability for Tx. */
};

/* Information for VLAN tag management. */
struct port_mng_info g_port_mng_info[RTE_MAX_ETHPORTS];

/* TPID of VLAN. */
static uint16_t g_vlan_tpid;

/* Initialize port ability. */
void
spp_port_ability_init(void)
{
	int cnt = 0;
	g_vlan_tpid = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	memset(g_port_mng_info, 0x00, sizeof(g_port_mng_info));
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		g_port_mng_info[cnt].rx.ref_index = 0;
		g_port_mng_info[cnt].rx.upd_index = 1;
		g_port_mng_info[cnt].tx.ref_index = 0;
		g_port_mng_info[cnt].tx.upd_index = 1;
	}
}

/* Get information of port ability. */
void
spp_port_ability_get_info(
		int port_id, enum sppwk_port_dir dir,
		struct sppwk_port_attrs **info)
{
	struct port_abl_info *mng = NULL;

	switch (dir) {
	case SPPWK_PORT_DIR_RX:
		mng = &g_port_mng_info[port_id].rx;
		break;
	case SPPWK_PORT_DIR_TX:
		mng = &g_port_mng_info[port_id].tx;
		break;
	default:
		/* Not used. */
		break;
	}
	*info = mng->port_attrs[mng->ref_index];
}

/* Calculation and Setting of FCS. */
static inline void
set_fcs_packet(struct rte_mbuf *pkt)
{
	uint32_t *fcs = NULL;
	fcs = rte_pktmbuf_mtod_offset(pkt, uint32_t *, pkt->data_len);
	*fcs = rte_net_crc_calc(rte_pktmbuf_mtod(pkt, void *),
			pkt->data_len, RTE_NET_CRC32_ETH);
}

/* Add VLAN tag to packet. */
static inline int
add_vlantag_packet(
		struct rte_mbuf *pkt,
		const union sppwk_port_capability *capability)
{
	struct ether_hdr *old_ether = NULL;
	struct ether_hdr *new_ether = NULL;
	struct vlan_hdr  *vlan      = NULL;
	const struct sppwk_vlan_tag *vlantag = &capability->vlantag;

	old_ether = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	if (old_ether->ether_type == g_vlan_tpid) {
		/* For packets with VLAN tags, only VLAN ID is updated */
		new_ether = old_ether;
		vlan = (struct vlan_hdr *)&new_ether[1];
	} else {
		/* For packets without VLAN tag, add VLAN tag. */
		new_ether = (struct ether_hdr *)rte_pktmbuf_prepend(pkt,
				sizeof(struct vlan_hdr));
		if (unlikely(new_ether == NULL)) {
			RTE_LOG(ERR, PORT, "Failed to "
					"get additional header area.\n");
			return SPP_RET_NG;
		}

		rte_memcpy(new_ether, old_ether, sizeof(struct ether_hdr));
		vlan = (struct vlan_hdr *)&new_ether[1];
		vlan->eth_proto = new_ether->ether_type;
		new_ether->ether_type = g_vlan_tpid;
	}

	vlan->vlan_tci = vlantag->tci;
	set_fcs_packet(pkt);
	return SPP_RET_OK;
}

/* Add VLAN tag to all packets. */
static inline int
add_vlantag_all_packets(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability)
{
	int ret = SPP_RET_OK;
	int cnt = 0;
	for (cnt = 0; cnt < nb_pkts; cnt++) {
		ret = add_vlantag_packet(pkts[cnt], capability);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, PORT,
					"Failed to add VLAN tag."
					"(pkts %d/%d)\n", cnt, nb_pkts);
			break;
		}
	}
	return cnt;
}

/* Delete VLAN tag to packet. */
static inline int
del_vlantag_packet(
		struct rte_mbuf *pkt,
		const union sppwk_port_capability *cbl __attribute__ ((unused)))
{
	struct ether_hdr *old_ether = NULL;
	struct ether_hdr *new_ether = NULL;
	uint32_t *old, *new;

	old_ether = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	if (old_ether->ether_type == g_vlan_tpid) {
		/* For packets without VLAN tag, delete VLAN tag. */
		new_ether = (struct ether_hdr *)rte_pktmbuf_adj(pkt,
				sizeof(struct vlan_hdr));
		if (unlikely(new_ether == NULL)) {
			RTE_LOG(ERR, PORT, "Failed to "
					"delete unnecessary header area.\n");
			return SPP_RET_NG;
		}

		old = (uint32_t *)old_ether;
		new = (uint32_t *)new_ether;
		new[2] = old[2];
		new[1] = old[1];
		new[0] = old[0];
		old[0] = 0;
		set_fcs_packet(pkt);
	}
	return SPP_RET_OK;
}

/* Delete VLAN tag to all packets. */
static inline int
del_vlantag_all_packets(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability)
{
	int ret = SPP_RET_OK;
	int cnt = 0;
	for (cnt = 0; cnt < nb_pkts; cnt++) {
		ret = del_vlantag_packet(pkts[cnt], capability);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, PORT,
					"Failed to del VLAN tag."
					"(pkts %d/%d)\n", cnt, nb_pkts);
			break;
		}
	}
	return cnt;
}

/* Change index of management information. */
void
spp_port_ability_change_index(
		enum port_ability_chg_index_type type,
		int port_id, enum sppwk_port_dir dir)
{
	int cnt;
	static int num_rx;
	static int rx_list[RTE_MAX_ETHPORTS];
	static int num_tx;
	static int tx_list[RTE_MAX_ETHPORTS];
	struct port_abl_info *mng = NULL;

	if (type == PORT_ABILITY_CHG_INDEX_UPD) {
		switch (dir) {
		case SPPWK_PORT_DIR_RX:
			mng = &g_port_mng_info[port_id].rx;
			mng->upd_index = mng->ref_index;
			rx_list[num_rx++] = port_id;
			break;
		case SPPWK_PORT_DIR_TX:
			mng = &g_port_mng_info[port_id].tx;
			mng->upd_index = mng->ref_index;
			tx_list[num_tx++] = port_id;
			break;
		default:
			/* Not used. */
			break;
		}
		return;
	}

	for (cnt = 0; cnt < num_rx; cnt++) {
		mng = &g_port_mng_info[rx_list[cnt]].rx;
		mng->ref_index = (mng->upd_index+1) % TWO_SIDES;
		rx_list[cnt] = 0;
	}
	for (cnt = 0; cnt < num_tx; cnt++) {
		mng = &g_port_mng_info[tx_list[cnt]].tx;
		mng->ref_index = (mng->upd_index+1) % TWO_SIDES;
		tx_list[cnt] = 0;
	}

	num_rx = 0;
	num_tx = 0;
}

/* Set ability data of port ability. */
static void
port_ability_set_ability(struct sppwk_port_info *port,
		enum sppwk_port_dir dir)
{
	int in_cnt, out_cnt = 0;
	int port_id = port->ethdev_port_id;
	struct port_mng_info *port_mng = &g_port_mng_info[port_id];
	struct port_abl_info *mng = NULL;
	struct sppwk_port_attrs *port_attrs_in = port->port_attrs;
	struct sppwk_port_attrs *port_attrs_out = NULL;
	struct sppwk_vlan_tag *tag = NULL;

	port_mng->iface_type = port->iface_type;
	port_mng->iface_no   = port->iface_no;

	switch (dir) {
	case SPPWK_PORT_DIR_RX:
		mng = &port_mng->rx;
		break;
	case SPPWK_PORT_DIR_TX:
		mng = &port_mng->tx;
		break;
	default:
		/* Not used. */
		break;
	}

	port_attrs_out = mng->port_attrs[mng->upd_index];
	memset(port_attrs_out, 0x00, sizeof(struct sppwk_port_attrs)
			* PORT_ABL_MAX);
	for (in_cnt = 0; in_cnt < PORT_ABL_MAX; in_cnt++) {
		if (port_attrs_in[in_cnt].dir != dir)
			continue;

		memcpy(&port_attrs_out[out_cnt], &port_attrs_in[in_cnt],
				sizeof(struct sppwk_port_attrs));

		switch (port_attrs_out[out_cnt].ops) {
		case SPPWK_PORT_OPS_ADD_VLAN:
			tag = &port_attrs_out[out_cnt].capability.vlantag;
			tag->tci = rte_cpu_to_be_16(SPP_VLANTAG_CALC_TCI(
					tag->vid, tag->pcp));
			break;
		case SPPWK_PORT_OPS_DEL_VLAN:
		default:
			/* Nothing to do. */
			break;
		}

		out_cnt++;
	}

	spp_port_ability_change_index(PORT_ABILITY_CHG_INDEX_UPD,
			port_id, dir);
}

/* Update port capability. */
void
spp_port_ability_update(const struct sppwk_comp_info *component)
{
	int cnt;
	struct sppwk_port_info *port = NULL;
	for (cnt = 0; cnt < component->nof_rx; cnt++) {
		port = component->rx_ports[cnt];
		port_ability_set_ability(port, SPPWK_PORT_DIR_RX);
	}

	for (cnt = 0; cnt < component->nof_tx; cnt++) {
		port = component->tx_ports[cnt];
		port_ability_set_ability(port, SPPWK_PORT_DIR_TX);
	}
}

/* Definition of functions that operate port abilities. */
typedef int (*port_ability_func)(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability);

/* List of functions per port ability. */
port_ability_func port_ability_function_list[] = {
	NULL,                    /* None */
	add_vlantag_all_packets, /* Add VLAN tag */
	del_vlantag_all_packets, /* Del VLAN tag */
	NULL                     /* Termination */
};

/* Each packet operation of port capability. */
static inline int
port_ability_each_operation(uint16_t port_id,
		struct rte_mbuf **pkts, const uint16_t nb_pkts,
		enum sppwk_port_dir dir)
{
	int cnt, buf;
	int ok_pkts = nb_pkts;
	struct sppwk_port_attrs *port_attrs = NULL;

	spp_port_ability_get_info(port_id, dir, &port_attrs);
	if (unlikely(port_attrs[0].ops == SPPWK_PORT_OPS_NONE))
		return nb_pkts;

	for (cnt = 0; cnt < PORT_ABL_MAX; cnt++) {
		if (port_attrs[cnt].ops == SPPWK_PORT_OPS_NONE)
			break;

		ok_pkts = port_ability_function_list[port_attrs[cnt].ops](
				pkts, ok_pkts, &port_attrs->capability);
	}

	/* Discard remained packets to release mbuf. */
	if (unlikely(ok_pkts < nb_pkts)) {
		for (buf = ok_pkts; buf < nb_pkts; buf++)
			rte_pktmbuf_free(pkts[buf]);
	}

	return ok_pkts;
}

/* Wrapper function for rte_eth_rx_burst(). */
uint16_t
spp_eth_rx_burst(
		uint16_t port_id, uint16_t queue_id  __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx = 0;
	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);
	if (unlikely(nb_rx == 0))
		return SPP_RET_OK;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	if (g_port_mng_info[port_id].iface_type == RING)
		spp_ringlatencystats_calculate_latency(
				g_port_mng_info[port_id].iface_no,
				rx_pkts, nb_pkts);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	return port_ability_each_operation(port_id, rx_pkts, nb_rx,
			SPPWK_PORT_DIR_RX);
}

/* Wrapper function for rte_eth_tx_burst(). */
uint16_t
spp_eth_tx_burst(
		uint16_t port_id, uint16_t queue_id  __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	nb_tx = port_ability_each_operation(port_id, tx_pkts, nb_pkts,
			SPPWK_PORT_DIR_TX);
	if (unlikely(nb_tx == 0))
		return SPP_RET_OK;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	if (g_port_mng_info[port_id].iface_type == RING)
		spp_ringlatencystats_add_time_stamp(
				g_port_mng_info[port_id].iface_no,
				tx_pkts, nb_pkts);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	return rte_eth_tx_burst(port_id, 0, tx_pkts, nb_tx);
}
