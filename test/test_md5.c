/*!
 * @file test_md5.c
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
 * @brief Tests related to MD5 generation
 */

#include "digest.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*!
 * @brief Basic test of MD5 generation
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Basic test of MD5 generation
 */
static int test_md5_basic(void);

/*!
 * @brief Main entry point for MD5 tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

int main(void)
{
	int ret = 0;

	ret |= test_md5_basic();

	return ret;
}

static int test_md5_basic(void)
{
	const char md5_challenge[] = "thequickbrownfox";
	const uint8_t md5_control[16] = { 0x30, 0x8f, 0xb7, 0x6d, 0xc4, 0xd7, 0x30, 0x36, 0x0e, 0xe3, 0x39, 0x32, 0xd2, 0xfb, 0x10, 0x56 };
	const char md5_control_str[33] = "308fb76dc4d730360ee33932d2fb1056";
	uint8_t md5_result[16] = { 0x00 };
	char md5_result_str[33] = "";

	digest_get((uint8_t *)md5_challenge, (unsigned int)strlen(md5_challenge), md5_result);

	digest_to_str(md5_control, md5_result_str);

	if (strcmp(md5_control_str, md5_result_str) != 0)
	{
		fprintf(stderr, "Error: digest_to_str mismatch. Expected 0x%s Got: 0x%s\n", md5_control_str, md5_result_str);
		return -EINVAL;
	}

	digest_to_str(md5_result, md5_result_str);

	if (strcmp(md5_control_str, md5_result_str) != 0)
	{
		fprintf(stderr, "Error: digest_get mismatch. Expected: 0x%s Got: 0x%s\n", md5_control_str, md5_result_str);
		return -EINVAL;
	}

	return 0;
}
