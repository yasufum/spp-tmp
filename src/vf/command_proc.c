#include <unistd.h>
#include <string.h>

#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "spp_vf.h"
#include "string_buffer.h"
#include "command_conn.h"
#include "command_dec.h"
#include "command_proc.h"

#define RTE_LOGTYPE_SPP_COMMAND_PROC RTE_LOGTYPE_USER1

/* request message initial size */
#define CMD_RES_ERR_MSG_SIZE  128
#define CMD_TAG_APPEND_SIZE   16
#define CMD_REQ_BUF_INIT_SIZE 2048
#define CMD_RES_BUF_INIT_SIZE 2048

#define COMMAND_RESP_LIST_EMPTY { "", NULL }

#define JSON_COMMA                ", "
#define JSON_APPEND_COMMA(flg)    ((flg)?JSON_COMMA:"")
#define JSON_APPEND_VALUE(format) "%s\"%s\": "format
#define JSON_APPEND_ARRAY         "%s\"%s\": [ %s ]"
#define JSON_APPEND_BLOCK         "%s\"%s\": { %s }"
#define JSON_APPEND_BLOCK_NONAME  "%s%s{ %s }"

/* command execution result code */
enum command_result_code {
	CRES_SUCCESS = 0,
	CRES_FAILURE,
	CRES_INVALID,
};

/* command execution result information */
struct command_result {
	/* Response code */
	int code;

	/* Response message */
	char result[SPP_CMD_NAME_BUFSZ];

	/* Detailed response message */
	char error_message[CMD_RES_ERR_MSG_SIZE];
};

/* command response list control structure */
struct command_response_list {
	/* Tag name */
	char tag_name[SPP_CMD_NAME_BUFSZ];

	/* Pointer to handling function */
	int (*func)(const char *name, char **output, void *tmp);
};

/* append a comma for JSON format */
static int
append_json_comma(char **output)
{
	*output = spp_strbuf_append(*output, JSON_COMMA, strlen(JSON_COMMA));
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC, "JSON's comma failed to add.\n");
		return -1;
	}

	return 0;
}

/* append data of unsigned integral type for JSON format */
static int
append_json_uint_value(const char *name, char **output, unsigned int value)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + CMD_TAG_APPEND_SIZE*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"JSON's numeric format failed to add. (name = %s, uint = %u)\n",
				name, value);
		return -1;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%u"),
			JSON_APPEND_COMMA(len), name, value);
	return 0;
}

/* append data of integral type for JSON format */
static int
append_json_int_value(const char *name, char **output, int value)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + CMD_TAG_APPEND_SIZE*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"JSON's numeric format failed to add. (name = %s, int = %d)\n",
				name, value);
		return -1;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%d"),
			JSON_APPEND_COMMA(len), name, value);
	return 0;
}

/* append data of string type for JSON format */
static int
append_json_str_value(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"JSON's string format failed to add. (name = %s, str = %s)\n",
				name, str);
		return -1;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("\"%s\""),
			JSON_APPEND_COMMA(len), name, str);
	return 0;
}

/* append brackets of the array for JSON format */
static int
append_json_array_brackets(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"JSON's square bracket failed to add. (name = %s, str = %s)\n",
				name, str);
		return -1;
	}

	sprintf(&(*output)[len], JSON_APPEND_ARRAY,
			JSON_APPEND_COMMA(len), name, str);
	return 0;
}

/* append brackets of the blocks for JSON format */
static int
append_json_block_brackets(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"JSON's curly bracket failed to add. (name = %s, str = %s)\n",
				name, str);
		return -1;
	}

	if (name[0] == '\0')
		sprintf(&(*output)[len], JSON_APPEND_BLOCK_NONAME,
				JSON_APPEND_COMMA(len), name, str);
	else
		sprintf(&(*output)[len], JSON_APPEND_BLOCK,
				JSON_APPEND_COMMA(len), name, str);
	return 0;
}

