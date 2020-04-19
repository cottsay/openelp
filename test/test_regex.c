/*!
 * @file test_regex.c
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
 * @brief tests related to regular expressions
 */

#include "regex.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*!
 * @brief Perform a regular expression matching test
 *
 * @param[in] pattern Regular expression pattern
 * @param[in] subject String to match pattern against
 * @param[in] expected Expected return value of match
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int assert_match(const char *pattern, const char *subject, int expected);

/*!
 * @brief Main entry point for MD5 tests
 *
 * @returns 0 on success, non-zero value on failure
 */
int main(void);

/*!
 * @brief Test regular expression catch-all case
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression catch-all case
 */
static int test_regex_catchall(void);

/*!
 * @brief Test regular expression catch-all case with empty string
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression catch-all case with empty string
 */
static int test_regex_catchall_empty(void);

/*!
 * @brief Test regular expression exact match case
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression exact match case
 */
static int test_regex_exact_match(void);

/*!
 * @brief Test regular expression negative match case
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression negative match case
 */
static int test_regex_no_match(void);

/*!
 * @brief Test regular expression OR matching exactly
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching exactly
 */
static int test_regex_or_exact(void);

/*!
 * @brief Test regular expression OR matching left side
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching left side
 */
static int test_regex_or_first(void);

/*!
 * @brief Test regular expression OR matching right side
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching right side
 */
static int test_regex_or_second(void);

/*!
 * @brief Test regular expression OR matching to a substring
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching to a substring
 */
static int test_regex_or_substring(void);

/*!
 * @brief Test regular expression OR matching to a superstring
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching to a superstring
 */
static int test_regex_or_superstring(void);

/*!
 * @brief Test regular expression OR matching to a superstring exactly
 *
 * @returns 0 on success, negative ERRNO value on failure
 *
 * @test Test regular expression OR matching to a superstring exactly
 */
static int test_regex_or_superstring_exact(void);

/*!
 * @brief Try to perform a regular expression match
 *
 * @param[in] pattern Regular expression pattern
 * @param[in] subject String to match pattern against
 *
 * @returns 1 if matched, 0 if no match, negative ERRNO value on failure
 */
static int try_match(const char *pattern, const char *subject);

static int assert_match(const char *pattern, const char *subject, int expected)
{
	int ret = try_match(pattern, subject);

	if (ret != expected)
	{
		switch (ret)
		{
		case 1:
			fprintf(stderr, "Error: Regex '%s' was not expected to match subject '%s', but did\n", pattern, subject);
			ret = -EINVAL;
			break;
		case 0:
			fprintf(stderr, "Error: Regex '%s' was expected to match subject '%s', but did not\n", pattern, subject);
			ret = -EINVAL;
			break;
		default:
			fprintf(stderr, "Error: Failure while attempting to match regex '%s' to subject '%s' (%d): %s\n", pattern, subject, -ret, strerror(-ret));
			break;
		}

		return ret;
	}

	return 0;
}

int main(void)
{
	int ret = 0;

	ret |= test_regex_catchall();
	ret |= test_regex_catchall_empty();
	ret |= test_regex_exact_match();
	ret |= test_regex_no_match();
	ret |= test_regex_or_exact();
	ret |= test_regex_or_first();
	ret |= test_regex_or_second();
	ret |= test_regex_or_substring();
	ret |= test_regex_or_superstring();
	ret |= test_regex_or_superstring_exact();

	return ret;
}

static int test_regex_catchall(void)
{
	return assert_match(".*", "KM0H", 1);
}

static int test_regex_catchall_empty(void)
{
	return assert_match(".*", "", 1);
}

static int test_regex_exact_match(void)
{
	return assert_match("KM0H", "KM0H", 1);
}

static int test_regex_no_match(void)
{
	return assert_match("asdf", "KM0H", 0);
}

static int test_regex_or_exact(void)
{
	return assert_match("^(KM0H|KD0JLT)$", "KM0H", 1);
}

static int test_regex_or_first(void)
{
	return assert_match("KM0H|KD0JLT", "KM0H", 1);
}

static int test_regex_or_second(void)
{
	return assert_match("KM0H|KD0JLT", "KD0JLT", 1);
}

static int test_regex_or_substring(void)
{
	return assert_match("KM0H|KD0JLT", "K", 0);
}

static int test_regex_or_superstring(void)
{
	return assert_match("KM0H|KD0JLT", "KKM0H", 1);
}

static int test_regex_or_superstring_exact(void)
{
	return assert_match("^(KM0H|KD0JLT)$", "KKM0H", 0);
}

static int try_match(const char *pattern, const char *subject)
{
	struct regex_handle re;
	int ret;

	memset(&re, 0x0, sizeof(struct regex_handle));

	ret = regex_init(&re);
	if (ret < 0)
	{
		fprintf(stderr, "Error: Failed to initialize regex (%d): %s\n", -ret, strerror(-ret));
		return ret;
	}

	ret = regex_compile(&re, pattern);
	if (ret < 0)
	{
		fprintf(stderr, "Error: Failed to compile regex '%s' (%d): %s\n", pattern, -ret, strerror(-ret));
		goto try_match_exit;
	}

	ret = regex_is_match(&re, subject);
	if (ret < 0)
	{
		fprintf(stderr, "Error: Failed to match '%s' against '%s' (%d): %s\n", subject, pattern, -ret, strerror(-ret));
	}

try_match_exit:
	regex_free(&re);

	return ret;
}
