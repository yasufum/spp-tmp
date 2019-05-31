/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_WORKER_TH_MIRROR_DEPS_H__
#define __SPP_WORKER_TH_MIRROR_DEPS_H__

#include "cmd_utils.h"

/**
 * Update Mirror info
 *
 * @param component
 *  The pointer to struct sppwk_comp_info.@n
 *  The data for updating the internal data of mirror.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_mirror_update(struct sppwk_comp_info *component);

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

#endif  /* __SPP_WORKER_TH_MIRROR_DEPS_H__ */
