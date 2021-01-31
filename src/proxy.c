/*!
 * @file proxy.c
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
 * @brief Implementation of OpenELP
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openelp/openelp.h"
#include "conf.h"
#include "conn.h"
#include "digest.h"
#include "log.h"
#include "mutex.h"
#include "proxy_conn.h"
#include "rand.h"
#include "regex.h"
#include "registration.h"
#include "worker.h"

#if PROXY_PASS_RES_LEN != DIGEST_LEN
#error Password Response Length Mismatch
#endif

/*!
 * @brief Owns and processes connections to clients
 */
struct proxy_worker {
	/*! Reference to the parent proxy instance handle */
	struct proxy_handle *ph;

	/*! Connection to the currently active client */
	struct conn_handle *conn_client;

	/*! The next ::proxy_worker in the linked list */
	struct proxy_worker *next;

	/*! Mutex for protecting proxy_worker::conn_client */
	struct mutex_handle mutex;

	/*! Worker for authenticating and processing messages */
	struct worker_handle worker;

	/*! Last callsign that this worker was connected to */
	char callsign[12];
};

/*!
 * @brief Private data for an instance of an EchoLink proxy
 */
struct proxy_priv {
	/*! Array which holds all of the proxy client connection handles */
	struct proxy_conn_handle *clients;

	/*! Linked list of available client connection handles */
	struct proxy_conn_handle *idle_clients_head;

	/*! Next pointer of last entry in the linked list of available client
	 *  connection handles */
	struct proxy_conn_handle **idle_clients_tail_ptr;

	/*! Array which holds all of the client connection worker handles */
	struct proxy_worker *client_workers;

	/*! Linked list of available client connection worker handles */
	struct proxy_worker *idle_workers_head;

	/*! Regular expression for matching allowed callsigns */
	struct regex_handle *re_calls_allowed;

	/*! Regular expression for matching denied callsigns */
	struct regex_handle *re_calls_denied;

	/*! Total number of clients in proxy_priv::clients */
	int num_clients;

	/*! Number of 'usable' clients in proxy_priv::clients */
	int usable_clients;

	/*! Network connection which listens for connections from clients */
	struct conn_handle conn_listen;

	/*! Logging infrastructure handle */
	struct log_handle log;

	/*! Used to protect proxy_priv::usable_clients */
	struct mutex_handle usable_clients_mutex;

	/*! Used to protect proxy_priv::idle_clients_head */
	struct mutex_handle idle_clients_mutex;

	/*! Used to protect proxy_priv::idle_workers_head */
	struct mutex_handle idle_workers_mutex;

	/*! Service for registering with echolink.org */
	struct registration_service_handle reg_service;

	/*! Null-terminated string which holds the listening port identifier */
	char port_str[6];
};

/*!
 * @brief Transfer ownership of a connection to the worker
 *
 * @param[in,out] pw Target proxy client worker instance
 * @param[in] conn_client Connection to a client
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int proxy_worker_accept(struct proxy_worker *pw,
			       struct conn_handle *conn_client);

/*!
 * @brief Authorize an incoming client for use of this proxy
 *
 * @param[in,out] pw Target proxy client worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int proxy_worker_authorize(struct proxy_worker *pw);

/*!
 * @brief Frees data allocated by ::proxy_worker_init
 *
 * @param[in,out] pw Target proxy client worker instance
 */
static void proxy_worker_free(struct proxy_worker *pw);

/*!
 * @brief Worker function for processing a client
 *
 * @param[in,out] wh Worker thread context
 */
static void proxy_worker_func(struct worker_handle *wh);

