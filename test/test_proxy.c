/*!
 * @file test_proxy.c
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
 * @brief Tests related to the proxy itself
 */

#include <errno.h>
#include <stdio.h>

#include "openelp/openelp.h"

/*!
 * @brief Basic test of proxy password response generation
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Basic test of proxy password response generation
 */
static int test_proxy_password_response(void);

/*!
 * @brief Main entry point for MD5 tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

int main(void)
{
	int ret = 0;

	ret |= test_proxy_password_response();

	return ret;
}

static int test_proxy_password_response(void)
{
	const uint32_t nonce = 0x4d3b6d47;
	static const char password[] = "asdf1234";
	const uint8_t expected_response[16] = {
		0x0c, 0x0b, 0xb9, 0x83, 0x5f, 0x31, 0x95, 0x53,
		0x10, 0x4b, 0xf9, 0x10, 0xfb, 0x72, 0x45, 0xec,
	};
	uint8_t response[16] = { 0x00 };
	int ret;
	size_t i;

	ret = get_password_response(nonce, password, response);
	if (ret != 0) {
		fprintf(stderr, "Error: get_password_response returned %d\n",
			ret);
		return ret;
	}

	for (i = 0; i < 16; i++) {
		if (expected_response[i] != response[i]) {
			fprintf(stderr,
				"Error: Password response value missmatch at index %lu. Expected 0x%02hx, got 0x%02hx\n",
				(unsigned long)i, (uint16_t)expected_response[i], (uint16_t)response[i]);
			return -EINVAL;
		}
	}

	return ret;
}
