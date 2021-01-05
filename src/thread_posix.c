/*!
 * @file thread_posix.c
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
 * @brief Threading implementation for POSIX machines
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mutex.h"
#include "thread.h"

/*!
 * @brief Private data for an instance of a POSIX thread
 */
struct thread_priv {
	/*! POSIX thread */
	pthread_t		thread;

	/*! Mutex for protecting the thread handle */
	struct mutex_handle	mutex;

	/*! Boolean value indicating if the mutex has been initialized */
	uint8_t			dirty;
};

void thread_free(struct thread_handle *pt)
{
	struct thread_priv *priv = pt->priv;

	if (pt->priv != NULL) {
		thread_join(pt);

		mutex_free(&priv->mutex);

		free(pt->priv);
		pt->priv = NULL;
	}
}

int thread_init(struct thread_handle *pt)
{
	struct thread_priv *priv = pt->priv;
	int ret;

	if (priv == NULL) {
		priv = calloc(1, sizeof(*priv));
		if (priv == NULL)
			return -ENOMEM;

		pt->priv = priv;
	}

	ret = mutex_init(&priv->mutex);
	if (ret < 0)
		goto thread_init_exit;

	return 0;

thread_init_exit:
	free(pt->priv);
	pt->priv = NULL;

	return ret;
}

int thread_join(struct thread_handle *pt)
{
	struct thread_priv *priv = pt->priv;
	int ret;

	mutex_lock(&priv->mutex);

	if (!priv->dirty) {
		ret = 0;
	} else {
		ret = pthread_join(priv->thread, NULL);

		priv->dirty = 0;

		if (ret == 0)
			memset(&priv->thread, 0x0, sizeof(priv->thread));
	}

	mutex_unlock(&priv->mutex);

	return ret > 0 ? -ret : ret;
}

int thread_start(struct thread_handle *pt)
{
	struct thread_priv *priv = pt->priv;
	pthread_attr_t attr;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return ret > 0 ? -ret : ret;

	if (pt->stack_size > 0) {
		ret = pthread_attr_setstacksize(&attr, pt->stack_size);
		if (ret != 0)
			return ret > 0 ? -ret : ret;
	}

	thread_join(pt);

	mutex_lock(&priv->mutex);

	ret = pthread_create(&priv->thread, &attr, pt->func_ptr, pt);

	priv->dirty = !(ret);

	mutex_unlock(&priv->mutex);

	return ret > 0 ? -ret : ret;
}
