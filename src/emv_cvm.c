/**
 * @file emv_cvm.c
 * @brief EMV Cardholder Verification Method (CVM) helper functions
 *
 * Copyright 2026 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_cvm.h"
#include "emv.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_config.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

int emv_cvm_init(struct emv_cvm_ctx_t* ctx)
{
	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_CVM_ERROR_INVALID_PARAMETER;
	}
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

int emv_cvm_results_set(
	struct emv_ctx_t* ctx,
	uint8_t cvm,
	uint8_t cvm_cond,
	uint8_t result
)
{
	int r;
	struct emv_tlv_t* tlv;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_CVM_ERROR_INVALID_PARAMETER;
	}

	// CVM Results (field 9F34) may already have been populated earlier in the
	// transaction, in which case it must be updated instead of duplicated
	tlv = emv_tlv_list_find(&ctx->terminal, EMV_TAG_9F34_CVM_RESULTS);
	if (tlv) {
		if (tlv->length != 3) {
			emv_debug_error("CVM Results (9F34) has invalid length %u",
				tlv->length
			);
			return EMV_CVM_ERROR_INTERNAL;
		}

		// CVM Results (field 9F34) exists; update it
		tlv->value[0] = cvm;
		tlv->value[1] = cvm_cond;
		tlv->value[2] = result;
	} else {
		// CVM Results (field 9F34) doesn't exist; add it
		r = emv_tlv_list_push(
			&ctx->terminal,
			EMV_TAG_9F34_CVM_RESULTS,
			3,
			(uint8_t[]){ cvm, cvm_cond, result },
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
			emv_debug_error("Internal error");
			return EMV_CVM_ERROR_INTERNAL;
		}
	}

	return 0;
}

int emv_cvm_results_set_failed(struct emv_ctx_t* ctx)
{
	struct emv_tlv_t* cvm_result;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_CVM_ERROR_INVALID_PARAMETER;
	}

	cvm_result = emv_tlv_list_find(&ctx->terminal, EMV_TAG_9F34_CVM_RESULTS);
	if (!cvm_result || cvm_result->length != 3) {
		emv_debug_error("CVM Results (9F34) not found or invalid");
		return EMV_CVM_ERROR_INTERNAL;
	}

	// See EMV 4.4 Book 3, 10.5.5, Figure 12
	// See EMV 4.4 Book 4, 6.3.4.5
	// See EMV 4.4 Book 4, 6.3.4.5, Table 2
	if (cvm_result->value[0] == EMV_CVM_NOT_PERFORMED) {
		emv_debug_info("Cardholder verification was not successful; no CVM performed");
		cvm_result->value[1] = 0x00;
		cvm_result->value[2] = EMV_CVM_RESULT_FAILED;
	} else {
		emv_debug_info("Cardholder verification was not successful; last CVM performed failed");
		// Retain last CVM Code and CVM Condition
		cvm_result->value[2] = EMV_CVM_RESULT_FAILED;
	}

	return 0;
}

int emv_cvm_list_init(struct emv_ctx_t* ctx)
{
	int r;
	const struct emv_tlv_t* cvm_list;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_CVM_ERROR_INVALID_PARAMETER;
	}

	// Determine whether CVM List is present and not empty
	// See EMV 4.4 Book 3, 10.5
	cvm_list = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_8E_CVM_LIST);
	if (!cvm_list || cvm_list->length == 0 || cvm_list->length == 4 + 4) {
		emv_debug_info("CVM List not found or empty");
		return 1;
	}

	r = emv_cvmlist_itr_init(
		cvm_list->value,
		cvm_list->length,
		&ctx->cvm.amounts,
		&ctx->cvm.itr
	);
	if (r) {
		emv_debug_trace_msg("emv_cvmlist_itr_init() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Internal error");
			return EMV_CVM_ERROR_INTERNAL;
		}

		emv_debug_error("Invalid CVM List");
		return EMV_CVM_ERROR_CVM_LIST_INVALID;
	}

	return 0;
}

bool emv_cvm_is_recognised(uint8_t cvm)
{
	// See EMV 4.4 Book 3, Annex C3, Table 43
	// See EMV 4.4 Book 4, 6.3.4
	switch (cvm) {
		case EMV_CV_RULE_CVM_FAIL:
		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT:
		case EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED:
		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT_AND_SIGNATURE:
		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED:
		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED_AND_SIGNATURE:
		case EMV_CV_RULE_CVM_SIGNATURE:
		case EMV_CV_RULE_NO_CVM:
			return true;

		default:
			// A terminal without biometric support may choose not to
			// recognise the Biometric CVM Codes
			// See EMV 4.4 Book 3, 10.5.5
			return false;
	}
}

bool emv_cvm_is_supported(uint8_t cvm, uint8_t cvm_caps)
{
	bool online_pin_enciphered = cvm_caps & EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE;
	bool offline_pin_plaintext = cvm_caps & EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE;
	bool offline_pin_enciphered = cvm_caps & (EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_RSA | EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_ECC);
	bool signature = cvm_caps & EMV_TERM_CAPS_CVM_SIGNATURE;
	bool no_cvm = cvm_caps & EMV_TERM_CAPS_CVM_NO_CVM;

	// Determine whether CVM is supported
	// See EMV 4.4 Book 4, 6.3.4
	switch (cvm) {
		case EMV_CV_RULE_CVM_FAIL:
			// Fail CVM shall always be considered supported
			return true;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT:
			return offline_pin_plaintext;

		case EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED:
			return online_pin_enciphered;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT_AND_SIGNATURE:
			// For Combination CVMs, both CVM codes must be supported
			return offline_pin_plaintext && signature;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED:
			return offline_pin_enciphered;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED_AND_SIGNATURE:
			// For Combination CVMs, both CVM codes must be supported
			return offline_pin_enciphered && signature;

		case EMV_CV_RULE_CVM_SIGNATURE:
			return signature;

		case EMV_CV_RULE_NO_CVM:
			return no_cvm;

		default:
			return false;
	}
}

static bool emv_cvm_txn_type_condition_is_satisfied(
	const struct emv_ctx_t* ctx,
	uint8_t cvm_cond
)
{
	const struct emv_tlv_t* term_type;
	const struct emv_tlv_t* txn_type;
	bool unattended;
	bool cash;
	bool cashback;

	term_type = emv_config_data_get(ctx, EMV_TAG_9F35_TERMINAL_TYPE);
	if (!term_type || term_type->length != 1) {
		// Data required by CVM Condition is not present
		// See EMV 4.4 Book 3, 10.5
		emv_debug_trace_msg("Terminal Type (9F35) not found or invalid");
		return false;
	}
	txn_type = emv_tlv_list_find_const(&ctx->params, EMV_TAG_9C_TRANSACTION_TYPE);
	if (!txn_type || txn_type->length != 1) {
		// Data required by CVM Condition is not present
		// See EMV 4.4 Book 3, 10.5
		emv_debug_trace_msg("Transaction Type (9C) not found or invalid");
		return false;
	}

	// Determine whether terminal is unattended
	// See EMV 4.4 Book 4, Annex A1
	switch (term_type->value[0] & EMV_TERM_TYPE_ENV_MASK) {
		case EMV_TERM_TYPE_ENV_UNATTENDED_ONLINE_ONLY:
		case EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_WITH_ONLINE:
		case EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_ONLY:
			unattended = true;
			break;

		default:
			unattended = false;
			break;
	}
	cash = txn_type->value[0] == EMV_TRANSACTION_TYPE_CASH;
	cashback = txn_type->value[0] == EMV_TRANSACTION_TYPE_CASHBACK;

	// See EMV 4.4 Book 3, Annex C3, Table 44
	switch (cvm_cond) {
		case EMV_CV_RULE_COND_UNATTENDED_CASH:
			return unattended && cash;

		case EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK:
			return !cash && !cashback;

		case EMV_CV_RULE_COND_MANUAL_CASH:
			return !unattended && cash;

		case EMV_CV_RULE_COND_CASHBACK:
			return cashback;

		default:
			return false;
	}
}

static bool emv_cvm_amount_condition_is_satisfied(
	const struct emv_ctx_t* ctx,
	uint8_t cvm_cond
)
{
	const struct emv_tlv_t* txn_currency_code;
	const struct emv_tlv_t* app_currency_code;

	txn_currency_code = emv_tlv_list_find_const(&ctx->params, EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE);
	if (!txn_currency_code || txn_currency_code->length != 2) {
		// Data required by CVM Condition is not present
		// See EMV 4.4 Book 3, 10.5
		emv_debug_trace_msg("Transaction Currency Code (5F2A) not found or invalid");
		return false;
	}
	app_currency_code = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F42_APPLICATION_CURRENCY_CODE);
	if (!app_currency_code || app_currency_code->length != 2) {
		// Data required by CVM Condition is not present
		// See EMV 4.4 Book 3, 10.5
		emv_debug_trace_msg("Application Currency Code (9F42) not found or invalid");
		return false;
	}

	// Determine whether transaction is in the application currency
	// See EMV 4.4 Book 3, Annex C3, Table 44
	if (memcmp(txn_currency_code->value, app_currency_code->value, 2) != 0) {
		emv_debug_trace_msg("Transaction Currency Code (5F2A) differs from Application Currency Code (9F42)");
		return false;
	}

	// See EMV 4.4 Book 3, Annex C3, Table 44
	switch (cvm_cond) {
		case EMV_CV_RULE_COND_LESS_THAN_X:
			return ctx->txn_amount < ctx->cvm.amounts.X;

		case EMV_CV_RULE_COND_MORE_THAN_X:
			return ctx->txn_amount > ctx->cvm.amounts.X;

		case EMV_CV_RULE_COND_LESS_THAN_Y:
			return ctx->txn_amount < ctx->cvm.amounts.Y;

		case EMV_CV_RULE_COND_MORE_THAN_Y:
			return ctx->txn_amount > ctx->cvm.amounts.Y;

		default:
			return false;
	}
}

bool emv_cvm_condition_is_satisfied(
	const struct emv_ctx_t* ctx,
	uint8_t cvm_cond,
	bool cvm_supported
)
{
	if (!ctx) {
		return false;
	}

	// See EMV 4.4 Book 3, Annex C3, Table 44
	switch (cvm_cond) {
		case EMV_CV_RULE_COND_ALWAYS:
			return true;

		case EMV_CV_RULE_COND_CVM_SUPPORTED:
			return cvm_supported;

		case EMV_CV_RULE_COND_UNATTENDED_CASH:
		case EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK:
		case EMV_CV_RULE_COND_MANUAL_CASH:
		case EMV_CV_RULE_COND_CASHBACK:
			return emv_cvm_txn_type_condition_is_satisfied(ctx, cvm_cond);

		case EMV_CV_RULE_COND_LESS_THAN_X:
		case EMV_CV_RULE_COND_MORE_THAN_X:
		case EMV_CV_RULE_COND_LESS_THAN_Y:
		case EMV_CV_RULE_COND_MORE_THAN_Y:
			return emv_cvm_amount_condition_is_satisfied(ctx, cvm_cond);

		default:
			// Unknown CVM Condition Code
			return false;
	}
}
