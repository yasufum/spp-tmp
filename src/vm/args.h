/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 */

#ifndef _ARGS_H_
#define _ARGS_H_

#include <stdint.h>

extern uint16_t client_id;
extern char *server_ip;
extern int server_port;

int parse_app_args(uint16_t max_ports, int argc, char *argv[]);

#endif /* ifndef _ARGS_H_ */
