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
	PROXY_MSG_TYPE_UDP_CONTROL
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

	/*! Worker for handling data sent to proxy_conn_priv::conn_control */
	struct worker_handle worker_control;

	/*! Worker for handling data sent to proxy_conn_priv::conn_data */
	struct worker_handle worker_data;

	/*! Worker for handling data sent to proxy_conn_priv::conn_tcp */
	struct worker_handle worker_tcp;

	/*! The buffer for receiving data from the client */
	uint8_t buff[CONN_BUFF_LEN];

	/*! Callsign of the currently connected client */
	char callsign[12];
};

/*!
 * @brief Worker thread for forwarding control information
 *
 * @param[in,out] wh Worker thread context
 */
static void forwarder_control(struct worker_handle *wh);

/*!
 * @brief Worker thread for forwarding UDP data
 *
 * @param[in,out] wh Worker thread context
 */
static void forwarder_data(struct worker_handle *wh);

/*!
 * @brief Worker thread for forwarding TCP data
 *
 * @param[in,out] wh Worker thread context
 */
static void forwarder_tcp(struct worker_handle *wh);

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
 * @brief Send a ::PROXY_MSG_TYPE_TCP_CLOSE message to the client
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int send_tcp_close(struct proxy_conn_handle *pc);

/*!
 * @brief Send a ::PROXY_MSG_TYPE_TCP_STATUS message to the client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] status Status to report to the client
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int send_tcp_status(struct proxy_conn_handle *pc, uint32_t status);

static void forwarder_control(struct worker_handle *wh)
{
	struct proxy_conn_handle *pc = wh->func_ctx;
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

				return;
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
		  "Client '%s' UDP Control worker is returning cleanly\n",
		  priv->callsign);
}

static void forwarder_data(struct worker_handle *wh)
{
	struct proxy_conn_handle *pc = wh->func_ctx;
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

				return;
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
		  "Client '%s' UDP Data worker is returning cleanly\n",
		  priv->callsign);
}

static void forwarder_tcp(struct worker_handle *wh)
{
	struct proxy_conn_handle *pc = wh->func_ctx;
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

				return;
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
		  "Client '%s' TCP worker is returning cleanly\n",
		  priv->callsign);
}

static int process_control_data_message(struct proxy_conn_handle *pc,
					struct proxy_msg *msg)
{
	struct proxy_conn_priv *priv = pc->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Processing UDP_CONTROL message (%zu bytes) from client '%s'\n",
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
		  "Processing UDP_DATA message (%zu bytes) from client '%s'\n",
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
		  "Processing TCP_DATA message (%zu bytes) from client '%s'\n",
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

	/* Disconnect any existing connection */
	conn_close(&priv->conn_tcp);
	ret = worker_wait_idle(&priv->worker_tcp);
	if (ret < 0) {
		send_tcp_status(pc, ret);
		return ret;
	}

	/* Attempt to connect */
	ret = conn_connect(&priv->conn_tcp, (const char *)addr, "5200");
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_WARN,
			  "Failed to open TCP connection for client '%s' (%d): %s\n",
			  priv->callsign, -ret, strerror(-ret));
	} else {
		/* Connection succeeded - signal the thread */
		ret = worker_wake(&priv->worker_tcp);
		if (ret < 0)
			conn_close(&priv->conn_tcp);
	}

	return send_tcp_status(pc, ret);
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

static int send_tcp_status(struct proxy_conn_handle *pc, uint32_t status)
{
	struct proxy_conn_priv *priv = pc->priv;
	uint8_t status_buf[sizeof(struct proxy_msg) + 4] = { 0 };
	struct proxy_msg *status_msg = (struct proxy_msg *)status_buf;
	int ret;

	status_msg->type = PROXY_MSG_TYPE_TCP_STATUS;
	status_msg->size = 4;

	/* Unless we can figure out what the client is expecting here, the
	 * best we can do is a "non-zero" value to indicate failure.
	 */
	memcpy(status_buf + sizeof(*status_msg), &status, 4);

	proxy_log(pc->ph, LOG_LEVEL_DEBUG,
		  "Sending TCP_STATUS message (%d) to client '%s'\n",
		  status, priv->callsign);

	mutex_lock(&priv->mutex_client_send);

	ret = conn_send(priv->conn_client, status_buf,
			sizeof(struct proxy_msg) + status_msg->size);

	mutex_unlock(&priv->mutex_client_send);

	return ret;
}

/*
 * API Functions
 */
