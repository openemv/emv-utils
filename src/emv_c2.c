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
#include "emv_tags.h"
#include "emv_fields.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include "crypto_mem.h"
#include "crypto_rand.h"

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

	// Clear existing ICC data and terminal data lists to avoid ambiguity
	emv_tlv_list_clear(&ctx->icc);
	emv_tlv_list_clear(&ctx->terminal);

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

	// NOTE: EMV 4.4 Book 1, 12.4, states that the terminal should set the
	// value of Application Identifier (AID) - terminal (field 9F06) before
	// GET PROCESSING OPTIONS. It is not explicitly stated that PDOL may list
	// 9F06, but the assumption is that the PDOL may list any field having the
	// terminal as the source. Therefore, this implementation will create the
	// initial terminal data fields for the current transaction before PDOL
	// processing and GET PROCESSING OPTIONS. This includes contactless fields
	// like Kernel Identifier - Terminal (field 96).

	// See EMV Contactless Book C-2 v2.11, 6.3.3, S1.9
	r = emv_ep_create_common_kernel_data(ctx, pos_entry_mode);
	if (r) {
		emv_debug_trace_msg("emv_ep_create_common_kernel_data() failed; r=%d", r);
		emv_debug_error("Failed to create initial terminal data");
		return r;
	}

	// Create Terminal Capabilities (field 9F33) from:
	// - Card Data Input Capability (field DF8117)
	// - Security Capability (field DF811F)
	// See EMV Contactless Book C-2 v2.11, 6.3.3, S1.9
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

	// See EMV Contactless Book C-2 v2.11, 6.3.3, S1.13.6
	r = emv_ep_initiate_kernel_processing(ctx, &gpo_list, &gpo_tal_result);
	if (r) {
		emv_debug_trace_msg("emv_ep_initiate_kernel_processing() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (gpo_tal_result) {
		emv_debug_trace_msg("gpo_tal_result=%d", gpo_tal_result);

		switch (gpo_tal_result) {
			// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.4
			case EMV_TAL_ERROR_TTL_FAILURE:
				// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.5
				r = EMV_OUTCOME_CARD_ERROR;
				break;

			// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.8
			case EMV_TAL_RESULT_GPO_DATA_NOT_USABLE:
			case EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED:
			case EMV_TAL_RESULT_GPO_NOT_ALLOWED:
			case EMV_TAL_ERROR_GPO_FAILED:
				// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.9.2
				r = EMV_OUTCOME_SELECT_NEXT;
				break;

			// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.11
			case EMV_TAL_ERROR_GPO_PARSE_FAILED:
				// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.90.2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			default:
				r = EMV_OUTCOME_CARD_ERROR;
				break;
		}

		goto error;
	}

	// Populate AIP pointer
	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.13
	ctx->aip = emv_tlv_list_find_const(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!ctx->aip || ctx->aip->length != 2) {
		emv_debug_error("AIP in GPO response not found or invalid");
		// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.90.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Populate AFL pointer
	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.13
	ctx->afl = emv_tlv_list_find_const(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!ctx->afl || !ctx->afl->length || (ctx->afl->length & 0x3) != 0) {
		emv_debug_error("AFL in GPO response not found or invalid");
		// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.90.2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.16
	if (!(ctx->aip->value[1] & EMV_AIP_EMV_MODE_SUPPORTED)) {
		emv_debug_error("EMV mode not supported in AIP");
		// See EMV Contactless Book C-2 v2.11, 6.5.3, S3.90.2
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
