/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_WORKER_TH_VF_DEPS_H__
#define __SPP_WORKER_TH_VF_DEPS_H__

#include <rte_malloc.h>
#include <rte_hash.h>
#include "spp_proc.h"

/* number of classifier information (reference/update) */
#define NUM_CLASSIFIER_MAC_INFO 2

/* mac address classification */
struct mac_classification {
	/* hash table keeps classification */
	struct rte_hash *classification_tab;

	/* number of valid classification */
	int num_active_classified;

	/* index of valid classification */
	int active_classifieds[RTE_MAX_ETHPORTS];

	/* index of default classification */
	int default_classified;
};

/* classified data (destination port, target packets, etc) */
struct classified_data {
	/* interface type (see "enum port_type") */
	enum port_type  iface_type;

	/* index of ports handled by classifier */
	int             iface_no;

	/* id for interface generated by spp_vf */
	int             iface_no_global;

	/* port id generated by DPDK */
	uint16_t        port;

	/* the number of packets in pkts[] */
	uint16_t        num_pkt;

	/* packet array to be classified */
	struct rte_mbuf *pkts[MAX_PKT_BURST];
};

/* classifier component information */
struct component_info {
	/* component name */
	char name[SPP_NAME_STR_LEN];

	/* mac address entry flag */
	int mac_addr_entry;

	/* mac address classification per vlan-id */
	struct mac_classification *mac_classifications[SPP_NUM_VLAN_VID];

	/* number of transmission ports */
	int n_classified_data_tx;

	/* receive port handled by classifier */
	struct classified_data classified_data_rx;

	/* transmission ports handled by classifier */
	struct classified_data classified_data_tx[RTE_MAX_ETHPORTS];
};

/* free mac classification instance. */
static inline void
free_mac_classification(struct mac_classification *mac_cls)
{
	if (mac_cls == NULL)
		return;

	if (mac_cls->classification_tab != NULL)
		rte_hash_free(mac_cls->classification_tab);

	rte_free(mac_cls);
}

/**
 * classifier(mac address) update component info.
 *
 * @param component_info
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of classifier.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_classifier_mac_update(struct spp_component_info *component_info);

/**
 * Update forward info
 *
 * @param component
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of forwarder and merger.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_forward_update(struct spp_component_info *component);

void init_classifier_info(int component_id);

void uninit_component_info(struct component_info *cmp_info);

int spp_classifier_mac_iterate_table(
		struct spp_iterate_classifier_table_params *params);

/**
 * classifier get component status.
 *
 *
 * @param lcore_id
 *  The logical core ID for classifier.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of classifier status.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_classifier_get_component_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

/**
 * Merge/Forward get component status
 *
 * @param lcore_id
 *  The logical core ID for forwarder and merger.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of forwarder/merger status.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_forward_get_component_status(
		unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif  /* __SPP_WORKER_TH_VF_DEPS_H__ */
