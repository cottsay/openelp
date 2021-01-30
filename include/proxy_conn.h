/*!
 * @file proxy_conn.h
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
 * @brief Internal API for proxy client connections
 */

#ifndef PROXY_CONN_H_
#define PROXY_CONN_H_

#include "conn.h"

/*!
 * @brief Represents an instance of a proxy client connection
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::proxy_conn_init function, and subsequently
 * freed by ::proxy_conn_free when the regular expression is no longer needed.
 */
struct proxy_conn_handle {
	/*! Private data - used internally by proxy_conn functions */
	void *priv;

	/*! Reference to the parent proxy instance handle */
	struct proxy_handle *ph;

	/*! Null-terminated string containing the source address for client data */
	const char *source_addr;
};

/*!
 * @brief Claims a proxy connection for use by the given client
 *
 * @param[in,out] pc Target proxy client connection instance
 * @param[in] conn_client Connection to a client
 * @param[in] callsign The authentication callsign given by the client
 * @param[in] reconnect_only Non-zero to accept only if the callsign matches
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * Note that ownership of \p conn_client remains with the caller, but
 * \p conn_client must remain valid until the proxy client has finished
 * with it. Calling \p proxy_conn_finish should ensure this.
 */
int proxy_conn_accept(struct proxy_conn_handle *pc,
		      struct conn_handle *conn_client,
		      const char *callsign,
		      uint8_t reconnect_only);

/*!
 * @brief Begins an orderly shutdown of all active connections
 *
 * @param[in,out] pc Target proxy client connection instance
 */
void proxy_conn_drop(struct proxy_conn_handle *pc);

/*!
 * @brief Waits for the proxy_conn to close connections and become idle
 *
 * @param[in,out] pc Target proxy client connection instance
 */
void proxy_conn_finish(struct proxy_conn_handle *pc);

/*!
 * @brief Frees data allocated by ::proxy_conn_init
 *
 * @param[in,out] pc Target proxy client connection instance
 */
void proxy_conn_free(struct proxy_conn_handle *pc);

/*!
 * @brief Initializes the private data in a ::proxy_conn_handle
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_conn_init(struct proxy_conn_handle *pc);

/*!
 * @brief Determine if the connection is currently in use
 *
 * @param[in] pc Target proxy client connection instance
 *
 * @returns 1 if in use, 0 if idle
 */
int proxy_conn_in_use(struct proxy_conn_handle *pc);

/*!
 * @brief Blocking call to process new messages
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_conn_process(struct proxy_conn_handle *pc);

/*!
 * @brief Starts the client thread and prepares to accept connections
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_conn_start(struct proxy_conn_handle *pc);

/*!
 * @brief Disconnects the connected client and stops the client thread
 *
 * @param[in,out] pc Target proxy client connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int proxy_conn_stop(struct proxy_conn_handle *pc);

#endif /* PROXY_CONN_H_ */
