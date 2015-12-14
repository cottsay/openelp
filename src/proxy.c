/*!
 * @file proxy.c
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

#include "conf.h"
#include "conn.h"
#include "digest.h"
#include "log.h"
#include "mutex.h"
#include "rand.h"
#include "thread.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// It seems that the native client can't handle messages from proxies which are
// larger than 4096 or so
#define CONN_BUFF_LEN 4096
#define CONN_BUFF_LEN_HEADERLESS CONN_BUFF_LEN - sizeof(struct proxy_msg)

#if PROXY_PASS_RES_LEN != DIGEST_LEN
#error Password Response Length Mismatch
#endif

struct forwarder_context
{
	struct proxy_handle *ph;
	struct proxy_conn *conn;
	uint8_t type;
};

struct proxy_priv
{
	struct log_handle log;

	struct proxy_conn conn_client;
	uint8_t conn_client_buff[CONN_BUFF_LEN];
	struct mutex_handle conn_client_mutex;
	struct proxy_conn conn_udp1;
	struct proxy_conn conn_udp2;
	struct proxy_conn conn_tcp;
	struct forwarder_context conn_udp1_forwarder_ctx;
	struct forwarder_context conn_udp2_forwarder_ctx;
	struct forwarder_context conn_tcp_forwarder_ctx;
	struct proxy_thread conn_udp1_forwarder;
	struct proxy_thread conn_udp2_forwarder;
	struct proxy_thread conn_tcp_forwarder;

	char client_callsign[12];
};

/*
 * Function Prototypes
 */
void * tcp_forwarder(void *);
void * udp_forwarder(void *);

/*
 * Functions
 */
int proxy_load_conf(struct proxy_handle *ph, const char *path)
{
	return conf_parse_file(path, &ph->conf);
}

int proxy_init(struct proxy_handle *ph)
{
	int ret = 0;

	if (ret == 0 && ph != NULL)
	{
		if (ph->status == PROXY_STATUS_NONE)
		{
			struct proxy_priv *priv;

			if (ph->priv == NULL)
			{
				ph->priv = malloc(sizeof(struct proxy_priv));
			}

			if (ph->priv == NULL)
			{
				return -ENOMEM;
			}

			memset(ph->priv, 0x0, sizeof(struct proxy_priv));
			priv = (struct proxy_priv *)ph->priv;

			// Initialize RNG
			ret = rand_init();
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			// Initialize log
			ret = log_init(&priv->log);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			// Initialize mutexes
			ret = mutex_init(&priv->conn_client_mutex);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			// Initialize config
			ret = conf_init(&ph->conf);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			// Initialize communications
			priv->conn_client.type = CONN_TYPE_TCP;
			ret = conn_init(&priv->conn_client);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			priv->conn_udp1.type = CONN_TYPE_UDP;
			ret = conn_init(&priv->conn_udp1);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			priv->conn_udp2.type = CONN_TYPE_UDP;
			ret = conn_init(&priv->conn_udp2);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			priv->conn_tcp.type = CONN_TYPE_TCP;
			ret = conn_init(&priv->conn_tcp);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}

			// Initialize threads
			ret = thread_init(&priv->conn_udp1_forwarder);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}
			priv->conn_udp1_forwarder_ctx.ph = ph;
			priv->conn_udp1_forwarder_ctx.conn = &priv->conn_udp1;
			priv->conn_udp1_forwarder_ctx.type = PROXY_MSG_TYPE_UDP_DATA;
			priv->conn_udp1_forwarder.func_ptr = udp_forwarder;
			priv->conn_udp1_forwarder.func_ctx = &priv->conn_udp1_forwarder_ctx;

			ret = thread_init(&priv->conn_udp2_forwarder);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}
			priv->conn_udp2_forwarder_ctx.ph = ph;
			priv->conn_udp2_forwarder_ctx.conn = &priv->conn_udp2;
			priv->conn_udp2_forwarder_ctx.type = PROXY_MSG_TYPE_UDP_CONTROL;
			priv->conn_udp2_forwarder.func_ptr = udp_forwarder;
			priv->conn_udp2_forwarder.func_ctx = &priv->conn_udp2_forwarder_ctx;

			ret = thread_init(&priv->conn_tcp_forwarder);
			if (ret < 0)
			{
				goto proxy_init_exit;
			}
			priv->conn_tcp_forwarder_ctx.ph = ph;
			priv->conn_tcp_forwarder_ctx.conn = &priv->conn_tcp;
			priv->conn_tcp_forwarder_ctx.type = PROXY_MSG_TYPE_TCP_DATA;
			priv->conn_tcp_forwarder.func_ptr = tcp_forwarder;
			priv->conn_tcp_forwarder.func_ctx = &priv->conn_tcp_forwarder_ctx;

			ph->status = PROXY_STATUS_DOWN;
		}
		else
		{
			ret = -EBUSY;
			goto proxy_init_exit;
		}
	}

	return 0;

