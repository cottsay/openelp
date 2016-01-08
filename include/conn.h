/*!
 * @file conn.h
 *
 * @section LICENSE
 *
 * Copyright &copy; 2016, Scott K Logan
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of OpenELP nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan <logans@cottsay.net>
 */

#ifndef _conn_h
#define _conn_h

#include <stdint.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

enum CONN_TYPE
{
	CONN_TYPE_TCP,
	CONN_TYPE_UDP,
};

struct conn_handle
{
	void *priv;
	const char *source_port;
	const char *source_addr;
	enum CONN_TYPE type;
};

int conn_init(struct conn_handle *conn);
void conn_free(struct conn_handle *conn);
int conn_listen(struct conn_handle *conn);
int conn_accept(struct conn_handle *conn, struct conn_handle *accepted);
int conn_connect(struct conn_handle *conn, const char *addr, const char *port);
int conn_recv(struct conn_handle *conn, uint8_t *buff, size_t buff_len);
int conn_recv_any(struct conn_handle *conn, uint8_t *buff, size_t buff_len, uint32_t *addr, uint16_t *port);
int conn_send(struct conn_handle *conn, const uint8_t *buff, size_t buff_len);
int conn_send_to(struct conn_handle *conn, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port);
void conn_drop(struct conn_handle *conn);
void conn_close(struct conn_handle *conn);
void conn_shutdown(struct conn_handle *conn);

#endif /* _conn_h */
