/*!
 * @file mutex_win.c
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
 * @brief Mutex implementation for Windows
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <synchapi.h>

#include "mutex.h"

/*!
 * @brief Private data for an instance of a Windows thread
 */
struct mutex_priv {
	/*! Represents a slim reader/writer lock on Windows */
	SRWLOCK lock;
};

/*!
 * @brief Private data for an instance of a Windows condition variable
 */
struct condvar_priv {
	/*! Represents a condition variable on Windows */
	CONDITION_VARIABLE cond;
};

int mutex_init(struct mutex_handle *mutex)
{
	struct mutex_priv *priv;

	if (mutex->priv == NULL)
		mutex->priv = malloc(sizeof(struct mutex_priv));

	if (mutex->priv == NULL)
		return -ENOMEM;

	memset(mutex->priv, 0x0, sizeof(struct mutex_priv));

	priv = mutex->priv;

	InitializeSRWLock(&priv->lock);

	return 0;
}

int mutex_lock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = mutex->priv;

	AcquireSRWLockExclusive(&priv->lock);

	return 0;
}

int mutex_lock_shared(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = mutex->priv;

	AcquireSRWLockShared(&priv->lock);

	return 0;
}

int mutex_unlock(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = mutex->priv;

	ReleaseSRWLockExclusive(&priv->lock);

	return 0;
}

int mutex_unlock_shared(struct mutex_handle *mutex)
{
	struct mutex_priv *priv = mutex->priv;

	ReleaseSRWLockShared(&priv->lock);

	return 0;
}

void mutex_free(struct mutex_handle *mutex)
{
	if (mutex->priv != NULL) {
		free(mutex->priv);

		mutex->priv = NULL;
	}
}

int condvar_init(struct condvar_handle *condvar)
{
	struct condvar_priv *priv;

	if (condvar->priv == NULL)
		condvar->priv = malloc(sizeof(struct condvar_priv));

	if (condvar->priv == NULL)
		return -ENOMEM;

	memset(condvar->priv, 0x0, sizeof(struct condvar_priv));

	priv = condvar->priv;

	InitializeConditionVariable(&priv->cond);

	return 0;
}

int condvar_wait(struct condvar_handle *condvar, struct mutex_handle *mutex)
{
	struct condvar_priv *priv = condvar->priv;
	struct mutex_priv *mpriv = mutex->priv;

	if (!SleepConditionVariableSRW(&priv->cond, &mpriv->lock, INFINITE, 0))
		return GetLastError();

	return 0;
}

int condvar_wait_time(struct condvar_handle *condvar,
		      struct mutex_handle *mutex, uint32_t msec)
{
	struct condvar_priv *priv = condvar->priv;
	struct mutex_priv *mpriv = mutex->priv;
	int ret = 0;

	if (!SleepConditionVariableSRW(&priv->cond, &mpriv->lock, msec, 0)) {
		ret = GetLastError();
		if (ret == ERROR_TIMEOUT)
			ret = 1;
	}

	return ret;
}

int condvar_wake_one(struct condvar_handle *condvar)
{
	struct condvar_priv *priv = condvar->priv;

	WakeConditionVariable(&priv->cond);

	return 0;
}

int condvar_wake_all(struct condvar_handle *condvar)
{
	struct condvar_priv *priv = condvar->priv;

	WakeAllConditionVariable(&priv->cond);

	return 0;
}

void condvar_free(struct condvar_handle *condvar)
{
	if (condvar->priv != NULL) {
		free(condvar->priv);

		condvar->priv = NULL;
	}
}
