/**
 * @file emv_read_application_data_test.c
 * @brief Unit tests for EMV Read Application Data
 *
 * Copyright (c) 2024 Leon Lynch
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
#include "emv_cardreader_emul.h"
#include "emv_ttl.h"
#include "emv_tal.h"
#include "emv_tlv.h"
#include "emv_tags.h"

#include <stdio.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

static const uint8_t test2_afl[] = { 0x09, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 }; // Malformed AFL
static const struct xpdu_t test2_apdu_list[] = {
	{ 0 } // No card interaction
};

static const uint8_t test3_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };
static const struct xpdu_t test3_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		2, (uint8_t[]){ 0x69, 0x85 }, // Conditions of use not satisfied
	},
	{ 0 }
};

static const uint8_t test4_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };
static const struct xpdu_t test4_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		4, (uint8_t[]){ 0x71, 0x00, 0x90, 0x00 }, // Invalid record template
	},
	{ 0 }
};

static const uint8_t test5_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };
static const struct xpdu_t test5_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		57, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0xFF, 0xFF, // Additional data after record template
			0x90, 0x00,
		},
	},
	{ 0 }
};

static const uint8_t test6_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };
static const struct xpdu_t test6_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0F, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30, // Malformed EMV data
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{ 0 }
};

static const uint8_t test7_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x00, 0x00, 0x18, 0x01, 0x02, 0x01 }; // Malformed AFL
static const struct xpdu_t test7_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{ 0 }
};

static const uint8_t test8_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x02, 0x00 };
static const struct xpdu_t test8_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x14, 0x00 }, // READ RECORD from SFI 2, record 1
		23, (uint8_t[]) {
			0x70, 0x13, 0x8F, 0x01, 0x94, 0x92, 0x00, 0x9F, 0x32, 0x01, 0x03, 0x9F,
			0x47, 0x01, 0x03, 0x9F, 0x49, 0x03, 0x9F, 0x37, 0x04,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x14, 0x00 }, // READ RECORD from SFI 2, record 2
		7, (uint8_t[]){
			0x70, 0x03, 0x8F, 0x01, 0x94, // Redundant EMV field
			0x90, 0x00,
		},
	},
	{ 0 }
};

static const uint8_t test9_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x02, 0x01 };
static const struct xpdu_t test9_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x14, 0x00 }, // READ RECORD from SFI 2, record 1
		24, (uint8_t[]) {
			0x70, 0x14, 0x5A, 0x08, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0x5F, 0x34, 0x01, 0x01, 0x5F, 0x24, 0x03, 0x22, 0x12, 0x31,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x14, 0x00 }, // READ RECORD from SFI 2, record 2
		23, (uint8_t[]){
			0x70, 0x13, 0x8F, 0x01, 0x94, 0x92, 0x00, 0x9F, 0x32, 0x01, 0x03, 0x9F,
			0x47, 0x01, 0x03, 0x9F, 0x49, 0x03, 0x9F, 0x37, 0x04,
			0x90, 0x00,
		},
	},
	{ 0 }
};

static const uint8_t test10_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x60, 0x01, 0x01, 0x01, 0x10, 0x01, 0x02, 0x01, 0x58, 0x01, 0x01, 0x01 };
static const struct xpdu_t test10_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x64, 0x00 }, // READ RECORD from SFI 12, record 1
		3, (uint8_t[]){ 0xFF, 0x90, 0x00 }, // Invalid record for offline data authentication
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x14, 0x00 }, // READ RECORD from SFI 2, record 1
		74, (uint8_t[]) {
			0x70, 0x46, 0x5A, 0x08, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0x5F, 0x34, 0x01, 0x01, 0x5F, 0x24, 0x03, 0x22, 0x12, 0x31,
			0x8C, 0x15, 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04,
			0x8D, 0x19, 0x8A, 0x02, 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04, 0x91, 0x08,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x14, 0x00 }, // READ RECORD from SFI 2, record 2
		23, (uint8_t[]){
			0x70, 0x13, 0x8F, 0x01, 0x94, 0x92, 0x00, 0x9F, 0x32, 0x01, 0x03, 0x9F,
			0x47, 0x01, 0x03, 0x9F, 0x49, 0x03, 0x9F, 0x37, 0x04,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x5C, 0x00 }, // READ RECORD from SFI 11, record 1
		7, (uint8_t[]){ 0x70, 0x03, 0x01, 0x01, 0xFF, 0x90, 0x00 },
	},
	{ 0 }
};

static const uint8_t test11_afl[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x02, 0x01, 0x58, 0x01, 0x01, 0x01 };
static const struct xpdu_t test11_apdu_list[] = {
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD from SFI 1, record 2
		55, (uint8_t[]) {
			0x70, 0x33, 0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0xD2, 0x21, 0x22, 0x01, 0x17, 0x58, 0x92, 0x88, 0x89, 0x5F, 0x20, 0x0C,
			0x45, 0x58, 0x50, 0x49, 0x52, 0x45, 0x44, 0x2F, 0x43, 0x41, 0x52, 0x44,
			0x9F, 0x1F, 0x0E, 0x31, 0x37, 0x35, 0x38, 0x39, 0x30, 0x39, 0x36, 0x30,
			0x30, 0x30, 0x30, 0x30, 0x30,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x14, 0x00 }, // READ RECORD from SFI 2, record 1
		74, (uint8_t[]) {
			0x70, 0x46, 0x5A, 0x08, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19,
			0x5F, 0x34, 0x01, 0x01, 0x5F, 0x24, 0x03, 0x22, 0x12, 0x31,
			0x8C, 0x15, 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04,
			0x8D, 0x19, 0x8A, 0x02, 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04, 0x91, 0x08,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x14, 0x00 }, // READ RECORD from SFI 2, record 2
		23, (uint8_t[]){
			0x70, 0x13, 0x8F, 0x01, 0x94, 0x92, 0x00, 0x9F, 0x32, 0x01, 0x03, 0x9F,
			0x47, 0x01, 0x03, 0x9F, 0x49, 0x03, 0x9F, 0x37, 0x04,
			0x90, 0x00,
		},
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x5C, 0x00 }, // READ RECORD from SFI 11, record 1
		7, (uint8_t[]){ 0x70, 0x03, 0x01, 0x01, 0xFF, 0x90, 0x00 },
	},
	{ 0 }
};

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_tlv_list_t icc = EMV_TLV_LIST_INIT;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	printf("\nTest 1: No AFL...\n");
	r = emv_read_application_data(
		&ttl,
		&icc
	);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "emv_initiate_application_processing() did not return EMV_OUTCOME_CARD_ERROR; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 2: Malformed AFL...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test2_afl),
		test2_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test2_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test2_afl,
		sizeof(test2_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_AFL_INVALID) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_AFL_INVALID; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Unexpected card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 3: Read Record status 6985...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test3_afl),
		test3_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test3_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test3_afl,
		sizeof(test3_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_READ_RECORD_FAILED) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_READ_RECORD_FAILED; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 4: Invalid record template...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test4_afl),
		test4_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test4_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test4_afl,
		sizeof(test4_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_READ_RECORD_INVALID) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_READ_RECORD_INVALID; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 5: Record with additional data after template...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test5_afl),
		test5_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test5_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test5_afl,
		sizeof(test5_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_READ_RECORD_INVALID) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_READ_RECORD_INVALID; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 6: Record with malformed EMV data...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test6_afl),
		test6_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test6_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test6_afl,
		sizeof(test6_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 7: Malformed AFL entry...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test7_afl),
		test7_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test7_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test7_afl,
		sizeof(test7_afl),
		&icc
	);
	if (r != EMV_TAL_ERROR_AFL_INVALID) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_ERROR_AFL_INVALID; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 8: Redundant EMV field...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test8_afl),
		test8_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test8_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_read_application_data(
		&ttl,
		&icc
	);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "emv_read_application_data() did not return EMV_OUTCOME_CARD_ERROR; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 9: Mandatory fields missing...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test9_afl),
		test9_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test9_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_read_application_data(
		&ttl,
		&icc
	);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "emv_read_application_data() did not return EMV_OUTCOME_CARD_ERROR; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 10: Invalid record template for offline data authentication...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test10_afl),
		test10_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test10_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_tal_read_afl_records(
		&ttl,
		test10_afl,
		sizeof(test10_afl),
		&icc
	);
	if (r != EMV_TAL_RESULT_ODA_RECORD_INVALID) {
		fprintf(stderr, "emv_tal_read_afl_records() did not return EMV_TAL_RESULT_ODA_RECORD_INVALID; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	printf("\nTest 11: Normal processing...\n");
	r = emv_tlv_list_push(
		&icc,
		EMV_TAG_94_APPLICATION_FILE_LOCATOR,
		sizeof(test11_afl),
		test11_afl,
		0
	);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d", r);
		r = 1;
		goto exit;
	}
	print_emv_tlv_list(&icc);
	emul_ctx.xpdu_list = test11_apdu_list;
	emul_ctx.xpdu_current = NULL;
	r = emv_read_application_data(
		&ttl,
		&icc
	);
	if (r) {
		fprintf(stderr, "emv_read_application_data() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&icc);
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&icc);

	return r;
}
