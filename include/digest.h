/*!
 * @file digest.h
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
 * @brief Internal API for digest generation
 */

#ifndef DIGEST_H_
#define DIGEST_H_

#include <stdint.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

/*! Length in bytes of all digests */
#define DIGEST_LEN 16

/*!
 * @brief Calculates the digest of the given data
 *
 * @param[in] data The data whose digest is calculated
 * @param[in] len Number of bytes at the location indicated by data
 * @param[out] result Resulting digest value
 */
void digest_get(const uint8_t *data, unsigned int len,
		uint8_t result[DIGEST_LEN]);

/*!
 * @brief Converts a 32-bit value to a base 16 string
 *
 * @param[in] data Numeric value to convert
 * @param[out] result Resulting ASCII characters
 */
void digest_to_hex32(uint32_t data, char result[8]);

/*!
 * @brief Converts the given digest to a null terminated string
 *
 * @param[in] md5 Input digest value
 * @param[out] result Resulting string representation of the given digest
 */
void digest_to_str(const uint8_t md5[DIGEST_LEN],
		   char result[2 * DIGEST_LEN + 1]);

/*!
 * @brief Converts a base 16 string to a 32-bit value
 *
 * @param[in] data ASCII characters to convert
 * @returns Resulting numeric value
 */
uint32_t hex32_to_digest(const char data[8]);

#endif /* DIGEST_H_ */
