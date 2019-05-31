/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <string.h>

#include <rte_ether.h>
#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "cmd_parser.h"

#define RTE_LOGTYPE_PCAP_PARSER RTE_LOGTYPE_USER2

/* set parse error */
static inline int
set_parse_error(struct sppwk_parse_err_msg *wk_err_msg,
		const int err_code, const char *err_msg)
{
	wk_err_msg->code = err_code;

	if (likely(err_msg != NULL))
		strcpy(wk_err_msg->msg, err_msg);

	return wk_err_msg->code;
}

/* set parse error */
static inline int
set_string_value_parse_error(struct sppwk_parse_err_msg *wk_err_msg,
		const char *err_details, const char *err_msg)
{
	strcpy(wk_err_msg->details, err_details);
	return set_parse_error(wk_err_msg, SPPWK_PARSE_INVALID_VALUE, err_msg);
}

/* Split command line parameter with spaces */
static int
parse_parameter_value(char *string, int max, int *argc, char *argv[])
{
	int cnt = 0;
	const char *delim = " ";
	char *argv_tok = NULL;
	char *saveptr = NULL;

	argv_tok = strtok_r(string, delim, &saveptr);
	while (argv_tok != NULL) {
		if (cnt >= max)
			return SPP_RET_NG;
		argv[cnt] = argv_tok;
		cnt++;
		argv_tok = strtok_r(NULL, delim, &saveptr);
	}
	*argc = cnt;

	return SPP_RET_OK;
}

/* command list for parse */
struct parse_command_list {
	const char *name;       /* Command name */
	int   param_min;        /* Min number of parameters */
	int   param_max;        /* Max number of parameters */
	int (*func)(struct spp_command_request *request, int argc,
			char *argv[], struct sppwk_parse_err_msg *error,
			int maxargc);
				/* Pointer to command handling function */
	enum spp_command_type type;
				/* Command type */
};

/* command list */
static struct parse_command_list command_list_pcap[] = {
	{ "_get_client_id", 1, 1, NULL, CMD_CLIENT_ID },
	{ "status",	    1, 1, NULL, CMD_STATUS    },
	{ "exit",           1, 1, NULL, CMD_EXIT      },
	{ "start",          1, 1, NULL, CMD_START     },
	{ "stop",           1, 1, NULL, CMD_STOP      },
	{ "",               0, 0, NULL, 0 }  /* termination */
};

/* Parse command line parameters */
static int
parse_command_in_list(struct spp_command_request *request,
			const char *request_str,
			struct sppwk_parse_err_msg *wk_err_msg)
{
	int ret = SPP_RET_OK;
	int command_name_check = 0;
	struct parse_command_list *list = NULL;
	int i = 0;
	int argc = 0;
	char *argv[SPPWK_MAX_PARAMS];
	char tmp_str[SPPWK_MAX_PARAMS * SPPWK_VAL_BUFSZ];
	memset(argv, 0x00, sizeof(argv));
	memset(tmp_str, 0x00, sizeof(tmp_str));

	strcpy(tmp_str, request_str);
	ret = parse_parameter_value(tmp_str, SPPWK_MAX_PARAMS,
			&argc, argv);
	if (ret < SPP_RET_OK) {
		RTE_LOG(ERR, PCAP_PARSER, "Parameter number over limit."
				"request_str=%s\n", request_str);
		return set_parse_error(wk_err_msg,
				SPPWK_PARSE_WRONG_FORMAT, NULL);
	}
	RTE_LOG(DEBUG, PCAP_PARSER, "Decode array. num=%d\n", argc);

	for (i = 0; command_list_pcap[i].name[0] != '\0'; i++) {
		list = &command_list_pcap[i];
		if (strcmp(argv[0], list->name) != 0)
			continue;

		if (unlikely(argc < list->param_min) ||
				unlikely(list->param_max < argc)) {
			command_name_check = 1;
			continue;
		}

		request->commands[0].type = command_list_pcap[i].type;
		if (list->func != NULL)
			return (*list->func)(request, argc, argv, wk_err_msg,
							list->param_max);

		return SPP_RET_OK;
	}

	if (command_name_check != 0) {
		RTE_LOG(ERR, PCAP_PARSER, "Parameter number out of range."
				"request_str=%s\n", request_str);
		return set_parse_error(wk_err_msg,
				SPPWK_PARSE_WRONG_FORMAT, NULL);
	}

	RTE_LOG(ERR, PCAP_PARSER,
			"Unknown command. command=%s, request_str=%s\n",
			argv[0], request_str);
	return set_string_value_parse_error(wk_err_msg, argv[0], "command");
}

/* parse request from no-null-terminated string */
int
spp_command_parse_request(
		struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct sppwk_parse_err_msg *wk_err_msg)
{
	int ret = SPP_RET_NG;
	int i;

	/* parse request */
	request->num_command = 1;
	ret = parse_command_in_list(request, request_str, wk_err_msg);
	if (unlikely(ret != SPP_RET_OK)) {
		RTE_LOG(ERR, PCAP_PARSER,
				"Cannot parse command request. "
				"ret=%d, request_str=%.*s\n",
				ret, (int)request_str_len, request_str);
		return ret;
	}
	request->num_valid_command = 1;

	/* check getter command */
	for (i = 0; i < request->num_valid_command; ++i) {
		switch (request->commands[i].type) {
		case CMD_CLIENT_ID:
			request->is_requested_client_id = 1;
			break;
		case CMD_STATUS:
			request->is_requested_status = 1;
			break;
		case CMD_EXIT:
			request->is_requested_exit = 1;
			break;
		case CMD_START:
			request->is_requested_start = 1;
			break;
		case CMD_STOP:
			request->is_requested_stop = 1;
			break;
		default:
			/* nothing to do */
			break;
		}
	}

	return ret;
}
