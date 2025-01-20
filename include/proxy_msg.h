/*!
 * @file proxy_msg.h
 *
 * @copyright
 * Copyright &copy; 2025, Scott K Logan
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
 * @brief Proxy message format
 */

#ifndef PROXY_MSG_H_
#define PROXY_MSG_H_

#include <stdint.h>

/*!
 * @brief Message types used in communication between the proxy and the client
 */
enum PROXY_MSG_TYPE {
	/*!
	 * @brief The proxy should open a new TCP connection
	 *
	 * * Sent by: client
	 * * Expected data: 0 bytes
	 */
	PROXY_MSG_TYPE_TCP_OPEN = 1,

	/*!
	 * @brief Data which has been sent or should be sent over the TCP connection
	 *
	 * The address field is ignored in this message
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_TCP_DATA,

	/*!
	 * @brief The TCP has been, or should be closed
	 *
	 * The address field is ignored in this message
	 *
	 * When the client requests that the TCP connection be closed, the proxy
	 * responds with another ::PROXY_MSG_TYPE_TCP_CLOSE message
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 0 bytes
	 */
	PROXY_MSG_TYPE_TCP_CLOSE,

	/*!
	 * @brief The status of the TCP connection
	 *
	 * The address field is ignored in this message
	 *
	 * The data included with this message should be zeroed when the TCP connection
	 * was opened successfully, and non-zero otherwise
	 *
	 * * Sent by: proxy
	 * * Expected data: 4 bytes
	 */
	PROXY_MSG_TYPE_TCP_STATUS,

	/*!
	 * @brief Data which has been or should be sent of the UDP Data connection
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_UDP_DATA,

	/*!
	 * @brief Data which has been or should be sent of the UDP Control connection
	 *
	 * * Sent by: client or proxy
	 * * Expected data: 1 or more bytes
	 */
	PROXY_MSG_TYPE_UDP_CONTROL
};

#ifdef _WIN32
#  pragma pack(push, 1)
#endif
/*!
 * @brief Proxy message header
 */
struct proxy_msg {
	/*! Type of proxy message, should be one of ::PROXY_MSG_TYPE */
	uint8_t type;

	/*! 32-bit IPv4 address, if applicable */
	uint32_t address;

	/*! Number of bytes in proxy_msg::data */
	uint32_t size;
#ifdef _WIN32
};
#  pragma pack(pop)
#else
} __attribute__((packed));
#endif

#endif /* PROXY_MSG_H_ */
