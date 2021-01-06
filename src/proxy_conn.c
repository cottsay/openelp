/*!
 * @file proxy_conn.c
 *
 * @copyright
 * Copyright &copy; 2016, Scott K Logan
 *
 * @copyright
 * All rights reserved.
 *
 * @copyright
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * @copyright
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * @copyright
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @copyright
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan &lt;logans@cottsay.net&gt;
 *
 * @brief Implementation of proxy client connection
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openelp/openelp.h"
#include "conn.h"
#include "digest.h"
#include "mutex.h"
#include "proxy_conn.h"
#include "rand.h"
#include "thread.h"
#include "worker.h"

/*!
 * @brief Maximum amount of data to process in one message
 *
 * @note It seems that the official client can't handle messages from proxies which
 * are larger than 4096 or so
 */
#define CONN_BUFF_LEN 4096

/*! Maximum amount of data to process not including the message header */
#define CONN_BUFF_LEN_HEADERLESS (CONN_BUFF_LEN - sizeof(struct proxy_msg))

/*!
 * @brief Message types used in communication between the proxy and the client
 */
enum PROXY_MSG_TYPE {
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
	 * The address field is ignored in this message
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
	PROXY_MSG_TYPE_SYSTEM
};

/*!
 * @brief System messages sent by the proxy to the client
 */
enum SYSTEM_MSG {
	/*! The client has supplied the proxy with an incorrect password */
	SYSTEM_MSG_BAD_PASSWORD = 1,

	/*! The client's callsign is not allowed to use the proxy */
	SYSTEM_MSG_ACCESS_DENIED
};

#ifdef _WIN32
#  pragma pack(push, 1)
#endif
/*!
 * @brief Proxy message header
 */
struct proxy_msg {
	/*! Type of proxy message, should be one of ::PROXY_MSG_TYPE */
	uint8_t type;

	/*! 32-bit IPv4 address, if applicable */
	uint32_t address;

	/*! Number of bytes in proxy_msg::data */
	uint32_t size;
#ifdef _WIN32
};
#  pragma pack(pop)
#else
} __attribute__((packed));
#endif

/*!
 * @brief Private data for an instance of a proxy client connection
 */
struct proxy_conn_priv {
	/*! TCP connection to the client */
	struct conn_handle *conn_client;

	/*! UDP connection for control information */
	struct conn_handle conn_control;

	/*! UDP connection for data */
	struct conn_handle conn_data;

	/*! TCP connection for directory information */
	struct conn_handle conn_tcp;

	/*! Mutex for protecting the proxy_conn_priv::sentinel */
	struct mutex_handle mutex_client;

	/*! Mutex for protecting transmissions on proxy_conn_priv::conn_client */
	struct mutex_handle mutex_client_send;

	/*! Thread for handling data sent to proxy_conn_priv::conn_control */
	struct thread_handle thread_control;

	/*! Thread for handling data sent to proxy_conn_priv::conn_data */
	struct thread_handle thread_data;

	/*! Thread for handling data sent to proxy_conn_priv::conn_tcp */
	struct thread_handle thread_tcp;

	/*! Worker for handling data sent from the client */
	struct worker_handle worker_client;

	/*! Last callsign that this proxy connection was connected to */
	char callsign[12];
};

/*!
 * @brief Authorize an incoming client for use of this proxy
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int client_authorize(struct proxy_conn_handle *pc);

/*!
 * @brief Worker thread for managing the connection to the client
 *
 * @param[in,out] wh Worker thread context
 */
static void client_manager(struct worker_handle *wh);

/*!
 * @brief Worker thread for forwarding control information
 *
 * @param[in,out] ctx Worker thread context
 *
 * @returns Always NULL
 */
static void *forwarder_control(void *ctx);

/*!
 * @brief Worker thread for forwarding UDP data
 *
 * @param[in,out] ctx Worker thread context
 *
 * @returns Always NULL
 */
static void *forwarder_data(void *ctx);

