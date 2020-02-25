/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_flow.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_byteorder.h>

#include "shared/common.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/spp_worker_th/data_types.h"
#include "primary/primary.h"
#include "flow.h"
#include "attr.h"
#include "common.h"

#include "primary/flow/pattern/eth.h"
#include "primary/flow/pattern/vlan.h"

#include "primary/flow/action/jump.h"
#include "primary/flow/action/queue.h"
#include "primary/flow/action/of_push_vlan.h"
#include "primary/flow/action/of_set_vlan_vid.h"
#include "primary/flow/action/of_set_vlan_pcp.h"


/* Flow list for each port */
static struct port_flow port_list[RTE_MAX_ETHPORTS] = { 0 };

/* Define item operations */
static struct flow_item_ops flow_item_ops_list[] = {
	{
		.str_type = "end",
		.type = RTE_FLOW_ITEM_TYPE_END,
		.parse = NULL,
		.detail_list = NULL
	},
	{
		.str_type = "eth",
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.size = sizeof(struct rte_flow_item_eth),
		.parse = parse_item_common,
		.detail_list = eth_ops_list,
		.status = append_item_eth_json,
	},
	{
		.str_type = "vlan",
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.size = sizeof(struct rte_flow_item_vlan),
		.parse = parse_item_common,
		.detail_list = vlan_ops_list,
		.status = append_item_vlan_json,
	},
};

/* Define action operations */
static struct flow_action_ops flow_action_ops_list[] = {
	{
		.str_type = "end",
		.type = RTE_FLOW_ACTION_TYPE_END,
		.size = 0,
		.parse = NULL,
		.detail_list = NULL,
		.status = NULL,
	},
	{
		.str_type = "jump",
		.type = RTE_FLOW_ACTION_TYPE_JUMP,
		.size = sizeof(struct rte_flow_action_jump),
		.parse = parse_action_common,
		.detail_list = jump_ops_list,
		.status = append_action_jump_json,
	},
	{
		.str_type = "queue",
		.type = RTE_FLOW_ACTION_TYPE_QUEUE,
		.size = sizeof(struct rte_flow_action_queue),
		.parse = parse_action_common,
		.detail_list = queue_ops_list,
		.status = append_action_queue_json,
	},
	{
		.str_type = "of_pop_vlan",
		.type = RTE_FLOW_ACTION_TYPE_OF_POP_VLAN,
		.size = 0,
		.parse = NULL,
		.detail_list = NULL,
		.status = append_action_null_json,
	},
	{
		.str_type = "of_push_vlan",
		.type = RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,
		.size = sizeof(struct rte_flow_action_of_push_vlan),
		.parse = parse_action_common,
		.detail_list = of_push_vlan_ops_list,
		.status = append_action_of_push_vlan_json,
	},
	{
		.str_type = "of_set_vlan_vid",
		.type = RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,
		.size = sizeof(struct rte_flow_action_of_set_vlan_vid),
		.parse = parse_action_common,
		.detail_list = of_set_vlan_vid_ops_list,
		.status = append_action_of_set_vlan_vid_json,
	},
	{
		.str_type = "of_set_vlan_pcp",
		.type = RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP,
		.size = sizeof(struct rte_flow_action_of_set_vlan_pcp),
		.parse = parse_action_common,
		.detail_list = of_set_vlan_pcp_ops_list,
		.status = append_action_of_set_vlan_pcp_json,
	},
};

/* Free memory of "flow_args". */
static void
free_flow_args(struct flow_args *input)
{
	int i;
	struct rte_flow_item *pattern;
	struct rte_flow_action *actions;
	char **target;

	if ((input->command != VALIDATE) &&
		(input->command != CREATE))
		return;

	pattern = input->args.rule.pattern;
	if (pattern != NULL) {
		for (i = 0; pattern[i].type != RTE_FLOW_ITEM_TYPE_END; i++) {
			target = (char **)((char *)(&pattern[i]) +
				offsetof(struct rte_flow_item, spec));
			if (*target != NULL)
				free(*target);

			target = (char **)((char *)(&pattern[i]) +
				offsetof(struct rte_flow_item, last));
			if (*target != NULL)
				free(*target);

			target = (char **)((char *)(&pattern[i]) +
				offsetof(struct rte_flow_item, mask));
			if (*target != NULL)
				free(*target);
		}

		free(pattern);
	}

	actions = input->args.rule.actions;
	if (actions != NULL) {
		for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
			target = (char **)((char *)(&actions[i]) +
				offsetof(struct rte_flow_action, conf));
			if (*target != NULL)
				free(*target);
		}

		free(actions);
	}
}

