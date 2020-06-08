/*!
 * @file registration.h
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
 * @brief Internal API for proxy server registration
 */

#ifndef _registration_h
#define _registration_h

#include "conf.h"

/*!
 * @brief Represents an instance of proxy registration service
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::registration_service_init function, and
 * subsequently freed by ::registration_service_free when the registration
 * infrastructure is no longer needed.
 */
struct registration_service_handle
{
	/// Private data - used internally by proxy_conn functions
	void *priv;
};

/*!
 * @brief Frees data allocated by ::registration_service_init
 *
 * @param[in,out] rs Target registration service instance
 */
void registration_service_free(struct registration_service_handle *rs);

/*!
 * @brief Initializes the private data in a ::registration_service_handle
 *
 * @param[in,out] rs Target registration service instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int registration_service_init(struct registration_service_handle *rs);

/*!
 * @brief Starts the registration service thread
 *
 * @param[in,out] rs Target registration service instance
 * @param[in] conf Proxy configuration
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int registration_service_start(struct registration_service_handle *rs, const struct proxy_conf *conf);

/*!
 * @brief Sends a final status message and stops the registration thread
 *
 * @param[in,out] rs Target registration service instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int registration_service_stop(struct registration_service_handle *rs);

/*!
 * @brief Queues a registration status message update
 *
 * @param[in,out] rs Target registration service instance
 * @param[in] slots_used Number of proxy slots currently in use
 * @param[in] slots_total Total number of configured proxy slots
 */
void registration_service_update(struct registration_service_handle *rs, size_t slots_used, size_t slots_avail);

#endif /* _registration_h */
