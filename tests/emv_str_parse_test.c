/**
 * @file emv_str_parse_test.c
 * @brief Unit tests for string to EMV format conversion functions
 *
 * Copyright (c) 2021 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_strings.h"

#include <stdio.h>
#include <string.h>

// For debug output
#include "../tools/print_helpers.h"

int main(void)
{
	int r;
	uint8_t buf2[2];
	uint8_t buf3[3];
	uint8_t buf4[4];

	printf("\nTesting parsing of \"123456\" as format \"cn\" into 2-byte buffer...\n");
	memset(buf2, 0x00, sizeof(buf2));
	r = emv_str_to_format_cn("123456", buf2, sizeof(buf2));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf2, (uint8_t[]){ 0x12, 0x34 }, sizeof(buf2)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf2", buf2, sizeof(buf2));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456\" as format \"cn\" into 3-byte buffer...\n");
	memset(buf3, 0x00, sizeof(buf3));
	r = emv_str_to_format_cn("123456", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x12, 0x34, 0x56 }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456\" as format \"cn\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_cn("123456", buf4, sizeof(buf4));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf4, (uint8_t[]){ 0x12, 0x34, 0x56, 0xFF }, sizeof(buf4)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf4", buf4, sizeof(buf4));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12345\" as format \"cn\" into 3-byte buffer...\n");
	memset(buf3, 0x00, sizeof(buf3));
	r = emv_str_to_format_cn("12345", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x12, 0x34, 0x5F }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12345\" as format \"cn\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_cn("12345", buf4, sizeof(buf4));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf4, (uint8_t[]){ 0x12, 0x34, 0x5F, 0xFF }, sizeof(buf4)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf4", buf4, sizeof(buf4));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12B456\" as format \"cn\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_cn("12B456", buf4, sizeof(buf4));
	if (r == 0) {
		fprintf(stderr, "emv_str_to_format_cn() unexpectedly didn't fail\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123F56\" as format \"cn\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_cn("123F56", buf4, sizeof(buf4));
	if (r == 0) {
		fprintf(stderr, "emv_str_to_format_cn() unexpectedly didn't fail\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456A\" as format \"cn\" into 3-byte buffer...\n");
	memset(buf3, 0x00, sizeof(buf3));
	// NOTE: parser stops when buffer is full and won't reach invalid digit
	r = emv_str_to_format_cn("123456A", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_cn() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x12, 0x34, 0x56 }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_cn() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456\" as format \"n\" into 2-byte buffer...\n");
	memset(buf2, 0xFF, sizeof(buf2));
	r = emv_str_to_format_n("123456", buf2, sizeof(buf2));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf2, (uint8_t[]){ 0x34, 0x56 }, sizeof(buf2)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf2", buf2, sizeof(buf2));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456\" as format \"n\" into 3-byte buffer...\n");
	memset(buf3, 0xFF, sizeof(buf3));
	r = emv_str_to_format_n("123456", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x12, 0x34, 0x56 }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123456\" as format \"n\" into 4-byte buffer...\n");
	memset(buf4, 0xFF, sizeof(buf4));
	r = emv_str_to_format_n("123456", buf4, sizeof(buf4));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf4, (uint8_t[]){ 0x00, 0x12, 0x34, 0x56 }, sizeof(buf4)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf4", buf4, sizeof(buf4));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12345\" as format \"n\" into 3-byte buffer...\n");
	memset(buf3, 0xFF, sizeof(buf3));
	r = emv_str_to_format_n("12345", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x01, 0x23, 0x45 }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12345\" as format \"n\" into 4-byte buffer...\n");
	memset(buf4, 0xFF, sizeof(buf4));
	r = emv_str_to_format_n("12345", buf4, sizeof(buf4));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf4, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45 }, sizeof(buf4)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf4", buf4, sizeof(buf4));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"12B456\" as format \"n\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_n("12B456", buf4, sizeof(buf4));
	if (r == 0) {
		fprintf(stderr, "emv_str_to_format_n() unexpectedly didn't fail\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"123F56\" as format \"n\" into 4-byte buffer...\n");
	memset(buf4, 0x00, sizeof(buf4));
	r = emv_str_to_format_n("123F56", buf4, sizeof(buf4));
	if (r == 0) {
		fprintf(stderr, "emv_str_to_format_n() unexpectedly didn't fail\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting parsing of \"A123456\" as format \"n\" into 3-byte buffer...\n");
	memset(buf3, 0x00, sizeof(buf3));
	// NOTE: parser stops when buffer is full and won't reach invalid digit
	r = emv_str_to_format_n("A123456", buf3, sizeof(buf3));
	if (r) {
		fprintf(stderr, "emv_str_to_format_n() failed; r=%d\n", r);
		return 1;
	}
	if (memcmp(buf3, (uint8_t[]){ 0x12, 0x34, 0x56 }, sizeof(buf3)) != 0) {
		fprintf(stderr, "emv_str_to_format_n() failed; incorrect output buffer\n");
		print_buf("buf3", buf3, sizeof(buf3));
		return 1;
	}
	printf("Success\n");
}
