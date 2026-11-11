/**
 * @file emv_completion_test.c
 * @brief Unit tests for EMV Completion
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

// Authorisation Response Code: '00' (Approved)
static const uint8_t test_arc_approved[2] = { 0x30, 0x30 };

struct test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	// Authorisation Response Code appended by emv_online_processing(), or NULL
	// if online processing was not performed
	const uint8_t* online_arc;

	uint8_t ref_ctrl;

	const struct xpdu_t* xpdu_list;

	int expected_result;

	// Expected Authorisation Response Code (field 8A) in the terminal data
	// list after completion
	uint8_t expected_arc[2];

	// Expected GENAC2 fields in the completion data list
	uint8_t expected_cid;
	uint8_t expected_atc[2];
	uint8_t expected_ac[8];

	uint8_t tvr[5];
	uint8_t tsi[2];
};

static const struct test_t test[] = {
	{
		.name = "Offline decline without online processing",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.online_arc = NULL,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_AAC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x00, 0x00, 0x02, 0x5A, 0x33, 0x00 }, // GENAC2 [AAC] with ARC 'Z3'
				15, (uint8_t[]){ 0x80, 0x0B, 0x00, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x90, 0x00 }, // GENAC response format 1 with AAC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_DECLINED,

		// Z3: Unable to go online, offline declined
		.expected_arc = { 0x5A, 0x33 },

		.expected_cid = EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC,
		.expected_atc = { 0x00, 0x01 },
		.expected_ac = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11 },

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Offline approval without online processing",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.online_arc = NULL,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x40, 0x00, 0x02, 0x59, 0x33, 0x00 }, // GENAC2 [TC] with ARC 'Y3'
				15, (uint8_t[]){ 0x80, 0x0B, 0x40, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x90, 0x00 }, // GENAC response format 1 with TC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_APPROVED,

		// Y3: Unable to go online, offline approved
		.expected_arc = { 0x59, 0x33 },

		.expected_cid = EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC,
		.expected_atc = { 0x00, 0x01 },
		.expected_ac = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11 },

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Approval after online processing",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.online_arc = test_arc_approved,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x40, 0x00, 0x02, 0x30, 0x30, 0x00 }, // GENAC2 [TC] with ARC '00'
				15, (uint8_t[]){ 0x80, 0x0B, 0x40, 0x00, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x90, 0x00 }, // GENAC response format 1 with TC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_APPROVED,

		// Authorisation Response Code is retained from online processing
		.expected_arc = { 0x30, 0x30 },

		.expected_cid = EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC,
		.expected_atc = { 0x00, 0x02 },
		.expected_ac = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Decline by card application after online approval",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.online_arc = test_arc_approved,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x40, 0x00, 0x02, 0x30, 0x30, 0x00 }, // GENAC2 [TC] with ARC '00'
				15, (uint8_t[]){ 0x80, 0x0B, 0x00, 0x00, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x90, 0x00 }, // GENAC response format 1 with AAC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_DECLINED,

		// Authorisation Response Code is retained from online processing
		.expected_arc = { 0x30, 0x30 },

		.expected_cid = EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC,
		.expected_atc = { 0x00, 0x02 },
		.expected_ac = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 },

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Invalid reference control parameter",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.online_arc = NULL,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_ARQC,

		.xpdu_list = NULL,

		.expected_result = EMV_ERROR_INVALID_PARAMETER,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
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

static int verify_tlv(
	const struct emv_tlv_list_t* list,
	unsigned int tag,
	const char* tag_str,
	const uint8_t* expected,
	size_t expected_len
)
{
	const struct emv_tlv_t* tlv;

	tlv = emv_tlv_list_find_const(list, tag);
	if (!tlv ||
		tlv->length != expected_len ||
		memcmp(tlv->value, expected, expected_len) != 0
	) {
		fprintf(stderr, "Incorrect field %s\n", tag_str);
		if (tlv) {
			print_buf(tag_str, tlv->value, tlv->length);
		}
		print_buf("Expected", expected, expected_len);
		return 1;
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

	// Ensure that the exit path is safe if emv_ctx_init() fails
	memset(&emv, 0, sizeof(emv));

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		printf("Test %zu (%s)...\n", i + 1, test[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, &ttl);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(
			test[i].icc_data,
			&emv.icc
		);
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
		if (test[i].online_arc) {
			// Emulate the issuer response fields appended by
			// emv_online_processing()
			// See EMV 4.4 Book 4, 6.3.8
			r = emv_tlv_list_push(&emv.terminal, EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE, 2, test[i].online_arc, 0);
			if (r) {
				fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
				r = 1;
				goto exit;
			}
		}
		emv.aip = emv_tlv_list_find_const(&emv.icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);

		// Prepare card emulation for current test
		emul_ctx.xpdu_list = test[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;

		// Test completion...
		r = emv_completion(&emv, test[i].ref_ctrl);
		if (r != test[i].expected_result) {
			fprintf(stderr, "Unexpected emv_completion() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}
		if (test[i].xpdu_list && emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.terminal);
		print_emv_tlv_list(&emv.completion);

		if (test[i].expected_result == EMV_ERROR_INVALID_PARAMETER) {
			// Validate that no completion data was produced
			if (!emv_tlv_list_is_empty(&emv.completion)) {
				fprintf(stderr, "Completion data list unexpectedly NOT empty\n");
				r = 1;
				goto exit;
			}
		} else {
			// Validate Authorisation Response Code
			r = verify_tlv(
				&emv.terminal,
				EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE,
				"8A",
				test[i].expected_arc,
				sizeof(test[i].expected_arc)
			);
			if (r) {
				r = 1;
				goto exit;
			}

			// Validate GENAC2 fields
			r = verify_tlv(
				&emv.completion,
				EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA,
				"9F27",
				&test[i].expected_cid,
				sizeof(test[i].expected_cid)
			);
			if (r) {
				r = 1;
				goto exit;
			}
			r = verify_tlv(
				&emv.completion,
				EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER,
				"9F36",
				test[i].expected_atc,
				sizeof(test[i].expected_atc)
			);
			if (r) {
				r = 1;
				goto exit;
			}
			r = verify_tlv(
				&emv.completion,
				EMV_TAG_9F26_APPLICATION_CRYPTOGRAM,
				"9F26",
				test[i].expected_ac,
				sizeof(test[i].expected_ac)
			);
			if (r) {
				r = 1;
				goto exit;
			}
		}

		// Validate TVR
		if (emv.tvr->length != sizeof(test[i].tvr) ||
			memcmp(emv.tvr->value, test[i].tvr, sizeof(test[i].tvr)) != 0
		) {
			fprintf(stderr, "Incorrect TVR\n");
			print_buf("TVR", emv.tvr->value, emv.tvr->length);
			print_buf("Expected", test[i].tvr, sizeof(test[i].tvr));
			r = 1;
			goto exit;
		}

		// Validate TSI
		if (emv.tsi->length != sizeof(test[i].tsi) ||
			memcmp(emv.tsi->value, test[i].tsi, sizeof(test[i].tsi)) != 0
		) {
			fprintf(stderr, "Incorrect TSI\n");
			print_buf("TSI", emv.tsi->value, emv.tsi->length);
			print_buf("Expected", test[i].tsi, sizeof(test[i].tsi));
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