/*!
 * @brief Initializes the members of a ::proxy_worker
 *
 * @param[in,out] pw Target proxy client worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int proxy_worker_init(struct proxy_worker *pw);

static int proxy_worker_accept(struct proxy_worker *pw,
			       struct conn_handle *conn_client)
{
	int ret;

	mutex_lock(&pw->mutex);

	if (pw->conn_client != NULL) {
		ret = -EBUSY;
		goto proxy_worker_accept_exit;
	}

	pw->conn_client = conn_client;
	ret = worker_wake(&pw->worker);
	if (ret < 0)
		pw->conn_client = NULL;

proxy_worker_accept_exit:
	mutex_unlock(&pw->mutex);

	return ret;
}

static int proxy_worker_authorize(struct proxy_worker *pw)
{
	uint8_t buff[28];
	size_t idx, j;
	uint32_t nonce;
	char nonce_str[9];
	uint8_t response[PROXY_PASS_RES_LEN];
	int ret;

	static const uint8_t msg_bad_pw[] = {
		0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	};
	static const uint8_t msg_bad_auth[] = {
		0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
	};


	ret = get_nonce(&nonce);
	if (ret < 0)
		return ret;

	digest_to_hex32(nonce, nonce_str);

	/* Generate the expected auth response */
	ret = get_password_response(nonce, pw->ph->conf.password, response);
	if (ret < 0)
		return ret;

	/* Send the nonce */
	ret = conn_send(pw->conn_client, (uint8_t *)nonce_str, 8);
	if (ret < 0)
		return ret;

	/* We can expect to receive a newline-terminated callsign and a 16-byte
	 * password response.
	 * Since this is variable-length, initially look only for 16 bytes. The
	 * callsign will be part of that, and we can figure out how much we're
	 * missing.
	 */
	ret = conn_recv(pw->conn_client, buff, 16);
	if (ret < 0)
		return ret;

	idx = 0;
	while (idx < 11 && buff[idx] != '\n')
		idx++;

	if (idx >= 11)
		return -EINVAL;

	/* Make the callsign null-terminated */
	buff[idx] = '\0';
	strcpy(pw->callsign, (char *)buff);

	ret = conn_recv(pw->conn_client, &buff[16], idx + 1);
	if (ret < 0)
		return ret;

	for (idx += 1, j = 0; j < PROXY_PASS_RES_LEN; idx++, j++) {
		if (response[j] != buff[idx]) {
			proxy_log(pw->ph, LOG_LEVEL_INFO,
				  "Client '%s' supplied an incorrect password. Dropping...\n",
				  pw->callsign);

			ret = conn_send(pw->conn_client, msg_bad_pw, sizeof(msg_bad_pw));

			return ret < 0 ? ret : -EACCES;
		}
	}

	ret = proxy_authorize_callsign(pw->ph, pw->callsign);
	if (ret != 1) {
		proxy_log(pw->ph, LOG_LEVEL_INFO,
			  "Client '%s' is not authorized to use this proxy. Dropping...\n",
			  pw->callsign);

		ret = conn_send(pw->conn_client, msg_bad_auth, sizeof(msg_bad_pw));

		return ret < 0 ? ret : -EACCES;
	}

	return 0;
}

static void proxy_worker_free(struct proxy_worker *pw)
{
	worker_free(&pw->worker);
	mutex_free(&pw->mutex);
}