/* execute one command */
static int
execute_command(const struct spp_command *command)
{
	int ret = 0;

	switch (command->type) {
	case SPP_CMDTYPE_CLASSIFIER_TABLE:
		RTE_LOG(INFO, SPP_COMMAND_PROC,
				"Execute classifier_table command.\n");
		ret = spp_update_classifier_table(
				command->spec.classifier_table.action,
				command->spec.classifier_table.type,
				command->spec.classifier_table.value,
				&command->spec.classifier_table.port);
		break;

	case SPP_CMDTYPE_FLUSH:
		RTE_LOG(INFO, SPP_COMMAND_PROC, "Execute flush command.\n");
		ret = spp_flush();
		break;

	case SPP_CMDTYPE_COMPONENT:
		RTE_LOG(INFO, SPP_COMMAND_PROC, "Execute component command.\n");
		ret = spp_update_component(
				command->spec.component.action,
				command->spec.component.name,
				command->spec.component.core,
				command->spec.component.type);
		break;

	case SPP_CMDTYPE_PORT:
		RTE_LOG(INFO, SPP_COMMAND_PROC,
				"Execute port command. (act = %d)\n",
				command->spec.port.action);
		ret = spp_update_port(
				command->spec.port.action,
				&command->spec.port.port,
				command->spec.port.rxtx,
				command->spec.port.name);
		break;

	case SPP_CMDTYPE_CANCEL:
		RTE_LOG(INFO, SPP_COMMAND_PROC, "Execute cancel command.\n");
		spp_cancel();
		break;

	default:
		RTE_LOG(INFO, SPP_COMMAND_PROC,
				"Execute other command. type=%d\n",
				command->type);
		/* nothing to do here */
		break;
	}

	return ret;
}

/* make decode error message for response */
static const char *
make_decode_error_message(
		const struct spp_command_decode_error *decode_error,
		char *message)
{
	switch (decode_error->code) {
	case SPP_CMD_DERR_BAD_FORMAT:
		sprintf(message, "bad message format");
		break;

	case SPP_CMD_DERR_UNKNOWN_COMMAND:
		sprintf(message, "unknown command(%s)", decode_error->value);
		break;

	case SPP_CMD_DERR_NO_PARAM:
		sprintf(message, "not enough parameter(%s)",
				decode_error->value_name);
		break;

	case SPP_CMD_DERR_BAD_TYPE:
		sprintf(message, "bad value type(%s)",
				decode_error->value_name);
		break;

	case SPP_CMD_DERR_BAD_VALUE:
		sprintf(message, "bad value(%s)", decode_error->value_name);
		break;

	default:
		sprintf(message, "error occur");
		break;
	}

	return message;
}

/* set the command result */
static inline void
set_command_results(struct command_result *result,
		int code, const char *error_messege)
{
	result->code = code;
	switch (code) {
	case CRES_SUCCESS:
		strcpy(result->result, "success");
		memset(result->error_message, 0x00, CMD_RES_ERR_MSG_SIZE);
		break;
	case CRES_FAILURE:
		strcpy(result->result, "error");
		strcpy(result->error_message, error_messege);
		break;
	case CRES_INVALID: /* FALLTHROUGH */
	default:
		strcpy(result->result, "invalid");
		memset(result->error_message, 0x00, CMD_RES_ERR_MSG_SIZE);
		break;
	}
}

/* set decode error to command result */
static void
set_decode_error_to_results(struct command_result *results,
		const struct spp_command_request *request,
		const struct spp_command_decode_error *decode_error)
{
	int i;
	const char *tmp_buff;
	char error_messege[CMD_RES_ERR_MSG_SIZE];

	for (i = 0; i < request->num_command; i++) {
		if (decode_error->code == 0)
			set_command_results(&results[i], CRES_SUCCESS, "");
		else
			set_command_results(&results[i], CRES_INVALID, "");
	}

	if (decode_error->code != 0) {
		tmp_buff = make_decode_error_message(decode_error,
				error_messege);
		set_command_results(&results[request->num_valid_command],
				CRES_FAILURE, tmp_buff);
	}
}

