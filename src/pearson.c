/*!
 * @file pearson.c
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
 * @brief Implmentation of a Pearson hashing function
 */

#include "pearson.h"

/*!
 * @brief Hash computation permutation table
 *
 * Chosen by fair dice roll.
 * Guaranteed to be random.
 */
static const uint8_t permutation_table[256] = {
	5,   42,  6,   37,  103, 84,  75,  83,
	219, 54,  116, 223, 192, 239, 163, 154,
	114, 189, 206, 105, 92,  76,  162, 225,
	158, 125, 74,  234, 160, 29,  153, 198,
	132, 34,  93,  86,  161, 159, 181, 66,
	197, 82,  204, 59,  237, 134, 36,  57,
	95,  39,  90,  32,  150, 217, 30,  202,
	174, 89,  38,  216, 117, 52,  169, 232,
	43,  254, 126, 222, 170, 129, 49,  207,
	73,  138, 120, 25,  27,  190, 100, 229,
	172, 67,  199, 210, 214, 218, 62,  18,
	97,  63,  101, 7,   20,  187, 131, 179,
	188, 247, 135, 68,  195, 71,  85,  144,
	107, 241, 182, 231, 98,  60,  245, 91,
	119, 87,  12,  51,  146, 96,  47,  45,
	22,  40,  1,   205, 88,  215, 141, 108,
	139, 23,  133, 78,  61,  21,  72,  226,
	183, 53,  106, 173, 224, 31,  44,  80,
	104, 167, 145, 81,  46,  10,  244, 193,
	246, 55,  112, 19,  58,  176, 221, 255,
	209, 196, 147, 14,  127, 79,  111, 157,
	249, 26,  177, 168, 212, 69,  13,  115,
	238, 227, 180, 33,  230, 109, 228, 121,
	142, 156, 28,  64,  110, 118, 165, 184,
	3,   149, 128, 203, 220, 186, 50,  213,
	171, 201, 164, 242, 191, 11,  152, 178,
	252, 194, 123, 48,  2,   130, 24,  124,
	113, 248, 251, 137, 253, 208, 99,  136,
	166, 0,   65,  4,   185, 15,  250, 235,
	16,  9,   200, 211, 35,  175, 70,  155,
	243, 240, 143, 122, 236, 233, 8,   148,
	102, 56,  77,  41,  94,  140, 17,  151,
};

uint8_t pearson_get(const uint8_t *data, size_t data_len)
{
	uint8_t ret = data_len % 256;

	if (data_len == 0)
		return 0;

	do {
		ret = permutation_table[ret ^ *data++];
	} while (--data_len > 0);

	return ret;
}
