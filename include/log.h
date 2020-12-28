/*!
 * @file log.h
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
 * @brief Internal API for message logging
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdarg.h>
#include <stdint.h>

#include "openelp/openelp.h"

/*!
 * @brief Represents an instance of logging infrastructure
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::log_init function, and subsequently
 * freed by ::log_free when the logging infrastructure is no longer needed.
 */
struct log_handle {
	/*! Private data - used internally by log functions */
	void *priv;

	/*! Severigy threshold for reporting log messages */
	uint32_t level;

	/*! Logging facility used to report logging event messages */
	uint32_t medium;
};

/*!
 * @brief Closes the current log medium by switching to ::LOG_MEDIUM_NONE
 *
 * @param[in,out] log Target logging infrastructure instance
 */
void log_close(struct log_handle *log);

/*!
 * @brief Frees data allocated by ::log_init
 *
 * @param[in,out] log Target logging infrastructure instance
 */
void log_free(struct log_handle *log);

/*!
 * @brief Identify the current OpenELP version to the current log medium
 *
 * @param[in] log Target logging infrastructure instance
 */
void log_ident(struct log_handle *log);

/*!
 * @brief Initializes the private data in a ::log_handle
 *
 * @param[in,out] log Target logging infrastructure instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int log_init(struct log_handle *log);

/*!
 * @brief Converts the given log medium value to a static string representation
 *
 * @param[in] medium Log medium value to convert
 *
 * @returns Pointer to a null-terminated static const string
 */
const char *log_medium_to_str(enum LOG_MEDIUM medium);

/*!
 * @brief Opens the log infrastructure by switching to ::LOG_MEDIUM_STDOUT
 *
 * @param[in,out] log Target logging infrastructure instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int log_open(struct log_handle *log);

/*!
 * @brief Logs the given message to the current logging facility
 *
 * @param[in] log Target logging infrastructure instance
 * @param[in] lvl Message importance level
 * @param[in] fmt String format of message
 * @param[in] ... Arguments for format specification
 */
void log_printf(struct log_handle *log, enum LOG_LEVEL lvl,
		const char *fmt, ...);

/*!
 * @brief Changes the target logging medium
 *
 * @param[in,out] log Target logging infrastructure instance
 * @param[in] medium New logging medium to use
 * @param[in] target Medium target, where appropriate
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int log_select_medium(struct log_handle *log, enum LOG_MEDIUM medium,
		      const char *target);

/*!
 * @brief Logs the given message to the current logging facility
 *
 * @param[in] log Target logging infrastructure instance
 * @param[in] lvl Message importance level
 * @param[in] fmt String format of message
 * @param[in] args Arguments for format specification
 */
void log_vprintf(struct log_handle *log, enum LOG_LEVEL lvl,
		 const char *fmt, va_list args);

#endif /* LOG_H_ */
