/*!
 * @file proxy_client.h
 *
 * @copyright
 * Copyright &copy; 2020, Scott K Logan
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
 * @brief Internal API for a client connection
 */

#ifndef PROXY_CLIENT_H_
#define PROXY_CLIENT_H_

#include <stdint.h>

#include "proxy_msg.h"

/*!
 * @brief Represents an instance of a client connection
 *
 * This struct should be initialized to zero before being used. The private
 * data should be initialized using the ::proxy_client_init function, and
 * subsequently freed by ::proxy_client_free when the worker is no longer
 * needed.
 */
struct proxy_client_handle {
	/*! Private data - used internally by proxy_client functions */
	void *priv;

	/*! Hostname or address of the proxy server */
	char *host_addr;

	/*! Port number of the proxy server */
	char *host_port;

	/*! The callsign to use during proxy authorization */
	char *callsign;

	/*! The password to use during proxy authorization */
	char *password;
};

/*!
 * @brief Connect to the proxy server
 *
 * @param[in,out] ch Target client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_client_connect(struct proxy_client_handle *ch);

/*!
 * @brief Disconnect from the proxy server
 *
 * @param[in,out] ch Target client connection instance
 */
void proxy_client_disconnect(struct proxy_client_handle *ch);

/*!
 * @brief Frees data allocated by ::proxy_client_init
 *
 * @param[in,out] ch Target client connection instance
 */
void proxy_client_free(struct proxy_client_handle *ch);

/*!
 * @brief Initializes the private data in a ::proxy_client_handle
 *
 * @param[in,out] ch Target client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_client_init(struct proxy_client_handle *ch);

/*!
 * @brief Copies a message which has been sent from the proxy server
 *
 * @param[in] ch Target client connection instance
 * @param[out] msg Received message header
 * @param[out] buff Buffer to copy received into
 * @param[in] buff_len Maximum number of bytes of data to read
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_client_recv(struct proxy_client_handle *ch, struct proxy_msg *msg,
		      uint8_t *buff, size_t buff_len);

/*!
 * @brief Send a message to the connected proxy server
 *
 * @param[in] ch Target client connection instance
 * @param[in] msg Message header to send
 * @param[in] buff Buffer containing data to be sent
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_client_send(struct proxy_client_handle *ch,
		      const struct proxy_msg *msg, const uint8_t *buff);

#endif /* PROXY_CLIENT_H_ */