/*!
 * @brief Worker thread for forwarding TCP data
 *
 * @param[in,out] ctx Worker thread context
 *
 * @returns Always NULL
 */
static void *forwarder_tcp(void *ctx);

/*!
 * @brief Process an incoming ::PROXY_MSG_TYPE_UDP_CONTROL message from the
 *        client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_control_data_message(struct proxy_conn_handle *pc,
					struct proxy_msg *msg);

/*!
 * @brief Process an incoming ::PROXY_MSG_TYPE_UDP_DATA message from the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_data_message(struct proxy_conn_handle *pc,
				struct proxy_msg *msg);

/*!
 * @brief Process an incoming message from the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_message(struct proxy_conn_handle *pc,
			   struct proxy_msg *msg);

/*!
 * @brief Process an incoming ::PROXY_MSG_TYPE_TCP_CLOSE message from the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_tcp_close_message(struct proxy_conn_handle *pc,
				     struct proxy_msg *msg);

/*!
 * @brief Process an incoming ::PROXY_MSG_TYPE_TCP_DATA message from the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_tcp_data_message(struct proxy_conn_handle *pc,
				    struct proxy_msg *msg);

/*!
 * @brief Process an incoming ::PROXY_MSG_TYPE_TCP_OPEN message from the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg Incoming message
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int process_tcp_open_message(struct proxy_conn_handle *pc,
				    const struct proxy_msg *msg);

/*!
 * @brief Send a ::PROXY_MSG_TYPE_SYSTEM message to the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] msg ::SYSTEM_MSG value
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int send_system(struct proxy_conn_handle *pc, enum SYSTEM_MSG msg);

/*!
 * @brief Send a ::PROXY_MSG_TYPE_TCP_CLOSE message to the client
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int send_tcp_close(struct proxy_conn_handle *pc);

static int client_authorize(struct proxy_conn_handle *pc)
{
	uint8_t buff[28];
	size_t idx, j;
	uint32_t nonce;
	char nonce_str[9];
	struct proxy_conn_priv *priv = pc->priv;
	uint8_t response[PROXY_PASS_RES_LEN];
	int ret;

	ret = get_nonce(&nonce);
	if (ret < 0)
		return ret;

	digest_to_hex32(nonce, nonce_str);

	/* Generate the expected auth response */
	ret = get_password_response(nonce, pc->ph->conf.password, response);
	if (ret < 0)
		return ret;

	/* Send the nonce */
	ret = conn_send(priv->conn_client, (uint8_t *)nonce_str, 8);
	if (ret < 0)
		return ret;

	/* We can expect to receive a newline-terminated callsign and a 16-byte
	 * password response.
	 * Since this is variable-length, initially look only for 16 bytes. The
	 * callsign will be part of that, and we can figure out how much we're
	 * missing.
	 */
	ret = conn_recv(priv->conn_client, buff, 16);
	if (ret < 0)
		return ret;

	idx = 0;
	while (idx < 11 && buff[idx] != '\n')
		idx++;

	if (idx >= 11)
		return -EINVAL;

	/* Make the callsign null-terminated */
	buff[idx] = '\0';
	strcpy(priv->callsign, (char *)buff);

	ret = conn_recv(priv->conn_client, &buff[16], idx + 1);
	if (ret < 0)
		return ret;

	for (idx += 1, j = 0; j < PROXY_PASS_RES_LEN; idx++, j++) {
		if (response[j] != buff[idx]) {
			proxy_log(pc->ph, LOG_LEVEL_INFO,
				  "Client '%s' supplied an incorrect password. Dropping...\n",
				  priv->callsign);

			ret = send_system(pc, SYSTEM_MSG_BAD_PASSWORD);

			return ret < 0 ? ret : -EACCES;
		}
	}

	ret = proxy_authorize_callsign(pc->ph, priv->callsign);
	if (ret != 1) {
		proxy_log(pc->ph, LOG_LEVEL_INFO,
			  "Client '%s' is not authorized to use this proxy. Dropping...\n",
			  priv->callsign);

		ret = send_system(pc, SYSTEM_MSG_ACCESS_DENIED);

		return ret < 0 ? ret : -EACCES;
	}

	return 0;
}

