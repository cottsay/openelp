/*!
 * @file conf.h
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
 * @brief Internal API for configuration values
 */

#ifndef CONF_H_
#define CONF_H_

#include <stdint.h>
#include <stdio.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "openelp/openelp.h"
#include "log.h"

/*!
 * @brief Frees data allocated by ::conf_init
 *
 * @param[in,out] conf Target configuration instance
 */
void conf_free(struct proxy_conf *conf);

/*!
 * @brief Initializes the private data in a ::proxy_conf
 *
 * @param[in,out] conf Target configuration instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conf_init(struct proxy_conf *conf);

/*!
 * @brief Parses the values from the given file into the given configuration
 *
 * @param[in] file Null-terminated string containing the path to the config
 * @param[in,out] conf Target configuration instance
 * @param[in,out] log Handle for reporting logging events
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int conf_parse_file(const char *file, struct proxy_conf *conf,
		    struct log_handle *log);

#endif /* CONF_H_ */
