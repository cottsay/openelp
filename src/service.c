/*!
 * @file service.c
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
 * @brief Windows service routines
 */

#include "openelp/openelp.h"

#include <windows.h>

#include <stdio.h>

/// Program usage message
static const char *usage = "Usage: openelp_service.exe [install|uninstall|-d]\n";

/// Handle to the EchoLink proxy for the service
static struct proxy_handle ph =
{
	.priv = NULL,
};

/// Service termination indicator
static uint8_t sentinel = 0;

/// Name and display name of the service
static char *service_name = "OpenELP";

/// Status handle for the serice
static SERVICE_STATUS_HANDLE service_status_handle = NULL;

/// Default log level
static enum LOG_LEVEL default_log_level = LOG_LEVEL_INFO;

/*!
 * @brief Main entry point for the service executable
 *
 * @param[in] argc Number of arguments in argv
 * @param[in] argv Array of null-terminated argument strings
 *
 * @returns 0 on success, non-zero error code on failure
 */
int main(int argc, char *argv[]);

/*!
 * @brief Control signal handler for the Windows service
 *
 * @param[in] ctrl Control indicator
 */
void WINAPI service_ctrl_handler(DWORD ctrl);

/*!
 * @brief Install the service on the current system
 *
 * @returns 0 on success, Windows error code value on failure
 */
static int service_install();

/*!
 * @brief Main entry point for the Windows service
 *
 * @param[in] argc Number of arguments in argv
 * @param[in] argv Array of null-terminated argument strings
 */
void WINAPI service_main(int argc, char *argv[]);

/*!
 * @brief Report the status of the Windows service
 *
 * @param[in] state Current service lifecycle state
 * @param[in] exit_code Exit code, if appropriate for the lifecycle state
 * @param[in] wait_hint Duration before another report should be expected
 */
static void service_report_status(int state, int exit_code, int wait_hint);

/*!
 * @brief Uninstall the service from the current system
 *
 * @returns 0 on success, Windows error code value on failure
 */
static int service_uninstall();

int main(int argc, char *argv[])
{
	int ret;
	SERVICE_TABLE_ENTRY service_table[] =
	{
		{ service_name, (LPSERVICE_MAIN_FUNCTION)service_main },
		{ NULL, NULL },
	};

	if (argc > 1)
	{
		if (argc > 2)
		{
			printf(usage);
			return -1;
		}

		if (strcmp(argv[1], "install") == 0)
		{
			ret = service_install();
			if (ret != 0)
			{
				fprintf(stderr, "Failed to install service '%s' (%d)\n", service_name, -ret);
				return -2;
			}

			printf("Successfully installed service '%s'\n", service_name);

			return 0;
		}
		else if (strcmp(argv[1], "uninstall") == 0)
		{
			ret = service_uninstall();
			if (ret != 0)
			{
				fprintf(stderr, "Failed to uninstall service '%s' (%d)\n", service_name, -ret);
				return -2;
			}

			printf("Successfully removed service '%s'\n", service_name);

			return 0;
		}
		else if (strcmp(argv[1], "--help") == 0)
		{
			printf(usage);
			return 0;
		}
		else if (strcmp(argv[1], "-d") == 0)
		{
			default_log_level = LOG_LEVEL_DEBUG;
		}
		else
		{
			printf(usage);
			return -1;
		}
	}

	// Start the control dispatcher thread for our service
	return StartServiceCtrlDispatcher(service_table) ? 0 : -1;
}

void WINAPI service_ctrl_handler(DWORD ctrl)
{
	switch (ctrl)
	{
	case SERVICE_CONTROL_STOP:
		proxy_log(&ph, LOG_LEVEL_INFO, "Received shutdown signal.");

		service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

		proxy_shutdown(&ph);

		sentinel = 1;

		break;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}

static int service_install()
{
	char exe_path[MAX_PATH];
	int ret;
	SC_HANDLE scman;
	SC_HANDLE svc;
	SERVICE_DESCRIPTION sdesc =
	{
		.lpDescription = "Open Source EchoLink Proxy",
	};

	ret = GetModuleFileName(NULL, exe_path, MAX_PATH);
	if (ret == 0)
	{
		return GetLastError();
	}

	// Get a handle to the SCM database.
	scman = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scman == NULL)
	{
		return GetLastError();
	}

	// Create the service
	svc = CreateService(
		scman,                     // SCM database
		service_name,              // name of service
		service_name,              // service name to display
		SERVICE_CHANGE_CONFIG,     // desired access
		SERVICE_WIN32_OWN_PROCESS, // service type
		SERVICE_DEMAND_START,      // start type
		SERVICE_ERROR_NORMAL,      // error control type
		exe_path,                  // path to service's binary
		NULL,                      // no load ordering group
		NULL,                      // no tag identifier
		"netman\0",                // dependencies
		NULL,                      // LocalSystem account
		NULL);                     // no password
	if (svc == NULL)
	{
		ret = -(long)GetLastError();

		goto service_uninstall_exit;
	}
	else
	{
		ret = 0;
	}

	ret = ChangeServiceConfig2(svc, SERVICE_CONFIG_DESCRIPTION, &sdesc);
	if (ret != 1)
	{
		ret = -(long)GetLastError();
	}
	else
	{
		ret = 0;
	}

	CloseServiceHandle(svc);

service_uninstall_exit:
	CloseServiceHandle(scman);

	return ret;
}