/*
 * Create response in JSON format.
 * `rule_id` must be empty if flow create is failed.
 */
static void
make_response(char *response, const char *result, const char *message,
	char *rule_id)
{
	if (rule_id == NULL)
		snprintf(response, MSG_SIZE,
			"{\"result\": \"%s\", \"message\": \"%s\"}",
			result, message);
	else
		snprintf(response, MSG_SIZE,
			"{\"result\": \"%s\", \"message\": \"%s\", "
			"\"rule_id\": \"%s\"}",
			result, message, rule_id);
}

/* Create error response from rte_flow_error */
static void
make_error_response(char *response, const char *message,
	struct rte_flow_error error, char *rule_id)
{
	/* Define description for each error type */
	static const char *const errstr_list[] = {
		[RTE_FLOW_ERROR_TYPE_NONE] = "No error",
		[RTE_FLOW_ERROR_TYPE_UNSPECIFIED] = "Cause unspecified",
		[RTE_FLOW_ERROR_TYPE_HANDLE] = "Flow rule (handle)",
		[RTE_FLOW_ERROR_TYPE_ATTR_GROUP] = "Group field",
		[RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY] = "Priority field",
		[RTE_FLOW_ERROR_TYPE_ATTR_INGRESS] = "Ingress field",
		[RTE_FLOW_ERROR_TYPE_ATTR_EGRESS] = "Egress field",
		[RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER] = "Transfer field",
		[RTE_FLOW_ERROR_TYPE_ATTR] = "Attributes structure",
		[RTE_FLOW_ERROR_TYPE_ITEM_NUM] = "Pattern length",
		[RTE_FLOW_ERROR_TYPE_ITEM_SPEC] = "Item specification",
		[RTE_FLOW_ERROR_TYPE_ITEM_LAST] = "Item specification range",
		[RTE_FLOW_ERROR_TYPE_ITEM_MASK] = "Item specification mask",
		[RTE_FLOW_ERROR_TYPE_ITEM] = "Specific pattern item",
		[RTE_FLOW_ERROR_TYPE_ACTION_NUM] = "Number of actions",
		[RTE_FLOW_ERROR_TYPE_ACTION_CONF] = "Action configuration",
		[RTE_FLOW_ERROR_TYPE_ACTION] = "Specific action",
	};
	int err = rte_errno;
	char msg[512] = "";
	char cause[32] = "";
	const char *errstr;

	if ((unsigned int)error.type >= RTE_DIM(errstr_list) ||
	    !errstr_list[error.type])
		errstr = "Unknown type";
	else
		errstr = errstr_list[error.type];


	if (error.cause != NULL)
		snprintf(cause, sizeof(cause), "cause: %p\\n", error.cause);

	snprintf(msg, sizeof(msg),
		"%s\\nerror type: %d (%s)\\n"
		"%serror message: %s\\nrte_errno: %s",
		message, error.type, errstr, cause,
		error.message ? error.message : "(no stated reason)",
		rte_strerror(err));
	make_response(response, "error", msg, rule_id);
}

/* Add to array, redeclare memory. */
static int
append_object_list(void **list, void *add, size_t obj_size, int num)
{
	char *new_list;

	new_list = malloc(obj_size * num);
	if (new_list == NULL)
		return -1;

	/* Copy original list*/
	if (*list != NULL) {
		memcpy(new_list, *list, obj_size * (num - 1));
		free(*list);
	}

	/* Add to list */
	memcpy(new_list + (obj_size * (num - 1)), add, obj_size);

	*list = (void *)new_list;
	return 0;
}

