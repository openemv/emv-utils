/**
 * @file emv_c3.c
 * @brief High level EMV contactless kernel C-3 interface
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

#include "emv_c3.h"
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

int emv_c3_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
)
{
	int r;
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

	// NOTE: EMV 4.4 Book 1, 12.4, states that the terminal should set the
	// value of Application Identifier (AID) - terminal (field 9F06) before
	// GET PROCESSING OPTIONS. It is not explicitly stated that PDOL may list
	// 9F06, but the assumption is that the PDOL may list any field having the
	// terminal as the source. Therefore, this implementation will create the
	// initial terminal data fields for the current transaction before PDOL
	// processing and GET PROCESSING OPTIONS. This includes contactless fields
	// like Kernel Identifier - Terminal (field 96).

	// See EMV Contactless Book C-3 v2.12, 2.1
	r = emv_ep_create_common_kernel_data(ctx, pos_entry_mode);
	if (r) {
		emv_debug_trace_msg("emv_ep_create_common_kernel_data() failed; r=%d", r);
		emv_debug_error("Failed to create initial terminal data");
		return r;
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

	// See EMV Contactless Book C-3 v2.12, 5.2.2
	r = emv_ep_initiate_kernel_processing(ctx, &gpo_list, &gpo_tal_result);
	if (r) {
		emv_debug_trace_msg("emv_ep_initiate_kernel_processing() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (gpo_tal_result) {
		emv_debug_trace_msg("gpo_tal_result=%d", gpo_tal_result);

		switch (gpo_tal_result) {
			// See EMV Contactless Book C-3 v2.12, 4.1.1.2
			case EMV_TAL_ERROR_TTL_FAILURE:
				r = EMV_OUTCOME_CARD_ERROR;
				break;

			// See EMV Contactless Book C-3 v2.12, 5.2.2.2
			case EMV_TAL_RESULT_GPO_DATA_NOT_USABLE:
				r = EMV_OUTCOME_TRY_ANOTHER_INTERFACE;
				break;

			// See EMV Contactless Book C-3 v2.12, 5.2.2.2
			case EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED:
				r = EMV_OUTCOME_SELECT_NEXT;
				break;

			// See EMV Contactless Book C-3 v2.12, 5.2.2.2
			case EMV_TAL_RESULT_GPO_NOT_ALLOWED:
				r = EMV_OUTCOME_TRY_AGAIN_SEE_PHONE;
				break;

			// See EMV Contactless Book C-3 v2.12, 5.2.2.2
			case EMV_TAL_ERROR_GPO_FAILED:
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			// See EMV Contactless Book C-3 v2.12, 4.1.1.4
			default:
				// See EMV Contactless Book C-3 v2.12, 4.2.1.1
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;
		}

		goto error;
	}

	// Populate AIP pointer (if available)
	// See EMV Contactless Book C-3 v2.12, Annex D.1
	ctx->aip = emv_tlv_list_find_const(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (ctx->aip && ctx->aip->length != 2) {
		emv_debug_error("AIP in GPO response is invalid");
		// See EMV Contactless Book C-3 v2.12, 4.2.1.1
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Populate AFL pointer (if available)
	// See EMV Contactless Book C-3 v2.12, Annex D.1
	ctx->afl = emv_tlv_list_find_const(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (ctx->afl && (!ctx->afl->length || (ctx->afl->length & 0x3) != 0)) {
		emv_debug_error("AFL in GPO response is invalid");
		// See EMV Contactless Book C-3 v2.12, 4.2.1.1
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

int emv_c3_read_application_data(struct emv_ctx_t* ctx)
{
	int r;
	struct emv_tlv_list_t record_data = EMV_TLV_LIST_INIT;
	int rr_tal_result;
	bool found_82 = false;
	bool found_9F36 = false;
	bool found_9F26 = false;
	bool found_9F10 = false;
	bool found_57 = false;
	bool found_9F38 = false;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("Read application data");

	// Application File Locator (AFL) is optional for kernel C-3
	if (ctx->afl) {
		// AFL found; read application data
		// See EMV Contactless Book C-3 v2.12, 5.3.2.1
		// See EMV 4.4 Book 3, 10.2
		r = emv_ep_read_application_data(ctx, &record_data, &rr_tal_result);
		if (r) {
			emv_debug_trace_msg("emv_ep_read_application_data() failed; r=%d", r);

			// Return error as-is
			goto error;
		}
		if (rr_tal_result && rr_tal_result != EMV_TAL_RESULT_ODA_RECORD_INVALID) {
			emv_debug_trace_msg("rr_tal_result=%d", rr_tal_result);

			switch (rr_tal_result) {
				// See EMV Contactless Book C-3 v2.12, 4.1.1.2
				case EMV_TAL_ERROR_TTL_FAILURE:
					r = EMV_OUTCOME_CARD_ERROR;
					break;

				// See EMV Contactless Book C-3 v2.12, 4.1.1.4
				case EMV_TAL_ERROR_AFL_INVALID:
				case EMV_TAL_ERROR_READ_RECORD_FAILED:
				case EMV_TAL_ERROR_READ_RECORD_INVALID:
				case EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED:
					// See EMV Contactless Book C-3 v2.12, 4.2.1.1
					r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
					break;

				// See EMV Contactless Book C-3 v2.12, 4.1.1.4
				default:
					// See EMV Contactless Book C-3 v2.12, 4.2.1.1
					r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
					break;
			}

			goto error;
		}

		r = emv_tlv_list_append(&ctx->icc, &record_data);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_ERROR_INTERNAL;
			goto error;
		}
	} else {
		// AFL not found; skip read application data
		// See EMV Contactless Book C-3 v2.12, 5.3.2.1
		emv_debug_info("No AFL; skip read application data");
	}

	// Card Read Complete
	// See EMV Contactless Book C-3 v2.12, 5.4.1.1
	emv_debug_info("Card Read OK");

	for (const struct emv_tlv_t* tlv = ctx->icc.front; tlv != NULL; tlv = tlv->next) {
		// Mandatory data objects
		// See EMV Contactless Book C-3 v2.12, 5.4.2.1
		// See EMV Contactless Book C-3 v2.12, Annex A.2, Table A-1

		// NOTE: Some mandatory data objects are not checked here:
		// - Application Identifier (field 4F) is provided by the PPSE but
		//   those fields are not added to the ICC list
		// - Template/constructed fields are not added to the ICC list

		if (tlv->tag == EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE) {
			found_82 = true;
		}
		if (tlv->tag == EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER) {
			found_9F36 = true;
		}
		if (tlv->tag == EMV_TAG_9F26_APPLICATION_CRYPTOGRAM) {
			found_9F26 = true;
		}
		if (tlv->tag == EMV_TAG_9F10_ISSUER_APPLICATION_DATA) {
			found_9F10 = true;
		}
		if (tlv->tag == EMV_TAG_57_TRACK2_EQUIVALENT_DATA) {
			found_57 = true;
		}
		if (tlv->tag == EMV_TAG_9F38_PDOL) {
			found_9F38 = true;
		}
	}
	if (!found_82 || !found_9F36 || !found_9F26 ||
		!found_9F10 || !found_57 || !found_9F38
	) {
		emv_debug_trace_msg("82=%s, 9F36=%s, 9F26=%s, 9F10=%s, 57=%s, 9F38=%s",
			found_82 ? "found" : "missing",
			found_9F36 ? "found" : "missing",
			found_9F26 ? "found" : "missing",
			found_9F10 ? "found" : "missing",
			found_57 ? "found" : "missing",
			found_9F38 ? "found" : "missing"
		);

		// Mandatory field not found; terminate session
		// See EMV Contactless Book C-3 v2.12, 5.4.2.1
		emv_debug_error("Mandatory field not found");
		// See EMV Contactless Book C-3 v2.12, 4.2.1.1
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	if (emv_tlv_list_has_duplicate(&ctx->icc)) {
		// Redundant primitive data objects are not permitted
		// See EMV Contactless Book C-3 v2.12, 5.4.2.2

		// NOTE: This implementation adds the application selection response
		// fields to the ICC list and therefore deviates from the specification
		// requiring that the redundancy check is for fields read during
		// Initiate Application Processing and Read Application Data.

		emv_debug_error("Application data contains redundant fields");
		// See EMV Contactless Book C-3 v2.12, 4.2.1.1
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
