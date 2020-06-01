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

#include "openelp/openelp.h"

#include "conf.h"
#include "conn.h"
#include "digest.h"
#include "log.h"
#include "proxy_conn.h"
#include "rand.h"
#include "regex.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PROXY_PASS_RES_LEN != DIGEST_LEN
#error Password Response Length Mismatch
#endif

/*!
 * @brief Private data for an instance of an EchoLink proxy
 */
struct proxy_priv
{
	/// Array which holds all of the proxy client connection handles
	struct proxy_conn_handle *clients;

	/// Network connection which listens for connections from clients
	struct conn_handle conn_listen;

	/// Logging infrastructure handle
	struct log_handle log;

	/// Number of clients in proxy_priv::clients
	int num_clients;

	/// Regular expression for matching allowed callsigns
	struct regex_handle *re_calls_allowed;

	/// Regular expression for matching denied callsigns
	struct regex_handle *re_calls_denied;

	/// Null-terminated string which holds the listening port identifier
	char port_str[6];
};

/*!
 * @brief Convert a port number to an ASCII string
 *
 * @param[in] port Port number to convert
 * @param[out] result Resulting ASCII string
 */
static inline void port_to_str(const uint16_t port, char result[6]);

static inline void port_to_str(const uint16_t port, char result[6])
{
	uint16_t port_tmp = port;
	uint8_t n = 0;

	do
	{
		n++;
		port_tmp /= 10;
	}
	while (port_tmp != 0);

	port_tmp = port;

	switch (n)
	{
	case 5:
		*result = 48 + port_tmp / 10000;
		port_tmp %= 10000;
		result++;
		// fall through
	case 4:
		*result = 48 + port_tmp / 1000;
		port_tmp %= 1000;
		result++;
		// fall through
	case 3:
		*result = 48 + port_tmp / 100;
		port_tmp %= 100;
		result++;
		// fall through
	case 2:
		*result = 48 + port_tmp / 10;
		port_tmp %= 10;
		result++;
		// fall through
	case 1:
		*result = 48 + port_tmp;
		result++;
	}

	*result = '\0';
}

int proxy_authorize_callsign(struct proxy_handle *ph, const char *callsign)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	if (priv->re_calls_denied != NULL)
	{
		ret = regex_is_match(priv->re_calls_denied, callsign);
		if (ret != 0)
		{
			if (ret < 0)
			{
				proxy_log(ph, LOG_LEVEL_WARN, "Failed to match callsign '%s' against denial pattern (%d): %s\n", callsign, -ret, strerror(-ret));
			}

			return 0;
		}
	}

	if (priv->re_calls_allowed != NULL)
	{
		ret = regex_is_match(priv->re_calls_allowed, callsign);
		if (ret != 1)
		{
			if (ret < 0)
			{
				proxy_log(ph, LOG_LEVEL_WARN, "Failed to match callsign '%s' against allowing pattern (%d): %s\n", callsign, -ret, strerror(-ret));
			}

			return 0;
		}
	}

	return 1;
}

int proxy_load_conf(struct proxy_handle *ph, const char *path)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	ret = conf_parse_file(path, &ph->conf, &priv->log);
	if (ret < 0)
	{
		return ret;
	}

	port_to_str(ph->conf.port, priv->port_str);

	if (ph->conf.bind_addr_ext_add != NULL)
	{
		if (ph->conf.bind_addr_ext == NULL || strcmp(ph->conf.bind_addr_ext, "0.0.0.0") == 0)
		{
			proxy_log(ph, LOG_LEVEL_ERROR, "ExternalBindAddresses must be specified if AdditionalExternalBindAddresses is used\n");
			return -EINVAL;
		}
	}

	return 0;
}

void proxy_ident(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	log_ident(&priv->log);
}

int proxy_init(struct proxy_handle *ph)
{
	struct proxy_priv *priv;
	int ret;

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

	// Initialize config
	ret = conf_init(&ph->conf);
	if (ret < 0)
	{
		goto proxy_init_exit;
	}

	// Initialize communications
	priv->conn_listen.type = CONN_TYPE_TCP;
	ret = conn_init(&priv->conn_listen);
	if (ret < 0)
	{
		goto proxy_init_exit;
	}

	priv->num_clients = 0;

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

		// Free connections
		conn_free(&priv->conn_listen);

		// Free config
		conf_free(&ph->conf);

		// Free logger
		log_free(&priv->log);

		if (priv->re_calls_allowed != NULL)
		{
			regex_free(priv->re_calls_allowed);
			free(priv->re_calls_allowed);
		}

		if (priv->re_calls_denied != NULL)
		{
			regex_free(priv->re_calls_denied);
			free(priv->re_calls_denied);
		}

		// Free RNG
		rand_free();

		free(ph->priv);
		ph->priv = NULL;
	}
}

