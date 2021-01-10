/*!
 * @file test_conn.c
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
 * @brief Tests related to network connections
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#endif

#include "conn.h"
#include "thread.h"

/*!
 * @brief Contextual data for a thread which receives some data and terminates.
 */
struct conn_recv_data {
	/*! The handle of the connection which receives the data */
	struct conn_handle conn;

	/*! The thread which performs the receive operation */
	struct thread_handle thread;

	/*! The return code of the receive operation */
	int ret;
};

/*!
 * @brief Thread function which performs a blocking receive
 *
 * @param[in,out] ctx The thread context
 *
 * @returns Always returns NULL
 */
static void *conn_recv_func(void *ctx);

/*!
 * @brief Basic test for connection closure during a blocking read
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Basic test for connection closure during a blocking read
 */
static int test_conn_close(void);

/*!
 * @brief Test for ::conn_set_timeout on a blocking read
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test for ::conn_set_timeout on a blocking read
 */
static int test_conn_timeout(void);

static void *conn_recv_func(void *ctx)
{
	struct thread_handle *th = ctx;
	struct conn_recv_data *data = th->func_ctx;
	uint8_t buff[1];

	data->ret = conn_recv_any(&data->conn, buff, sizeof(buff), NULL, NULL);

	return NULL;
}

/*!
 * @brief Main entry point for connection tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

int main(void)
{
	int ret = 0;

	ret |= test_conn_close();
	ret |= test_conn_timeout();

	return ret;
}

static int test_conn_close(void)
{
	int ret;
	struct conn_recv_data data;

	memset(&data, 0x0, sizeof(struct conn_recv_data));

	data.conn.type = CONN_TYPE_UDP;
	ret = conn_init(&data.conn);
	if (ret < 0)
		goto test_conn_blocking_exit;

	ret = conn_listen(&data.conn);
	if (ret < 0)
		goto test_conn_blocking_exit;

	data.thread.func_ctx = &data;
	data.thread.func_ptr = conn_recv_func;
	ret = thread_init(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	data.ret = -EINPROGRESS;

	ret = thread_start(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	sleep(1);

	conn_close(&data.conn);

	ret = thread_join(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	switch (data.ret)
	{
	case -EBADF:
	case -EINTR:
	case -EPIPE:
		ret = 0;
		break;
	default:
		ret = data.ret;
		fprintf(stderr,
			"Error: Invalid return from conn_recv on close (%d): %s\n",
			-ret, strerror(-ret));
		break;
	}

test_conn_blocking_exit:
	thread_free(&data.thread);
	conn_free(&data.conn);

	return ret;
}

static int test_conn_timeout(void)
{
	int ret;
	struct conn_recv_data data;

	memset(&data, 0x0, sizeof(struct conn_recv_data));

	data.conn.type = CONN_TYPE_UDP;
	ret = conn_init(&data.conn);
	if (ret < 0)
		goto test_conn_blocking_exit;

	ret = conn_listen(&data.conn);
	if (ret < 0)
		goto test_conn_blocking_exit;

	ret = conn_set_timeout(&data.conn, 500);
	if (ret < 0)
		goto test_conn_blocking_exit;

	data.thread.func_ctx = &data;
	data.thread.func_ptr = conn_recv_func;
	ret = thread_init(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	data.ret = -EINPROGRESS;

	ret = thread_start(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	sleep(1);

	conn_close(&data.conn);

	ret = thread_join(&data.thread);
	if (ret < 0)
		goto test_conn_blocking_exit;

	switch (data.ret)
	{
	case -EAGAIN:
	case -ETIMEDOUT:
		ret = 0;
		break;
	default:
		ret = data.ret;
		fprintf(stderr,
			"Error: Invalid return from conn_recv on close (%d): %s\n",
			-ret, strerror(-ret));
		break;
	}

test_conn_blocking_exit:
	thread_free(&data.thread);
	conn_free(&data.conn);

	return ret;
}
