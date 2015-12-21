/*!
 * @file thread_win.c
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

#include "thread.h"
#include "mutex.h"

#include <windows.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct thread_priv
{
	HANDLE thread;
	DWORD thread_id;
	struct mutex_handle mutex;
};

DWORD WINAPI thread_wrapper(LPVOID ctx)
{
	struct thread_handle *pt = (struct thread_handle *)ctx;

	pt->func_ptr(pt);

	return 0;
}

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

	mutex_lock(&priv->mutex);

	priv->thread = CreateThread(NULL, 0, thread_wrapper, pt, 0, &priv->thread_id);

	mutex_unlock(&priv->mutex);

	return priv->thread == NULL ? -ECHILD : 0;
}

int thread_join(struct thread_handle *pt)
{
	struct thread_priv *priv = (struct thread_priv *)pt->priv;

	mutex_lock(&priv->mutex);

	WaitForSingleObject(priv->thread, INFINITE);

	CloseHandle(priv->thread);

	priv->thread = NULL;
	priv->thread_id = 0;

	mutex_unlock(&priv->mutex);

	return -ENOSYS;
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