static void proxy_worker_func(struct worker_handle *wh)
{
	struct proxy_worker *pw = wh->func_ctx;
	struct proxy_priv *priv = pw->ph->priv;
	struct proxy_conn_handle *pc = NULL;
	int ret;
	char remote_addr[54];

	mutex_lock_shared(&pw->mutex);

	if (pw->conn_client == NULL) {
		proxy_log(pw->ph, LOG_LEVEL_ERROR,
			  "New connection was signaled, but no connection was given\n");

		mutex_unlock_shared(&pw->mutex);

		return;
	}

	mutex_unlock_shared(&pw->mutex);

	conn_get_remote_addr(pw->conn_client, remote_addr);

	proxy_log(pw->ph, LOG_LEVEL_DEBUG,
		  "New connection - beginning authorization procedure\n");

	ret = proxy_worker_authorize(pw);
	if (ret < 0) {
		switch (ret) {
		case -ECONNRESET:
		case -EINTR:
		case -ENOTCONN:
		case -EPIPE:
			proxy_log(pw->ph, LOG_LEVEL_WARN,
				  "Connection to client was lost before authorization could complete\n");
			break;
		default:
			proxy_log(pw->ph, LOG_LEVEL_ERROR,
				  "Authorization failed for client '%s' (%d): %s\n",
				  remote_addr, -ret, strerror(-ret));
		}

		mutex_lock(&pw->mutex);
		conn_free(pw->conn_client);
		free(pw->conn_client);
		pw->conn_client = NULL;
		mutex_unlock(&pw->mutex);

		mutex_lock(&priv->idle_workers_mutex);
		pw->next = priv->idle_workers_head;
		priv->idle_workers_head = pw;
		mutex_unlock(&priv->idle_workers_mutex);

		return;
	}

	proxy_update_registration(pw->ph);

	mutex_lock(&priv->idle_clients_mutex);
	if (priv->idle_clients_head == NULL) {
		mutex_unlock(&priv->idle_clients_mutex);
		proxy_log(pw->ph, LOG_LEVEL_ERROR,
			  "Idle slot pool is empty.\n");
		goto proxy_worker_func_exit;
	}

	pc = priv->idle_clients_head;
	/* First, check for a reconnect */
	while (pc != NULL) {
		ret = proxy_conn_accept(pc, pw->conn_client, pw->callsign, 1);
		if (ret != -EBUSY)
			break;
		pc = pc->next;
	}
	/* Fall back on the oldest available slot */
	if (pc == NULL) {
		pc = priv->idle_clients_head;
		ret = proxy_conn_accept(pc, pw->conn_client, pw->callsign, 0);
	}
	if (ret < 0) {
		mutex_unlock(&priv->idle_clients_mutex);
		proxy_log(pw->ph, LOG_LEVEL_ERROR,
			  "Failed to acquire slot (%d): %s\n",
			  -ret, strerror(-ret));
		goto proxy_worker_func_exit;
	}

	/* Remove the slot from the pool */
	*pc->prev_ptr = pc->next;
	if (pc->next == NULL)
		priv->idle_clients_tail_ptr = pc->prev_ptr;
	else
		pc->next->prev_ptr = pc->prev_ptr;
	mutex_unlock(&priv->idle_clients_mutex);

	do {
		ret = proxy_conn_process(pc);
	} while (ret >= 0);

	proxy_log(pw->ph, LOG_LEVEL_INFO,
		  "Disconnected from client '%s'.\n", pw->callsign);

	proxy_conn_finish(pc);

	/* Put the slot back in the pool */
	pc->next = NULL;
	mutex_lock(&priv->idle_clients_mutex);
	pc->prev_ptr = priv->idle_clients_tail_ptr;
	*priv->idle_clients_tail_ptr = pc;
	priv->idle_clients_tail_ptr = &pc->next;
	mutex_unlock(&priv->idle_clients_mutex);

proxy_worker_func_exit:
	mutex_lock(&pw->mutex);
	conn_free(pw->conn_client);
	free(pw->conn_client);
	pw->conn_client = NULL;
	mutex_unlock(&pw->mutex);

	mutex_lock(&priv->idle_workers_mutex);
	pw->next = priv->idle_workers_head;
	priv->idle_workers_head = pw;
	mutex_unlock(&priv->idle_workers_mutex);

	proxy_update_registration(pw->ph);

	proxy_log(pw->ph, LOG_LEVEL_DEBUG,
		  "Client worker is returning cleanly.\n");
}

static int proxy_worker_init(struct proxy_worker *pw)
{
	int ret;

	ret = mutex_init(&pw->mutex);
	if (ret < 0)
		return ret;

	pw->worker.func_ctx = pw;
	pw->worker.func_ptr = proxy_worker_func;
	pw->worker.stack_size = 1024 * 1024;
	ret = worker_init(&pw->worker);
	if (ret < 0)
		goto proxy_worker_init_exit;

	return 0;

proxy_worker_init_exit:
	mutex_free(&pw->mutex);

	return ret;
}

int proxy_authorize_callsign(struct proxy_handle *ph,
			     const char *callsign)
{
	struct proxy_priv *priv = ph->priv;
	int ret;

	if (priv->re_calls_denied != NULL) {
		ret = regex_is_match(priv->re_calls_denied, callsign);
		if (ret != 0) {
			if (ret < 0)
				proxy_log(ph, LOG_LEVEL_WARN,
					  "Failed to match callsign '%s' against denial pattern (%d): %s\n",
					  callsign, -ret, strerror(-ret));

			return 0;
		}
	}

	if (priv->re_calls_allowed != NULL) {
		ret = regex_is_match(priv->re_calls_allowed, callsign);
		if (ret != 1) {
			if (ret < 0)
				proxy_log(ph, LOG_LEVEL_WARN,
					  "Failed to match callsign '%s' against allowing pattern (%d): %s\n",
					  callsign, -ret, strerror(-ret));

			return 0;
		}
	}

	return 1;
}