proxy_init_exit:
	proxy_free(ph);

	free(ph->priv);

	return ret;
}

void proxy_free(struct proxy_handle *ph)
{
	if (ph->priv != NULL)
	{
		struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

		proxy_close(ph);

		// Free threads
		thread_free(&priv->conn_tcp_forwarder);
		thread_free(&priv->conn_udp2_forwarder);
		thread_free(&priv->conn_udp1_forwarder);

		// Free connections
		conn_free(&priv->conn_tcp);
		conn_free(&priv->conn_udp1);
		conn_free(&priv->conn_udp2);
		conn_free(&priv->conn_client);

		// Free config
		conf_free(&ph->conf);

		// Free mutexes
		mutex_free(&priv->conn_client_mutex);

		// Free logger
		log_free(&priv->log);

		// Free RNG
		rand_free();

		free(ph->priv);
		ph->priv = NULL;
	}

	ph->status = PROXY_STATUS_NONE;
}

int proxy_open(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	if (ph->status < PROXY_STATUS_DOWN)
	{
		return -EINVAL;
	}
	else if (ph->status > PROXY_STATUS_DOWN)
	{	
		return -EBUSY;
	}

	// TODO: Specifiable port
	ret = conn_listen(&priv->conn_client, ph->conf.port);
	if (ret < 0)
	{
		return ret;
	}

	ph->status = PROXY_STATUS_AVAILABLE;

	return 0;
}

void proxy_close(struct proxy_handle *ph)
{
	if (ph->status > PROXY_STATUS_DOWN)
	{
		struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

		conn_close(&priv->conn_tcp);
		conn_close(&priv->conn_udp2);
		conn_close(&priv->conn_udp1);
		conn_close(&priv->conn_client);

		thread_join(&priv->conn_tcp_forwarder);
		thread_join(&priv->conn_udp2_forwarder);
		thread_join(&priv->conn_udp1_forwarder);

		ph->status = PROXY_STATUS_DOWN;
	}
}

void proxy_drop(struct proxy_handle *ph)
{
	if (ph->status > PROXY_STATUS_AVAILABLE)
	{
		struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

		conn_close(&priv->conn_tcp);
		conn_close(&priv->conn_udp2);
		conn_close(&priv->conn_udp1);
		conn_drop(&priv->conn_client);

		thread_join(&priv->conn_tcp_forwarder);
		thread_join(&priv->conn_udp2_forwarder);
		thread_join(&priv->conn_udp1_forwarder);

		ph->status = PROXY_STATUS_AVAILABLE;
	}
}

void proxy_shutdown(struct proxy_handle *ph)
{
	if (ph->status > PROXY_STATUS_DOWN)
	{
		struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

		conn_shutdown(&priv->conn_client);

		ph->status = PROXY_STATUS_SHUTDOWN;
	}
}

