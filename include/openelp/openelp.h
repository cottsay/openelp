/*!
 * @file openelp.h
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
 *
 * @brief Public API for OpenELP, An Open Source EchoLink&reg; Proxy
 *
 * @section DESCRIPTION
 *
 * These definitions, data structures and functions manipulate and operate the
 * proxy.
 */
#ifndef _openelp_h
#define _openelp_h

#include <stdint.h>
#include <unistd.h>

/*!
 * @brief Length in bytes of the expected password response from the client
 */
#define PROXY_PASS_RES_LEN 16

/*!
 * @brief Message types used in communication between the proxy and the client
 */
enum PROXY_MSG_TYPE
{
	/*!
	 * @brief The proxy should open a new TCP connection
	 *
	 * * Sent by: client
	 * * Expected data: 0 bytes
	 */
	PROXY_MSG_TYPE_TCP_OPEN = 1,

	/*!
	 * @brief Data which has been sent or should be sent over the TCP connection
	 *
	 * The address field is ignored in this message
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_TCP_DATA,

	/*!
	 * @brief The TCP has been, or should be closed
	 *
	 * The address fieled is ignored in this message
	 *
	 * When the client requests that the TCP connection be closed, the proxy
	 * responds with another ::PROXY_MSG_TYPE_TCP_CLOSE message
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 0 bytes
	 */
	PROXY_MSG_TYPE_TCP_CLOSE,

	/*!
	 * @brief The status of the TCP connection
	 *
	 * The address field is ignored in this message
	 *
	 * The data included with this message should be zeroed when the TCP connection
	 * was opened successfully, and non-zero otherwise
	 *
	 * * Sent by: proxy
	 * * Expected data: 4 bytes
	 */
	PROXY_MSG_TYPE_TCP_STATUS,

	/*!
	 * @brief Data which has been or should be sent of the UDP Data connection
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_UDP_DATA,

	/*!
	 * @brief Data which has been or should be sent of the UDP Control connection
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_UDP_CONTROL,

	/*!
	 * @brief Proxy system information
	 *
	 * The contents of this message are a single ::SYSTEM_MSG
	 *
	 * * Sent by: proxy
	 * * Expected data: 1 byte
	 */
	PROXY_MSG_TYPE_SYSTEM,
};

/*!
 * @brief System messages sent by the proxy to the client
 */
enum SYSTEM_MSG
{
	/*!
	 * @brief The client has supplied the proxy with an incorrect password
	 */
	SYSTEM_MSG_BAD_PASSWORD = 1,

	/*!
	 * @brief The client's callsign is not allowed to use the proxy
	 */
	SYSTEM_MSG_ACCESS_DENIED,
};

/*!
 * @brief OpenELP Status
 */
enum PROXY_STATUS
{
	/*!
	 * @brief The proxy is completely uninitialized
	 */
	PROXY_STATUS_NONE = 0,

	/*!
	 * @brief The proxy is initialized but is not currently listening a client
	 */
	PROXY_STATUS_DOWN,

	/*!
	 * @brief The proxy is listening for a client, and none are connected
	 */
	PROXY_STATUS_AVAILABLE,

	/*!
	 * @brief There is a single client connected to the proxy
	 */
	PROXY_STATUS_IN_USE,

	/*!
	 * @brief The proxy is in the process of shutting down
	 */
	PROXY_STATUS_SHUTDOWN,
};

struct proxy_msg
{
	uint8_t type;
	uint32_t address;
	uint32_t size;
	uint8_t data[];
} __attribute__((packed));

struct proxy_handle
{
	const char *config_path;
	uint8_t status;
	void *priv;
};

/*
 * Resource Management
 */
int proxy_init(struct proxy_handle *ph);
void proxy_free(struct proxy_handle *ph);
int proxy_open(struct proxy_handle *ph);
void proxy_close(struct proxy_handle *ph);
void proxy_drop(struct proxy_handle *ph);
int proxy_process(struct proxy_handle *ph);
void proxy_handle_client_error(struct proxy_handle *ph, int ret);
void proxy_shutdown(struct proxy_handle *ph);

/*
 * Message Processing
 */
int process_msg(struct proxy_handle *proxy, struct proxy_msg *msg, size_t len);
int process_system_msg(struct proxy_handle *proxy, struct proxy_msg *msg);
int process_tcp_open_msg(struct proxy_handle *proxy, struct proxy_msg *msg);
int process_tcp_data_msg(struct proxy_handle *proxy, struct proxy_msg *msg, size_t len);
int process_tcp_close_msg(struct proxy_handle *proxy, struct proxy_msg *msg);
int process_tcp_status_msg(struct proxy_handle *proxy, struct proxy_msg *msg);
int process_udp_data_msg(struct proxy_handle *proxy, struct proxy_msg *msg, size_t len);
int process_udp_control_msg(struct proxy_handle *proxy, struct proxy_msg *msg, size_t len);

/*
 * Messages
 */
int send_system(struct proxy_handle *ph, enum SYSTEM_MSG msg);
int send_tcp_close(struct proxy_handle *ph);
int send_tcp_status(struct proxy_handle *ph, uint32_t status);

/*
 * Helpers
 */
int get_nonce(uint32_t *nonce);
int get_password_response(const uint32_t nonce, const char *password, uint8_t response[PROXY_PASS_RES_LEN]);

#endif /* _openelp_h */
