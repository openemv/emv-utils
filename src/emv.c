/**
 * @file emv.c
 * @brief High level EMV library interface
 *
 * Copyright 2023-2025 Leon Lynch
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

#include "emv.h"
#include "emv_utils_config.h"
#include "emv_ttl.h"
#include "emv_tal.h"
#include "emv_app.h"
#include "emv_dol.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_oda.h"
#include "emv_date.h"

#include "iso7816.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include "crypto_mem.h"
#include "crypto_rand.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

const char* emv_lib_version_string(void)
{
	return EMV_UTILS_VERSION_STRING;
}

const char* emv_error_get_string(enum emv_error_t error)
{
	switch (error) {
		case EMV_ERROR_INTERNAL: return "Internal error";
		case EMV_ERROR_INVALID_PARAMETER: return "Invalid function parameter";
		case EMV_ERROR_INVALID_CONFIG: return "Invalid configuration";
	}

	return "Unknown error";
}

int emv_ctx_init(struct emv_ctx_t* ctx, struct emv_ttl_t* ttl)
{
	if (!ctx || !ttl) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->ttl = ttl;

	return 0;
}

int emv_ctx_reset(struct emv_ctx_t* ctx)
{
	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_tlv_list_clear(&ctx->params);
	emv_tlv_list_clear(&ctx->icc);
	emv_tlv_list_clear(&ctx->terminal);
	emv_app_free(ctx->selected_app);
	ctx->selected_app = NULL;
	emv_oda_clear(&ctx->oda);

	ctx->aid = NULL;
	ctx->tvr = NULL;
	ctx->tsi = NULL;
	ctx->aip = NULL;
	ctx->afl = NULL;

	return 0;
}

int emv_ctx_clear(struct emv_ctx_t* ctx)
{
	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	ctx->ttl = NULL;
	emv_tlv_list_clear(&ctx->config);
	emv_tlv_list_clear(&ctx->supported_aids);
	emv_ctx_reset(ctx);

	return 0;
}

const char* emv_outcome_get_string(enum emv_outcome_t outcome)
{
	// See EMV 4.4 Book 4, 11.2, table 8
	switch (outcome) {
		case EMV_OUTCOME_CARD_ERROR: return "Card error"; // Message 06
		case EMV_OUTCOME_CARD_BLOCKED: return "Card blocked"; // Not in EMV specification
		case EMV_OUTCOME_NOT_ACCEPTED: return "Not accepted"; // Message 0C
		case EMV_OUTCOME_TRY_AGAIN: return "Try again"; // Message 13
		case EMV_OUTCOME_GPO_NOT_ACCEPTED: return "Not accepted"; // Message 0C
	}

	return "Invalid outcome";
}

int emv_atr_parse(const void* atr, size_t atr_len)
{
	int r;
	struct iso7816_atr_info_t atr_info;
	unsigned int TD1_protocol = 0; // Default is T=0
	unsigned int TD2_protocol = 0; // Default is T=0

	if (!atr || !atr_len) {
		emv_debug_trace_msg("atr=%p, atr_len=%zu", atr, atr_len);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		emv_debug_trace_msg("iso7816_atr_parse() failed; r=%d", r);

		if (r < 0) {
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;
		}
		if (r > 0) {
			emv_debug_error("Failed to parse ATR");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}
	emv_debug_atr_info(&atr_info);

	// The intention of this function is to validate the ATR in accordance with
	// EMV Level 1 Contact Interface Specification v1.0, 8.3. Some of the
	// validation may already be performed by iso7816_atr_parse() and should be
	// noted below in comments. The intention is also not to limit this
	// function to only the "basic ATR", but instead to allow all possible ATRs
	// that are allowed by the specification.

	// TS - Initial character
	// See EMV Level 1 Contact Interface v1.0, 8.3.1
	// Validated by iso7816_atr_parse()

	// T0 - Format character
	// See EMV Level 1 Contact Interface v1.0, 8.3.2
	// Validated by iso7816_atr_parse()

	// TA1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.1
	if (atr_info.TA[1]) { // TA1 is present
		if (atr_info.TA[2] && // TA2 is present
			(*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) == 0 && // Specific mode
			(*atr_info.TA[1] < 0x11 || *atr_info.TA[1] > 0x13) // TA1 must be in the range 0x11 to 0x13
		) {
			emv_debug_error("TA2 indicates specific mode but TA1 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		if (!atr_info.TA[2]) { // TA2 is absent
			// Max frequency must be at least 5 MHz
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_FI_MASK) == 0) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// Baud rate adjustment factor must be at least 4
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_DI_MASK) < 3) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// TB1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.2
	// Validated by iso7816_atr_parse()

	// TC1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.3
	if (atr_info.TC[1]) { // TC1 is present
		// TC1 must be either 0x00 or 0xFF
		if (*atr_info.TC[1] != 0x00 && *atr_info.TC[1] != 0xFF) {
			emv_debug_error("TC1 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TD1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.4
	if (atr_info.TD[1]) { // TD1 is present
		// TD1 protocol type must be T=0 or T=1
		if ((*atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK) > ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD1 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
		TD1_protocol = *atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK;
	}

	// TA2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.5
	if (atr_info.TA[2]) { // TA2 is present
		unsigned int TA2_protocol;

		// TA2 protocol must be the same as the first indicated protocol
		TA2_protocol = *atr_info.TA[2] & ISO7816_ATR_TA2_PROTOCOL_MASK;
		if (TA2_protocol != TD1_protocol) {
			emv_debug_error("TA2 protocol differs from TD1 protocol");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TA2 must indicate specific mode, not implicit mode
		if (*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) {
			emv_debug_error("TA2 implicit mode is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TB2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.6
	// Validated by iso7816_atr_parse()

	// TC2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.7
	if (atr_info.TC[2]) { // TC2 is present
		// TC2 is specific to T=0
		if (TD1_protocol != ISO7816_PROTOCOL_T0) {
			emv_debug_error("TC2 is not allowed when protocol is not T=0");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TC2 for T=0 must be 0x0A
		if (*atr_info.TC[2] != 0x0A) {
			emv_debug_error("TC2 for T=0 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TD2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.8
	if (atr_info.TD[2]) { // TD2 is present
		// TD2 protocol type must be T=15 if TD1 protocol type was T=0
		if (TD1_protocol == ISO7816_PROTOCOL_T0 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T15
		) {
			emv_debug_error("TD2 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TD2 protocol type must be T=1 if TD1 protocol type was T=1
		if (TD1_protocol == ISO7816_PROTOCOL_T1 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T1
		) {
			emv_debug_error("TD2 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		TD2_protocol = *atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK;

	} else { // TD2 is absent
		// TB3, and therefore TD2, must be present for T=1
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (TD1_protocol == ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD2 for T=1 is absent");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// T=1 Interface Characters
	if (TD2_protocol == ISO7816_PROTOCOL_T1) {
		// TA3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.9
		if (atr_info.TA[3]) { // TA3 is present
			// TA3 for T=1 must be in the range 0x10 to 0xFE
			// iso7816_atr_parse() already rejects 0xFF
			if (*atr_info.TA[3] < 0x10) {
				emv_debug_error("TA3 for T=1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}

		// TB3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (atr_info.TB[3]) { // TB3 is present
			// TB3 for T=1 BWI must be 4 or less
			if (((*atr_info.TB[3] & ISO7816_ATR_TBi_BWI_MASK) >> ISO7816_ATR_TBi_BWI_SHIFT) > 4) {
				emv_debug_error("TB3 for T=1 has invalid BWI");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// TB3 for T=1 CWI must be 5 or less
			if ((*atr_info.TB[3] & ISO7816_ATR_TBi_CWI_MASK) > 5) {
				emv_debug_error("TB3 for T=1 has invalid CWI");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// For T=1, reject 2^CWI < (N + 1)
			// - if N==0xFF, consider N to be -1
			// - if N==0x00, consider CWI to be 1
			// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
			int N = (atr_info.global.N != 0xFF) ? atr_info.global.N : -1;
			unsigned int CWI = atr_info.global.N ? atr_info.protocol_T1.CWI : 1;
			unsigned int pow_2_CWI = 1 << CWI;
			if (pow_2_CWI < (N + 1)) {
				emv_debug_error("2^CWI < (N + 1) for T=1 is not allowed");
				return EMV_OUTCOME_CARD_ERROR;
			}

		} else { // TB3 is absent
			emv_debug_error("TB3 for T=1 is absent");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TC3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.11
		if (atr_info.TC[3]) { // TC3 is present
			// TC for T=1 must be 0x00
			if (*atr_info.TC[3] != 0x00) {
				emv_debug_error("TC3 for T=1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// TCK - Check Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.4
	// Validated by iso7816_atr_parse()

	return 0;
}

int emv_build_candidate_list(
	const struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list
)
{
	int r;

	if (!ctx || !app_list) {
		emv_debug_trace_msg("ctx=%p, app_list=%p", ctx, app_list);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("SELECT Payment System Environment (PSE)");
	r = emv_tal_read_pse(ctx->ttl, &ctx->supported_aids, app_list);
	if (r < 0) {
		emv_debug_trace_msg("emv_tal_read_pse() failed; r=%d", r);
		emv_debug_error("Failed to read PSE; terminate session");
		if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
			return EMV_OUTCOME_CARD_BLOCKED;
		} else {
			return EMV_OUTCOME_CARD_ERROR;
		}
	}
	if (r > 0) {
		emv_debug_trace_msg("emv_tal_read_pse() failed; r=%d", r);
		emv_debug_info("Failed to process PSE; continue session");
	}

	// If PSE failed or no apps found by PSE, use list of AIDs method
	// See EMV 4.4 Book 1, 12.3.2, step 5
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Discover list of AIDs");
		r = emv_tal_find_supported_apps(ctx->ttl, &ctx->supported_aids, app_list);
		if (r) {
			emv_debug_trace_msg("emv_tal_find_supported_apps() failed; r=%d", r);
			emv_debug_error("Failed to find supported AIDs; terminate session");
			if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
				return EMV_OUTCOME_CARD_BLOCKED;
			} else {
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// If there are no mutually supported applications, terminate session
	// See EMV 4.4 Book 1, 12.4, step 1
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Candidate list empty");
		return EMV_OUTCOME_NOT_ACCEPTED;
	}

	// Sort application list according to priority
	// See EMV 4.4 Book 1, 12.4, step 4
	r = emv_app_list_sort_priority(app_list);
	if (r) {
		emv_debug_trace_msg("emv_app_list_sort_priority() failed; r=%d", r);
		emv_debug_error("Failed to sort application list; terminate session");
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

int emv_select_application(
	struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list,
	unsigned int index
)
{
	int r;
	struct emv_app_t* current_app = NULL;
	uint8_t current_aid[16];
	size_t current_aid_len;

	if (!ctx || !app_list) {
		emv_debug_trace_msg("ctx=%p, app_list=%p, index=%u", ctx, app_list, index);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (ctx->selected_app) {
		// Free any previously selected app and ensure that it succeeds
		r = emv_app_free(ctx->selected_app);
		if (r) {
			emv_debug_trace_msg("emv_app_free() failed; r=%d", r);
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;

		}
		ctx->selected_app = NULL;
	}

	current_app = emv_app_list_remove_index(app_list, index);
	if (!current_app) {
		emv_debug_trace_msg("emv_app_list_remove_index() failed; index=%u", index);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (current_app->aid->length > sizeof(current_aid)) {
		goto try_again;
	}
	current_aid_len = current_app->aid->length;
	memcpy(current_aid, current_app->aid->value, current_app->aid->length);
	emv_app_free(current_app);
	current_app = NULL;

	r = emv_tal_select_app(
		ctx->ttl,
		current_aid,
		current_aid_len,
		&ctx->selected_app
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_select_app() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Error during application selection; terminate session");
			if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
				r = EMV_OUTCOME_CARD_BLOCKED;
			} else {
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto exit;
		}
		if (r > 0) {
			emv_debug_info("Failed to select application; continue session");
			goto try_again;
		}
	}

	// Success
	r = 0;
	goto exit;

try_again:
	// If no applications remain, terminate session
	// Otherwise, try again
	// See EMV 4.4 Book 1, 12.4
	// See EMV 4.4 Book 4, 11.3
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Candidate list empty");
		r = EMV_OUTCOME_NOT_ACCEPTED;
	} else {
		r = EMV_OUTCOME_TRY_AGAIN;
	}

exit:
	if (current_app) {
		emv_app_free(current_app);
		current_app = NULL;
	}

	return r;
}

int emv_initiate_application_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
)
{
	int r;
	uint8_t un[4];
	const struct emv_tlv_t* pdol;
	uint8_t gpo_data_buf[EMV_CAPDU_DATA_MAX];
	uint8_t* gpo_data;
	size_t gpo_data_len;
	struct emv_tlv_list_t gpo_output = EMV_TLV_LIST_INIT;

	if (!ctx || !ctx->selected_app) {
		emv_debug_trace_msg("ctx=%p, selected_app=%p", ctx, ctx->selected_app);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Clear existing ICC data and terminal data lists to avoid ambiguity
	emv_tlv_list_clear(&ctx->icc);
	emv_tlv_list_clear(&ctx->terminal);

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
	// processing and GET PROCESSING OPTIONS.

	// Create Point-of-Service (POS) Entry Mode (field 9F39)
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9F39_POS_ENTRY_MODE,
		1,
		&pos_entry_mode,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Application Identifier (AID) - terminal (field 9F06)
	// See EMV 4.4 Book 1, 12.4
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9F06_AID,
		ctx->selected_app->aid->length,
		ctx->selected_app->aid->value,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Transaction Status Information (TSI, field 9B)
	// See EMV 4.4 Book 3, 10.1
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION,
		2,
		(uint8_t[]){ 0x00, 0x00 },
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Terminal Verification Results (TVR, field 95)
	// See EMV 4.4 Book 3, 10.1
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS,
		5,
		(uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 },
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Create Unpredictable Number (field 9F37)
	// See EMV 4.4 Book 4, 6.5.6
	crypto_rand(un, sizeof(un));
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9F37_UNPREDICTABLE_NUMBER,
		sizeof(un),
		un,
		0
	);
	crypto_cleanse(un, sizeof(un));
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	// Cache various terminal fields
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

	// Process PDOL, if available
	// See EMV 4.4 Book 3, 10.1
	pdol = emv_tlv_list_find_const(&ctx->selected_app->tlv_list, EMV_TAG_9F38_PDOL);
	if (pdol) {
		struct emv_tlv_sources_t sources = EMV_TLV_SOURCES_INIT;
		int pdol_data_len;
		size_t pdol_data_offset;

		emv_debug_info_data("PDOL found", pdol->value, pdol->length);

		// Prepare ordered data sources
		sources.count = 3;
		sources.list[0] = &ctx->params;
		sources.list[1] = &ctx->config;
		sources.list[2] = &ctx->terminal;

		// Validate PDOL data length
		pdol_data_len = emv_dol_compute_data_length(pdol->value, pdol->length);
		if (pdol_data_len < 0) {
			emv_debug_trace_msg("emv_dol_compute_data_length() failed; pdol_data_len=%d", pdol_data_len);
			emv_debug_error("Failed to compute PDOL data length");
			return EMV_OUTCOME_CARD_ERROR;
		}
		if (pdol_data_len > sizeof(ctx->oda.pdol_data)) {
			emv_debug_error("Invalid PDOL data length of %u", pdol_data_len);
			return EMV_OUTCOME_CARD_ERROR;
		}

		// Prepare GPO data buffer with field 83
		gpo_data = gpo_data_buf;
		gpo_data[0] = EMV_TAG_83_COMMAND_TEMPLATE;
		pdol_data_offset = 1;
		if (pdol_data_len < 0x80) {
			// Short length form
			gpo_data[1] = pdol_data_len;
			pdol_data_offset += 1;
		} else {
			// Long length form
			gpo_data[1] = 0x81;
			gpo_data[2] = pdol_data_len;
			pdol_data_offset += 2;
		}

		// Populate PDOL data in cache buffer
		ctx->oda.pdol_data_len = sizeof(ctx->oda.pdol_data);
		r = emv_dol_build_data(
			pdol->value,
			pdol->length,
			&sources,
			ctx->oda.pdol_data,
			&ctx->oda.pdol_data_len
		);
		if (r) {
			emv_debug_trace_msg("emv_dol_build_data() failed; r=%d", r);
			emv_debug_error("Failed to build PDOL data");

			// This is considered an internal error because the PDOL has
			// already been sucessfully parsed and the PDOL data length is
			// already known not to exceed the GPO data buffer.
			return EMV_ERROR_INTERNAL;
		}

		// Finalise GPO data buffer
		if (ctx->oda.pdol_data_len > sizeof(gpo_data_buf) - pdol_data_offset) {
			emv_debug_error("Error during PDOL processing; "
				"pdol_data_len=%zu; sizeof(gpo_data_buf)=%zu; pdol_data_offset=%zu",
				ctx->oda.pdol_data_len, sizeof(gpo_data_buf), pdol_data_offset
			);
			return EMV_ERROR_INTERNAL;
		}
		memcpy(gpo_data + pdol_data_offset, ctx->oda.pdol_data, ctx->oda.pdol_data_len);
		gpo_data_len = pdol_data_offset + ctx->oda.pdol_data_len;

	} else {
		// PDOL not available. emv_ttl_get_processing_options() will build
		// empty Command Template (field 83) if no GPO data is provided
		gpo_data = NULL;
		gpo_data_len = 0;
	}

	r = emv_tal_get_processing_options(
		ctx->ttl,
		gpo_data,
		gpo_data_len,
		&gpo_output,
		&ctx->aip,
		&ctx->afl
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_get_processing_options() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Error during application processing; terminate session");

			if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
				r = EMV_ERROR_INTERNAL;
			} else {
				// All other GPO errors are card errors
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto error;
		}
		if (r > 0) {
			emv_debug_info("Failed to initiate application processing");

			if (r == EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED) {
				// Conditions of use not satisfied; ignore app and continue
				// See EMV 4.4 Book 3, 10.1
				// See EMV 4.4 Book 4, 6.3.1
				r = EMV_OUTCOME_GPO_NOT_ACCEPTED;
			} else {
				// All other GPO outcomes are card errors
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto error;
		}
	}

	// Move application data to ICC data list
	ctx->icc = ctx->selected_app->tlv_list;
	ctx->selected_app->tlv_list = EMV_TLV_LIST_INIT;

	// Append GPO output to ICC data list
	r = emv_tlv_list_append(&ctx->icc, &gpo_output);
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
	emv_tlv_list_clear(&gpo_output);
exit:
	return r;
}

int emv_read_application_data(struct emv_ctx_t* ctx)
{
	int r;
	struct emv_tlv_list_t record_data = EMV_TLV_LIST_INIT;
	bool found_5F24 = false;
	bool found_5A = false;
	bool found_8C = false;
	bool found_8D = false;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Application File Locator (AFL) is required to read application records
	if (!ctx->afl) {
		// AFL not found; terminate session
		// See EMV 4.4 Book 3, 6.5.8.4
		emv_debug_error("AFL not found");
		return EMV_OUTCOME_CARD_ERROR;
	}

	// Ensure that Offline Data Authentication (ODA) context is ready when
	// reading application records
	r = emv_oda_prepare_records(&ctx->oda, ctx->afl->value, ctx->afl->length);
	if (r) {
		emv_debug_trace_msg("emv_oda_prepare_records() failed; r=%d", r);

		if (r == EMV_ODA_ERROR_INTERNAL ||
			r == EMV_ODA_ERROR_INVALID_PARAMETER
		) {
			// Internal error; terminate session
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;
		} else {
			// All other ODA errors are card errors
			emv_debug_error("Invalid ICC data during ODA initialisation");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// Process Application File Locator (AFL)
	// See EMV 4.4 Book 3, 10.2
	r = emv_tal_read_afl_records(
		ctx->ttl,
		ctx->afl->value,
		ctx->afl->length,
		&record_data,
		&ctx->oda
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_read_afl_records() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Error reading application data");
			if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
				r = EMV_ERROR_INTERNAL;
			} else {
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto error;
		}
		if (r != EMV_TAL_RESULT_ODA_RECORD_INVALID) {
			emv_debug_error("Failed to read application data");
			r = EMV_OUTCOME_CARD_ERROR;
			goto error;
		}
		// Continue regardless of offline data authentication failure
		// See EMV 4.4 Book 3, 10.3 (page 98)
	}

	if (emv_tlv_list_has_duplicate(&record_data)) {
		// Redundant primitive data objects are not permitted
		// See EMV 4.4 Book 3, 10.2
		emv_debug_error("Application data contains redundant fields");
		r = EMV_OUTCOME_CARD_ERROR;
		goto error;
	}

	for (const struct emv_tlv_t* tlv = record_data.front; tlv != NULL; tlv = tlv->next) {
		// Mandatory data objects
		// See EMV 4.4 Book 3, 7.2
		if (tlv->tag == EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE) {
			found_5F24 = true;
		}
		if (tlv->tag == EMV_TAG_5A_APPLICATION_PAN) {
			found_5A = true;
		}
		if (tlv->tag == EMV_TAG_8C_CDOL1) {
			found_8C = true;
		}
		if (tlv->tag == EMV_TAG_8D_CDOL2) {
			found_8D = true;
		}
	}
	if (!found_5F24 || !found_5A || !found_8C || !found_8D) {
		// Mandatory field not found; terminate session
		// See EMV 4.4 Book 3, 10.2
		emv_debug_error("Mandatory field not found");
		r = EMV_OUTCOME_CARD_ERROR;
		goto error;
	}

	r = emv_tlv_list_append(&ctx->icc, &record_data);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_TAL_ERROR_INTERNAL;
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

int emv_offline_data_authentication(struct emv_ctx_t* ctx)
{
	int r;
	const struct emv_tlv_t* term_caps;
	const struct emv_tlv_t* default_ddol;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Ensure mandatory configuration fields are present and have valid length
	term_caps = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F33_TERMINAL_CAPABILITIES);
	if (!term_caps || term_caps->length != 3) {
		emv_debug_trace_msg("term_caps=%p, term_caps->length=%u",
			term_caps, term_caps ? term_caps->length : 0);
		emv_debug_error("Terminal Capabilities (9F33) not found or invalid");
		r = EMV_ERROR_INVALID_CONFIG;
		goto exit;
	}
	default_ddol = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F49_DDOL);
	if (!default_ddol || default_ddol->length < 2) {
		emv_debug_trace_msg("default_ddol=%p, default_ddol->length=%u",
			default_ddol, default_ddol ? default_ddol->length : 0);
		emv_debug_error("Default DDOL (9F49) not found or invalid");
		r = EMV_ERROR_INVALID_CONFIG;
		goto exit;
	}

	r = emv_oda_apply(ctx, term_caps->value);
	if (r) {
		if (r < 0) {
			emv_debug_trace_msg("emv_oda_apply() failed; r=%d", r);
			emv_debug_error("Error during offline data authentication");
			if (r == EMV_ODA_ERROR_INTERNAL || r == EMV_ODA_ERROR_INVALID_PARAMETER) {
				r = EMV_ERROR_INTERNAL;
			} else {
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto exit;
		}

		// Otherwise session may continue although offline data authentication
		// was not possible or has failed.
		if (r == EMV_ODA_NO_SUPPORTED_METHOD) {
			emv_debug_info("Offline data authentication was not performed");
		} else {
			emv_debug_error("Offline data authentication failed");
		}
	}

	// Success
	r = 0;
	goto exit;

exit:
	// Clear only records because they are no longer needed and contain
	// sensitive card data.
	emv_oda_clear_records(&ctx->oda);
	return r;
}

int emv_processing_restrictions(struct emv_ctx_t* ctx)
{
	const struct emv_tlv_t* term_app_version;
	const struct emv_tlv_t* term_type;
	const struct emv_tlv_t* addl_term_caps;
	const struct emv_tlv_t* term_country_code;
	const struct emv_tlv_t* txn_type;
	const struct emv_tlv_t* txn_date;
	const struct emv_tlv_t* app_version;
	const struct emv_tlv_t* auc;
	const struct emv_tlv_t* issuer_country_code;
	const struct emv_tlv_t* app_effective_date;
	const struct emv_tlv_t* app_expiration_date;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->tvr) {
		emv_debug_trace_msg("tvr=%p", ctx->tvr);
		emv_debug_error("Invalid context variable");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Ensure mandatory configuration fields are present and have valid length
	term_app_version = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL);
	if (!term_app_version || term_app_version->length != 2) {
		emv_debug_trace_msg("term_app_version=%p, term_app_version->length=%u",
			term_app_version, term_app_version ? term_app_version->length : 0);
		emv_debug_error("Application Version Number - terminal (9F09) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	term_type = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F35_TERMINAL_TYPE);
	if (!term_type || term_type->length != 1) {
		emv_debug_trace_msg("term_type=%p, term_type->length=%u",
			term_type, term_type ? term_type->length : 0);
		emv_debug_error("Terminal Type (9F35) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	addl_term_caps = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES);
	if (!addl_term_caps || addl_term_caps->length != 5) {
		emv_debug_trace_msg("addl_term_caps=%p, addl_term_caps->length=%u",
			addl_term_caps, addl_term_caps ? addl_term_caps->length : 0);
		emv_debug_error("Additional Terminal Capabilities (9F40) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}
	term_country_code = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE);
	if (!term_country_code || term_country_code->length != 2) {
		emv_debug_trace_msg("term_country_code=%p, term_country_code->length=%u",
			term_country_code, term_country_code ? term_country_code->length : 0);
		emv_debug_error("Terminal Country Code (9F1A) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}

	// Ensure mandatory transaction parameters are present and have valid length
	txn_type = emv_tlv_list_find_const(&ctx->params, EMV_TAG_9C_TRANSACTION_TYPE);
	if (!txn_type || txn_type->length != 1) {
		emv_debug_trace_msg("txn_type=%p, txn_type->length=%u",
			txn_type, txn_type ? txn_type->length : 0);
		emv_debug_error("Transaction Type (9C) not found or invalid");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	txn_date = emv_tlv_list_find_const(&ctx->params, EMV_TAG_9A_TRANSACTION_DATE);
	if (!txn_date || txn_date->length != 3) {
		emv_debug_trace_msg("txn_date=%p, txn_date->length=%u",
			txn_date, txn_date ? txn_date->length : 0);
		emv_debug_error("Transaction Date (9A) not found or invalid");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Check compatibility of Application Version Number (field 9F08)
	// See EMV 4.4 Book 3, 10.4.1
	app_version = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F08_APPLICATION_VERSION_NUMBER);
	if (app_version) {
		if (app_version->length != term_app_version->length ||
			memcmp(app_version->value, term_app_version->value, term_app_version->length) != 0
		) {
			emv_debug_info("ICC and terminal have different application versions");
			ctx->tvr->value[1] |= EMV_TVR_APPLICATION_VERSIONS_DIFFERENT;
		} else {
			emv_debug_info("ICC and terminal application versions match");
			ctx->tvr->value[1] &= ~EMV_TVR_APPLICATION_VERSIONS_DIFFERENT;
		}
	} else {
		// If not present, assume compatible and continue
		emv_debug_trace_msg("Application Version Number (9F08) not found");
		ctx->tvr->value[1] &= ~EMV_TVR_APPLICATION_VERSIONS_DIFFERENT;
	}

	// Check compatibility of Application Usage Control (field 9F07)
	// See EMV 4.4 Book 3, 10.4.2
	auc = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F07_APPLICATION_USAGE_CONTROL);
	if (auc) {
		// Determine whether terminal is an ATM
		// See EMV 4.4 Book 4, Annex A1
		if (
			(
				term_type->value[0] == 0x14 ||
				term_type->value[0] == 0x15 ||
				term_type->value[0] == 0x16
			) &&
			(addl_term_caps->value[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH)
		) {
			// Terminal is an ATM
			if ((auc->value[0] & EMV_AUC_ATM) == 0) {
				emv_debug_info("Terminal is ATM but AUC is not valid at ATM ");
				ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
			}
		} else {
			// Terminal is non-ATM
			if ((auc->value[0] & EMV_AUC_NON_ATM) == 0) {
				emv_debug_info("Terminal is non-ATM but AUC is not valid at non-ATM ");
				ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
			}
		}

		// Transaction type checks require both AUC and issuer country code to be present
		issuer_country_code = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_5F28_ISSUER_COUNTRY_CODE);
		if (issuer_country_code) {
			// Determine whether it is a domestic transaction
			bool domestic = false;
			if (issuer_country_code->length == term_country_code->length &&
				memcmp(issuer_country_code->value, term_country_code->value, term_country_code->length) == 0
			) {
				domestic = true;
			}

			// Determine whether transaction type is allowed
			// See EMV 4.4 Book 3, table 36
			switch (txn_type->value[0]) {
				case EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES:
					if (domestic &&
						(auc->value[0] & (EMV_AUC_DOMESTIC_GOODS | EMV_AUC_DOMESTIC_SERVICES)) == 0
					) {
						emv_debug_info("AUC does not allow domestic goods/services transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					if (!domestic &&
						(auc->value[0] & (EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_INTERNATIONAL_SERVICES)) == 0
					) {
						emv_debug_info("AUC does not allow international goods/services transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					break;

				case EMV_TRANSACTION_TYPE_CASH:
					if (domestic &&
						(auc->value[0] & EMV_AUC_DOMESTIC_CASH) == 0
					) {
						emv_debug_info("AUC does not allow domestic cash transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					if (!domestic &&
						(auc->value[0] & EMV_AUC_INTERNATIONAL_CASH) == 0
					) {
						emv_debug_info("AUC does not allow international cash transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					break;

				case EMV_TRANSACTION_TYPE_CASHBACK:
					if (domestic &&
						(auc->value[1] & EMV_AUC_DOMESTIC_CASHBACK) == 0
					) {
						emv_debug_info("AUC does not allow domestic cashback transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					if (!domestic &&
						(auc->value[1] & EMV_AUC_INTERNATIONAL_CASHBACK) == 0
					) {
						emv_debug_info("AUC does not allow international cashback transaction");
						ctx->tvr->value[1] |= EMV_TVR_SERVICE_NOT_ALLOWED;
					}
					break;
			}

			if ((ctx->tvr->value[1] & EMV_TVR_SERVICE_NOT_ALLOWED) == 0) {
				emv_debug_info("Service allowed");
			}
		}

	} else {
		// If not present, assume compatible and continue
		emv_debug_trace_msg("Application Usage Control (9F07) not found");
		ctx->tvr->value[1] &= ~EMV_TVR_SERVICE_NOT_ALLOWED;
	}

	// Check validity of Application Effective Date (field 5F25)
	// See EMV 4.4 Book 3, 10.4.3
	app_effective_date = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE);
	if (app_effective_date) {
		if (emv_date_is_not_effective(txn_date, app_effective_date)) {
			emv_debug_info("Application is not yet effective");
			ctx->tvr->value[1] |= EMV_TVR_APPLICATION_NOT_EFFECTIVE;
		} else {
			emv_debug_info("Application is effective");
			ctx->tvr->value[1] &= ~EMV_TVR_APPLICATION_NOT_EFFECTIVE;
		}
	} else {
		// If not present, assume valid and continue
		emv_debug_trace_msg("Application Effective Date (5F25) not found");
		ctx->tvr->value[1] &= ~EMV_TVR_APPLICATION_NOT_EFFECTIVE;
	}

	// Check validity of Application Expiration Date (field 5F24)
	// See EMV 4.4 Book 3, 10.4.3
	app_expiration_date = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE);
	if (!app_expiration_date) {
		// Presence of Application Expiration Date (field 5F24) should have
		// been confirmed by emv_read_application_data()
		emv_debug_error("Application Expiration Date (5F24) not found");
		return EMV_ERROR_INTERNAL;
	}
	if (emv_date_is_expired(txn_date, app_expiration_date)) {
		emv_debug_info("Application is expired");
		ctx->tvr->value[1] |= EMV_TVR_APPLICATION_EXPIRED;
	} else {
		emv_debug_info("Application is not expired");
		ctx->tvr->value[1] &= ~EMV_TVR_APPLICATION_EXPIRED;
	}

	return 0;
}

int emv_terminal_risk_management(struct emv_ctx_t* ctx,
	const struct emv_txn_log_entry_t* txn_log,
	size_t txn_log_cnt
)
{
	int r;
	const struct emv_tlv_t* term_floor_limit;
	const struct emv_tlv_t* txn_amount;
	uint32_t floor_limit_value;
	uint32_t amount_value;
	const struct emv_tlv_t* lower_offline_limit;
	const struct emv_tlv_t* upper_offline_limit;
	struct emv_tlv_list_t get_data_list = EMV_TLV_LIST_INIT;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->tvr || !ctx->tsi) {
		emv_debug_trace_msg("tvr=%p, tsi=%p", ctx->tvr, ctx->tsi);
		emv_debug_error("Invalid context variable");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!txn_log && txn_log_cnt) {
		emv_debug_trace_msg("txn_log=%p, txn_log_cnt=%zu", txn_log, txn_log_cnt);
		emv_debug_error("Invalid transaction log");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Ensure mandatory configuration fields are present and have valid length
	term_floor_limit = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT);
	if (!term_floor_limit || term_floor_limit->length != 4) {
		emv_debug_trace_msg("term_floor_limit=%p, term_floor_limit->length=%u",
			term_floor_limit, term_floor_limit ? term_floor_limit->length : 0);
		emv_debug_error("Terminal Floor Limit (9F1B) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}

	// Ensure that random selection configuration values are valid
	if (ctx->random_selection_percentage > 99 ||
		ctx->random_selection_max_percentage > 99 ||
		ctx->random_selection_percentage > ctx->random_selection_max_percentage
	) {
		emv_debug_trace_msg("random_selection_percentage=%u, "
			"random_selection_max_percentage->length=%u",
			ctx->random_selection_percentage,
			ctx->random_selection_max_percentage
		);
		emv_debug_error("Invalid random selection configuration");
		return EMV_ERROR_INVALID_CONFIG;
	}

	// Ensure mandatory transaction parameters are present and have valid length
	txn_amount = emv_tlv_list_find_const(&ctx->params, EMV_TAG_81_AMOUNT_AUTHORISED_BINARY);
	if (!txn_amount || txn_amount->length != 4) {
		emv_debug_trace_msg("txn_amount=%p, txn_amount->length=%u",
			txn_amount, txn_amount ? txn_amount->length : 0);
		emv_debug_error("Amount, Authorised - Binary (81) not found or invalid");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Mandatory fields are present for terminal risk management to proceed
	ctx->tsi->value[0] |= EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED;

	// Floor Limits
	// See EMV 4.4 Book 3, 10.6.1
	r = emv_format_b_to_uint(
		term_floor_limit->value,
		term_floor_limit->length,
		&floor_limit_value
	);
	if (r) {
		emv_debug_trace_msg("emv_format_b_to_uint() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}
	emv_debug_trace_msg("Terminal Floor Limit value is %u", (unsigned int)floor_limit_value);
	r = emv_format_b_to_uint(
		txn_amount->value,
		txn_amount->length,
		&amount_value
	);
	if (r) {
		emv_debug_trace_msg("emv_format_b_to_uint() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}
	emv_debug_trace_msg("Amount, Authorised (Binary) value is %u", (unsigned int)amount_value);
	if (txn_log && txn_log_cnt) {
		const struct emv_tlv_t* pan;
		const struct emv_txn_log_entry_t* entry = NULL;

		pan = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_5A_APPLICATION_PAN);
		if (!pan || !pan->length || pan->length > 10) {
			// Presence of the PAN should have been confirmed by
			// emv_read_application_data()
			emv_debug_error("Application Primary Account Number (PAN) not found or invalid");
			return EMV_ERROR_INTERNAL;
		}

		// Find the latest approved transaction with the same PAN. Note that it
		// is not mandatory to compare the Application PAN Sequence Number and
		// that this implementation specifically chooses not to do so because
		// the risk is considered for the card as a whole.
		for (size_t i = 0; i < txn_log_cnt; ++i) {
			if (pan->length <= sizeof(txn_log[i].pan) &&
				memcmp(pan->value, txn_log[i].pan, pan->length) == 0
			) {
				entry = &txn_log[i];
			}
		}

		if (entry) {
			emv_debug_trace_data("Using transaction log entry with amount %u for PAN", entry->pan, sizeof(entry->pan), entry->transaction_amount);
			amount_value += entry->transaction_amount;
		}

		emv_debug_trace_msg("Amount risk value is %u", (unsigned int)amount_value);
	}
	if (amount_value >= floor_limit_value) {
		emv_debug_info("Floor limit exceeded");
		ctx->tvr->value[3] |= EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED;
	} else {
		emv_debug_info("Floor limit not exceeded");
		ctx->tvr->value[3] &= ~EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED;
	}

	// Random Transaction Selection
	// See EMV 4.4 Book 3, 10.6.2
	if (amount_value < floor_limit_value &&
		ctx->random_selection_percentage
	) {
		int x;

		// Ensure that random selection threshold is valid to avoid invalid
		// computation of transaction target percent later
		if (ctx->random_selection_threshold >= floor_limit_value) {
			emv_debug_trace_msg("random_selection_threshold=%u",
				ctx->random_selection_threshold);
			emv_debug_error("Invalid random selection threshold");
			return EMV_ERROR_INVALID_CONFIG;
		}

		x = crypto_rand_byte(1, 99);
		if (x < 0) {
			emv_debug_trace_msg("crypto_rand_byte() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;
		}

		if (amount_value < ctx->random_selection_threshold) {
			// Apply unbiased transaction selection
			if (x <= ctx->random_selection_percentage) {
				emv_debug_info("Transaction selected randomly for online processing");
				ctx->tvr->value[3] |= EMV_TVR_RANDOM_SELECTED_ONLINE;
			}
		} else {
			// Compute transaction target percent
			// See EMV 4.4 Book 3, 10.6.2, figure 15
			unsigned int ttp =
				(
					(ctx->random_selection_max_percentage - ctx->random_selection_percentage) *
					(amount_value - ctx->random_selection_threshold)
				)
				/ (floor_limit_value - ctx->random_selection_threshold)
				+ ctx->random_selection_percentage;

			// Apply biased transaction selection
			if (x <= ttp) {
				emv_debug_info("Transaction selected randomly for online processing");
				ctx->tvr->value[3] |= EMV_TVR_RANDOM_SELECTED_ONLINE;
			}
		}
	} else {
		emv_debug_info("Random transaction selection not applied");
		ctx->tvr->value[3] &= ~EMV_TVR_RANDOM_SELECTED_ONLINE;
	}

	// Velocity Checking
	// See EMV 4.4 Book 3, 10.6.3
	lower_offline_limit = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT);
	upper_offline_limit = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT);
	if (lower_offline_limit && lower_offline_limit->length == 1 &&
		 upper_offline_limit && upper_offline_limit->length == 1
	) {
		const struct emv_tlv_t* atc;
		uint32_t atc_value;
		const struct emv_tlv_t* last_online_atc;
		uint32_t last_online_atc_value;

		// Retrieve Application Transaction Counter (9F36)
		r = emv_tal_get_data(
			ctx->ttl,
			EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER,
			&get_data_list
		);
		if (r) {
			emv_debug_trace_msg("emv_tal_get_data() failed; r=%d", r);
			emv_debug_error("Failed to retrieve Application Transaction Counter (9F36)");

			if (r < 0) {
				if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
					r = EMV_ERROR_INTERNAL;
				} else {
					// All other GET DATA errors are card errors
					r = EMV_OUTCOME_CARD_ERROR;
				}
				goto exit;
			}

			// Otherwise continue processing
		}
		atc = emv_tlv_list_find_const(&get_data_list, EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER);
		if (atc) {
			r = emv_format_b_to_uint(
				atc->value,
				atc->length,
				&atc_value
			);
			if (r) {
				emv_debug_trace_msg("emv_format_b_to_uint() failed; r=%d", r);

				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_ERROR_INTERNAL;
				goto exit;
			}

			emv_debug_trace_msg("ATC value is %u", atc_value);
		}

		// Retrieve Last Online ATC Register (9F13)
		r = emv_tal_get_data(
			ctx->ttl,
			EMV_TAG_9F13_LAST_ONLINE_ATC_REGISTER,
			&get_data_list
		);
		if (r) {
			emv_debug_trace_msg("emv_tal_get_data() failed; r=%d", r);
			emv_debug_error("Failed to retrieve Last Online ATC Register (9F13)");

			if (r < 0) {
				if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
					r = EMV_ERROR_INTERNAL;
				} else {
					// All other GET DATA errors are card errors
					r = EMV_OUTCOME_CARD_ERROR;
				}
				goto exit;
			}

			// Otherwise continue processing
		}
		last_online_atc = emv_tlv_list_find_const(&get_data_list, EMV_TAG_9F13_LAST_ONLINE_ATC_REGISTER);
		if (last_online_atc) {
			r = emv_format_b_to_uint(
				last_online_atc->value,
				last_online_atc->length,
				&last_online_atc_value
			);
			if (r) {
				emv_debug_trace_msg("emv_format_b_to_uint() failed; r=%d", r);

				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_ERROR_INTERNAL;
				goto exit;
			}

			emv_debug_trace_msg("Last Online ATC value is %u", last_online_atc_value);
		}

		// If both ATC and Last Online ATC are available, and offline
		// transaction attempts have happened since the previous online
		// authorisation, apply issuer velocity limits
		if (atc && last_online_atc &&
			atc_value > last_online_atc_value
		) {
			// Check velocity limits
			// See EMV 4.4 Book 3, 10.6.3
			if (atc_value - last_online_atc_value > lower_offline_limit->value[0]) {
				emv_debug_info("Lower Consecutive Offline Limits exceeded");
				ctx->tvr->value[3] |= EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
			} else {
				emv_debug_info("Lower Consecutive Offline Limits not exceeded");
				ctx->tvr->value[3] &= ~EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
			}
			if (atc_value - last_online_atc_value > upper_offline_limit->value[0]) {
				emv_debug_info("Upper Consecutive Offline Limits exceeded");
				ctx->tvr->value[3] |= EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
			} else {
				emv_debug_info("Upper Consecutive Offline Limits not exceeded");
				ctx->tvr->value[3] &= ~EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
			}

		} else {
			// Unable to apply velocity checking
			// See EMV 4.4 Book 3, 10.6.3
			emv_debug_info("Velocity checking not possible. Assume both Consecutive Offline Limits exceeded.");
			ctx->tvr->value[3] |= EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
			ctx->tvr->value[3] |= EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
		}

		// Check for new card
		// See EMV 4.4 Book 3, 10.6.3
		if (last_online_atc && last_online_atc_value == 0) {
			emv_debug_info("New card");
			ctx->tvr->value[1] |= EMV_TVR_NEW_CARD;
		} else {
			ctx->tvr->value[1] &= ~EMV_TVR_NEW_CARD;
		}

	} else {
		// If not present, skip velocity checking
		// See EMV 4.4 Book 3, 10.6.3
		// See EMV 4.4 Book 3, 7.3
		emv_debug_trace_msg("One or both Consecutive Offline Limits (9F14 or 9F23) not found");
		emv_debug_info("ICC does not support velocity checking");
		ctx->tvr->value[1] &= ~EMV_TVR_NEW_CARD;
		ctx->tvr->value[3] &= ~EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
		ctx->tvr->value[3] &= ~EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED;
	}

	// Append GET DATA output to ICC data list
	r = emv_tlv_list_append(&ctx->icc, &get_data_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_ERROR_INTERNAL;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&get_data_list);
	return r;
}

int emv_card_action_analysis(struct emv_ctx_t* ctx)
{
	int r;
	struct emv_tlv_sources_t sources = EMV_TLV_SOURCES_INIT;
	const struct emv_tlv_t* cdol1;
	struct emv_tlv_list_t genac_list = EMV_TLV_LIST_INIT;

	// Always decline offline for now until Terminal Action Analysis is fully
	// implemented
	uint8_t ref_ctrl = EMV_TTL_GENAC_TYPE_AAC;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->tvr) {
		emv_debug_trace_msg("tvr=%p", ctx->tvr);
		emv_debug_error("Invalid context variable");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	if (ctx->oda.method == EMV_ODA_METHOD_CDA &&
		!(ctx->tvr->value[0] & EMV_TVR_CDA_FAILED)
	) {
		// Only request CDA signature if CDA was previously selected but has
		// not yet failed.
		// See EMV 4.4 Book 2, 6.6
		// See EMV 4.4 Book 4, 6.3.2.1
		ref_ctrl |= EMV_TTL_GENAC_SIG_CDA;
	}

	// Prepare ordered data sources
	sources.count = 3;
	sources.list[0] = &ctx->params;
	sources.list[1] = &ctx->config;
	sources.list[2] = &ctx->terminal;

	// Prepare Card Risk Management Data
	// See EMV 4.4 Book 3, 9.2.1
	cdol1 = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_8C_CDOL1);
	if (!cdol1) {
		// Presence of CDOL1 should have been confirmed by
		// emv_read_application_data()
		emv_debug_error("Card Risk Management Data Object List 1 (CDOL1) not found");
		return EMV_ERROR_INTERNAL;
	}

	// Populate CDOL1 data in cache buffer
	ctx->oda.cdol1_data_len = sizeof(ctx->oda.cdol1_data);
	r = emv_dol_build_data(
		cdol1->value,
		cdol1->length,
		&sources,
		ctx->oda.cdol1_data,
		&ctx->oda.cdol1_data_len
	);
	if (r) {
		emv_debug_trace_msg("emv_dol_build_data() failed; r=%d", r);
		emv_debug_error("Failed to build CDOL1 data");

		// This is considered a card error because CDOL1 is provided by the
		// ICC, should be valid, and should not cause the maximum length to be
		// exceeded.
		return EMV_OUTCOME_CARD_ERROR;
	}

	// Perform Card Action Analysis using GENAC1
	// See EMV 4.4 Book 3, 10.8
	r = emv_tal_genac(
		ctx->ttl,
		ref_ctrl,
		ctx->oda.cdol1_data,
		ctx->oda.cdol1_data_len,
		&genac_list,
		(ref_ctrl & EMV_TTL_GENAC_SIG_MASK) ? &ctx->oda : NULL
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_genac() failed; r=%d", r);
		emv_debug_error("Error during card action analysis; terminate session");

		if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
			r = EMV_ERROR_INTERNAL;
		} else {
			// All other GENAC1 errors are card errors
			r = EMV_OUTCOME_CARD_ERROR;
		}
		goto exit;
	}

	// TODO: Implement offline decline if CID indicates AAC
	// See EMV 4.4 Book 2, 6.6.2
	// See EMV 4.4 Book 4, 6.3.7

	if (ref_ctrl & EMV_TTL_GENAC_SIG_MASK) {
		// Validate GENAC1 response which will in turn append it to the ICC
		// data list
		r = emv_oda_process_genac(ctx, &genac_list);
		if (r) {
			if (r < 0) {
				emv_debug_trace_msg("emv_oda_process_genac() failed; r=%d", r);
				emv_debug_error("Error during card action analysis; terminate session");
				if (r == EMV_ODA_ERROR_INTERNAL || r == EMV_ODA_ERROR_INVALID_PARAMETER) {
					r = EMV_ERROR_INTERNAL;
				} else {
					r = EMV_OUTCOME_CARD_ERROR;
				}
				goto exit;
			}

		// Otherwise session may continue although offline data authentication
		// has failed.
		emv_debug_error("Offline data authentication failed");
		}
	} else {
		// Append GENAC1 output to ICC data list
		r = emv_tlv_list_append(&ctx->icc, &genac_list);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_ERROR_INTERNAL;
			goto exit;
		}
	}

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&genac_list);
	return r;
}
