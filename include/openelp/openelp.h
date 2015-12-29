/*!
 * @file openelp.h
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
 *
 * @brief Public API for OpenELP, An Open Source EchoLink&reg; Proxy
 *
 * @section DESCRIPTION
 *
 * These definitions, data structures and functions manipulate and operate the
 * proxy.
 */
#ifndef _openelp_h
#define _openelp_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

#ifdef _WIN32
#  ifndef OPENELP_API
#    define OPENELP_API __declspec(dllimport)
#  endif
#else
#  define OPENELP_API
#endif

/*!
 * @brief Length in bytes of the expected password response from the client
 */
#define PROXY_PASS_RES_LEN 16

enum LOG_LEVEL
{
	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
};

enum LOG_MEDIUM
{
	LOG_MEDIUM_STDOUT = 0,
	LOG_MEDIUM_FILE,
	LOG_MEDIUM_SYSLOG,
	LOG_MEDIUM_EVENTLOG,
};

struct proxy_conf
{
	char *password;
	uint16_t port;
};

struct proxy_handle
{
	struct proxy_conf conf;
	void *priv;
};

/*
 * Resource Management
 */
int OPENELP_API proxy_load_conf(struct proxy_handle *ph, const char *path);
void OPENELP_API proxy_ident(struct proxy_handle *ph);
int OPENELP_API proxy_init(struct proxy_handle *ph);
void OPENELP_API proxy_free(struct proxy_handle *ph);
int OPENELP_API proxy_open(struct proxy_handle *ph);
void OPENELP_API proxy_close(struct proxy_handle *ph);
void OPENELP_API proxy_drop(struct proxy_handle *ph);
int OPENELP_API proxy_process(struct proxy_handle *ph);
void OPENELP_API proxy_shutdown(struct proxy_handle *ph);
void OPENELP_API proxy_log(struct proxy_handle *ph, enum LOG_LEVEL lvl, const char *fmt, ...);
void OPENELP_API proxy_log_level(struct proxy_handle *ph, const enum LOG_LEVEL lvl);
int OPENELP_API proxy_log_select_medium(struct proxy_handle *ph, const enum LOG_MEDIUM medium, const char *target);

/*
 * Helpers
 */
int OPENELP_API get_nonce(uint32_t *nonce);
int OPENELP_API get_password_response(const uint32_t nonce, const char *password, uint8_t response[PROXY_PASS_RES_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* _openelp_h */
