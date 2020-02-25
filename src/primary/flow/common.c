/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_ethdev.h>

#include "shared/secondary/spp_worker_th/data_types.h"
#include "shared/secondary/utils.h"
#include "flow.h"
#include "common.h"

/*
 * Check if port_id is used
 * Return 0: Port_id used
 * Return 1: Unused port_id
 */
int
is_portid_used(int port_id)
{
	uint16_t pid;

	RTE_ETH_FOREACH_DEV(pid) {
		if (port_id == pid)
			return 0;
	}

	return 1;
}

/*
 * Retrieve port ID from source UID of phy port. Return error code if port
 * type is other than phy.
 */
int
parse_phy_port_id(char *res_uid, int *port_id)
{
	int ret;
	char *port_type;
	uint16_t queue_id;

	if (res_uid == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"RES UID is NULL(%s:%d)\n", __func__, __LINE__);
		return -1;
	}

	ret = parse_resource_uid(res_uid, &port_type, port_id, &queue_id);
	if (ret < 0) {
		RTE_LOG(ERR, SPP_FLOW,
			"Failed to parse RES UID(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	if (strcmp(port_type, SPPWK_PHY_STR) != 0) {
		RTE_LOG(ERR, SPP_FLOW,
			"It's not phy type(%s:%d)\n", __func__, __LINE__);
		return -1;
	}
	return 0;
}

/*
 * Convert string to rte_ether_addr.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_rte_ether_addr(char *mac_str, void *output)
{
	int i = 0;
	uint8_t byte;
	char *end, *token;
	char tmp_mac_str[32] = { 0 };
	struct rte_ether_addr *mac_addr = output;

	strncpy(tmp_mac_str, mac_str, 32);
	token = strtok(tmp_mac_str, ":");

	while (token != NULL) {
		if (i >= RTE_ETHER_ADDR_LEN)
			return -1;

		byte = (uint8_t)strtoul(token, &end, 16);
		if (end == NULL || *end != '\0')
			return -1;

		mac_addr->addr_bytes[i] = byte;
		i++;
		token = strtok(NULL, ":");
	}

	return 0;
}

/*
 * Convert string to tci.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_tci(char *tci_str, void *output)
{
	char *end;
	rte_be16_t *tci = output;

	*tci = (rte_be16_t)strtoul(tci_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	return 0;
}

/*
 * Convert string to pcp.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_pcp(char *pcp_str, void *output)
{
	char *end;
	uint8_t *pcp = output;

	*pcp = (uint8_t)strtoul(pcp_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	/* 3bit check */
	if (*pcp > 0x7)
		return -1;

	return 0;
}

/*
 * Set PCP in TCI. TCI is a 16bits value and consists of 3bits PCP,
 * 1bit DEI and the rest 12bits VID.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
set_pcp_in_tci(char *pcp_str, void *output)
{
	int ret;
	uint8_t pcp = 0;
	rte_be16_t *tci = output;

	ret = str_to_pcp(pcp_str, &pcp);
	if (ret != 0)
		return -1;

	/* Assign to the first 3 bits */
	pcp = pcp << 1;
	((char *)tci)[0] = ((char *)tci)[0] | pcp;

	return 0;
}

/*
 * Set DEI in TCI. TCI is a 16bits value and consists of 3bits PCP,
 * 1bit DEI and the rest 12bits VID.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
set_dei_in_tci(char *dei_str, void *output)
{
	char *end;
	uint8_t dei = 0;
	rte_be16_t *tci = output;

	dei = (uint8_t)strtoul(dei_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	/* 1bit check */
	if (dei > 0x1)
		return -1;

	/* Assign to 4th bit */
	((char *)tci)[0] = ((char *)tci)[0] | dei;

	return 0;
}

/*
 * Set VID in TCI. TCI is a 16bits value and consists of 3bits PCP,
 * 1bit DEI and the rest 12bits VID.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
set_vid_in_tci(char *vid_str, void *output)
{
	char *end;
	rte_be16_t vid = 0;
	rte_be16_t *tci = output;

	vid = (rte_be16_t)strtoul(vid_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	/* 12bit check */
	if (vid > 0x0fff)
		return -1;

	/* Convert vid to big endian if system is little endian. */
	int i = 1;
	if (*(char *)&i) { /* check if little endian */
		uint8_t b1 = ((char *)&vid)[0];
		uint8_t b2 = ((char *)&vid)[1];
		((char *)&vid)[0] = b2;
		((char *)&vid)[1] = b1;
	}

	/* Assign to 5-16 bit */
	*tci = *tci | vid;

	return 0;
}

