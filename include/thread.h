/*!
 * @file thread.h
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
 * @brief Internal API for threads
 */

#ifndef _thread_h
#define _thread_h

#include "mutex.h"

#include <stdint.h>

/*!
 * @brief Represents an instance of a thread
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::thread_init function, and subsequently
 * freed by ::thread_free when the thread is no longer needed.
 */
struct thread_handle
{
	/// Private data - used internally by thread functions
	void *priv;

	/// Pointer to the function to be called by the thread when it starts
	void * (*func_ptr)(void *);

	/// Context to pass to thread_handle::func_ptr
	void *func_ctx;

	/// Size for stack used for the thread
	unsigned int stack_size;
};

/*!
 * @brief Frees data allocated by ::thread_init
 *
 * @param[in,out] th Target thread instance
 */
void thread_free(struct thread_handle *th);

/*!
 * @brief Initializes the private data in a ::thread_handle
 *
 * @param[in,out] th Target thread instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int thread_init(struct thread_handle *th);

/*!
 * @brief Blocks until the target thread returns
 *
 * @param[in,out] th Target thread instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int thread_join(struct thread_handle *th);

/*!
 * @brief Starts the target thread instance
 *
 * @param[in,out] th Target thread instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int thread_start(struct thread_handle *th);

#endif /* _thread_h */
