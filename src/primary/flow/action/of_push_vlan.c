/* SPDX-License-Identifier: BSD-3-Claus1
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "of_push_vlan.h"

/* Define action "of_push_vlan" operations */
struct flow_detail_ops of_push_vlan_ops_list[] = {
	{
		.token = "ethertype",
		.offset = offsetof(struct rte_flow_action_of_push_vlan,
			ethertype),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = str_to_rte_be16_t,
	},
	{
		.token = NULL,
	},
};

int
append_action_of_push_vlan_json(const void *conf, int buf_size,
	char *action_str)
{
	const struct rte_flow_action_of_push_vlan *of_push_vlan = conf;
	char tmp_str[64] = { 0 };

	snprintf(tmp_str, 64,
		"{\"ethertype\":\"0x%04x\"}",
		of_push_vlan->ethertype);

	if ((int)strlen(action_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(action_str, tmp_str, strlen(tmp_str));

	return 0;
}
