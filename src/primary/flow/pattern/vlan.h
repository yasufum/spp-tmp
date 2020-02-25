/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_PATTERN_VLAN_H_
#define _PRIMARY_FLOW_PATTERN_VLAN_H_

extern struct flow_detail_ops vlan_ops_list[];

int append_item_vlan_json(const void *element, int buf_size,
	char *pattern_str);

#endif
