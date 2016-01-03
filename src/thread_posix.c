/*!
 * @file thread_posix.c
 *
 * @section LICENSE
 *
 * Copyright &copy; 2016, Scott K Logan
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

#include "mutex.h"
#include "thread.h"

#include <pthread.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct thread_priv
{
	uint8_t dirty;
	pthread_t thread;
	struct mutex_handle mutex;
};

int thread_init(struct thread_handle *pt)
{
	struct thread_priv *priv;
	int ret;

	if (pt->priv == NULL)
	{
		pt->priv = malloc(sizeof(struct thread_priv));
	}
	if (pt->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(pt->priv, 0x0, sizeof(struct thread_priv));

	priv = (struct thread_priv *)pt->priv;

	ret = mutex_init(&priv->mutex);
	if (ret < 0)
	{
		goto thread_init_exit;
	}

	return 0;

thread_init_exit:
	free(pt->priv);
	pt->priv = NULL;

	return ret;
}

int thread_start(struct thread_handle *pt)
{
	struct thread_priv *priv = (struct thread_priv *)pt->priv;
	int ret;

	thread_join(pt);

	mutex_lock(&priv->mutex);

	ret = pthread_create(&priv->thread, NULL, pt->func_ptr, pt);

	priv->dirty = !(ret);

	mutex_unlock(&priv->mutex);

	return ret > 0 ? -ret : ret;
}

int thread_join(struct thread_handle *pt)
{
	struct thread_priv *priv = (struct thread_priv *)pt->priv;
	int ret;

	mutex_lock(&priv->mutex);

	if (!priv->dirty)
	{
		ret = 0;
	}
	else
	{
		ret = pthread_join(priv->thread, NULL);

		priv->dirty = 0;

		if (ret == 0)
		{
			memset(&priv->thread, 0x0, sizeof(pthread_t));
		}
	}

	mutex_unlock(&priv->mutex);

	return ret > 0 ? -ret : ret;
}

void thread_free(struct thread_handle *pt)
{
	struct thread_priv *priv = (struct thread_priv *)pt->priv;

	if (pt->priv != NULL)
	{
		thread_join(pt);

		mutex_free(&priv->mutex);

		free(pt->priv);
		pt->priv = NULL;
	}
}
