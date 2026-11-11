/**
 * @file emv_card_action_analysis_test.c
 * @brief Unit tests for EMV Card Action Analysis
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

// Unpredictable Number (field 9F37) requested by CDOL1
static const uint8_t test_un[4] = { 0x01, 0x02, 0x03, 0x04 };

struct test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	// Offline Data Authentication (ODA) method selected before GENAC1
	enum emv_oda_method_t oda_method;

	uint8_t ref_ctrl;

	const struct xpdu_t* xpdu_list;

	int expected_result;

	// Expected Authorisation Response Code (field 8A) in the terminal data
	// list, or NULL if none is expected
	const uint8_t* expected_arc;

	// Expected Cryptogram Information Data (field 9F27) in the completion data
	// list, or NULL if no GENAC2 is expected
	const uint8_t* expected_completion_cid;

	uint8_t tvr[5];
	uint8_t tsi[2];
};

static const struct test_t test[] = {
	{
		.name = "ARQC with CDA failure declines using GENAC2 for an AAC",

		.icc_data = (struct emv_tlv_t[]){
			// CDOL1: Unpredictable Number
			{ {{ EMV_TAG_8C_CDOL1, 3, (uint8_t[]){ 0x9F, 0x37, 0x04 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.oda_method = EMV_ODA_METHOD_CDA,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_ARQC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				10, (uint8_t[]){ 0x80, 0xAE, 0x90, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x00 }, // GENAC1 [ARQC + CDA]
				24, (uint8_t[]){ 0x77, 0x14, 0x9F, 0x27, 0x01, 0x80, 0x9F, 0x36, 0x02, 0x00, 0x01, 0x9F, 0x4B, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x90, 0x00 }, // GENAC response format 2 with ARQC and invalid SDAD
			},
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x00, 0x00, 0x02, 0x5A, 0x31, 0x00 }, // GENAC2 [AAC] with ARC 'Z1'
				15, (uint8_t[]){ 0x80, 0x0B, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x90, 0x00 }, // GENAC response format 1 with AAC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_DECLINED,

		// Z1: Offline declined
		.expected_arc = (uint8_t[]){ 0x5A, 0x31 },

		.expected_completion_cid = (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC },

		.tvr = { EMV_TVR_CDA_FAILED, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED | EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "TC attempt answered by ARQC with CDA failure declines using GENAC2 for an AAC",

		.icc_data = (struct emv_tlv_t[]){
			// CDOL1: Unpredictable Number
			{ {{ EMV_TAG_8C_CDOL1, 3, (uint8_t[]){ 0x9F, 0x37, 0x04 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.oda_method = EMV_ODA_METHOD_CDA,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				10, (uint8_t[]){ 0x80, 0xAE, 0x50, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x00 }, // GENAC1 [TC + CDA]
				24, (uint8_t[]){ 0x77, 0x14, 0x9F, 0x27, 0x01, 0x80, 0x9F, 0x36, 0x02, 0x00, 0x01, 0x9F, 0x4B, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x90, 0x00 }, // GENAC response format 2 with ARQC and invalid SDAD
			},
			{
				8, (uint8_t[]){ 0x80, 0xAE, 0x00, 0x00, 0x02, 0x5A, 0x31, 0x00 }, // GENAC2 [AAC] with ARC 'Z1'
				15, (uint8_t[]){ 0x80, 0x0B, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x90, 0x00 }, // GENAC response format 1 with AAC
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_DECLINED,

		// Z1: Offline declined, replacing the 'Y1' of the TC attempt
		.expected_arc = (uint8_t[]){ 0x5A, 0x31 },

		.expected_completion_cid = (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC },

		.tvr = { EMV_TVR_CDA_FAILED, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED | EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "TC with CDA failure declines without GENAC2",

		.icc_data = (struct emv_tlv_t[]){
			// CDOL1: Unpredictable Number
			{ {{ EMV_TAG_8C_CDOL1, 3, (uint8_t[]){ 0x9F, 0x37, 0x04 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.oda_method = EMV_ODA_METHOD_CDA,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_TC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				10, (uint8_t[]){ 0x80, 0xAE, 0x50, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x00 }, // GENAC1 [TC + CDA]
				24, (uint8_t[]){ 0x77, 0x14, 0x9F, 0x27, 0x01, 0x40, 0x9F, 0x36, 0x02, 0x00, 0x01, 0x9F, 0x4B, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x90, 0x00 }, // GENAC response format 2 with TC and invalid SDAD
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_DECLINED,

		// Y1: Offline approved, as requested by Terminal Action Analysis
		.expected_arc = (uint8_t[]){ 0x59, 0x31 },

		.expected_completion_cid = NULL,

		.tvr = { EMV_TVR_CDA_FAILED, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED | EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "ARQC without CDA continues to online processing",

		.icc_data = (struct emv_tlv_t[]){
			// CDOL1: Unpredictable Number
			{ {{ EMV_TAG_8C_CDOL1, 3, (uint8_t[]){ 0x9F, 0x37, 0x04 }, 0 }}, NULL },
			// CDOL2: Authorisation Response Code
			{ {{ EMV_TAG_8D_CDOL2, 2, (uint8_t[]){ 0x8A, 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.oda_method = EMV_ODA_METHOD_NONE,

		.ref_ctrl = EMV_TTL_GENAC_TYPE_ARQC,

		.xpdu_list = (const struct xpdu_t[]){
			{
				10, (uint8_t[]){ 0x80, 0xAE, 0x80, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x00 }, // GENAC1 [ARQC]
				15, (uint8_t[]){ 0x80, 0x0B, 0x80, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x90, 0x00 }, // GENAC response format 1 with ARQC
			},
			{ 0 }
		},

		.expected_result = 0,

		// Authorisation Response Code is provided by online processing
		.expected_arc = NULL,

		.expected_completion_cid = NULL,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED, 0x00 },
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

	// Ensure that the exit path is safe if emv_ctx_init() fails
	memset(&emv, 0, sizeof(emv));

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		const struct emv_tlv_t* arc;
		const struct emv_tlv_t* cid;

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
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_9F37_UNPREDICTABLE_NUMBER, sizeof(test_un), test_un, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);

		// Emulate the Offline Data Authentication (ODA) state produced by
		// emv_offline_data_authentication(). The ICC public key modulus length
		// deliberately does not match the length of the Signed Dynamic
		// Application Data (SDAD) provided by the card emulation, such that
		// CDA fails in conjunction with GENAC1.
		// See EMV 4.4 Book 2, 6.6.2, step 1
		emv.oda.method = test[i].oda_method;
		emv.oda.icc_pkey.modulus_len = 128;

		// Prepare card emulation for current test
		emul_ctx.xpdu_list = test[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;

		// Test card action analysis...
		r = emv_card_action_analysis(&emv, test[i].ref_ctrl);
		if (r != test[i].expected_result) {
			fprintf(stderr, "Unexpected emv_card_action_analysis() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}
		if (test[i].xpdu_list && emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);
		print_emv_tlv_list(&emv.terminal);
		print_emv_tlv_list(&emv.completion);

		// Validate that no field was duplicated in the terminal data list
		if (emv_tlv_list_has_duplicate(&emv.terminal)) {
			fprintf(stderr, "Duplicate field in terminal data list\n");
			r = 1;
			goto exit;
		}

		// Validate Authorisation Response Code
		arc = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE);
		if (test[i].expected_arc) {
			if (!arc ||
				arc->length != 2 ||
				memcmp(arc->value, test[i].expected_arc, 2) != 0
			) {
				fprintf(stderr, "Incorrect Authorisation Response Code (8A)\n");
				if (arc) {
					print_buf("8A", arc->value, arc->length);
				}
				print_buf("Expected", test[i].expected_arc, 2);
				r = 1;
				goto exit;
			}
		} else if (arc) {
			fprintf(stderr, "Unexpected Authorisation Response Code (8A)\n");
			r = 1;
			goto exit;
		}

		// Validate GENAC2 fields in the completion data list
		cid = emv_tlv_list_find_const(&emv.completion, EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA);
		if (test[i].expected_completion_cid) {
			if (!cid ||
				cid->length != 1 ||
				cid->value[0] != *test[i].expected_completion_cid
			) {
				fprintf(stderr, "Incorrect completion Cryptogram Information Data (9F27)\n");
				if (cid) {
					print_buf("9F27", cid->value, cid->length);
				}
				print_buf("Expected", test[i].expected_completion_cid, 1);
				r = 1;
				goto exit;
			}
		} else if (!emv_tlv_list_is_empty(&emv.completion)) {
			fprintf(stderr, "Completion data list unexpectedly NOT empty\n");
			r = 1;
			goto exit;
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