int proxy_open(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int i;
	int ret;

	priv->num_clients = 1 + ph->conf.bind_addr_ext_add_len;

	priv->clients = malloc(sizeof(struct proxy_conn_handle) * priv->num_clients);
	if (priv->clients == NULL)
	{
		return -ENOMEM;
	}

	memset(priv->clients, 0x0, sizeof(struct proxy_conn_handle) * priv->num_clients);

	ret = log_open(&priv->log);
	if (ret < 0)
	{
		goto proxy_open_exit;
	}

	if (ph->conf.calls_allowed != NULL)
	{
		if (priv->re_calls_allowed == NULL)
		{
			priv->re_calls_allowed = malloc(sizeof(struct regex_handle));
			if (priv->re_calls_allowed == NULL)
			{
				ret = -ENOMEM;
				goto proxy_open_exit;
			}

			memset(priv->re_calls_allowed, 0x0, sizeof(struct regex_handle));

			ret = regex_init(priv->re_calls_allowed);
			if (ret < 0)
			{
				proxy_log(ph, LOG_LEVEL_FATAL, "Failed to initialize allowed callsigns regex (%d): %s\n", -ret, strerror(-ret));
				goto proxy_open_exit;
			}
		}

		ret = regex_compile(priv->re_calls_allowed, ph->conf.calls_allowed);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_FATAL, "Failed to compile allowed callsigns regex (%d): %s\n", -ret, strerror(-ret));
			goto proxy_open_exit;
		}
	}
	else if (priv->re_calls_allowed != NULL)
	{
		regex_free(priv->re_calls_allowed);
		free(priv->re_calls_allowed);
		priv->re_calls_allowed = NULL;
	}

	if (ph->conf.calls_denied != NULL)
	{
		if (priv->re_calls_denied == NULL)
		{
			priv->re_calls_denied = malloc(sizeof(struct regex_handle));
			if (priv->re_calls_denied == NULL)
			{
				ret = -ENOMEM;
				goto proxy_open_exit;
			}

			memset(priv->re_calls_denied, 0x0, sizeof(struct regex_handle));

			ret = regex_init(priv->re_calls_denied);
			if (ret < 0)
			{
				proxy_log(ph, LOG_LEVEL_FATAL, "Failed to initialize denied callsigns regex (%d): %s\n", -ret, strerror(-ret));
				goto proxy_open_exit;
			}
		}

		ret = regex_compile(priv->re_calls_denied, ph->conf.calls_denied);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_FATAL, "Failed to compile denied callsigns regex (%d): %s\n", -ret, strerror(-ret));
			goto proxy_open_exit;
		}
	}
	else if (priv->re_calls_denied != NULL)
	{
		regex_free(priv->re_calls_denied);
		free(priv->re_calls_denied);
		priv->re_calls_denied = NULL;
	}

	priv->clients[0].source_addr = ph->conf.bind_addr_ext;

	for (i = 1; i < priv->num_clients; i++)
	{
		priv->clients[i].source_addr = ph->conf.bind_addr_ext_add[i - 1];
	}

	for (i = 0; i < priv->num_clients; i++)
	{
		priv->clients[i].ph = ph;
		ret = proxy_conn_init(&priv->clients[i]);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_FATAL, "Failed to initialize proxy connection #%d (%d): %s\n", i, -ret, strerror(-ret));

			for (i--; i >= 0; i--)
			{
				proxy_conn_free(&priv->clients[i]);
			}

			goto proxy_open_exit;
		}
	}

	priv->conn_listen.source_addr = (const char *)ph->conf.bind_addr;
	priv->conn_listen.source_port = (const char *)priv->port_str;

	ret = conn_listen(&priv->conn_listen);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_FATAL, "Failed to open listening port (%d): %s\n", -ret, strerror(-ret));
		goto proxy_open_exit_late;
	}

	if (ph->conf.bind_addr == NULL)
	{
		proxy_log(ph, LOG_LEVEL_INFO, "Listening for connections on port %s\n", priv->port_str);
	}
	else
	{
		proxy_log(ph, LOG_LEVEL_INFO, "Listening for connections at %s:%s\n", ph->conf.bind_addr, priv->port_str);
	}

	return 0;