void WINAPI service_main(int argc, char *argv[])
{
	char config_path[MAX_PATH];
	int ret;

	memset(&ph, 0x0, sizeof(struct proxy_handle));

	service_status_handle = RegisterServiceCtrlHandler(service_name, service_ctrl_handler);
	if (service_status_handle == NULL)
	{
		//proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to register service ctrl handler\n");
		return;
	}

	service_report_status(SERVICE_START_PENDING, NO_ERROR, 1000);

	// Initialize proxy
	ret = proxy_init(&ph);
	if (ret < 0)
	{
		//proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to initialize proxy (%d): %s\n", -ret, strerror(-ret));
		goto service_main_exit_early;
	}

	// Set the logging level
	proxy_log_level(&ph, default_log_level);

	// Open the log early
	ret = proxy_log_select_medium(&ph, LOG_MEDIUM_EVENTLOG, NULL);
	if (ret < 0)
	{
		//proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to open eventlog (%d)\n", -ret);
		goto service_main_exit;
	}

	// Find the config
	ret = GetModuleFileName(NULL, config_path, MAX_PATH);
	if (ret == 0)
	{
		ret = -(long)GetLastError();
		proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to get current executable path (%d)\n", -ret);
		goto service_main_exit;
	}

	for (ret = (int)strlen(config_path); ret >= 0 && config_path[ret] != '\\'; ret--);

	if (ret > 0)
	{
		for (ret--; ret >= 0 && config_path[ret] != '\\'; ret--);
	}

	if (ret < 0)
	{
		ret = -EINVAL;
		goto service_main_exit;
	}
	else
	{
		strcpy(&config_path[ret], "\\ELProxy.conf");
	}

	// Load the config
	ret = proxy_load_conf(&ph, config_path);
	if (ret < 0)
	{
		proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to load config from '%s' (%d): %s\n", config_path, -ret, strerror(-ret));
		goto service_main_exit;
	}

	// Start listening
	ret = proxy_open(&ph);
	if (ret < 0)
	{
		proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to open proxy (%d): %s\n", -ret, strerror(-ret));
		goto service_main_exit;
	}

	ret = proxy_start(&ph);
	if (ret < 0)
	{
		proxy_log(&ph, LOG_LEVEL_FATAL, "Failed to start proxy (%d): %s\n", -ret, strerror(-ret));
		goto service_main_exit;
	}

	proxy_log(&ph, LOG_LEVEL_INFO, "Ready.\n");

	service_report_status(SERVICE_RUNNING, NO_ERROR, 0);

	// Main dispatch loop
	while (ret == 0 && sentinel == 0)
	{
		proxy_log(&ph, LOG_LEVEL_DEBUG, "Starting a processing run...\n");
		ret = proxy_process(&ph);
		if (ret < 0)
		{
			switch (ret)
			{
			case -EINTR:
				ret = 0;
				break;
			default:
				proxy_log(&ph, LOG_LEVEL_FATAL, "Message processing failure (%d): %s\n", -ret, strerror(-ret));
				break;
			}
		}
	}

	proxy_log(&ph, LOG_LEVEL_INFO, "Shutting down...\n");

service_main_exit:
	proxy_free(&ph);

service_main_exit_early:
	service_report_status(SERVICE_STOPPED, ret, 0);
}

static void service_report_status(int state, int exit_code, int wait_hint)
{
	static unsigned long check_point = 1;

	SERVICE_STATUS status =
	{
		.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
		.dwCurrentState = state,
		.dwWin32ExitCode = exit_code == 0 ? 0 : ERROR_SERVICE_SPECIFIC_ERROR,
		.dwServiceSpecificExitCode = exit_code,
		.dwWaitHint = wait_hint,
		.dwControlsAccepted = state == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP,
		.dwCheckPoint = ((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED)) ? 0 : check_point++,
	};

	SetServiceStatus(service_status_handle, &status);
}

static int service_uninstall()
{
	int ret;
	SC_HANDLE scman;
	SC_HANDLE svc;

	// Get a handle to the SCM database.
	scman = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scman == NULL)
	{
		return GetLastError();
	}

	// Create the service
	svc = OpenService(
		scman,              // SCM database
		service_name,       // name of service
		DELETE);            // need delete access
	if (svc == NULL)
	{
		ret = GetLastError();

		goto service_uninstall_exit;
	}

	ret = DeleteService(svc);
	if (ret == 0)
	{
		ret = -(long)GetLastError();
	}
	else
	{
		ret = 0;
	}

	CloseServiceHandle(svc);

service_uninstall_exit:
	CloseServiceHandle(scman);

	return ret;
}