int proxy_load_conf(struct proxy_handle *ph, const char *path)
{
	struct proxy_priv *priv = ph->priv;
	int ret;

	ret = conf_parse_file(path, &ph->conf, &priv->log);
	if (ret < 0)
		return ret;

	conn_port_to_str(ph->conf.port, priv->port_str);

	if (ph->conf.connection_timeout != 0) {
		proxy_log(ph, LOG_LEVEL_WARN,
			  "ConnectionTimeout is not supported this version of OpenELP\n");
	}

	if (ph->conf.bind_addr_ext_add != NULL) {
		if (ph->conf.bind_addr_ext == NULL ||
		    strcmp(ph->conf.bind_addr_ext, "0.0.0.0") == 0) {
			proxy_log(ph, LOG_LEVEL_ERROR,
				  "ExternalBindAddresses must be specified if AdditionalExternalBindAddresses is used\n");
			return -EINVAL;
		}
	}

	return 0;
}

void proxy_ident(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;

	log_ident(&priv->log);
}

int proxy_init(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		ph->priv = priv;
	}

	/* Initialize RNG */
	ret = rand_init();
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize log */
	ret = log_init(&priv->log);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize config */
	ret = conf_init(&ph->conf);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize communications */
	priv->conn_listen.type = CONN_TYPE_TCP;
	ret = conn_init(&priv->conn_listen);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize registration service */
	ret = registration_service_init(&priv->reg_service);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize the usable_clients mutex */
	ret = mutex_init(&priv->usable_clients_mutex);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize the idle_clients mutex */
	ret = mutex_init(&priv->idle_clients_mutex);
	if (ret < 0)
		goto proxy_init_exit;

	/* Initialize the idle_workers mutex */
	ret = mutex_init(&priv->idle_workers_mutex);
	if (ret < 0)
		goto proxy_init_exit;

	return 0;

proxy_init_exit:
	proxy_free(ph);

	return ret;
}

void proxy_free(struct proxy_handle *ph)
{
	if (ph->priv != NULL) {
		struct proxy_priv *priv = ph->priv;

		proxy_close(ph);

		/* Free idle_workers mutex */
		mutex_free(&priv->idle_workers_mutex);

		/* Free idle_clients mutex */
		mutex_free(&priv->idle_clients_mutex);

		/* Free usable_clients mutex */
		mutex_free(&priv->usable_clients_mutex);

		/* Free registration service */
		registration_service_free(&priv->reg_service);

		/* Free connections */
		conn_free(&priv->conn_listen);

		/* Free config */
		conf_free(&ph->conf);

		/* Free logger */
		log_free(&priv->log);

		if (priv->re_calls_allowed != NULL) {
			regex_free(priv->re_calls_allowed);
			free(priv->re_calls_allowed);
		}

		if (priv->re_calls_denied != NULL) {
			regex_free(priv->re_calls_denied);
			free(priv->re_calls_denied);
		}

		/* Free RNG */
		rand_free();

		free(ph->priv);
		ph->priv = NULL;
	}
}

