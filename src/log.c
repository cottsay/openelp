/*!
 * @file log.c
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

#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct log_priv
{
};

int log_init(struct log_handle *log)
{
	log->priv = malloc(sizeof(struct log_priv));
	if (log->priv == NULL)
	{
		return -ENOMEM;
	}

	log->medium = LOG_MEDIUM_STDOUT;
	log->level = LOG_LEVEL_INFO;

	return 0;
}

void log_free(struct log_handle *log)
{
	if (log->priv != NULL)
	{
		free(log->priv);
		log->priv = NULL;
	}
}

void log_printf(struct log_handle *log, enum LOG_LEVEL lvl, const char *fmt, ...)
{
	va_list args;

	if (lvl > log->level)
	{
		return;
	}

	va_start(args, fmt);
	log_vprintf(log, lvl, fmt, args);
	va_end(args);
}


void log_vprintf(struct log_handle *log, enum LOG_LEVEL lvl, const char *fmt, va_list args)
{
	if (lvl > log->level)
	{
		return;
	}

	switch (log->medium)
	{
	case LOG_MEDIUM_STDOUT:
		if (lvl >= LOG_LEVEL_ERROR)
		{
			vfprintf(stderr, fmt, args);
		}
		else
		{
			vprintf(fmt, args);
		}

		break;
	case LOG_MEDIUM_FILE:
		break;
	case LOG_MEDIUM_SYSLOG:
		break;
	}
}