static void client_manager(struct worker_handle *wh)
{
	struct proxy_conn_handle *pc = wh->func_ctx;
	struct proxy_conn_priv *priv = pc->priv;
	int ret;
	uint8_t buff[CONN_BUFF_LEN];
	char remote_addr[54];

	mutex_lock_shared(&priv->mutex_client);

	if (priv->conn_client == NULL) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "New connection was signaled, but no connection was given\n");

		mutex_unlock_shared(&priv->mutex_client);

		return;
	}

	mutex_unlock_shared(&priv->mutex_client);

	conn_get_remote_addr(priv->conn_client, remote_addr);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "New connection - beginning authorization procedure\n");

	ret = client_authorize(pc);
	if (ret < 0) {
		switch (ret) {
		case -ECONNRESET:
		case -EINTR:
		case -ENOTCONN:
		case -EPIPE:
			proxy_log(pc->ph, LOG_LEVEL_WARN,
				  "Connection to client was lost before authorization could complete\n");
			break;
		default:
			proxy_log(pc->ph, LOG_LEVEL_ERROR,
				  "Authorization failed for client '%s' (%d): %s\n",
				  remote_addr, -ret, strerror(-ret));
		}

		mutex_lock(&priv->mutex_client);
		conn_free(priv->conn_client);
		free(priv->conn_client);
		priv->conn_client = NULL;
		mutex_unlock(&priv->mutex_client);

		return;
	}

	proxy_update_registration(pc->ph);

	ret = conn_listen(&priv->conn_control);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to open UDP control port (5199). Dropping...\n");
		goto client_manager_exit;
	}

	ret = conn_listen(&priv->conn_data);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to open UDP data port (5198). Dropping...\n");
		goto client_manager_exit;
	}

	ret = thread_start(&priv->thread_control);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to start UDP control forwarder. Dropping...\n");
		goto client_manager_exit;
	}

	ret = thread_start(&priv->thread_data);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to start UDP data forwarder. Dropping...\n");
		goto client_manager_exit;
	}

	proxy_log(pc->ph, LOG_LEVEL_INFO,
		  "Connected to client '%s', using external interface '%s'.\n",
		  priv->callsign, pc->source_addr == NULL ? "0.0.0.0" :
		  pc->source_addr);

	/* DO STUFF */
	while (1) {
		ret = conn_recv(priv->conn_client, buff, sizeof(struct proxy_msg));
		if (ret < 0) {
			switch (ret) {
			case -ECONNRESET:
			case -EINTR:
			case -ENOTCONN:
			case -EPIPE:
				break;
			default:
				proxy_log(pc->ph, LOG_LEVEL_ERROR,
					  "Failed to receive data from client '%s' (%d): %s\n",
					  priv->callsign, -ret, strerror(-ret));
				break;
			}

			break;
		}

		ret = process_message(pc, (struct proxy_msg *)buff);
		if (ret < 0) {
			conn_drop(priv->conn_client);
			break;
		}
	}

	proxy_log(pc->ph, LOG_LEVEL_INFO,
		  "Disconnected from client '%s'.\n", priv->callsign);

client_manager_exit:
	conn_close(&priv->conn_control);
	conn_close(&priv->conn_data);
	conn_close(&priv->conn_tcp);

	thread_join(&priv->thread_data);
	thread_join(&priv->thread_control);
	thread_join(&priv->thread_tcp);

	mutex_lock(&priv->mutex_client);
	conn_free(priv->conn_client);
	free(priv->conn_client);
	priv->conn_client = NULL;
	mutex_unlock(&priv->mutex_client);

	proxy_update_registration(pc->ph);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Client manager is returning cleanly.\n");
}