void proxy_log(struct proxy_handle *ph, enum LOG_LEVEL lvl, const char *fmt, ...)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	va_list args;

	if (priv == NULL || lvl > priv->log.level)
	{
		return;
	}

	va_start(args, fmt);
	log_vprintf(&priv->log, lvl, fmt, args);
	va_end(args);
}

int proxy_process(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	switch (ph->status)
	{
	case PROXY_STATUS_SHUTDOWN:
		proxy_drop(ph);
		proxy_log(ph, LOG_LEVEL_INFO, "Proxy is shutting down...\n");
		proxy_close(ph);
		break;
	case PROXY_STATUS_AVAILABLE:
		proxy_log(ph, LOG_LEVEL_INFO, "Waiting for a client...\n");

		ret = conn_listen_wait(&priv->conn_client);
		if (ret < 0)
		{
			return ret == -EINTR ? 0 : ret;
		}

		ret = process_new_client(ph);
		if (ret < 0)
		{
			return ret;
		}

		break;
	default:
		ret = conn_recv(&priv->conn_client, priv->conn_client_buff, sizeof(struct proxy_msg));
		if (ret < 0)
		{
			if (ret != -EINTR)
			{
				proxy_handle_client_error(ph, ret);
			}

			return 0;
		}

		ret = process_msg(ph, (struct proxy_msg *)priv->conn_client_buff);
		if (ret < 0)
		{
			return ret;
		}
	}

	return 0;
}

int process_new_client(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	uint32_t nonce;
	char nonce_str[9];
	uint8_t response[PROXY_PASS_RES_LEN];
	size_t idx, j;
	int ret;

	proxy_log(ph, LOG_LEVEL_INFO, "Client connected. Authorizing...\n");

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
	ret = get_password_response(nonce, ph->conf.password, response);
	if (ret < 0)
	{
		return ret;
	}

	// Send the nonce
	ret = conn_send(&priv->conn_client, (uint8_t *)nonce_str, 8);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_WARN, "Communications error with client. Dropping...\n");

		conn_drop(&priv->conn_client);

		return 0;
	}

	// We can expect to receive a newline-terminated callsign and a 16-byte
	// password response.
	// Since this is variable-length, initially look only for 16 bytes. The
	// callsign will be part of that, and we can figure out how much we're
	// missing.
	ret = conn_recv(&priv->conn_client, priv->conn_client_buff, 16);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_WARN, "Communications error with client. Dropping...\n");

		conn_drop(&priv->conn_client);

		return 0;
	}

	for (idx = 0; idx < 11 && priv->conn_client_buff[idx] != '\n'; idx++);

	if (idx >= 11)
	{
		proxy_log(ph, LOG_LEVEL_WARN, "Communications error with client. Dropping...\n");

		conn_drop(&priv->conn_client);

		return 0;
	}

	strncpy(priv->client_callsign, (char *)priv->conn_client_buff, idx);

	ret = conn_recv(&priv->conn_client, &priv->conn_client_buff[16], idx + 1);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_WARN, "Communications error with client '%s'. Dropping...\n", priv->client_callsign);

		conn_drop(&priv->conn_client);

		return 0;
	}

	for (idx += 1, j = 0; j < PROXY_PASS_RES_LEN; idx++, j++)
	{
		if (response[j] != priv->conn_client_buff[idx])
		{
			proxy_log(ph, LOG_LEVEL_WARN, "Wrong password from client '%s'. Dropping...\n", priv->client_callsign);

			ret = send_system(ph, SYSTEM_MSG_BAD_PASSWORD);

			conn_drop(&priv->conn_client);

			return 0;
		}
	}

	// TODO: Verify the callsign before accepting the connection

	ret = conn_listen(&priv->conn_udp1, 5198);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_ERROR, "Failed to open UDP data port (5198). Dropping...\n");

		proxy_drop(ph);

		return 0;
	}

	ret = thread_start(&priv->conn_udp1_forwarder);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_ERROR, "Failed to start UDP data forwarder. Dropping...\n");

		proxy_drop(ph);

		return 0;
	}

	ret = conn_listen(&priv->conn_udp2, 5199);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_ERROR, "Failed to open UDP control port (5199). Dropping...\n");

		proxy_drop(ph);

		return 0;
	}

	ret = thread_start(&priv->conn_udp2_forwarder);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_ERROR, "Failed to start UDP control forwarder. Dropping...\n");

		proxy_drop(ph);

		return 0;
	}

	proxy_log(ph, LOG_LEVEL_INFO, "Connected to client '%s'.\n", priv->client_callsign);

	ph->status = PROXY_STATUS_IN_USE;

	return 0;
}

