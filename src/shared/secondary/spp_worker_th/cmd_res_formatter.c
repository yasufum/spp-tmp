/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "cmd_res_formatter.h"
#include "cmd_utils.h"
#include "shared/secondary/json_helper.h"

#define RTE_LOGTYPE_WK_CMD_RES_FMT RTE_LOGTYPE_USER1

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
 * enum `spp_port_ability_type` in spp_vf.h.
 */
const char *PORT_ABILITY_STAT_LIST[] = {
	"none",
	"add",
	"del",
	"",  /* termination */
};

/* append a command result for JSON format */
int
append_result_value(const char *name, char **output, void *tmp)
{
	const struct cmd_result *result = tmp;
	return append_json_str_value(output, name, result->result);
}

/* append error details for JSON format */
int
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
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
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
int
is_port_flushed(enum port_type iface_type, int iface_no)
{
	struct sppwk_port_info *port = get_sppwk_port(iface_type, iface_no);
	return port->ethdev_port_id >= 0;
}

/* append a list of interface numbers */
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
					"Interface number failed to add. "
					"(type = %d)\n", type);
			return SPP_RET_NG;
		}

		port_cnt++;
	}

	return SPP_RET_OK;
}

/* TODO(yasufum) move to another file for util funcs. */
/* Get proc type from global command params. */
int
sppwk_get_proc_type(void)
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
			SPPWK_PROC_TYPE_LIST[sppwk_get_proc_type()]);
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