static void *forwarder_control(void *ctx)
{
	struct thread_handle *th = ctx;
	struct proxy_conn_handle *pc = th->func_ctx;
	struct proxy_conn_priv *priv = pc->priv;

	uint32_t addr;
	uint8_t buf[CONN_BUFF_LEN] = { 0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_UDP_CONTROL;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "UDP Control forwarding thread is starting for client '%s'\n",
		  priv->callsign);

	do {
		ret = conn_recv_any(&priv->conn_control, buf + sizeof(*msg),
				    CONN_BUFF_LEN_HEADERLESS, &addr, NULL);
		if (ret > 0) {
			msg->address = addr;
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG,
				  "Sending UDP_DATA message to client '%s' (%d bytes)\n",
				  priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg,
					sizeof(*msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			/* This is an error with the client connection */
			if (ret < 0) {
				conn_close(&priv->conn_control);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG,
					  "Client '%s' UDP Control thread is returning due to a client connection error (%d): %s\n",
					  priv->callsign, -ret, strerror(-ret));

				switch (ret) {
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
		} else if (ret == 0) {
			ret = -EPIPE;
		}
	} while (ret >= 0);

	switch (ret) {
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_INFO,
			  "Failed to receive data on client '%s' UDP Control connection (%d): %s\n",
			  priv->callsign, -ret, strerror(-ret));
		/* Since the UDP ports must be open while the client is connected,
		 * we should shut down the client if we don't exit cleanly
		 */
		proxy_conn_drop(pc);
		break;
	}

	conn_close(&priv->conn_control);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Client '%s' UDP Control thread is returning cleanly\n",
		  priv->callsign);

	return NULL;
}

static void *forwarder_data(void *ctx)
{
	struct thread_handle *th = ctx;
	struct proxy_conn_handle *pc = th->func_ctx;
	struct proxy_conn_priv *priv = pc->priv;

	uint32_t addr;
	uint8_t buf[CONN_BUFF_LEN] = { 0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_UDP_DATA;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "UDP Data forwarding thread is starting for client '%s'\n",
		  priv->callsign);

	do {
		ret = conn_recv_any(&priv->conn_data, buf + sizeof(*msg),
				    CONN_BUFF_LEN_HEADERLESS, &addr, NULL);
		if (ret > 0) {
			msg->address = addr;
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG,
				  "Sending UDP_DATA message to client '%s' (%d bytes)\n",
				  priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg,
					sizeof(*msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			/* This is an error with the client connection */
			if (ret < 0) {
				conn_close(&priv->conn_data);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG,
					  "Client '%s' UDP Data thread is returning due to a client connection error (%d): %s\n",
					  priv->callsign, -ret, strerror(-ret));

				switch (ret) {
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
		} else if (ret == 0) {
			ret = -EPIPE;
		}
	} while (ret >= 0);

	switch (ret) {
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_INFO,
			  "Failed to receive data on client '%s' UDP Data connection (%d): %s\n",
			  priv->callsign, -ret, strerror(-ret));
		/* Since the UDP ports must be open while the client is connected,
		 * we should shut down the client if we don't exit cleanly
		 */
		proxy_conn_drop(pc);
		break;
	}

	conn_close(&priv->conn_data);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Client '%s' UDP Data thread is returning cleanly\n",
		  priv->callsign);

	return NULL;
}