int process_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	switch(msg->type)
	{
	case PROXY_MSG_TYPE_TCP_OPEN:
		return process_tcp_open_msg(ph, msg);
	case PROXY_MSG_TYPE_TCP_DATA:
		return process_tcp_data_msg(ph, msg);
	case PROXY_MSG_TYPE_TCP_CLOSE:
		return process_tcp_close_msg(ph, msg);
	case PROXY_MSG_TYPE_UDP_DATA:
		return process_udp_data_msg(ph, msg);
	case PROXY_MSG_TYPE_UDP_CONTROL:
		return process_udp_control_msg(ph, msg);
	default:
		proxy_log(ph, LOG_LEVEL_WARN, "Invalid data received from client (beginning with %02x)\n", msg->type);
		proxy_handle_client_error(ph, -EINVAL);

		return 0;
	}
}

int process_tcp_open_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Processing TCP_OPEN message\n");

	// Attempt to connect
	ret = conn_connect(&priv->conn_tcp, msg->address, 5200);
	if (ret >= 0)
	{
		ret = thread_start(&priv->conn_tcp_forwarder);
		if (ret < 0)
		{
			conn_close(&priv->conn_tcp);
		}
	}

	// Report results
	ret = send_tcp_status(ph, ret);
	if (ret < 0)
	{
		proxy_handle_client_error(ph, ret);
	}

	return 0;
}

int process_tcp_data_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	size_t msg_size = msg->size;
	size_t curr_msg_size;
	int tcp_ret = 0;
	int ret;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Processing TCP_DATA message (%d bytes)\n", msg->size);

	while (msg_size > 0)
	{
		curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(&priv->conn_client, priv->conn_client_buff, curr_msg_size);
		if (ret < 0)
		{
			proxy_handle_client_error(ph, ret);

			return 0;
		}

		msg_size -= ret;

		// Send the data
		if (tcp_ret == 0)
		{
			tcp_ret = conn_send(&priv->conn_tcp, priv->conn_client_buff, ret);
			if (tcp_ret < 0)
			{
				conn_close(&priv->conn_tcp);
			}
		}
	}

	if (tcp_ret != 0)
	{
		send_tcp_close(ph);
	}

	return 0;
}

int process_tcp_close_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Processing TCP_CLOSE message\n");

	conn_close(&priv->conn_tcp);

	return 0;
}

int process_udp_data_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Processing UDP_DATA message (%d bytes)\n", msg->size);

	while (msg_size > 0)
	{
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(&priv->conn_client, priv->conn_client_buff, curr_msg_size);
		if (ret < 0)
		{
			proxy_handle_client_error(ph, ret);

			return 0;
		}

		msg_size -= ret;

		// Send the data
		ret = conn_send_to(&priv->conn_udp1, priv->conn_client_buff, ret, addr, 5198);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_WARN, "Failed to send UDP_DATA packet of size %zu: %d (%s)\n", curr_msg_size, -ret, strerror(-ret));
			// Drop?
		}
	}

	return 0;
}

