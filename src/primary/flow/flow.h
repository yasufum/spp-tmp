/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_H_
#define _PRIMARY_FLOW_H_

#include <rte_log.h>

#define RTE_LOGTYPE_SPP_FLOW RTE_LOGTYPE_USER1

enum flow_command {
	VALIDATE = 0,
	CREATE,
	DESTROY,
	FLUSH
};

/* Parser result of flow command arguments */
struct flow_args {
	enum flow_command command;
	int port_id;
	union {
		struct {
			struct rte_flow_attr attr;
			struct rte_flow_item *pattern;
			struct rte_flow_action *actions;
		} rule; /* validate or create arguments. */
		struct {
			uint32_t rule_id;
		} destroy; /* destroy arguments. */
	} args;
};

/* Descriptor for a single flow. */
struct flow_rule {
	/* Flow rule ID */
	uint32_t rule_id;

	/* Previous flow in list. */
	struct flow_rule *prev;

	/* Opaque flow object returned by PMD. */
	struct rte_flow *flow_handle;

	/* Saved flow rule description. */
	struct rte_flow_conv_rule rule;
};

/* Flow rule list of the port */
struct port_flow {
	/* Associated flows */
	struct flow_rule *flow_list;
};

/* Detail parse operation for a specific item or action */
struct flow_detail_ops {
	const char *token;
	const size_t offset;
	const size_t size;
	int flg_value;
	int (*parse_detail)(char *str, void *output);
};

/* Operation for each item type */
struct flow_item_ops {
	const char *str_type;
	enum rte_flow_item_type type;
	size_t size;
	int (*parse)(char *token_list[], int *index,
		struct rte_flow_item *pattern,
		struct flow_item_ops *ops);
	struct flow_detail_ops *detail_list;
	int (*status)(const void *element,
		int buf_size, char *pattern_str);
};

/* Operation for each action type */
struct flow_action_ops {
	const char *str_type;
	enum rte_flow_action_type type;
	size_t size;
	int (*parse)(char *token_list[], int *index,
		struct rte_flow_action *action,
		struct flow_action_ops *ops);
	struct flow_detail_ops *detail_list;
	int (*status)(const void *conf,
		int buf_size, char *action_str);
};

int parse_flow(char *token_list[], char *response);
int append_flow_json(int port_id, int buf_size, char *output);

#endif
