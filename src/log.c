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
#include "log_syslog.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct log_priv
{
	struct log_priv_medium_file
	{
		FILE *fp;
	} medium_file;
};

static const int SYSLOG_LEVEL[] =
{
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG,
};

void log_close(struct log_handle *log)
{
	struct log_priv *priv = (struct log_priv *)log->priv;

	switch (log->medium)
	{
	case LOG_MEDIUM_STDOUT:
		break;
	case LOG_MEDIUM_FILE:
		if (priv->medium_file.fp != NULL)
		{
			fclose(priv->medium_file.fp);
			priv->medium_file.fp = NULL;
		}

		break;
	case LOG_MEDIUM_SYSLOG:
		closelog();
		break;
	}
}

void log_free(struct log_handle *log)
{
	log_close(log);

	if (log->priv != NULL)
	{
		free(log->priv);
		log->priv = NULL;
	}
}

void log_ident(struct log_handle *log)
{
	log_printf(log, LOG_LEVEL_INFO, "OpenELP %d.%d.%d\n", OPENELP_MAJOR_VERSION, OPENELP_MINOR_VERSION, OPENELP_PATCH_VERSION);
}

int log_init(struct log_handle *log)
{
	struct log_priv *priv;

	log->priv = malloc(sizeof(struct log_priv));
	if (log->priv == NULL)
	{
		return -ENOMEM;
	}

	priv = (struct log_priv *)log->priv;

	log->medium = LOG_MEDIUM_STDOUT;
	log->level = LOG_LEVEL_INFO;

	priv->medium_file.fp = NULL;

	return 0;
}

const char * log_medium_to_str(enum LOG_MEDIUM medium)
{
	switch (medium)
	{
	case LOG_MEDIUM_STDOUT:
		return "Console";
	case LOG_MEDIUM_FILE:
		return "Log File";
	case LOG_MEDIUM_SYSLOG:
		return "Syslog";
	default:
		return "Unknown Medium";
	}
}

int log_open(struct log_handle *log, const char *target)
{
	struct log_priv *priv = (struct log_priv *)log->priv;
	int ret = 0;

	switch (log->medium)
	{
	case LOG_MEDIUM_STDOUT:
		break;
	case LOG_MEDIUM_FILE:
		{
			FILE *fp = fopen(target, "a");
			if (fp == NULL)
			{
				ret = -errno;
			}
			else
			{
				priv->medium_file.fp = fp;
			}
		}
		break;
	case LOG_MEDIUM_SYSLOG:
		openlog("openelp", LOG_CONS | LOG_NDELAY, LOG_DAEMON);
		break;
	}

	return ret;
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

int log_select_medium(struct log_handle *log, const enum LOG_MEDIUM medium, const char *target)
{
	struct log_priv *priv = (struct log_priv *)log->priv;
	int ret = 0;

	// Open the new medium
	switch (medium)
	{
	case LOG_MEDIUM_STDOUT:
		log->medium = LOG_MEDIUM_STDOUT;
		switch (log->medium)
		{
		case LOG_MEDIUM_FILE:
			fclose(priv->medium_file.fp);
			priv->medium_file.fp = NULL;
			break;
		case LOG_MEDIUM_SYSLOG:
			closelog();
			break;
		default:
			break;
		}
		break;
	case LOG_MEDIUM_FILE:
		{
			FILE *fp = fopen(target, "a");
			if (fp == NULL)
			{
				ret = -errno;
			}
			else
			{
				switch (log->medium)
				{
				case LOG_MEDIUM_FILE:
					{
						FILE *old_fp = priv->medium_file.fp;
						priv->medium_file.fp = fp;
						fclose(old_fp);
					}
					break;
				case LOG_MEDIUM_SYSLOG:
					priv->medium_file.fp = fp;
					log->medium = LOG_MEDIUM_FILE;
					closelog();
					break;
				default:
					priv->medium_file.fp = fp;
					log->medium = LOG_MEDIUM_FILE;
					break;
				}
			}
		}
		break;
	case LOG_MEDIUM_SYSLOG:
		switch (log->medium)
		{
		case LOG_MEDIUM_FILE:
			openlog("openelp", LOG_CONS | LOG_NDELAY, LOG_DAEMON);
			log->medium = LOG_MEDIUM_SYSLOG;
			fclose(priv->medium_file.fp);
			priv->medium_file.fp = NULL;
			break;
		default:
			openlog("openelp", LOG_CONS | LOG_NDELAY, LOG_DAEMON);
			log->medium = LOG_MEDIUM_SYSLOG;
			break;
		}
		break;
	}

	return ret;
}

void log_vprintf(struct log_handle *log, enum LOG_LEVEL lvl, const char *fmt, va_list args)
{
	struct log_priv *priv = (struct log_priv *)log->priv;

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
			fflush(stderr);
		}
		else
		{
			vprintf(fmt, args);
			fflush(stdout);
		}

		break;
	case LOG_MEDIUM_FILE:
		{
			time_t epoch;
			struct tm cal_time;
			char tstamp[16];

			time(&epoch);
			localtime_r(&epoch, &cal_time);
			strftime(tstamp, 16, "%b %d %H:%M:%S", &cal_time);
			fprintf(priv->medium_file.fp, "%s : ", tstamp);

			vfprintf(priv->medium_file.fp, fmt, args);
			fflush(priv->medium_file.fp);
		}
		break;
	case LOG_MEDIUM_SYSLOG:
		vsyslog(SYSLOG_LEVEL[lvl], fmt, args);
		break;
	}
}