static void *forwarder_tcp(void *ctx)
{
	struct thread_handle *th = ctx;
	struct proxy_conn_handle *pc = th->func_ctx;
	struct proxy_conn_priv *priv = pc->priv;

	uint8_t buf[CONN_BUFF_LEN] = { 0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = PROXY_MSG_TYPE_TCP_DATA;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "TCP forwarding thread is starting for client '%s'\n",
		  priv->callsign);

	do {
		ret = conn_recv_any(&priv->conn_tcp, buf + sizeof(*msg),
				    CONN_BUFF_LEN_HEADERLESS, NULL, NULL);
		if (ret > 0) {
			msg->size = ret;

			proxy_log(pc->ph, LOG_LEVEL_DEBUG,
				  "Sending TCP_DATA message to client '%s' (%d bytes)\n",
				  priv->callsign, msg->size);

			mutex_lock(&priv->mutex_client_send);

			ret = conn_send(priv->conn_client, (uint8_t *)msg,
					sizeof(*msg) + msg->size);

			mutex_unlock(&priv->mutex_client_send);

			/* This is an error with the client connection */
			if (ret < 0) {
				conn_close(&priv->conn_tcp);

				proxy_log(pc->ph, LOG_LEVEL_DEBUG,
					  "Client '%s' TCP thread is returning due to a client connection error (%d): %s\n",
					  priv->callsign, -ret, strerror(-ret));

				switch (ret) {
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
		} else if (ret == 0) {
			ret = -EPIPE;
		}
	} while (ret >= 0);

	switch (ret) {
	case -ECONNRESET:
	case -EINTR:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(pc->ph, LOG_LEVEL_WARN,
			  "Failed to receive data on client '%s' TCP connection (%d): %s\n",
			  priv->callsign, -ret, strerror(-ret));
		break;
	}

	conn_close(&priv->conn_tcp);

	send_tcp_close(pc);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Client '%s' TCP thread is returning cleanly\n",
		  priv->callsign);

	return NULL;
}

static int process_control_data_message(struct proxy_conn_handle *pc,
					struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing UDP_CONTROL message (%d bytes) from client '%s'\n",
		  msg_size, priv->callsign);

	while (msg_size > 0) {
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ?
				       CONN_BUFF_LEN : msg_size;

		/* Get the data segment from the client */
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			return -EPIPE;

		msg_size -= ret;

		/* Send the data */
		ret = conn_send_to(&priv->conn_control, (void *)msg, ret, addr, 5199);
		if (ret < 0)
			proxy_log(pc->ph, LOG_LEVEL_WARN,
				  "Failed to send UDP_CONTROL packet of size %zu to client '%s': %d (%s)\n",
				  curr_msg_size, priv->callsign, -ret,
				  strerror(-ret));
			/*! @TODO Drop? */
	}

	return 0;
}

static int process_data_message(struct proxy_conn_handle *pc,
				struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing UDP_DATA message (%d bytes) from client '%s'\n",
		  msg_size, priv->callsign);

	while (msg_size > 0) {
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ?
				       CONN_BUFF_LEN : msg_size;

		/* Get the data segment from the client */
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			return -EPIPE;

		msg_size -= ret;

		/* Send the data */
		ret = conn_send_to(&priv->conn_data, (void *)msg, ret, addr, 5198);
		if (ret < 0)
			proxy_log(pc->ph, LOG_LEVEL_WARN,
				  "Failed to send UDP_DATA packet of size %zu to client '%s': %d (%s)\n",
				  curr_msg_size, priv->callsign, -ret,
				  strerror(-ret));
			/*! @TODO Drop? */
	}

	return 0;
}

/* This should return non-zero for conn_client errors only */
static int process_message(struct proxy_conn_handle *pc,
			   struct proxy_msg *msg)
{
	switch (msg->type) {
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
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Invalid data received from client (beginning with %02x)\n",
			  msg->type);
		return -EINVAL;
	}
}

static int process_tcp_close_message(struct proxy_conn_handle *pc,
				     struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing TCP_CLOSE message from client '%s'\n",
		  priv->callsign);
	(void)msg;

	conn_close(&priv->conn_tcp);

	return 0;
}

