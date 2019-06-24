/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "cmd_res_formatter.h"
#include "cmd_utils.h"
#include "shared/secondary/json_helper.h"

#ifdef SPP_VF_MODULE
#include "vf_deps.h"
#endif

#ifdef SPP_MIRROR_MODULE
#include "mirror_deps.h"
#endif

#define RTE_LOGTYPE_WK_CMD_RES_FMT RTE_LOGTYPE_USER1

/* Proto type declaration for a list of operator functions. */
static int append_result_value(const char *name, char **output, void *tmp);
static int append_error_details_value(const char *name, char **output,
		void *tmp);
static int add_interface(const char *name, char **output,
		void *tmp __attribute__ ((unused)));
static int add_master_lcore(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

/**
 * List of worker process type. The order of items should be same as the order
 * of enum `secondary_type` in cmd_utils.h.
 */
/* TODO(yasufum) rename `secondary_type` to `sppwk_proc_type`. */
const char *SPPWK_PROC_TYPE_LIST[] = {
	"none",
	"vf",
	"mirror",
	"",  /* termination */
};

/**
 * List of port abilities. The order of items should be same as the order of
 * enum `sppwk_port_abl_ops` in spp_vf.h.
 */
const char *PORT_ABILITY_STAT_LIST[] = {
	"none",
	"add",
	"del",
	"",  /* termination */
};

/* command response result string list */
struct cmd_response response_result_list[] = {
	{ "result", append_result_value },
	{ "error_details", append_error_details_value },
	{ "", NULL }
};

/**
 * List of combination of tag and operator function. It is used to assemble
 * a result of command in JSON like as following.
 *
 *     {
 *         "client-id": 1,
 *         "ports": ["phy:0", "phy:1", "vhost:0", "ring:0"],
 *         "components": [
 *             {
 *                 "core": 2,
 *                 ...
 */
struct cmd_response response_info_list[] = {
	{ "client-id", add_client_id },
	{ "phy", add_interface },
	{ "vhost", add_interface },
	{ "ring", add_interface },
	{ "master-lcore", add_master_lcore},
	{ "core", add_core},
#ifdef SPP_VF_MODULE
	{ "classifier_table", add_classifier_table},
#endif /* SPP_VF_MODULE */
	{ "", NULL }
};

/* Append a command result in JSON format. */
static int
append_result_value(const char *name, char **output, void *tmp)
{
	const struct cmd_result *result = tmp;
	return append_json_str_value(output, name, result->result);
}

/* Append error details in JSON format. */
static int
append_error_details_value(const char *name, char **output, void *tmp)
{
	int ret = SPP_RET_NG;
	const struct cmd_result *result = tmp;
	char *tmp_buff;
	/* string is empty, except for errors */
	if (result->err_msg[0] == '\0')
		return SPP_RET_OK;

	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Fail to alloc buf for `%s`.\n", name);
		return SPP_RET_NG;
	}

	ret = append_json_str_value(&tmp_buff, "message", result->err_msg);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		return SPP_RET_NG;
	}

	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* Check if port is already flushed. */
static int
is_port_flushed(enum port_type iface_type, int iface_no)
{
	struct sppwk_port_info *port = get_sppwk_port(iface_type, iface_no);
	return port->ethdev_port_id >= 0;
}

/* Append index number as comma separated format such as `0, 1, ...`. */
int
append_interface_array(char **output, const enum port_type type)
{
	int i, port_cnt = 0;
	char tmp_str[CMD_TAG_APPEND_SIZE];

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!is_port_flushed(type, i))
			continue;

		sprintf(tmp_str, "%s%d", JSON_APPEND_COMMA(port_cnt), i);

		*output = spp_strbuf_append(*output, tmp_str, strlen(tmp_str));
		if (unlikely(*output == NULL)) {
			RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) replace %d to string. */
				"Failed to add index for type `%d`.\n", type);
			return SPP_RET_NG;
		}
		port_cnt++;
	}
	return SPP_RET_OK;
}

/* TODO(yasufum) move to another file for util funcs. */
/* Get proc type from global command params. */
static int
get_wk_type(void)
{
	struct startup_param *params;
	sppwk_get_mng_data(&params, NULL, NULL, NULL, NULL, NULL, NULL);
	return params->secondary_type;
}

