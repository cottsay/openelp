/*!
 * @file proxyd.c
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
 * @brief Executable application which starts OpenELP
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <conio.h>
#  include <io.h>
#  include <windows.h>
#else
#  include <signal.h>
#  include <sys/stat.h>
#endif

#include "openelp/openelp.h"

#ifdef _WIN32
#  define access(...) _access(__VA_ARGS__)
#  define R_OK 0x4
#endif

/*! Universal fallback configuration path */
static const char config_path_default[] = "ELProxy.conf";

/*! Stringization macro - stage one */
#define OCH_STR1(x) #x

/*! Stringization macro - stage two */
#define OCH_STR2(x) OCH_STR1(x)

#ifdef OPENELP_CONFIG_HINT
/*! Platform-specific path hint */
static const char config_path_hint[] = OCH_STR2(OPENELP_CONFIG_HINT);
#endif

/*!
 * @brief Configuration options for running the application
 */
struct proxy_opts {
	/*! Null terminated path to the proxy configuration file */
	const char *config_path;

	/*! Path to file where log output should go */
	const char *log_path;

	/*! Boolean value indicating if debug information should be printed */
	uint8_t debug;

	/*! Boolean value indicating if the Windows Event Log should be used */
	uint8_t eventlog;

	/*! Boolean value indicating if OpenELP should not daemonize */
	uint8_t foreground;

	/*! Boolean value indicating if syslog should be used */
	uint8_t syslog;

	/*! Boolean value indicating if messages to stdout should be suppressed */
	uint8_t quiet;
};

/*! Global proxy handle */
static struct proxy_handle ph;

/*! Program termination indicator */
static uint8_t sentinel;

#ifdef _WIN32
/*!
 * @brief Callback which is used to shut down the EchoLink proxy
 *
 * @param[in] ctrl_type Control event code
 *
 * @returns TRUE if the event was handled, FALSE if not
 */
static BOOL WINAPI graceful_shutdown(DWORD ctrl_type);
#else
/*!
 * @brief Callback which is used to shut down the EchoLink proxy
 *
 * @param[in] signum Signal number
 * @param[in] info Extra signal information
 * @param[in] ptr Signal handler context
 */
static void graceful_shutdown(int signum, siginfo_t *info, void *ptr);
#endif

/*!
 * @brief Main entry point for the EchoLink Proxy daemon executable
 *
 * @param[in] argc Number of arguments in argv
 * @param[in] argv Array of null-terminated argument strings
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int main(int argc, const char * const argv[]);

/*!
 * @brief Parse command line arguments into ::proxy_opts values
 *
 * @param[in] argc Number of arguments in argv
 * @param[in] argv Array of null-terminated argument strings
 * @param[in,out] opts Options parsed from the given arguments
 */
static void parse_args(int argc, const char * const argv[],
		       struct proxy_opts *opts);

#ifdef OPENELP_CONFIG_HINT
/*!
 * @brief Determine and return the path hint to the proxy configuration file
 *
 * @returns Static, null-terminated path to the configuration file
 */
static const char *proxy_config_hint(void);
#endif

/*!
 * @brief Print the program usage to STDOUT
 */
static void print_usage(void);

#ifdef _WIN32
static BOOL WINAPI graceful_shutdown(DWORD ctrl_type)
{
	if (ctrl_type == CTRL_C_EVENT) {
		proxy_log(&ph, LOG_LEVEL_INFO, "Caught signal\n");

		sentinel = 1;

		proxy_shutdown(&ph);

		return TRUE;
	}

	return FALSE;
}
#else
/*! @TODO Make this async-signal-safe */
static void graceful_shutdown(int signum, siginfo_t *info, void *ptr)
{
	proxy_log(&ph, LOG_LEVEL_INFO, "Caught signal\n");
	(void)info;
	(void)ptr;

	if (signum == SIGTERM || signum == SIGINT)
		sentinel = 1;

	if (signum == SIGTERM || signum == SIGINT || signum == SIGUSR1)
		proxy_shutdown(&ph);
}
#endif

