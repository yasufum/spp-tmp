/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_RES_FORMATTER_H_
#define _SPPWK_CMD_RES_FORMATTER_H_

#include "shared/common.h"

#define CMD_RES_LEN  32  /* Size of message including null char. */
#define CMD_ERR_MSG_LEN 128

#define CMD_RES_BUF_INIT_SIZE 2048
#define CMD_TAG_APPEND_SIZE 16

struct cmd_result {
	int code;  /* Response code. */
	char result[CMD_RES_LEN];  /* Response msg in short. */
	char err_msg[CMD_ERR_MSG_LEN];  /* Used only if cmd is failed. */
};

int append_result_value(const char *name, char **output, void *tmp);

int append_error_details_value(const char *name, char **output, void *tmp);

int is_port_flushed(enum port_type iface_type, int iface_no);

int append_interface_array(char **output, const enum port_type type);

int append_process_type_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

int sppwk_get_proc_type(void);

int append_vlan_value(char **output, const int ope, const int vid,
		const int pcp);

#endif
