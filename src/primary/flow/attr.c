/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "attr.h"

int
parse_flow_attr(char *token_list[], int *index,
	struct rte_flow_attr *attr)
{
	int ret;
	char *token;
	char *end;
	unsigned long temp = 0;

	while (token_list[*index] != NULL) {
		token = token_list[*index];

		if (!strcmp(token, "group")) {
			/* "group" requires option argument */
			if (token_list[*index + 1] == NULL) {
				ret = -1;
				break;
			}

			temp = strtoul(token_list[*index + 1], &end, 10);
			if (end == NULL || *end != '\0') {
				ret = -1;
				break;
			}

			attr->group = (uint32_t)temp;
			(*index)++;

		} else if (!strcmp(token, "priority")) {
			/* "priority" requires option argument */
			if (token_list[*index + 1] == NULL) {
				ret = -1;
				break;
			}

			temp = strtoul(token_list[*index + 1], &end, 10);
			if (end == NULL || *end != '\0') {
				ret = -1;
				break;
			}

			attr->priority = (uint32_t)temp;
			(*index)++;

		} else if (!strcmp(token, "ingress")) {
			attr->ingress = 1;

		} else if (!strcmp(token, "egress")) {
			attr->egress = 1;

		} else if (!strcmp(token, "transfer")) {
			attr->transfer = 1;

		} else if (!strcmp(token, "pattern")) {
			/* Attribute parameter end */
			ret = 0;
			break;

		} else {
			/* Illegal parameter */
			ret = -1;
			break;

		}

		(*index)++;
	}

	if (token_list[*index] == NULL)
		ret = -1;

	return ret;
}

int
append_flow_attr_json(const struct rte_flow_attr *attr, int buf_size,
	char *attr_str)
{
	char tmp_str[128] = { 0 };

	snprintf(tmp_str, 128,
		"{\"group\":%d,"
		"\"priority\":%d,"
		"\"ingress\":%d,"
		"\"egress\":%d,"
		"\"transfer\":%d}",
		attr->group, attr->priority, attr->ingress,
		attr->egress, attr->transfer);

	if ((int)strlen(attr_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(attr_str, tmp_str, strlen(tmp_str));

	return 0;
}
