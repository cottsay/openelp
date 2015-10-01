/*!
 * @file mutex.c
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

#include "mutex.h"

#include <pthread.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct mutex_priv
{
	pthread_rwlock_t lock;
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

	ret = pthread_rwlock_init(&priv->lock, NULL);
	if (ret != 0)
	{
		if (ret > 0)
		{
			ret = -ret;
		}

		goto mutex_init_exit;
	}

	return 0;

mutex_init_exit:
	free(mutex->priv);
	mutex->priv = NULL;

	return ret;
}

int mutex_lock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

	return pthread_rwlock_wrlock(&priv->lock);
}

int mutex_lock_shared(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

	return pthread_rwlock_rdlock(&priv->lock);
}

int mutex_unlock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

	return pthread_rwlock_unlock(&priv->lock);
}

void mutex_free(struct mutex_handle *mutex)
{
	if (mutex->priv != NULL)
	{
		struct mutex_priv *priv = (struct mutex_priv *)mutex->priv;

		pthread_rwlock_destroy(&priv->lock);

		free(mutex->priv);
	}
}

