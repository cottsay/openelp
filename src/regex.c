/*!
 * @file regex.c
 *
 * @section LICENSE
 *
 * Copyright &copy; 2016, Scott K Logan
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

#include <pcre2.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct regex_priv
{
	pcre2_code *re;
};

int regex_compile(struct regex_handle *re, const char *pattern)
{
	struct regex_priv *priv = (struct regex_priv *)re->priv;
	int errorcode;
	PCRE2_SIZE erroroffset;
	int ret;

	if (priv->re != NULL)
	{
		pcre2_code_free(priv->re);
	}

	priv->re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
	if (priv->re == NULL)
	{
		ret =-EINVAL;
		goto regex_compile_exit;
	}

	return 0;

regex_compile_exit:
	if (priv->re != NULL)
	{
		pcre2_code_free(priv->re);
		priv->re = NULL;
	}

	return ret;
}

void regex_free(struct regex_handle *re)
{
	if (re->priv != NULL)
	{
		struct regex_priv *priv = (struct regex_priv *)re->priv;

		if (priv->re != NULL)
		{
			pcre2_code_free(priv->re);
		}

		free(re->priv);
		re->priv = NULL;
	}
}

int regex_init(struct regex_handle *re)
{
	if (re->priv == NULL)
	{
		re->priv = malloc(sizeof(struct regex_priv));
	}

	if (re->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(re->priv, 0x0, sizeof(struct regex_priv));

	return 0;
}

int regex_is_match(struct regex_handle *re, const char *subject)
{
	struct regex_priv *priv = (struct regex_priv *)re->priv;
	PCRE2_SPTR sub = (PCRE2_SPTR)subject;
	size_t sub_len = strlen(subject);
	pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(priv->re, NULL);
	int ret;

	if (match_data == NULL)
	{
		return -EINVAL;
	}

	ret = pcre2_match(priv->re, sub, sub_len, 0, 0, match_data, NULL);

	pcre2_match_data_free(match_data);

	if (ret < 0)
	{
		if (ret == PCRE2_ERROR_NOMATCH)
		{
			return 0;
		}
		else
		{
			return -EINVAL;
		}
	}
	else
	{
		return 1;
	}
}
