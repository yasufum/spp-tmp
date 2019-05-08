/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_PARSER_H_
#define _SPPWK_CMD_PARSER_H_

/**
 * @file cmd_parser.h
 * @brief Define a set of vars and functions for parsing SPP worker commands.
 */

#include "spp_proc.h"

/* Maximum number of commands per request. */
#define SPPWK_MAX_CMDS 32

/* Maximum number of parameters per command. */
#define SPPWK_MAX_PARAMS 8

/* Size of string buffer of message including null char. */
#define SPPWK_NAME_BUFSZ  32

/* Size of string buffer of detailed message including null char. */
#define SPPWK_VAL_BUFSZ 111

/**
 * Error code for diagnosis and notifying the reason. It starts from 1 because
 * 0 is used for succeeded and not appropriate for error in general.
 */
enum sppwk_parse_error_code {
	SPPWK_PARSE_WRONG_FORMAT = 1,  /**< Wrong format */
	SPPWK_PARSE_UNKNOWN_CMD,  /**< Unknown command */
	SPPWK_PARSE_NO_PARAM,  /**< No parameters */
	SPPWK_PARSE_INVALID_TYPE,  /**< Invalid data type */
	SPPWK_PARSE_INVALID_VALUE,  /**< Invalid value */
};

/**
 * Define actions of SPP worker threads. Each of targeting objects and actions
 * is defined as following.
 *   - compomnent      : start, stop
 *   - port            : add, del
 *   - classifier_table: add, del
 */
enum sppwk_action {
	SPPWK_ACT_NONE,  /**< none */
	SPPWK_ACT_START, /**< start */
	SPPWK_ACT_STOP,  /**< stop */
	SPPWK_ACT_ADD,   /**< add */
	SPPWK_ACT_DEL,   /**< delete */
};

/**
 * SPP command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in command_dec.c
 */
/*
 * TODO(yasufum) consider to divide because each of target of scope is
 * different and not so understandable for usage. For example, worker is
 * including classifier or it status.
 */
enum sppwk_cmd_type {
	SPPWK_CMDTYPE_CLS_MAC,
	SPPWK_CMDTYPE_CLS_VLAN,
	SPPWK_CMDTYPE_CLIENT_ID,  /**< get_client_id */
	SPPWK_CMDTYPE_STATUS,  /**< status */
	SPPWK_CMDTYPE_EXIT,  /**< exit */
	SPPWK_CMDTYPE_WORKER,  /**< worker thread */
	SPPWK_CMDTYPE_PORT,  /**< port */
};

/* `classifier_table` command specific parameters. */
struct sppwk_cls_cmd_attr {
	enum sppwk_action wk_action;  /**< add or del */
	enum spp_classifier_type type;  /**< currently only for mac */
	int vid;  /**< VLAN ID  */
	char mac[SPPWK_VAL_BUFSZ];  /**< MAC address  */
	struct spp_port_index port;/**< Destination port type and number */
};

/* `flush` command specific parameters. */
struct sppwk_cmd_flush {
	/* nothing specific */
};

/* `component` command parameters. */
struct sppwk_cmd_comp {
	enum sppwk_action wk_action;  /**< start or stop */
	char name[SPPWK_NAME_BUFSZ];  /**< component name */
	unsigned int core;  /**< logical core number */
	enum spp_component_type type;  /**< component type */
};

/* `port` command parameters. */
struct spp_command_port {
	enum sppwk_action wk_action;  /**< add or del */
	struct spp_port_index port;  /**< port type and number */
	enum spp_port_rxtx rxtx;  /**< rx or tx identifier */
	char name[SPPWK_NAME_BUFSZ];  /**<  component name */
	struct spp_port_ability ability;  /**< port ability */
};

struct spp_command {
	enum sppwk_cmd_type type; /**< command type */

	union {  /**< command descriptors */
		struct sppwk_cls_cmd_attr cls_table;
		struct sppwk_cmd_flush flush;
		struct sppwk_cmd_comp comp;
		struct spp_command_port port;
	} spec;
};

/* Request parameters. */
struct spp_command_request {
	int num_command;  /**< Number of accepted commands */
	int num_valid_command;  /**< Number of executed commands */
	struct spp_command commands[SPPWK_MAX_CMDS];  /**< list of cmds */

	int is_requested_client_id;
	int is_requested_status;
	int is_requested_exit;
};

/* Error message if parse failed. */
struct sppwk_parse_err_msg {
	int code;  /**< Code in enu sppwk_parse_error_code */
	char msg[SPPWK_NAME_BUFSZ];   /**< Message in short */
	char details[SPPWK_VAL_BUFSZ];  /**< Detailed message */
};

/**
 * Parse request of non null terminated string.
 *
 * @param request
 *  The pointer to struct spp_command_request.@n
 *  The result value of decoding the command message.
 * @param request_str
 *  The pointer to requested command message.
 * @param request_str_len
 *  The length of requested command message.
 * @param wk_err_msg
 *  The pointer to struct sppwk_parse_err_msg.@n
 *  Detailed error information will be stored.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval !0 failed.
 */
int spp_command_decode_request(struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct sppwk_parse_err_msg *wk_err_msg);

#endif /* _SPPWK_CMD_PARSER_H_ */
