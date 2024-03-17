/**
 * @file emv_ttl_pcsc_test.c
 * @brief Unit tests for EMV TTL APDU cases using typical PC/SC APDUs
 *
 * Copyright (c) 2021, 2024 Leon Lynch
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

#include "emv_ttl.h"
#include "emv_cardreader_emul.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// PC/SC exchanges for case 1 normal processing
// See EMV Contact Interface Specification v1.0, Annex A1
static const struct xpdu_t test_pcsc_case_1_normal[] = {
	{
		4, (uint8_t[]){ 0x12, 0x34, 0x56, 0x78 },
		2, (uint8_t[]){ 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_1_normal_data[] = { 0x90, 0x00 };

// PC/SC exchanges for case 1 error processing
// See EMV Contact Interface Specification v1.0, Annex A1
static const struct xpdu_t test_pcsc_case_1_error[] = {
	{
		4, (uint8_t[]){ 0x12, 0x34, 0x56, 0x78 },
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_1_error_data[] = { 0x6A, 0x81 };

// PC/SC exchanges for case 2 normal processing
// See EMV Contact Interface Specification v1.0, Annex A2
static const struct xpdu_t test_pcsc_case_2_normal[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x00 }, // READ RECORD 1,1
		2, (uint8_t[]){ 0x6C, 0x1C },
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x1C },
		0x1E, (uint8_t[]){ 0x70, 0x1A, 0x61, 0x18, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50, 0x0A, 0x56, 0x49, 0x53, 0x41, 0x20, 0x44, 0x45, 0x42, 0x49, 0x54, 0x87, 0x01, 0x01, 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_2_normal_data[] = {
	0x70, 0x1A, 0x61, 0x18, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50, 0x0A, 0x56, 0x49, 0x53, 0x41, 0x20, 0x44, 0x45, 0x42, 0x49, 0x54, 0x87, 0x01, 0x01,
};

// PC/SC exchanges for case 2 error processing (early)
// See EMV Contact Interface Specification v1.0, Annex A2
static const struct xpdu_t test_pcsc_case_2_error_early[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x00 }, // READ RECORD 1,1
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_2_error_early_data[] = { 0x6A, 0x81 };

// PC/SC exchanges for case 2 error processing (late)
// See EMV Contact Interface Specification v1.0, Annex A2
static const struct xpdu_t test_pcsc_case_2_error_late[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x00 }, // READ RECORD 1,1
		2, (uint8_t[]){ 0x6C, 0x1C },
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x1C },
		2, (uint8_t[]){ 0x65, 0x81 }, // Memory failure
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_2_error_late_data[] = { 0x65, 0x81 };

// PC/SC exchanges for case 3 normal processing
// See EMV Contact Interface Specification v1.0, Annex A3
static const struct xpdu_t test_pcsc_case_3_normal[] = {
	{
		9, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x04, 0xde, 0xad, 0xbe, 0xef }, // EXTERNAL AUTHENTICATE
		2, (uint8_t[]){ 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_3_normal_data[] = { 0x90, 0x00 };

// PC/SC exchanges for case 3 error processing
// See EMV Contact Interface Specification v1.0, Annex A3
static const struct xpdu_t test_pcsc_case_3_error[] = {
	{
		9, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x04, 0xde, 0xad, 0xbe, 0xef }, // EXTERNAL AUTHENTICATE
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
};
static const uint8_t test_pcsc_case_3_error_data[] = { 0x6A, 0x81 };

// PC/SC exchanges for case 4 normal processing
// See EMV Contact Interface Specification v1.0, Annex A4
static const struct xpdu_t test_pcsc_case_4_normal[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x61, 0x26 },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x26 },
		0x28, (uint8_t[]){ 0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01, 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_normal_data[] = {
	0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01,
};

// PC/SC exchanges for case 4 error processing
// See EMV Contact Interface Specification v1.0, Annex A4
static const struct xpdu_t test_pcsc_case_4_error_early[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x6A, 0x82 }, // File or application not found
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_error_early_data[] = { 0x6A, 0x82 };

// PC/SC exchanges for case 4 error processing (late)
// See EMV Contact Interface Specification v1.0, Annex A4
static const struct xpdu_t test_pcsc_case_4_error_late[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x61, 0x26 },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x26 },
		2, (uint8_t[]){ 0x65, 0x81 }, // Memory failure
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_error_late_data[] = { 0x65, 0x81 };

// PC/SC exchanges for case 2 using both '61' and '6C' procedure bytes
// See EMV Contact Interface Specification v1.0, Annex A5
static const struct xpdu_t test_pcsc_case_2_normal_advanced[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x00 }, // READ RECORD 1,1
		2, (uint8_t[]){ 0x6C, 0x1C },
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x1C },
		2, (uint8_t[]){ 0x61, 0x1C },
	},
	{
		// This deviates slightly from Annex A5 because the TTL has no reason to respond with a smaller Le
		// The ICC can nonetheless force a partial response and indicate the number of remaining bytes
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x1C },
		0x0F, (uint8_t[]){ 0x70, 0x1A, 0x61, 0x18, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x61, 0x0F },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x0F },
		0x11, (uint8_t[]){ 0x50, 0x0A, 0x56, 0x49, 0x53, 0x41, 0x20, 0x44, 0x45, 0x42, 0x49, 0x54, 0x87, 0x01, 0x01, 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_2_normal_advanced_data[] = {
	0x70, 0x1A, 0x61, 0x18, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50, 0x0A, 0x56, 0x49, 0x53, 0x41, 0x20, 0x44, 0x45, 0x42, 0x49, 0x54, 0x87, 0x01, 0x01,
};

// PC/SC exchanges for case 4 (using multiple '61' procedure bytes)
// See EMV Contact Interface Specification v1.0, Annex A6
static const struct xpdu_t test_pcsc_case_4_normal_advanced[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x61, 0x26 },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x26 },
		0x11, (uint8_t[]){ 0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x61, 0x14  },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x14 },
		0x19, (uint8_t[]){ 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01, 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_normal_advanced_data[] = {
	0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01,
};

// PC/SC exchanges for case 4 warning processing ('62' then '6C')
// See EMV Contact Interface Specification v1.0, Annex A7, first example
static const struct xpdu_t test_pcsc_case_4_warning1[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x62, 0x86 }, // No input available from a sensor on the card
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x00 },
		2, (uint8_t[]){ 0x6C, 0x26 },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x26 },
		0x28, (uint8_t[]){ 0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01, 0x90, 0x00 },
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_warning1_data[] = {
	0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01,
};

// PC/SC exchanges for case 4 warning processing ('61' then '62')
// See EMV Contact Interface Specification v1.0, Annex A7, second example
static const struct xpdu_t test_pcsc_case_4_warning2[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT
		2, (uint8_t[]){ 0x61, 0x26 },
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x26 },
		0x28, (uint8_t[]){ 0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01, 0x62, 0x86 }, // No input available from a sensor on the card
	},
	{ 0 }
};
static const uint8_t test_pcsc_case_4_warning2_data[] = {
	0x6F, 0x24, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x12, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x08, 0x65, 0x6E, 0x65, 0x73, 0x66, 0x72, 0x64, 0x65, 0x9F, 0x11, 0x01, 0x01,
};

int main(void)
{
	int r;

	struct emv_ttl_t ttl;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	uint8_t r_apdu[EMV_RAPDU_MAX];
	size_t r_apdu_len = sizeof(r_apdu);
	uint8_t data[EMV_RAPDU_DATA_MAX];
	size_t data_len;
	uint16_t sw1sw2;

	// Enable debug output
	r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_ALL, &print_emv_debug);
	if (r) {
		fprintf(stderr, "emv_debug_init() failed; r=%d\n", r);
		return 1;
	}

	// Test APDU case 1; normal processing
	printf("\nTesting APDU case 1 (PC/SC mode); normal processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_1_normal;
	emul_ctx.xpdu_current = NULL;
	r_apdu_len = sizeof(r_apdu);

	r = emv_ttl_trx(
		&ttl,
		(uint8_t[]){ 0x12, 0x34, 0x56, 0x78 },
		4,
		r_apdu,
		&r_apdu_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (r_apdu_len != 2) {
		fprintf(stderr, "Unexpected R-APDU length %zu\n", r_apdu_len);
		return 1;
	}
	if (memcmp(r_apdu, test_pcsc_case_1_normal_data, r_apdu_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("r_apdu", r_apdu, r_apdu_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 1;
	}
	printf("Success\n");

	// Test APDU case 1; error processing
	printf("\nTesting APDU case 1 (PC/SC mode); error processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_1_error;
	emul_ctx.xpdu_current = NULL;
	r_apdu_len = sizeof(r_apdu);

	r = emv_ttl_trx(
		&ttl,
		(uint8_t[]){ 0x12, 0x34, 0x56, 0x78 },
		4,
		r_apdu,
		&r_apdu_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (r_apdu_len != 2) {
		fprintf(stderr, "Unexpected R-APDU length %zu\n", r_apdu_len);
		return 1;
	}
	if (memcmp(r_apdu, test_pcsc_case_1_error_data, r_apdu_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("r_apdu", r_apdu, r_apdu_len);
		return 1;
	}
	if (sw1sw2 != 0x6A81) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 1;
	}
	printf("Success\n");

	// Test APDU case 2; normal processing
	printf("\nTesting APDU case 2 (PC/SC mode); normal processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_2_normal;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_read_record(
		&ttl,
		1,
		1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_2_normal_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_2_normal_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 2; error processing (early)
	printf("\nTesting APDU case 2 (PC/SC mode); error processing (early)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_2_error_early;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_read_record(
		&ttl,
		1,
		1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != 0) {
		fprintf(stderr, "Unexpected data length %zu\n", r_apdu_len);
		return 1;
	}
	if (memcmp(data, test_pcsc_case_2_error_early_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6A81) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 2; error processing (late)
	printf("\nTesting APDU case 2 (PC/SC mode); error processing (late)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_2_error_late;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_read_record(
		&ttl,
		1,
		1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != 0) {
		fprintf(stderr, "Unexpected data length %zu\n", data_len);
		return 1;
	}
	if (memcmp(data, test_pcsc_case_2_error_late_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6581) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 3; normal processing
	printf("\nTesting APDU case 3 (PC/SC mode); normal processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_3_normal;
	emul_ctx.xpdu_current = NULL;
	r_apdu_len = sizeof(r_apdu);

	r = emv_ttl_trx(
		&ttl,
		(uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x04, 0xde, 0xad, 0xbe, 0xef }, // EXTERNAL AUTHENTICATE
		9,
		r_apdu,
		&r_apdu_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (r_apdu_len != 2) {
		fprintf(stderr, "Unexpected R-APDU length %zu\n", r_apdu_len);
		return 1;
	}
	if (memcmp(r_apdu, test_pcsc_case_3_normal_data, r_apdu_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("r_apdu", r_apdu, r_apdu_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 3; error processing
	printf("\nTesting APDU case 3 (PC/SC mode); error processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_3_error;
	emul_ctx.xpdu_current = NULL;
	r_apdu_len = sizeof(r_apdu);

	r = emv_ttl_trx(
		&ttl,
		(uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x04, 0xde, 0xad, 0xbe, 0xef }, // EXTERNAL AUTHENTICATE
		9,
		r_apdu,
		&r_apdu_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (r_apdu_len != 2) {
		fprintf(stderr, "Unexpected R-APDU length %zu\n", r_apdu_len);
		return 1;
	}
	if (memcmp(r_apdu, test_pcsc_case_3_error_data, r_apdu_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("r_apdu", r_apdu, r_apdu_len);
		return 1;
	}
	if (sw1sw2 != 0x6A81) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; normal processing
	printf("\nTesting APDU case 4 (PC/SC mode); normal processing...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_normal;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	const uint8_t PSE[] = "1PAY.SYS.DDF01";
	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_4_normal_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_normal_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; error processing (early)
	printf("\nTesting APDU case 4 (PC/SC mode); error processing (early)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_error_early;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_error_early_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6A82) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; error processing (late)
	printf("\nTesting APDU case 4 (PC/SC mode); error processing (late)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_error_late;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_error_late_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6581) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 2; normal processing (using both '61' and '6C' procedure bytes)
	printf("\nTesting APDU case 2 (PC/SC mode); normal processing (using both '61' and '6C' procedure bytes)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_2_normal_advanced;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_read_record(
		&ttl,
		1,
		1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_2_normal_advanced_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_2_normal_advanced_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; normal processing (using multiple '61' procedure bytes)
	printf("\nTesting APDU case 4 (PC/SC mode); normal processing (using multiple '61' procedure bytes)...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_normal_advanced;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_4_normal_advanced_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_normal_advanced_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x9000) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; warning processing ('62' then '6C')
	printf("\nTesting APDU case 4 (PC/SC mode); warning processing ('62' then '6C')...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_warning1;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_4_warning1_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_warning1_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6286) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	// Test APDU case 4; warning processing ('61' then '62')
	printf("\nTesting APDU case 4 (PC/SC mode); warning processing ('61' then '62')...\n");
	emul_ctx.xpdu_list = test_pcsc_case_4_warning2;
	emul_ctx.xpdu_current = NULL;
	data_len = sizeof(data);

	r = emv_ttl_select_by_df_name(
		&ttl,
		PSE,
		sizeof(PSE) - 1,
		data,
		&data_len,
		&sw1sw2
	);
	if (r) {
		fprintf(stderr, "emv_ttl_trx() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test_pcsc_case_4_warning2_data)) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data length\n");
		return 1;
	}
	if (memcmp(data, test_pcsc_case_4_warning2_data, data_len) != 0) {
		fprintf(stderr, "emv_ttl_trx() failed; incorrect response data\n");
		print_buf("data", data, data_len);
		return 1;
	}
	if (sw1sw2 != 0x6286) {
		fprintf(stderr, "Unexpected SW1-SW2 %04X\n", sw1sw2);
		return 2;
	}
	printf("Success\n");

	return 0;
}
