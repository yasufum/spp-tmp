/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_MIRROR_H__
#define __SPP_MIRROR_H__

/**
 * @file
 * SPP_MIRROR main
 *
 * Main function of spp_mirror.
 * This provides the function for initializing and starting the threads.
 *
 * There is two kinds of reproduction classification. I choose it by a
 * compilation switch.
 *  -DeepCopy
 *  -ShallowCopy
 *
 * Attention
 *  I do not do the deletion of the VLAN tag, the addition.
 */

/**
 * Update Mirror info
 *
 * @param component
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of mirror.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_mirror_update(struct spp_component_info *component);

/**
 * Mirror get component status
 *
 * @param lcore_id
 *  The logical core ID for forwarder and merger.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of mirror status.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_mirror_get_component_status(
		unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif /* __SPP_MIRROR_H__ */