int process_udp_control_msg(struct proxy_handle *ph, struct proxy_msg *msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	size_t msg_size = msg->size;
	uint32_t addr = msg->address;
	int ret;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Processing UDP_CONTROL message (%d bytes)\n", msg->size);

	while (msg_size > 0)
	{
		size_t curr_msg_size = msg_size > CONN_BUFF_LEN ? CONN_BUFF_LEN : msg_size;

		// Get the data segment from the client
		ret = conn_recv(&priv->conn_client, priv->conn_client_buff, curr_msg_size);
		if (ret < 0)
		{
			proxy_handle_client_error(ph, ret);

			return 0;
		}

		msg_size -= ret;

		// Send the data
		ret = conn_send_to(&priv->conn_udp2, priv->conn_client_buff, ret, addr, 5199);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_WARN, "Failed to send UDP_CONTROL packet of size %zu: %d (%s)\n", curr_msg_size, -ret, strerror(-ret));
			// Drop?
		}
	}

	return 0;
}

void proxy_handle_client_error(struct proxy_handle *ph, int ret)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	switch (ret)
	{
	case -ENOTCONN:
		return;
	case -ECONNRESET:
	case -EPIPE:
		if (ph->status > PROXY_STATUS_AVAILABLE)
		{
			proxy_log(ph, LOG_LEVEL_INFO, "Disconnected client '%s'.\n", priv->client_callsign);
		}
		break;
	default:
		proxy_log(ph, LOG_LEVEL_WARN, "Error %d: %s\nCommunications error with client '%s' . Dropping...\n", -ret, strerror(-ret), priv->client_callsign);
		break;
	}

	proxy_drop(ph);
}

int send_system(struct proxy_handle *ph, enum SYSTEM_MSG msg)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	uint8_t buf[sizeof(struct proxy_msg) + 1] = { 0x0 };
	struct proxy_msg *message = (struct proxy_msg *)buf;
	int ret;

	message->type = PROXY_MSG_TYPE_SYSTEM;
	message->size = 1;
	message->data[0] = msg;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Sending SYSTEM message (%d)\n", msg);

	mutex_lock(&priv->conn_client_mutex);

	ret = conn_send(&priv->conn_client, buf, sizeof(struct proxy_msg) + message->size);

	mutex_unlock(&priv->conn_client_mutex);

	return ret;
}

int send_tcp_close(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	struct proxy_msg message;
	int ret;

	memset(&message, 0x0, sizeof(struct proxy_msg));
	message.type = PROXY_MSG_TYPE_TCP_CLOSE;
	message.size = 0;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Sending TCP_CLOSE message\n");

	mutex_lock(&priv->conn_client_mutex);

	ret = conn_send(&priv->conn_client, (uint8_t *)&message, sizeof(struct proxy_msg));

	mutex_unlock(&priv->conn_client_mutex);

	return ret;
}

int send_tcp_status(struct proxy_handle *ph, uint32_t status)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	uint8_t buf[sizeof(struct proxy_msg) + 4] = { 0x0 };
	struct proxy_msg *message = (struct proxy_msg *)buf;
	int ret;

	message->type = PROXY_MSG_TYPE_TCP_STATUS;
	message->size = 4;
	memcpy(message->data, &status, 4);

	proxy_log(ph, LOG_LEVEL_DEBUG, "Sending TCP_STATUS message (%d)\n", status);

	mutex_lock(&priv->conn_client_mutex);

	ret = conn_send(&priv->conn_client, buf, sizeof(struct proxy_msg) + message->size);

	mutex_unlock(&priv->conn_client_mutex);

	return ret;
}

int get_nonce(uint32_t *nonce)
{
	return rand_get(nonce);
}

