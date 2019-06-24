/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_WORKER_TH_MIRROR_DEPS_H__
#define __SPP_WORKER_TH_MIRROR_DEPS_H__

#include "cmd_utils.h"

/**
 * Update mirror info.
 *
 * @param wk_comp_info Pointer to internal data of mirror.
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
int update_mirror(struct sppwk_comp_info *wk_comp_info);

/**
 * Get mirror status.
 *
 * @param lcore_id Lcore ID for forwarder and merger.
 * @param id Unique component ID.
 * @param params Pointer to detailed data of mirror status.
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
int get_mirror_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif  /* __SPP_WORKER_TH_MIRROR_DEPS_H__ */
