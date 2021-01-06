/*!
 * @file worker.h
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
 * @brief Internal API for threaded workers
 */

#ifndef WORKER_H_
#define WORKER_H_

#include <stdint.h>

#include "thread.h"

/*!
 * @brief Represents an instance of a worker
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::worker_init function, and subsequently
 * freed by ::worker_free when the worker is no longer needed.
 */
struct worker_handle {
	/*! Private data - used internally by worker functions */
	void *priv;

	/*! Pointer to the function called when work is available */
	void (*func_ptr)(struct worker_handle *wh);

	/*! Context to pass to worker_handle::func_ptr */
	void *func_ctx;

	/*! Size for stack used for the thread */
	unsigned int stack_size;

	/*! Optional maximum idle time in milliseconds between work */
	uint32_t periodic_wake;
};

/*!
 * @brief Frees data allocated by ::worker_init
 *
 * @param[in,out] wh Target worker instance
 */
void worker_free(struct worker_handle *wh);

/*!
 * @brief Initializes the private data in a ::worker_handle
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int worker_init(struct worker_handle *wh);

/*!
 * @brief Determine if the worker is currently waiting for work
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 1 if idle, 0 otherwise
 */
int worker_is_idle(struct worker_handle *wh);

/*!
 * @brief Blocks until the target worker stops
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * If work has already been signaled by ::worker_wake but has not been started
 * when this function is called, this function will block until that work is
 * finished.
 */
int worker_join(struct worker_handle *wh);

/*!
 * @brief Starts the target worker instance
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int worker_start(struct worker_handle *wh);

/*!
 * @brief Wait for the worker to become idle
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 0 when the worker becomes idle, negative ERRNO value on failure
 */
int worker_wait_idle(struct worker_handle *wh);

/*!
 * @brief Signal to the worker that work is available
 *
 * @param[in,out] wh Target worker instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * Barring abnormal termination of the backing thread, a successful call to
 * this function guarantees that work will be performed, even if the work does
 * not begin before a subsequent call to ::worker_join.
 */
int worker_wake(struct worker_handle *wh);

#endif /* WORKER_H_ */
