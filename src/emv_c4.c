/**
 * @file emv_c4.c
 * @brief High level EMV contactless kernel C-4 interface
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

#include "emv_c4.h"
#include "emv.h"
#include "emv_ep.h"
#include "emv_ttl.h"
#include "emv_tal.h"
#include "emv_app.h"
#include "emv_oda.h"
#include "emv_dol.h"
#include "emv_tags.h"
#include "emv_fields.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include "crypto_mem.h"
#include "crypto_rand.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

int emv_c4_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
)
{
	int r;
	const struct emv_tlv_t* cl_reader_caps_config;
	const struct emv_tlv_t* enh_cl_reader_caps_config;
	const struct emv_tlv_t* term_type_config;
	uint8_t cl_reader_caps;
	uint8_t enh_cl_reader_caps[4];
	uint8_t term_type;
	uint8_t term_env;
	const struct emv_tlv_t* pdol;
	struct emv_tlv_t* term_type_modified = NULL;
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

	r = emv_ep_create_common_kernel_data(ctx, pos_entry_mode);
	if (r) {
		emv_debug_trace_msg("emv_ep_create_common_kernel_data() failed; r=%d", r);
		emv_debug_error("Failed to create initial terminal data");
		return r;
	}

	// Retrieve the configured reader capabilities required by pre-PDOL
	// processing
	// See EMV Contactless Book C-4 v2.11, Annex B.1
	cl_reader_caps_config = emv_config_data_get(ctx, EMV_C4_TAG_9F6D_CONTACTLESS_READER_CAPABILITIES);
	if (!cl_reader_caps_config || cl_reader_caps_config->length != 1) {
		emv_debug_error("Contactless Reader Capabilities (9F6D) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	enh_cl_reader_caps_config = emv_config_data_get(ctx, EMV_C4_TAG_9F6E_ENHANCED_CONTACTLESS_READER_CAPABILITIES);
	if (!enh_cl_reader_caps_config || enh_cl_reader_caps_config->length != 4) {
		emv_debug_error("Enhanced Contactless Reader Capabilities (9F6E) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	term_type_config = emv_config_data_get(ctx, EMV_TAG_9F35_TERMINAL_TYPE);
	if (!term_type_config || term_type_config->length != 1) {
		emv_debug_error("Terminal Type (9F35) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	cl_reader_caps = cl_reader_caps_config->value[0];
	memcpy(enh_cl_reader_caps, enh_cl_reader_caps_config->value, sizeof(enh_cl_reader_caps));
	term_type = term_type_config->value[0];

	// Apply pre-PDOL processing
	// See EMV Contactless Book C-4 v2.11, 4.3.1.1
	cl_reader_caps &= ~EMV_C4_CL_READER_CAPS_CVM_REQUIRED;
	enh_cl_reader_caps[2] = 0x00;

	// Apply offline-only pre-PDOL processing
	// See EMV Contactless Book C-4 v2.11, 4.3.1.3
	// See EMV Contactless Book C-4 v2.11, 4.3.1.4
	// See EMV Contactless Book C-4 v2.11, 13.3
	term_env = term_type & EMV_TERM_TYPE_ENV_MASK;
	if (term_env == EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_ONLY ||
		term_env == EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_ONLY
	) {
		emv_debug_info("Reader is offline only");
		enh_cl_reader_caps[2] |= EMV_C4_ENH_CL_READER_CAPS_OFFLINE_ONLY_READER;
	}

	// Create Contactless Reader Capabilities (field 9F6D)
	// See EMV Contactless Book C-4 v2.11, 4.3.3, Table 4-2
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_C4_TAG_9F6D_CONTACTLESS_READER_CAPABILITIES,
		sizeof(cl_reader_caps),
		&cl_reader_caps,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Enhanced Contactless Reader Capabilities (field 9F6E)
	// See EMV Contactless Book C-4 v2.11, 4.3.4, Table 4-4
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_C4_TAG_9F6E_ENHANCED_CONTACTLESS_READER_CAPABILITIES,
		sizeof(enh_cl_reader_caps),
		enh_cl_reader_caps,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Terminal Type - Modified (field 9F35) if necessary
	// See EMV Contactless Book C-4 v2.11, 4.3.3
	// See EMV Contactless Book C-4 v2.11, 4.3.5
	pdol = emv_tlv_list_find_const(&ctx->selected_app->tlv_list, EMV_TAG_9F38_PDOL);
	if (pdol) {
		bool term_type_in_pdol;
		bool enh_cl_reader_caps_in_pdol;

		r = emv_dol_find_tag(
			pdol->value,
			pdol->length,
			EMV_TAG_9F35_TERMINAL_TYPE,
			NULL
		);
		if (r < 0) {
			emv_debug_trace_msg("emv_dol_find_tag() failed; r=%d", r);
			emv_debug_error("Failed to parse PDOL");
			return EMV_OUTCOME_CARD_ERROR;
		}
		term_type_in_pdol = !r;

		r = emv_dol_find_tag(
			pdol->value,
			pdol->length,
			EMV_C4_TAG_9F6E_ENHANCED_CONTACTLESS_READER_CAPABILITIES,
			NULL
		);
		if (r < 0) {
			emv_debug_trace_msg("emv_dol_find_tag() failed; r=%d", r);
			emv_debug_error("Failed to parse PDOL");
			return EMV_OUTCOME_CARD_ERROR;
		}
		enh_cl_reader_caps_in_pdol = !r;

		// See EMV Contactless Book C-4 v2.11, 4.3.3.1
		// See EMV Contactless Book C-4 v2.11, 4.3.5.1
		if (term_type_in_pdol && !enh_cl_reader_caps_in_pdol) {
			uint8_t modified_value;

			// Create Terminal Type - Modified (field 9F35)
			// See EMV Contactless Book C-4 v2.11, 4.3.3, Table 4-3
			modified_value =
				(term_type & (EMV_TERM_TYPE_OPERATIONAL_CONTROL_MASK | EMV_TERM_TYPE_ENV_MASK)) |
				(cl_reader_caps & EMV_C4_CL_READER_CAPS_MASK);
			emv_debug_info("Using Terminal Type - Modified (9F35) 0x%02X", modified_value);
			r = emv_tlv_list_push(
				&ctx->terminal,
				EMV_TAG_9F35_TERMINAL_TYPE,
				1,
				&modified_value,
				0
			);
			if (r) {
				emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

				// Internal error; terminate session
				emv_debug_error("Internal error");
				return EMV_ERROR_INTERNAL;
			}

			// Cache Terminal Type - Modified (field 9F35) to revert after GPO
			term_type_modified = emv_tlv_list_find(&ctx->terminal, EMV_TAG_9F35_TERMINAL_TYPE);
			if (!term_type_modified) {
				emv_debug_error("Terminal Type (9F35) not found");
				return EMV_ERROR_INTERNAL;
			}
		}
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

	// See EMV Contactless Book C-4 v2.11, 4.2.1.1
	// See EMV Contactless Book C-4 v2.11, 4.3.2
	r = emv_ep_initiate_kernel_processing(ctx, &gpo_list, &gpo_tal_result);

	if (r) {
		emv_debug_trace_msg("emv_ep_initiate_kernel_processing() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (gpo_tal_result) {
		emv_debug_trace_msg("gpo_tal_result=%d", gpo_tal_result);

		// NOTE: Kernel C-4 does not specify how GET PROCESSING OPTIONS
		// responses should be interpreted. In addition:
		// - EMV Contactless Book C-4 v2.11, 4.3.6.1 that response data is
		//   formed according to EMV 4.4 Book 3, 6.5.8.4
		// - EMV Contactless Book C-4 v2.11, Glossary states that Select Next
		//   outcome is not used by kernel C-4
		// - There is no mention of Start C in any outcomes
		// Therefore assume that interface errors are card error outcomes, and
		// response errors are End Application - Try Another Card outcomes.

		switch (gpo_tal_result) {
			case EMV_TAL_ERROR_TTL_FAILURE:
				r = EMV_OUTCOME_CARD_ERROR;
				break;

			case EMV_TAL_RESULT_GPO_DATA_NOT_USABLE:
			case EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED:
			case EMV_TAL_RESULT_GPO_NOT_ALLOWED:
			case EMV_TAL_ERROR_GPO_FAILED:
			case EMV_TAL_ERROR_GPO_PARSE_FAILED:
				// See EMV Contactless Book C-4 v2.11, Table 11-2
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			default:
				r = EMV_OUTCOME_CARD_ERROR;
				break;
		}

		goto error;
	}

	// Revert Terminal Type (field 9F35) if necessary
	// See EMV Contactless Book C-4 v2.11, 4.3.3
	if (term_type_modified) {
		term_type_modified->value[0] = term_type;
	}

	// Populate AIP pointer
	// See EMV Contactless Book C-4 v2.11, 4.3.6.1
	ctx->aip = emv_tlv_list_find_const(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!ctx->aip || ctx->aip->length != 2) {
		emv_debug_error("AIP in GPO response not found or invalid");
		// See EMV Contactless Book C-4 v2.11, Annex A.1
		// See EMV Contactless Book C-4 v2.11, Table 11-2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Populate AFL pointer
	// See EMV Contactless Book C-4 v2.11, 4.3.6.1
	ctx->afl = emv_tlv_list_find_const(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!ctx->afl || !ctx->afl->length || (ctx->afl->length & 0x3) != 0) {
		emv_debug_error("AFL in GPO response not found or invalid");
		// See EMV Contactless Book C-4 v2.11, Annex A.1
		// See EMV Contactless Book C-4 v2.11, Table 11-2
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	// Enforce contactless EMV mode
	// See EMV Contactless Book C-4 v2.11, 2.1.1
	// See EMV Contactless Book C-4 v2.11, 4.3.7
	if (!(ctx->aip->value[1] & EMV_AIP_EMV_MODE_SUPPORTED)) {
		emv_debug_info("Contactless EMV mode not supported by card");

		if ((enh_cl_reader_caps[0] & EMV_C4_ENH_CL_READER_CAPS_CONTACT_SUPPORTED) &&
			!(ctx->aip->value[1] & EMV_AIP_MOBILE_PHONE) &&
			!(ctx->aip->value[1] & EMV_AIP_HCE_SUPPORTED)
		) {
			// See EMV Contactless Book C-4 v2.11, 4.3.7.2
			// See EMV Contactless Book C-4 v2.11, Table 11-1
			r = EMV_OUTCOME_TRY_ANOTHER_INTERFACE;
		} else {
			// See EMV Contactless Book C-4 v2.11, 4.3.7.3
			// See EMV Contactless Book C-4 v2.11, Table 11-2
			r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		}

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

int emv_c4_read_application_data(struct emv_ctx_t* ctx)
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

	// See EMV Contactless Book C-4 v2.11, 5.3.1
	r = emv_ep_read_application_data(ctx, &record_data, &rr_tal_result);
	if (r) {
		emv_debug_trace_msg("emv_ep_read_application_data() failed; r=%d", r);

		// Return error as-is
		goto error;
	}
	if (rr_tal_result && rr_tal_result != EMV_TAL_RESULT_ODA_RECORD_INVALID) {
		emv_debug_trace_msg("rr_tal_result=%d", rr_tal_result);

		// NOTE: Kernel C-4 does not specify how READ RECORD response should
		// interpreted. Therefore assume that interface errors are card error
		// outcomes, and response errors are End Application - Try Another Card
		// outcomes.

		switch (rr_tal_result) {
			case EMV_TAL_ERROR_TTL_FAILURE:
				r = EMV_OUTCOME_CARD_ERROR;
				break;

			case EMV_TAL_ERROR_AFL_INVALID:
			case EMV_TAL_ERROR_READ_RECORD_FAILED:
			case EMV_TAL_ERROR_READ_RECORD_INVALID:
			case EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED:
				// See EMV Contactless Book C-4 v2.11, 5.3.5
				r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
				break;

			default:
				r = EMV_OUTCOME_CARD_ERROR;
				break;
		}

		goto error;
	}

	if (emv_tlv_list_has_duplicate(&record_data)) {
		// Redundant primitive data objects are not permitted
		// See EMV Contactless Book C-4 v2.11, 5.3.2
		emv_debug_error("Application data contains redundant fields");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto error;
	}

	for (const struct emv_tlv_t* tlv = record_data.front; tlv != NULL; tlv = tlv->next) {
		// Mandatory data objects
		// See EMV Contactless Book C-4 v2.11, 5.3.3
		// See EMV Contactless Book C-4 v2.11, Annex A.3, Table 14-3
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
		// See EMV Contactless Book C-4 v2.11, 5.3.3
		emv_debug_error("Mandatory field not found");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
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

	// Success
	r = 0;
	goto exit;

error:
	emv_oda_clear(&ctx->oda);
exit:
	emv_tlv_list_clear(&record_data);
	return r;
}
