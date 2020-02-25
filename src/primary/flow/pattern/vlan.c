/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "vlan.h"

/* Define item "vlan" operations */
struct flow_detail_ops vlan_ops_list[] = {
	{
		.token = "tci",
		.offset = offsetof(struct rte_flow_item_vlan, tci),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = str_to_tci,
	},
	{
		.token = "pcp",
		.offset = offsetof(struct rte_flow_item_vlan, tci),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = set_pcp_in_tci,
	},
	{
		.token = "dei",
		.offset = offsetof(struct rte_flow_item_vlan, tci),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = set_dei_in_tci,
	},
	{
		.token = "vid",
		.offset = offsetof(struct rte_flow_item_vlan, tci),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = set_vid_in_tci,
	},
	{
		.token = "inner_type",
		.offset = offsetof(struct rte_flow_item_vlan, inner_type),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = str_to_rte_be16_t,
	},
	{
		.token = NULL,
	},
};

int
append_item_vlan_json(const void *element, int buf_size, char *pattern_str)
{
	const struct rte_flow_item_vlan *vlan = element;
	char tmp_str[128] = { 0 };

	snprintf(tmp_str, 128,
		"{\"tci\":\"0x%04x\","
		"\"inner_type\":\"0x%04x\"}",
		vlan->tci, vlan->inner_type);

	if ((int)strlen(pattern_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(pattern_str, tmp_str, strlen(tmp_str));

	return 0;
}
