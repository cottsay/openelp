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
#include "worker.h"

/*! Stringization macro - stage one */
#define OCH_STR1(x) #x

/*! Stringization macro - stage two */
#define OCH_STR2(x) OCH_STR1(x)

/*! Update (at least) every 10 minutes */
#define UPDATE_INTERVAL 600000

/*!
 * @brief Possible statuses of a proxy server to report to registrar
 */
enum REGISTRATION_STATUS {
	REGISTRATION_STATUS_UNKNOWN = 0,
	REGISTRATION_STATUS_READY,
	REGISTRATION_STATUS_BUSY,
	REGISTRATION_STATUS_OFF
};

/*!
 * @brief Private data for an instance of a proxy server registration service
 */
struct registration_service_priv {
	/*! The name of the proxy server to report */
	const char *reg_name;

	/*! A short comment about the proxy server to report */
	const char *reg_comment;

	/*! Pre-computed static portion of the update body */
	char *reg_suffix;

	/*! 'Y' to list the server for public access, otherwise 'N' */
	char public;

	/*! Mutex for protecting the status and slot members */
	struct mutex_handle mutex;

	/*! Handle to the worker thread which services update requests */
	struct worker_handle worker;

	/*! Maximum number of clients to report on the next update */
	size_t slots_total;

	/*! Number of connected clients to report on the next update */
	size_t slots_used;

	/*! Server status to report on the next update */
	enum REGISTRATION_STATUS status;
};

/*! First part of the HTTP message sent to the registrar */
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

/*! Host name or IP address of the registrar */
static const char http_host[] = "www.echolink.org";

/*! Salt used when computing the MD5 */
static const char digest_salt[] = "#5A!zu";

/*! Reported protocol version of this proxy server */
static const char protocol_version[] = "1.2.3o";

/*!
 * @brief Status phrases which are sent to the registrar
 *
 * These entries correspond to ::REGISTRATION_STATUS indexes.
 */
static const char * const status_phrase[] = {
	NULL,
	"Ready",
	"Busy",
	"Off",
};

/*!
 * @brief Worker function for registration updates
 *
 * @param[in,out] wh The handle to the worker object
 */
static void registration_func(struct worker_handle *wh);

/*!
 * @brief Reports status to the registrar
 *
 * @param[in,out] rs Target registration service instance
 * @param[in] status The status of the proxy to report
 * @param[in] slots_used The number of proxy server slots currently in use
 * @param[in] slots_total The maximum simultaneous clients the proxy can service
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int send_report(struct registration_service_handle *rs,
		       enum REGISTRATION_STATUS status, size_t slots_used,
		       size_t slots_total);

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
	const char *status_str = status_phrase[status];

	if (status_str == NULL)
		return -EINVAL;

	memset(&conn, 0x0, sizeof(conn));

	/* printf("Updating registration (%s %s, %lu/%lu)\n",
	 *	 priv->reg_name, status_str,
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
		(unsigned long)slots_total, priv->public, status_str,
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

		worker_free(&priv->worker);
		mutex_free(&priv->mutex);

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

	ret = mutex_init(&priv->mutex);
	if (ret != 0)
		goto registration_service_init_exit;

	priv->worker.func_ctx = rs;
	priv->worker.func_ptr = registration_func;
	priv->worker.periodic_wake = UPDATE_INTERVAL;
	priv->worker.stack_size = 1024 * 1024;
	ret = worker_init(&priv->worker);
	if (ret != 0)
		goto registration_service_init_exit;

	if (priv->status == REGISTRATION_STATUS_OFF)
		priv->status = REGISTRATION_STATUS_UNKNOWN;

	return 0;

registration_service_init_exit:
	worker_free(&priv->worker);
	mutex_free(&priv->mutex);

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

	ret = worker_start(&priv->worker);
	if (ret < 0)
		goto registration_service_start_end;

	ret = worker_wake(&priv->worker);
	if (ret < 0)
		goto registration_service_start_end;

	if (priv->status == REGISTRATION_STATUS_OFF)
		priv->status = REGISTRATION_STATUS_UNKNOWN;

registration_service_start_end:
	mutex_unlock(&priv->mutex);

	free(reg_suffix);

	return ret;
}

int registration_service_stop(struct registration_service_handle *rs)
{
	struct registration_service_priv *priv = rs->priv;

	mutex_lock(&priv->mutex);
	priv->status = REGISTRATION_STATUS_OFF;
	worker_wake(&priv->worker);
	mutex_unlock(&priv->mutex);

	return worker_join(&priv->worker);
}

void registration_service_update(struct registration_service_handle *rs,
				 size_t slots_used, size_t slots_total)
{
	struct registration_service_priv *priv = rs->priv;

	mutex_lock(&priv->mutex);
	if (priv->status < REGISTRATION_STATUS_OFF) {
		priv->status = slots_used >= slots_total ?
			       REGISTRATION_STATUS_BUSY :
			       REGISTRATION_STATUS_READY;
		priv->slots_used = slots_used;
		priv->slots_total = slots_total;
		worker_wake(&priv->worker);
	}
	mutex_unlock(&priv->mutex);
}

static void registration_func(struct worker_handle *wh)
{
	struct registration_service_handle *rs = wh->func_ctx;
	struct registration_service_priv *priv = rs->priv;

	int ret;
	size_t slots_total;
	size_t slots_used;
	enum REGISTRATION_STATUS status;

	mutex_lock(&priv->mutex);

	slots_total = priv->slots_total;
	slots_used = priv->slots_used;
	status = priv->status;

	mutex_unlock(&priv->mutex);

	if (status <= REGISTRATION_STATUS_UNKNOWN)
		return;

	ret = send_report(rs, status, slots_used, slots_total);
	if (ret < 0) {
		/* printf("Proxy registration failed (%d): %s\n",
		 *	  -ret, strerror(-ret));
		 */
	}
}