int proxy_conn_accept(struct proxy_conn_handle *pc,
		      struct conn_handle *conn_client,
		      const char *callsign,
		      uint8_t reconnect_only)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret = 0;

	mutex_lock(&priv->mutex_client);

	if (priv->conn_client != NULL ||
	    (reconnect_only && strcmp(priv->callsign, callsign) != 0)) {
		mutex_unlock(&priv->mutex_client);
		return -EBUSY;
	}

	strncpy(priv->callsign, callsign, sizeof(priv->callsign) - 1);
	priv->conn_client = conn_client;

	mutex_unlock(&priv->mutex_client);

	ret = conn_listen(&priv->conn_control);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to open UDP control port (5199). Dropping...\n");
		goto proxy_conn_accept_exit;
	}

	ret = conn_listen(&priv->conn_data);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to open UDP data port (5198). Dropping...\n");
		goto proxy_conn_accept_exit;
	}

	ret = worker_wake(&priv->worker_control);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to signal UDP control forwarder. Dropping...\n");
		goto proxy_conn_accept_exit;
	}

	ret = worker_wake(&priv->worker_data);
	if (ret < 0) {
		proxy_log(pc->ph, LOG_LEVEL_ERROR,
			  "Failed to signal UDP data forwarder. Dropping...\n");
		goto proxy_conn_accept_exit;
	}

	proxy_log(pc->ph, LOG_LEVEL_INFO,
		  "%s to client '%s', using external interface '%s'.\n",
		  reconnect_only ? "Reconnected" : "Connected", priv->callsign,
		  pc->source_addr == NULL ? "0.0.0.0" : pc->source_addr);

	return 0;

proxy_conn_accept_exit:
	proxy_conn_finish(pc);

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

void proxy_conn_finish(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;

	proxy_conn_drop(pc);

	conn_close(&priv->conn_control);
	conn_close(&priv->conn_data);
	conn_close(&priv->conn_tcp);

	worker_wait_idle(&priv->worker_tcp);
	worker_wait_idle(&priv->worker_data);
	worker_wait_idle(&priv->worker_control);

	mutex_lock(&priv->mutex_client);

	priv->conn_client = NULL;

	mutex_unlock(&priv->mutex_client);
}

void proxy_conn_free(struct proxy_conn_handle *pc)
{
	if (pc->priv != NULL) {
		struct proxy_conn_priv *priv = pc->priv;

		proxy_conn_stop(pc);

		worker_free(&priv->worker_tcp);
		worker_free(&priv->worker_data);
		worker_free(&priv->worker_control);

		mutex_free(&priv->mutex_client);
		mutex_free(&priv->mutex_client_send);

		conn_free(&priv->conn_tcp);
		conn_free(&priv->conn_data);
		conn_free(&priv->conn_control);

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
	priv->conn_control.source_port = pc->control_port;
	priv->conn_control.type = CONN_TYPE_UDP;
	ret = conn_init(&priv->conn_control);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->conn_data.source_addr = pc->source_addr;
	priv->conn_data.source_port = pc->data_port;
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

	priv->worker_control.func_ctx = pc;
	priv->worker_control.func_ptr = forwarder_control;
	priv->worker_control.stack_size = 1024 * 1024;
	ret = worker_init(&priv->worker_control);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->worker_data.func_ctx = pc;
	priv->worker_data.func_ptr = forwarder_data;
	priv->worker_data.stack_size = 1024 * 1024;
	ret = worker_init(&priv->worker_data);
	if (ret != 0)
		goto proxy_conn_init_exit;

	priv->worker_tcp.func_ctx = pc;
	priv->worker_tcp.func_ptr = forwarder_tcp;
	priv->worker_tcp.stack_size = 1024 * 1024;
	ret = worker_init(&priv->worker_tcp);
	if (ret != 0)
		goto proxy_conn_init_exit;

	return 0;

proxy_conn_init_exit:
	worker_free(&priv->worker_tcp);
	worker_free(&priv->worker_data);
	worker_free(&priv->worker_control);

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

int proxy_conn_process(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret;

	ret = conn_recv(priv->conn_client, priv->buff, sizeof(struct proxy_msg));
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

		return ret;
	}

	return process_message(pc, (struct proxy_msg *)priv->buff);
}

int proxy_conn_start(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret;

	mutex_lock_shared(&priv->mutex_client);

	ret = worker_start(&priv->worker_control);
	if (ret < 0)
		goto proxy_conn_start_exit;

	ret = worker_start(&priv->worker_data);
	if (ret < 0)
		goto proxy_conn_start_exit;

	ret = worker_start(&priv->worker_tcp);
	if (ret < 0)
		goto proxy_conn_start_exit;

proxy_conn_start_exit:
	mutex_unlock_shared(&priv->mutex_client);

	return ret;
}

int proxy_conn_stop(struct proxy_conn_handle *pc)
{
	struct proxy_conn_priv *priv = pc->priv;
	int ret;
	int final_ret = 0;

	proxy_conn_finish(pc);

	ret = worker_join(&priv->worker_tcp);
	if (ret < 0)
		final_ret = ret;

	ret = worker_join(&priv->worker_data);
	if (ret < 0)
		final_ret = ret;

	ret = worker_join(&priv->worker_control);
	if (ret < 0)
		final_ret = ret;

	return final_ret;
}
