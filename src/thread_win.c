/*!
 * @file thread_win.c
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
 * @brief Threading implementation for Windows
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "mutex.h"
#include "thread.h"

/*!
 * @brief Private data for an instance of a Windows thread
 */
struct thread_priv {
	/*! Handle to the thread */
	HANDLE			thread;

	/*! Mutex for protecting the thread handle */
	struct mutex_handle	mutex;
};

/*!
 * @brief Wrapper for Windows worker function calling syntax
 *
 * @param ctx Worker function context
 *
 * @returns Exit code of the thread
 *
 * Since the Windows API for threading uses a different worker function
 * signature than the one used by these threading routines, this wrapper
 * function is used to call the worker function specified in the thread
 * handle.
 */
static DWORD WINAPI windows_thread_wrapper(LPVOID ctx)
{
	struct thread_handle *pt = ctx;

	pt->func_ptr(pt);

	return 0;
}

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

	mutex_lock(&priv->mutex);

	WaitForSingleObject(priv->thread, INFINITE);

	CloseHandle(priv->thread);

	priv->thread = NULL;

	mutex_unlock(&priv->mutex);

	return -ENOSYS;
}

int thread_start(struct thread_handle *pt)
{
	struct thread_priv *priv = pt->priv;

	mutex_lock(&priv->mutex);

	priv->thread = CreateThread(NULL, 0, windows_thread_wrapper, pt, 0,
				    NULL);

	mutex_unlock(&priv->mutex);

	return priv->thread == NULL ? -ECHILD : 0;
}
