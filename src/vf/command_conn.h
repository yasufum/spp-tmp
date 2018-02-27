/*-
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

#ifndef _COMMAND_CONN_H_
#define _COMMAND_CONN_H_

/**
 * @file
 * SPP Connection
 *
 * Command connection management.
 */

/** result code - temporary error. please retry */
#define SPP_CONNERR_TEMPORARY -1
/** result code - fatal error occurred. should terminate process. */
#define SPP_CONNERR_FATAL     -2

/**
 * initialize command connection.
 *
 * @param controller_ip
 *  The controller's ip address.
 * @param controller_port
 *  The controller's port number.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_command_conn_init(const char *controller_ip, int controller_port);

/**
 * connect to controller.
 *
 * @note bocking.
 *
 * @param sock
 *  Socket number for connecting to controller.
 *
 * @retval 0                     succeeded.
 * @retval SPP_CONNERR_TEMPORARY temporary error. please retry.
 */
int spp_connect_to_controller(int *sock);

/**
 * receive message.
 *
 * @note non-blocking.
 *
 * @param sock
 *  The socket number for the connection.
 * @param msgbuf
 *  The pointer to command message buffer.
 *
 * @retval 0 <                   succeeded. number of bytes received.
 * @retval 0                     no receive message.
 * @retval SPP_CONNERR_TEMPORARY temporary error. please reconnect.
 * @retval SPP_CONNERR_FATAL     fatal error occurred. should terminate process.
 */
int spp_receive_message(int *sock, char **msgbuf);

/**
 * send message.
 *
 * @note non-blocking.
 *
 * @param sock
 *  The socket number to be sent.
 * @param message
 *  The pointer to the message to be sent.
 * @param message_len
 *  The length of message.
 *
 * @retval 0                     succeeded.
 * @retval SPP_CONNERR_TEMPORARY temporary error. please reconnect.
 */
int spp_send_message(int *sock, const char *message, size_t message_len);

#endif /* _COMMAND_CONN_H_ */
