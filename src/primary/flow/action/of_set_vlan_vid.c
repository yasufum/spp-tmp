/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "of_set_vlan_vid.h"

/* Define action "of_set_vlan_vid" operations */
struct flow_detail_ops of_set_vlan_vid_ops_list[] = {
	{
		.token = "vlan_vid",
		.offset = offsetof(struct rte_flow_action_of_set_vlan_vid,
			vlan_vid),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = str_to_rte_be16_t,
	},
	{
		.token = NULL,
	},
};

int
append_action_of_set_vlan_vid_json(const void *conf, int buf_size,
	char *action_str)
{
	const struct rte_flow_action_of_set_vlan_vid *vid = conf;
	char tmp_str[64] = { 0 };

	snprintf(tmp_str, 64,
		"{\"vlan_vid\":\"0x%04x\"}",
		vid->vlan_vid);

	if ((int)strlen(action_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(action_str, tmp_str, strlen(tmp_str));

	return 0;
}
