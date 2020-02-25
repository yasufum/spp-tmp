/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_ACTION_JUMP_H_
#define _PRIMARY_FLOW_ACTION_JUMP_H_

extern struct flow_detail_ops jump_ops_list[];

int append_action_jump_json(const void *conf, int buf_size, char *action_str);

#endif