int proxy_open(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	int i;
	int ret;

	priv->num_clients = 1 + ph->conf.bind_addr_ext_add_len;

	priv->clients = calloc(priv->num_clients, sizeof(*priv->clients));
	if (priv->clients == NULL)
		return -ENOMEM;

	priv->client_workers = calloc(priv->num_clients,
				      sizeof(struct proxy_worker));
	if (priv->client_workers == NULL) {
		ret = -ENOMEM;
		goto proxy_open_exit;
	}

	ret = log_open(&priv->log);
	if (ret < 0)
		goto proxy_open_exit;

	if (ph->conf.calls_allowed != NULL) {
		if (priv->re_calls_allowed == NULL) {
			priv->re_calls_allowed = calloc(1,
				sizeof(*priv->re_calls_allowed));
			if (priv->re_calls_allowed == NULL) {
				ret = -ENOMEM;
				goto proxy_open_exit;
			}

			ret = regex_init(priv->re_calls_allowed);
			if (ret < 0) {
				proxy_log(ph, LOG_LEVEL_FATAL,
					  "Failed to initialize allowed callsigns regex (%d): %s\n",
					  -ret, strerror(-ret));
				goto proxy_open_exit;
			}
		}

		ret = regex_compile(priv->re_calls_allowed,
				    ph->conf.calls_allowed);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to compile allowed callsigns regex (%d): %s\n",
				  -ret, strerror(-ret));
			goto proxy_open_exit;
		}
	} else if (priv->re_calls_allowed != NULL) {
		regex_free(priv->re_calls_allowed);
		free(priv->re_calls_allowed);
		priv->re_calls_allowed = NULL;
	}

	if (ph->conf.calls_denied != NULL) {
		if (priv->re_calls_denied == NULL) {
			priv->re_calls_denied = calloc(1,
				sizeof(*priv->re_calls_denied));
			if (priv->re_calls_denied == NULL) {
				ret = -ENOMEM;
				goto proxy_open_exit;
			}

			ret = regex_init(priv->re_calls_denied);
			if (ret < 0) {
				proxy_log(ph, LOG_LEVEL_FATAL,
					  "Failed to initialize denied callsigns regex (%d): %s\n",
					  -ret, strerror(-ret));
				goto proxy_open_exit;
			}
		}

		ret = regex_compile(priv->re_calls_denied,
				    ph->conf.calls_denied);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to compile denied callsigns regex (%d): %s\n",
				  -ret, strerror(-ret));
			goto proxy_open_exit;
		}
	} else if (priv->re_calls_denied != NULL) {
		regex_free(priv->re_calls_denied);
		free(priv->re_calls_denied);
		priv->re_calls_denied = NULL;
	}

	priv->clients[0].source_addr = ph->conf.bind_addr_ext;

	priv->clients[0].prev_ptr = &priv->idle_clients_head;
	priv->idle_clients_head = &priv->clients[0];

	for (i = 1; i < priv->num_clients; i++) {
		priv->clients[i].source_addr = ph->conf.bind_addr_ext_add[i - 1];
		priv->clients[i - 1].next = &priv->clients[i];
		priv->clients[i].prev_ptr = &priv->clients[i - 1].next;
	}

	priv->clients[i - 1].next = NULL;
	priv->idle_clients_tail_ptr = &priv->clients[i - 1].next;

	for (i = 0; i < priv->num_clients; i++) {
		priv->clients[i].ph = ph;
		ret = proxy_conn_init(&priv->clients[i]);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to initialize proxy connection #%d (%d): %s\n",
				  i, -ret, strerror(-ret));

			for (i--; i >= 0; i--)
				proxy_conn_free(&priv->clients[i]);

			goto proxy_open_exit;
		}
	}

	for (i = 0; i < priv->num_clients; i++) {
		priv->client_workers[i].ph = ph;
		ret = proxy_worker_init(&priv->client_workers[i]);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to initialize proxy client worker #%d (%d): %s\n",
				  i, -ret, strerror(-ret));

			for (i--; i >= 0; i--)
				proxy_worker_free(&priv->client_workers[i]);

			goto proxy_open_exit_late;
		}

		priv->client_workers[i].next = priv->idle_workers_head;
		priv->idle_workers_head = &priv->client_workers[i];
	}

	priv->conn_listen.source_addr = (const char *)ph->conf.bind_addr;
	priv->conn_listen.source_port = (const char *)priv->port_str;

	ret = conn_listen(&priv->conn_listen);
	if (ret < 0) {
		proxy_log(ph, LOG_LEVEL_FATAL,
			  "Failed to open listening port (%d): %s\n",
			  -ret, strerror(-ret));
		goto proxy_open_exit_later;
	}

	if (ph->conf.bind_addr == NULL)
		proxy_log(ph, LOG_LEVEL_INFO,
			  "Listening for connections on port %s\n",
			  priv->port_str);
	else
		proxy_log(ph, LOG_LEVEL_INFO,
			  "Listening for connections at %s:%s\n",
			  ph->conf.bind_addr,
			  priv->port_str);

	return 0;

