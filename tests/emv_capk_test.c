/**
 * @file emv_capk_test.c
 * @brief Unit tests for CAPK helper functions
 *
 * Copyright 2025-2026 Leon Lynch
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

#include "emv_capk.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

static const unsigned int verify_capk_count = 30;

// Visa 1408-bit test CAPK [92]
static const uint8_t visa_92_rid[] = { 0xA0, 0x00, 0x00, 0x00, 0x03 };
static const uint8_t visa_92_modulus[] = {
	0x99, 0x6A, 0xF5, 0x6F, 0x56, 0x91, 0x87, 0xD0, 0x92, 0x93, 0xC1, 0x48, 0x10, 0x45, 0x0E, 0xD8,
	0xEE, 0x33, 0x57, 0x39, 0x7B, 0x18, 0xA2, 0x45, 0x8E, 0xFA, 0xA9, 0x2D, 0xA3, 0xB6, 0xDF, 0x65,
	0x14, 0xEC, 0x06, 0x01, 0x95, 0x31, 0x8F, 0xD4, 0x3B, 0xE9, 0xB8, 0xF0, 0xCC, 0x66, 0x9E, 0x3F,
	0x84, 0x40, 0x57, 0xCB, 0xDD, 0xF8, 0xBD, 0xA1, 0x91, 0xBB, 0x64, 0x47, 0x3B, 0xC8, 0xDC, 0x9A,
	0x73, 0x0D, 0xB8, 0xF6, 0xB4, 0xED, 0xE3, 0x92, 0x41, 0x86, 0xFF, 0xD9, 0xB8, 0xC7, 0x73, 0x57,
	0x89, 0xC2, 0x3A, 0x36, 0xBA, 0x0B, 0x8A, 0xF6, 0x53, 0x72, 0xEB, 0x57, 0xEA, 0x5D, 0x89, 0xE7,
	0xD1, 0x4E, 0x9C, 0x7B, 0x6B, 0x55, 0x74, 0x60, 0xF1, 0x08, 0x85, 0xDA, 0x16, 0xAC, 0x92, 0x3F,
	0x15, 0xAF, 0x37, 0x58, 0xF0, 0xF0, 0x3E, 0xBD, 0x3C, 0x5C, 0x2C, 0x94, 0x9C, 0xBA, 0x30, 0x6D,
	0xB4, 0x4E, 0x6A, 0x2C, 0x07, 0x6C, 0x5F, 0x67, 0xE2, 0x81, 0xD7, 0xEF, 0x56, 0x78, 0x5D, 0xC4,
	0xD7, 0x59, 0x45, 0xE4, 0x91, 0xF0, 0x19, 0x18, 0x80, 0x0A, 0x9E, 0x2D, 0xC6, 0x6F, 0x60, 0x08,
	0x05, 0x66, 0xCE, 0x0D, 0xAF, 0x8D, 0x17, 0xEA, 0xD4, 0x6A, 0xD8, 0xE3, 0x0A, 0x24, 0x7C, 0x9F,
};
static const uint8_t visa_92_exponent[] = { 0x03 };
static const uint8_t visa_92_hash[] = {
	0x42, 0x9C, 0x95, 0x4A, 0x38, 0x59, 0xCE, 0xF9, 0x12, 0x95, 0xF6, 0x63, 0xC9, 0x63, 0xE5, 0x82,
	0xED, 0x6E, 0xB2, 0x53,
};

// Alternative Visa 1408-bit CAPK [92]
// First byte of modulus modified and hash regenerated to test dynamic override of static CAPKs.
static const uint8_t visa_92_alt_modulus[] = {
	0x66, 0x6A, 0xF5, 0x6F, 0x56, 0x91, 0x87, 0xD0, 0x92, 0x93, 0xC1, 0x48, 0x10, 0x45, 0x0E, 0xD8,
	0xEE, 0x33, 0x57, 0x39, 0x7B, 0x18, 0xA2, 0x45, 0x8E, 0xFA, 0xA9, 0x2D, 0xA3, 0xB6, 0xDF, 0x65,
	0x14, 0xEC, 0x06, 0x01, 0x95, 0x31, 0x8F, 0xD4, 0x3B, 0xE9, 0xB8, 0xF0, 0xCC, 0x66, 0x9E, 0x3F,
	0x84, 0x40, 0x57, 0xCB, 0xDD, 0xF8, 0xBD, 0xA1, 0x91, 0xBB, 0x64, 0x47, 0x3B, 0xC8, 0xDC, 0x9A,
	0x73, 0x0D, 0xB8, 0xF6, 0xB4, 0xED, 0xE3, 0x92, 0x41, 0x86, 0xFF, 0xD9, 0xB8, 0xC7, 0x73, 0x57,
	0x89, 0xC2, 0x3A, 0x36, 0xBA, 0x0B, 0x8A, 0xF6, 0x53, 0x72, 0xEB, 0x57, 0xEA, 0x5D, 0x89, 0xE7,
	0xD1, 0x4E, 0x9C, 0x7B, 0x6B, 0x55, 0x74, 0x60, 0xF1, 0x08, 0x85, 0xDA, 0x16, 0xAC, 0x92, 0x3F,
	0x15, 0xAF, 0x37, 0x58, 0xF0, 0xF0, 0x3E, 0xBD, 0x3C, 0x5C, 0x2C, 0x94, 0x9C, 0xBA, 0x30, 0x6D,
	0xB4, 0x4E, 0x6A, 0x2C, 0x07, 0x6C, 0x5F, 0x67, 0xE2, 0x81, 0xD7, 0xEF, 0x56, 0x78, 0x5D, 0xC4,
	0xD7, 0x59, 0x45, 0xE4, 0x91, 0xF0, 0x19, 0x18, 0x80, 0x0A, 0x9E, 0x2D, 0xC6, 0x6F, 0x60, 0x08,
	0x05, 0x66, 0xCE, 0x0D, 0xAF, 0x8D, 0x17, 0xEA, 0xD4, 0x6A, 0xD8, 0xE3, 0x0A, 0x24, 0x7C, 0x9F,
};
static const uint8_t visa_92_alt_hash[] = {
	0x69, 0xC1, 0x4B, 0x10, 0x6D, 0x94, 0x46, 0x2D, 0x3C, 0xDC, 0x64, 0xED, 0x84, 0xFF, 0xC6, 0x53,
	0x47, 0x8E, 0x23, 0x7A,
};

// Amex 1984-bit live CAPK [10]
static const uint8_t amex_10_rid[] = { 0xA0, 0x00, 0x00, 0x00, 0x25 };
static const uint8_t amex_10_modulus[] = {
	0xCF, 0x98, 0xDF, 0xED, 0xB3, 0xD3, 0x72, 0x79, 0x65, 0xEE, 0x77, 0x97, 0x72, 0x33, 0x55, 0xE0,
	0x75, 0x1C, 0x81, 0xD2, 0xD3, 0xDF, 0x4D, 0x18, 0xEB, 0xAB, 0x9F, 0xB9, 0xD4, 0x9F, 0x38, 0xC8,
	0xC4, 0xA8, 0x26, 0xB9, 0x9D, 0xC9, 0xDE, 0xA3, 0xF0, 0x10, 0x43, 0xD4, 0xBF, 0x22, 0xAC, 0x35,
	0x50, 0xE2, 0x96, 0x2A, 0x59, 0x63, 0x9B, 0x13, 0x32, 0x15, 0x64, 0x22, 0xF7, 0x88, 0xB9, 0xC1,
	0x6D, 0x40, 0x13, 0x5E, 0xFD, 0x1B, 0xA9, 0x41, 0x47, 0x75, 0x05, 0x75, 0xE6, 0x36, 0xB6, 0xEB,
	0xC6, 0x18, 0x73, 0x4C, 0x91, 0xC1, 0xD1, 0xBF, 0x3E, 0xDC, 0x2A, 0x46, 0xA4, 0x39, 0x01, 0x66,
	0x8E, 0x0F, 0xFC, 0x13, 0x67, 0x74, 0x08, 0x0E, 0x88, 0x80, 0x44, 0xF6, 0xA1, 0xE6, 0x5D, 0xC9,
	0xAA, 0xA8, 0x92, 0x8D, 0xAC, 0xBE, 0xB0, 0xDB, 0x55, 0xEA, 0x35, 0x14, 0x68, 0x6C, 0x6A, 0x73,
	0x2C, 0xEF, 0x55, 0xEE, 0x27, 0xCF, 0x87, 0x7F, 0x11, 0x06, 0x52, 0x69, 0x4A, 0x0E, 0x34, 0x84,
	0xC8, 0x55, 0xD8, 0x82, 0xAE, 0x19, 0x16, 0x74, 0xE2, 0x5C, 0x29, 0x62, 0x05, 0xBB, 0xB5, 0x99,
	0x45, 0x51, 0x76, 0xFD, 0xD7, 0xBB, 0xC5, 0x49, 0xF2, 0x7B, 0xA5, 0xFE, 0x35, 0x33, 0x6F, 0x7E,
	0x29, 0xE6, 0x8D, 0x78, 0x39, 0x73, 0x19, 0x94, 0x36, 0x63, 0x3C, 0x67, 0xEE, 0x5A, 0x68, 0x0F,
	0x05, 0x16, 0x0E, 0xD1, 0x2D, 0x16, 0x65, 0xEC, 0x83, 0xD1, 0x99, 0x7F, 0x10, 0xFD, 0x05, 0xBB,
	0xDB, 0xF9, 0x43, 0x3E, 0x8F, 0x79, 0x7A, 0xEE, 0x3E, 0x9F, 0x02, 0xA3, 0x42, 0x28, 0xAC, 0xE9,
	0x27, 0xAB, 0xE6, 0x2B, 0x8B, 0x92, 0x81, 0xAD, 0x08, 0xD3, 0xDF, 0x5C, 0x73, 0x79, 0x68, 0x50,
	0x45, 0xD7, 0xBA, 0x5F, 0xCD, 0xE5, 0x86, 0x37,
};
static const uint8_t amex_10_exponent[] = { 0x03 };
static const uint8_t amex_10_hash[] = {
	0xC7, 0x29, 0xCF, 0x2F, 0xD2, 0x62, 0x39, 0x4A, 0xBC, 0x4C, 0xC1, 0x73, 0x50, 0x65, 0x02, 0x44,
	0x6A, 0xA9, 0xB9, 0xFD,
};

static const struct emv_capk_t capk_lookup_tests[] = {
	{
		visa_92_rid,
		0x92,
		EMV_PKEY_HASH_SHA1,
		visa_92_modulus,
		sizeof(visa_92_modulus),
		visa_92_exponent,
		sizeof(visa_92_exponent),
		visa_92_hash,
		sizeof(visa_92_hash),
	},

	{
		amex_10_rid,
		0x10,
		EMV_PKEY_HASH_SHA1,
		amex_10_modulus,
		sizeof(amex_10_modulus),
		amex_10_exponent,
		sizeof(amex_10_exponent),
		amex_10_hash,
		sizeof(amex_10_hash),
	},
};

static int verify_capk(const struct emv_capk_t* capk, const struct emv_capk_t* expected, size_t test_idx)
{
	if (memcmp(capk->rid, expected->rid, EMV_CAPK_RID_LEN) != 0) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		print_buf("rid", capk->rid, EMV_CAPK_RID_LEN);
		print_buf("expected", expected->rid, EMV_CAPK_RID_LEN);
		return 1;
	}
	if (capk->index != expected->index) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		printf("index=0x%02X; expected=0x%02X\n", capk->index, expected->index);
		return 1;
	}
	if (capk->hash_id != expected->hash_id) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		printf("hash_id=0x%02X; expected=0x%02X\n", capk->hash_id, expected->hash_id);
		return 1;
	}
	if (capk->modulus_len != expected->modulus_len ||
		memcmp(capk->modulus, expected->modulus, expected->modulus_len) != 0
	) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		print_buf("modulus", capk->modulus, capk->modulus_len);
		print_buf("expected", expected->modulus, expected->modulus_len);
		return 1;
	}
	if (capk->exponent_len != expected->exponent_len ||
		memcmp(capk->exponent, expected->exponent, expected->exponent_len) != 0
	) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		print_buf("exponent", capk->exponent, capk->exponent_len);
		print_buf("expected", expected->exponent, expected->exponent_len);
		return 1;
	}
	if (capk->hash_len != expected->hash_len ||
		memcmp(capk->hash, expected->hash, expected->hash_len) != 0
	) {
		fprintf(stderr, "CAPK lookup test %zu failed\n", test_idx);
		print_buf("hash", capk->hash, capk->hash_len);
		print_buf("expected", expected->hash, expected->hash_len);
		return 1;
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_capk_itr_t itr;
	const struct emv_capk_t* capk;
	unsigned int capk_count;

	printf("Testing static CAPKs...\n");
	r = emv_capk_load_static();
	if (r) {
		fprintf(stderr, "emv_capk_load_static() failed; r=%d\n", r);
		return 1;
	}

	r = emv_capk_itr_init(&itr);
	if (r) {
		fprintf(stderr, "emv_capk_itr_init() failed; r=%d\n", r);
		return 1;
	}
	capk_count = 0;
	while ((capk = emv_capk_itr_next(&itr)) != NULL) {
		++capk_count;
	}
	if (capk_count != verify_capk_count) {
		fprintf(stderr, "Unexpected number of static CAPKs; expected %u; found %u\n",
			verify_capk_count, capk_count);
		return 1;
	}

	for (size_t i = 0; i < sizeof(capk_lookup_tests) / sizeof(capk_lookup_tests[0]); ++i) {
		const struct emv_capk_t* test = &capk_lookup_tests[i];

		printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
			test->rid[0], test->rid[1], test->rid[2], test->rid[3], test->rid[4],
			test->index
		);
		capk = emv_capk_lookup(test->rid, test->index);
		if (!capk) {
			fprintf(stderr, "emv_capk_lookup(%02X%02X%02X%02X%02X, 0x%02X) failed\n",
				test->rid[0], test->rid[1], test->rid[2], test->rid[3], test->rid[4],
				test->index
			);
			return 1;
		}
		if (verify_capk(capk, test, i)) {
			return 1;
		}
	}

	const uint8_t test_rid[5] = { 0xA0, 0x00, 0x00, 0x00, 0x05 };
	const uint8_t test_index = 0xF5;
	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		test_rid[0], test_rid[1], test_rid[2], test_rid[3], test_rid[4],
		test_index
	);
	capk = emv_capk_lookup(test_rid, test_index);
	if (capk) {
		fprintf(stderr, "Unexpected CAPK found: %02X%02X%02X%02X%02X #%02X\n",
			capk->rid[0], capk->rid[1], capk->rid[2], capk->rid[3], capk->rid[4],
			capk->index
		);
		return 1;
	}

	printf("\nTesting dynamic CAPKs...\n");
	emv_capk_clear();
	r = emv_capk_add(&capk_lookup_tests[0]);
	if (r) {
		fprintf(stderr, "emv_capk_add() failed; r=%d\n", r);
		return 1;
	}

	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		visa_92_rid[0], visa_92_rid[1], visa_92_rid[2], visa_92_rid[3], visa_92_rid[4],
		0x92
	);
	capk = emv_capk_lookup(visa_92_rid, 0x92);
	if (!capk) {
		fprintf(stderr, "emv_capk_lookup(%02X%02X%02X%02X%02X, 0x%02X) failed\n",
			visa_92_rid[0], visa_92_rid[1], visa_92_rid[2], visa_92_rid[3], visa_92_rid[4],
			0x92
		);
		return 1;
	}
	if (verify_capk(capk, &capk_lookup_tests[0], 0)) {
		return 1;
	}

	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		amex_10_rid[0], amex_10_rid[1], amex_10_rid[2], amex_10_rid[3], amex_10_rid[4],
		0x10
	);
	capk = emv_capk_lookup(amex_10_rid, 0x10);
	if (capk) {
		fprintf(stderr, "Unexpected CAPK found: %02X%02X%02X%02X%02X #%02X\n",
			capk->rid[0], capk->rid[1], capk->rid[2], capk->rid[3], capk->rid[4],
			capk->index
		);
		return 1;
	}

	r = emv_capk_itr_init(&itr);
	if (r) {
		fprintf(stderr, "emv_capk_itr_init() failed; r=%d\n", r);
		return 1;
	}
	capk_count = 0;
	while ((capk = emv_capk_itr_next(&itr)) != NULL) {
		++capk_count;
	}
	if (capk_count != 1) {
		fprintf(stderr, "Unexpected number of dynamic CAPKs; expected 1; found %u\n",
			capk_count);
		return 1;
	}

	printf("\nTesting invalid CAPK...\n");
	emv_capk_clear();
	uint8_t visa_92_invalid_hash[sizeof(visa_92_hash)];
	memcpy(visa_92_invalid_hash, visa_92_hash, sizeof(visa_92_invalid_hash));
	visa_92_invalid_hash[0] ^= 0xFF; // Corrupt one byte of the hash

	const struct emv_capk_t visa_92_invalid_capk = {
		visa_92_rid,
		0x92,
		EMV_PKEY_HASH_SHA1,
		visa_92_modulus,
		sizeof(visa_92_modulus),
		visa_92_exponent,
		sizeof(visa_92_exponent),
		visa_92_invalid_hash,
		sizeof(visa_92_invalid_hash),
	};
	r = emv_capk_add(&visa_92_invalid_capk);
	if (r == 0) {
		fprintf(stderr, "emv_capk_add() unexpected for invalid CAPK hash\n");
		return 1;
	}

	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		visa_92_rid[0], visa_92_rid[1], visa_92_rid[2], visa_92_rid[3], visa_92_rid[4],
		0x92
	);
	capk = emv_capk_lookup(visa_92_rid, 0x92);
	if (capk) {
		fprintf(stderr, "Unexpected CAPK found: %02X%02X%02X%02X%02X #%02X\n",
			capk->rid[0], capk->rid[1], capk->rid[2], capk->rid[3], capk->rid[4],
			capk->index
		);
		return 1;
	}

	printf("\nTesting CAPK iterator...\n");
	emv_capk_clear();
	r = emv_capk_add(&capk_lookup_tests[0]);
	if (r) {
		fprintf(stderr, "emv_capk_add() failed; r=%d\n", r);
		return 1;
	}
	r = emv_capk_load_static();
	if (r) {
		fprintf(stderr, "emv_capk_load_static() failed; r=%d\n", r);
		return 1;
	}

	r = emv_capk_itr_init(&itr);
	if (r) {
		fprintf(stderr, "emv_capk_itr_init() failed; r=%d\n", r);
		return 1;
	}
	capk_count = 0;
	while ((capk = emv_capk_itr_next(&itr)) != NULL) {
		++capk_count;
	}
	if (capk_count != verify_capk_count + 1) {
		fprintf(stderr, "Unexpected combined CAPK count; expected %u; found %u\n",
			verify_capk_count + 1, capk_count);
		return 1;
	}

	printf("\nTesting CAPK priority...\n");
	emv_capk_clear();
	r = emv_capk_load_static();
	if (r) {
		fprintf(stderr, "emv_capk_load_static() failed; r=%d\n", r);
		return 1;
	}
	const struct emv_capk_t visa_92_alt_capk = {
		visa_92_rid,
		0x92,
		EMV_PKEY_HASH_SHA1,
		visa_92_alt_modulus,
		sizeof(visa_92_alt_modulus),
		visa_92_exponent,
		sizeof(visa_92_exponent),
		visa_92_alt_hash,
		sizeof(visa_92_alt_hash),
	};
	r = emv_capk_add(&visa_92_alt_capk);
	if (r) {
		fprintf(stderr, "emv_capk_add() failed; r=%d\n", r);
		return 1;
	}

	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		visa_92_rid[0], visa_92_rid[1], visa_92_rid[2], visa_92_rid[3], visa_92_rid[4],
		0x92
	);
	capk = emv_capk_lookup(visa_92_rid, 0x92);
	if (!capk) {
		fprintf(stderr, "emv_capk_lookup(%02X%02X%02X%02X%02X, 0x%02X) failed\n",
			visa_92_rid[0], visa_92_rid[1], visa_92_rid[2], visa_92_rid[3], visa_92_rid[4],
			0x92
		);
		return 1;
	}
	if (verify_capk(capk, &visa_92_alt_capk, 0)) {
		return 1;
	}

	printf("Looking up CAPK %02X%02X%02X%02X%02X #%02X\n",
		amex_10_rid[0], amex_10_rid[1], amex_10_rid[2], amex_10_rid[3], amex_10_rid[4],
		0x10
	);
	capk = emv_capk_lookup(amex_10_rid, 0x10);
	if (!capk) {
		fprintf(stderr, "emv_capk_lookup(%02X%02X%02X%02X%02X, 0x%02X) failed\n",
			amex_10_rid[0], amex_10_rid[1], amex_10_rid[2], amex_10_rid[3], amex_10_rid[4],
			0x10
		);
		return 1;
	}
	if (verify_capk(capk, &capk_lookup_tests[1], 1)) {
		return 1;
	}

	printf("\nTesting CAPK clear...\n");
	emv_capk_clear();
	capk = emv_capk_lookup(visa_92_rid, 0x92);
	if (capk) {
		fprintf(stderr, "Unexpected CAPK found: %02X%02X%02X%02X%02X #%02X\n",
			capk->rid[0], capk->rid[1], capk->rid[2], capk->rid[3], capk->rid[4],
			capk->index
		);
		return 1;
	}
	capk = emv_capk_lookup(amex_10_rid, 0x10);
	if (capk) {
		fprintf(stderr, "Unexpected CAPK found: %02X%02X%02X%02X%02X #%02X\n",
			capk->rid[0], capk->rid[1], capk->rid[2], capk->rid[3], capk->rid[4],
			capk->index
		);
		return 1;
	}

	r = emv_capk_itr_init(&itr);
	if (r) {
		fprintf(stderr, "emv_capk_itr_init() failed; r=%d\n", r);
		return 1;
	}
	capk_count = 0;
	while ((capk = emv_capk_itr_next(&itr)) != NULL) {
		++capk_count;
	}
	if (capk_count != 0) {
		fprintf(stderr, "Expected 0 CAPKs after clear; found %u\n", capk_count);
		return 1;
	}

	printf("Success\n");

	return 0;
}
