/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _CLASSIFIER_MAC_H_
#define _CLASSIFIER_MAC_H_

#include "shared/secondary/spp_worker_th/cmd_utils.h"

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
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
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
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int spp_classifier_mac_do(int id);

/**
 * classifier(mac address) iterate classifier table.
 *
 * @param params
 *  Point to struct spp_iterate_classifier_table_params.@n
 *  Detailed data of classifier table.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int add_classifier_table_val(
		struct spp_iterate_classifier_table_params *params);

/**
 * Get classifier status.
 *
 * @param[in] lcore_id Lcore ID for classifier.
 * @param[in] id Unique component ID.
 * @param[in,out] params Pointer to detailed data of classifier status.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
/**
 * TODO(yasufum) Consider to move this function to `vf_cmd_runner.c`.
 * This function is called only from `vf_cmd_runner.c`, but
 * must be defined in `classifier_mac.c` because it refers g_mng_info defined
 * in this file. It is bad dependency for the global variable.
 */
int get_classifier_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif /* _CLASSIFIER_MAC_H_ */
