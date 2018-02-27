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

#ifndef _CLASSIFIER_MAC_H_
#define _CLASSIFIER_MAC_H_

/**
 * @file
 * SPP Classifier
 *
 * Classifier component provides packet forwarding function from
 * one port to one port. Classifier has table of virtual MAC address.
 * According to this table, classifier lookups L2 destination MAC address
 * and determines which port to be transferred to incoming packets.
 */

/* forward declaration */
struct spp_component_info;
struct spp_iterate_classifier_table_params;

/**
 * classifier(mac address) initialize globals.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_init(void);

/**
 * classifier(mac address) update component info.
 *
 * @param component_info
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of classifier.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_update(struct spp_component_info *component_info);

/**
 * classifier(mac address) thread function.
 *
 * @param id
 *  The unique component ID.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_do(int id);

/**
 * classifier get component status.
 *
 *
 * @param lcore_id
 *  The logical core ID for classifier.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of classifier status.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int
spp_classifier_get_component_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

/**
 * classifier(mac address) iterate classifier table.
 *
 * @param params
 *  Point to struct spp_iterate_classifier_table_params.@n
 *  Detailed data of classifier table.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_iterate_table(
		struct spp_iterate_classifier_table_params *params);

#endif /* _CLASSIFIER_MAC_H_ */