static int
parse_flow_actions(char *token_list[], int *index,
	struct rte_flow_action **actions)
{
	int ret;
	int action_count = 0;
	uint16_t i;
	char *token;
	struct flow_action_ops *ops;
	struct rte_flow_action action;

	if (strcmp(token_list[*index], "actions")) {
		RTE_LOG(ERR, SPP_FLOW,
			"Invalid parameter is %s(%s:%d)\n",
			token_list[*index], __func__, __LINE__);
		return -1;
	}

	/* Next to word */
	(*index)++;

	while (token_list[*index] != NULL) {
		token = token_list[*index];

		for (i = 0; i < RTE_DIM(flow_action_ops_list); i++) {
			ops = &flow_action_ops_list[i];
			if (strcmp(token, ops->str_type))
				continue;

			memset(&action, 0, sizeof(struct rte_flow_action));
			action.type = ops->type;
			if (ops->parse != NULL) {
				ret = ops->parse(token_list, index, &action,
					ops);
				if (ret < 0)
					return -1;
			} else {
				(*index)++;
			}
			break;
		}

		/*
		 * Error occurs if a action string that is not defined in
		 * str_type of flow_action_ops_list is specified
		 */
		if (i == RTE_DIM(flow_action_ops_list)) {
			RTE_LOG(ERR, SPP_FLOW,
				"Invalid parameter "
				"is %s action(%s:%d)\n",
				token, __func__, __LINE__);
			return -1;
		}

		/* Add to "actions" list */
		action_count++;
		ret = append_object_list((void **)actions, &action,
			sizeof(struct rte_flow_action), action_count);

		if (!strcmp(token, "end"))
			break;

		(*index)++;
	}

	return 0;
}

static int
parse_flow_pattern(char *token_list[], int *index,
	struct rte_flow_item **pattern)
{
	int ret;
	int item_count = 0;
	uint32_t i;
	char *token;
	struct flow_item_ops *ops;
	struct rte_flow_item item;

	while (token_list[*index] != NULL) {
		token = token_list[*index];

		for (i = 0; i < RTE_DIM(flow_item_ops_list); i++) {
			ops = &flow_item_ops_list[i];
			if (strcmp(token, ops->str_type))
				continue;

			memset(&item, 0, sizeof(struct rte_flow_item));
			item.type = ops->type;
			if (ops->parse != NULL) {
				ret = ops->parse(token_list, index, &item,
					ops);
				if (ret < 0)
					return -1;
			}
			break;
		}

		/*
		 * Error occurs if a pattern string that is not defined in
		 * str_type of flow_item_ops_list is specified
		 */
		if (i == RTE_DIM(flow_item_ops_list)) {
			RTE_LOG(ERR, SPP_FLOW,
				"Invalid parameter "
				"is %s pattern(%s:%d)\n",
				token, __func__, __LINE__);
			return -1;
		}

		/* Add to "pattern" list */
		item_count++;
		ret = append_object_list((void **)pattern, &item,
			sizeof(struct rte_flow_item), item_count);

		if (!strcmp(token, "end"))
			break;

		(*index)++;
	}

	return 0;
}