/* append a secondary process type for JSON format */
int
append_process_type_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	return append_json_str_value(output, name,
			SPPWK_PROC_TYPE_LIST[get_wk_type()]);
}

/* append a value of vlan for JSON format */
int
append_vlan_value(char **output, const int ope, const int vid, const int pcp)
{
	int ret = SPP_RET_OK;
	ret = append_json_str_value(output, "operation",
			PORT_ABILITY_STAT_LIST[ope]);
	if (unlikely(ret < SPP_RET_OK))
		return SPP_RET_NG;

	ret = append_json_int_value(output, "id", vid);
	if (unlikely(ret < 0))
		return SPP_RET_NG;

	ret = append_json_int_value(output, "pcp", pcp);
	if (unlikely(ret < 0))
		return SPP_RET_NG;

	return SPP_RET_OK;
}

/* append a block of vlan for JSON format */
int
append_vlan_block(const char *name, char **output,
		const int port_id, const enum spp_port_rxtx rxtx)
{
	int ret = SPP_RET_NG;
	int i = 0;
	struct spp_port_ability *info = NULL;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	spp_port_ability_get_info(port_id, rxtx, &info);
	for (i = 0; i < SPP_PORT_ABILITY_MAX; i++) {
		switch (info[i].ops) {
		case SPPWK_PORT_ABL_OPS_ADD_VLANTAG:
		case SPPWK_PORT_ABL_OPS_DEL_VLANTAG:
			ret = append_vlan_value(&tmp_buff, info[i].ops,
					info[i].data.vlantag.vid,
					info[i].data.vlantag.pcp);
			if (unlikely(ret < SPP_RET_OK))
				return SPP_RET_NG;

			/*
			 * Change counter to "maximum+1" for exit the loop.
			 * An if statement after loop termination is false
			 * by "maximum+1 ".
			 */
			i = SPP_PORT_ABILITY_MAX + 1;
			break;
		default:
			/* not used */
			break;
		}
	}
	if (i == SPP_PORT_ABILITY_MAX) {
		ret = append_vlan_value(&tmp_buff, SPPWK_PORT_ABL_OPS_NONE,
				0, 0);
		if (unlikely(ret < SPP_RET_OK))
			return SPP_RET_NG;
	}

	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/**
 * Get consistent port ID of rte ethdev from resource UID such as `phy:0`.
 * It returns a port ID, or error code if it's failed to.
 */
static int
get_ethdev_port_id(enum port_type iface_type, int iface_no)
{
	struct iface_info *iface_info = NULL;

	sppwk_get_mng_data(NULL, &iface_info,
				NULL, NULL, NULL, NULL, NULL);
	switch (iface_type) {
	case PHY:
		return iface_info->nic[iface_no].ethdev_port_id;
	case RING:
		return iface_info->ring[iface_no].ethdev_port_id;
	case VHOST:
		return iface_info->vhost[iface_no].ethdev_port_id;
	default:
		return SPP_RET_NG;
	}
}

/* append a block of port numbers for JSON format */
int
append_port_block(char **output, const struct sppwk_port_idx *port,
		const enum spp_port_rxtx rxtx)
{
	int ret = SPP_RET_NG;
	char port_str[CMD_TAG_APPEND_SIZE];
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = port_block)\n");
		return SPP_RET_NG;
	}

	spp_format_port_string(port_str, port->iface_type, port->iface_no);
	ret = append_json_str_value(&tmp_buff, "port", port_str);
	if (unlikely(ret < SPP_RET_OK))
		return SPP_RET_NG;

	ret = append_vlan_block("vlan", &tmp_buff,
			get_ethdev_port_id(
				port->iface_type, port->iface_no),
			rxtx);
	if (unlikely(ret < SPP_RET_OK))
		return SPP_RET_NG;

	ret = append_json_block_brackets(output, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* append a list of port numbers for JSON format */
int
append_port_array(const char *name, char **output, const int num,
		const struct sppwk_port_idx *ports,
		const enum spp_port_rxtx rxtx)
{
	int ret = SPP_RET_NG;
	int i = 0;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	for (i = 0; i < num; i++) {
		ret = append_port_block(&tmp_buff, &ports[i], rxtx);
		if (unlikely(ret < SPP_RET_OK))
			return SPP_RET_NG;
	}

	ret = append_json_array_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/**
 * TODO(yasufum) add usages called from `add_core` or refactor
 * confusing function names.
 */
/* append one element of core information for JSON format */
int
append_core_element_value(
		struct spp_iterate_core_params *params,
		const unsigned int lcore_id,
		const char *name, const char *type,
		const int num_rx, const struct sppwk_port_idx *rx_ports,
		const int num_tx, const struct sppwk_port_idx *tx_ports)
{
	int ret = SPP_RET_NG;
	int unuse_flg = 0;
	char *buff, *tmp_buff;
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		/* TODO(yasufum) refactor no meaning err msg */
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"allocate error. (name = %s)\n",
				name);
		return ret;
	}

	/* there is unnecessary data when "unuse" by type */
	unuse_flg = strcmp(type, SPPWK_TYPE_NONE_STR);

	/**
	 * TODO(yasufum) change ambiguous "core" to more specific one such as
	 * "worker-lcores" or "slave-lcores".
	 */
	ret = append_json_uint_value(&tmp_buff, "core", lcore_id);
	if (unlikely(ret < SPP_RET_OK))
		return ret;

	if (unuse_flg) {
		ret = append_json_str_value(&tmp_buff, "name", name);
		if (unlikely(ret < 0))
			return ret;
	}

	ret = append_json_str_value(&tmp_buff, "type", type);
	if (unlikely(ret < SPP_RET_OK))
		return ret;

	if (unuse_flg) {
		ret = append_port_array("rx_port", &tmp_buff,
				num_rx, rx_ports, SPP_PORT_RXTX_RX);
		if (unlikely(ret < 0))
			return ret;

		ret = append_port_array("tx_port", &tmp_buff,
				num_tx, tx_ports, SPP_PORT_RXTX_TX);
		if (unlikely(ret < SPP_RET_OK))
			return ret;
	}

	ret = append_json_block_brackets(&buff, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* append string of command response list for JSON format */
int
append_response_list_value(char **output, struct cmd_response *responses,
		void *tmp)
{
	int ret = SPP_RET_NG;
	int i;
	char *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = response_list)\n");
		return SPP_RET_NG;
	}

	for (i = 0; responses[i].tag_name[0] != '\0'; i++) {
		tmp_buff[0] = '\0';
		ret = responses[i].func(responses[i].tag_name, &tmp_buff, tmp);
		if (unlikely(ret < SPP_RET_OK)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RES_FMT,
					"Failed to get reply string. "
					"(tag = %s)\n", responses[i].tag_name);
			return SPP_RET_NG;
		}

		if (tmp_buff[0] == '\0')
			continue;

		if ((*output)[0] != '\0') {
			ret = append_json_comma(output);
			if (unlikely(ret < SPP_RET_OK)) {
				spp_strbuf_free(tmp_buff);
				RTE_LOG(ERR, WK_CMD_RES_FMT,
						"Failed to add commas. "
						"(tag = %s)\n",
						responses[i].tag_name);
				return SPP_RET_NG;
			}
		}

		*output = spp_strbuf_append(*output, tmp_buff,
				strlen(tmp_buff));
		if (unlikely(*output == NULL)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RES_FMT,
					"Failed to add reply string. "
					"(tag = %s)\n",
					responses[i].tag_name);
			return SPP_RET_NG;
		}
	}

	spp_strbuf_free(tmp_buff);
	return SPP_RET_OK;
}

