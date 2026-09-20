/**
 * @file emv_c3_initiate_kernel_processing_test.c
 * @brief Unit tests for EMV contactless kernel C-3 initiation
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

#include "emv_c3.h"
#include "emv.h"
#include "emv_cardreader_emul.h"
#include "emv_ttl.h"
#include "emv_tlv.h"
#include "emv_app.h"
#include "emv_config.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Reuse source data for all tests
static const uint8_t test_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
static const uint8_t test_kernel_id[] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// Contactless Floor Limit is not enabled and the amount exceeds the Terminal
// Floor Limit (field 9F1B), therefore Online Cryptogram Required is set
static const uint8_t test_ttq[] = { 0x37, 0x80, 0x40, 0x00 };

static const struct emv_tlv_t test_param_data[] = {
	{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x07, 0x5B, 0xCD, 0x15 }, 0 }}, NULL }, // Numeric 123456789
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL }, // Binary 0x75BCD15
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x24, 0x02, 0x17 }, 0 }}, NULL },
	{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ 0x09 }, 0 }}, NULL },
};
static const struct emv_tlv_t test_config_data[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL }, // Numeric 10000
};
static const struct emv_config_app_t test_config_app = { // Combination for kernel C-3
	.aid = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 },
	.aid_len = 7,
	.asi = EMV_ASI_PARTIAL_MATCH,
};
static const struct emv_tlv_t test_config_app_data[] = { // Configuration data for kernel C-3
	{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x01, 0x2C }, 0 }}, NULL },
	{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
};

static const uint8_t test_fci[] = { // Contactless FCI without PDOL
	0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00,
	0x03, 0x10, 0x10, 0xA5, 0x07, 0x50, 0x05, 0x41,
	0x50, 0x50, 0x20, 0x31,
};
static const uint8_t test_fci_pdol[] = { // Contactless FCI with PDOL
	0x6F, 0x25, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00,
	0x03, 0x10, 0x10, 0xA5, 0x1A, 0x50, 0x05, 0x41,
	0x50, 0x50, 0x20, 0x31,
	// PDOL requesting:
	// - Terminal Transaction Qualifiers (field 9F66)
	// - Amount, Authorised (field 9F02)
	// - Terminal Country Code (field 9F1A)
	// - Transaction Date (field 9A)
	// - Transaction Type (field 9C)
	// - Unpredictable Number (field 9F37)
	0x9F, 0x38, 0x10, 0x9F, 0x66, 0x04, 0x9F, 0x02,
	0x06, 0x9F, 0x1A, 0x02, 0x9A, 0x03, 0x9C, 0x01,
	0x9F, 0x37, 0x04,
};

static const struct xpdu_t test_gpo_ttl_failure[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		1, (uint8_t[]){ 0x00 }, // Invalid response
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_6984[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		2, (uint8_t[]){ 0x69, 0x84 }, // Reference data not usable
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_6985[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		2, (uint8_t[]){ 0x69, 0x85 }, // Conditions of use not satisfied
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_6986[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		2, (uint8_t[]){ 0x69, 0x86 }, // Command not allowed
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_6a81[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_invalid_format[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		26, (uint8_t[]){
			// Neither response message template format 1 nor format 2
			0x70, 0x16, 0x82, 0x02, 0x20, 0x00, 0x94, 0x10,
			0x08, 0x03, 0x03, 0x00, 0x10, 0x01, 0x03, 0x00,
			0x10, 0x05, 0x05, 0x00, 0x18, 0x01, 0x01, 0x01,
			0x90, 0x00,
		}, // GPO response
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_format1[] = {
	{
		28, (uint8_t[]){
			0x80, 0xA8, 0x00, 0x00, 0x16, 0x83, 0x14,
			0x37, 0x80, 0x40, 0x00, // Terminal Transaction Qualifiers (field 9F66)
			0x00, 0x01, 0x23, 0x45, 0x67, 0x89, // Amount, Authorised (field 9F02)
			0x05, 0x28, // Terminal Country Code (field 9F1A)
			0x24, 0x02, 0x17, // Transaction Date (field 9A)
			0x09, // Transaction Type (field 9C)
			0x00, 0x00, 0x00, 0x00, // Unpredictable Number (field 9F37)
			0x00,
		}, // GPO with PDOL data requested by test_fci_pdol

		18, (uint8_t[]){
			0x80, 0x0E, 0x20, 0x00, 0x08, 0x03, 0x03, 0x00,
			0x10, 0x01, 0x03, 0x00, 0x18, 0x01, 0x01, 0x01,
			0x90, 0x00,
		}, // GPO response format 1

		23, 4, // Ignore random Unpredictable Number in GPO
	},
	{ 0 }
};
static const uint8_t test_gpo_format1_aip[] = { 0x20, 0x00 };
static const uint8_t test_gpo_format1_afl[] = {
	0x08, 0x03, 0x03, 0x00, 0x10, 0x01, 0x03, 0x00,
	0x18, 0x01, 0x01, 0x01,
};

static const struct xpdu_t test_gpo_format2[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		26, (uint8_t[]){
			0x77, 0x16, 0x82, 0x02, 0x20, 0x00, 0x94, 0x10,
			0x08, 0x03, 0x03, 0x00, 0x10, 0x01, 0x03, 0x00,
			0x10, 0x05, 0x05, 0x00, 0x18, 0x01, 0x01, 0x01,
			0x90, 0x00,
		}, // GPO response format 2
	},
	{ 0 }
};
static const uint8_t test_gpo_format2_aip[] = { 0x20, 0x00 };
static const uint8_t test_gpo_format2_afl[] = {
	0x08, 0x03, 0x03, 0x00, 0x10, 0x01, 0x03, 0x00,
	0x10, 0x05, 0x05, 0x00, 0x18, 0x01, 0x01, 0x01,
};

static const struct xpdu_t test_gpo_no_aip[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		22, (uint8_t[]){
			0x77, 0x12, 0x94, 0x10, 0x08, 0x03, 0x03, 0x00,
			0x10, 0x01, 0x03, 0x00, 0x10, 0x05, 0x05, 0x00,
			0x18, 0x01, 0x01, 0x01,
			0x90, 0x00,
		}, // GPO response format 2 without AIP
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_no_afl[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		8, (uint8_t[]){
			0x77, 0x04, 0x82, 0x02, 0x20, 0x00,
			0x90, 0x00
		}, // GPO response format 2 without AFL
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_no_aip_afl[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		23, (uint8_t[]){
			0x77, 0x13,
			// Track 2 Equivalent Data (57) only
			0x57, 0x11, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01,
			0x01, 0x19, 0xD2, 0x21, 0x22, 0x01, 0x17, 0x58,
			0x92, 0x88, 0x89,
			0x90, 0x00,
		}, // GPO response format 2 without AIP or AFL
	},
	{ 0 }
};

struct test_t {
	const char* name;
	const struct xpdu_t* xpdu_list;
	int result;
};

static const struct test_t test_gpo_failure[] = {
	// See EMV Contactless Book C-3 v2.11, 4.1.1.2
	{ "GPO transport failure", test_gpo_ttl_failure, EMV_OUTCOME_CARD_ERROR },

	// See EMV Contactless Book C-3 v2.11, 5.2.2.2
	{ "GPO status 6984", test_gpo_6984, EMV_OUTCOME_TRY_ANOTHER_INTERFACE },
	{ "GPO status 6985", test_gpo_6985, EMV_OUTCOME_SELECT_NEXT },
	{ "GPO status 6986", test_gpo_6986, EMV_OUTCOME_TRY_AGAIN_SEE_PHONE },
	{ "GPO status 6A81", test_gpo_6a81, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },

	// See EMV Contactless Book C-3 v2.11, 4.2.1.1
	{ "GPO response format invalid", test_gpo_invalid_format, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },
};

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	size_t tlv_array_count,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	for (size_t i = 0; i < tlv_array_count; ++i) {
		r = emv_tlv_list_push(list, tlv_array[i].tag, tlv_array[i].length, tlv_array[i].value, 0);
		if (r) {
			return r;
		}
	}

	return 0;
}

static int verify_gpo_field(
	const char* name,
	const struct emv_tlv_t* tlv,
	const uint8_t* value,
	size_t value_len
)
{
	if (!value) {
		if (tlv) {
			fprintf(stderr, "%s unexpectedly populated\n", name);
			print_buf(name, tlv->value, tlv->length);
			return 1;
		}

		return 0;
	}

	if (!tlv) {
		fprintf(stderr, "Failed to find %s\n", name);
		return 1;
	}
	if (tlv->length != value_len ||
		memcmp(tlv->value, value, value_len) != 0
	) {
		fprintf(stderr, "Incorrect %s\n", name);
		print_buf(name, tlv->value, tlv->length);
		print_buf("Expected", value, value_len);
		return 1;
	}

	return 0;
}

static int verify_terminal_data(const struct emv_ctx_t* ctx)
{
	const struct emv_tlv_t* tlv;

	if (!ctx) {
		return -1;
	}

	// This is ugly but that's why it's in a helper function
	// See EMV Contactless Book C-3 v2.11, 2.1
	if (emv_tlv_list_is_empty(&ctx->terminal) ||
		ctx->terminal.front->tag != EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL ||
		!ctx->terminal.front->next ||
		ctx->terminal.front->next->tag != EMV_TAG_9F39_POS_ENTRY_MODE ||
		!ctx->terminal.front->next->next ||
		ctx->terminal.front->next->next->tag != EMV_TAG_9F06_AID ||
		!ctx->terminal.front->next->next->next ||
		ctx->terminal.front->next->next->next->tag != EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION ||
		!ctx->terminal.front->next->next->next->next ||
		ctx->terminal.front->next->next->next->next->tag != EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS ||
		!ctx->terminal.front->next->next->next->next->next ||
		ctx->terminal.front->next->next->next->next->next->tag != EMV_TAG_9F37_UNPREDICTABLE_NUMBER ||
		!ctx->terminal.front->next->next->next->next->next->next ||
		ctx->terminal.front->next->next->next->next->next->next->tag != EMV_TAG_9F66_TTQ ||
		ctx->terminal.front->next->next->next->next->next->next->next
	) {
		fprintf(stderr, "Unexpected terminal data list state\n");
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL);
	if (tlv->length != sizeof(test_kernel_id) ||
		memcmp(tlv->value, test_kernel_id, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Kernel Identifier - Terminal (96)\n");
		print_buf("Kernel Identifier - Terminal", tlv->value, tlv->length);
		print_buf("Expected", test_kernel_id, sizeof(test_kernel_id));
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F06_AID);
	if (tlv->length != sizeof(test_aid) ||
		memcmp(tlv->value, test_aid, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Application Identifier (AID) - terminal (9F06)\n");
		print_buf("AID", tlv->value, tlv->length);
		print_buf("Expected", test_aid, sizeof(test_aid));
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F66_TTQ);
	if (tlv->length != sizeof(test_ttq) ||
		memcmp(tlv->value, test_ttq, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Terminal Transaction Qualifiers (9F66)\n");
		print_buf("TTQ", tlv->value, tlv->length);
		print_buf("Expected", test_ttq, sizeof(test_ttq));
		return 1;
	}

	// Validate cached terminal fields
	if (ctx->kernel_id != emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL) ||
		ctx->aid != emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F06_AID) ||
		ctx->tvr != emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS) ||
		ctx->tsi != emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION)
	) {
		fprintf(stderr, "Incorrect cached terminal fields\n");
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
	struct emv_config_app_t config_app;

	memset(&ttl, 0, sizeof(ttl));
	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;
	ttl.contactless = true;

	config_app = test_config_app;

	r = emv_ctx_init(&emv, &ttl);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Populate data sources
	r = populate_tlv_list(test_param_data, sizeof(test_param_data) / sizeof(test_param_data[0]), &emv.params);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_tlv_list(test_config_data, sizeof(test_config_data) / sizeof(test_config_data[0]), &emv.config.data);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_tlv_list(test_config_app_data, sizeof(test_config_app_data) / sizeof(test_config_app_data[0]), &config_app.data);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		r = 1;
		goto exit;
	}

	printf("\nTesting invalid parameters...\n");
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(NULL, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting missing selected_app...\n");
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	for (size_t i = 0; i < sizeof(test_gpo_failure) / sizeof(test_gpo_failure[0]); ++i) {
		printf("\nTesting %s...\n", test_gpo_failure[i].name);
		emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
		if (!emv.selected_app) {
			fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
			r = 1;
			goto exit;
		}
		emv.selected_app->config = &config_app;
		emul_ctx.xpdu_list = test_gpo_failure[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;
		r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
		if (r != test_gpo_failure[i].result) {
			fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}
		if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		if (!emv_tlv_list_is_empty(&emv.icc)) {
			fprintf(stderr, "ICC list unexpectedly NOT empty\n");
			r = 1;
			goto exit;
		}
		if (emv.aip || emv.afl) {
			fprintf(stderr, "AIP or AFL unexpectedly populated\n");
			r = 1;
			goto exit;
		}
		emv_app_free(emv.selected_app);
		emv.selected_app = NULL;
		printf("Success\n");
	}

	printf("\nTesting PDOL requiring TTQ, and GPO response format 1...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci_pdol, sizeof(test_fci_pdol));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_format1;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AIP", emv.aip, test_gpo_format1_aip, sizeof(test_gpo_format1_aip));
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AFL", emv.afl, test_gpo_format1_afl, sizeof(test_gpo_format1_afl));
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_terminal_data(&emv);
	if (r) {
		print_emv_tlv_list(&emv.terminal);
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	printf("\nTesting no PDOL and GPO response format 2...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_format2;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_is_empty(&emv.icc)) {
		fprintf(stderr, "ICC list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AIP", emv.aip, test_gpo_format2_aip, sizeof(test_gpo_format2_aip));
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AFL", emv.afl, test_gpo_format2_afl, sizeof(test_gpo_format2_afl));
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_terminal_data(&emv);
	if (r) {
		print_emv_tlv_list(&emv.terminal);
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	printf("\nTesting GPO response without AIP...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_no_aip;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	// Application Interchange Profile (field 82) is optional in the GPO
	// response for kernel C-3
	// See EMV Contactless Book C-3 v2.11, Annex D.1
	r = verify_gpo_field("AIP", emv.aip, NULL, 0);
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AFL", emv.afl, test_gpo_format2_afl, sizeof(test_gpo_format2_afl));
	if (r) {
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	printf("\nTesting GPO response without AFL...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_no_afl;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AIP", emv.aip, test_gpo_format2_aip, sizeof(test_gpo_format2_aip));
	if (r) {
		r = 1;
		goto exit;
	}
	// Application File Locator (field 94) is optional in the GPO response for
	// kernel C-3
	// See EMV Contactless Book C-3 v2.11, Annex D.1
	r = verify_gpo_field("AFL", emv.afl, NULL, 0);
	if (r) {
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	printf("\nTesting GPO response without AIP or AFL...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_no_aip_afl;
	emul_ctx.xpdu_current = NULL;
	r = emv_c3_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c3_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	// Application Interchange Profile (field 82) and Application File Locator
	// (field 94) are optional in the GPO response for kernel C-3
	// See EMV Contactless Book C-3 v2.11, Annex D.1
	r = verify_gpo_field("AIP", emv.aip, NULL, 0);
	if (r) {
		r = 1;
		goto exit;
	}
	r = verify_gpo_field("AFL", emv.afl, NULL, 0);
	if (r) {
		r = 1;
		goto exit;
	}
	// The remaining GPO output must still be available
	if (!emv_tlv_list_find_const(&emv.icc, EMV_TAG_57_TRACK2_EQUIVALENT_DATA)) {
		fprintf(stderr, "Failed to find Track 2 Equivalent Data (57)\n");
		print_emv_tlv_list(&emv.icc);
		r = 1;
		goto exit;
	}
	r = verify_terminal_data(&emv);
	if (r) {
		print_emv_tlv_list(&emv.terminal);
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&config_app.data);
	emv_ctx_clear(&emv);

	return r;
}