/* append a command result for JSON format */
static int
append_result_value(const char *name, char **output, void *tmp)
{
	const struct command_result *result = tmp;
	return append_json_str_value(name, output, result->result);
}

/* append error details for JSON format */
static int
append_error_details_value(const char *name, char **output, void *tmp)
{
	int ret = -1;
	const struct command_result *result = tmp;
	char *tmp_buff;
	/* string is empty, except for errors */
	if (result->error_message[0] == '\0')
		return 0;

	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	ret = append_json_str_value("message", &tmp_buff,
			result->error_message);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		return -1;
	}

	ret = append_json_block_brackets(name, output, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* append a client id for JSON format */
static int
append_client_id_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	return append_json_int_value(name, output, spp_get_client_id());
}

/* append a list of interface numbers */
static int
append_interface_array(char **output, const enum port_type type)
{
	int i, port_cnt = 0;
	char tmp_str[CMD_TAG_APPEND_SIZE];

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!spp_check_flush_port(type, i))
			continue;

		sprintf(tmp_str, "%s%d", JSON_APPEND_COMMA(port_cnt), i);

		*output = spp_strbuf_append(*output, tmp_str, strlen(tmp_str));
		if (unlikely(*output == NULL)) {
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Interface number failed to add. (type = %d)\n",
					type);
			return -1;
		}

		port_cnt++;
	}

	return 0;
}