int main(int argc, const char * const argv[])
{
	struct proxy_opts opts;

#ifndef _WIN32
	struct sigaction sigact;
#endif
	int ret;

	memset(&opts, 0x0, sizeof(opts));
#ifndef _WIN32
	memset(&sigact, 0x0, sizeof(sigact));
#endif
	memset(&ph, 0x0, sizeof(ph));

	parse_args(argc, argv, &opts);
	if (opts.config_path == NULL) {
#ifdef OPENELP_CONFIG_HINT
		opts.config_path = proxy_config_hint();
#else
		opts.config_path = config_path_default;
#endif
	}

#ifndef _WIN32
	/* Handle SIGINT/SIGTERM */
	sigact.sa_sigaction = graceful_shutdown;
	sigact.sa_flags |= SA_SIGINFO;

	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
#else
	if (!SetConsoleCtrlHandler(graceful_shutdown, TRUE))
		fprintf(stderr, "Failed to set signal handler (%d)\n",
			GetLastError());

#endif

	/* Initialize proxy */
	ret = proxy_init(&ph);
	if (ret < 0) {
		fprintf(stderr, "Failed to initialize proxy (%d): %s\n", -ret,
			strerror(-ret));
		goto proxyd_exit;
	}

	/* Set the initial logging level */
	proxy_log_level(&ph, opts.quiet ? LOG_LEVEL_WARN : opts.debug ?
			LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);

	/* Open the log */
	ret = proxy_log_select_medium(&ph, LOG_MEDIUM_STDOUT, NULL);
	if (ret < 0) {
		fprintf(stderr, "Failed to switch log to STDOUT (%d): %s\n",
			-ret, strerror(-ret));
		goto proxyd_exit;
	}

	/* Load the config */
	ret = proxy_load_conf(&ph, opts.config_path);
	if (ret < 0) {
		proxy_log(&ph, LOG_LEVEL_FATAL,
			  "Failed to load config from '%s' (%d): %s\n",
			  opts.config_path, -ret, strerror(-ret));
		goto proxyd_exit;
	}

	/* Start listening */
	ret = proxy_open(&ph);
	if (ret < 0) {
		proxy_log(&ph, LOG_LEVEL_FATAL,
			  "Failed to open proxy (%d): %s\n",
			  -ret, strerror(-ret));
		goto proxyd_exit;
	}

#ifndef _WIN32
	/* Daemonize */
	if (!opts.foreground) {
		pid_t pid;
		pid_t sid;

		pid = fork();

		if (pid < 0) {
			proxy_log(&ph, LOG_LEVEL_FATAL,
				  "Error forking daemon process\n");
			ret = pid;
			goto proxyd_exit;
		}

		if (pid > 0)
			exit(0);

		/*! @TODO Double-fork */

		if (opts.log_path) {
			ret = proxy_log_select_medium(&ph, LOG_MEDIUM_FILE,
						      opts.log_path);
			if (ret != 0)
				proxy_log(&ph, LOG_LEVEL_ERROR,
					  "Failed to open log file (%d): %s\n",
					  -ret, strerror(-ret));
			proxy_log_level(&ph, opts.debug ? LOG_LEVEL_DEBUG :
					LOG_LEVEL_INFO);
		} else if (opts.syslog) {
			ret = proxy_log_select_medium(&ph, LOG_MEDIUM_SYSLOG,
						      opts.log_path);
			if (ret != 0)
				proxy_log(&ph, LOG_LEVEL_ERROR,
					  "Failed to activate syslog (%d): %s\n",
					  -ret, strerror(-ret));
			proxy_log_level(&ph, opts.debug ? LOG_LEVEL_DEBUG :
					LOG_LEVEL_INFO);
		}

		umask(0);

		sid = setsid();
		if (sid < 0) {
			proxy_log(&ph, LOG_LEVEL_FATAL,
				  "Process error (%d): %s\n",
				  errno, strerror(errno));
			ret = errno;
			goto proxyd_exit;
		}

		if (chdir("/") < 0) {
			proxy_log(&ph, LOG_LEVEL_FATAL,
				  "Failed to change dir (%d): %s\n",
				  errno, strerror(errno));
			ret = errno;
			goto proxyd_exit;
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	} else
#endif
	{
		/* Switch log, if necessary */
		if (opts.log_path) {
			proxy_log(&ph, LOG_LEVEL_INFO,
				  "Switching log to file \"%s\"\n",
				  opts.log_path);

			ret = proxy_log_select_medium(&ph, LOG_MEDIUM_FILE,
						      opts.log_path);
			if (ret != 0)
				proxy_log(&ph, LOG_LEVEL_ERROR,
					  "Failed to open log file (%d): %s\n",
					  -ret, strerror(-ret));
		} else if (opts.syslog) {
			proxy_log(&ph, LOG_LEVEL_INFO,
				  "Switching log to syslog\n");

			ret = proxy_log_select_medium(&ph, LOG_MEDIUM_SYSLOG,
						      opts.log_path);
			if (ret != 0)
				proxy_log(&ph, LOG_LEVEL_ERROR,
					  "Failed to activate syslog (%d): %s\n",
					  -ret, strerror(-ret));
		} else if (opts.eventlog) {
			proxy_log(&ph, LOG_LEVEL_INFO,
				  "Switching log to eventlog\n");

			ret = proxy_log_select_medium(&ph, LOG_MEDIUM_EVENTLOG,
						      opts.log_path);
			if (ret != 0)
				proxy_log(&ph, LOG_LEVEL_ERROR,
					  "Failed to activate eventlog (%d): %s\n",
					  -ret, strerror(-ret));
		}
	}

	ret = proxy_start(&ph);
	if (ret < 0) {
		proxy_log(&ph, LOG_LEVEL_FATAL,
			  "Failed to start proxy (%d): %s\n",
			  -ret, strerror(-ret));
		goto proxyd_exit;
	}

	proxy_log(&ph, LOG_LEVEL_INFO, "Ready.\n");

	/* Main dispatch loop */
	while (ret == 0 && sentinel == 0) {
		proxy_log(&ph, LOG_LEVEL_DEBUG,
			  "Starting a processing run...\n");
		ret = proxy_process(&ph);
		if (ret < 0) {
			switch (ret) {
			case -EINTR:
				ret = 0;

				/*! @TODO Something better than a busy loop */
				while (!sentinel) {
#ifdef _WIN32
					Sleep(50);
#else
					usleep(50000);
#endif
				}

				break;
			default:
				proxy_log(&ph, LOG_LEVEL_FATAL,
					  "Message processing failure (%d): %s\n",
					  -ret, strerror(-ret));
				break;
			}
		}
	}

	proxy_log(&ph, LOG_LEVEL_INFO, "Shutting down...\n");

proxyd_exit:
	proxy_free(&ph);

#ifdef _WIN32
	if (ret != 0) {
		HWND console_window = GetConsoleWindow();

		if (console_window != NULL) {
			DWORD process_id;

			GetWindowThreadProcessId(console_window, &process_id);
			if (GetCurrentProcessId() == process_id) {
				printf("Press any key to exit . . . ");
				_getch();
			}
		}
	}
#endif

	return ret;
}

static void parse_args(int argc, const char * const argv[],
		       struct proxy_opts *opts)
{
	int i;
	size_t j;
	size_t arg_len;

	for (i = 1; i < argc; i++) {
		arg_len = strlen(argv[i]);

		if (arg_len > 1 && argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				if (strcmp(&argv[i][2], "debug") == 0) {
					opts->debug = 1;
					continue;
				} else if (strcmp(&argv[i][2], "help") == 0) {
					print_usage();
					exit(0);
				} else if (strcmp(&argv[i][2], "quiet") == 0) {
					opts->quiet = 1;
					continue;
				} else if (strcmp(&argv[i][2], "version") ==
					   0) {
					printf(OCH_STR2(OPENELP_VERSION) "\n");
					exit(0);
				}
			} else {
				for (j = 1; j < arg_len; j++) {
					switch (argv[i][j]) {
					case 'd':
						opts->debug = 1;
						break;
#ifdef HAVE_EVENTLOG
					case 'E':
						if (opts->log_path ||
						    opts->syslog) {
							fprintf(stderr,
								"ERROR: Only one logging mechanism is allowed\n");
							exit(-EINVAL);
						}
						opts->eventlog = 1;
						break;
#endif
					case 'F':
						opts->foreground = 1;
						break;
					case 'h':
						print_usage();
						exit(0);
					case 'L':
						if (opts->eventlog ||
						    opts->syslog) {
							fprintf(stderr,
								"ERROR: Only one logging mechanism is allowed\n");
							exit(-EINVAL);
						} else if (arg_len > j + 1) {
							opts->log_path =
								&argv[i][j + 1];
							j = arg_len;
						} else if (i + 1 < argc) {
							i++;
							opts->log_path =
								argv[i];
						} else {
							fprintf(stderr,
								"ERROR: Invalid log file path\n");
							exit(-EINVAL);
						}

						break;
					case 'q':
						opts->quiet = 1;
						break;
#ifdef HAVE_SYSLOG
					case 'S':
						if (opts->eventlog ||
						    opts->log_path) {
							fprintf(stderr,
								"ERROR: Only one logging mechanism is allowed\n");
							exit(-EINVAL);
						}

						opts->syslog = 1;
						break;
#endif
					case 'V':
						printf(OCH_STR2(
							       OPENELP_VERSION)
						       "\n");
						exit(0);
					default:
						fprintf(stderr,
							"ERROR: Invalid flag '%c'\n",
							argv[i][j]);
						exit(-EINVAL);
					}
				}

				continue;
			}
		} else if (arg_len > 0) {
			if (opts->config_path == NULL) {
				opts->config_path = argv[i];
				continue;
			} else {
				fprintf(stderr,
					"ERROR: Config path already specified\n");
				exit(-EINVAL);
			}
		}

		fprintf(stderr, "ERROR: Invalid option '%s'\n", argv[i]);
		exit(-EINVAL);
	}
}

