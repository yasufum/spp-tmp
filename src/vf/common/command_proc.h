/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _COMMAND_PROC_H_
#define _COMMAND_PROC_H_

/**
 * @file
 * SPP Command processing
 *
 * Receive and process the command message, then send back the
 * result JSON formatted data.
 */

/**
 * initialize command processor.
 *
 * @param controller_ip
 *  The controller's ip address.
 * @param controller_port
 *  The controller's port number.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int
spp_command_proc_init(const char *controller_ip, int controller_port);

/**
 * process command from controller.
 *
 * @retval 0  succeeded.
 * @retval -1 process termination is required.
 *            (occurred connection failure, or received exit command)
 */
int
spp_command_proc_do(void);

#endif /* _COMMAND_PROC_H_ */
