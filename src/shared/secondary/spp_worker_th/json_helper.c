/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "string_buffer.h"
#include "json_helper.h"

#define RTE_LOGTYPE_WK_JSON_HELPER RTE_LOGTYPE_USER1

int
append_json_comma(char **output)
{
	*output = spp_strbuf_append(*output, ", ", strlen(", "));
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's comma failed to add.\n");
		return SPP_RET_NG;
	}

	return SPP_RET_OK;
}

/* append data of unsigned integral type for JSON format */
int
append_json_uint_value(const char *name, char **output, unsigned int value)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + CMD_TAG_APPEND_SIZE*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's numeric format failed to add. "
				"(name = %s, uint = %u)\n", name, value);
		return SPP_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%u"),
			JSON_APPEND_COMMA(len), name, value);
	return SPP_RET_OK;
}

/* append data of integral type for JSON format */
int
append_json_int_value(const char *name, char **output, int value)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + CMD_TAG_APPEND_SIZE*2);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's numeric format failed to add. "
				"(name = %s, int = %d)\n", name, value);
		return SPP_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("%d"),
			JSON_APPEND_COMMA(len), name, value);
	return SPP_RET_OK;
}

/* append data of string type for JSON format */
int
append_json_str_value(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's string format failed to add. "
				"(name = %s, str = %s)\n", name, str);
		return SPP_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_VALUE("\"%s\""),
			JSON_APPEND_COMMA(len), name, str);
	return SPP_RET_OK;
}

/* append brackets of the array for JSON format */
int
append_json_array_brackets(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's square bracket failed to add. "
				"(name = %s, str = %s)\n", name, str);
		return SPP_RET_NG;
	}

	sprintf(&(*output)[len], JSON_APPEND_ARRAY,
			JSON_APPEND_COMMA(len), name, str);
	return SPP_RET_OK;
}

/* append brackets of the blocks for JSON format */
int
append_json_block_brackets(const char *name, char **output, const char *str)
{
	int len = strlen(*output);
	/* extend the buffer */
	*output = spp_strbuf_append(*output, "",
			strlen(name) + strlen(str) + CMD_TAG_APPEND_SIZE);
	if (unlikely(*output == NULL)) {
		RTE_LOG(ERR, WK_JSON_HELPER,
				"JSON's curly bracket failed to add. "
				"(name = %s, str = %s)\n", name, str);
		return SPP_RET_NG;
	}

	if (name[0] == '\0')
		sprintf(&(*output)[len], JSON_APPEND_BLOCK_NONAME,
				JSON_APPEND_COMMA(len), name, str);
	else
		sprintf(&(*output)[len], JSON_APPEND_BLOCK,
				JSON_APPEND_COMMA(len), name, str);
	return SPP_RET_OK;
}
