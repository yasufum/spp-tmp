/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_PCAP_H__
#define __SPP_PCAP_H__

#include "cmd_utils.h"

/**
 * Pcap get core status
 *
 * @param lcore_id The logical core ID for forwarder and merger.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of pcap status.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int spp_pcap_get_core_status(
		unsigned int lcore_id,
		struct spp_iterate_core_params *params);

#endif /* __SPP_PCAP_H__ */
