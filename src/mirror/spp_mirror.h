/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_MIRROR_H__
#define __SPP_MIRROR_H__

#include "shared/secondary/spp_worker_th/spp_proc.h"

/**
 * @file
 * SPP_MIRROR main
 *
 * Main function of spp_mirror.
 * This provides the function for initializing and starting the threads.
 *
 * There is two kinds of reproduction classification. I choose it by a
 * compilation switch.
 *  -DeepCopy
 *  -ShallowCopy
 *
 * Attention
 *  I do not do the deletion of the VLAN tag, the addition.
 */

#endif /* __SPP_MIRROR_H__ */
