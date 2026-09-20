/**
 * @file emv_c2_initiate_kernel_processing_test.c
 * @brief Unit tests for EMV contactless kernel C-2 initiation
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

#include "emv_c2.h"
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
static const uint8_t test_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 };
static const uint8_t test_kernel_id[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t test_term_caps[] = { 0x20, 0x00, 0x08 };

static const struct emv_tlv_t test_param_data[] = {
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL }, // Numeric 1000
};
static const struct emv_tlv_t test_config_data[] = {
	{ {{ EMV_C2_TAG_DF8117_CARD_DATA_INPUT_CAPABILITY, 1, (uint8_t[]){ 0x20 }, 0 }}, NULL },
	{ {{ EMV_C2_TAG_DF811F_SECURITY_CAPABILITY, 1, (uint8_t[]){ 0x08 }, 0 }}, NULL },
};
static const struct emv_config_app_t test_config_app = { // Combination for kernel C-2
	.aid = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 },
	.aid_len = 7,
	.asi = EMV_ASI_PARTIAL_MATCH,
};
static const struct emv_tlv_t test_config_app_data[] = { // Configuration data for kernel C-2
	{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x00, 0x02 }, 0 }}, NULL },
};

static const uint8_t test_fci[] = { // Contactless FCI without PDOL
	0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00,
	0x04, 0x10, 0x10, 0xA5, 0x07, 0x50, 0x05, 0x41,
	0x50, 0x50, 0x20, 0x31,
};
static const uint8_t test_fci_pdol[] = { // Contactless FCI with PDOL
	0x6F, 0x1B, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00,
	0x04, 0x10, 0x10, 0xA5, 0x10, 0x50, 0x05, 0x41,
	0x50, 0x50, 0x20, 0x31,
	// PDOL requesting:
	// - Terminal Capabilities (field 9F33)
	// - Amount, Authorised (field 9F02)
	0x9F, 0x38, 0x06, 0x9F, 0x33, 0x03, 0x9F, 0x02,
	0x06,
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
			0x70, 0x16, 0x82, 0x02, 0x59, 0x80, 0x94, 0x10,
			0x08, 0x01, 0x01, 0x00, 0x10, 0x01, 0x01, 0x01,
			0x18, 0x01, 0x02, 0x00, 0x20, 0x01, 0x02, 0x00,
			0x90, 0x00,
		}, // GPO response
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_format1[] = {
	{
		17, (uint8_t[]){
			0x80, 0xA8, 0x00, 0x00, 0x0B, 0x83, 0x09, 0x20,
			0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
			0x00,
		}, // GPO with PDOL data requested by test_fci_pdol
		18, (uint8_t[]){
			0x80, 0x0E, 0x59, 0x80, 0x08, 0x02, 0x02, 0x00,
			0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01,
			0x90, 0x00,
		}, // GPO response format 1
	},
	{ 0 }
};
static const uint8_t test_gpo_format1_aip[] = { 0x59, 0x80 };
static const uint8_t test_gpo_format1_afl[] = {
	0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00,
	0x18, 0x01, 0x02, 0x01,
};

static const struct xpdu_t test_gpo_format2[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		26, (uint8_t[]){
			0x77, 0x16, 0x82, 0x02, 0x59, 0x80, 0x94, 0x10,
			0x08, 0x01, 0x01, 0x00, 0x10, 0x01, 0x01, 0x01,
			0x18, 0x01, 0x02, 0x00, 0x20, 0x01, 0x02, 0x00,
			0x90, 0x00,
		}, // GPO response format 2
	},
	{ 0 }
};
static const uint8_t test_gpo_format2_aip[] = { 0x59, 0x80 };
static const uint8_t test_gpo_format2_afl[] = {
	0x08, 0x01, 0x01, 0x00, 0x10, 0x01, 0x01, 0x01,
	0x18, 0x01, 0x02, 0x00, 0x20, 0x01, 0x02, 0x00,
};

static const struct xpdu_t test_gpo_no_aip[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		22, (uint8_t[]){
			0x77, 0x12, 0x94, 0x10, 0x08, 0x01, 0x01, 0x00,
			0x10, 0x01, 0x01, 0x01, 0x18, 0x01, 0x02, 0x00,
			0x20, 0x01, 0x02, 0x00,
			0x90, 0x00,
		}, // GPO response format 2 without AIP
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_invalid_aip[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		25, (uint8_t[]){
			0x77, 0x15, 0x82, 0x01, 0x59, 0x94, 0x10, 0x08,
			0x01, 0x01, 0x00, 0x10, 0x01, 0x01, 0x01, 0x18,
			0x01, 0x02, 0x00, 0x20, 0x01, 0x02, 0x00,
			0x90, 0x00,
		}, // GPO response format 2 with invalid AIP length
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_no_afl[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		8, (uint8_t[]){
			0x77, 0x04, 0x82, 0x02, 0x59, 0x80,
			0x90, 0x00
		}, // GPO response format 2 without AFL
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_invalid_afl[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		17, (uint8_t[]){
			0x77, 0x0D, 0x82, 0x02, 0x59, 0x80, 0x94, 0x07,
			0x08, 0x01, 0x01, 0x00, 0x10, 0x01, 0x01,
			0x90, 0x00,
		}, // GPO response format 2 with invalid AFL length
	},
	{ 0 }
};

static const struct xpdu_t test_gpo_no_emv_mode[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		26, (uint8_t[]){
			// Application Interchange Profile without EMV mode support
			0x77, 0x16, 0x82, 0x02, 0x59, 0x00, 0x94, 0x10,
			0x08, 0x01, 0x01, 0x00, 0x10, 0x01, 0x01, 0x01,
			0x18, 0x01, 0x02, 0x00, 0x20, 0x01, 0x02, 0x00,
			0x90, 0x00,
		}, // GPO response format 2
	},
	{ 0 }
};

struct test_t {
	const char* name;
	const struct xpdu_t* xpdu_list;
	int result;
};

static const struct test_t test_gpo_failure[] = {
	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.4
	{ "GPO transport failure", test_gpo_ttl_failure, EMV_OUTCOME_CARD_ERROR },

	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.8
	{ "GPO status 6984", test_gpo_6984, EMV_OUTCOME_SELECT_NEXT },
	{ "GPO status 6985", test_gpo_6985, EMV_OUTCOME_SELECT_NEXT },
	{ "GPO status 6986", test_gpo_6986, EMV_OUTCOME_SELECT_NEXT },
	{ "GPO status 6A81", test_gpo_6a81, EMV_OUTCOME_SELECT_NEXT },

	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.11
	{ "GPO response format invalid", test_gpo_invalid_format, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },

	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.13
	{ "GPO response without AIP", test_gpo_no_aip, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },
	{ "GPO response with invalid AIP", test_gpo_invalid_aip, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },
	{ "GPO response without AFL", test_gpo_no_afl, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },
	{ "GPO response with invalid AFL", test_gpo_invalid_afl, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },

	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.16
	{ "GPO response without EMV mode support", test_gpo_no_emv_mode, EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD },
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

static int verify_gpo_output(
	const struct emv_ctx_t* ctx,
	const uint8_t* aip,
	size_t aip_len,
	const uint8_t* afl,
	size_t afl_len
)
{
	if (!ctx) {
		return -1;
	}

	if (emv_tlv_list_is_empty(&ctx->icc)) {
		fprintf(stderr, "ICC list unexpectedly empty\n");
		return 1;
	}

	if (!ctx->aip) {
		fprintf(stderr, "Failed to find AIP\n");
		return 1;
	}
	if (ctx->aip->length != aip_len ||
		memcmp(ctx->aip->value, aip, aip_len) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AIP", ctx->aip->value, ctx->aip->length);
		print_buf("Expected", aip, aip_len);
		return 1;
	}

	if (!ctx->afl) {
		fprintf(stderr, "Failed to find AFL\n");
		return 1;
	}
	if (ctx->afl->length != afl_len ||
		memcmp(ctx->afl->value, afl, afl_len) != 0
	) {
		fprintf(stderr, "Incorrect AFL\n");
		print_buf("AFL", ctx->afl->value, ctx->afl->length);
		print_buf("Expected", afl, afl_len);
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
	// See EMV Contactless Book C-2 v2.11, 6.3.3, S1.9
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
		ctx->terminal.front->next->next->next->next->next->next->tag != EMV_TAG_9F33_TERMINAL_CAPABILITIES ||
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

	// Terminal Capabilities (field 9F33) is built from Card Data Input
	// Capability (field DF8117) and Security Capability (field DF811F)
	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F33_TERMINAL_CAPABILITIES);
	if (tlv->length != sizeof(test_term_caps) ||
		memcmp(tlv->value, test_term_caps, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Terminal Capabilities (9F33)\n");
		print_buf("Terminal Capabilities", tlv->value, tlv->length);
		print_buf("Expected", test_term_caps, sizeof(test_term_caps));
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
	r = emv_c2_initiate_kernel_processing(NULL, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
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
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Card Data Input Capability (DF8117) not found or invalid...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	r = populate_tlv_list(test_config_data + 1, 1, &emv.config.data); // Omit DF8117
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	r = emv_tlv_list_push(&emv.config.data, EMV_C2_TAG_DF8117_CARD_DATA_INPUT_CAPABILITY, 2, (uint8_t[]){ 0x20, 0x00 }, 0); // Invalid DF8117
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	printf("\nTesting Security Capability (DF811F) not found or invalid...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	r = populate_tlv_list(test_config_data, 1, &emv.config.data); // Omit DF811F
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	r = emv_tlv_list_push(&emv.config.data, EMV_C2_TAG_DF811F_SECURITY_CAPABILITY, 2, (uint8_t[]){ 0x08, 0x00 }, 0); // Invalid DF811F
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	emv_app_free(emv.selected_app);
	emv.selected_app = NULL;
	printf("Success\n");

	// Restore configuration for remaining tests
	r = populate_tlv_list(test_config_data, sizeof(test_config_data) / sizeof(test_config_data[0]), &emv.config.data);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

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
		r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
		if (r != test_gpo_failure[i].result) {
			fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
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

	printf("\nTesting PDOL present and GPO response format 1...\n");
	emv.selected_app = emv_app_create_from_fci(test_fci_pdol, sizeof(test_fci_pdol));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}
	emv.selected_app->config = &config_app;
	emul_ctx.xpdu_list = test_gpo_format1;
	emul_ctx.xpdu_current = NULL;
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	r = verify_gpo_output(&emv, test_gpo_format1_aip, sizeof(test_gpo_format1_aip), test_gpo_format1_afl, sizeof(test_gpo_format1_afl));
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
	r = emv_c2_initiate_kernel_processing(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r) {
		fprintf(stderr, "Unexpected emv_c2_initiate_kernel_processing() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	r = verify_gpo_output(&emv, test_gpo_format2_aip, sizeof(test_gpo_format2_aip), test_gpo_format2_afl, sizeof(test_gpo_format2_afl));
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

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&config_app.data);
	emv_ctx_clear(&emv);

	return r;
}