/* append a list of interface numbers for JSON format */
static int
append_interface_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = -1;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	if (strcmp(name, SPP_IFTYPE_NIC_STR) == 0)
		ret = append_interface_array(&tmp_buff, PHY);

	else if (strcmp(name, SPP_IFTYPE_VHOST_STR) == 0)
		ret = append_interface_array(&tmp_buff, VHOST);

	else if (strcmp(name, SPP_IFTYPE_RING_STR) == 0)
		ret = append_interface_array(&tmp_buff, RING);

	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		return -1;
	}

	ret = append_json_array_brackets(name, output, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* append a list of port numbers for JSON format */
static int
apeend_port_array(const char *name, char **output,
		const int num, const struct spp_port_index *ports)
{
	int ret = -1;
	int i = 0;
	char port_str[CMD_TAG_APPEND_SIZE];
	char append_str[CMD_TAG_APPEND_SIZE];
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	for (i = 0; i < num; i++) {
		spp_format_port_string(port_str, ports[i].iface_type,
				ports[i].iface_no);

		sprintf(append_str, "%s\"%s\"", JSON_APPEND_COMMA(i), port_str);

		tmp_buff = spp_strbuf_append(tmp_buff, append_str,
				strlen(append_str));
		if (unlikely(tmp_buff == NULL))
			return -1;
	}

	ret = append_json_array_brackets(name, output, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* append one element of core information for JSON format */
static int
append_core_element_value(
		struct spp_iterate_core_params *params,
		const unsigned int lcore_id,
		const char *name, const char *type,
		const int num_rx, const struct spp_port_index *rx_ports,
		const int num_tx, const struct spp_port_index *tx_ports)
{
	int ret = -1;
	int unuse_flg = 0;
	char *buff, *tmp_buff;
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return ret;
	}

	/* there is unnecessary data when "unuse" by type */
	unuse_flg = strcmp(type, SPP_TYPE_UNUSE_STR);

	ret = append_json_uint_value("core", &tmp_buff, lcore_id);
	if (unlikely(ret < 0))
		return ret;

	if (unuse_flg) {
		ret = append_json_str_value("name", &tmp_buff, name);
		if (unlikely(ret < 0))
			return ret;
	}

	ret = append_json_str_value("type", &tmp_buff, type);
	if (unlikely(ret < 0))
		return ret;

	if (unuse_flg) {
		ret = apeend_port_array("rx_port", &tmp_buff,
				num_rx, rx_ports);
		if (unlikely(ret < 0))
			return ret;

		ret = apeend_port_array("tx_port", &tmp_buff,
				num_tx, tx_ports);
		if (unlikely(ret < 0))
			return ret;
	}

	ret = append_json_block_brackets("", &buff, tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* append a list of core information for JSON format */
static int
append_core_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = -1;
	struct spp_iterate_core_params itr_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	itr_params.output = tmp_buff;
	itr_params.element_proc = append_core_element_value;

	ret = spp_iterate_core_info(&itr_params);
	if (unlikely(ret != 0)) {
		spp_strbuf_free(itr_params.output);
		return -1;
	}

	ret = append_json_array_brackets(name, output, itr_params.output);
	spp_strbuf_free(itr_params.output);
	return ret;
}

/* append one element of classifier table for JSON format */
static int
append_classifier_element_value(
		struct spp_iterate_classifier_table_params *params,
		__rte_unused enum spp_classifier_type type,
		const char *data,
		const struct spp_port_index *port)
{
	int ret = -1;
	char *buff, *tmp_buff;
	char port_str[CMD_TAG_APPEND_SIZE];
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = classifier_table)\n");
		return ret;
	}

	spp_format_port_string(port_str, port->iface_type, port->iface_no);

	ret = append_json_str_value("type", &tmp_buff, "mac");
	if (unlikely(ret < 0))
		return ret;

	ret = append_json_str_value("value", &tmp_buff, data);
	if (unlikely(ret < 0))
		return ret;

	ret = append_json_str_value("port", &tmp_buff, port_str);
	if (unlikely(ret < 0))
		return ret;

	ret = append_json_block_brackets("", &buff, tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* append a list of classifier table for JSON format */
static int
append_classifier_table_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = -1;
	struct spp_iterate_classifier_table_params itr_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	itr_params.output = tmp_buff;
	itr_params.element_proc = append_classifier_element_value;

	ret = spp_iterate_classifier_table(&itr_params);
	if (unlikely(ret != 0)) {
		spp_strbuf_free(itr_params.output);
		return -1;
	}

	ret = append_json_array_brackets(name, output, itr_params.output);
	spp_strbuf_free(itr_params.output);
	return ret;
}

/* append string of command response list for JSON format */
static int
append_response_list_value(char **output,
		struct command_response_list *list,
		void *tmp)
{
	int ret = -1;
	int i;
	char *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = response_list)\n");
		return -1;
	}

	for (i = 0; list[i].tag_name[0] != '\0'; i++) {
		tmp_buff[0] = '\0';
		ret = list[i].func(list[i].tag_name, &tmp_buff, tmp);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Failed to get reply string. (tag = %s)\n",
					list[i].tag_name);
			return -1;
		}

		if (tmp_buff[0] == '\0')
			continue;

		if ((*output)[0] != '\0') {
			ret = append_json_comma(output);
			if (unlikely(ret < 0)) {
				spp_strbuf_free(tmp_buff);
				RTE_LOG(ERR, SPP_COMMAND_PROC,
						"Failed to add commas. (tag = %s)\n",
						list[i].tag_name);
				return -1;
			}
		}

		*output = spp_strbuf_append(*output, tmp_buff,
				strlen(tmp_buff));
		if (unlikely(*output == NULL)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Failed to add reply string. (tag = %s)\n",
					list[i].tag_name);
			return -1;
		}
	}

	spp_strbuf_free(tmp_buff);
	return 0;
}

/* termination constant of command response list */
#define COMMAND_RESP_TAG_LIST_EMPTY { "", NULL }

/* command response result string list */
struct command_response_list response_result_list[] = {
	{ "result",        append_result_value },
	{ "error_details", append_error_details_value },
	COMMAND_RESP_TAG_LIST_EMPTY
};

