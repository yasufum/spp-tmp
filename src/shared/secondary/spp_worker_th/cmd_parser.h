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
#define SPP_CMD_MAX_COMMANDS 32

/* Maximum number of parameters per command. */
#define SPP_CMD_MAX_PARAMETERS 8

/* Size of string buffer of message including null char. */
#define SPP_CMD_NAME_BUFSZ  32

/* Size of string buffer of detailed message including null char. */
#define SPP_CMD_VALUE_BUFSZ 111

/* Fix value for 'unused' status. */
#define SPP_CMD_UNUSE "unuse"

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
/* TODO(yasufum) refactor each name prefix `SPP_CMD_ACTION_`. */
enum spp_command_action {
	SPP_CMD_ACTION_NONE,  /**< none */
	SPP_CMD_ACTION_START, /**< start */
	SPP_CMD_ACTION_STOP,  /**< stop */
	SPP_CMD_ACTION_ADD,   /**< add */
	SPP_CMD_ACTION_DEL,   /**< delete */
};

/**
 * SPP command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in command_dec.c
 */
/* TODO(yasufum) refactor each name prefix `SPP_`. */
enum spp_command_type {
	SPP_CMDTYPE_CLASSIFIER_TABLE_MAC,
	SPP_CMDTYPE_CLASSIFIER_TABLE_VLAN,
	SPP_CMDTYPE_CLIENT_ID,  /**< get_client_id */
	SPP_CMDTYPE_STATUS,  /**< status */
	SPP_CMDTYPE_EXIT,  /**< exit */
	SPP_CMDTYPE_COMPONENT,  /**< component */
	SPP_CMDTYPE_PORT,  /**< port */
};

/* `classifier_table` command specific parameters. */
struct spp_command_classifier_table {
	enum spp_command_action action;  /**< add or del */
	enum spp_classifier_type type;  /**< currently only for mac */
	int vid;  /**< VLAN ID  */
	char mac[SPP_CMD_VALUE_BUFSZ];  /**< MAC address  */
	struct spp_port_index port;/**< Destination port type and number */
};

/* `flush` command specific parameters. */
struct spp_command_flush {
	/* nothing specific */
};

/* `component` command parameters. */
struct spp_command_component {
	enum spp_command_action action;  /**< start or stop */
	char name[SPP_CMD_NAME_BUFSZ];  /**< component name */
	unsigned int core;  /**< logical core number */
	enum spp_component_type type;  /**< component type */
};

/* `port` command parameters. */
struct spp_command_port {
	enum spp_command_action action;  /**< add or del */
	struct spp_port_index port;  /**< port type and number */
	enum spp_port_rxtx rxtx;  /**< rx or tx identifier */
	char name[SPP_CMD_NAME_BUFSZ];  /**<  component name */
	struct spp_port_ability ability;  /**< port ability */
};

struct spp_command {
	enum spp_command_type type; /**< command type */

	union {  /**< command descriptors */
		struct spp_command_classifier_table classifier_table;
		struct spp_command_flush flush;
		struct spp_command_component component;
		struct spp_command_port port;
	} spec;
};

/* Request parameters. */
struct spp_command_request {
	int num_command;  /**< Number of accepted commands */
	int num_valid_command;  /**< Number of executed commands */
	struct spp_command commands[SPP_CMD_MAX_COMMANDS];  /**< list of cmds */

	int is_requested_client_id;
	int is_requested_status;
	int is_requested_exit;
};

/* Error message if parse failed. */
struct sppwk_parse_err_msg {
	int code;  /**< Code in enu sppwk_parse_error_code */
	char msg[SPP_CMD_NAME_BUFSZ];   /**< Message in short */
	char details[SPP_CMD_VALUE_BUFSZ];  /**< Detailed message */
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
