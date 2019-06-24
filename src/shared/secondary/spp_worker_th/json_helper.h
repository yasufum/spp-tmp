/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_JSON_HELPER_H_
#define _SPPWK_JSON_HELPER_H_

#include "cmd_utils.h"

#define CMD_TAG_APPEND_SIZE 16

#define JSON_APPEND_COMMA(flg)    ((flg)?", ":"")

#define JSON_APPEND_VALUE(format) "%s\"%s\": "format

#define JSON_APPEND_ARRAY         "%s\"%s\": [ %s ]"

#define JSON_APPEND_BLOCK_NONAME  "%s%s{ %s }"
#define JSON_APPEND_BLOCK         "%s\"%s\": { %s }"

int append_json_comma(char **output);

int append_json_uint_value(const char *name, char **output,
		unsigned int value);

int append_json_int_value(const char *name, char **output,
		int value);

int append_json_str_value(const char *name, char **output,
		const char *str);

int append_json_array_brackets(const char *name, char **output,
		const char *str);

int append_json_block_brackets(const char *name, char **output,
		const char *str);

#endif