static int process_tcp_data_message(struct proxy_conn_handle *pc,
				    struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	size_t msg_size = msg->size;
	size_t curr_msg_size;
	int tcp_ret = 0;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing TCP_DATA message (%d bytes) from client '%s'\n",
		  msg_size, priv->callsign);

	while (msg_size > 0) {
		curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN :
				msg_size;

		/* Get the data segment from the client */
		ret = conn_recv(priv->conn_client, (void *)msg, curr_msg_size);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			return -EPIPE;

		msg_size -= ret;

		/* Send the data */
		if (tcp_ret == 0) {
			proxy_log(pc->ph, LOG_LEVEL_DEBUG,
				  "Sending TCP_DATA message (%d bytes) from client '%s' to remote host\n",
				  ret, priv->callsign);

			tcp_ret = conn_send(&priv->conn_tcp, (void *)msg, ret);
			if (tcp_ret < 0) {
				proxy_log(pc->ph, LOG_LEVEL_DEBUG,
					  "Error sending data to remote host (%d): %s\n",
					  -tcp_ret, strerror(-tcp_ret));

				conn_close(&priv->conn_tcp);
			}
		}
	}

	if (tcp_ret != 0)
		send_tcp_close(pc);

	return 0;
}

static int process_tcp_open_message(struct proxy_conn_handle *pc,
				    const struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	uint8_t status_buf[sizeof(struct proxy_msg) + 4] = { 0 };
	struct proxy_msg *status_msg = (struct proxy_msg *)status_buf;
	const uint8_t *addr_sep = (const uint8_t *)&msg->address;
	char addr[16] = "";
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing TCP_OPEN message from client '%s'\n",
		  priv->callsign);

	ret = snprintf(addr, 16, "%hu.%hu.%hu.%hu",
		       (uint16_t)addr_sep[0], (uint16_t)addr_sep[1],
		       (uint16_t)addr_sep[2], (uint16_t)addr_sep[3]);
	if (ret < 7 || ret > 15) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Address conversion failed (%d)\n", ret);
		return -EINVAL;
	}

	/* Attempt to connect */
	ret = conn_connect(&priv->conn_tcp, (const char *)addr, "5200");
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_WARN,
			  "Failed to open TCP connection for client '%s' (%d): %s\n",
			  priv->callsign, -ret, strerror(-ret));
	} else {
		/* Connection succeeded - start the thread */
		ret = thread_start(&priv->thread_tcp);
		if (ret < 0)
			conn_close(&priv->conn_tcp);
	}

	status_msg->type = PROXY_MSG_TYPE_TCP_STATUS;
	status_msg->size = 4;

	/* Unless we can figure out what the client is expecting here, the
	 * best we can do is a "non-zero" value to indicate failure.
	 */
	memcpy(status_buf + sizeof(*status_msg), &ret, 4);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Sending TCP_STATUS message (%d) to client '%s'\n",
		  ret, priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, status_buf,
			sizeof(*status_msg) + status_msg->size);

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

static int send_system(struct proxy_conn_handle *pc, enum SYSTEM_MSG msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	uint8_t buf[sizeof(struct proxy_msg) + 1] = { 0 };
	struct proxy_msg *message = (struct proxy_msg *)buf;
	int ret;

	message->type = PROXY_MSG_TYPE_SYSTEM;
	message->size = 1;
	buf[sizeof(*message)] = (uint8_t)msg;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Sending SYSTEM message (%d) to client '%s'\n",
		  msg, priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, buf,
			sizeof(*message) + message->size);

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

static int send_tcp_close(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	struct proxy_msg message = { 0 };
	int ret;

	message.type = PROXY_MSG_TYPE_TCP_CLOSE;
	message.size = 0;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Sending TCP_CLOSE message to client '%s'\n", priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, (uint8_t *)&message,
			sizeof(struct proxy_msg));

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

/*
 * API Functions
 */
int proxy_conn_accept(struct proxy_conn_handle *pc,
		      struct conn_handle *conn_client)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret = 0;

	mutex_lock(&priv->mutex_client);

	if (priv->conn_client != NULL) {
		ret = -EBUSY;
		goto proxy_conn_accept_exit;
	}

	priv->conn_client = conn_client;
	worker_wake(&priv->worker_client);

proxy_conn_accept_exit:
	mutex_unlock(&priv->mutex_client);

	return ret;
}

void proxy_conn_drop(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;

	mutex_lock_shared(&priv->mutex_client);

	if (priv->conn_client != NULL)
		conn_drop(priv->conn_client);

	mutex_unlock_shared(&priv->mutex_client);
}

