/*!
 * @file proxy_conn.c
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

#include "openelp/openelp.h"

#include "conn.h"
#include "mutex.h"
#include "proxy_conn.h"
#include "rand.h"
#include "thread.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// It seems that the official client can't handle messages from proxies which
// are larger than 4096 or so
#define CONN_BUFF_LEN 4096
#define CONN_BUFF_LEN_HEADERLESS CONN_BUFF_LEN - sizeof(struct proxy_msg)

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

#ifdef _WIN32
#  pragma pack(push,1)
#endif
struct proxy_msg
{
	uint8_t type;
	uint32_t address;
	uint32_t size;
	uint8_t data[];
#ifdef _WIN32
};
#  pragma pack(pop)
#else
} __attribute__((packed));
#endif

struct proxy_conn_priv
{
	char callsign[12];

	struct condvar_handle condvar_client;

	struct conn_handle *conn_client;
	struct conn_handle conn_control;
	struct conn_handle conn_data;
	struct conn_handle conn_tcp;

	struct mutex_handle mutex_client_send;
	struct mutex_handle mutex_sentinel;

	uint8_t sentinel;

	struct thread_handle thread_client;
	struct thread_handle thread_control;
	struct thread_handle thread_data;
	struct thread_handle thread_tcp;
};

/*
 * Static Functions
 */

static int client_authorize(struct proxy_conn_handle *pc);
static void * client_manager(void *ctx);
static void * forwarder_control(void *ctx);
static void * forwarder_data(void *ctx);
static void * forwarder_tcp(void *ctx);
static int process_control_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int process_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int process_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int process_tcp_close_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int process_tcp_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int process_tcp_open_message(struct proxy_conn_handle *pc, struct proxy_msg *msg);
static int send_system(struct proxy_conn_handle *pc, enum SYSTEM_MSG msg);
static int send_tcp_close(struct proxy_conn_handle *pc);

static int client_authorize(struct proxy_conn_handle *pc)
{
	uint8_t buff[28];
	size_t idx, j;
	uint32_t nonce;
	char nonce_str[9];
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	uint8_t response[PROXY_PASS_RES_LEN];
	int ret;

	ret = get_nonce(&nonce);
	if (ret < 0)
	{
		return ret;
	}

	ret = snprintf(nonce_str, 9, "%08x", nonce);
	if (ret != 8)
	{
		return -EINVAL;
	}

	// Generate the expected auth response
	ret = get_password_response(nonce, pc->ph->conf.password, response);
	if (ret < 0)
	{
		return ret;
	}

	// Send the nonce
	ret = conn_send(priv->conn_client, (uint8_t *)nonce_str, 8);
	if (ret < 0)
	{
		return ret;
	}

	// We can expect to receive a newline-terminated callsign and a 16-byte
	// password response.
	// Since this is variable-length, initially look only for 16 bytes. The
	// callsign will be part of that, and we can figure out how much we're
	// missing.
	ret = conn_recv(priv->conn_client, buff, 16);
	if (ret < 0)
	{
		return ret;
	}

	for (idx = 0; idx < 11 && buff[idx] != '\n'; idx++);

	if (idx >= 11)
	{
		return -EINVAL;
	}

	strncpy(priv->callsign, (char *)buff, idx);

	ret = conn_recv(priv->conn_client, &buff[16], idx + 1);
	if (ret < 0)
	{
		return ret;
	}

	for (idx += 1, j = 0; j < PROXY_PASS_RES_LEN; idx++, j++)
	{
		if (response[j] != buff[idx])
		{
			proxy_log(pc->ph, LOG_LEVEL_INFO, "Client '%s' supplied an incorrect password. Dropping...\n", priv->callsign);

			ret = send_system(pc, SYSTEM_MSG_BAD_PASSWORD);

			return ret < 0 ? ret : -EACCES;
		}
	}

	// TODO: Verify the callsign before accepting the connection

	return 0;
}

