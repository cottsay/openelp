/*!
 * @file openelp.h
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
	 /// Public API decorator
#    define OPENELP_API __declspec(dllimport)
#  endif
#else
   /// Public API decorator
#  define OPENELP_API
#endif

/// Length in bytes of the expected password response from the client
#define PROXY_PASS_RES_LEN 16

/*!
 * @brief Severity level of log information
 */
enum LOG_LEVEL
{
	/// A fatal event, which will result in program termination
	LOG_LEVEL_FATAL = 0,

	/// A failure event which should not have happened under normal circumstances
	LOG_LEVEL_ERROR,

	/// An unusual event which could indicate a problem
	LOG_LEVEL_WARN,

	/// An event which is part of the normal lifecycle of the program
	LOG_LEVEL_INFO,

	/// A verbose event
	LOG_LEVEL_DEBUG,
};

/*!
 * @brief Logging facilities to write logging events to
 */
enum LOG_MEDIUM
{
	/// Discard all log messages
	LOG_MEDIUM_NONE = 0,

	/// Print all log messages to stdout and stderr
	LOG_MEDIUM_STDOUT,

	/// Append all log messages to the given file.
	LOG_MEDIUM_FILE,

	/// Send all log messages to Syslog
	LOG_MEDIUM_SYSLOG,

	/// Send all log messages to the Windows Event Log
	LOG_MEDIUM_EVENTLOG,
};

/*!
 * @brief Configuration instance for a ::proxy_handle
 *
 * These values correspond to those in the proxy configuration file. If the
 * value is absent or empty in the configuration file, it is NULL in this
 * struct.
 */
struct proxy_conf
{
	/// Address to bind to for listening for client connections
	char *bind_addr;

	/// Address to bind to for sending and receiving the client's data
	char *bind_addr_ext;

	/// Additional addresses to bind to for additional clients' data
	char **bind_addr_ext_add;

	/// Number of additional addresses specified by bind_addr_ext_add
	uint16_t bind_addr_ext_add_len;

	/// Regular expression for matching allowed callsigns
	char *calls_allowed;

	/// Regular expression for matching denied callsigns
	char *calls_denied;

	/// Required password for access
	char *password;

	/// Port on which to listen for client connections
	uint16_t port;

	/// Name to use when registering in the official list
	char *reg_name;

	/// Optional comment showen in the official proxy list
	char *reg_comment;
};

/*!
 * @brief Represents an instance of an EchoLink proxy
 *
 * This struct should be initialized to zero before bing used. The private data
 * should be initialized using the ::proxy_init function, and subsequently
 * freed by ::proxy_free when the proxy is no longer needed.
 */
struct proxy_handle
{
	/// Configuration for the proxy
	struct proxy_conf conf;

	/// Private data - used internally by proxy functions
	void *priv;
};

/*!
 * @brief Get a single-use 32-bit number
 *
 * @param[out] nonce Resulting single-use number
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API get_nonce(uint32_t *nonce);

/*!
 * @brief Gets the expected response for a given nonce and password
 *
 * @param[in] nonce Single use number
 * @param[in] password Null terminated password string
 * @param[out] response Expected password response
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API get_password_response(const uint32_t nonce, const char *password, uint8_t response[PROXY_PASS_RES_LEN]);

/*!
 * @brief Authorizes the given callsign against the proxy's configuration
 *
 * @param[in] ph Target proxy instance
 * @param[in] callsign Null-terminated string containing the callsign
 *
 * @returns 1 if call is authorized, 0 if not, negative ERRNO value on failure
 */
int OPENELP_API proxy_authorize_callsign(struct proxy_handle *ph, const char *callsign);

/*!
 * @brief Closes the proxy so no more clients can connect
 *
 * @param[in,out] ph Target proxy instance
 */
void OPENELP_API proxy_close(struct proxy_handle *ph);

/*!
 * @brief Drops all currently connected clients from the proxy
 *
 * @param[in,out] ph Target proxy instance
 */
void OPENELP_API proxy_drop(struct proxy_handle *ph);

/*!
 * @brief Frees data allocated by ::proxy_init
 *
 * @param[in,out] ph Target proxy instance
 */
void OPENELP_API proxy_free(struct proxy_handle *ph);

/*!
 * @brief Instructs the proxy to identify itself to the current log medium
 *
 * @param[in] ph Target proxy instance
 */
void OPENELP_API proxy_ident(struct proxy_handle *ph);

/*!
 * @brief Initializes the private data in a ::proxy_handle
 *
 * @param[in,out] ph Target proxy instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_init(struct proxy_handle *ph);

/*!
 * @brief Loads the configuration from the file at the given path
 *
 * @param[in,out] ph Target proxy instance
 * @param[in] path Null-terminated string containing the path to the file
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_load_conf(struct proxy_handle *ph, const char *path);

/*!
 * @brief Logs the given message to the current medium if lvl is high enough
 *
 * @param[in] ph Target proxy instance
 * @param[in] lvl Message's level of importance
 * @param[in] fmt String format of message
 * @param[in] ... Arguments for format specification
 */
void OPENELP_API proxy_log(struct proxy_handle *ph, enum LOG_LEVEL lvl, const char *fmt, ...);

/*!
 * @brief Changes the log message importance threshold
 *
 * @param[in,out] ph Target proxy instance
 * @param[in] lvl New message importance threshold
 */
void OPENELP_API proxy_log_level(struct proxy_handle *ph, const enum LOG_LEVEL lvl);

/*!
 * @brief Changes the target logging medium
 *
 * @param[in,out] ph Target proxy instance
 * @param[in] medium New logging medium to use
 * @param[in] target Medium target, where appropriate
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_log_select_medium(struct proxy_handle *ph, const enum LOG_MEDIUM medium, const char *target);

/*!
 * @brief Opens the proxy for client connections
 *
 * @param[in,out] ph Target proxy instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_open(struct proxy_handle *ph);

/*!
 * @brief Blocking call to process new clients
 *
 * @param[in,out] ph Target proxy instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_process(struct proxy_handle *ph);

/*!
 * @brief Gracefully shut down all proxy operations asynchronously
 *
 * @param[in,out] ph Target proxy instance
 */
void OPENELP_API proxy_shutdown(struct proxy_handle *ph);

/*!
 * @brief Starts the client processing thread(s)
 *
 * @param[in,out] ph Target proxy instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int OPENELP_API proxy_start(struct proxy_handle *ph);

#ifdef __cplusplus
}
#endif

#endif /* _openelp_h */
