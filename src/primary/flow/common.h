/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_FLOW_COMMON_H_
#define _PRIMARY_FLOW_COMMON_H_

int is_portid_used(int port_id);

/* Function for flow command parse */
int parse_phy_port_id(char *res_uid, int *port_id);

/* Functions for converting string to data */
int str_to_rte_ether_addr(char *mac_str, void *output);
int str_to_tci(char *tci_str, void *output);
int str_to_pcp(char *pcp_str, void *output);
int str_to_rte_be16_t(char *target_str, void *output);
int str_to_uint16_t(char *target_str, void *output);
int str_to_uint32_t(char *target_str, void *output);

/* Functions for setting string to data */
int set_pcp_in_tci(char *pcp_str, void *output);
int set_dei_in_tci(char *dei_str, void *output);
int set_vid_in_tci(char *vid_str, void *output);

/* Parse rte_flow_item for each field */
int parse_rte_flow_item_field(char *token_list[], int *index,
	struct flow_detail_ops *detail_list, size_t size,
	void **spec, void **last, void **mask);

/*
 * Common parse for item type. Perform detailed parse with
 * flow_detail_ops according to type.
 */
int parse_item_common(char *token_list[], int *index,
	struct rte_flow_item *item,
	struct flow_item_ops *ops);

/*
 * Common parse for action type. Perform detailed parse with
 * flow_detail_ops according to type.
 */
int parse_action_common(char *token_list[], int *index,
	struct rte_flow_action *action,
	struct flow_action_ops *ops);

/* Append action json, conf field is null */
int append_action_null_json(const void *conf, int buf_size, char *action_str);

/* Allocate memory for the size */
int malloc_object(void **ptr, size_t size);

#endif
