/*!
 * @file conf.c
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

#include "conf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int conf_readline(char **lineptr, size_t *n, FILE *stream)
{
	int so_far = 0;
	char temp;
	char *temp_ptr;

	if (*lineptr == NULL)
	{
		*lineptr = malloc(128);
		if (*lineptr == NULL)
		{
			return -ENOMEM;
		}

		*n = 128;
	}
	else if (*n == 0)
	{
		temp_ptr = realloc(*lineptr, 128);
		if (temp_ptr == NULL)
		{
			return -ENOMEM;
		}

		*lineptr = temp_ptr;
		*n = 128;
	}

	while ((temp = fgetc(stream)) != EOF)
	{
		// Need more space?
		if (so_far + 1 >= *n)
		{
			temp_ptr = realloc(*lineptr, *n + 128);
			if (temp_ptr == NULL)
			{
				return -ENOMEM;
			}

			*lineptr = temp_ptr;
			*n += 128;
		}

		(*lineptr)[so_far++] = temp;

		if (temp == '\n')
		{
			break;
		}
	}

	(*lineptr)[so_far] = '\0';

	return so_far;
}

int conf_init(struct proxy_conf *conf)
{
	conf->password = NULL;
	conf->port = 8100;

	return 0;
}

void conf_free(struct proxy_conf *conf)
{
	if (conf->calls_allowed != NULL)
	{
		free(conf->calls_allowed);
		conf->calls_allowed = NULL;
	}

	if (conf->calls_denied != NULL)
	{
		free(conf->calls_denied);
		conf->calls_denied = NULL;
	}

	if (conf->password != NULL)
	{
		free(conf->password);
		conf->password = NULL;
	}
}

int conf_parse_line(const char *line, struct proxy_conf *conf)
{
	const char *key = line;
	size_t key_len = 0;
	const char *val;
	size_t val_len = 0;

	// Find the beginning of the key
	for (; *key == ' ' || *key == '\t' || *key == '\n' || *key == '\r'; key++);

	// If the line is a comment or empty, ignore it
	if (*key == '#' || *key == '\0' || *key == '=')
	{
		return 0;
	}

	// Find the '='
	for(; key[key_len] != '='; key_len++)
	{
		// If the line doesn't have '=', ignore it
		if (key[key_len] == '\0')
		{
			return 0;
		}
	};

	val = &key[key_len + 1];

	// Backtrack if there is a space
	for(; key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t'); key_len--);

	// Find the beginning of the key
	for (; *val == ' ' && *val == '\t'; val++);

	for(; val[val_len] != '\0'; val_len++);

	// Backtrack if there is a space
	for(; val_len > 0 && (val[val_len - 1] == ' ' || val[val_len - 1] == '\t' || val[val_len - 1] == '\n' || val[val_len - 1] == '\r'); val_len--);

	return conf_parse_pair(key, key_len, val, val_len, conf);
}

int conf_parse_pair(const char *key, size_t key_len, const char *val, size_t val_len, struct proxy_conf *conf)
{
	char dummy;

	switch (key_len)
	{
	case 4:
		if (strncmp(key, "Port", key_len) == 0)
		{
			if (sscanf(val, "%hu%1s", &conf->port, &dummy) != 1)
			{
				fprintf(stderr, "Invalid configuration value for 'Port': '%*s'\n", (int)val_len, val);

				return -EINVAL;
			}
		}
		break;
	case 8:
		if (strncmp(key, "Password", key_len) == 0)
		{
			if (conf->password != NULL)
			{
				free(conf->password);
			}

			conf->password = malloc(val_len + 1);
			if (conf->password == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->password, val, val_len);
			conf->password[val_len] = '\0';

			if (strcmp(conf->password, "notset") == 0)
			{
				free(conf->password);
				conf->password = NULL;

				fprintf(stderr, "Error: Missing password\n");

				return -EINVAL;
			}
		}
		break;
	case 15:
		if (strncmp(key, "CallsignsDenied", key_len) == 0)
		{
			if (conf->calls_denied != NULL)
			{
				free(conf->calls_denied);
			}

			if (val_len == 0)
			{
				conf->calls_denied = NULL;
				break;
			}

			conf->calls_denied = malloc(val_len + 1);
			if (conf->calls_denied == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->calls_denied, val, val_len);
			conf->calls_denied[val_len] = '\0';
		}
		break;
	case 16:
		if (strncmp(key, "CallsignsAllowed", key_len) == 0)
		{
			if (conf->calls_allowed != NULL)
			{
				free(conf->calls_allowed);
			}

			if (val_len == 0)
			{
				conf->calls_allowed = NULL;
				break;
			}

			conf->calls_allowed = malloc(val_len + 1);
			if (conf->calls_allowed == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->calls_allowed, val, val_len);
			conf->calls_allowed[val_len] = '\0';
		}
		break;
	}

	return 0;
}

int conf_parse_stream(FILE *stream, struct proxy_conf *conf)
{
	char *line = NULL;
	size_t line_len = 0;
	int ret = 0;

	while (conf_readline(&line, &line_len, stream) > 0)
	{
		ret = conf_parse_line(line, conf);

		free(line);
		line = NULL;

		if (ret < 0)
		{
			return ret;
		}
	}

	free(line);

	return 0;
}

int conf_parse_file(const char *file, struct proxy_conf *conf)
{
	FILE *stream = fopen(file, "r");
	int ret;

	if (stream == NULL)
	{
		return -errno;
	}

	ret = conf_parse_stream(stream, conf);

	fclose(stream);

	return ret;
}
