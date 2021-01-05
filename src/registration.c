/*!
 * @file registration.c
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
 * @brief Implementation of proxy server registration
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openelp/openelp.h"
#include "digest.h"
#include "conn.h"
#include "mutex.h"
#include "registration.h"
#include "thread.h"

#define OCH_STR1(x) #x
#define OCH_STR2(x) OCH_STR1(x)

/*! Update (at least) every 10 minutes */
#define UPDATE_INTERVAL 600000

enum REGISTRATION_STATUS {
	REGISTRATION_STATUS_READY = 0,
	REGISTRATION_STATUS_BUSY,
	REGISTRATION_STATUS_OFF
};

enum REGISTRATION_FLAGS {
	REGISTRATION_FLAGS_NONE = 0,
	REGISTRATION_FLAG_SENTINEL = (1 << 0),
	REGISTRATION_FLAG_UPDATE = (1 << 1)
};

struct registration_service_priv {
	const char *reg_name;
	const char *reg_comment;
	char *reg_suffix;
	char public;

	struct condvar_handle condvar;
	struct mutex_handle mutex;
	struct thread_handle thread;

	size_t slots_total;
	size_t slots_used;
	enum REGISTRATION_STATUS status;
	enum REGISTRATION_FLAGS flags;
};

static const char http_message[] =
	"POST /proxypost.jsp HTTP/1.1\r\n"
	"Content-Type: application/x-www-form-urlencoded\r\n"
	"Cache-Control: no-cache\r\n"
	"Pragma: no-cache\r\n"
	"User-Agent: OpenELP/" OCH_STR2(OPENELP_VERSION) "\r\n"
	"Host: www.echolink.org\r\n"
	"Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\r\n"
	"Connection: keep-alive\r\n"
	"Content-Length: ";

static const char http_host[] = "www.echolink.org";

static const char digest_salt[] = "#5A!zu";

static const char protocol_version[] = "1.2.3o";

static const char * const status_phrase[] = {
	"Ready",
	"Busy",
	"Off",
};

static void *registration_thread(void *ctx);

static int send_report(struct registration_service_handle *rs,
		       enum REGISTRATION_STATUS status, size_t slots_used,
		       size_t slots_total)
{
	struct registration_service_priv *priv = rs->priv;
	struct conn_handle conn;
	int ret = 0;
	int header_length;
	int body_length = 0;
	char message_header[sizeof(http_message) + 14];
	char *message_body = NULL;

	memset(&conn, 0x0, sizeof(conn));

	/* printf("Updating registration (%s %s, %lu/%lu)\n",
	 *	 priv->reg_name, status_phrase[status],
	 *	 (unsigned long)slots_used, (unsigned long)slots_total);
	 */

	/*! @TODO URL encoding */

	/* Allocate a buffer we *know* will be big enough for the body */
	message_body = malloc(110 + sizeof(protocol_version) +
			      strlen(priv->reg_name) +
			      strlen(priv->reg_comment));
	if (message_body == NULL)
		return -ENOMEM;

	body_length = sprintf(
		message_body,
		"name=%s&comment=%s [%lu/%lu]&public=%c&status=%s%s",
		priv->reg_name, priv->reg_comment, (unsigned long)slots_used,
		(unsigned long)slots_total, priv->public, status_phrase[status],
		priv->reg_suffix);
	if (body_length <= 0) {
		ret = -EINVAL; /*! @TODO */
		goto registration_update_exit;
	}

	header_length = sprintf(message_header, "%s%d\r\n\r\n", http_message,
				body_length);
	if (header_length <= 0) {
		ret = -EINVAL; /*! @TODO */
		goto registration_update_exit;
	}

	conn.type = CONN_TYPE_TCP;
	ret = conn_init(&conn);
	if (ret < 0)
		goto registration_update_exit;

	ret = conn_connect(&conn, http_host, "80");
	if (ret < 0)
		goto registration_update_exit;

	ret = conn_send(&conn, (uint8_t *)message_header, header_length);
	if (ret < 0)
		goto registration_update_exit;

	ret = conn_send(&conn, (uint8_t *)message_body, body_length);
	if (ret < 0)
		goto registration_update_exit;

	ret = conn_recv(&conn, (uint8_t *)message_body, 13);
	if (ret < 0)
		goto registration_update_exit;

	if (strncmp(message_body, "HTTP/1.1 200 ", 13) != 0)
		ret = -EINVAL;

registration_update_exit:
	conn_free(&conn);

	free(message_body);

	return ret;
}

void registration_service_free(struct registration_service_handle *rs)
{
	if (rs->priv != NULL) {
		struct registration_service_priv *priv = rs->priv;

		registration_service_stop(rs);

		thread_free(&priv->thread);
		mutex_free(&priv->mutex);
		condvar_free(&priv->condvar);

		free((void *)priv->reg_suffix);

		free(rs->priv);
		rs->priv = NULL;
	}
}

