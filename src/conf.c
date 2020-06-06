/*!
 * @file conf.c
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
 * @brief Proxy configuration implementation
 */

#include "conf.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Maximum size for a single line in a configuration file
#define CONF_LINE_SIZE 512

/*!
 * @brief Parse a single null- or newline-terminated line into the configuration
 *
 * @param[in] line null- or newline-terminated line to parse
 * @param[in,out] conf Target configuration instance
 * @param[in,out] log Log handle for reporting logging events
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int conf_parse_line(const char *line, struct proxy_conf *conf, struct log_handle *log);

/*!
 * @brief Parse a key/value pair into the configuration
 *
 * @param[in] key Configuration key
 * @param[in] key_len Length of key in characters
 * @param[in] val Configuration value
 * @param[in] val_len Length of val in characters
 * @param[in,out] conf Target configuration instance
 * @param[in,out] log Log handle for reporting logging events
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int conf_parse_pair(const char *key, size_t key_len, const char *val, size_t val_len, struct proxy_conf *conf, struct log_handle *log);

/*!
 * @brief Parse a configuration file
 *
 * @param[in] stream File stream to parse
 * @param[in,out] conf Target configuration instance
 * @param[in,out] log Log handle for reporting logging events
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int conf_parse_stream(FILE *stream, struct proxy_conf *conf, struct log_handle *log);

/*!
 * @brief Cross-platform implementation of the getline function
 *
 * @param[in,out] lineptr Buffer to store read text into
 * @param[in,out] n Number of bytes in buffer
 * @param[in] stream File stream to read from
 *
 * @returns 0 on success, negative ERRNO value on failure
 */
static int conf_readline(char **lineptr, size_t *n, FILE *stream);

static int conf_readline(char **lineptr, size_t *n, FILE *stream)
{
	size_t so_far = 0;
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

	return (int)so_far;
}

static int conf_parse_line(const char *line, struct proxy_conf *conf, struct log_handle *log)
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
	for (; key[key_len] != '='; key_len++)
	{
		// If the line doesn't have '=', ignore it
		if (key[key_len] == '\0')
		{
			return 0;
		}
	};

	val = &key[key_len + 1];

	// Backtrack if there is a space
	for (; key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t'); key_len--);

	// Find the beginning of the key
	for (; *val == ' ' || *val == '\t'; val++);

	for (; val[val_len] != '\0'; val_len++);

	// Backtrack if there is a space
	for (; val_len > 0 && (val[val_len - 1] == ' ' || val[val_len - 1] == '\t' || val[val_len - 1] == '\n' || val[val_len - 1] == '\r'); val_len--);

	return conf_parse_pair(key, key_len, val, val_len, conf, log);
}