static void * client_manager(void *ctx)
{
	struct thread_handle *th = (struct thread_handle *)ctx;
	struct proxy_conn_handle *pc = (struct proxy_conn_handle *)th->func_ctx;
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	int ret;
	uint8_t buff[CONN_BUFF_LEN];

	while (1)
	{
		mutex_lock(&priv->mutex_sentinel);

		if (priv->conn_client != NULL)
		{
			conn_close(priv->conn_client);
			conn_free(priv->conn_client);
			free(priv->conn_client);
			priv->conn_client = NULL;
		}

		if (priv->sentinel != 0)
		{
			break;
		}

		condvar_wait(&priv->condvar_client, &priv->mutex_sentinel);

		if (priv->sentinel != 0)
		{
			break;
		}
		else if (priv->conn_client == NULL)
		{
			proxy_log(pc->ph, LOG_LEVEL_ERROR, "New connection was signaled, but no connection was given\n");
			continue;
		}

		mutex_unlock(&priv->mutex_sentinel);

		proxy_log(pc->ph, LOG_LEVEL_DEBUG, "New connection - beginning authorization procedure\n");

		ret = client_authorize(pc);
		if (ret < 0)
		{
			switch (ret)
			{
			case -ECONNRESET:
			case -EINTR:
			case -ENOTCONN:
			case -EPIPE:
				proxy_log(pc->ph, LOG_LEVEL_WARN, "Connection to client was lost before authorization could complete\n");
			case -EACCES:
				break;
			default:
				proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to connect to client (%d): %s\n", -ret, strerror(-ret));
			}

			conn_drop(priv->conn_client);

			continue;
		}

		ret = conn_listen(&priv->conn_control, 5199);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to open UDP control port (5199). Dropping...\n");

			conn_drop(priv->conn_client);

			continue;
		}

		ret = conn_listen(&priv->conn_data, 5198);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to open UDP data port (5198). Dropping...\n");

			conn_close(&priv->conn_control);

			conn_drop(priv->conn_client);

			continue;
		}

		ret = thread_start(&priv->thread_control);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to start UDP control forwarder. Dropping...\n");

			conn_close(&priv->conn_control);
			conn_close(&priv->conn_data);

			conn_drop(priv->conn_client);

			continue;
		}

		ret = thread_start(&priv->thread_data);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to start UDP data forwarder. Dropping...\n");

			conn_close(&priv->conn_control);
			conn_close(&priv->conn_data);

			thread_join(&priv->thread_control);

			conn_drop(priv->conn_client);

			continue;
		}

		proxy_log(pc->ph, LOG_LEVEL_INFO, "Connected to client '%s'.\n", priv->callsign);

		// DO STUFF
		while (1)
		{
			ret = conn_recv(priv->conn_client, buff, sizeof(struct proxy_msg));
			if (ret < 0)
			{
				switch (ret)
				{
				case -ECONNRESET:
				case -EINTR:
				case -ENOTCONN:
				case -EPIPE:
					break;
				default:
					proxy_log(pc->ph, LOG_LEVEL_ERROR, "Failed to receive data from client '%s' (%d): %s\n", priv->callsign, -ret, strerror(-ret));
					break;
				}

				break;
			}

			ret = process_message(pc, (struct proxy_msg *)buff);
			if (ret < 0)
			{
				break;
			}
		}

		proxy_log(pc->ph, LOG_LEVEL_INFO, "Disconnected from client '%s'.\n", priv->callsign);

		conn_close(&priv->conn_control);
		conn_close(&priv->conn_data);
		conn_close(&priv->conn_tcp);

		thread_join(&priv->thread_data);
		thread_join(&priv->thread_control);
		thread_join(&priv->thread_tcp);

		conn_drop(priv->conn_client);
	}

	mutex_unlock(&priv->mutex_sentinel);

	return NULL;
}

static void * forwarder_control(void *ctx)
{
	struct thread_handle *th = (struct thread_handle *)ctx;
	struct proxy_conn_handle *pc = (struct proxy_conn_handle *)th->func_ctx;
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

	uint8_t buf[CONN_BUFF_LEN] = { 0x0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_UDP_CONTROL;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "UDP Control forwarding thread is starting for client '%s'\n", priv->callsign);

	do
	{
		ret = conn_recv_any(&priv->conn_control, msg->data, CONN_BUFF_LEN_HEADERLESS, &msg->address);
		if (ret > 0)
		{
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending UDP_DATA message to client '%s' (%d bytes)\n", priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg, sizeof(struct proxy_msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			// This is an error with the client connection
			if (ret < 0)
			{
				conn_close(&priv->conn_control);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' UDP Control thread is returning due to a client connection error (%d): %s\n", priv->callsign, -ret, strerror(-ret));

				switch (ret)
				{
				case -ECONNRESET:
				case -EINTR:
				case -ENOTCONN:
				case -EPIPE:
					break;
				default:
					proxy_conn_drop(pc);
					break;
				}

				return NULL;
			}
		}
		else if (ret == 0)
		{
			ret = -EPIPE;
		}
	}
	while (ret >= 0);

	switch (ret)
	{
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_INFO, "Failed to receive data on client '%s' UDP Control connection (%d): %s\n", priv->callsign, -ret, strerror(-ret));
		// Since the UDP ports must be open while the client is connected,
		// we should shut down the client if we don't exit cleanly
		proxy_conn_drop(pc);
		break;
	}

	conn_close(&priv->conn_control);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' UDP Control thread is returning cleanly\n", priv->callsign);

	return NULL;
}

