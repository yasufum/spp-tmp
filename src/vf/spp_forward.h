/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_FORWARD_H__
#define __SPP_FORWARD_H__

/**
 * @file
 * SPP Forwarder and Merger
 *
 * Forwarder
 * This component provides function for packet processing from one port
 * to one port. Incoming packets from port are to be transferred to
 * specific one port. The direction of this transferring is specified
 * by port command.
 * Merger
 * This component provides packet forwarding function from multiple
 * ports to one port. Incoming packets from multiple ports are to be
 * transferred to one specific port. The flow of this merging process
 * is specified by port command.
 */

/** Clear info */
void spp_forward_init(void);

/**
 * Update forward info
 *
 * @param component
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of forwarder and merger.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_forward_update(struct spp_component_info *component);

/**
 * Merge/Forward
 *
 * @param id
 *  The unique component ID.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_forward(int id);

/**
 * Merge/Forward get component status
 *
 * @param lcore_id
 *  The logical core ID for forwarder and merger.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of forwarder/merger status.
 *
 * @retval SPP_RET_OK succeeded.
 * @retval SPP_RET_NG failed.
 */
int spp_forward_get_component_status(
		unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif /* __SPP_FORWARD_H__ */
