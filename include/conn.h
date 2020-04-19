/*!
 * @file conn.h
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
 * @brief Internal API for network connections
 */

#ifndef _conn_h
#define _conn_h

#include <stdint.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

/*!
 * @brief Supported connection protocols
 */
enum CONN_TYPE
{
	/// Transmission Control Protocol
	CONN_TYPE_TCP,

	/// User Datagram Protocol
	CONN_TYPE_UDP,
};

/*!
 * @brief Represents an instance of a network connection
 *
 * This struct should be initialized to zero before bing used. The private data
 * should be initialized using the ::conn_init function, and subsequently
 * freed by ::conn_free when the network connection is no longer needed.
 */
struct conn_handle
{
	/// Private data - used internally by conn functions
	void *priv;

	/// Local network interface to bind to, or NULL for all
	const char *source_addr;

	/// Local socket port to bind to, or NULL for any
	const char *source_port;

	/// Protocol to use for this connection
	enum CONN_TYPE type;
};

/*!
 * @brief Blocks until a connection is made to the given network connection
 *
 * @param[in,out] conn Target network connection instance
 * @param[in,out] accepted Network connection instance for newly accepted client
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_accept(struct conn_handle *conn, struct conn_handle *accepted);

/*!
 * @brief Closes the target connection with the client
 *
 * @param[in,out] conn Target network connection instance
 */
void conn_close(struct conn_handle *conn);

/*!
 * @brief Opens a connection to a remote socket
 *
 * @param[in,out] conn Target network connection instance
 * @param[in] addr Address of listening network host
 * @param[in] port Socket port on listening network host
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_connect(struct conn_handle *conn, const char *addr, const char *port);

/*!
 * @brief Drops any active connections but doesn't close the connection
 *
 * @param[in,out] conn Target network connection instance
 */
void conn_drop(struct conn_handle *conn);

/*!
 * @brief Frees data allocated by ::conn_init
 *
 * @param[in,out] conn Target network connection instance
 */
void conn_free(struct conn_handle *conn);

/*!
 * @brief Initializes the private data in a ::conn_handle
 *
 * @param[in,out] conn Target network connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_init(struct conn_handle *conn);

/*!
 * @brief Blocking call to listen for incoming connections from clients
 *
 * @param[in,out] conn Target network connection instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_listen(struct conn_handle *conn);

/*!
 * @brief Copies data which has been transferred to the connection
 *
 * @param[in] conn Target network connection instance
 * @param[out] buff Buffer to copy received into
 * @param[in] buff_len Number of bytes of data to expect
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_recv(struct conn_handle *conn, uint8_t *buff, size_t buff_len);

/*!
 * @brief Like ::conn_recv, but for any client and any amount of data
 *
 * @param[in] conn Target network connection instance
 * @param[out] buff Buffer to copy received into
 * @param[in] buff_len Maximu number of bytes of data to read
 * @param[out] addr Remote address of sending client, or NULL if unwanted
 * @param[out] port Remote port on sending client, or NULL if unwanted
 *
 * @returns Number of bytes copied on success, negative ERRNO value on failure
 */
int conn_recv_any(struct conn_handle *conn, uint8_t *buff, size_t buff_len, uint32_t *addr, uint16_t *port);

/*!
 * @brief Send data to the connected client
 *
 * @param[in] conn Target network connection instance
 * @param[in] buff Buffer containing data to be sent
 * @param[in] buff_len Number of bytes in buff to send
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_send(struct conn_handle *conn, const uint8_t *buff, size_t buff_len);

/*!
 * @brief Like ::conn_send, but to a specified, unconnected client
 *
 * @param[in] conn Target network connection instance
 * @param[in] buff Buffer containing data to be sent
 * @param[in] buff_len Number of bytes in buff to send
 * @param[in] addr Remote address of listening client
 * @param[in] port Remote port on listening client
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conn_send_to(struct conn_handle *conn, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port);

/*!
 * @brief Stops socket operations but does not close the socket
 *
 * @param[in,out] conn Target network connection instance
 */
void conn_shutdown(struct conn_handle *conn);

#endif /* _conn_h */