/**
 * Setup `results` section in JSON msg. This is an example.
 *   "results": [ { "result": "success" } ]
 */
int
append_command_results_value(const char *name, char **output,
		int num, struct cmd_result *results)
{
	int ret = SPP_RET_NG;
	int i;
	char *tmp_buff1, *tmp_buff2;

	/* Setup result statement step by step with two buffers. */
	tmp_buff1 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff1 == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Faield to alloc 1st buf for `%s`.\n", name);
		return SPP_RET_NG;
	}
	tmp_buff2 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff2 == NULL)) {
		spp_strbuf_free(tmp_buff1);
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Faield to alloc 2nd buf for `%s`.\n", name);
		return SPP_RET_NG;
	}

	for (i = 0; i < num; i++) {
		tmp_buff1[0] = '\0';

		/* Setup key-val pair such as `"result": "success"` */
		ret = append_response_list_value(&tmp_buff1,
				response_result_list, &results[i]);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return SPP_RET_NG;
		}

		/* Surround key-val pair such as `{ "result": "success" }`. */
		ret = append_json_block_brackets(&tmp_buff2, "", tmp_buff1);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return SPP_RET_NG;
		}
	}

	/**
	 * Setup result statement such as
	 * `"results": [ { "result": "success" } ]`.
	 */
	ret = append_json_array_brackets(output, name, tmp_buff2);

	spp_strbuf_free(tmp_buff1);
	spp_strbuf_free(tmp_buff2);
	return ret;
}

