/*!
 * @file regex.h
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
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of OpenELP nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * @copyright
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @copyright
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan &lt;logans@cottsay.net&gt;
 *
 * @brief Internal API for regular expression matching
 */

#ifndef _regex_h
#define _regex_h

/*!
 * @brief Represents an instance of a compiled regular expression
 *
 * This struct should be initialized to zero before bing used. The private data
 * should be initialized using the ::regex_init function, and subsequently
 * freed by ::regex_free when the regular expression is no longer needed.
 */
struct regex_handle
{
	/// Private data - used internally by regex functions
	void *priv;
};

/*!
 * @brief Compiles the given regular expression pattern
 *
 * @param[in,out] re Target regular expression instance
 * @param[in] pattern Regular expression pattern to be compiled
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int regex_compile(struct regex_handle *re, const char *pattern);

/*!
 * @brief Frees data allocated by ::regex_init
 *
 * @param[in,out] re Target regular expression instance
 */
void regex_free(struct regex_handle *re);

/*!
 * @brief Initializes the private data in a ::regex_handle
 *
 * @param[in,out] re Target regular expression instance
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
int regex_init(struct regex_handle *re);

/*!
 * @brief Tests the given subject against the regular expression
 *
 * @param[in] re Target regular expression instance
 * @param[in] subject Null terminated string to test against the regular
 *                    expression
 *
 * @returns 1 on match, 0 if no match, negative ERRNO value on failure
 */
int regex_is_match(struct regex_handle *re, const char *subject);

#endif /* _regex_h */
