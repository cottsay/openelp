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

struct proxy_priv
{
	struct proxy_conn_handle *clients;
	struct conn_handle conn_listen;
	struct log_handle log;
	int num_clients;
	struct regex_handle *re_calls_allowed;
	struct regex_handle *re_calls_denied;
};

/*
 * Functions
 */
int proxy_authorize(struct proxy_handle *ph, const char *callsign)
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
	return conf_parse_file(path, &ph->conf);
}

void proxy_ident(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;

	log_ident(&priv->log);
}

int proxy_init(struct proxy_handle *ph)
{
	int ret;

	if (ph != NULL)
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

	priv->num_clients = 1;
	priv->clients = malloc(sizeof(struct proxy_conn_handle) * priv->num_clients);
	if (priv->clients == NULL)
	{
		return -ENOMEM;
	}

	memset(priv->clients, 0x0, sizeof(struct proxy_conn_handle) * priv->num_clients);

	ret = log_open(&priv->log, NULL);
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

	for (i = 0; i < priv->num_clients; i++)
	{
		ret = proxy_conn_start(&priv->clients[i]);
		if (ret < 0)
		{
			proxy_log(ph, LOG_LEVEL_FATAL, "Failed to start proxy connection #%d (%d): %s\n", i, -ret, strerror(-ret));
			goto proxy_open_exit_late;
		}
	}

	ret = conn_listen(&priv->conn_listen, ph->conf.port);
	if (ret < 0)
	{
		proxy_log(ph, LOG_LEVEL_FATAL, "Failed to open listening port (%d): %s\n", -ret, strerror(-ret));
		goto proxy_open_exit_late;
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

	for (i = 0; i < priv->num_clients; i++)
	{
		proxy_conn_free(&priv->clients[i]);
	}
	free(priv->clients);
	priv->clients = NULL;

	conn_close(&priv->conn_listen);
	log_close(&priv->log);
}

void proxy_drop(struct proxy_handle *ph)
{
	struct proxy_priv *priv = (struct proxy_priv *)ph->priv;
	int i;

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
	if (ret == 0)
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

	ret = conn_accept(&priv->conn_listen, conn);
	if (ret < 0)
	{
		goto conn_process_exit;
	}

	proxy_log(ph, LOG_LEVEL_INFO, "Incoming connection from TODO.\n");

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

