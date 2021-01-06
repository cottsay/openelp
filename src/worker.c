/*!
 * @file worker.c
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
 * @brief Worker implementation
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "worker.h"

/*!
 * @brief The current state of a worker instance
 */
enum worker_state {
	/*! The worker is not running */
	WORKER_STOPPED = 0x0,

	/*! The worker is busy, but will shut down */
	WORKER_STOPPING,

	/*! The worker will process work and then shut down */
	WORKER_STOPPING_AFTER_WORK,

	/*! The worker is currently processing work */
	WORKER_BUSY,

	/*! The worker will process work soon */
	WORKER_SIGNALED,

	/*! The worker is waiting for new work */
	WORKER_IDLE,

	/*! The worker is starting up */
	WORKER_STARTING
};

/*!
 * @brief Private data for an instance of a worker
 */
struct worker_priv {
	/*! Used to signal new work available */
	struct condvar_handle condvar;

	/*! Used to signal that the worker is idle */
	struct condvar_handle condvar_idle;

	/*! Mutex for protecting worker_priv::state */
	struct mutex_handle mutex;

	/*! Execution thread */
	struct thread_handle thread;

	/*! The current state of the worker */
	enum worker_state state;
};

/*!
 * @brief Thread function which services a worker's work signals
 *
 * @param[in,out] ctx The thread context
 *
 * @returns Always returns NULL
 */
static void *worker_func(void *ctx);

static void *worker_func(void *ctx)
{
	struct thread_handle *th = ctx;
	struct worker_handle *wh = th->func_ctx;
	struct worker_priv *priv = wh->priv;

	mutex_lock(&priv->mutex);

	while (priv->state > WORKER_STOPPING) {
		while (priv->state == WORKER_SIGNALED ||
		       priv->state == WORKER_STOPPING_AFTER_WORK) {
			if (priv->state == WORKER_STOPPING_AFTER_WORK)
				priv->state = WORKER_STOPPING;
			else
				priv->state = WORKER_BUSY;
			mutex_unlock(&priv->mutex);
			wh->func_ptr(wh);
			mutex_lock(&priv->mutex);
		}

		if (priv->state <= WORKER_STOPPING)
			break;

		priv->state = WORKER_IDLE;

		condvar_wake_all(&priv->condvar_idle);

		if (wh->periodic_wake > 0) {
			condvar_wait_time(&priv->condvar, &priv->mutex,
					  wh->periodic_wake);
			if (priv->state == WORKER_IDLE)
				priv->state = WORKER_SIGNALED;
		} else {
			condvar_wait(&priv->condvar, &priv->mutex);
		}
	}

	priv->state = WORKER_STOPPED;

	mutex_unlock(&priv->mutex);

	return NULL;
}

void worker_free(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;

	if (wh->priv != NULL) {
		worker_join(wh);

		thread_free(&priv->thread);
		mutex_free(&priv->mutex);
		condvar_free(&priv->condvar_idle);
		condvar_free(&priv->condvar);

		free(wh->priv);
		wh->priv = NULL;
	}
}

int worker_init(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		wh->priv = priv;
	}

	ret = condvar_init(&priv->condvar);
	if (ret < 0)
		goto worker_init_exit;

	ret = condvar_init(&priv->condvar_idle);
	if (ret < 0)
		goto worker_init_exit;

	ret = mutex_init(&priv->mutex);
	if (ret < 0)
		goto worker_init_exit;

	priv->thread.func_ctx = wh;
	priv->thread.func_ptr = worker_func;
	priv->thread.stack_size = wh->stack_size;
	ret = thread_init(&priv->thread);
	if (ret < 0)
		goto worker_init_exit;

	return 0;

worker_init_exit:
	mutex_free(&priv->mutex);

	condvar_free(&priv->condvar_idle);
	condvar_free(&priv->condvar);

	free(wh->priv);
	wh->priv = NULL;

	return ret;
}

int worker_is_idle(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;
	int ret = 0;

	mutex_lock_shared(&priv->mutex);

	if (priv->state >= WORKER_IDLE)
		ret = 1;

	mutex_unlock_shared(&priv->mutex);

	return ret;
}

int worker_join(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;

	mutex_lock(&priv->mutex);

	if (priv->state > WORKER_STOPPING_AFTER_WORK) {
		if (priv->state == WORKER_IDLE)
			condvar_wake_all(&priv->condvar);

		if (priv->state == WORKER_SIGNALED)
			priv->state = WORKER_STOPPING_AFTER_WORK;
		else
			priv->state = WORKER_STOPPING;
	}

	condvar_wake_all(&priv->condvar_idle);

	mutex_unlock(&priv->mutex);

	return thread_join(&priv->thread);
}

int worker_start(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;
	int ret = 0;

	mutex_lock(&priv->mutex);

	if (priv->state <= WORKER_STOPPED) {
		priv->state = WORKER_STARTING;
		ret = thread_start(&priv->thread);
	}

	mutex_unlock(&priv->mutex);

	return ret;
}

int worker_wait_idle(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;
	int ret = 0;

	mutex_lock(&priv->mutex);

	while (priv->state < WORKER_IDLE) {
		if (priv->state < WORKER_BUSY) {
			ret = -EINVAL;
			break;
		}

		condvar_wait(&priv->condvar_idle, &priv->mutex);
	}

	mutex_unlock(&priv->mutex);

	return ret;
}

int worker_wake(struct worker_handle *wh)
{
	struct worker_priv *priv = wh->priv;
	int ret = -EINVAL;

	mutex_lock(&priv->mutex);

	if (priv->state > WORKER_STOPPING_AFTER_WORK) {
		if (priv->state == WORKER_IDLE)
			condvar_wake_all(&priv->condvar);
		priv->state = WORKER_SIGNALED;
		ret = 0;
	}

	mutex_unlock(&priv->mutex);

	return ret;
}
