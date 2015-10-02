#include "openelp/openelp.h"

#include <errno.h>
#include <stdio.h>

int test_proxy_password_response(void)
{
	const uint32_t nonce = 0x4d3b6d47;
	const char password[] = "asdf1234";
	const uint8_t expected_response[16] = { 0x0c, 0x0b, 0xb9, 0x83, 0x5f, 0x31, 0x95, 0x53, 0x10, 0x4b, 0xf9, 0x10, 0xfb, 0x72, 0x45, 0xec };
	uint8_t response[16] = { 0x00 };
	int ret;
	size_t i;

	ret = get_password_response(nonce, password, response);
	if (ret != 0)
	{
		fprintf(stderr, "Error: get_password_response returned %d\n", ret);
		return ret;
	}

	for (i = 0; i < 16; i++)
	{
		if (expected_response[i] != response[i])
		{
			fprintf(stderr, "Error: Password response value missmatch at index %zu. Expected 0x%02hhx, got 0x%02hhx\n", i, expected_response[i], response[i]);
			return -EINVAL;
		}
	}

	return ret;
}

int main(void)
{
	int ret;

	ret = proxy_init(NULL);
	if (ret != 0)
	{
		fprintf(stderr, "Error: proxy_init returned %d\n", ret);
		goto main_exit;
	}

	ret |= test_proxy_password_response();

main_exit:
	return ret;
}
