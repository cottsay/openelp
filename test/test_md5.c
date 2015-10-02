#include "digest.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char md5_challenge[] = "thequickbrownfox";
	const uint8_t md5_control[16] = { 0x30, 0x8f, 0xb7, 0x6d, 0xc4, 0xd7, 0x30, 0x36, 0x0e, 0xe3, 0x39, 0x32, 0xd2, 0xfb, 0x10, 0x56 };
	const char md5_control_str[33] = "308fb76dc4d730360ee33932d2fb1056";
	uint8_t md5_result[16] = { 0x00 };
	char md5_result_str[33] = "";
	int ret = 0;

	digest_get((uint8_t *)md5_challenge, strlen(md5_challenge), md5_result);

	ret = digest_to_str(md5_control, md5_result_str);
	if (ret != 0)
	{
		fprintf(stderr, "Error: digest_to_str returned %d\n", ret);
		return ret;
	}

	if (strcmp(md5_control_str, md5_result_str) != 0)
	{
		fprintf(stderr, "Error: digest_to_str mismatch. Expected 0x%s Got: 0x%s\n", md5_control_str, md5_result_str);
		return -EINVAL;
	}

	ret = digest_to_str(md5_result, md5_result_str);
	if (ret != 0)
	{
		fprintf(stderr, "Error: digest_to_str returned %d\n", ret);
		return ret;
	}

	if (strcmp(md5_control_str, md5_result_str) != 0)
	{
		fprintf(stderr, "Error: digest_get mismatch. Expected: 0x%s Got: 0x%s\n", md5_control_str, md5_result_str);
		return -EINVAL;
	}

	return 0;
}
