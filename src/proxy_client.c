/*!
 * @file proxy_client.c
 *
 * @copyright
 * Copyright &copy; 2020, Scott K Logan
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
 * @brief Client connection implementation
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "conn.h"
#include "digest.h"
#include "openelp/openelp.h"
#include "proxy_client.h"

/*!
 * @brief Private data for an instance of a client connection
 */
struct proxy_client_priv {
	/*! The connection to the proxy server */
	struct conn_handle conn;
};

int proxy_client_connect(struct proxy_client_handle *ch)
{
	struct proxy_client_priv *priv = ch->priv;
	const char *password = ch->password;
	uint32_t nonce;
	char nonce_str[8];
	uint8_t response[PROXY_PASS_RES_LEN];
	int ret;

	if (password == NULL)
		password = "PUBLIC";

	ret = conn_connect(&priv->conn, ch->host_addr, ch->host_port);
	if (ret < 0)
		goto proxy_client_connection_exit;

	/* Receive the nonce */
	ret = conn_recv(&priv->conn, (uint8_t *)nonce_str,
			sizeof(nonce_str));
	if (ret < 0)
		goto proxy_client_connection_exit;

	nonce = hex32_to_digest(nonce_str);

	/* Compute the password response */
	ret = get_password_response(nonce, password, response);
	if (ret < 0)
		goto proxy_client_connection_exit;

	/* Send the callsign, a newline, and the response */
	if (ch->callsign != NULL) {
		ret = conn_send(&priv->conn, (uint8_t *)ch->callsign,
				strlen(ch->callsign));
		if (ret < 0)
			goto proxy_client_connection_exit;
	}

	ret = conn_send(&priv->conn, (uint8_t *)"\n", 1);
	if (ret < 0)
		goto proxy_client_connection_exit;
		
	ret = conn_send(&priv->conn, response, PROXY_PASS_RES_LEN);
	if (ret < 0)
		goto proxy_client_connection_exit;

	/* No news is good news, so we won't know how things went
	 * until we actually try to process the returning messages
	 */

	return 0;

proxy_client_connection_exit:
	conn_close(&priv->conn);

	return ret;
}

void proxy_client_disconnect(struct proxy_client_handle *ch)
{
	struct proxy_client_priv *priv = ch->priv;

	conn_close(&priv->conn);
}

void proxy_client_free(struct proxy_client_handle *ch)
{
	struct proxy_client_priv *priv = ch->priv;

	if (ch->priv != NULL) {
		proxy_client_disconnect(ch);

		conn_free(&priv->conn);

		free(ch->priv);
		ch->priv = NULL;
	}
}

int proxy_client_init(struct proxy_client_handle *ch)
{
	struct proxy_client_priv *priv = ch->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		ch->priv = priv;
	}

	priv->conn.type = CONN_TYPE_TCP;
	ret = conn_init(&priv->conn);
	if (ret < 0)
		goto proxy_client_init_exit;

	return 0;

proxy_client_init_exit:
	conn_free(&priv->conn);

	free(ch->priv);
	ch->priv = NULL;

	return ret;
}
