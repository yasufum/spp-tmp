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

#ifndef _STRING_BUFFER_H_
#define _STRING_BUFFER_H_

/**
 * @file
 * SPP String buffer management
 *
 * Management features of string buffer which is used for communicating
 * between spp_vf and controller.
 */

/**
 * allocate string buffer from heap memory.
 *
 * @attention allocated memory must free by spp_strbuf_free function.
 *
 * @param capacity
 *  initial buffer size (include null char).
 *
 * @retval not-NULL pointer to the allocated memory.
 * @retval NULL     error.
 */
char *spp_strbuf_allocate(size_t capacity);

/**
 * free string buffer.
 *
 * @param strbuf
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 */
void spp_strbuf_free(char *strbuf);

/**
 * append string to buffer.
 *
 * @param strbuf
 *  destination string buffer.
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 * @param append
 *  string to append. normal c-string.
 * @param append_len
 *  length of append string.
 *
 * @return if "strbuf" has enough space to append, returns "strbuf"
 *         else returns a new pointer to the allocated memory.
 */
char *spp_strbuf_append(char *strbuf, const char *append, size_t append_len);

/**
 * remove string from front.
 *
 * @param strbuf
 *  target string buffer.
 *  spp_strbuf_allocate/spp_strbuf_append return value.
 * @param remove_len
 *  length of remove.
 *
 * @return
 *  The pointer to removed string.
 */
char *spp_strbuf_remove_front(char *strbuf, size_t remove_len);

#endif /* _STRING_BUFFER_H_ */