#ifdef OPENELP_CONFIG_HINT
static const char *proxy_config_hint(void)
{
	if (access(config_path_default, R_OK) != 0) {
#  ifdef _WIN32
		/* Check if path hint is relative */
		if (config_path_hint[0] != '\\' &&
		    config_path_hint[0] != '/' &&
		    config_path_hint[1] != ':' &&
		    config_path_hint[2] != ':') {
			static char exe_path[MAX_PATH];
			int exe_path_ret;

			exe_path_ret = GetModuleFileName(NULL, exe_path,
							 MAX_PATH);
			if (exe_path > 0 && exe_path_ret +
			    strlen(config_path_hint) + 1 < MAX_PATH) {
				char *tmp;

				while (exe_path_ret > 0 &&
				       exe_path[exe_path_ret - 1] != '\\')
					exe_path_ret--;
				tmp = &exe_path[exe_path_ret];
				strcpy(tmp, config_path_hint);

				for (; *tmp != '\0'; tmp++)
					if (*tmp == '/')
						*tmp = '\\';

				return exe_path;
			} else {
				return config_path_default;
			}
		} else
#  endif
		{
			return config_path_hint;
		}
	} else {
		return config_path_default;
	}
}
#endif

static void print_usage(void)
{
	printf("OpenELP - Open EchoLink Proxy " OCH_STR2(OPENELP_VERSION) "\n\n"
	       "Usage: openelpd [OPTION...] [CONFIG FILE]\n\n"
	       "Options:\n"
	       "  -d, --debug    Enable debugging output\n"
#ifdef HAVE_EVENTLOG
	       "  -E             Use Event Log for logging\n"
#endif
#ifndef _WIN32
	       "  -F             Stay in foreground (don't daemonize)\n"
#endif
	       "  -h, --help     Display this help\n"
	       "  -L <log path>  Log output the given log file\n"
	       "  -q, --quiet    Suppress messages to stdout\n"
#ifdef HAVE_SYSLOG
	       "  -S             Use syslog for logging\n"
#endif
	       "  -V, --version  Display version and exit\n"
#ifndef _WIN32
	       "\n"
#endif
	       );
}
