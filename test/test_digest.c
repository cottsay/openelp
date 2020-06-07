/*!
 * @file test_digest.c
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
 * @brief Tests related to digest utilities
 */

#include "digest.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*!
 * @brief Test of the round-trip digest conversion routines
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test of the round-trip digest conversion routines
 */
static int test_digest_conversion(void);

/*!
 * @brief Main entry point for digest tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

int main(void)
{
	int ret = 0;

	ret |= test_digest_conversion();

	return ret;
}

static int test_digest_conversion(void)
{
	const uint32_t nonce = 0x4d3b6d47;
	const char expected_response[9] = "4d3b6d47";
	char response[9] = { 0x00 };
	uint32_t round_trip;
	int ret = 0;

	digest_to_hex32(nonce, response);
	if (strcmp(expected_response, response) != 0)
	{
		fprintf(stderr, "Error: Conversion to hex32 failed. Expected: '%s'. Got: '%s'.\n", expected_response, response);
		ret |= -EINVAL;
	}

	round_trip = hex32_to_digest(expected_response);
	if (nonce != round_trip)
	{
		fprintf(stderr, "Error: Conversion from hex32 failed. Expected: 0x%08X. Got: 0x%08X.\n", nonce, round_trip);
		ret |= -EINVAL;
	}

	return ret;
}
