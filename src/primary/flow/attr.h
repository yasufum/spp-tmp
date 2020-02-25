/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_ATTR_H_
#define _PRIMARY_FLOW_ATTR_H_

int parse_flow_attr(char *token_list[], int *index,
	struct rte_flow_attr *attr);
int append_flow_attr_json(const struct rte_flow_attr *attr,
	int buf_size, char *attr_str);

#endif