/**
 * Setup response of `status` command.
 *
 * This is an example of the response.
 *   "results": [ { "result": "success" } ],
 *   "info": {
 *       "client-id": 2,
 *       "phy": [ 0, 1 ], "vhost": [  ], "ring": [  ],
 *       "master-lcore": 1,
 *       "core": [
 *           {"core": 2, "type": "unuse"}, {"core": 3, "type": "unuse"}, ...
 *       ],
 *       "classifier_table": [  ]
 *   }
 */
int
append_info_value(const char *name, char **output)
{
	int ret = SPP_RET_NG;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Failed to get empty buf for append `%s`.\n",
				name);
		return SPP_RET_NG;
	}

	/* Setup JSON msg in value of `info` key. */
	ret = append_response_list_value(&tmp_buff, response_info_list, NULL);
	if (unlikely(ret < SPP_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		return SPP_RET_NG;
	}

	/* Setup response of JSON msg. */
	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* TODO(yasufum) move to another file for util funcs. */
/* Get client ID from global command params. */
static int
wk_get_client_id(void)
{
	struct startup_param *params;
	sppwk_get_mng_data(&params, NULL, NULL, NULL, NULL, NULL, NULL);
	return params->client_id;
}

/**
 * Operator functions start with prefix `add_` defined in `response_info_list`
 * of struct `cmd_response` which are for making each of parts of command
 * response.
 */

/* Add entry of client ID such as `"client-id": 1` to a response in JSON. */
int
add_client_id(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	return append_json_int_value(output, name, wk_get_client_id());
}

/* Add entry of port to a response in JSON such as "phy:0". */
static int
add_interface(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPP_RET_NG;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	if (strcmp(name, SPP_IFTYPE_NIC_STR) == 0)
		ret = append_interface_array(&tmp_buff, PHY);

	else if (strcmp(name, SPP_IFTYPE_VHOST_STR) == 0)
		ret = append_interface_array(&tmp_buff, VHOST);

	else if (strcmp(name, SPP_IFTYPE_RING_STR) == 0)
		ret = append_interface_array(&tmp_buff, RING);

	if (unlikely(ret < SPP_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		return SPP_RET_NG;
	}

	ret = append_json_array_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* Add entry of master lcore to a response in JSON. */
static int
add_master_lcore(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPP_RET_NG;
	ret = append_json_int_value(output, name, rte_get_master_lcore());
	return ret;
}

#ifdef SPP_VF_MODULE
/* Iterate classifier_table to create response to status command */
static int
_add_classifier_table(
		struct spp_iterate_classifier_table_params *params)
{
	int ret;

	ret = add_classifier_table_val(params);
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Cannot iterate classifier_mac_table.\n");
		return SPP_RET_NG;
	}

	return SPP_RET_OK;
}

/**
 * Add entries of classifier table in JSON. Before iterating the entries,
 * this function calls several nested functions.
 *   add_classifier_table()  // This function.
 *     -> _add_classifier_table()  // Wrapper and doesn't almost nothing.
 *       -> add_classifier_table_val()  // Setup data and call iterator.
 *         -> iterate_adding_mac_entry()
 */
int
add_classifier_table(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPP_RET_NG;
	struct spp_iterate_classifier_table_params itr_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	itr_params.output = tmp_buff;
	itr_params.element_proc = append_classifier_element_value;

	ret = _add_classifier_table(&itr_params);
	if (unlikely(ret != SPP_RET_OK)) {
		spp_strbuf_free(itr_params.output);
		return SPP_RET_NG;
	}

	ret = append_json_array_brackets(output, name, itr_params.output);
	spp_strbuf_free(itr_params.output);
	return ret;
}
#endif /* SPP_VF_MODULE */
