/*!
 * @file mutex.h
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
 * @brief Internal API for mutual exclusion objects
 */

#ifndef _mutex_h
#define _mutex_h

/*!
 * @brief Represents an instance of a condition variable
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::condvar_init function, and subsequently
 * freed by ::condvar_free when the condition variable is no longer needed.
 */
struct condvar_handle
{
	/// Private data - used internally by condvar functions
	void *priv;
};

/*!
 * @brief Represents an instance of a mutex
 *
 * This struct should be initialized to zero before being used. The private data
 * should be initialized using the ::mutex_init function, and subsequently
 * freed by ::mutex_free when the mutex is no longer needed.
 */
struct mutex_handle
{
	/// Private data - used internally by mutex functions
	void *priv;
};

/*!
 * @brief Frees data allocated by ::condvar_init
 *
 * @param[in,out] condvar Target condition variable instance
 */
void condvar_free(struct condvar_handle *condvar);

/*!
 * @brief Initializes the private data in a ::condvar_handle
 *
 * @param[in,out] condvar Target condition variable instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int condvar_init(struct condvar_handle *condvar);

/*!
 * @brief Unlocks the given mutex and blocks until awoken by ::condvar_wake_all
 *        or ::condvar_wake_one.
 *
 * @param[in,out] condvar Target condition variable instance
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int condvar_wait(struct condvar_handle *condvar, struct mutex_handle *mutex);

/*!
 * @brief Awakens all calls to ::condvar_wait currently blocked
 *
 * @param[in,out] condvar Target condition variable instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int condvar_wake_all(struct condvar_handle *condvar);

/*!
 * @brief Awakens a single blocked call to ::condvar_wait
 *
 * @param[in,out] condvar Target condition variable instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int condvar_wake_one(struct condvar_handle *condvar);

/*!
 * @brief Frees data allocated by ::mutex_init
 *
 * @param[in,out] mutex Target mutex instance
 */
void mutex_free(struct mutex_handle *mutex);

/*!
 * @brief Initializes the private data in a ::mutex_handle
 *
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int mutex_init(struct mutex_handle *mutex);

/*!
 * @brief Acquires the exclusive lock on the given mutex, blocking if it is
 *        already locked exclusively or shared
 *
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int mutex_lock(struct mutex_handle *mutex);

/*!
 * @brief Acquires a shared Lock on the given mutex, blocking if it is already
 *        locked exclusively
 *
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int mutex_lock_shared(struct mutex_handle *mutex);

/*!
 * @brief Releases the exclusive lock acquired by ::mutex_lock
 *
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int mutex_unlock(struct mutex_handle *mutex);

/*!
 * @brief Releases the shared lock acquired by ::mutex_lock_shared
 *
 * @param[in,out] mutex Target mutex instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int mutex_unlock_shared(struct mutex_handle *mutex);

#endif /* _mutex_h */