/* command response status information string list */
struct command_response_list response_info_list[] = {
	{ "client-id",        append_client_id_value },
	{ "phy",              append_interface_value },
	{ "vhost",            append_interface_value },
	{ "ring",             append_interface_value },
	{ "core",             append_core_value },
	{ "classifier_table", append_classifier_table_value },
	COMMAND_RESP_TAG_LIST_EMPTY
};

/* append a list of command results for JSON format. */
static int
append_command_results_value(const char *name, char **output,
		int num, struct command_result *results)
{
	int ret = -1;
	int i;
	char *tmp_buff1, *tmp_buff2;
	tmp_buff1 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff1 == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s, buff=1)\n",
				name);
		return -1;
	}

	tmp_buff2 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff2 == NULL)) {
		spp_strbuf_free(tmp_buff1);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s, buff=2)\n",
				name);
		return -1;
	}

	for (i = 0; i < num; i++) {
		tmp_buff1[0] = '\0';
		ret = append_response_list_value(&tmp_buff1,
				response_result_list, &results[i]);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return -1;
		}

		ret = append_json_block_brackets("", &tmp_buff2, tmp_buff1);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return -1;
		}

	}

	ret = append_json_array_brackets(name, output, tmp_buff2);
	spp_strbuf_free(tmp_buff1);
	spp_strbuf_free(tmp_buff2);
	return ret;
}

/* append a list of status information for JSON format. */
static int
append_info_value(const char *name, char **output)
{
	int ret = -1;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = %s)\n",
				name);
		return -1;
	}

	ret = append_response_list_value(&tmp_buff,
			response_info_list, NULL);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		return -1;
	}

	ret = append_json_block_brackets(name, output, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* send response for decode error */
static void
send_decode_error_response(int *sock, const struct spp_command_request *request,
		struct command_result *command_results)
{
	int ret = -1;
	char *msg, *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = decode_error_response)\n");
		return;
	}

	/* create & append result array */
	ret = append_command_results_value("results", &tmp_buff,
			request->num_command, command_results);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"Failed to make command result response.\n");
		return;
	}

	msg = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(msg == NULL)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = decode_error_response)\n");
		return;
	}
	ret = append_json_block_brackets("", &msg, tmp_buff);
	spp_strbuf_free(tmp_buff);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(msg);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = result_response)\n");
		return;
	}

	RTE_LOG(DEBUG, SPP_COMMAND_PROC,
			"Make command response (decode error). "
			"response_str=\n%s\n", msg);

	/* send response to requester */
	ret = spp_send_message(sock, msg, strlen(msg));
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"Failed to send decode error response.\n");
		/* not return */
	}

	spp_strbuf_free(msg);
}

/* send response for command execution result */
static void
send_command_result_response(int *sock,
		const struct spp_command_request *request,
		struct command_result *command_results)
{
	int ret = -1;
	char *msg, *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = result_response)\n");
		return;
	}

	/* create & append result array */
	ret = append_command_results_value("results", &tmp_buff,
			request->num_command, command_results);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"Failed to make command result response.\n");
		return;
	}

	/* append client id information value */
	if (request->is_requested_client_id) {
		ret = append_client_id_value("client_id", &tmp_buff, NULL);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Failed to make client id response.\n");
			return;
		}
	}

	/* append info value */
	if (request->is_requested_status) {
		ret = append_info_value("info", &tmp_buff);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Failed to make status response.\n");
			return;
		}
	}

	msg = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(msg == NULL)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = result_response)\n");
		return;
	}
	ret = append_json_block_brackets("", &msg, tmp_buff);
	spp_strbuf_free(tmp_buff);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(msg);
		RTE_LOG(ERR, SPP_COMMAND_PROC,
				"allocate error. (name = result_response)\n");
		return;
	}

	RTE_LOG(DEBUG, SPP_COMMAND_PROC,
			"Make command response (command result). "
			"response_str=\n%s\n", msg);

	/* send response to requester */
	ret = spp_send_message(sock, msg, strlen(msg));
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, SPP_COMMAND_PROC,
			"Failed to send command result response.\n");
		/* not return */
	}

	spp_strbuf_free(msg);
}

