/*!
 * @file test_e2e.c
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
 * @brief End-to-end test of a proxy connection
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "openelp/openelp.h"
#include "proxy_client.h"
#include "worker.h"

#if _WIN32
#  define strdup _strdup
#endif

/*! Context for ::proxy_processor */
struct processor_context
{
	/*! Handle to the proxy instance to process messages for */
	struct proxy_handle *ph;

	/*! Return value from the most recent processing run */
	int ret;
};

/*!
 * @brief Worker function for processing proxy server messages
 *
 * @param[in,out] wh The worker context
 */
static void proxy_processor(struct worker_handle *wh);

/*!
 * @brief Test basic proxy lifecycle and functions
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test basic proxy lifecycle and functions
 */
static int test_proxy_e2e(void);

/*!
 * @brief Main entry point for authorization tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

int main(void)
{
	int ret = 0;

	ret |= test_proxy_e2e();

	return ret;
}

static void proxy_processor(struct worker_handle *wh)
{
	struct processor_context *ctx = wh->func_ctx;

	ctx->ret = proxy_process(ctx->ph);
}

static int test_proxy_e2e(void)
{
	struct proxy_client_handle client = { 0 };
	struct proxy_client_handle client2 = { 0 };
	struct proxy_handle proxy = { 0 };
	struct worker_handle worker = { 0 };
	struct processor_context ctx = { 0 };
	int ret;

	/* Initialize */

	ctx.ph = &proxy;
	ctx.ret = 0;
	worker.func_ptr = proxy_processor;
	worker.func_ctx = &ctx;
	ret = worker_init(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	ret = proxy_init(&proxy);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	client.callsign = "KM0H";
	client.host_addr = "127.0.0.1";
	client.host_port = "8100";
	client.password = "PUBLIC";
	ret = proxy_client_init(&client);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	client2.callsign = "KM0H";
	client2.host_addr = "127.0.0.1";
	client2.host_port = "8100";
	client2.password = "PUBLIC";
	ret = proxy_client_init(&client2);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	/* Start the proxy server */

	proxy_log_level(&proxy, LOG_LEVEL_WARN);

	proxy.conf.bind_addr = strdup("127.0.0.1");
	proxy.conf.bind_addr_ext = strdup("127.0.0.1");
	proxy.conf.calls_allowed = strdup("^KM0H$");
	proxy.conf.password = strdup("PUBLIC");
	proxy.conf.port = 8100;
	ret = proxy_open(&proxy);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	ret = proxy_start(&proxy);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	ret = worker_start(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	/* Try to connect and authorize */

	ret = worker_wake(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	ret = proxy_client_connect(&client);
	if (ret < 0) {
		fprintf(stderr, "Failed to connect to the client (%d): %s\n",
			-ret, strerror(-ret));
		goto test_proxy_authorize_exit;
	}

	ret = worker_wait_idle(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	/* Verify that the authorization succeeded */
	if (ctx.ret < 0) {
		fprintf(stderr, "Authorization failed (%d): %s\n",
			-ctx.ret, strerror(-ctx.ret));
	}

	/* Attempt another connection */

	ret = worker_wake(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	ret = proxy_client_connect(&client2);
	if (ret != -EPIPE) {
		fprintf(stderr, "Invalid busy signal (%d): %s\n",
			-ret, strerror(-ret));
		goto test_proxy_authorize_exit;
	}

	ret = worker_wait_idle(&worker);
	if (ret < 0)
		goto test_proxy_authorize_exit;

	/* Verify that the drop was handled correctly */
	if (ctx.ret < 0) {
		fprintf(stderr, "Busy client drop failed (%d): %s\n",
			-ctx.ret, strerror(-ctx.ret));
	}

test_proxy_authorize_exit:
	proxy_client_free(&client2);
	proxy_client_free(&client);
	proxy_free(&proxy);
	worker_free(&worker);

	return ret;
}