proxy_open_exit_later:
	priv->idle_workers_head = NULL;
	for (i = 0; i < priv->num_clients; i++)
		proxy_worker_free(&priv->client_workers[i]);

proxy_open_exit_late:
	priv->idle_clients_head = NULL;
	priv->idle_clients_tail_ptr = NULL;
	for (i = 0; i < priv->num_clients; i++)
		proxy_conn_free(&priv->clients[i]);

proxy_open_exit:
	if (priv->re_calls_allowed != NULL) {
		regex_free(priv->re_calls_allowed);
		free(priv->re_calls_allowed);
		priv->re_calls_allowed = NULL;
	}

	log_close(&priv->log);

	free(priv->client_workers);
	priv->client_workers = NULL;

	free(priv->clients);
	priv->clients = NULL;

	priv->num_clients = 0;

	return ret;
}

void proxy_close(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	int i;
	int ret;

	ret = registration_service_stop(&priv->reg_service);
	if (ret < 0)
		proxy_log(ph, LOG_LEVEL_ERROR,
			  "Failed to stop registration service (%d): %s\n",
			  -ret, strerror(-ret));

	proxy_shutdown(ph);
	proxy_drop(ph);

	proxy_log(ph, LOG_LEVEL_DEBUG, "Closing client connections...\n");

	priv->idle_workers_head = NULL;
	for (i = 0; i < priv->num_clients; i++)
		proxy_worker_free(&priv->client_workers[i]);

	priv->idle_clients_head = NULL;
	priv->idle_clients_tail_ptr = NULL;
	for (i = 0; i < priv->num_clients; i++)
		proxy_conn_free(&priv->clients[i]);

	free(priv->client_workers);
	priv->client_workers = NULL;
	free(priv->clients);
	priv->clients = NULL;
	priv->num_clients = 0;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Closing listening connection...\n");

	conn_close(&priv->conn_listen);

	proxy_log(ph, LOG_LEVEL_DEBUG, "Proxy is down - closing log.\n");

	log_close(&priv->log);
}

void proxy_drop(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	int i;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Dropping all clients...\n");

	for (i = 0; i < priv->num_clients; i++)
		proxy_conn_drop(&priv->clients[i]);
}

void proxy_shutdown(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Proxy shutdown requested.\n");

	mutex_lock(&priv->usable_clients_mutex);
	priv->usable_clients = 0;
	mutex_unlock(&priv->usable_clients_mutex);

	proxy_update_registration(ph);

	conn_shutdown(&priv->conn_listen);
}

void proxy_log(struct proxy_handle *ph, enum LOG_LEVEL lvl,
	       const char *fmt, ...)
{
	struct proxy_priv *priv = ph->priv;
	va_list args;

	if (priv == NULL || (unsigned int)lvl > priv->log.level)
		return;

	va_start(args, fmt);
	log_vprintf(&priv->log, lvl, fmt, args);
	va_end(args);
}

void proxy_log_level(struct proxy_handle *ph, enum LOG_LEVEL lvl)
{
	struct proxy_priv *priv = ph->priv;

	priv->log.level = lvl;
}

int proxy_log_select_medium(struct proxy_handle *ph, enum LOG_MEDIUM medium,
			    const char *target)
{
	struct proxy_priv *priv = ph->priv;
	int ret;

	ret = log_select_medium(&priv->log, medium, target);
	if (medium != LOG_MEDIUM_NONE && ret == 0)
		log_ident(&priv->log);

	return ret;
}