static void * forwarder_data(void *ctx)
{
	struct thread_handle *th = (struct thread_handle *)ctx;
	struct proxy_conn_handle *pc = (struct proxy_conn_handle *)th->func_ctx;
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

	uint8_t buf[CONN_BUFF_LEN] = { 0x0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_UDP_DATA;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "UDP Data forwarding thread is starting for client '%s'\n", priv->callsign);

	do
	{
		ret = conn_recv_any(&priv->conn_data, msg->data, CONN_BUFF_LEN_HEADERLESS, &msg->address);
		if (ret > 0)
		{
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending UDP_DATA message to client '%s' (%d bytes)\n", priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg, sizeof(struct proxy_msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			// This is an error with the client connection
			if (ret < 0)
			{
				conn_close(&priv->conn_data);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' UDP Data thread is returning due to a client connection error (%d): %s\n", priv->callsign, -ret, strerror(-ret));

				switch (ret)
				{
				case -ECONNRESET:
				case -EINTR:
				case -ENOTCONN:
				case -EPIPE:
					break;
				default:
					proxy_conn_drop(pc);
					break;
				}

				return NULL;
			}
		}
		else if (ret == 0)
		{
			ret = -EPIPE;
		}
	}
	while (ret >= 0);

	switch (ret)
	{
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_INFO, "Failed to receive data on client '%s' UDP Data connection (%d): %s\n", priv->callsign, -ret, strerror(-ret));
		// Since the UDP ports must be open while the client is connected,
		// we should shut down the client if we don't exit cleanly
		proxy_conn_drop(pc);
		break;
	}

	conn_close(&priv->conn_data);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' UDP Data thread is returning cleanly\n", priv->callsign);

	return NULL;
}

static void * forwarder_tcp(void *ctx)
{
	struct thread_handle *th = (struct thread_handle *)ctx;
	struct proxy_conn_handle *pc = (struct proxy_conn_handle *)th->func_ctx;
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

	uint8_t buf[CONN_BUFF_LEN] = { 0x0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_TCP_DATA;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "TCP forwarding thread is starting for client '%s'\n", priv->callsign);

	do
	{
		ret = conn_recv_any(&priv->conn_tcp, msg->data, CONN_BUFF_LEN_HEADERLESS, NULL);
		if (ret > 0)
		{
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending TCP_DATA message to client '%s' (%d bytes)\n", priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg, sizeof(struct proxy_msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			// This is an error with the client connection
			if (ret < 0)
			{
				conn_close(&priv->conn_tcp);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' TCP thread is returning due to a client connection error (%d): %s\n", priv->callsign, -ret, strerror(-ret));

				switch (ret)
				{
				case -ECONNRESET:
				case -EINTR:
				case -ENOTCONN:
				case -EPIPE:
					break;
				default:
					proxy_conn_drop(pc);
					break;
				}

				return NULL;
			}
		}
		else if (ret == 0)
		{
			ret = -EPIPE;
		}
	}
	while (ret >= 0);

	switch (ret)
	{
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_WARN, "Failed to receive data on client '%s' TCP connection (%d): %s\n", priv->callsign, -ret, strerror(-ret));
		break;
	}

	conn_close(&priv->conn_tcp);

	send_tcp_close(pc);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Client '%s' TCP thread is returning cleanly\n", priv->callsign);

	return NULL;
}

static int process_control_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Processing UDP_CONTROL message (%d bytes) from client '%s'\n", msg_size, priv->callsign);

	while (msg_size > 0)
	{
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
		{
			return ret;
		}

		msg_size -= ret;

		// Send the data
		ret = conn_send_to(&priv->conn_control, (void *)msg, ret, addr, 5199);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_WARN, "Failed to send UDP_CONTROL packet of size %zu to client '%s': %d (%s)\n", curr_msg_size, priv->callsign, -ret, strerror(-ret));
			// Drop?
		}
	}

	return 0;
}

