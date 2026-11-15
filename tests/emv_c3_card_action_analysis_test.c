/**
 * @file emv_c3_card_action_analysis_test.c
 * @brief Unit tests for EMV contactless kernel C-3 Card Action Analysis
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
#include "emv_c3.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Terminal Transaction Qualifiers: EMV mode supported
static const uint8_t test_ttq[] = { EMV_TTQ_EMV_MODE_SUPPORTED, 0x00, 0x00, 0x00 };

// Terminal Transaction Qualifiers: EMV mode supported, online cryptogram required
static const uint8_t test_ttq_online_required[] = { EMV_TTQ_EMV_MODE_SUPPORTED, EMV_TTQ_ONLINE_CRYPTOGRAM_REQUIRED, 0x00, 0x00 };

// Terminal Transaction Qualifiers: EMV mode supported, offline-only reader
static const uint8_t test_ttq_offline_only[] = { EMV_TTQ_EMV_MODE_SUPPORTED | EMV_TTQ_OFFLINE_ONLY_READER, 0x00, 0x00, 0x00 };

struct test_t {
	const char* name;

	struct emv_tlv_t* icc_data;

	const uint8_t* ttq;

	int expected_result;
};

static const struct test_t test[] = {
	{
		.name = "CID indicates TC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_APPROVED,
	},

	{
		.name = "CID indicates AAC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "CID indicates ARQC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_ARQC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = 0,
	},

	{
		.name = "CID indicates RFU",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_RFU }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "CID invalid",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 2, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "No CID; IAD indicates TC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F10_ISSUER_APPLICATION_DATA, 7, (uint8_t[]){ 0x06, 0x01, 0x11, 0x03, 0x10, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_APPROVED,
	},

	{
		.name = "No CID; IAD indicates AAC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F10_ISSUER_APPLICATION_DATA, 7, (uint8_t[]){ 0x06, 0x01, 0x11, 0x03, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "No CID; IAD indicates ARQC",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F10_ISSUER_APPLICATION_DATA, 7, (uint8_t[]){ 0x06, 0x01, 0x11, 0x03, 0x20, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = 0,
	},

	{
		.name = "No CID; IAD too short",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F10_ISSUER_APPLICATION_DATA, 4, (uint8_t[]){ 0x06, 0x01, 0x11, 0x03 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "CID indicates TC; online cryptogram required by reader",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq_online_required,

		.expected_result = 0,
	},

	{
		.name = "CID indicates AAC; online cryptogram required by reader",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq_online_required,

		.expected_result = EMV_OUTCOME_DECLINED,
	},

	{
		.name = "CID indicates ARQC; offline-only reader",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_ARQC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = test_ttq_offline_only,

		.expected_result = 0,
	},

	{
		.name = "No TTQ",

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA, 1, (uint8_t[]){ EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.ttq = NULL,

		.expected_result = EMV_ERROR_INVALID_CONFIG,
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
	struct emv_ctx_t emv;

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
		r = emv_ctx_init(&emv, NULL);
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
		if (test[i].ttq) {
			r = emv_tlv_list_push(&emv.terminal, EMV_TAG_9F66_TTQ, 4, test[i].ttq, 0);
			if (r) {
				fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
				r = 1;
				goto exit;
			}
		}

		// Test card action analysis...
		r = emv_c3_card_action_analysis(&emv);
		if (r != test[i].expected_result) {
			fprintf(stderr, "Unexpected emv_c3_card_action_analysis() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
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
