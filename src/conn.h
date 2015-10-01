/*!
 * @file conn.h
 *
 * @section LICENSE
 *
 * Copyright &copy; 2015, Scott K Logan
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

#ifndef _openelink_conn_h
#define _openelink_conn_h

#include <stdint.h>
#include <unistd.h>

enum CONN_TYPE
{
	CONN_TYPE_TCP,
	CONN_TYPE_UDP,
};

struct proxy_conn
{
	void *priv;
	enum CONN_TYPE type;
};

int conn_init(struct proxy_conn *pc);
void conn_free(struct proxy_conn *pc);
int conn_listen(struct proxy_conn *pc, uint16_t port);
int conn_listen_wait(struct proxy_conn *pc);
int conn_connect(struct proxy_conn *pc, uint32_t addr, uint16_t port);
int conn_recv(struct proxy_conn *pc, uint8_t *buff, size_t buff_len);
int conn_recv_any(struct proxy_conn *pc, uint8_t *buff, size_t buff_len, uint32_t *addr);
int conn_send(struct proxy_conn *pc, const uint8_t *buff, size_t buff_len);
int conn_send_to(struct proxy_conn *pc, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port);
void conn_drop(struct proxy_conn *pc);
void conn_close(struct proxy_conn *pc);
void conn_shutdown(struct proxy_conn *pc);

#endif /* _openelink_conn_h */
