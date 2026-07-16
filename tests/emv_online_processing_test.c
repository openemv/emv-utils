/**
 * @file emv_online_processing_test.c
 * @brief Unit tests for EMV Online Processing and EMV Completion
 *
 * Copyright 2026 Leon Lynch
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
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_ttl.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Common 8-byte Issuer Authentication Data used across the online processing
// test cases (arbitrary bytes; the emulator matches them verbatim).
static const uint8_t iad_bytes[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
};

// 2-byte Authorisation Response Code: '00' — Approved
static const uint8_t arc_approved[2] = { 0x30, 0x30 };

struct online_test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	const uint8_t* iad;
	size_t iad_len;

	// Expected function return value
	int expected_return;

	const struct xpdu_t* xpdu_list;

	uint8_t tvr[5];
	uint8_t tsi[2];

	// Whether tag 91 should be present in terminal data after the call
	bool expect_tag_91;
};

static const struct online_test_t online_tests[] = {
	{
		.name = "No Issuer Authentication Data",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = NULL,
		.iad_len = 0,

		.expected_return = 0,
		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },

		.expect_tag_91 = false,
	},

	{
		.name = "IAD received, AIP does not support issuer authentication",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = iad_bytes,
		.iad_len = sizeof(iad_bytes),

		.expected_return = 0,
		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },

		.expect_tag_91 = true,
	},

	{
		.name = "IAD received, EXTERNAL AUTHENTICATE succeeds (9000)",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = iad_bytes,
		.iad_len = sizeof(iad_bytes),

		.expected_return = 0,
		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){
					0x00, 0x82, 0x00, 0x00, 0x08,
					0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
				},
				2, (uint8_t[]){ 0x90, 0x00 },
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },

		.expect_tag_91 = true,
	},

	{
		.name = "IAD received, EXTERNAL AUTHENTICATE fails (6300)",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = iad_bytes,
		.iad_len = sizeof(iad_bytes),

		.expected_return = 0,
		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){
					0x00, 0x82, 0x00, 0x00, 0x08,
					0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
				},
				2, (uint8_t[]){ 0x63, 0x00 },
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, EMV_TVR_ISSUER_AUTHENTICATION_FAILED },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },

		.expect_tag_91 = true,
	},

	{
		.name = "IAD received, EXTERNAL AUTHENTICATE returns 6985 (terminate)",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = iad_bytes,
		.iad_len = sizeof(iad_bytes),

		.expected_return = EMV_OUTCOME_CARD_ERROR,
		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){
					0x00, 0x82, 0x00, 0x00, 0x08,
					0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
				},
				2, (uint8_t[]){ 0x69, 0x85 },
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },

		.expect_tag_91 = true,
	},

	{
		.name = "Invalid iad_len (iad=NULL, iad_len=8)",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = NULL,
		.iad_len = 8,

		.expected_return = EMV_ERROR_INVALID_PARAMETER,
		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },

		.expect_tag_91 = false,
	},

	{
		.name = "Invalid iad_len (too short)",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = iad_bytes,
		.iad_len = 4,

		.expected_return = EMV_ERROR_INVALID_PARAMETER,
		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },

		.expect_tag_91 = false,
	},
};

struct completion_test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	uint8_t ref_ctrl;
	// Expected 8A value that emv_completion() will synthesise and that will
	// therefore appear in the GENAC2 C-APDU data field.
	uint8_t expected_arc[2];

	const struct xpdu_t* xpdu_list;
};

// GENAC2 response format 1: 80 0B <CID> <ATC(2)> <AC(8)> SW1SW2
// CID varies per test to reflect the GENAC2 type.
#define GENAC2_RESPONSE(CID) \
	15, (uint8_t[]){ \
		0x80, 0x0B, \
		(CID), \
		0x00, 0x01, \
		0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, \
		0x90, 0x00, \
	}

static const struct completion_test_t completion_tests[] = {
	{
		.name = "Completion AAC fallback: synthesises 8A = Z3",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ref_ctrl = EMV_TTL_GENAC_TYPE_AAC,
		.expected_arc = { 0x5A, 0x33 },

		.xpdu_list = (const struct xpdu_t[]){
			{
				// CLA=80 INS=AE P1=00 (AAC) P2=00 Lc=02 <8A_value> Le=00
				8, (uint8_t[]){
					0x80, 0xAE, 0x00, 0x00, 0x02,
					0x5A, 0x33,
					0x00,
				},
				GENAC2_RESPONSE(EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC),
			},
			{ 0 }
		},
	},

	{
		.name = "Completion TC fallback: synthesises 8A = Y3",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,
		.expected_arc = { 0x59, 0x33 },

		.xpdu_list = (const struct xpdu_t[]){
			{
				// CLA=80 INS=AE P1=40 (TC) P2=00 Lc=02 <8A_value> Le=00
				8, (uint8_t[]){
					0x80, 0xAE, 0x40, 0x00, 0x02,
					0x59, 0x33,
					0x00,
				},
				GENAC2_RESPONSE(EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC),
			},
			{ 0 }
		},
	},
};

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	while (tlv_array && tlv_array->tag) {
		r = emv_tlv_list_push(list, tlv_array->tag, tlv_array->length, tlv_array->value, 0);
		if (r) {
			return r;
		}

		++tlv_array;
	}
	return 0;
}

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	// Zero-initialise so the exit path can safely call emv_ctx_clear() even
	// if the first iteration returns before populating anything.
	memset(&emv, 0, sizeof(emv));

	for (size_t i = 0; i < sizeof(online_tests) / sizeof(online_tests[0]); ++i) {
		const struct emv_tlv_t* tag_8a;
		const struct emv_tlv_t* tag_91;

		printf("Online test %zu (%s)...\n", i + 1, online_tests[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, &ttl);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(online_tests[i].icc_data, &emv.icc);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION, 2, (uint8_t[]){ 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.aip = emv_tlv_list_find_const(&emv.icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);

		// Prepare card emulation for current test
		emul_ctx.xpdu_list = online_tests[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;

		// Test online processing...
		r = emv_online_processing(&emv, arc_approved, online_tests[i].iad, online_tests[i].iad_len);
		if (r != online_tests[i].expected_return) {
			fprintf(stderr,
				"emv_online_processing() returned %d, expected %d\n",
				r, online_tests[i].expected_return
			);
			r = 1;
			goto exit;
		}
		if (online_tests[i].xpdu_list && emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.terminal);

		tag_8a = emv_tlv_list_find_const(
			&emv.terminal,
			EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE
		);
		tag_91 = emv_tlv_list_find_const(
			&emv.terminal,
			EMV_TAG_91_ISSUER_AUTHENTICATION_DATA
		);

		if (online_tests[i].expected_return >= 0) {
			// Tag 8A is pushed before EXTERNAL AUTHENTICATE, so it is
			// present both on success and on a positive card outcome.
			if (!tag_8a ||
				tag_8a->length != 2 ||
				memcmp(tag_8a->value, arc_approved, 2) != 0
			) {
				fprintf(stderr, "Tag 8A missing or incorrect\n");
				r = 1;
				goto exit;
			}
			if (online_tests[i].expect_tag_91) {
				if (!tag_91 ||
					tag_91->length != online_tests[i].iad_len ||
					memcmp(tag_91->value, online_tests[i].iad, online_tests[i].iad_len) != 0
				) {
					fprintf(stderr, "Tag 91 missing or incorrect\n");
					r = 1;
					goto exit;
				}
			} else if (tag_91) {
				fprintf(stderr, "Unexpected tag 91 present\n");
				r = 1;
				goto exit;
			}
		} else {
			// Negative return means a validation error before any push
			if (tag_8a) {
				fprintf(stderr, "Tag 8A unexpectedly present after error\n");
				r = 1;
				goto exit;
			}
			if (tag_91) {
				fprintf(stderr, "Tag 91 unexpectedly present after error\n");
				r = 1;
				goto exit;
			}
		}

		// Validate TVR
		if (emv.tvr->length != sizeof(online_tests[i].tvr) ||
			memcmp(emv.tvr->value, online_tests[i].tvr, sizeof(online_tests[i].tvr)) != 0
		) {
			fprintf(stderr, "Incorrect TVR\n");
			print_buf("TVR", emv.tvr->value, emv.tvr->length);
			print_buf("Expected", online_tests[i].tvr, sizeof(online_tests[i].tvr));
			r = 1;
			goto exit;
		}

		// Validate TSI
		if (emv.tsi->length != sizeof(online_tests[i].tsi) ||
			memcmp(emv.tsi->value, online_tests[i].tsi, sizeof(online_tests[i].tsi)) != 0
		) {
			fprintf(stderr, "Incorrect TSI\n");
			print_buf("TSI", emv.tsi->value, emv.tsi->length);
			print_buf("Expected", online_tests[i].tsi, sizeof(online_tests[i].tsi));
			r = 1;
			goto exit;
		}

		r = emv_ctx_clear(&emv);
		if (r) {
			fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		printf("Passed!\n\n");
	}

	for (size_t i = 0; i < sizeof(completion_tests) / sizeof(completion_tests[0]); ++i) {
		const struct emv_tlv_t* tag_8a;

		printf("Completion fallback test %zu (%s)...\n", i + 1, completion_tests[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, &ttl);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(completion_tests[i].icc_data, &emv.icc);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION, 2, (uint8_t[]){ 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.aip = emv_tlv_list_find_const(&emv.icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);

		// Prepare card emulation for current test
		emul_ctx.xpdu_list = completion_tests[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;

		// Test completion...
		r = emv_completion(&emv, completion_tests[i].ref_ctrl);
		if (r) {
			fprintf(stderr, "emv_completion() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		if (completion_tests[i].xpdu_list && emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.terminal);

		tag_8a = emv_tlv_list_find_const(
			&emv.terminal,
			EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE
		);
		if (!tag_8a ||
			tag_8a->length != 2 ||
			memcmp(tag_8a->value, completion_tests[i].expected_arc, 2) != 0
		) {
			fprintf(stderr, "Synthesised tag 8A missing or incorrect\n");
			print_buf("Expected", completion_tests[i].expected_arc, 2);
			if (tag_8a) {
				print_buf("Actual", tag_8a->value, tag_8a->length);
			}
			r = 1;
			goto exit;
		}

		r = emv_ctx_clear(&emv);
		if (r) {
			fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		printf("Passed!\n\n");
	}

	// Success
	printf("Success!\n");
	r = 0;
	goto exit;

exit:
	emv_ctx_clear(&emv);

	return r;
}
