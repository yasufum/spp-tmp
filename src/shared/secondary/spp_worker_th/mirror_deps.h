/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_WORKER_TH_MIRROR_DEPS_H__
#define __SPP_WORKER_TH_MIRROR_DEPS_H__

#include "cmd_utils.h"
#include "cmd_parser.h"

int exec_one_cmd(const struct sppwk_cmd_attrs *cmd);

int add_core(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

/**
 * Update mirror info.
 *
 * @param wk_comp_info Pointer to internal data of mirror.
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
int update_mirror(struct sppwk_comp_info *wk_comp_info);

#endif  /* __SPP_WORKER_TH_MIRROR_DEPS_H__ */