int proxy_process(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	struct conn_handle *conn;
	struct proxy_worker *worker = NULL;
	int ret = -EBUSY;
	char remote_addr[54] = { 0 };

	conn = calloc(1, sizeof(*conn));
	if (conn == NULL)
		return -ENOMEM;

	ret = conn_init(conn);
	if (ret < 0) {
		free(conn);
		return ret;
	}

	proxy_log(ph, LOG_LEVEL_DEBUG, "Waiting for a client...\n");

	ret = conn_accept(&priv->conn_listen, conn);
	if (ret < 0)
		goto conn_process_exit;

	conn_get_remote_addr(conn, remote_addr);
	proxy_log(ph, LOG_LEVEL_DEBUG, "Incoming connection from %s.\n",
		  remote_addr);

	mutex_lock_shared(&priv->usable_clients_mutex);
	mutex_lock(&priv->idle_workers_mutex);
	if (priv->usable_clients > 0 && priv->idle_workers_head != NULL) {
		worker = priv->idle_workers_head;
		priv->idle_workers_head = worker->next;
	}
	mutex_unlock(&priv->idle_workers_mutex);
	mutex_unlock_shared(&priv->usable_clients_mutex);

	if (worker == NULL) {
		proxy_log(ph, LOG_LEVEL_INFO,
			  "Dropping client because there are no available slots.\n");
		ret = 0;
		goto conn_process_exit;
	}

	ret = proxy_worker_accept(worker, conn);
	if (ret < 0)
		goto conn_process_exit;

	return 0;

conn_process_exit:
	conn_free(conn);
	free(conn);

	return ret;
}

int get_nonce(uint32_t *nonce)
{
	return rand_get(nonce);
}

int get_password_response(uint32_t nonce, const char *password,
			  uint8_t response[PROXY_PASS_RES_LEN])
{
	unsigned int pass_with_nonce_len = (unsigned int)strlen(password) + 8;
	uint8_t *pass_with_nonce = malloc(pass_with_nonce_len);
	char *iter = (char *)pass_with_nonce;

	while (*password != '\0') {
		if (*password >= 97 && *password <= 122)
			*iter = *password - 32;
		else
			*iter = *password;

		iter++;
		password++;
	}

	digest_to_hex32(nonce, iter);

	digest_get(pass_with_nonce, pass_with_nonce_len, response);

	free(pass_with_nonce);

	return 0;
}

int proxy_start(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	int ret;
	int i;

	for (i = 0; i < priv->num_clients; i++) {
		ret = proxy_conn_start(&priv->clients[i]);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to start proxy connection #%d (%d): %s\n",
				  i, -ret, strerror(-ret));
			goto proxy_start_exit_early;
		}
	}

	for (i = 0; i < priv->num_clients; i++) {
		ret = worker_start(&priv->client_workers[i].worker);
		if (ret < 0) {
			proxy_log(ph, LOG_LEVEL_FATAL,
				  "Failed to start proxy worker #%d (%d): %s\n",
				  i, -ret, strerror(-ret));
			goto proxy_start_exit;
		}
	}

	mutex_lock(&priv->usable_clients_mutex);
	priv->usable_clients = priv->num_clients;
	mutex_unlock(&priv->usable_clients_mutex);

	proxy_update_registration(ph);
	ret = registration_service_start(&priv->reg_service, &ph->conf);
	if (ret < 0) {
		proxy_log(ph, LOG_LEVEL_FATAL,
			  "Failed to start registration service (%d): %s\n",
			  -ret, strerror(-ret));
		goto proxy_start_exit;
	}

	return 0;

proxy_start_exit:
	for (i--; i >= 0; i--)
		worker_join(&priv->client_workers[i].worker);

	i = priv->num_clients;

proxy_start_exit_early:
	for (i--; i >= 0; i--)
		proxy_conn_stop(&priv->clients[i]);

	return ret;
}

void proxy_update_registration(struct proxy_handle *ph)
{
	struct proxy_priv *priv = ph->priv;
	struct proxy_worker *pw;
	int slots_used;
	int slots_total;

	mutex_lock_shared(&priv->usable_clients_mutex);
	slots_total = priv->usable_clients;
	slots_used = priv->num_clients;
	mutex_unlock_shared(&priv->usable_clients_mutex);

	mutex_lock_shared(&priv->idle_workers_mutex);
	for (pw = priv->idle_workers_head; pw != NULL; pw = pw->next)
		slots_used--;
	mutex_unlock_shared(&priv->idle_workers_mutex);

	proxy_log(ph, LOG_LEVEL_DEBUG,
		  "Sending update to registrar (%d/%d)\n",
		  slots_used, slots_total);

	registration_service_update(&priv->reg_service, slots_used,
				    slots_total);
}