static int process_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Processing UDP_DATA message (%d bytes) from client '%s'\n", msg_size, priv->callsign);

	while (msg_size > 0)
	{
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
		{
			return ret;
		}

		msg_size -= ret;

		// Send the data
		ret = conn_send_to(&priv->conn_data, (void *)msg, ret, addr, 5198);
		if (ret < 0)
		{
			proxy_log(pc->ph, LOG_LEVEL_WARN, "Failed to send UDP_DATA packet of size %zu to client '%s': %d (%s)\n", curr_msg_size, priv->callsign, -ret, strerror(-ret));
			// Drop?
		}
	}

	return 0;
}

// This should return non-zero for conn_client errors only
static int process_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	switch(msg->type)
	{
	case PROXY_MSG_TYPE_TCP_OPEN:
		return process_tcp_open_message(pc, msg);
	case PROXY_MSG_TYPE_TCP_DATA:
		return process_tcp_data_message(pc, msg);
	case PROXY_MSG_TYPE_TCP_CLOSE:
		return process_tcp_close_message(pc, msg);
	case PROXY_MSG_TYPE_UDP_DATA:
		return process_data_message(pc, msg);
	case PROXY_MSG_TYPE_UDP_CONTROL:
		return process_control_data_message(pc, msg);
	default:
		proxy_log(pc->ph, LOG_LEVEL_ERROR, "Invalid data received from client (beginning with %02x)\n", msg->type);
		return -EINVAL;
	}
}

static int process_tcp_close_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Processing TCP_CLOSE message from client '%s'\n", priv->callsign);

	conn_close(&priv->conn_tcp);

	return 0;
}

static int process_tcp_data_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	size_t msg_size = msg->size;
	size_t curr_msg_size;
	int tcp_ret = 0;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Processing TCP_DATA message (%d bytes) from client '%s'\n", msg_size, priv->callsign);

	while (msg_size > 0)
	{
		curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
		{
			return ret;
		}

		msg_size -= ret;

		// Send the data
		if (tcp_ret == 0)
		{
			tcp_ret = conn_send(&priv->conn_tcp, (void *)msg, ret);
			if (tcp_ret < 0)
			{
				conn_close(&priv->conn_tcp);
			}
		}
	}

	if (tcp_ret != 0)
	{
		send_tcp_close(pc);
	}

	return 0;
}

static int process_tcp_open_message(struct proxy_conn_handle *pc, struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	uint8_t status_buf[sizeof(struct proxy_msg) + 4] = { 0x0 };
	struct proxy_msg *status_msg = (struct proxy_msg *)status_buf;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Processing TCP_OPEN message from client '%s'\n", priv->callsign);

	// Attempt to connect
	ret = conn_connect(&priv->conn_tcp, msg->address, 5200);
	if (ret < 0)
	{
		proxy_log(pc->ph, LOG_LEVEL_WARN, "Failed to open TCP connection for client '%s'\n", priv->callsign);
	}
	else
	{
		// Connection succeeded - start the thread
		ret = thread_start(&priv->thread_tcp);
		if (ret < 0)
		{
			conn_close(&priv->conn_tcp);
		}
	}

	status_msg->type = PROXY_MSG_TYPE_TCP_STATUS;
	status_msg->size = 4;

	// Unless we can figure out what the client is expecting here, the
	// best we can do is a "non-zero" value to indicate failure.
	memcpy(status_msg->data, &ret, 4);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending TCP_STATUS message (%d) to client '%s'\n", ret, priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, status_buf, sizeof(struct proxy_msg) + status_msg->size);

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

static int send_system(struct proxy_conn_handle *pc, enum SYSTEM_MSG msg)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	uint8_t buf[sizeof(struct proxy_msg) + 1] = { 0x0 };
	struct proxy_msg *message = (struct proxy_msg *)buf;
	int ret;

	message->type = PROXY_MSG_TYPE_SYSTEM;
	message->size = 1;
	message->data[0] = msg;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending SYSTEM message (%d) to client '%s'\n", msg, priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, buf, sizeof(struct proxy_msg) + message->size);

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

static int send_tcp_close(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	struct proxy_msg message;
	int ret;

	memset(&message, 0x0, sizeof(struct proxy_msg));
	message.type = PROXY_MSG_TYPE_TCP_CLOSE;
	message.size = 0;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG, "Sending TCP_CLOSE message to client '%s'\n", priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, (uint8_t *)&message, sizeof(struct proxy_msg));

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

/*
 * API Functions
 */

int proxy_conn_accept(struct proxy_conn_handle *pc, struct conn_handle *conn_client)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	int ret = 0;

	mutex_lock(&priv->mutex_sentinel);
	if (priv->sentinel != 0 || priv->conn_client != NULL)
	{
		ret = -EBUSY;
		goto proxy_conn_accept_exit;
	}

	priv->conn_client = conn_client;
	condvar_wake_one(&priv->condvar_client);

proxy_conn_accept_exit:
	mutex_unlock(&priv->mutex_sentinel);

	return ret;
}