void proxy_conn_free(struct proxy_conn_handle *pc)
{
	if (pc->priv != NULL) {
		struct proxy_conn_priv *priv = pc->priv;

		proxy_conn_stop(pc);

		worker_free(&priv->worker_client);

		thread_free(&priv->thread_tcp);
		thread_free(&priv->thread_data);
		thread_free(&priv->thread_control);

		mutex_free(&priv->mutex_client);
		mutex_free(&priv->mutex_client_send);

		conn_free(&priv->conn_tcp);
		conn_free(&priv->conn_data);
		conn_free(&priv->conn_control);

		if (priv->conn_client != NULL) {
			conn_free(priv->conn_client);
			free(priv->conn_client);
		}

		free(pc->priv);
		pc->priv = NULL;
	}
}

int proxy_conn_init(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		pc->priv = priv;
	}

	priv->conn_control.source_addr = pc->source_addr;
	priv->conn_control.source_port = "5199";
	priv->conn_control.type = CONN_TYPE_UDP;
	ret = conn_init(&priv->conn_control);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->conn_data.source_addr = pc->source_addr;
	priv->conn_data.source_port = "5198";
	priv->conn_data.type = CONN_TYPE_UDP;
	ret = conn_init(&priv->conn_data);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->conn_tcp.source_addr = pc->source_addr;
	priv->conn_tcp.source_port = NULL;
	priv->conn_tcp.type = CONN_TYPE_TCP;
	ret = conn_init(&priv->conn_tcp);
	if (ret != 0)
		goto proxy_conn_init_exit;

	ret = mutex_init(&priv->mutex_client_send);
	if (ret != 0)
		goto proxy_conn_init_exit;

	ret = mutex_init(&priv->mutex_client);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->thread_control.func_ctx = pc;
	priv->thread_control.func_ptr = forwarder_control;
	priv->thread_control.stack_size = 1024 * 1024;
	ret = thread_init(&priv->thread_control);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->thread_data.func_ctx = pc;
	priv->thread_data.func_ptr = forwarder_data;
	priv->thread_data.stack_size = 1024 * 1024;
	ret = thread_init(&priv->thread_data);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->thread_tcp.func_ctx = pc;
	priv->thread_tcp.func_ptr = forwarder_tcp;
	priv->thread_tcp.stack_size = 1024 * 1024;
	ret = thread_init(&priv->thread_tcp);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->worker_client.func_ctx = pc;
	priv->worker_client.func_ptr = client_manager;
	priv->worker_client.stack_size = 1024 * 1024;
	ret = worker_init(&priv->worker_client);
	if (ret != 0)
		goto proxy_conn_init_exit;

	return 0;

proxy_conn_init_exit:
	worker_free(&priv->worker_client);

	thread_free(&priv->thread_tcp);
	thread_free(&priv->thread_data);
	thread_free(&priv->thread_control);

	mutex_free(&priv->mutex_client);
	mutex_free(&priv->mutex_client_send);

	conn_free(&priv->conn_tcp);
	conn_free(&priv->conn_data);
	conn_free(&priv->conn_control);

	free(pc->priv);
	pc->priv = NULL;

	return 0;
}

int proxy_conn_in_use(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret = 0;

	mutex_lock_shared(&priv->mutex_client);

	if (priv->conn_client != NULL)
		ret = 1;

	mutex_unlock_shared(&priv->mutex_client);

	return ret;
}

int proxy_conn_start(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret;

	mutex_lock_shared(&priv->mutex_client);

	ret = worker_start(&priv->worker_client);
	if (ret < 0)
		goto proxy_conn_start_exit;

	if (priv->conn_client != NULL && worker_is_idle(&priv->worker_client))
		worker_wake(&priv->worker_client);

proxy_conn_start_exit:
	mutex_unlock_shared(&priv->mutex_client);

	return ret;
}

int proxy_conn_stop(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;

	proxy_conn_drop(pc);

	return worker_join(&priv->worker_client);
}