/* process command request from no-null-terminated string */
static int
process_request(int *sock, const char *request_str, size_t request_str_len)
{
	int ret = -1;
	int i;

	struct spp_command_request request;
	struct spp_command_decode_error decode_error;
	struct command_result command_results[SPP_CMD_MAX_COMMANDS];

	memset(&request, 0, sizeof(struct spp_command_request));
	memset(&decode_error, 0, sizeof(struct spp_command_decode_error));
	memset(command_results, 0, sizeof(command_results));

	RTE_LOG(DEBUG, SPP_COMMAND_PROC, "Start command request processing. "
			"request_str=\n%.*s\n",
			(int)request_str_len, request_str);

	/* decode request message */
	ret = spp_command_decode_request(
			&request, request_str, request_str_len, &decode_error);
	if (unlikely(ret != 0)) {
		/* send error response */
		set_decode_error_to_results(command_results, &request,
				&decode_error);
		send_decode_error_response(sock, &request, command_results);
		RTE_LOG(DEBUG, SPP_COMMAND_PROC,
				"End command request processing.\n");
		return 0;
	}

	RTE_LOG(DEBUG, SPP_COMMAND_PROC, "Command request is valid. "
			"num_command=%d, num_valid_command=%d\n",
			request.num_command, request.num_valid_command);

	/* execute commands */
	for (i = 0; i < request.num_command ; ++i) {
		ret = execute_command(request.commands + i);
		if (unlikely(ret != 0)) {
			set_command_results(&command_results[i], CRES_FAILURE,
					"error occur");

			/* not execute remaining commands */
			for (++i; i < request.num_command ; ++i)
				set_command_results(&command_results[i],
					CRES_INVALID, "");

			break;
		}

		set_command_results(&command_results[i], CRES_SUCCESS, "");
	}

	if (request.is_requested_exit) {
		/* Terminated by process exit command.                       */
		/* Other route is normal end because it responds to command. */
		RTE_LOG(INFO, SPP_COMMAND_PROC,
				"No response with process exit command.\n");
		return -1;
	}

	/* send response */
	send_command_result_response(sock, &request, command_results);

	RTE_LOG(DEBUG, SPP_COMMAND_PROC, "End command request processing.\n");

	return 0;
}

/* initialize command processor. */
int
spp_command_proc_init(const char *controller_ip, int controller_port)
{
	return spp_command_conn_init(controller_ip, controller_port);
}

/* process command from controller. */
int
spp_command_proc_do(void)
{
	int ret = -1;
	int msg_ret = -1;

	static int sock = -1;
	static char *msgbuf;

	if (unlikely(msgbuf == NULL)) {
		msgbuf = spp_strbuf_allocate(CMD_REQ_BUF_INIT_SIZE);
		if (unlikely(msgbuf == NULL)) {
			RTE_LOG(ERR, SPP_COMMAND_PROC,
					"Cannot allocate memory for receive data(init).\n");
			return -1;
		}
	}

	ret = spp_connect_to_controller(&sock);
	if (unlikely(ret != 0))
		return 0;

	msg_ret = spp_receive_message(&sock, &msgbuf);
	if (unlikely(msg_ret <= 0)) {
		if (likely(msg_ret == 0))
			return 0;
		else if (unlikely(msg_ret == SPP_CONNERR_TEMPORARY))
			return 0;
		else
			return -1;
	}

	ret = process_request(&sock, msgbuf, msg_ret);
	spp_strbuf_remove_front(msgbuf, msg_ret);

	return ret;
}