static int conf_parse_pair(const char *key, size_t key_len, const char *val, size_t val_len, struct proxy_conf *conf, struct log_handle *log)
{
	char dummy[2];

	switch (key_len)
	{
	case 4:
		if (strncmp(key, "Port", key_len) == 0)
		{
			if (sscanf(val, "%hu%1s", &conf->port, dummy) != 1)
			{
				log_printf(log, LOG_LEVEL_ERROR, "Invalid configuration value for 'Port': '%.*s'\n", (int)val_len, val);

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

				log_printf(log, LOG_LEVEL_ERROR, "Error: Missing password\n");

				return -EINVAL;
			}
		}

		break;
	case 11:
		if (strncmp(key, "BindAddress", key_len) == 0)
		{
			if (conf->bind_addr != NULL)
			{
				free(conf->bind_addr);
			}

			if (val_len == 0)
			{
				conf->bind_addr = NULL;
				break;
			}

			conf->bind_addr = malloc(val_len + 1);
			if (conf->bind_addr == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->bind_addr, val, val_len);
			conf->bind_addr[val_len] = '\0';
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
		else if (strncmp(key, "RegistrationName", key_len) == 0)
		{
			if (conf->reg_name != NULL)
			{
				free(conf->reg_name);
			}

			if (val_len == 0)
			{
				conf->reg_name = NULL;
				break;
			}

			conf->reg_name = malloc(val_len + 1);
			if (conf->reg_name == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->reg_name, val, val_len);
			conf->reg_name[val_len] = '\0';
		}

		break;
	case 19:
		if (strncmp(key, "ExternalBindAddress", key_len) == 0)
		{
			if (conf->bind_addr_ext != NULL)
			{
				free(conf->bind_addr_ext);
			}

			if (val_len == 0)
			{
				conf->bind_addr_ext = NULL;
				break;
			}

			conf->bind_addr_ext = malloc(val_len + 1);
			if (conf->bind_addr_ext == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->bind_addr_ext, val, val_len);
			conf->bind_addr_ext[val_len] = '\0';
		}
		else if (strncmp(key, "RegistrationComment", key_len) == 0)
		{
			if (conf->reg_comment != NULL)
			{
				free(conf->reg_comment);
			}

			if (val_len == 0)
			{
				conf->reg_comment = NULL;
				break;
			}

			conf->reg_comment = malloc(val_len + 1);
			if (conf->reg_comment == NULL)
			{
				return -ENOMEM;
			}

			memcpy(conf->reg_comment, val, val_len);
			conf->reg_comment[val_len] = '\0';
		}

		break;
	case 31:
		if (strncmp(key, "AdditionalExternalBindAddresses", key_len) == 0)
		{
			size_t i, j;

			if (conf->bind_addr_ext_add != NULL)
			{
				free(conf->bind_addr_ext_add);
			}

			if (val_len == 0)
			{
				conf->bind_addr_ext_add = NULL;
				conf->bind_addr_ext_add_len = 0;
				break;
			}

			for (i = 1; i < val_len; i++)
			{
				if (val[i] == ',')
				{
					conf->bind_addr_ext_add_len++;
				}
			}

			conf->bind_addr_ext_add_len++;

			conf->bind_addr_ext_add = malloc(conf->bind_addr_ext_add_len * sizeof(char *));
			if (conf->bind_addr_ext_add == NULL)
			{
				return -ENOMEM;
			}

			memset(conf->bind_addr_ext_add, 0x0, conf->bind_addr_ext_add_len * sizeof(char *));

			for (i = 0, j = 0; i < conf->bind_addr_ext_add_len; i++)
			{
				size_t k = j;

				for (; j < val_len && val[j] != ','; j++);

				conf->bind_addr_ext_add[i] = malloc(j - k + 1);
				if (conf->bind_addr_ext_add[i] == NULL)
				{
					goto bind_addr_ext_add_exit;
				}

				memcpy(conf->bind_addr_ext_add[i], &val[k], j - k);
				conf->bind_addr_ext_add[i][j - k] = '\0';

				j++;
			}

			break;

		bind_addr_ext_add_exit:
			for (i = 0; i < conf->bind_addr_ext_add_len; i++)
			{
				if (conf->bind_addr_ext_add[i] != NULL)
				{
					free(conf->bind_addr_ext_add[i]);
				}
			}

			free(conf->bind_addr_ext_add);
			conf->bind_addr_ext_add = NULL;
			conf->bind_addr_ext_add_len = 0;

			return -ENOMEM;
		}
	}

	return 0;
}

static int conf_parse_stream(FILE *stream, struct proxy_conf *conf, struct log_handle *log)
{
	char *line = NULL;
	size_t line_len = 0;
	int ret = 0;

	while (conf_readline(&line, &line_len, stream) > 0)
	{
		ret = conf_parse_line(line, conf, log);

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

int conf_init(struct proxy_conf *conf)
{
	conf->password = NULL;
	conf->port = 8100;

	return 0;
}

void conf_free(struct proxy_conf *conf)
{
	int i;

	if (conf->bind_addr_ext_add != NULL)
	{
		for (i = 0; i < conf->bind_addr_ext_add_len; i++)
		{
			free(conf->bind_addr_ext_add[i]);
		}

		conf->bind_addr_ext_add_len = 0;

		free(conf->bind_addr_ext_add);
		conf->bind_addr_ext_add = NULL;
	}

	if (conf->bind_addr != NULL)
	{
		free(conf->bind_addr);
		conf->bind_addr = NULL;
	}

	if (conf->bind_addr_ext != NULL)
	{
		free(conf->bind_addr_ext);
		conf->bind_addr_ext = NULL;
	}

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

	if (conf->reg_name != NULL)
	{
		free(conf->reg_name);
		conf->reg_name = NULL;
	}

	if (conf->reg_comment != NULL)
	{
		free(conf->reg_comment);
		conf->reg_comment = NULL;
	}
}

int conf_parse_file(const char *file, struct proxy_conf *conf, struct log_handle *log)
{
	FILE *stream;
	int ret;

	log_printf(log, LOG_LEVEL_DEBUG, "Loading proxy config from '%s'\n", file);

	if ((stream = fopen(file, "r")) == NULL)
	{
		return -errno;
	}

	ret = conf_parse_stream(stream, conf, log);

	fclose(stream);

	return ret;
}
