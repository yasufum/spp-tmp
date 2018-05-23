/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#ifndef _INIT_H_
#define _INIT_H_

#include <stdint.h>

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;

int init(int argc, char *argv[]);

#endif /* ifndef _INIT_H_ */
