/*!
 * @file proxyd.c
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

#include "openelp/openelp.h"

#ifndef _WIN32
#  include <signal.h>
#  include <sys/stat.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 */
const char default_config_path[] = "ELProxy.conf";

/*
 * Definitions
 */
struct proxy_opts
{
	uint8_t foreground;
	const char *config_path;
};

/*
 * Global Variables
 */
struct proxy_handle ph;

#ifndef _WIN32
/*
 * Signal Handling
 */
void graceful_shutdown(int signum, siginfo_t *info, void *ptr)
{
	printf("Caught signal\n");

	proxy_shutdown(&ph);
}
#endif

/*
 * Functions
 */
void print_usage(void)
{
#ifndef _WIN32
	printf("Usage: openelinkproxyd [-F] [--help] [<config path>]\n");
#else
	printf("Usage: openelinkproxyd [--help] [<config path>]\n");
#endif
}

void parse_args(const int argc, const char *argv[], struct proxy_opts *opts)
{
	int i;
	size_t j;
	size_t arg_len;

	for (i = 1; i < argc; i++)
	{
		arg_len = strlen(argv[i]);

		if (arg_len > 1 && argv[i][0] == '-')
		{
			if (argv[i][1] == '-')
			{
				if (strcmp(&argv[i][2], "help") == 0)
				{
					print_usage();
					exit(0);
				}
			}
			else
			{
				for (j = 1; j < arg_len; j++)
				{
					switch (argv[i][j])
					{
#ifndef _WIN32
					case 'F':
						opts->foreground = 1;
						break;
#endif
					default:
						fprintf(stderr, "Invalid flag '%c'\n", argv[i][j]);
						exit(-EINVAL);
						break;
					}
				}

				continue;
			}
		}
		else if (arg_len > 0)
		{
			if (opts->config_path == NULL)
			{
				opts->config_path = argv[i];
				continue;
			}
			else
			{
				fprintf(stderr, "Config path already specified\n");
				exit(-EINVAL);
			}
		}

		fprintf(stderr, "Invalid option '%s'\n", argv[i]);
		exit(-EINVAL);
	}
}

int main(int argc, const char *argv[])
{
	struct proxy_opts opts;
#ifndef _WIN32
	struct sigaction sigact;
#endif
	int ret;

	memset(&opts, 0x0, sizeof(struct proxy_opts));
#ifndef _WIN32
	memset(&sigact, 0x0, sizeof(struct sigaction));
#endif
	memset(&ph, 0x0, sizeof(struct proxy_handle));

	parse_args(argc, argv, &opts);
	if (opts.config_path == NULL)
	{
		opts.config_path = default_config_path;
	}

#ifndef _WIN32
	// Handle SIGINT/SIGTERM
	sigact.sa_sigaction = graceful_shutdown;
	sigact.sa_flags |= SA_SIGINFO;

	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
#endif

	// Initialize proxy
	ret = proxy_init(&ph);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to initialize proxy (%d): %s\n", -ret, strerror(-ret));
		exit(ret);
	}

	// Load the config
	ret = proxy_load_conf(&ph, opts.config_path);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to load config from '%s' (%d): %s\n", opts.config_path, -ret, strerror(-ret));
		exit(ret);
	}

	// Start listening
	ret = proxy_open(&ph);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to open proxy (%d): %s\n", -ret, strerror(-ret));
		goto proxyd_exit;
	}

#ifndef _WIN32
	// Daemonize
	if (!opts.foreground)
	{
		pid_t pid;
		pid_t sid;

		pid = fork();

		if (pid < 0)
		{
			fprintf(stderr, "Error forking daemon process\n");
			return pid;
		}

		if (pid > 0)
		{
			exit(0);
		}

		umask(0);

		sid = setsid();
		if (sid < 0)
		{
			fprintf(stderr, "Process error (%d): %s\n", errno, strerror(errno));
			exit(-errno);
		}

		if (chdir("/") < 0)
		{
			fprintf(stderr, "Failed to change dir (%d): %s\n", errno, strerror(errno));
			exit(-errno);
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}
#endif

	// Main dispatch loop
	while (ph.status > PROXY_STATUS_DOWN)
	{
		ret = proxy_process(&ph);
		if (ret < 0)
		{
			fprintf(stderr, "Message processing failure (%d): %s\n", -ret, strerror(-ret));
			break;
		}
	}

proxyd_exit:
	proxy_free(&ph);

	return ret;
}
