/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_PATTERN_ETH_H_
#define _PRIMARY_FLOW_PATTERN_ETH_H_

extern struct flow_detail_ops eth_ops_list[];

int append_item_eth_json(const void *element, int buf_size,
	char *pattern_str);

#endif
