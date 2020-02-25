/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>

#include "primary/flow/flow.h"
#include "primary/flow/common.h"
#include "queue.h"

/* Define action "queue" operations */
struct flow_detail_ops queue_ops_list[] = {
	{
		.token = "index",
		.offset = offsetof(struct rte_flow_action_queue, index),
		.size = sizeof(uint16_t),
		.flg_value = 1,
		.parse_detail = str_to_uint16_t,
	},
	{
		.token = NULL,
	},
};

int
append_action_queue_json(const void *conf, int buf_size, char *action_str)
{
	const struct rte_flow_action_queue *queue = conf;
	char tmp_str[64] = { 0 };

	snprintf(tmp_str, 64,
		"{\"index\":%d}",
		queue->index);

	if ((int)strlen(action_str) + (int)strlen(tmp_str)
		> buf_size)
		return -1;

	strncat(action_str, tmp_str, strlen(tmp_str));

	return 0;
}
