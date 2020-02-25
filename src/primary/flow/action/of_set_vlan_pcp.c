/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "of_set_vlan_pcp.h"

/* Define action "of_set_vlan_pcp" operations */
struct flow_detail_ops of_set_vlan_pcp_ops_list[] = {
	{
		.token = "vlan_pcp",
		.offset = offsetof(struct rte_flow_action_of_set_vlan_pcp,
			vlan_pcp),
		.size = sizeof(uint8_t),
		.flg_value = 1,
		.parse_detail = str_to_pcp,
	},
	{
		.token = NULL,
	},
};

int
append_action_of_set_vlan_pcp_json(const void *conf, int buf_size,
	char *action_str)
{
	const struct rte_flow_action_of_set_vlan_pcp *pcp = conf;
	char tmp_str[64] = { 0 };

	snprintf(tmp_str, 64,
		"{\"vlan_pcp\":\"0x%01x\"}",
		pcp->vlan_pcp);

	if ((int)strlen(action_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(action_str, tmp_str, strlen(tmp_str));

	return 0;
}