int get_password_response(const uint32_t nonce, const char *password, uint8_t response[PROXY_PASS_RES_LEN])
{
	size_t pass_with_nonce_len = strlen(password) + 8;
	char *pass_with_nonce = malloc(pass_with_nonce_len + 1);
	int ret = 0;
	char *iter = pass_with_nonce;

	while (*password != '\0')
	{
		if (*password >= 97 && *password <= 122)
		{
			*iter = *password - 32;
		}
		else
		{
			*iter = *password;
		}

		iter++;
		password++;
	}

	if (snprintf(iter, 9, "%08x", nonce) != 8)
	{
		ret = -EINVAL;
		goto get_password_response_exit;
	}

	digest_get((uint8_t *)pass_with_nonce, pass_with_nonce_len, response);

get_password_response_exit:
	free(pass_with_nonce);

	return ret;
}

void * tcp_forwarder(void *ctx)
{
	struct proxy_thread *pt = (struct proxy_thread *)ctx;
	struct forwarder_context *fc = (struct forwarder_context *)pt->func_ctx;
	struct proxy_priv *priv = (struct proxy_priv *)fc->ph->priv;
	uint8_t buf[CONN_BUFF_LEN] = { 0x0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret;

	msg->type = fc->type;

	do
	{
		ret = conn_recv_any(fc->conn, msg->data, CONN_BUFF_LEN_HEADERLESS, NULL);
		if (ret > 0)
		{
			msg->size = ret;

			proxy_log(fc->ph, LOG_LEVEL_DEBUG, "Sending TCP_DATA message (%d bytes)\n", msg->size);

			mutex_lock(&priv->conn_client_mutex);

			ret = conn_send(&priv->conn_client, (uint8_t *)msg, sizeof(struct proxy_msg) + msg->size);

			mutex_unlock(&priv->conn_client_mutex);

			// This is an error with the client connection
			if (ret < 0)
			{
				proxy_handle_client_error(fc->ph, ret);

				conn_close(fc->conn);

				return NULL;
			}
		}
		else if (ret == 0)
		{
			ret = -EPIPE;
		}
	}
	while (ret >= 0);

	conn_close(fc->conn);

	ret = send_tcp_close(fc->ph);
	if (ret < 0)
	{
		// This is an error with this connection
		proxy_handle_client_error(fc->ph, ret);
	}

	return NULL;
}

void * udp_forwarder(void *ctx)
{
	struct proxy_thread *pt = (struct proxy_thread *)ctx;
	struct forwarder_context *fc = (struct forwarder_context *)pt->func_ctx;
	struct proxy_priv *priv = (struct proxy_priv *)fc->ph->priv;
	uint8_t buf[CONN_BUFF_LEN] = { 0x0 };
	struct proxy_msg *msg = (struct proxy_msg *)buf;
	int ret = 0;

	msg->type = fc->type;

	do
	{
		ret = conn_recv_any(fc->conn, msg->data, CONN_BUFF_LEN_HEADERLESS, &msg->address);
		if (ret > 0)
		{
			msg->size = ret;

			proxy_log(fc->ph, LOG_LEVEL_DEBUG, "Sending UDP_%s message (%d bytes)\n", fc->type == PROXY_MSG_TYPE_UDP_DATA ? "DATA" : "CONTROL", msg->size);

			mutex_lock(&priv->conn_client_mutex);

			ret = conn_send(&priv->conn_client, (uint8_t *)msg, sizeof(struct proxy_msg) + msg->size);

			mutex_unlock(&priv->conn_client_mutex);

			// This is an error with the client connection
			if (ret < 0)
			{
				proxy_handle_client_error(fc->ph, ret);

				conn_close(fc->conn);

				return NULL;
			}
		}
		else if (ret == 0)
		{
			ret = -EPIPE;
		}
	}
	while (ret >= 0);

	conn_close(fc->conn);

	// This is an error with this connection
	// This is pretty bad - we shouldn't have issues receiving on a
	// UDP port like this. We better just shut it all down...
	switch (ret)
	{
	case -ECONNRESET:
	case -ENOTCONN:
	case -EPIPE:
		break;
	default:
		proxy_log(fc->ph, LOG_LEVEL_WARN, "UDP communication error. Disconnecting...\n");
		proxy_drop(fc->ph);
		break;
	}

	return NULL;
}

