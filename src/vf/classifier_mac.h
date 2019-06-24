/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _CLASSIFIER_MAC_H_
#define _CLASSIFIER_MAC_H_

/**
 * @file
 * SPP Classifier
 *
 * Classifier component provides packet forwarding function from
 * one port to one port. Classifier has table of virtual MAC address.
 * According to this table, classifier lookups L2 destination MAC address
 * and determines which port to be transferred to incoming packets.
 */

struct spp_iterate_classifier_table_params;

/**
 * classifier(mac address) initialize globals.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_classifier_mac_init(void);

/**
 * initialize classifier information.
 *
 * @param component_id
 *  The unique component ID.
 *
 */
void init_classifier_info(int component_id);


/**
 * classifier(mac address) thread function.
 *
 * @param id
 *  The unique component ID.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_classifier_mac_do(int id);

/**
 * classifier(mac address) iterate classifier table.
 *
 * @param params
 *  Point to struct spp_iterate_classifier_table_params.@n
 *  Detailed data of classifier table.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int add_classifier_table_val(
		struct spp_iterate_classifier_table_params *params);

#endif /* _CLASSIFIER_MAC_H_ */
