/*!
 * @file test_regex.c
 *
 * @section LICENSE
 *
 * Copyright &copy; 2015, Scott K Logan
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of OpenELP nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
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
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan <logans@cottsay.net>
 */

#include "regex.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int try_match(const char *pattern, const char *subject)
{
	struct regex_handle re;
	int ret;

	memset(&re, 0x0, sizeof(struct regex_handle));

	ret = regex_init(&re);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to initialize regex (%d): %s\n", -ret, strerror(-ret));
		return ret;
	}

	ret = regex_compile(&re, pattern);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to compile regex '%s' (%d): %s\n", pattern, -ret, strerror(-ret));
		goto try_match_exit;
	}

	ret = regex_is_match(&re, subject);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to match '%s' against '%s' (%d): %s\n", subject, pattern, -ret, strerror(-ret));
	}

try_match_exit:
	regex_free(&re);

	return ret;
}

static int assert_match(const char *pattern, const char *subject, int expected)
{
	int ret = try_match(pattern, subject);

	if (ret != expected)
	{
		switch (ret)
		{
		case 1:
			fprintf(stderr, "Regex '%s' was not expected to match subject '%s', but did\n", pattern, subject);
			break;
		case 0:
			fprintf(stderr, "Regex '%s' was expected to match subject '%s', but did not\n", pattern, subject);
			break;
		default:
			fprintf(stderr, "Failure while attempting to match regex '%s' to subject '%s' (%d): %s\n", pattern, subject, -ret, strerror(-ret));
			break;
		}

		return -EINVAL;
	}

	return 0;
}

int main(void)
{
	int ret = 0;

	ret |= assert_match(".*", "KM0H", 1);

	ret |= assert_match(".*", "", 1);

	ret |= assert_match("asdf", "KM0H", 0);

	ret |= assert_match("KM0H", "KM0H", 1);

	ret |= assert_match("KM0H|KD0JLT", "KM0H", 1);

	ret |= assert_match("KM0H|KD0JLT", "KD0JLT", 1);

	ret |= assert_match("KM0H|KD0JLT", "K", 0);

	ret |= assert_match("KM0H|KD0JLT", "KKM0H", 1);

	ret |= assert_match("^(KM0H|KD0JLT)$", "KKM0H", 0);

	ret |= assert_match("^(KM0H|KD0JLT)$", "KM0H", 1);

	return ret;
}