/*
 * Convert string to rte_be16_t.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_rte_be16_t(char *target_str, void *output)
{
	char *end;
	rte_be16_t *value = output;

	*value = (rte_be16_t)strtoul(target_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;
	/* Convert vid to big endian if system is little endian. */
	int i = 1;
	if (*(char *)&i) { /* check if little endian */
		uint8_t b1 = ((char *)value)[0];
		uint8_t b2 = ((char *)value)[1];
		((char *)value)[0] = b2;
		((char *)value)[1] = b1;
	}

	return 0;
}

/*
 * Convert string to uint16_t.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_uint16_t(char *target_str, void *output)
{
	char *end;
	uint16_t *value = output;

	*value = (uint16_t)strtoul(target_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	return 0;
}

/*
 * Convert string to uint32_t.
 * This function is intended to be called as a function pointer to
 * 'parse_detail' in 'struct flow_detail_ops'.
 */
int
str_to_uint32_t(char *target_str, void *output)
{
	char *end;
	uint32_t *value = output;

	*value = (uint32_t)strtoul(target_str, &end, 0);
	if (end == NULL || *end != '\0')
		return -1;

	return 0;
}

int
parse_rte_flow_item_field(char *token_list[], int *index,
	struct flow_detail_ops *detail_list, size_t size,
	void **spec, void **last, void **mask
	)
{
	int ret = 0;
	uint32_t prefix, i, j, bitmask;
	char *end, *target;

	if (!strcmp(token_list[*index], "is")) {
		/* Match value perfectly (with full bit-mask). */
		(*index)++;

		if (*spec == NULL) {
			ret = malloc_object(spec, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		ret = detail_list->parse_detail(token_list[*index],
			(char *)(*spec) + detail_list->offset);
		if (ret != 0) {
			RTE_LOG(ERR, SPP_FLOW,
				"parse_detail error (%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

		/* Set full bit-mask */
		if (*mask == NULL) {
			ret = malloc_object(mask, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		memset((char *)(*mask) + detail_list->offset, 0xff,
			detail_list->size);

	} else if (!strcmp(token_list[*index], "spec")) {
		/* Match value according to configured bit-mask. */
		(*index)++;

		if (*spec == NULL) {
			ret = malloc_object(spec, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		ret = detail_list->parse_detail(token_list[*index],
			(char *)(*spec) + detail_list->offset);
		if (ret != 0) {
			RTE_LOG(ERR, SPP_FLOW,
				"parse_detail error (%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

	} else if (!strcmp(token_list[*index], "last")) {
		/* Specify upper bound to establish a range. */
		(*index)++;

		if (*last == NULL) {
			ret = malloc_object(last, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		ret = detail_list->parse_detail(token_list[*index],
			(char *)(*last) + detail_list->offset);
		if (ret != 0) {
			RTE_LOG(ERR, SPP_FLOW,
				"parse_detail error (%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

	} else if (!strcmp(token_list[*index], "mask")) {
		/* Specify bit-mask with relevant bits set to one. */
		(*index)++;

		if (*mask == NULL) {
			ret = malloc_object(mask, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		ret = detail_list->parse_detail(token_list[*index],
			(char *)(*mask) + detail_list->offset);
		if (ret != 0) {
			RTE_LOG(ERR, SPP_FLOW,
				"parse_detail error (%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

	} else if (!strcmp(token_list[*index], "prefix")) {
		/*
		 * generate bit-mask with <prefix-length>
		 * most-significant bits set to one.
		 */
		(*index)++;

		if (*mask == NULL) {
			ret = malloc_object(mask, size);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Failed to alloc memory (%s:%d)\n",
					__func__, __LINE__);
				return -1;
			}
		}

		prefix = strtoul(token_list[*index], &end, 10);
		if (end == NULL || *end != '\0') {
			RTE_LOG(ERR, SPP_FLOW,
				"Prefix is not a number(%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

		/* Compare prefix (bit) and size (byte). */
		if (prefix > detail_list->size * 8) {
			RTE_LOG(ERR, SPP_FLOW,
				"Prefix value is too large(%s:%d)\n",
				__func__, __LINE__);
			return -1;
		}

		target = (char *)(*mask) + detail_list->offset;
		memset(target, 0, detail_list->size);
		for (i = 0; i < detail_list->size; i++) {
			if (prefix <= 0)
				break;

			bitmask = 0x80;

			for (j = 0; j < 8; j++) {
				if (prefix <= 0)
					break;

				target[i] = target[i] | bitmask;
				bitmask = bitmask >> 1;
				prefix--;
			}
		}

	} else {
		RTE_LOG(ERR, SPP_FLOW,
			"Invalid parameter is %s(%s:%d)\n",
			token_list[*index], __func__, __LINE__);
		return -1;
	}

	return 0;
}

int
parse_item_common(char *token_list[], int *index,
	struct rte_flow_item *item,
	struct flow_item_ops *ops)
{
	int ret = 0;
	int i = 0;
	//void *spec, *last, *mask;
	void *spec = NULL, *last = NULL, *mask = NULL;
	struct flow_detail_ops *detail_list = ops->detail_list;

	/* Next to pattern word */
	(*index)++;

	while (token_list[*index] != NULL) {

		/* Exit if "/" */
		if (!strcmp(token_list[*index], "/"))
			break;

		/* First is value type */
		i = 0;
		while (detail_list[i].token != NULL) {
			if (!strcmp(token_list[*index],
				detail_list[i].token)) {
				break;
			}

			i++;
		}
		if (detail_list[i].token == NULL) {
			RTE_LOG(ERR, SPP_FLOW,
				"Invalid \"%s\" pattern arguments(%s:%d)\n",
				ops->str_type, __func__, __LINE__);
			ret = -1;
			break;
		}

		/* Parse token value */
		if (detail_list[i].flg_value == 1) {
			(*index)++;

			ret = parse_rte_flow_item_field(token_list, index,
				&detail_list[i], ops->size,
				&spec, &last, &mask);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Invalid \"%s\" pattern arguments"
					"(%s:%d)\n",
					ops->str_type, __func__, __LINE__);
				ret = -1;
				break;
			}
		}

		(*index)++;
	}

	/* Free memory allocated in case of failure. */
	if (ret != 0) {
		if (spec != NULL)
			free(spec);

		if (last != NULL)
			free(last);

		if (mask != NULL)
			free(mask);
	}

	/* Parse result to item. */
	item->spec = spec;
	item->last = last;
	item->mask = mask;

	return ret;
}

int
parse_action_common(char *token_list[], int *index,
	struct rte_flow_action *action,
	struct flow_action_ops *ops)
{
	int ret = 0;
	int i = 0;
	struct rte_flow_action_queue  *conf;
	struct flow_detail_ops *detail_list = ops->detail_list;

	conf = malloc(ops->size);
	if (conf == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"Memory allocation failure(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}
	memset(conf, 0, ops->size);

	/* Next to word */
	(*index)++;

	while (token_list[*index] != NULL) {

		/* Exit if "/" */
		if (!strcmp(token_list[*index], "/"))
			break;

		/* First is value type */
		i = 0;
		while (detail_list[i].token != NULL) {
			if (!strcmp(token_list[*index],
				detail_list[i].token)) {
				break;
			}

			i++;
		}
		if (detail_list[i].token == NULL) {
			RTE_LOG(ERR, SPP_FLOW,
				"Invalid \"%s\" pattern arguments(%s:%d)\n",
				ops->str_type, __func__, __LINE__);
			ret = -1;
			break;
		}

		/* Parse token value */
		if (detail_list[i].flg_value == 1) {
			(*index)++;

			ret = detail_list[i].parse_detail(
				token_list[*index],
				(char *)conf + detail_list[i].offset);
			if (ret != 0) {
				RTE_LOG(ERR, SPP_FLOW,
					"Invalid \"%s\" pattern arguments"
					"(%s:%d)\n",
					detail_list[i].token,
					__func__, __LINE__);
				ret = -1;
				break;
			}
		}

		(*index)++;
	}

	/* Free memory allocated in case of failure. */
	if ((ret != 0) && (conf != NULL))
		free(conf);

	/* Parse result to action. */
	action->conf = conf;

	return ret;
}

/* Append action json, conf field is null */
int
append_action_null_json(const void *conf __attribute__ ((unused)),
	int buf_size, char *action_str)
{
	char null_str[] = "null";

	if ((int)strlen(action_str) + (int)strlen(null_str)
		> buf_size)
		return -1;

	strncat(action_str, null_str, strlen(null_str));

	return 0;
}

int
malloc_object(void **ptr, size_t size)
{
	*ptr = malloc(size);
	if (*ptr == NULL) {
		RTE_LOG(ERR, SPP_FLOW,
			"Memory allocation failure(%s:%d)\n",
			__func__, __LINE__);
		return -1;
	}

	memset(*ptr, 0, size);
	return 0;
}