void proxy_conn_drop(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

	mutex_lock(&priv->mutex_sentinel);

	if (priv->conn_client != NULL)
	{
		conn_drop(priv->conn_client);
	}

	mutex_unlock(&priv->mutex_sentinel);
}

void proxy_conn_free(struct proxy_conn_handle *pc)
{
	if (pc->priv != NULL)
	{
		struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;

		proxy_conn_stop(pc);

		thread_free(&priv->thread_tcp);
		thread_free(&priv->thread_data);
		thread_free(&priv->thread_control);
		thread_free(&priv->thread_client);

		mutex_free(&priv->mutex_sentinel);
		mutex_free(&priv->mutex_client_send);

		condvar_free(&priv->condvar_client);

		conn_free(&priv->conn_tcp);
		conn_free(&priv->conn_data);
		conn_free(&priv->conn_control);

		if (priv->conn_client != NULL)
		{
			conn_free(priv->conn_client);
		}

		free(pc->priv);
		pc->priv = NULL;
	}
}

int proxy_conn_init(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv;
	int ret;

	if (pc->priv == NULL)
	{
		pc->priv = malloc(sizeof(struct proxy_conn_priv));
	}

	if (pc->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(pc->priv, 0x0, sizeof(struct proxy_conn_priv));

	priv = (struct proxy_conn_priv *)pc->priv;

	priv->conn_client = NULL;

	ret = conn_init(&priv->conn_control);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = conn_init(&priv->conn_data);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = conn_init(&priv->conn_tcp);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	priv->conn_control.type = CONN_TYPE_UDP;
	priv->conn_data.type = CONN_TYPE_UDP;
	priv->conn_tcp.type = CONN_TYPE_TCP;

	ret = condvar_init(&priv->condvar_client);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = mutex_init(&priv->mutex_client_send);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = mutex_init(&priv->mutex_sentinel);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = thread_init(&priv->thread_client);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = thread_init(&priv->thread_control);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = thread_init(&priv->thread_data);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	ret = thread_init(&priv->thread_tcp);
	if (ret != 0)
	{
		goto proxy_conn_init_exit;
	}

	priv->thread_client.func_ctx = pc;
	priv->thread_control.func_ctx = pc;
	priv->thread_data.func_ctx = pc;
	priv->thread_tcp.func_ctx = pc;

	priv->thread_client.func_ptr = client_manager;
	priv->thread_control.func_ptr = forwarder_control;
	priv->thread_data.func_ptr = forwarder_data;
	priv->thread_tcp.func_ptr = forwarder_tcp;

	return 0;

proxy_conn_init_exit:
	thread_free(&priv->thread_tcp);
	thread_free(&priv->thread_data);
	thread_free(&priv->thread_control);
	thread_free(&priv->thread_client);

	mutex_free(&priv->mutex_sentinel);
	mutex_free(&priv->mutex_client_send);

	condvar_free(&priv->condvar_client);

	conn_free(&priv->conn_tcp);
	conn_free(&priv->conn_data);
	conn_free(&priv->conn_control);

	free(pc->priv);
	pc->priv = NULL;

	return 0;
}

int proxy_conn_start(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	int ret;

	ret = thread_start(&priv->thread_client);

	return ret;
}

int proxy_conn_stop(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = (struct proxy_conn_priv *)pc->priv;
	int ret = 0;

	proxy_conn_drop(pc);

	mutex_lock(&priv->mutex_sentinel);
	priv->sentinel = 1;
	condvar_wake_all(&priv->condvar_client);
	mutex_unlock(&priv->mutex_sentinel);

	ret = thread_join(&priv->thread_client);

	if (priv->conn_client != NULL)
	{
		proxy_log(pc->ph, LOG_LEVEL_ERROR, "Proxy connection client thread didn't clean up the conn_client!\n");
		conn_close(priv->conn_client);
		conn_free(priv->conn_client);
		free(priv->conn_client);
		priv->conn_client = NULL;
	}

	return ret;
}
