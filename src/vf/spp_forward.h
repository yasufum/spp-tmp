/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Nippon Telegraph and Telephone Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Nippon Telegraph and Telephone Corporation
 *       nor the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_forward_update(struct spp_component_info *component);

/**
 * Merge/Forward
 *
 * @param id
 *  The unique component ID.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
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
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_forward_get_component_status(
		unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

#endif /* __SPP_FORWARD_H__ */
