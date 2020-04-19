/*!
 * @file conn_wsa_errno.c
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
 * @brief WSA error conversion implementation
 */

#include "conn_wsa_errno.h"

#include <winsock2.h>

#include <errno.h>

/// WSA error value coversion table
/// @TODO Can this be const?
static int wsa_errno[68] =
{
	10000,        EPERM,        ENOENT,       ESRCH,
	EINTR,        EIO,          ENXIO,        E2BIG,
	ENOEXEC,      EBADF,        ECHILD,       EAGAIN,
	ENOMEM,       EACCES,       EFAULT,       10015,
	EBUSY,        EEXIST,       EXDEV,        ENODEV,
	ENOTDIR,      EISDIR,       10022,        ENFILE,
	EMFILE,       ENOTTY,       10026,        EFBIG,
	ENOSPC,       ESPIPE,       EROFS,        EMLINK,
	10032,        10033,        10034,        EWOULDBLOCK,
	EINPROGRESS,  EALREADY,     ENOTSOCK,     EDESTADDRREQ,
	EMSGSIZE,     EPROTOTYPE,   ENOPROTOOPT,  EPROTONOSUPPORT,
	10044,        EOPNOTSUPP,   10046,        EAFNOSUPPORT,
	EADDRINUSE,   EADDRNOTAVAIL,ENETDOWN,     ENETUNREACH,
	ENETRESET,    ECONNABORTED, ECONNRESET,   ENOBUFS,
	EISCONN,      ENOTCONN,     10058,        10059,
	ETIMEDOUT,    ECONNREFUSED, ELOOP,        ENAMETOOLONG,
	10064,        EHOSTUNREACH, ENOTEMPTY,    10067,
};

int conn_wsa_errno()
{
	int ret = WSAGetLastError();

	if (ret >= 10000 && ret <= 10067)
	{
		return wsa_errno[ret - 10000];
	}

	return ret;
}
