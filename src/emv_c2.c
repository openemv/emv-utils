/**
 * @file emv_c2.c
 * @brief High level EMV contactless kernel C-2 interface
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

#include "emv_c2.h"
#include "emv.h"
#include "emv_ep.h"
#include "emv_tal.h"
#include "emv_app.h"
#include "emv_oda.h"
#include "emv_cvm.h"
#include "emv_tags.h"
#include "emv_fields.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include "crypto_mem.h"
#include "crypto_rand.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

int emv_c2_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
)
{
	int r;
	uint8_t term_caps[3];
	const struct emv_tlv_t* card_data_input_caps;
	const struct emv_tlv_t* security_caps;
	struct emv_tlv_list_t gpo_list = EMV_TLV_LIST_INIT;
	int gpo_tal_result;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->selected_app) {
		emv_debug_trace_msg("selected_app=%p", ctx->selected_app);
		emv_debug_error("No selected application");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("Initiate application processing");

	// Clear existing ICC data, terminal data and completion data lists to
	// avoid ambiguity
	emv_tlv_list_clear(&ctx->icc);
	emv_tlv_list_clear(&ctx->terminal);
	emv_tlv_list_clear(&ctx->completion);

	// Clear existing cached terminal fields to avoid stale pointers
	ctx->kernel_id = NULL;
	ctx->aid = NULL;
	ctx->tvr = NULL;
	ctx->tsi = NULL;
	ctx->aip = NULL;
	ctx->afl = NULL;

	// Clear existing ODA state to avoid ambiguity
	r = emv_oda_init(&ctx->oda);
	if (r) {
		emv_debug_trace_msg("emv_oda_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Clear existing CVM state to avoid ambiguity
	r = emv_cvm_init(&ctx->cvm);
	if (r) {
		emv_debug_trace_msg("emv_cvm_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// NOTE: EMV 4.4 Book 1, 12.4, states that the terminal should set the
	// value of Application Identifier (AID) - terminal (field 9F06) before
	// GET PROCESSING OPTIONS. It is not explicitly stated that PDOL may list
	// 9F06, but the assumption is that the PDOL may list any field having the
	// terminal as the source. Therefore, this implementation will create the
	// initial terminal data fields for the current transaction before PDOL
	// processing and GET PROCESSING OPTIONS. This includes contactless fields
	// like Kernel Identifier - Terminal (field 96).

	// See EMV Contactless Book C-2 v2.12, 6.3.3, S1.9
	r = emv_ep_create_common_kernel_data(ctx, pos_entry_mode);
	if (r) {
		emv_debug_trace_msg("emv_ep_create_common_kernel_data() failed; r=%d", r);
		emv_debug_error("Failed to create initial terminal data");
		return r;
	}

	// Create Terminal Capabilities (field 9F33) from:
	// - Card Data Input Capability (field DF8117)
	// - Security Capability (field DF811F)
	// See EMV Contactless Book C-2 v2.12, 6.3.3, S1.9
	memset(term_caps, 0, sizeof(term_caps));
	card_data_input_caps = emv_config_data_get(ctx, EMV_C2_TAG_DF8117_CARD_DATA_INPUT_CAPABILITY);
	if (!card_data_input_caps || card_data_input_caps->length != 1) {
		emv_debug_error("Card Data Input Capability (DF8117) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	security_caps = emv_config_data_get(ctx, EMV_C2_TAG_DF811F_SECURITY_CAPABILITY);
	if (!security_caps || security_caps->length != 1) {
		emv_debug_error("Security Capability (DF811F) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	term_caps[0] = card_data_input_caps->value[0];
	term_caps[2] = security_caps->value[0];
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9F33_TERMINAL_CAPABILITIES,
		sizeof(term_caps),
		term_caps,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Cache various terminal fields
	ctx->kernel_id = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL);
	if (!ctx->kernel_id) {
		emv_debug_error("Kernel Identifier - Terminal (96) not found");
		return EMV_ERROR_INTERNAL;
	}
	ctx->aid = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F06_AID);
	if (!ctx->aid) {
		emv_debug_error("AID not found");
		return EMV_ERROR_INTERNAL;
	}
	ctx->tvr = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
	if (!ctx->tvr) {
		emv_debug_error("TVR not found");
		return EMV_ERROR_INTERNAL;
	}
	ctx->tsi = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);
	if (!ctx->tsi) {
		emv_debug_error("TSI not found");
		return EMV_ERROR_INTERNAL;
	}

	// See EMV Contactless Book C-2 v2.12, 6.3.3, S1.13.6
	r = emv_ep_initiate_kernel_processing(ctx, &gpo_list, &gpo_tal_result);
	if (r) {
		emv_debug_trace_msg("emv_ep_initiate_kernel_processing() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (gpo_tal_result) {
		emv_debug_trace_msg("gpo_tal_result=%d", gpo_tal_result);

		switch (gpo_tal_result) {
			// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.4
			case EMV_TAL_ERROR_TTL_FAILURE:
				// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.5
				r = EMV_OUTCOME_CARD_ERROR;
				break;

			// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.8
			case EMV_TAL_RESULT_GPO_DATA_NOT_USABLE:
			case EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED:
			case EMV_TAL_RESULT_GPO_NOT_ALLOWED:
			case EMV_TAL_ERROR_GPO_FAILED:
				// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.9.2
				r = EMV_OUTCOME_SELECT_NEXT;
				break;

			// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.11
			case EMV_TAL_ERROR_GPO_PARSE_FAILED:
				// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.90.2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			default:
				r = EMV_OUTCOME_CARD_ERROR;
				break;
		}

		goto error;
	}

	// Populate AIP pointer
	// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.13
	ctx->aip = emv_tlv_list_find_const(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!ctx->aip || ctx->aip->length != 2) {
		emv_debug_error("AIP in GPO response not found or invalid");
		// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.90.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Populate AFL pointer
	// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.13
	ctx->afl = emv_tlv_list_find_const(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!ctx->afl || !ctx->afl->length || (ctx->afl->length & 0x3) != 0) {
		emv_debug_error("AFL in GPO response not found or invalid");
		// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.90.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.16
	if (!(ctx->aip->value[1] & EMV_AIP_EMV_MODE_SUPPORTED)) {
		emv_debug_error("EMV mode not supported in AIP");
		// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.90.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Move application data to ICC data list
	ctx->icc = ctx->selected_app->tlv_list;
	ctx->selected_app->tlv_list = EMV_TLV_LIST_INIT;

	// Append GPO output to ICC data list
	r = emv_tlv_list_append(&ctx->icc, &gpo_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_ERROR_INTERNAL;
		goto error;
	}

	// Success
	r = 0;
	goto exit;

error:
	ctx->aip = NULL;
	ctx->afl = NULL;
	emv_tlv_list_clear(&gpo_list);
exit:
	return r;
}

int emv_c2_read_application_data(struct emv_ctx_t* ctx)
{
	int r;
	struct emv_tlv_list_t record_data = EMV_TLV_LIST_INIT;
	int rr_tal_result;
	bool found_5F24 = false;
	bool found_5A = false;
	bool found_8C = false;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("Read application data");

	// See EMV Contactless Book C-2 v2.12, 6.7.3, S3R1.7
	r = emv_ep_read_application_data(ctx, &record_data, &rr_tal_result);
	if (r) {
		emv_debug_trace_msg("emv_ep_read_application_data() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (rr_tal_result && rr_tal_result != EMV_TAL_RESULT_ODA_RECORD_INVALID) {
		emv_debug_trace_msg("rr_tal_result=%d", rr_tal_result);

		switch (rr_tal_result) {
			// See EMV Contactless Book C-2 v2.12, 6.5.3, S3.11
			case EMV_TAL_ERROR_AFL_INVALID:
				// See EMV Contactless Book C-2 v2.12, S3.90.2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.4
			case EMV_TAL_ERROR_TTL_FAILURE:
				// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.6
				r = EMV_OUTCOME_END_APPLICATION_RESTART;
				break;

			// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.9
			case EMV_TAL_ERROR_READ_RECORD_FAILED:
				// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.10.2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.25
			case EMV_TAL_ERROR_READ_RECORD_INVALID:
			case EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED:
				// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.27.2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			default:
				r = EMV_OUTCOME_CARD_ERROR;
				break;
		}

		goto error;
	}

	// Append application records to FCI and GPO data before checking for
	// redundant or mandatory fields
	r = emv_tlv_list_append(&ctx->icc, &record_data);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_ERROR_INTERNAL;
		goto error;
	}

	if (emv_tlv_list_has_duplicate(&ctx->icc)) {
		// Redundant primitive data objects are not permitted
		// See EMV Contactless Book C-2 v2.12, 4.1.3, ParseAndStoreCardResponse
		// See EMV Contactless Book C-2 v2.12, 6.3.3, S1.7
		// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.24
		emv_debug_error("Application data contains redundant fields");
		// See EMV Contactless Book C-2 v2.12, 6.8.3, S4.27.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	for (const struct emv_tlv_t* tlv = ctx->icc.front; tlv != NULL; tlv = tlv->next) {
		// Mandatory data objects when CDA is supported
		// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.16
		if (tlv->tag == EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE) {
			found_5F24 = true;
		}
		if (tlv->tag == EMV_TAG_5A_APPLICATION_PAN) {
			found_5A = true;
		}
		if (tlv->tag == EMV_TAG_8C_CDOL1) {
			found_8C = true;
		}
	}
	if (!found_5F24 || !found_5A || !found_8C) {
		emv_debug_trace_msg("5F24=%s, 5A=%s, 8C=%s",
			found_5F24 ? "found" : "missing",
			found_5A ? "found" : "missing",
			found_8C ? "found" : "missing"
		);

		// Mandatory field not found; terminate session
		// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.17.2
		emv_debug_error("Mandatory field not found");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Success
	r = 0;
	goto exit;

error:
	emv_oda_clear(&ctx->oda);
exit:
	emv_tlv_list_clear(&record_data);
	return r;
}

static int emv_c2_cvm_capability_prepare(
	struct emv_ctx_t* ctx,
	struct emv_tlv_t* term_caps,
	bool* cvm_required_limit_exceeded
)
{
	const struct emv_tlv_t* cvm_caps_cvm_required;
	const struct emv_tlv_t* cvm_caps_no_cvm_required;
	const struct emv_tlv_t* cvm_required_limit;
	const struct emv_tlv_t* amount;

	// Ensure mandatory configuration fields are present and have valid length
	cvm_caps_cvm_required = emv_config_data_get(ctx, EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED);
	if (!cvm_caps_cvm_required || cvm_caps_cvm_required->length != 1) {
		emv_debug_trace_msg("cvm_caps_cvm_required=%p, cvm_caps_cvm_required->length=%u",
			cvm_caps_cvm_required, cvm_caps_cvm_required ? cvm_caps_cvm_required->length : 0);
		emv_debug_error("CVM Capability - CVM Required (DF8118) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	cvm_caps_no_cvm_required = emv_config_data_get(ctx, EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED);
	if (!cvm_caps_no_cvm_required || cvm_caps_no_cvm_required->length != 1) {
		emv_debug_trace_msg("cvm_caps_no_cvm_required=%p, cvm_caps_no_cvm_required->length=%u",
			cvm_caps_no_cvm_required, cvm_caps_no_cvm_required ? cvm_caps_no_cvm_required->length : 0);
		emv_debug_error("CVM Capability - No CVM Required (DF8119) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	cvm_required_limit = emv_config_data_get(ctx, EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT);
	if (!cvm_required_limit || cvm_required_limit->length != 6) {
		emv_debug_trace_msg("cvm_required_limit=%p, cvm_required_limit->length=%u",
			cvm_required_limit, cvm_required_limit ? cvm_required_limit->length : 0);
		emv_debug_error("Reader CVM Required Limit (DF8126) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}

	// Kernel C-2 does not perform offline PIN verification
	if (cvm_caps_cvm_required->value[0] & (
		EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE |
		EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_RSA |
		EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_ECC
	)) {
		emv_debug_trace_msg("cvm_caps_cvm_required=0x%02X",
			cvm_caps_cvm_required->value[0]);
		emv_debug_error("CVM Capability - CVM Required (DF8118) indicates offline PIN which is not supported");
		return EMV_ERROR_INVALID_CONFIG;
	}
	if (cvm_caps_no_cvm_required->value[0] & (
		EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE |
		EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_RSA |
		EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_ECC
	)) {
		emv_debug_trace_msg("cvm_caps_no_cvm_required=0x%02X",
			cvm_caps_no_cvm_required->value[0]);
		emv_debug_error("CVM Capability - No CVM Required (DF8119) indicates offline PIN which is not supported");
		return EMV_ERROR_INVALID_CONFIG;
	}

	amount = emv_tlv_list_find_const(&ctx->params, EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC);
	if (!amount || amount->length != 6) {
		// Presence of Amount, Authorised (Numeric) (field 9F02) should have
		// been ensured by emv_ctx_prepare_txn()
		emv_debug_error("Amount, Authorised (Numeric) (9F02) not found or invalid");
		return EMV_ERROR_INTERNAL;
	}

	// Amount, Authorised (Numeric) and Reader CVM Required Limit are both
	// EMV format "n" of the same length and can be compared directly.
	// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.30
	*cvm_required_limit_exceeded = memcmp(amount->value, cvm_required_limit->value, 6) > 0;

	// Populate CVM Capability in Terminal Capabilities (field 9F33)
	// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.32
	// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.33
	if (*cvm_required_limit_exceeded) {
		emv_debug_info("Reader CVM Required Limit exceeded");
		term_caps->value[1] = cvm_caps_cvm_required->value[0];
	} else {
		emv_debug_info("Reader CVM Required Limit not exceeded");
		term_caps->value[1] = cvm_caps_no_cvm_required->value[0];
	}

	return 0;
}

int emv_c2_cardholder_verification(struct emv_ctx_t* ctx)
{
	int r;
	const struct emv_tlv_t* kernel_config;
	struct emv_tlv_t* term_caps;
	bool cvm_required_limit_exceeded;
	struct emv_cv_rule_t rule;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->tvr || !ctx->aip) {
		emv_debug_trace_msg("tvr=%p, aip=%p", ctx->tvr, ctx->aip);
		emv_debug_error("Invalid context variable");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (ctx->cvm.state == EMV_CVM_STATE_COMPLETE) {
		// Cardholder verification has already been completed
		emv_debug_trace_msg("cvm.state=%d", ctx->cvm.state);
		return 0;
	}

	emv_debug_info("Cardholder verification");

	// Ensure mandatory configuration fields are present and have valid length
	kernel_config = emv_config_data_get(ctx, EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION);
	if (!kernel_config || kernel_config->length != 1) {
		emv_debug_error("Kernel Configuration (DF811B) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}

	term_caps = emv_tlv_list_find(&ctx->terminal, EMV_TAG_9F33_TERMINAL_CAPABILITIES);
	if (!term_caps || term_caps->length != 3) {
		// Presence of Terminal Capabilities (field 9F33) should have been
		// ensured by emv_c2_initiate_kernel_processing()
		emv_debug_error("Terminal Capabilities (9F33) not found or invalid");
		return EMV_ERROR_INTERNAL;
	}

	// Kernel C-2 populates the CVM Capability according to the Reader CVM
	// Required Limit immediately before CVM Selection
	// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.30
	r = emv_c2_cvm_capability_prepare(ctx, term_caps, &cvm_required_limit_exceeded);
	if (r) {
		return r;
	}

	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.1
	if ((ctx->aip->value[0] & EMV_AIP_ODCV_SUPPORTED) &&
		(kernel_config->value[0] & EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED)
	) {
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.2
		if (cvm_required_limit_exceeded) {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.4
			emv_debug_info("CDCVM performed");
			r = emv_cvm_results_set(
				ctx,
				EMV_C2_CVM_CDCVM_PERFORMED,
				0x00,
				EMV_CVM_RESULT_SUCCESSFUL
			);
		} else {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.3
			emv_debug_info("No CVM required");
			r = emv_cvm_results_set(
				ctx,
				EMV_CVM_NOT_PERFORMED,
				0x00,
				EMV_CVM_RESULT_SUCCESSFUL
			);
		}
		goto cvm_complete;
	}

	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.5
	if (!(ctx->aip->value[0] & EMV_AIP_CV_SUPPORTED)) {
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.6
		emv_debug_info("ICC does not support cardholder verification");
		r = emv_cvm_results_set(
			ctx,
			EMV_CVM_NOT_PERFORMED,
			0x00,
			EMV_CVM_RESULT_UNKNOWN
		);
		goto cvm_complete;
	}

	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.7
	r = emv_cvm_list_init(ctx);
	if (r < 0) {
		emv_debug_trace_msg("emv_cvm_list_init() failed; r=%d", r);
		if (r == EMV_CVM_ERROR_CVM_LIST_INVALID) {
			// Treat formatting error in CVM List like other card data errors
			// See EMV Contactless Book C-2 v2.12, 6.11.3, S456.27.2
			emv_debug_error("Invalid CVM List");
			return EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		}

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}
	if (r > 0) {
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.8
		r = emv_cvm_results_set(
			ctx,
			EMV_CVM_NOT_PERFORMED,
			0x00,
			EMV_CVM_RESULT_UNKNOWN
		);
		ctx->tvr->value[0] |= EMV_TVR_ICC_DATA_MISSING;
		goto cvm_complete;
	}

	// Process each CV Rule in the order in which it appears in the CVM List
	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.9
	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.21
	while ((r = emv_cvmlist_itr_next(&ctx->cvm.itr, &rule)) > 0) {
		uint8_t cvm = rule.cvm & EMV_CV_RULE_CVM_MASK;
		bool cvm_recognised = false;
		bool cvm_supported = false;

		emv_debug_trace_msg("CV Rule: cvm=0x%02X, cvm_cond=0x%02X", rule.cvm, rule.cvm_cond);

		cvm_recognised = emv_cvm_is_recognised(cvm);
		if (cvm_recognised) {
			cvm_supported = emv_cvm_is_supported(cvm, term_caps->value[1]);
		}

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.10
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.11
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.12
		if (!emv_cvm_condition_is_satisfied(ctx, rule.cvm_cond, cvm_supported)) {
			emv_debug_trace_msg("CVM Condition not satisfied");

			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.13
			if (emv_cvmlist_itr_is_empty(&ctx->cvm.itr)) {
				// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.14
				emv_debug_info("Cardholder verification was not successful; no CVM performed");
				r = emv_cvm_results_set(
					ctx,
					EMV_CVM_NOT_PERFORMED,
					0x00,
					EMV_CVM_RESULT_FAILED
				);
				ctx->tvr->value[2] |= EMV_TVR_CV_PROCESSING_FAILED;
				goto cvm_complete;
			}

			// Proceed to next CV Rule
			continue;
		}

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.15
		if (!cvm_recognised) {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.16
			emv_debug_info("Unrecognised CVM");
			ctx->tvr->value[2] |= EMV_TVR_CVM_UNRECOGNISED;

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.17
		} else if (cvm_supported && cvm != EMV_CV_RULE_CVM_FAIL) {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.18
			switch (cvm) {
				case EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED:
					emv_debug_info("Online PIN");
					ctx->tvr->value[2] |= EMV_TVR_ONLINE_CVM_CAPTURED;
					r = emv_cvm_results_set(
						ctx,
						rule.cvm,
						rule.cvm_cond,
						EMV_CVM_RESULT_UNKNOWN
					);
					break;

				case EMV_CV_RULE_CVM_SIGNATURE:
					emv_debug_info("Signature");
					r = emv_cvm_results_set(
						ctx,
						rule.cvm,
						rule.cvm_cond,
						EMV_CVM_RESULT_UNKNOWN
					);
					break;

				case EMV_CV_RULE_NO_CVM:
					emv_debug_info("No CVM required");
					r = emv_cvm_results_set(
						ctx,
						rule.cvm,
						rule.cvm_cond,
						EMV_CVM_RESULT_SUCCESSFUL
					);
					break;

				default:
					// Unexpected CVM
					emv_debug_trace_msg("cvm=0x%02X", cvm);
					emv_debug_error("Internal error");
					return EMV_ERROR_INTERNAL;
			}
			goto cvm_complete;

		} else {
			emv_debug_info("CVM not supported or Fail CVM processing");
		}

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.19
		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.20
		if ((rule.cvm & EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL) &&
			!emv_cvmlist_itr_is_empty(&ctx->cvm.itr)
		) {
			// Proceed to next CV Rule
			continue;
		}

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.22
		ctx->tvr->value[2] |= EMV_TVR_CV_PROCESSING_FAILED;

		// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.23
		if (cvm == EMV_CV_RULE_CVM_FAIL) {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.24
			emv_debug_info("Cardholder verification was not successful; Fail CVM processing");
			r = emv_cvm_results_set(
				ctx,
				rule.cvm,
				rule.cvm_cond,
				EMV_CVM_RESULT_FAILED
			);
		} else {
			// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.25
			emv_debug_info("Cardholder verification was not successful; no CVM performed");
			r = emv_cvm_results_set(
				ctx,
				EMV_CVM_NOT_PERFORMED,
				0x00,
				EMV_CVM_RESULT_FAILED
			);
		}
		goto cvm_complete;
	}
	if (r < 0) {
		emv_debug_trace_msg("emv_cvmlist_itr_next() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Last CV Rule always completes CVM Selection and end of CVM list should
	// not be reached
	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.13
	// See EMV Contactless Book C-2 v2.12, 7.1.3, CVM.20
	emv_debug_error("Internal error");
	return EMV_ERROR_INTERNAL;

cvm_complete:
	if (r) {
		emv_debug_trace_msg("emv_cvm_results_set() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}
	ctx->cvm.state = EMV_CVM_STATE_COMPLETE;

	return 0;
}
