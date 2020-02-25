/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "jump.h"

/* Define action "jump" operations */
struct flow_detail_ops jump_ops_list[] = {
	{
		.token = "group",
		.offset = offsetof(struct rte_flow_action_jump, group),
		.size = sizeof(uint32_t),
		.flg_value = 1,
		.parse_detail = str_to_uint32_t,
	},
	{
		.token = NULL,
	},
};

int
append_action_jump_json(const void *conf, int buf_size, char *action_str)
{
	const struct rte_flow_action_jump *jump = conf;
	char tmp_str[64] = { 0 };

	snprintf(tmp_str, 64,
		"{\"group\":%d}",
		jump->group);

	if ((int)strlen(action_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(action_str, tmp_str, strlen(tmp_str));

	return 0;
}
