/*!
 * @file log.h
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

#ifndef _log_h
#define _log_h

#include <stdarg.h>
#include <stdint.h>

#include "openelp/openelp.h"

struct log_handle
{
	void *priv;
	uint32_t medium;
	uint32_t level;
};

void log_close();
void log_free(struct log_handle *log);
void log_ident(struct log_handle *log);
int log_init(struct log_handle *log);
const char * log_medium_to_str(enum LOG_MEDIUM medium);
int log_open(struct log_handle *log, const char *target);
void log_printf(struct log_handle *log, enum LOG_LEVEL lvl, const char *fmt, ...);
int log_select_medium(struct log_handle *log, const enum LOG_MEDIUM medium, const char *target);
void log_vprintf(struct log_handle *log, enum LOG_LEVEL lvl, const char *fmt, va_list args);

#endif /* _log_h */
