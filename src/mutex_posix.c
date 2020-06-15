/*!
 * @file mutex_posix.c
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
 * @brief Mutex implementation for POSIX machines
 */

#include "mutex.h"

#include <pthread.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*!
 * @brief Private data for an instance of a POSIX mutex
 */
struct mutex_priv
{
	/// Condition variable used to implement the shared lock
	pthread_cond_t cond;

	/// POSIX mutex used for exclusive locking
	pthread_mutex_t lock;

	/// Number of holders of the shared lock
	unsigned int readers;
};

/*!
 * @brief Private data for an instance of a POSIX condition variable
 */
struct condvar_priv
{
	/// POSIX condition variable
	pthread_cond_t cond;
};

int mutex_init(struct mutex_handle *mutex)
{
	struct mutex_priv *priv;
	int ret;

	if (mutex->priv == NULL)
	{
		mutex->priv = malloc(sizeof(struct mutex_priv));
	}

	if (mutex->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(mutex->priv, 0x0, sizeof(struct mutex_priv));

	priv = (struct mutex_priv *)mutex->priv;

	ret = pthread_mutex_init(&priv->lock, NULL);
	if (ret != 0)
	{
		ret = -ret;

		goto mutex_init_exit;
	}

	ret = pthread_cond_init(&priv->cond, NULL);
	if (ret != 0)
	{
		ret = -ret;

		goto mutex_init_exit_late;
	}

	return 0;

mutex_init_exit_late:
	pthread_mutex_destroy(&priv->lock);

mutex_init_exit:
	free(mutex->priv);
	mutex->priv = NULL;

	return ret;
}

int mutex_lock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;
	int ret;

	ret = pthread_mutex_lock(&priv->lock);
	if (ret != 0)
	{
		return -ret;
	}

	while (priv->readers > 0)
	{
		ret = pthread_cond_wait(&priv->cond, &priv->lock);
		if (ret != 0)
		{
			ret = -ret;
			goto mutex_lock_exit;
		}
	}

	return 0;

mutex_lock_exit:
	pthread_mutex_unlock(&priv->lock);

	return ret;
}

int mutex_lock_shared(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;
	int ret;

	ret = pthread_mutex_lock(&priv->lock);
	if (ret != 0)
	{
		return -ret;
	}

	priv->readers++;

	return -pthread_mutex_unlock(&priv->lock);
}

int mutex_unlock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

	return pthread_mutex_unlock(&priv->lock);
}

int mutex_unlock_shared(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;
	int ret;

	ret = pthread_mutex_lock(&priv->lock);
	if (ret != 0)
	{
		return -ret;
	}

	if (--(priv->readers) <= 0)
	{
		ret = pthread_cond_broadcast(&priv->cond);
		if (ret != 0)
		{
			ret = -ret;
			goto mutex_unlock_shared_exit;
		}
	}

	return -pthread_mutex_unlock(&priv->lock);

mutex_unlock_shared_exit:
	pthread_mutex_unlock(&priv->lock);

	return ret;
}

void mutex_free(struct mutex_handle *mutex)
{
	if (mutex->priv != NULL)
	{
		struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

		pthread_mutex_destroy(&priv->lock);

		pthread_cond_destroy(&priv->cond);

		free(mutex->priv);
		mutex->priv = NULL;
	}
}

int condvar_init(struct condvar_handle *condvar)
{
	struct condvar_priv *priv;
	int ret;

	if (condvar->priv == NULL)
	{
		condvar->priv = malloc(sizeof(struct condvar_priv));
	}

	if (condvar->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(condvar->priv, 0x0, sizeof(struct condvar_priv));

	priv = (struct condvar_priv *)condvar->priv;

	ret = pthread_cond_init(&priv->cond, NULL);
	if (ret != 0)
	{
		ret = -ret;

		goto condvar_init_exit;
	}

	return 0;

condvar_init_exit:
	free(condvar->priv);
	condvar->priv = NULL;

	return ret;
}

int condvar_wait(struct condvar_handle *condvar, struct mutex_handle *mutex)
{
	struct condvar_priv *priv = (struct condvar_priv *)condvar->priv;
	struct mutex_priv *mpriv = (struct mutex_priv *)mutex->priv;

	return -pthread_cond_wait(&priv->cond, &mpriv->lock);
}

int condvar_wait_time(struct condvar_handle *condvar, struct mutex_handle *mutex, uint32_t msec)
{
	struct condvar_priv *priv = (struct condvar_priv *)condvar->priv;
	struct mutex_priv *mpriv = (struct mutex_priv *)mutex->priv;
	struct timespec abstime;
	int ret;

	clock_gettime(CLOCK_REALTIME, &abstime);
	abstime.tv_nsec += (msec % 1000) * 1000000;
	abstime.tv_sec += (msec / 1000) + (abstime.tv_nsec / 1000000000);
	abstime.tv_nsec %= 1000000000;

	ret = pthread_cond_timedwait(&priv->cond, &mpriv->lock, &abstime);
	return ret == ETIMEDOUT ? 1 : -ret;
}

int condvar_wake_one(struct condvar_handle *condvar)
{
	struct condvar_priv *priv = (struct condvar_priv *)condvar->priv;

	return -pthread_cond_signal(&priv->cond);
}

int condvar_wake_all(struct condvar_handle *condvar)
{
	struct condvar_priv *priv = (struct condvar_priv *)condvar->priv;

	return -pthread_cond_broadcast(&priv->cond);
}

void condvar_free(struct condvar_handle *condvar)
{
	if (condvar->priv != NULL)
	{
		struct condvar_priv *priv = (struct condvar_priv *)condvar->priv;

		pthread_cond_destroy(&priv->cond);

		free(condvar->priv);
		condvar->priv = NULL;
	}
}

