/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "eth.h"

/* Define item "eth" operations */
struct flow_detail_ops eth_ops_list[] = {
	{
		.token = "src",
		.offset = offsetof(struct rte_flow_item_eth, src),
		.size = sizeof(struct rte_ether_addr),
		.flg_value = 1,
		.parse_detail = str_to_rte_ether_addr,
	},
	{
		.token = "dst",
		.offset = offsetof(struct rte_flow_item_eth, dst),
		.size = sizeof(struct rte_ether_addr),
		.flg_value = 1,
		.parse_detail = str_to_rte_ether_addr,
	},
	{
		.token = "type",
		.offset = offsetof(struct rte_flow_item_eth, type),
		.size = sizeof(rte_be16_t),
		.flg_value = 1,
		.parse_detail = str_to_rte_be16_t,
	},
	{
		.token = NULL,
	},
};

int
append_item_eth_json(const void *element, int buf_size, char *pattern_str)
{
	const struct rte_flow_item_eth *eth = element;
	char dst_mac[RTE_ETHER_ADDR_FMT_SIZE] = { 0 };
	char src_mac[RTE_ETHER_ADDR_FMT_SIZE] = { 0 };
	char tmp_str[128] = { 0 };

	rte_ether_format_addr(dst_mac, RTE_ETHER_ADDR_FMT_SIZE, &eth->dst);
	rte_ether_format_addr(src_mac, RTE_ETHER_ADDR_FMT_SIZE, &eth->src);

	snprintf(tmp_str, 128,
		"{\"dst\":\"%s\","
		"\"src\":\"%s\","
		"\"type\":\"0x%04x\"}",
		dst_mac, src_mac, eth->type);

	if ((int)strlen(pattern_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(pattern_str, tmp_str, strlen(tmp_str));

	return 0;
}
