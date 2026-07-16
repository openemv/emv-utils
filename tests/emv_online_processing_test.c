/**
 * @file emv_online_processing_test.c
 * @brief Unit tests for EMV Online Processing
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
static const uint8_t test_arc[2] = { 0x30, 0x30 };

// Authorisation Response Code: 'Y1' (Offline approved)
// See EMV 4.4 Book 4, 6.3.6
// See EMV 4.4 Book 4, Annex A6, Table 35
static const uint8_t test_arc_offline_approved[2] = { 0x59, 0x31 };

// Issuer Authentication Data of minimum length
// See EMV 4.4 Book 3, 6.5.4.3
static const uint8_t test_iad_min[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
};

// Issuer Authentication Data of maximum length
// See EMV 4.4 Book 3, 6.5.4.3
static const uint8_t test_iad_max[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
};

// Issuer Authentication Data that is too short
// See EMV 4.4 Book 3, 6.5.4.3
static const uint8_t test_iad_too_short[] = {
	0x11, 0x22, 0x33, 0x44,
};

// Issuer Authentication Data that is too long
// See EMV 4.4 Book 3, 6.5.4.3
static const uint8_t test_iad_too_long[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
	0x12,
};

struct test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	const uint8_t* iad;
	size_t iad_len;

	const uint8_t* existing_arc;

	const struct xpdu_t* xpdu_list;

	int expected_result;

	uint8_t tvr[5];
	uint8_t tsi[2];
};

static const struct test_t test[] = {
	{
		.name = "No Issuer Authentication Data",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = NULL,
		.iad_len = 0,

		.xpdu_list = NULL,

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Issuer authentication not supported by card application",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: No issuer authentication
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_min,
		.iad_len = sizeof(test_iad_min),

		.xpdu_list = NULL,

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Issuer authentication successful",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_min,
		.iad_len = sizeof(test_iad_min),

		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }, // EXTERNAL AUTHENTICATE
				2, (uint8_t[]){ 0x90, 0x00 },
			},
			{ 0 }
		},

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },
	},

	{
		.name = "Issuer authentication successful; overrides offline approved ARC",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_min,
		.iad_len = sizeof(test_iad_min),

		.existing_arc = test_arc_offline_approved,

		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }, // EXTERNAL AUTHENTICATE
				2, (uint8_t[]){ 0x90, 0x00 },
			},
			{ 0 }
		},

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },
	},

	{
		.name = "Issuer authentication successful using maximum length",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_max,
		.iad_len = sizeof(test_iad_max),

		.xpdu_list = (const struct xpdu_t[]){
			{
				21, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x10, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 }, // EXTERNAL AUTHENTICATE
				2, (uint8_t[]){ 0x90, 0x00 },
			},
			{ 0 }
		},

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },
	},

	{
		.name = "Issuer authentication failed",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_min,
		.iad_len = sizeof(test_iad_min),

		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }, // EXTERNAL AUTHENTICATE
				2, (uint8_t[]){ 0x63, 0x00 }, // Issuer authentication failed
			},
			{ 0 }
		},

		.expected_result = 0,

		.tvr = { 0x00, 0x00, 0x00, 0x00, EMV_TVR_ISSUER_AUTHENTICATION_FAILED },
		.tsi = { EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED, 0x00 },
	},

	{
		.name = "Issuer authentication error",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_min,
		.iad_len = sizeof(test_iad_min),

		.xpdu_list = (const struct xpdu_t[]){
			{
				13, (uint8_t[]){ 0x00, 0x82, 0x00, 0x00, 0x08, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }, // EXTERNAL AUTHENTICATE
				2, (uint8_t[]){ 0x69, 0x85 }, // Issuer authentication error
			},
			{ 0 }
		},

		.expected_result = EMV_OUTCOME_CARD_ERROR,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Invalid Issuer Authentication Data pointer",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = NULL,
		.iad_len = sizeof(test_iad_min),

		.xpdu_list = NULL,

		.expected_result = EMV_ERROR_INVALID_PARAMETER,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Issuer Authentication Data too short",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_too_short,
		.iad_len = sizeof(test_iad_too_short),

		.xpdu_list = NULL,

		.expected_result = EMV_ERROR_INVALID_PARAMETER,

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Issuer Authentication Data too long",

		.icc_data = (struct emv_tlv_t[]){
			// Application Interchange Profile: Issuer authentication supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.iad = test_iad_too_long,
		.iad_len = sizeof(test_iad_too_long),

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
		const struct emv_tlv_t* iad;

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
		if (test[i].existing_arc) {
			// Simulate emv_card_action_analysis()
			r = emv_tlv_list_push(&emv.terminal, EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE, 2, test[i].existing_arc, 0);
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

		// Test online processing...
		r = emv_online_processing(
			&emv,
			test_arc,
			test[i].iad,
			test[i].iad_len
		);
		if (r != test[i].expected_result) {
			fprintf(stderr, "Unexpected emv_online_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
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

		// Validate issuer response fields which are only appended to the
		// terminal data list after the parameters have been validated
		// See EMV 4.4 Book 4, 6.3.8
		arc = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE);
		iad = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_91_ISSUER_AUTHENTICATION_DATA);
		if (test[i].expected_result == EMV_ERROR_INVALID_PARAMETER) {
			if (arc) {
				fprintf(stderr, "Unexpected Authorisation Response Code (8A)\n");
				r = 1;
				goto exit;
			}
			if (iad) {
				fprintf(stderr, "Unexpected Issuer Authentication Data (91)\n");
				r = 1;
				goto exit;
			}
		} else {
			if (!arc ||
				arc->length != sizeof(test_arc) ||
				memcmp(arc->value, test_arc, sizeof(test_arc)) != 0
			) {
				fprintf(stderr, "Incorrect Authorisation Response Code (8A)\n");
				if (arc) {
					print_buf("8A", arc->value, arc->length);
				}
				print_buf("Expected", test_arc, sizeof(test_arc));
				r = 1;
				goto exit;
			}
			if (test[i].iad && test[i].iad_len) {
				if (!iad ||
					iad->length != test[i].iad_len ||
					memcmp(iad->value, test[i].iad, test[i].iad_len) != 0
				) {
					fprintf(stderr, "Incorrect Issuer Authentication Data (91)\n");
					if (iad) {
						print_buf("91", iad->value, iad->length);
					}
					print_buf("Expected", test[i].iad, test[i].iad_len);
					r = 1;
					goto exit;
				}
			} else if (iad) {
				fprintf(stderr, "Unexpected Issuer Authentication Data (91)\n");
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
