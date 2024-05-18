/**
 * @file emv_atr_parse_test.c
 * @brief Unit tests for EMV ATR parsing
 *
 * Copyright 2023 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv.h"
#include "emv_debug.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

static const uint8_t basic_atr_t0_test[] = { 0x3B, 0x60, 0x00, 0x00 };
static const uint8_t basic_atr_t1_test[] = { 0x3B, 0xE0, 0x00, 0xFF, 0x81, 0x31, 0x7C, 0x41, 0x92 };
static const uint8_t non_emv_atr[] = { 0x3B, 0xDA, 0x18, 0xFF, 0x81, 0xB1, 0xFE, 0x75, 0x1F, 0x03, 0x00, 0x31, 0xF5, 0x73, 0xC7, 0x8A, 0x40, 0x00, 0x90, 0x00, 0xB0 };
static const uint8_t complex_atr_t0_test[] = { 0x3B, 0xF4, 0x13, 0x00, 0xFF, 0xD0, 0x00, 0x0A, 0x3F, 0x03, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xDC };
static const uint8_t complex_atr_t1_test[] = { 0x3B, 0xF4, 0x13, 0x00, 0xFF, 0x91, 0x01, 0x71, 0x7C, 0x41, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xE6 };

int main(void)
{
	int r;
	uint8_t atr_t0_test[sizeof(complex_atr_t0_test)];
	uint8_t atr_t1_test[sizeof(complex_atr_t1_test)];

	// Enable debug output
	r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_ALL, &print_emv_debug);
	if (r) {
		fprintf(stderr, "emv_debug_init() failed; r=%d\n", r);
		return 1;
	}

	printf("Testing basic ATR for T=0\n");
	r = emv_atr_parse(basic_atr_t0_test, sizeof(basic_atr_t0_test));
	if (r) {
		fprintf(stderr, "emv_atr_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing basic ATR for T=1\n");
	r = emv_atr_parse(basic_atr_t1_test, sizeof(basic_atr_t1_test));
	if (r) {
		fprintf(stderr, "emv_atr_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing non-EMV ATR\n");
	r = emv_atr_parse(non_emv_atr, sizeof(non_emv_atr));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for non-EMV ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0\n");
	r = emv_atr_parse(complex_atr_t0_test, sizeof(complex_atr_t0_test));
	if (r) {
		fprintf(stderr, "emv_atr_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TA1\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[2];
	atr_t0_test[2] = 0x14; // TA1 must be in the range 0x11 to 0x13
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[2];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TC1\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[4];
	atr_t0_test[4] = 0x01; // TC1 must be either 0x00 or 0xFF
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[4];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TD1\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[5];
	atr_t0_test[5] = (atr_t0_test[5] & 0xF0) + 0x02; // TD1 protocol type must be T=0 or T=1
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[5];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TA2\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[6];
	atr_t0_test[6] = 0x10; // TA2 must indicate specific mode, not implicit mode
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[6];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TA2\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[6];
	atr_t0_test[6] = (atr_t0_test[6] & 0xF0) + 0x01; // TA2 protocol must be the same as the first indicated protocol
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[6];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TC2\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[7];
	atr_t0_test[7] = 0x00; // TC2 for T=0 must be 0x0A
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[7];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=0 with invalid TD2\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[8];
	atr_t0_test[8] = (atr_t0_test[8] & 0xF0) + 0x00; // TD2 protocol type must be T=15 if TD1 protocol type was T=0
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[8];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=15 with invalid TA3\n");
	memcpy(atr_t0_test, complex_atr_t0_test, sizeof(atr_t0_test));
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[9];
	atr_t0_test[9] = (atr_t0_test[9] & 0xC0) + 0x08; // Class indicator Y of TA3 for T=15 must be in the range 1 to 7
	atr_t0_test[sizeof(atr_t0_test)-1] ^= atr_t0_test[9];
	r = emv_atr_parse(atr_t0_test, sizeof(atr_t0_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1\n");
	r = emv_atr_parse(complex_atr_t1_test, sizeof(complex_atr_t1_test));
	if (r) {
		fprintf(stderr, "emv_atr_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TA1\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[2];
	atr_t1_test[2] = 0x23; // TA1 must be in the range 0x11 to 0x13
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[2];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TC1\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[4];
	atr_t1_test[4] = 0x01; // TC1 must be either 0x00 or 0xFF
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[4];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TD1\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[5];
	atr_t1_test[5] = (atr_t1_test[5] & 0xF0) + 0x02; // TD1 protocol type must be T=0 or T=1
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[5];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TA2\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[6];
	atr_t1_test[6] = 0x11; // TA2 must indicate specific mode, not implicit mode
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[6];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TA2\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[6];
	atr_t1_test[6] = (atr_t1_test[6] & 0xF0) + 0x00; // TA2 protocol must be the same as the first indicated protocol
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[6];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TD2\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[7];
	atr_t1_test[7] = (atr_t1_test[7] & 0xF0) + 0x00; // TD2 protocol type must be T=1 if TD1 protocol type was T=1
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[7];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TA3\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[8];
	atr_t1_test[8] = 0x0F; // TA3 for T=1 must be in the range 0x10 to 0xFE
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[8];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TB3\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[9];
	atr_t1_test[9] = (atr_t1_test[9] & 0xF0) + 0x06; // TB3 for T=1 CWI must be 5 or less
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[9];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing complex ATR for T=1 with invalid TC3\n");
	memcpy(atr_t1_test, complex_atr_t1_test, sizeof(atr_t1_test));
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[10];
	atr_t1_test[10] = 0x01; // TC for T=1 must be 0x00
	atr_t1_test[sizeof(atr_t1_test)-1] ^= atr_t1_test[10];
	r = emv_atr_parse(atr_t1_test, sizeof(atr_t1_test));
	if (r == 0) {
		fprintf(stderr, "emv_atr_parse() succeeded for invalid ATR\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
