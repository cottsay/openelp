/*!
 * @file log_eventlog.h
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
 * @brief Defenitions and prototypes for interfacing with the Windows Event Log
 */

#ifndef _log_eventlog_h
#define _log_eventlog_h

#ifdef HAVE_EVENTLOG
#  include <windows.h>
#  include <winbase.h>
#  include "log_eventlog_messages.h"
#  define EVENTLOG_ERRNO -(long)GetLastError()
typedef HANDLE EVENTLOG_HANDLE;
#else
#  define DeregisterEventSource(...)
#  define RegisterEventSource(...) NULL
#  define ReportEvent(handle, type, cat, eventId, userSid, numStrings, dataSize, strings, rawData) (void)handle, (void)type, (void)cat, (void)eventId, (void)userSid, (void)numStrings, (void)dataSize, (void)strings, (void)rawData
#  define EVENTLOG_ERRNO -ENOTSUP
#  define EVENTLOG_ERROR_TYPE 0
#  define EVENTLOG_WARNING_TYPE 0
#  define EVENTLOG_INFORMATION_TYPE 0
#  define LOG_IDENT_FATAL 0
#  define LOG_IDENT_ERROR 0
#  define LOG_IDENT_WARN 0
#  define LOG_IDENT_INFO 0
#  define LOG_IDENT_DEBUG 0
typedef void * EVENTLOG_HANDLE;
#endif

#endif /* _log_eventlog_h */
