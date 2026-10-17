/**
 * @file emv_ctx_prepare_txn_test.c
 * @brief Unit tests for EMV transaction preparation
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
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Mandatory application independent configuration data
static const struct emv_tlv_t test_config_data[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0x20, 0xF8, 0xC8 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){ 0x22 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){ 0xFA, 0x00, 0xF0, 0xA3, 0xFF }, 0 }}, NULL },
	{ {{ 0 }} },
};

// Mandatory transaction parameters, excluding Transaction Type (9C) and the
// amount fields, which are provided by each test
static const struct emv_tlv_t test_param_data[] = {
	{ {{ EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER, 2, (uint8_t[]){ 0x00, 0x42 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x26, 0x10, 0x17 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F21_TRANSACTION_TIME, 3, (uint8_t[]){ 0x12, 0x34, 0x56 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT, 1, (uint8_t[]){ 0x02 }, 0 }}, NULL },
	{ {{ 0 }} },
};

struct test_t {
	const char* name;

	// Transaction Type (9C) and amount fields for current test
	const struct emv_tlv_t* params_data;

	int result;

	// Expected cached transaction amount. Only used if result is zero.
	uint32_t txn_amount;

	// Expected amount fields in the transaction parameters after transaction
	// preparation. NULL if the field is expected to be absent. Only used if
	// result is zero.
	const uint8_t* amount_n;
	const uint8_t* amount_b;
	const uint8_t* other_amount_n;
	const uint8_t* other_amount_b;
};

static const struct test_t test[] = {
	{
		.name = "Amount, Authorised (Numeric) (9F02) only",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised: 1000
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 1000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 },
	},

	{
		.name = "Amount, Authorised (Binary) (81) only",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised: 3000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 3000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
	},

	{
		.name = "Both forms of Amount, Authorised present",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised: 123456789
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x07, 0x5B, 0xCD, 0x15 }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 123456789,
		.amount_n = (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 },
		.amount_b = (uint8_t[]){ 0x07, 0x5B, 0xCD, 0x15 },
	},

	{
		.name = "Zero Amount, Authorised",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 0,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Both forms of Amount, Authorised differ",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 3000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			// Amount, Authorised (Numeric): 1000
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Authorised (Numeric) (9F02) with invalid length",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Authorised (Binary) (81) with invalid length",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 3, (uint8_t[]){ 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Authorised (Numeric) (9F02) with invalid digit",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Authorised (Numeric) (9F02) exceeding maximum",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised: 999999999999
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x99, 0x99, 0x99, 0x99, 0x99, 0x99 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Other not processed for non-cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			// Amount, Other (Numeric): 1000
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 3000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
		.other_amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
	},

	{
		.name = "Amount, Other (Numeric) (9F03) only for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			// Amount, Other: 1000
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 3000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
		.other_amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
		.other_amount_b = (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 },
	},

	{
		.name = "Amount, Other (Binary) (9F04) only for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			// Amount, Other: 1000
			{ {{ EMV_TAG_9F04_AMOUNT_OTHER_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 3000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
		.other_amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
		.other_amount_b = (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 },
	},

	{
		.name = "Amount, Other equal to Amount, Authorised for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.txn_amount = 3000,
		.amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
		.other_amount_n = (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 },
		.other_amount_b = (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 },
	},

	{
		.name = "Amount, Other not found for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Both forms of Amount, Other differ for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			// Amount, Other (Numeric): 1000
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			// Amount, Other (Binary): 2000
			{ {{ EMV_TAG_9F04_AMOUNT_OTHER_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x07, 0xD0 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Other exceeding Amount, Authorised for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			// Amount, Authorised: 1000
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			// Amount, Other: 3000
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Amount, Other (Numeric) (9F03) with invalid length for cashback transaction",

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x30, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x10, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},
};

static int append_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	int r;

	while (tlv_array && tlv_array->tag) {
		r = emv_tlv_list_push(list, tlv_array->tag, tlv_array->length, tlv_array->value, 0);
		if (r) {
			return r;
		}

		++tlv_array;
	}

	return 0;
}

static int verify_amount_fields(
	const struct emv_tlv_list_t* params,
	unsigned int tag_n,
	const uint8_t* amount_n,
	unsigned int tag_b,
	const uint8_t* amount_b
)
{
	const struct emv_tlv_t* tlv;

	// Each form of the amount must be present if it is expected and must be
	// absent if it is not expected
	tlv = emv_tlv_list_find_const(params, tag_n);
	if (!amount_n) {
		if (tlv) {
			fprintf(stderr, "Unexpected amount field (0x%04X)\n", tag_n);
			return 1;
		}
	} else if (!tlv || tlv->length != 6 ||
		memcmp(tlv->value, amount_n, 6) != 0
	) {
		fprintf(stderr, "Incorrect amount field (0x%04X)\n", tag_n);
		if (tlv) {
			print_buf("Amount field", tlv->value, tlv->length);
		}
		print_buf("Expected", amount_n, 6);
		return 1;
	}

	tlv = emv_tlv_list_find_const(params, tag_b);
	if (!amount_b) {
		if (tlv) {
			fprintf(stderr, "Unexpected amount field (0x%04X)\n", tag_b);
			return 1;
		}
	} else if (!tlv || tlv->length != 4 ||
		memcmp(tlv->value, amount_b, 4) != 0
	) {
		fprintf(stderr, "Incorrect amount field (0x%04X)\n", tag_b);
		if (tlv) {
			print_buf("Amount field", tlv->value, tlv->length);
		}
		print_buf("Expected", amount_b, 4);
		return 1;
	}

	return 0;
}

static int verify_txn_amounts(
	const struct emv_ctx_t* ctx,
	const struct test_t* test_case
)
{
	int r;

	if (ctx->txn_amount != test_case->txn_amount) {
		fprintf(stderr, "Incorrect transaction amount of %u instead of %u\n",
			(unsigned int)ctx->txn_amount, (unsigned int)test_case->txn_amount);
		return 1;
	}

	// Transaction preparation must not duplicate transaction parameters
	if (emv_tlv_list_has_duplicate(&ctx->params)) {
		fprintf(stderr, "Transaction parameters unexpectedly contain duplicate fields\n");
		return 1;
	}

	r = verify_amount_fields(
		&ctx->params,
		EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC,
		test_case->amount_n,
		EMV_TAG_81_AMOUNT_AUTHORISED_BINARY,
		test_case->amount_b
	);
	if (r) {
		return r;
	}

	return verify_amount_fields(
		&ctx->params,
		EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC,
		test_case->other_amount_n,
		EMV_TAG_9F04_AMOUNT_OTHER_BINARY,
		test_case->other_amount_b
	);
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

	// Transaction preparation does not require a Terminal Transport Layer
	// (TTL) context because it does not interact with the card
	r = emv_ctx_init(&emv, NULL);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	printf("Testing invalid parameters...\n");
	r = emv_ctx_prepare_txn(NULL);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ctx_prepare_txn() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	printf("Passed!\n\n");

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		printf("Test %zu (%s)...\n", i + 1, test[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, NULL);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = append_tlv_list(test_config_data, &emv.config.data);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = append_tlv_list(test_param_data, &emv.params);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = append_tlv_list(test[i].params_data, &emv.params);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.params);

		// Test transaction preparation...
		r = emv_ctx_prepare_txn(&emv);
		if (r != test[i].result) {
			fprintf(stderr, "Unexpected emv_ctx_prepare_txn() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}

		if (!test[i].result) {
			print_emv_tlv_list(&emv.params);

			// Validate cached transaction amount and amount fields
			r = verify_txn_amounts(&emv, &test[i]);
			if (r) {
				r = 1;
				goto exit;
			}

			// Repeated transaction preparation must yield the same result
			r = emv_ctx_prepare_txn(&emv);
			if (r) {
				fprintf(stderr, "emv_ctx_prepare_txn() failed; r=%d\n", r);
				r = 1;
				goto exit;
			}
			r = verify_txn_amounts(&emv, &test[i]);
			if (r) {
				r = 1;
				goto exit;
			}

			// Resetting the context must clear the cached transaction amount
			r = emv_ctx_reset(&emv);
			if (r) {
				fprintf(stderr, "emv_ctx_reset() failed; r=%d\n", r);
				r = 1;
				goto exit;
			}
			if (emv.txn_amount) {
				fprintf(stderr, "Transaction amount unexpectedly not cleared; txn_amount=%u\n",
					(unsigned int)emv.txn_amount);
				r = 1;
				goto exit;
			}
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