int registration_service_init(struct registration_service_handle *rs)
{
	struct registration_service_priv *priv = rs->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		rs->priv = priv;
	}

	ret = condvar_init(&priv->condvar);
	if (ret != 0)
		goto registration_service_init_exit;

	ret = mutex_init(&priv->mutex);
	if (ret != 0)
		goto registration_service_init_exit;

	priv->thread.func_ctx = rs;
	priv->thread.func_ptr = registration_thread;
	priv->thread.stack_size = 1024 * 1024;
	ret = thread_init(&priv->thread);
	if (ret != 0)
		goto registration_service_init_exit;

	return 0;

registration_service_init_exit:
	thread_free(&priv->thread);
	mutex_free(&priv->mutex);
	condvar_free(&priv->condvar);

	free(rs->priv);
	rs->priv = NULL;

	return ret;
}

int registration_service_start(struct registration_service_handle *rs,
			       const struct proxy_conf *conf)
{
	struct registration_service_priv *priv = rs->priv;
	char *reg_suffix = NULL;
	uint8_t digest[DIGEST_LEN];
	const char *public_addr = conf->public_addr == NULL ?
				   "" : conf->public_addr;
	int ret;

	if (conf->reg_name == NULL)
		return 0;

	mutex_lock(&priv->mutex);

	priv->public = strcmp(conf->password, "PUBLIC") == 0 ? 'Y' : 'N';

	priv->reg_name = conf->reg_name;
	priv->reg_comment = conf->reg_comment;

	if (priv->reg_suffix != NULL)
		free((void *)priv->reg_suffix);
	reg_suffix = malloc(strlen(public_addr) + sizeof(protocol_version) + 50);
	if (reg_suffix == NULL) {
		ret = -ENOMEM;
		goto registration_service_start_end;
	}

	priv->flags &= ~REGISTRATION_FLAG_SENTINEL;

	ret = sprintf(reg_suffix, "%s%s%s", priv->reg_name, public_addr,
		      digest_salt);
	if (ret < 0)
		goto registration_service_start_end;

	digest_get((uint8_t *)reg_suffix, ret, digest);

	ret = sprintf(reg_suffix, "&a=%s&d=", public_addr);
	if (ret < 0)
		goto registration_service_start_end;

	digest_to_str(digest, &reg_suffix[ret]);

	ret = sprintf(&reg_suffix[ret + 32], "&p=%d&v=%s", conf->port,
		      protocol_version);
	if (ret < 0)
		goto registration_service_start_end;

	priv->reg_suffix = reg_suffix;
	reg_suffix = NULL;

	ret = thread_start(&priv->thread);

registration_service_start_end:
	mutex_unlock(&priv->mutex);

	free(reg_suffix);

	return ret;
}

int registration_service_stop(struct registration_service_handle *rs)
{
	struct registration_service_priv *priv = rs->priv;

	mutex_lock(&priv->mutex);
	priv->flags |= REGISTRATION_FLAG_SENTINEL | REGISTRATION_FLAG_UPDATE;
	priv->status = REGISTRATION_STATUS_OFF;
	condvar_wake_all(&priv->condvar);
	mutex_unlock(&priv->mutex);

	return thread_join(&priv->thread);
}

void registration_service_update(struct registration_service_handle *rs,
				 size_t slots_used, size_t slots_total)
{
	struct registration_service_priv *priv = rs->priv;

	mutex_lock(&priv->mutex);
	if (!(priv->flags & REGISTRATION_FLAG_SENTINEL)) {
		priv->status = slots_used >= slots_total ?
			       REGISTRATION_STATUS_BUSY :
			       REGISTRATION_STATUS_READY;
		priv->slots_used = slots_used;
		priv->slots_total = slots_total;
		priv->flags |= REGISTRATION_FLAG_UPDATE;
		condvar_wake_all(&priv->condvar);
	}
	mutex_unlock(&priv->mutex);
}

static void *registration_thread(void *ctx)
{
	struct thread_handle *th = ctx;
	struct registration_service_handle *rs = th->func_ctx;
	struct registration_service_priv *priv = rs->priv;

	int ret;
	size_t slots_total;
	size_t slots_used;
	enum REGISTRATION_STATUS status;

	mutex_lock(&priv->mutex);

	while (1) {
		slots_total = priv->slots_total;
		slots_used = priv->slots_used;
		status = priv->status;
		priv->flags &= ~REGISTRATION_FLAG_UPDATE;

		mutex_unlock(&priv->mutex);
		ret = send_report(rs, status, slots_used, slots_total);
		if (ret < 0) {
			/* printf("Proxy registration failed (%d): %s\n",
			 *      -ret, strerror(-ret));
			 */
		}
		mutex_lock(&priv->mutex);

		if (priv->flags & REGISTRATION_FLAG_UPDATE)
			continue;

		if (priv->flags & REGISTRATION_FLAG_SENTINEL)
			break;

		condvar_wait_time(&priv->condvar, &priv->mutex,
				  UPDATE_INTERVAL);
	}

	priv->flags = REGISTRATION_FLAGS_NONE;

	mutex_unlock(&priv->mutex);

	return NULL;
}