proxy_open_exit_late:
	for (i = 0; i < priv->num_clients; i++)
	{
		proxy_conn_free(&priv->clients[i]);
	}

proxy_open_exit:
	if (priv->re_calls_allowed != NULL)
	{
		regex_free(priv->re_calls_allowed);
		free(priv->re_calls_allowed);
		priv->re_calls_allowed = NULL;
	}

	log_close(&priv->log);

	free(priv->clients);
	priv->clients = NULL;

	priv->num_clients = 0;

	return ret;
}

void proxy_close(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int i;

	proxy_shutdown(ph);
	proxy_drop(ph);

	proxy_log(ph, LOG_LEVEL_DEBUG, "Closing client connections...\n");

	for (i = 0; i < priv->num_clients; i++)
	{
		proxy_conn_free(&priv->clients[i]);
	}

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
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int i;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Dropping all clients...\n");

	for (i = 0; i < priv->num_clients; i++)
	{
		proxy_conn_drop(&priv->clients[i]);
	}
}

void proxy_shutdown(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	proxy_log(ph, LOG_LEVEL_DEBUG, "Proxy shutdown requested.\n");

	conn_shutdown(&priv->conn_listen);
}

void proxy_log(struct proxy_handle *ph, enum LOG_LEVEL lvl, const char *fmt, ...)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	va_list args;

	if (priv == NULL || (unsigned)lvl > priv->log.level)
	{
		return;
	}

	va_start(args, fmt);
	log_vprintf(&priv->log, lvl, fmt, args);
	va_end(args);
}

void proxy_log_level(struct proxy_handle *ph, const enum LOG_LEVEL lvl)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	priv->log.level = lvl;
}

int proxy_log_select_medium(struct proxy_handle *ph, const enum LOG_MEDIUM medium, const char *target)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;

	ret = log_select_medium(&priv->log, medium, target);
	if (medium != LOG_MEDIUM_NONE && ret == 0)
	{
		log_ident(&priv->log);
	}

	return ret;
}

int proxy_process(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	struct conn_handle *conn = NULL;
	int ret = -EBUSY;
	int i;
	uint32_t remote_addr;
	uint16_t remote_port;
	char remote_addr_str[16] = { 0x0 };

	conn = malloc(sizeof(struct conn_handle));
	if (conn == NULL)
	{
		return -ENOMEM;
	}

	memset(conn, 0x0, sizeof(struct conn_handle));

	ret = conn_init(conn);
	if (ret < 0)
	{
		free(conn);
		return ret;
	}

	proxy_log(ph, LOG_LEVEL_DEBUG, "Waiting for a client...\n");

	ret = conn_accept(&priv->conn_listen, conn, &remote_addr, &remote_port);
	if (ret < 0)
	{
		goto conn_process_exit;
	}

	conn_sprintaddr(remote_addr, remote_addr_str);
	proxy_log(ph, LOG_LEVEL_INFO, "Incoming connection from %s:%u.\n", remote_addr_str, remote_port);

	for (i = 0; i < priv->num_clients; i++)
	{
		ret = proxy_conn_accept(&priv->clients[i], conn);
		if (ret != -EBUSY)
		{
			break;
		}
	}

	if (ret == -EBUSY)
	{
		proxy_log(ph, LOG_LEVEL_INFO, "Dropping client because there are no available slots.\n");
		ret = 0;
		goto conn_process_exit;
	}
	else if (ret < 0)
	{
		goto conn_process_exit;
	}

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

int get_password_response(const uint32_t nonce, const char *password, uint8_t response[PROXY_PASS_RES_LEN])
{
	unsigned int pass_with_nonce_len = (unsigned int)strlen(password) + 8;
	uint8_t *pass_with_nonce = malloc(pass_with_nonce_len);
	char *iter = (char *)pass_with_nonce;

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

	digest_to_hex32(nonce, iter);

	digest_get(pass_with_nonce, pass_with_nonce_len, response);

	free(pass_with_nonce);

	return 0;
}

int proxy_start(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int ret;
	int i;

	for (i = 0; i < priv->num_clients; i++)
	{
		ret = proxy_conn_start(&priv->clients[i]);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_FATAL, "Failed to start proxy connection #%d (%d): %s\n", i, -ret, strerror(-ret));
			for (i--; i >= 0; i--)
			{
				proxy_conn_stop(&priv->clients[i]);
			}

			return ret;
		}
	}

	return 0;
}