static int
parse_flow_rule(char *token_list[], struct flow_args *input)
{
	int ret = 0;
	int index;

	ret = parse_phy_port_id(token_list[2], &input->port_id);
	if (ret < 0)
		return -1;

	/* The next index of the port */
	index = 3;

	/* Attribute parse */
	ret = parse_flow_attr(token_list, &index, &input->args.rule.attr);
	if (ret < 0) {
		RTE_LOG(ERR, SPP_FLOW,
			"Failed to parse Attribute(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	/* The next index of the pattern */
	index++;

	/* Pattern parse */
	ret = parse_flow_pattern(token_list, &index,
		&input->args.rule.pattern);
	if (ret < 0) {
		RTE_LOG(ERR, SPP_FLOW,
			"Failed to parse Pattern(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	/* The next index of the actions */
	index++;

	/* Actions parse */
	ret = parse_flow_actions(token_list, &index,
		&input->args.rule.actions);
	if (ret < 0) {
		RTE_LOG(ERR, SPP_FLOW,
			"Failed to parse Actions(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	return 0;
}

static int
parse_flow_destroy(char *token_list[], struct flow_args *input)
{
	int ret;
	char *end;

	ret = parse_phy_port_id(token_list[2], &input->port_id);
	if (ret < 0)
		return -1;

	if (token_list[3] == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"rule_id is not specified(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	if (!strcmp(token_list[3], "ALL")) {
		input->command = FLUSH;

	} else {
		input->command = DESTROY;
		input->args.destroy.rule_id = strtoul(token_list[3],
			&end, 10);
	}

	return 0;
}

/** Generate a flow_rule entry from attributes/pattern/actions. */
static struct flow_rule *
create_flow_rule(struct rte_flow_attr *attr,
	struct rte_flow_item *pattern,
	struct rte_flow_action *actions,
	struct rte_flow_error *error)
{
	const struct rte_flow_conv_rule conv_rule = {
		.attr_ro = attr,
		.pattern_ro = pattern,
		.actions_ro = actions,
	};
	struct flow_rule *rule;
	int ret;

	ret = rte_flow_conv(RTE_FLOW_CONV_OP_RULE, NULL, 0, &conv_rule,
		error);
	if (ret < 0)
		return NULL;

	rule = calloc(1, offsetof(struct flow_rule, rule) + ret);
	if (!rule) {
		rte_flow_error_set
			(error, errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "calloc() failed");
		return NULL;
	}

	ret = rte_flow_conv(RTE_FLOW_CONV_OP_RULE, &rule->rule, ret, &conv_rule,
			  error);
	if (ret >= 0)
		return rule;

	free(rule);
	return NULL;
}

/* Execute rte_flow_validate().*/
static void
exec_flow_validate(int port_id,
	struct rte_flow_attr *attr,
	struct rte_flow_item *pattern,
	struct rte_flow_action *actions,
	char *response)
{
	int ret;
	struct rte_flow_error error;

	memset(&error, 0, sizeof(error));

	ret = rte_flow_validate(port_id, attr, pattern, actions, &error);
	if (ret != 0)
		make_error_response(response, "Flow validate error", error,
			NULL);
	else
		make_response(response, "success", "Flow rule validated",
			NULL);
}

/* Execute rte_flow_create(). Save flow rules globally */
static void
exec_flow_create(int port_id,
	struct rte_flow_attr *attr,
	struct rte_flow_item *pattern,
	struct rte_flow_action *actions,
	char *response)
{
	uint32_t rule_id;
	char mes[32];
	char rule_id_str[11] = {0};
	struct rte_flow_error error;
	struct rte_flow *flow;
	struct flow_rule *rule;
	struct port_flow *port;

	memset(&error, 0, sizeof(error));

	flow = rte_flow_create(port_id, attr, pattern, actions, &error);
	if (flow == NULL) {
		make_error_response(response, "Flow create error", error,
			rule_id_str);
		return;
	}

	port = &port_list[port_id];
	if (port->flow_list != NULL) {
		if (port->flow_list->rule_id >= UINT32_MAX) {
			make_response(response, "error",
				"Rule ID must be less than %"PRIu32,
				rule_id_str);
			rte_flow_destroy(port_id, flow, NULL);
			return;
		}
		rule_id = port->flow_list->rule_id + 1;
	} else {
		rule_id = 0;
	}

	rule = create_flow_rule(attr, pattern, actions, &error);
	if (rule == NULL) {
		rte_flow_destroy(port_id, flow, NULL);
		make_error_response(response, "Flow create error", error,
			rule_id_str);
		return;
	}

	/* Keep it globally as a list */
	rule->rule_id = rule_id;
	rule->flow_handle = flow;

	if (port->flow_list == NULL)
		rule->prev = NULL;
	else
		rule->prev = port->flow_list;

	port->flow_list = rule;

	sprintf(mes, "Flow rule #%d created", rule_id);
	sprintf(rule_id_str, "%d", rule_id);
	make_response(response, "success", mes, rule_id_str);
}

/* Execute rte_flow_destroy(). Destroying a globally saved flow rule */
static void
exec_flow_destroy(int port_id, uint32_t rule_id, char *response)
{
	int ret;
	int found_flg = 0;
	char mes[64];
	struct flow_rule *rule, **next_ptr;
	struct rte_flow_error error;

	memset(&error, 0, sizeof(error));

	ret = is_portid_used(port_id);
	if (ret != 0) {
		sprintf(mes, "Invalid port %d", port_id);
		make_response(response, "error", mes, NULL);
		return;
	}

	next_ptr = &(port_list[port_id].flow_list);
	rule = port_list[port_id].flow_list;

	while (rule != NULL) {
		if (rule->rule_id != rule_id) {
			next_ptr = &(rule->prev);
			rule = rule->prev;
			continue;
		}

		ret = rte_flow_destroy(port_id, rule->flow_handle, &error);
		if (ret != 0) {
			make_error_response(response, "Flow destroy error",
				error, NULL);
			return;
		}

		/* Remove flow from global list */
		*next_ptr = rule->prev;
		free(rule);
		found_flg = 1;

		sprintf(mes, "Flow rule #%d destroyed", rule_id);
		make_response(response, "success", mes, NULL);
		break;
	}

	/* Rule_id not found */
	if (found_flg == 0) {
		sprintf(mes, "Flow rule #%d not found", rule_id);
		make_response(response, "error", mes, NULL);
	}
}

/* Delete all globally saved flow rules */
static void
exec_flow_flush(int port_id, char *response)
{
	int ret;
	char mes[64];
	struct flow_rule *rule;
	struct rte_flow_error error;

	memset(&error, 0, sizeof(error));

	ret = is_portid_used(port_id);
	if (ret != 0) {
		sprintf(mes, "Invalid port %d", port_id);
		make_response(response, "error", mes, NULL);
		return;
	}

	ret = rte_flow_flush(port_id, &error);
	if (ret != 0)
		make_error_response(response, "Flow destroy error",
			error, NULL);
	else
		make_response(response, "success", "Flow rule all destroyed",
			NULL);

	/*
	 * Even if a failure occurs, flow handle is invalidated,
	 * so delete flow_list.
	 */

	while (port_list[port_id].flow_list != NULL) {
		rule = port_list[port_id].flow_list->prev;
		free(port_list[port_id].flow_list);
		port_list[port_id].flow_list = rule;
	}
}

static void
exec_flow(struct flow_args *input, char *response)
{
	switch (input->command) {
	case VALIDATE:
		exec_flow_validate(input->port_id,
			&input->args.rule.attr,
			input->args.rule.pattern,
			input->args.rule.actions,
			response);
		break;
	case CREATE:
		exec_flow_create(input->port_id,
			&input->args.rule.attr,
			input->args.rule.pattern,
			input->args.rule.actions,
			response);
		break;
	case DESTROY:
		exec_flow_destroy(input->port_id,
			input->args.destroy.rule_id,
			response);
		break;
	case FLUSH:
		exec_flow_flush(input->port_id, response);
		break;
	}

	/* Argument data is no longer needed and freed */
	free_flow_args(input);
}

int
parse_flow(char *token_list[], char *response)
{
	int ret = 0;
	struct flow_args input = { 0 };

	if (token_list[1] == NULL) {
		ret = -1;
	} else if (!strcmp(token_list[1], "validate")) {
		input.command = VALIDATE;
		ret = parse_flow_rule(token_list, &input);

	} else if (!strcmp(token_list[1], "create")) {
		input.command = CREATE;
		ret = parse_flow_rule(token_list, &input);

	} else if (!strcmp(token_list[1], "destroy")) {
		ret = parse_flow_destroy(token_list, &input);

	} else {
		ret = -1;
	}

	if (ret != 0) {
		free_flow_args(&input);
		make_response(response, "error",
			"Flow command invalid argument", NULL);
		return 0;
	}

	exec_flow(&input, response);

	return 0;
}

static int
append_flow_pattern_json(const struct rte_flow_item *pattern, int buf_size,
	char *pattern_str)
{
	uint32_t i, j;
	uint32_t nof_elems = 3;
	int ret = 0;
	char *tmp_str;
	const char element_str[][5] = { "spec", "last", "mask" };
	const struct rte_flow_item *ptn = pattern;
	struct flow_item_ops *ops;
	const void *tmp_ptr[nof_elems];

	tmp_str = malloc(buf_size);
	if (tmp_str == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"Memory allocation failure(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	while (ptn->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(tmp_str, 0, buf_size);

		tmp_ptr[0] = ptn->spec;
		tmp_ptr[1] = ptn->last;
		tmp_ptr[2] = ptn->mask;

		for (i = 0; i < RTE_DIM(flow_item_ops_list); i++) {
			ops = &flow_item_ops_list[i];
			if (ptn->type != ops->type)
				continue;

			snprintf(tmp_str, buf_size,
				"{\"type\":\"%s\",",
				ops->str_type);

			for (j = 0; j < nof_elems; j++) {
				snprintf(tmp_str + strlen(tmp_str), buf_size,
					"\"%s\":",
					element_str[j]);

				if (tmp_ptr[j] != NULL)
					ret = ops->status(tmp_ptr[j],
						buf_size - (int)strlen(tmp_str),
						tmp_str + strlen(tmp_str));
				else
					snprintf(tmp_str + strlen(tmp_str),
						buf_size,
						"null");

				if (ret != 0)
					break;

				if (j < nof_elems - 1)
					tmp_str[strlen(tmp_str)] = ',';
			}

			tmp_str[strlen(tmp_str)] = '}';

			break;
		}

		if (ret != 0)
			break;

		if ((int)strlen(pattern_str) + (int)strlen(tmp_str)
			> buf_size - 1) {
			ret = -1;
			break;
		}
		strncat(pattern_str, tmp_str, strlen(tmp_str));

		/*
		 * If there is the following pattern, add ',' to
		 * pattern_str
		 */
		ptn++;
		if (ptn->type != RTE_FLOW_ITEM_TYPE_END) {
			if ((int)strlen(pattern_str) + 1 > buf_size - 1) {
				ret = -1;
				break;
			}
			pattern_str[strlen(pattern_str)] = ',';
		}
	}

	if (tmp_str != NULL)
		free(tmp_str);

	return ret;
}

static int
append_flow_action_json(const struct rte_flow_action *actions, int buf_size,
	char *actions_str)
{
	uint32_t i;
	int ret = 0;
	char *tmp_str;
	const struct rte_flow_action *act = actions;
	struct flow_action_ops *ops;

	tmp_str = malloc(buf_size);
	if (tmp_str == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"Memory allocation failure(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	while (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(tmp_str, 0, buf_size);

		for (i = 0; i < RTE_DIM(flow_action_ops_list); i++) {
			ops = &flow_action_ops_list[i];
			if (act->type != ops->type)
				continue;

			snprintf(tmp_str, buf_size,
				"{\"type\":\"%s\",\"conf\":",
				ops->str_type);

			ret = ops->status(act->conf,
				buf_size - (int)strlen(tmp_str),
				tmp_str + strlen(tmp_str));
			tmp_str[strlen(tmp_str)] = '}';
			break;
		}

		if (ret != 0)
			break;

		if ((int)strlen(actions_str) + (int)strlen(tmp_str)
			> buf_size - 1) {
			ret = -1;
			break;
		}
		strncat(actions_str, tmp_str, strlen(tmp_str));

		/*
		 * If there is the following pattern, add ',' to
		 * actions_str
		 */
		act++;
		if (act->type != RTE_FLOW_ACTION_TYPE_END) {
			if ((int)strlen(actions_str) + 1 > buf_size - 1) {
				ret = -1;
				break;
			}
			actions_str[strlen(actions_str)] = ',';
		}
	}

	if (tmp_str != NULL)
		free(tmp_str);

	return ret;
}

static int
append_flow_rule_json(struct flow_rule *flow, int buf_size, char *flow_str)
{
	int ret = 0;
	struct rte_flow_conv_rule rule;
	char *tmp_str, *attr_str, *pattern_str, *actions_str;

	while (1) {
		tmp_str = malloc(buf_size);
		attr_str = malloc(buf_size);
		pattern_str = malloc(buf_size);
		actions_str = malloc(buf_size);
		if (tmp_str == NULL || attr_str == NULL
			|| pattern_str == NULL || actions_str == NULL) {
			RTE_LOG(ERR, SPP_FLOW,
				"Memory allocation failure(%s:%d)\n",
				__func__, __LINE__);
			ret = -1;
			break;
		}
		memset(tmp_str, 0, buf_size);
		memset(attr_str, 0, buf_size);
		memset(pattern_str, 0, buf_size);
		memset(actions_str, 0, buf_size);

		rule = flow->rule;

		ret = append_flow_attr_json(rule.attr_ro, buf_size, attr_str);
		if (ret != 0)
			break;

		ret = append_flow_pattern_json(rule.pattern_ro, buf_size,
			pattern_str);
		if (ret != 0)
			break;

		ret = append_flow_action_json(rule.actions_ro, buf_size,
			actions_str);
		if (ret != 0)
			break;

		snprintf(tmp_str, buf_size,
			"{\"rule_id\":%d,"
			"\"attr\":%s,"
			"\"patterns\":[%s],"
			"\"actions\":[%s]}",
			flow->rule_id, attr_str, pattern_str, actions_str);

		if ((int)strlen(tmp_str) > buf_size - 1) {
			ret = -1;
			break;
		}

		snprintf(flow_str, buf_size, "%s", tmp_str);
		break;
	}

	if (tmp_str != NULL)
		free(tmp_str);
	if (attr_str != NULL)
		free(attr_str);
	if (pattern_str != NULL)
		free(pattern_str);
	if (actions_str != NULL)
		free(actions_str);

	return ret;
}

int
append_flow_json(int port_id, int buf_size, char *output)
{
	int ret = 0;
	int str_size = 0;
	char *flow_str, *tmp_str;
	struct flow_rule *flow;

	flow_str = malloc(buf_size);
	tmp_str = malloc(buf_size);
	if (flow_str == NULL || tmp_str == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"Memory allocation failure(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	flow = port_list[port_id].flow_list;

	while (flow != NULL) {
		memset(flow_str, 0, buf_size);

		ret = append_flow_rule_json(flow, buf_size, flow_str);
		if (ret != 0)
			break;

		if (str_size == 0) {
			snprintf(output, buf_size, "%s", flow_str);
			str_size += (int)strlen(flow_str);

		} else {
			str_size += ((int)strlen(flow_str) + 1);
			if (str_size > buf_size - 1) {
				ret = -1;
				break;
			}

			/*
			 * Since flow_list is in descending order,
			 * concatenate the strings in front.
			 */
			memset(tmp_str, 0, buf_size);
			strncpy(tmp_str, output, buf_size);
			memset(output, 0, buf_size);

			snprintf(output, buf_size, "%s,%s",
				flow_str, tmp_str);
		}

		flow = flow->prev;
	}

	if (ret == 0) {
		if ((int)strlen("[]") + (int)strlen(flow_str)
			> buf_size - 1)
			ret = -1;
		else {
			memset(tmp_str, 0, buf_size);
			strncpy(tmp_str, output, buf_size);
			memset(output, 0, buf_size);

			snprintf(output, buf_size, "[%s]", tmp_str);
		}
	}

	if (ret != 0)
		RTE_LOG(ERR, SPP_FLOW,
			"Cannot send all of flow stats(%s:%d)\n",
			__func__, __LINE__);

	if (flow_str != NULL)
		free(flow_str);
	if (tmp_str != NULL)
		free(tmp_str);

	return ret;
}
