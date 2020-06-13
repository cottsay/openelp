/*!
 * @file digest.c
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
 * @brief Digest generation implementation
 */

#include "digest.h"
#include "md5.h"

#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

/*!
 * @brief Converts a 8-bit value to a base 16 string
 *
 * @param[in] data Numeric value to convert
 * @param[out] result Resulting ASCII characters
 */
static inline void digest_to_hex8(const uint8_t data, char result[2]);

/*!
 * @brief Converts a 8-bit value to a base 16 string with uppercase letters
 *
 * @param[in] data Numeric value to convert
 * @param[out] result Resulting ASCII characters
 */
static inline void digest_to_hex8_u(const uint8_t data, char result[2]);

/*!
 * @brief Converts a big-endian 32-bit value to a base 16 string
 *
 * @param[in] data Numeric value to convert (big-endian)
 * @param[out] result Resulting ASCII characters
 */
static inline void digest_to_hex32_be(const uint32_t data, char result[8]);

/*!
 * @brief Converts a base 16 string to a 4-bit value
 *
 * @param[in] data ASCII character to convert
 * @returns Resulting numeric value
 */
static inline uint8_t hex4_to_digest(const char data);

/*!
 * @brief Converts a base 16 string to an 8-bit value
 *
 * @param[in] data ASCII characters to convert
 * @returns Resulting numeric value
 */
static inline uint8_t hex8_to_digest(const char data[2]);

void digest_get(const uint8_t *data, const unsigned int len, uint8_t result[DIGEST_LEN])
{
	MD5_CTX ctx;

	MD5_Init(&ctx);

	MD5_Update(&ctx, data, len);

	MD5_Final((unsigned char *)result, &ctx);
}

static inline void digest_to_hex8(const uint8_t data, char result[2])
{
	static const char lookup[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	result[0] = lookup[data / 16];
	result[1] = lookup[data % 16];
}

static inline void digest_to_hex8_u(const uint8_t data, char result[2])
{
	static const char lookup[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	result[0] = lookup[data / 16];
	result[1] = lookup[data % 16];
}

void digest_to_hex32(const uint32_t data, char result[8])
{
	digest_to_hex32_be(htonl(data), result);
}

static inline void digest_to_hex32_be(const uint32_t data, char result[8])
{
	const uint8_t *tgt = (const uint8_t *)&data;

	digest_to_hex8(tgt[0], &result[0]);
	digest_to_hex8(tgt[1], &result[2]);
	digest_to_hex8(tgt[2], &result[4]);
	digest_to_hex8(tgt[3], &result[6]);
}

void digest_to_str(const uint8_t md5[DIGEST_LEN], char result[2 * DIGEST_LEN + 1])
{
	size_t i;

	for (i = 0; i < DIGEST_LEN; i++, result += 2)
	{
		digest_to_hex8_u(md5[i], result);
	}
}

static inline uint8_t hex4_to_digest(const char data)
{
	char base = '0';

	if (data > '9')
	{
		if (data > 'F')
		{
			if (data > 'f')
			{
				return 0;
			}
			else
			{
				base = 'a' - 10;
			}
		}
		else
		{
			base = 'A' - 10;
		}
	}

	if (data < base)
	{
		return 0;
	}

	return data - base;
}

static inline uint8_t hex8_to_digest(const char data[2])
{
	return (hex4_to_digest(data[0]) * 16) + hex4_to_digest(data[1]);
}

uint32_t hex32_to_digest(const char data[8])
{
	union
	{
		uint32_t value;
		uint8_t parts[4];
	} result = {
		.parts = {
			hex8_to_digest(&data[0]),
			hex8_to_digest(&data[2]),
			hex8_to_digest(&data[4]),
			hex8_to_digest(&data[6]),
		},
	};

	return ntohl(result.value);
}
