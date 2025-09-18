/**
 * @file emv_tal.c
 * @brief EMV Terminal Application Layer (TAL)
 *
 * Copyright 2021, 2024-2025 Leon Lynch
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

#include "emv_tal.h"
#include "emv_ttl.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_tlv.h"
#include "emv_app.h"
#include "emv_oda.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_TAL
#include "emv_debug.h"

#include "iso8825_ber.h"

#include <stdint.h>
#include <string.h>

// Helper functions
static int emv_tal_parse_aef_record(
	struct emv_tlv_list_t* pse_tlv_list,
	const void* aef_record,
	size_t aef_record_len,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
);
static int emv_tal_read_sfi_records(
	struct emv_ttl_t* ttl,
	const struct emv_afl_entry_t* afl_entry,
	struct emv_tlv_list_t* list,
	struct emv_oda_ctx_t* oda
);

int emv_tal_read_pse(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
)
{
	int r;
	uint8_t fci[EMV_RAPDU_DATA_MAX];
	size_t fci_len = sizeof(fci);
	uint16_t sw1sw2;
	struct emv_tlv_list_t pse_tlv_list = EMV_TLV_LIST_INIT;

	const struct emv_tlv_t* pse_sfi;

	if (!ttl || !app_list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	// SELECT Payment System Environment (PSE) Directory Definition File (DDF)
	// See EMV 4.4 Book 1, 12.2.2
	// See EMV 4.4 Book 1, 12.3.2
	emv_debug_info("SELECT %s", EMV_PSE);
	r = emv_ttl_select_by_df_name(ttl, EMV_PSE, strlen(EMV_PSE), fci, &fci_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_select_by_df_name() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}

	if (sw1sw2 == 0x6A81) {
		// Card blocked or SELECT not supported; terminate session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("Card blocked or SELECT not supported");
		return EMV_TAL_ERROR_CARD_BLOCKED;
	}

	if (sw1sw2 == 0x6A82) {
		// PSE not found; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_info("PSE not found");
		return EMV_TAL_RESULT_PSE_NOT_FOUND;
	}

	if (sw1sw2 == 0x6283) {
		// PSE is blocked; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("PSE is blocked");
		return EMV_TAL_RESULT_PSE_BLOCKED;
	}

	if (sw1sw2 != 0x9000) {
		// Failed to SELECT PSE; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("Failed to SELECT PSE");
		return EMV_TAL_RESULT_PSE_SELECT_FAILED;
	}

	emv_debug_info_ber("FCI", fci, fci_len);

	// Parse File Control Information (FCI) provided by PSE DDF
	// NOTE: FCI may contain padding (r > 0)
	// See EMV 4.4 Book 1, 11.3.4, table 8
	r = emv_tlv_parse(fci, fci_len, &pse_tlv_list);
	if (r < 0) {
		emv_debug_trace_msg("emv_tlv_parse() failed; r=%d", r);

		// Internal error while parsing FCI data; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("Failed to parse FCI");
		r = EMV_TAL_RESULT_PSE_FCI_PARSE_FAILED;
		goto exit;
	}

	// Find Short File Identifier (SFI) for PSE directory Application Elementary File (AEF)
	// See EMV 4.4 Book 1, 11.3.4, table 8
	pse_sfi = emv_tlv_list_find_const(&pse_tlv_list, EMV_TAG_88_SFI);
	if (!pse_sfi) {
		emv_debug_trace_msg("emv_tlv_list_find_const() failed; pse_sfi=%p", pse_sfi);

		// Failed to find SFI for PSE records; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("Failed to find SFI for PSE records");
		r = EMV_TAL_RESULT_PSE_SFI_NOT_FOUND;
		goto exit;
	}
	if (pse_sfi->length != 1 || !pse_sfi->value) {
		emv_debug_trace_data("pse_sfi=", pse_sfi->value, pse_sfi->length);

		// Invalid SFI for PSE; terminal may continue session
		// See EMV 4.4 Book 1, 12.3.2, step 1
		emv_debug_error("Invalid SFI length or value for PSE records");
		r = EMV_TAL_RESULT_PSE_SFI_INVALID;
		goto exit;
	}

	// Read all records from PSE AEF using the SFI
	// See EMV 4.4 Book 1, 12.2.3
	for (uint8_t record_number = 1; ; ++record_number) {
		uint8_t aef_record[EMV_RAPDU_DATA_MAX];
		size_t aef_record_len = sizeof(aef_record);

		// Read PSE AEF record
		// See EMV 4.4 Book 1, 12.2.3
		r = emv_ttl_read_record(ttl, pse_sfi->value[0], record_number, aef_record, &aef_record_len, &sw1sw2);
		if (r) {
			emv_debug_trace_msg("emv_ttl_read_record() failed; r=%d", r);

			// TTL failure; terminate session
			// (bad card or reader; infinite loop if we continue)
			emv_debug_error("TTL failure");
			r = EMV_TAL_ERROR_TTL_FAILURE;
			goto exit;
		}

		if (sw1sw2 == 0x6A83) {
			// No more records
			// See EMV 4.4 Book 1, 12.3.2, step 2
			emv_debug_info("No more PSE records");
			break;
		}

		if (sw1sw2 != 0x9000) {
			// Unexpected error; ignore record and continue
			// See EMV 4.4 Book 1, 12.3.2, step 2
			emv_debug_error("Unexpected PSE record error");
			continue;
		}

		emv_debug_info_ber("AEF", aef_record, aef_record_len);

		r = emv_tal_parse_aef_record(
			&pse_tlv_list,
			aef_record,
			aef_record_len,
			supported_aids,
			app_list
		);
		if (r) {
			emv_debug_trace_msg("emv_tal_parse_aef_record() failed; r=%d", r);
			if (r < 0) {
				// Unknown error; terminate session
				emv_debug_error("Unknown PSE AEF record error");
				goto exit;
			}
			if (r > 0) {
				// Invalid PSE AEF record; ignore and continue
				emv_debug_error("Invalid PSE AEF record");
			}
		}
	}

	// Successful PSE processing
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&pse_tlv_list);
	if (r) {
		emv_app_list_clear(app_list);
	}
	return r;
}

static int emv_tal_parse_aef_record(
	struct emv_tlv_list_t* pse_tlv_list,
	const void* aef_record,
	size_t aef_record_len,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
)
{
	int r;
	struct iso8825_tlv_t aef_template_tlv;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	// Record should contain AEF template (field 70)
	// See EMV 4.4 Book 1, 12.2.3, table 11
	r = iso8825_ber_decode(aef_record, aef_record_len, &aef_template_tlv);
	if (r <= 0) {
		emv_debug_trace_msg("iso8825_ber_decode() failed; r=%d", r);

		// Failed to parse PSE AEF record; ignore and continue
		emv_debug_error("Failed to parse PSE AEF record");
		return EMV_TAL_RESULT_PSE_AEF_PARSE_FAILED;
	}
	if (aef_template_tlv.tag != EMV_TAG_70_DATA_TEMPLATE) {
		// No AEF template in PSE record; ignore and continue
		emv_debug_error("Unexpected data element 0x%02X in PSE AEF record", tlv.tag);
		return EMV_TAL_RESULT_PSE_AEF_INVALID;
	}

	// NOTE: AEF template (field 70) may contain multiple Application Templates (field 61)
	// See EMV 4.4 Book 1, 10.1.4
	// See EMV 4.4 Book 1, 12.2.3

	r = iso8825_ber_itr_init(aef_template_tlv.value, aef_template_tlv.length, &itr);
	if (r) {
		emv_debug_trace_msg("iso8825_ber_itr_init() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_TAL_ERROR_INTERNAL;
	}

	// Iterate Application Templates (field 61)
	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		struct emv_app_t* app;

		if (tlv.tag != EMV_TAG_61_APPLICATION_TEMPLATE) {
			// Ignore unexpected data elements in AEF template
			// See EMV 4.4 Book 1, 12.2.3
			emv_debug_error("Unexpected data element 0x%02X in AEF template", tlv.tag);
			continue;
		}

		// Create EMV application object
		// See EMV 4.4 Book 1, 12.2.3, table 12
		app = emv_app_create_from_pse(pse_tlv_list, tlv.value, tlv.length);
		if (!app) {
			// Ignore invalid Application Template (field 61) content
			// See EMV 4.4 Book 1, 12.2.3
			emv_debug_error("Invalid Application Template content");
			continue;
		}

		if (emv_app_is_supported(app, supported_aids)) {
			// App supported; add to candidate list
			// See EMV 4.4 Book 1, 12.3.2, step 3
			emv_debug_info("Application is supported");
			emv_app_list_push(app_list, app);
		} else {
			// App not supported; ignore app and continue
			emv_debug_info("Application is not supported");
			emv_app_free(app);
			app = NULL;
		}
	}

	return 0;
}

int emv_tal_find_supported_apps(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
)
{
	int r;
	struct emv_tlv_t* aid;
	bool exact_match;

	if (!ttl || !supported_aids || !app_list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	aid = supported_aids->front;
	exact_match = true;

	while (aid != NULL) {
		uint8_t fci[EMV_RAPDU_DATA_MAX];
		size_t fci_len = sizeof(fci);
		uint16_t sw1sw2;
		struct emv_app_t* app;

		if (exact_match) {
			// SELECT application
			// See EMV 4.4 Book 1, 12.3.3, step 1
			emv_debug_info_data("SELECT application", aid->value, aid->length);
			r = emv_ttl_select_by_df_name(ttl, aid->value, aid->length, fci, &fci_len, &sw1sw2);
			if (r) {
				emv_debug_trace_msg("emv_ttl_select_by_df_name() failed; r=%d", r);

				// TTL failure; terminate session
				// (bad card or reader)
				emv_debug_error("TTL failure");
				return EMV_TAL_ERROR_TTL_FAILURE;
			}

			if (sw1sw2 == 0x6A81) {
				// Card blocked or SELECT not supported; terminate session
				// See EMV 4.4 Book 1, 12.3.3, step 2
				emv_debug_error("Card blocked or SELECT not supported");
				return EMV_TAL_ERROR_CARD_BLOCKED;
			}

		} else {
			// SELECT next application for partial AID match
			// See EMV 4.4 Book 1, 12.3.3, step 7
			emv_debug_info_data("SELECT next application", aid->value, aid->length);
			r = emv_ttl_select_by_df_name_next(ttl, aid->value, aid->length, fci, &fci_len, &sw1sw2);
			if (r) {
				emv_debug_trace_msg("emv_ttl_select_by_df_name_next() failed; r=%d", r);

				// TTL failure; terminate session
				// (bad card or reader)
				emv_debug_error("TTL failure");
				return EMV_TAL_ERROR_TTL_FAILURE;
			}
		}

		if (sw1sw2 != 0x9000 && sw1sw2 != 0x6283) {
			// Unexpected SELECT status; ignore app and continue to next supported AID
			// See EMV 4.4 Book 1, 12.3.3, step 3
			if (sw1sw2 == 0x6A82) {
				emv_debug_info("Application not found");
			} else {
				emv_debug_error("Unexpected SELECT status 0x%04X", sw1sw2);
			}
			aid = aid->next;
			exact_match = true;
			continue;
		}

		emv_debug_info_ber("FCI", fci, fci_len);

		// Extract FCI data
		// See EMV 4.4 Book 1, 12.3.3, step 3
		app = emv_app_create_from_fci(fci, fci_len);
		if (!app) {
			emv_debug_trace_msg("emv_app_create_from_fci() failed; app=%p", app);

			// Unexpected error; ignore app and continue to next supported AID
			// See EMV 4.4 Book 1, 12.3.3, step 3
			aid = aid->next;
			exact_match = true;
			continue;
		}

		// NOTE: It is assumed that the SELECT command will only provide
		// AIDs that are already a partial or exact match. Therefore it is
		// only necessary to compare the lengths to know whether it was a
		// partial or exact match.

		if (aid->length == app->aid->length) {
			// Exact match; check whether valid or blocked
			// See EMV 4.4 Book 1, 12.3.3, step 4

			if (sw1sw2 == 0x9000) {
				// Valid app; add app and continue to next supported AID
				// See EMV 4.4 Book 1, 12.3.3, step 4
				emv_debug_info("Application is supported: exact match");
				emv_app_list_push(app_list, app);
			} else {
				// Blocked app; ignore app and continue to next supported AID
				// See EMV 4.4 Book 1, 12.3.3, step 4
				emv_debug_info("Application is blocked");
				emv_app_free(app);
				app = NULL;
			}

			// See EMV 4.4 Book 1, 12.3.3, step 5
			aid = aid->next;
			exact_match = true;
			continue;

		} else {
			// Partial match; check Application Selection Indicator (ASI)
			// See EMV 4.4 Book 1, 12.3.3, step 6

			if (aid->flags == EMV_ASI_PARTIAL_MATCH) {
				// Partial match allowed; check whether valid or blocked

				if (sw1sw2 == 0x9000) {
					// Valid app; add app and continue to next partial AID
					// See EMV 4.4 Book 1, 12.3.3, step 6
					emv_debug_info("Application is supported: partial match");
					emv_app_list_push(app_list, app);
				} else {
					// Blocked app; ignore app and continue to next partial AID
					// See EMV 4.4 Book 1, 12.3.3, step 6
					emv_debug_info("Application is blocked");
					emv_app_free(app);
					app = NULL;
				}

				// See EMV 4.4 Book 1, 12.3.3, step 7
				exact_match = false;
				continue;

			} else {
				// Partial match not allowed; ignore app and continue to next supported AID
				// See EMV 4.4 Book 1, 12.3.3, step 6
				emv_debug_info("Application is not supported: partial match not allowed");
				emv_app_free(app);
				app = NULL;

				// See EMV 4.4 Book 1, 12.3.3, step 5
				aid = aid->next;
				exact_match = true;
				continue;
			}
		}
	}

	return 0;
}

int emv_tal_select_app(
	struct emv_ttl_t* ttl,
	const uint8_t* aid,
	size_t aid_len,
	struct emv_app_t** selected_app
)
{
	int r;
	uint8_t fci[EMV_RAPDU_DATA_MAX];
	size_t fci_len = sizeof(fci);
	uint16_t sw1sw2;
	struct emv_app_t* app;

	if (!ttl || !aid || aid_len < 5 || aid_len > 16 || !selected_app) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	*selected_app = NULL;

	// SELECT application
	// See EMV 4.4 Book 1, 12.4
	emv_debug_info_data("SELECT application", aid, aid_len);
	r = emv_ttl_select_by_df_name(ttl, aid, aid_len, fci, &fci_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_select_by_df_name() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}

	if (sw1sw2 != 0x9000) {
		switch (sw1sw2) {
			case 0x6A81:
				// Card blocked or SELECT not supported; terminate session
				emv_debug_error("Card blocked or SELECT not supported");
				return EMV_TAL_ERROR_CARD_BLOCKED;

			case 0x6A82:
				// Application not found; ignore app and continue
				emv_debug_info("Application not found");
				return EMV_TAL_RESULT_APP_NOT_FOUND;

			case 0x6283:
				// Application blocked; ignore app and continue
				emv_debug_info("Application is blocked");
				return EMV_TAL_RESULT_APP_BLOCKED;

			default:
				// Unknown error; ignore app and continue
				// See EMV Book 1, 12.4, regarding status other than 9000
				emv_debug_error("SW1SW2=0x%04hX\n", sw1sw2);
				return EMV_TAL_RESULT_APP_SELECTION_FAILED;
		}
	}

	emv_debug_info_ber("FCI", fci, fci_len);

	// Parse FCI to confirm that selected application is valid
	app = emv_app_create_from_fci(fci, fci_len);
	if (!app) {
		emv_debug_trace_msg("emv_app_create_from_fci() failed; app=%p", app);

		// Failed to parse FCI; ignore app and continue
		// See EMV Book 1, 12.4, regarding format errors
		emv_debug_error("Failed to parse application FCI");
		return EMV_TAL_RESULT_APP_FCI_PARSE_FAILED;
	}

	// Ensure that the AID used by the SELECT command exactly matches the
	// DF Name (field 84) provided by the FCI
	// See EMV 4.4 Book 1, 12.4
	// NOTE: emv_app_create_from_fci() sets AID to DF Name (field 84)
	if (app->aid->length != aid_len ||
		memcmp(app->aid->value, aid, aid_len)
	) {
		emv_debug_error("DF Name mismatch");
		emv_app_free(app);
		return EMV_TAL_RESULT_APP_SELECTION_FAILED;
	}

	// Output
	*selected_app = app;

	return 0;
}

int emv_tal_get_processing_options(
	struct emv_ttl_t* ttl,
	const void* data,
	size_t data_len,
	struct emv_tlv_list_t* list,
	const struct emv_tlv_t** aip,
	const struct emv_tlv_t** afl
)
{
	int r;
	uint8_t gpo_response[EMV_RAPDU_DATA_MAX];
	size_t gpo_response_len = sizeof(gpo_response);
	uint16_t sw1sw2;
	struct iso8825_tlv_t gpo_tlv;
	struct emv_tlv_list_t gpo_list = EMV_TLV_LIST_INIT;
	const struct emv_tlv_t* tlv;

	if (!ttl || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (data && (data_len < 2 || data_len > 255)) { // See EMV_CAPDU_DATA_MAX
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (!data && data_len) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (aip) {
		*aip = NULL;
	}
	if (afl) {
		*afl = NULL;
	}

	// GET PROCESSING OPTIONS
	// See EMV 4.4 Book 3, 10.1
	emv_debug_info_data("GET PROCESSING OPTIONS", data, data_len);
	r = emv_ttl_get_processing_options(ttl, data, data_len, gpo_response, &gpo_response_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_get_processing_options() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}

	if (sw1sw2 != 0x9000) {
		switch (sw1sw2) {
			case 0x6985:
				// Conditions of use not satisfied; ignore app and continue
				// See EMV 4.4 Book 3, 10.1
				emv_debug_info("Conditions of use not satisfied");
				return EMV_TAL_RESULT_GPO_CONDITIONS_NOT_SATISFIED;

			default:
				// Unknown error; terminate session
				// According to EMV 4.4 Book 3, 10.1 the card should provide
				// no status other than 9000 or 6985
				emv_debug_error("SW1SW2=0x%04hX", sw1sw2);
				return EMV_TAL_ERROR_GPO_FAILED;
		}
	}

	emv_debug_info_ber("GPO response", gpo_response, gpo_response_len);

	// Determine GPO response format
	r = iso8825_ber_decode(gpo_response, gpo_response_len, &gpo_tlv);
	if (r <= 0) {
		emv_debug_trace_msg("iso8825_ber_decode() failed; r=%d", r);

		// Parse error; terminate session
		// See EMV 4.4 Book 3, 6.5.8.4
		emv_debug_error("Failed to parse GPO response");
		return EMV_TAL_ERROR_GPO_PARSE_FAILED;
	}

	if (gpo_tlv.tag == EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1) {
		// GPO response format 1
		// See EMV 4.4 Book 3, 6.5.8.4

		// Validate length
		// See EMV 4.4 Book 3, 10.2
		// AIP is 2 bytes
		// AFL is multiples of 4 bytes
		if (gpo_tlv.length < 6 || // AIP and at least one AFL entry
			((gpo_tlv.length - 2) & 0x3) != 0 // AFL is multiple of 4 bytes
		) {
			// Parse error; terminate session
			// See EMV 4.4 Book 3, 6.5.8.4
			emv_debug_error("Invalid GPO response format 1 length of %u", gpo_tlv.length);
			return EMV_TAL_ERROR_GPO_PARSE_FAILED;
		}

		// Create Application Interchange Profile (field 82)
		r = emv_tlv_list_push(
			&gpo_list,
			EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE,
			2,
			gpo_tlv.value,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		// Create Application File Locator (field 94)
		r = emv_tlv_list_push(
			&gpo_list,
			EMV_TAG_94_APPLICATION_FILE_LOCATOR,
			gpo_tlv.length - 2,
			gpo_tlv.value + 2,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		emv_debug_info_tlv_list("Extracted format 1 fields", &gpo_list);

	} else if (gpo_tlv.tag == EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2) {
		// GPO response format 2
		// See EMV 4.4 Book 3, 6.5.8.4

		r = emv_tlv_parse(gpo_tlv.value, gpo_tlv.length, &gpo_list);
		if (r) {
			emv_debug_trace_msg("emv_tlv_parse() failed; r=%d", r);
			if (r < 0) {
				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_TAL_ERROR_INTERNAL;
				goto exit;
			}
			if (r > 0) {
				// Parse error; terminate session
				// See EMV 4.4 Book 3, 6.5.8.4
				emv_debug_error("Failed to parse GPO response format 2");
				r = EMV_TAL_ERROR_GPO_PARSE_FAILED;
				goto exit;
			}
		}

	} else {
		emv_debug_error("Invalid GPO response format");
		r = EMV_TAL_ERROR_GPO_PARSE_FAILED;
		goto exit;
	}

	// Populate AIP pointer
	tlv = emv_tlv_list_find_const(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!tlv) {
		// Mandatory field missing; terminate session
		// See EMV 4.4 Book 3, 6.5.8.4
		emv_debug_error("AIP not found in GPO response");
		r = EMV_TAL_ERROR_GPO_FIELD_NOT_FOUND;
		goto exit;
	}
	if (aip) {
		*aip = tlv;
	}

	// Populate AFL pointer
	tlv = emv_tlv_list_find_const(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!tlv) {
		// Mandatory field missing; terminate session
		// See EMV 4.4 Book 3, 6.5.8.4
		emv_debug_error("AFL not found in GPO response");
		r = EMV_TAL_ERROR_GPO_FIELD_NOT_FOUND;
		goto exit;
	}
	if (afl) {
		*afl = tlv;
	}

	r = emv_tlv_list_append(list, &gpo_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_TAL_ERROR_INTERNAL;
		goto exit;
	}

	// Successful GPO processing
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&gpo_list);
	return r;
}

static int emv_tal_read_sfi_records(
	struct emv_ttl_t* ttl,
	const struct emv_afl_entry_t* afl_entry,
	struct emv_tlv_list_t* list,
	struct emv_oda_ctx_t* oda
)
{
	int r;
	bool oda_record_invalid = false;

	if (!ttl || !afl_entry || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	for (uint8_t record_number = afl_entry->first_record; record_number <= afl_entry->last_record; ++record_number) {
		uint8_t record[EMV_RAPDU_DATA_MAX];
		size_t record_len = sizeof(record);
		uint16_t sw1sw2;
		bool record_oda = false;
		struct emv_tlv_list_t record_list = EMV_TLV_LIST_INIT;

		// READ RECORD
		// See EMV 4.4 Book 3, 10.2
		emv_debug_info("READ RECORD from SFI %u, record %u", afl_entry->sfi, record_number);
		r = emv_ttl_read_record(ttl, afl_entry->sfi, record_number, record, &record_len, &sw1sw2);
		if (r) {
			emv_debug_trace_msg("emv_ttl_read_record() failed; r=%d", r);
			// TTL failure; terminate session
			// (bad card or reader)
			emv_debug_error("TTL failure");
			return EMV_TAL_ERROR_TTL_FAILURE;
		}

		if (sw1sw2 != 0x9000) {
			// Failed to READ RECORD; terminate session
			// See EMV 4.4 Book 3, 10.2
			emv_debug_error("Failed to READ RECORD");
			return EMV_TAL_ERROR_READ_RECORD_FAILED;
		}

		// Determine whether record is intended for offline data authentication
		if (!oda_record_invalid &&
			afl_entry->oda_record_count &&
			record_number - afl_entry->first_record < afl_entry->oda_record_count
		) {
			record_oda = true;
		}

		// The records for SFIs 1 - 10 must be encoded as field 70 and the
		// records for SFIs beyond that range are outside of EMV except for the
		// card Transaction Log
		// See EMV 4.4 Book 3, 5.3.2.2
		// See EMV 4.4 Book 3, 6.5.11.4
		// See EMV 4.4 Book 3, 7.1

		// The records intended for offline data authentication must be encoded
		// as field 70. If not, then offline data authentication will be
		// considered to have been performed but failed.
		// See EMV 4.4 Book 3, 10.3 (page 98)

		// Therefore, any record that is not encoded as field 70 should not be
		// parsed for EMV fields, and if that record is also intended for
		// offline data authentication then further offline data authentication
		// will be invalidated. However, this implementation chooses to parse
		// proprietary records with an SFI outside 1 - 10 that are encoded as
		// field 70.
		if (record[0] != EMV_TAG_70_DATA_TEMPLATE) {
			if (afl_entry->sfi >= 1 && afl_entry->sfi <= 10) {
				// Invalid record for SFIs 1 - 10
				// EMV 4.4 Book 3, 6.5.11.4
				emv_debug_error("Invalid record for SFI %u", afl_entry->sfi);
				return EMV_TAL_ERROR_READ_RECORD_INVALID;
			}

			if (record_oda) {
				// See EMV 4.4 Book 3, 10.3 (page 98)
				emv_debug_error("Offline data authentication not possible due to proprietary record");
				oda_record_invalid = true;
			}

			// This implementation chooses to continue reading records if a
			// proprietary record has an SFI outside 1 - 10 and is not encoded
			// as field 70, although it will not be parsed for EMV fields.
			emv_debug_info("Skip proprietary record");
			continue;
		}

		if (afl_entry->sfi >= 1 && afl_entry->sfi <= 10) {
			struct iso8825_tlv_t record_template;
			size_t record_template_len;

			// Record should contain a single record template for SFIs 1 - 10
			// See EMV 4.4 Book 3, 6.5.11.4
			r = iso8825_ber_decode(record, record_len, &record_template);
			if (r <= 0) {
				emv_debug_trace_msg("iso8825_ber_decode() failed; r=%d", r);

				// Failed to parse application data record template
				// See EMV 4.4 Book 3, 6.5.11.4
				emv_debug_error("Failed to parse record template");
				return EMV_TAL_ERROR_READ_RECORD_INVALID;
			}
			record_template_len = r;
			if (record_template.tag != EMV_TAG_70_DATA_TEMPLATE) {
				// Invalid record template for SFIs 1 - 10
				// See EMV 4.4 Book 3, 6.5.11.4
				emv_debug_error("Invalid record template tag 0x%02X", record_template.tag);
				return EMV_TAL_ERROR_READ_RECORD_INVALID;
			}
			if (record_template_len != record_len) {
				// Record should contain a single record template without
				// additional data after it
				// See EMV 4.4 Book 3, 6.5.11.4
				emv_debug_error("Invalid record template total length %zu; expected %zu", record_template_len, record_len);
				return EMV_TAL_ERROR_READ_RECORD_INVALID;
			}

			if (record_oda) {
				emv_debug_info("Record template content used for offline data authentication");

				if (oda) {
					// For SFIs 1 - 10, use record template content for ODA
					// See EMV 4.4 Book 3, 10.3 (page 98)
					r = emv_oda_append_record(oda, record_template.value, record_template.length);
					if (r) {
						emv_debug_trace_msg("emv_oda_append_record() failed; r=%d", r);
						// emv_oda_append_record() also prints error
						oda_record_invalid = true;
					}
				}
			}
		} else {
			if (record_oda) {
				emv_debug_info("Record used verbatim for offline data authentication");
				if (oda) {
					// For SFIs 11 - 30, use record verbatim for ODA
					// See EMV 4.4 Book 3, 10.3 (page 98)
					r = emv_oda_append_record(oda, record, record_len);
					if (r) {
						emv_debug_trace_msg("emv_oda_append_record() failed; r=%d", r);
						// emv_oda_append_record() also prints error
						oda_record_invalid = true;
					}
				}
			}
		}

		// Parse application data knowing that the record starts with 70 and
		// that it contains a single record template for SFIs 1 - 10. The EMV
		// specification does not indicate whether SFIs beyond 1 - 10 may
		// contain multiple record templates or additional data after the
		// record template(s), and only states that the record template tag and
		// length should not be excluded during offline data authentication
		// processing. This implementation therefore assumes that any record
		// that has passed the preceding validations is suitable for parsing.
		r = emv_tlv_parse(record, record_len, &record_list);
		if (r) {
			emv_debug_trace_msg("emv_tlv_parse() failed; r=%d", r);

			// Always cleanup record list
			emv_tlv_list_clear(&record_list);

			if (r < 0) {
				// Internal error; terminate session
				emv_debug_error("Internal error");
				return EMV_TAL_ERROR_INTERNAL;
			}
			if (r > 0) {
				// Parse error; terminate session
				emv_debug_error("Failed to parse application data record");
				return EMV_TAL_ERROR_READ_RECORD_PARSE_FAILED;
			}
		}
		emv_debug_info_ber("READ RECORD [%u,%u] response", record, record_len, afl_entry->sfi, record_number);

		r = emv_tlv_list_append(list, &record_list);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

			// Always cleanup record list
			emv_tlv_list_clear(&record_list);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			return EMV_TAL_ERROR_INTERNAL;
		}
	}

	// Successfully read records although offline data authentication may have
	// failed
	if (oda_record_invalid) {
		return EMV_TAL_RESULT_ODA_RECORD_INVALID;
	} else {
		return 0;
	}
}

int emv_tal_read_afl_records(
	struct emv_ttl_t* ttl,
	const uint8_t* afl,
	size_t afl_len,
	struct emv_tlv_list_t* list,
	struct emv_oda_ctx_t* oda
)
{
	int r;
	struct emv_afl_itr_t afl_itr;
	struct emv_afl_entry_t afl_entry;
	bool oda_record_invalid = false;

	if (!ttl || !afl || !afl_len || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	r = emv_afl_itr_init(afl, afl_len, &afl_itr);
	if (r) {
		emv_debug_trace_msg("emv_afl_itr_init() failed; r=%d", r);
		if (r < 0) {
			// Internal error; terminate session
			emv_debug_error("Internal error");
			return EMV_TAL_ERROR_INTERNAL;
		}
		if (r > 0) {
			// Invalid AFL; terminate session
			// See EMV 4.4 Book 3, 10.2
			emv_debug_error("Invalid AFL");
			return EMV_TAL_ERROR_AFL_INVALID;
		}
	}

	while ((r = emv_afl_itr_next(&afl_itr, &afl_entry)) > 0) {
		r = emv_tal_read_sfi_records(ttl, &afl_entry, list, oda);
		if (r) {
			emv_debug_trace_msg("emv_tal_read_sfi_records() failed; r=%d", r);
			if (r == EMV_TAL_RESULT_ODA_RECORD_INVALID) {
				// Continue regardless of offline data authentication failure
				// See EMV 4.4 Book 3, 10.3 (page 98)
				oda_record_invalid = true;
			} else {
				// Return error value as-is
				return r;
			}
		}
	}
	if (r < 0) {
		emv_debug_trace_msg("emv_afl_itr_next() failed; r=%d", r);

		// AFL parse error; terminate session
		// See EMV 4.4 Book 3, 10.2
		emv_debug_error("AFL parse error");
		return EMV_TAL_ERROR_AFL_INVALID;
	}

	// Successfully read records although offline data authentication may have
	// failed
	if (oda_record_invalid) {
		return EMV_TAL_RESULT_ODA_RECORD_INVALID;
	} else {
		return 0;
	}
}

int emv_tal_get_data(
	struct emv_ttl_t* ttl,
	uint16_t tag,
	struct emv_tlv_list_t* list
)
{
	int r;
	uint8_t response[EMV_RAPDU_DATA_MAX];
	size_t response_len = sizeof(response);
	struct emv_tlv_list_t response_list = EMV_TLV_LIST_INIT;
	uint16_t sw1sw2;

	if (!ttl || !tag || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	// GET DATA
	// See EMV 4.4 Book 3, 6.5.7
	emv_debug_info("GET DATA [%X]", tag);
	r = emv_ttl_get_data(ttl, tag, response, &response_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_get_data() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}
	// See EMV 4.4 Book 3, 6.5.7.5
	if (sw1sw2 != 0x9000) {
		// Unknown error; terminal may continue session
		// See EMV 4.4 Book 3, 6.3.5 (page 50)
		emv_debug_error("SW1SW2=0x%04hX", sw1sw2);
		return EMV_TAL_RESULT_GET_DATA_FAILED;
	}

	emv_debug_info_ber("GET DATA response", response, response_len);

	r = emv_tlv_parse(response, response_len, &response_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_parse() failed; r=%d", r);
		if (r < 0) {
			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}
		if (r > 0) {
			// Parse error; terminate session
			// See EMV 4.4 Book 3, 6.5.7.4
			emv_debug_error("Failed to parse GET DATA response");
			r = EMV_TAL_ERROR_GET_DATA_PARSE_FAILED;
			goto exit;
		}
	}

	r = emv_tlv_list_append(list, &response_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_TAL_ERROR_INTERNAL;
	}

	// Successful GET DATA processing
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&response_list);
	return r;
}

int emv_tal_internal_authenticate(
	struct emv_ttl_t* ttl,
	const void* data,
	size_t data_len,
	struct emv_tlv_list_t* list
)
{
	int r;
	uint8_t response[EMV_RAPDU_DATA_MAX];
	size_t response_len = sizeof(response);
	uint16_t sw1sw2;
	struct iso8825_tlv_t response_tlv;
	struct emv_tlv_list_t response_list = EMV_TLV_LIST_INIT;
	const struct emv_tlv_t* sdad_tlv;

	if (!ttl || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (data && data_len > EMV_CAPDU_DATA_MAX) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (!data && data_len) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	// INTERNAL AUTHENTICATE
	// See EMV 4.4 Book 2, 6.5
	emv_debug_info_data("INTERNAL AUTHENTICATE", data, data_len);
	r = emv_ttl_internal_authenticate(ttl, data, data_len, response, &response_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_internal_authenticate() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}
	// See EMV 4.4 Book 3, 6.5.9.5
	if (sw1sw2 != 0x9000) {
		// Unknown error; terminate session
		emv_debug_error("SW1SW2=0x%04hX", sw1sw2);
		return EMV_TAL_ERROR_INT_AUTH_FAILED;
	}

	emv_debug_info_ber("INTERNAL AUTHENTICATE response", response, response_len);

	// Determine response format
	r = iso8825_ber_decode(response, response_len, &response_tlv);
	if (r <= 0) {
		emv_debug_trace_msg("iso8825_ber_decode() failed; r=%d", r);

		// Parse error; terminate session
		// See EMV 4.4 Book 3, 6.5.9.4
		emv_debug_error("Failed to parse INTERNAL AUTHENTICATE response");
		return EMV_TAL_ERROR_INT_AUTH_PARSE_FAILED;
	}

	if (response_tlv.tag == EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1) {
		// Response format 1
		// See EMV 4.4 Book 3, 6.5.9.4

		// Validate length
		// See EMV 4.4 Book 2, 6.5.2, table 17
		// Assume that ICC public key will not be less than 512-bit and
		// therefore SDAD must be at least 64 bytes.
		if (response_tlv.length < 64) {
			// Parse error; terminate session
			// See EMV 4.4 Book 3, 6.5.9.4
			emv_debug_error("Invalid INTERNAL AUTHENTICATE response format 1 length of %u", response_tlv.length);
			return EMV_TAL_ERROR_INT_AUTH_PARSE_FAILED;
		}

		// Create Signed Dynamic Application Data (field 9F4B)
		r = emv_tlv_list_push(
			&response_list,
			EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA,
			response_tlv.length,
			response_tlv.value,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		emv_debug_info_tlv_list("Extracted format 1 fields", &response_list);

	} else if (response_tlv.tag == EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2) {
		// Response format 2
		// See EMV 4.4 Book 3, 6.5.9.4

		r = emv_tlv_parse(response_tlv.value, response_tlv.length, &response_list);
		if (r) {
			emv_debug_trace_msg("emv_tlv_parse() failed; r=%d", r);
			if (r < 0) {
				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_TAL_ERROR_INTERNAL;
				goto exit;
			}
			if (r > 0) {
				// Parse error; terminate session
				// See EMV 4.4 Book 3, 6.5.9.4
				emv_debug_error("Failed to parse INTERNAL AUTHENTICATE response format 2");
				r = EMV_TAL_ERROR_INT_AUTH_PARSE_FAILED;
				goto exit;
			}
		}

	} else {
		emv_debug_error("Invalid INTERNAL AUTHENTICATE response format");
		r = EMV_TAL_ERROR_INT_AUTH_PARSE_FAILED;
		goto exit;
	}

	// Ensure that Signed Dynamic Application Data (field 9F4B) is present
	sdad_tlv = emv_tlv_list_find_const(&response_list, EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA);
	if (!sdad_tlv) {
		// Mandatory field missing; terminate session
		// See EMV 4.4 Book 3, 6.5.9.4
		emv_debug_error("SDAD not found in INTERNAL AUTHENTICATE response");
		r = EMV_TAL_ERROR_INT_AUTH_FIELD_NOT_FOUND;
		goto exit;
	}

	r = emv_tlv_list_append(list, &response_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_TAL_ERROR_INTERNAL;
	}

	// Successful INTERNAL AUTHENTICATE processing
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&response_list);
	return r;
}

int emv_tal_genac(
	struct emv_ttl_t* ttl,
	uint8_t ref_ctrl,
	const void* data,
	size_t data_len,
	struct emv_tlv_list_t* list,
	struct emv_oda_ctx_t* oda
)
{
	int r;
	uint8_t response[EMV_RAPDU_DATA_MAX];
	size_t response_len = sizeof(response);
	uint16_t sw1sw2;
	struct iso8825_tlv_t response_tlv;
	struct emv_tlv_list_t response_list = EMV_TLV_LIST_INIT;
	const struct emv_tlv_t* tlv;

	if (!ttl || !list) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (data && data_len > EMV_CAPDU_DATA_MAX) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}
	if (!data && data_len) {
		// Invalid parameters; terminate session
		return EMV_TAL_ERROR_INVALID_PARAMETER;
	}

	// GENERATE APPLICATION CRYPTOGRAM
	// See EMV 4.4 Book 3, 9
	emv_debug_info("GENAC [P1=0x%02X]", ref_ctrl);
	r = emv_ttl_genac(ttl, ref_ctrl, data, data_len, response, &response_len, &sw1sw2);
	if (r) {
		emv_debug_trace_msg("emv_ttl_genac() failed; r=%d", r);

		// TTL failure; terminate session
		// (bad card or reader)
		emv_debug_error("TTL failure");
		return EMV_TAL_ERROR_TTL_FAILURE;
	}
	// See EMV 4.4 Book 3, 6.5.5.5
	if (sw1sw2 != 0x9000) {
		// Unknown error; terminate session
		emv_debug_error("SW1SW2=0x%04hX", sw1sw2);
		return EMV_TAL_ERROR_GENAC_FAILED;
	}

	emv_debug_info_ber("GENAC response", response, response_len);

	// Determine response format
	r = iso8825_ber_decode(response, response_len, &response_tlv);
	if (r <= 0) {
		emv_debug_trace_msg("iso8825_ber_decode() failed; r=%d", r);

		// Parse error; terminate session
		// See EMV 4.4 Book 3, 6.5.5.4
		emv_debug_error("Failed to parse GENAC response");
		return EMV_TAL_ERROR_GENAC_PARSE_FAILED;
	}

	if (response_tlv.tag == EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1) {
		// Response format 1
		// See EMV 4.4 Book 3, 6.5.5.4

		// Validate length
		// See EMV 4.4 Book 3, 6.5.5.4, table 13
		if (response_tlv.length < 1 + 2 + 8 ||
			response_tlv.length > 1 + 2 + 8 + 32
		) {
			// Parse error; terminate session
			// See EMV 4.4 Book 3, 6.5.5.4
			emv_debug_error("Invalid GENAC response format 1 length of %u", response_tlv.length);
			return EMV_TAL_ERROR_GENAC_PARSE_FAILED;
		}

		// Create Cryptogram Information Data (field 9F27)
		r = emv_tlv_list_push(
			&response_list,
			EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA,
			1,
			response_tlv.value,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		// Create Application Transaction Counter (field 9F36)
		r = emv_tlv_list_push(
			&response_list,
			EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER,
			2,
			response_tlv.value + 1,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		// Create Application Cryptogram (field 9F26)
		r = emv_tlv_list_push(
			&response_list,
			EMV_TAG_9F26_APPLICATION_CRYPTOGRAM,
			8,
			response_tlv.value + 1 + 2,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		if (response_tlv.length > 1 + 2 + 8) {
			// Create Issuer Application Data (field 9F10)
			r = emv_tlv_list_push(
				&response_list,
				EMV_TAG_9F10_ISSUER_APPLICATION_DATA,
				response_tlv.length - 1 - 2 - 8,
				response_tlv.value + 1 + 2 + 8,
				0
			);
			if (r) {
				emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_TAL_ERROR_INTERNAL;
				goto exit;
			}
		}

		emv_debug_info_tlv_list("Extracted format 1 fields", &response_list);

	} else if (response_tlv.tag == EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2) {
		// Response format 2
		// See EMV 4.4 Book 3, 6.5.5.4

		// NOTE: This is parsed manually instead of using emv_tlv_parse() to
		// find the encoded offset of SDAD (field 9F4B) such that the remaining
		// encoded fields can be cached. Recursive parsing of constructed
		// fields is also not required.

		struct iso8825_ber_itr_t itr;
		struct iso8825_tlv_t tlv;
		unsigned int offset_after_sdad = 0;
		unsigned int length_after_sdad = 0;

		r = iso8825_ber_itr_init(response_tlv.value, response_tlv.length, &itr);
		if (r) {
			emv_debug_trace_msg("iso8825_ber_itr_init() failed; r=%d", r);
			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_TAL_ERROR_INTERNAL;
			goto exit;
		}

		while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
			if (oda &&
				tlv.tag == EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA
			) {
				unsigned int offset_before_sdad =
					itr.ptr - (void*)response_tlv.value - r;

				if (offset_before_sdad <= sizeof(oda->genac_data)) {
					// Found SDAD (field 9F4B) and buffer has enough space,
					// therefore cache fields before SDAD
					memcpy(
						oda->genac_data,
						response_tlv.value,
						offset_before_sdad
					);
					oda->genac_data_len = offset_before_sdad;
					offset_after_sdad = offset_before_sdad + r;
					length_after_sdad = response_tlv.length - offset_after_sdad;
				}
			}

			r = emv_tlv_list_push(
				&response_list,
				tlv.tag,
				tlv.length,
				tlv.value,
				0
			);
			if (r) {
				emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
				// Internal error; terminate session
				emv_debug_error("Internal error");
				r = EMV_TAL_ERROR_INTERNAL;
				goto exit;
			}
		}
		if (r < 0) {
			emv_debug_trace_msg("iso8825_ber_itr_next() failed; r=%d", r);

			// Parse error; terminate session
			// See EMV 4.4 Book 3, 6.5.5.4
			emv_debug_error("Failed to parse GENAC response format 2");
			r = EMV_TAL_ERROR_GENAC_PARSE_FAILED;
			goto exit;
		}

		if (length_after_sdad) {
			// Fields before SDAD (field 9F4B) have already been cached and
			// more fields exists after SDAD, therefore cache fields after SDAD
			memcpy(
				oda->genac_data + oda->genac_data_len,
				response_tlv.value + offset_after_sdad,
				length_after_sdad
			);
			oda->genac_data_len += length_after_sdad;
		}

	} else {
		emv_debug_error("Invalid GENAC response format");
		r = EMV_TAL_ERROR_GENAC_PARSE_FAILED;
		goto exit;
	}

	// Ensure that Cryptogram Information Data (field 9F27) is present
	tlv = emv_tlv_list_find_const(&response_list, EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA);
	if (!tlv) {
		// Mandatory field missing; terminate session
		// See EMV 4.4 Book 3, 6.5.5.4
		emv_debug_error("CID not found in GENAC response");
		r = EMV_TAL_ERROR_GENAC_FIELD_NOT_FOUND;
		goto exit;
	}

	// Ensure that Application Transaction Counter (field 9F36) is present
	tlv = emv_tlv_list_find_const(&response_list, EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER);
	if (!tlv) {
		// Mandatory field missing; terminate session
		// See EMV 4.4 Book 3, 6.5.5.4
		emv_debug_error("ATC not found in GENAC response");
		r = EMV_TAL_ERROR_GENAC_FIELD_NOT_FOUND;
		goto exit;
	}

	if ((ref_ctrl & EMV_TTL_GENAC_SIG_MASK) == EMV_TTL_GENAC_SIG_NONE) {
		// Ensure that Application Cryptogram (field 9F26) is present
		tlv = emv_tlv_list_find_const(&response_list, EMV_TAG_9F26_APPLICATION_CRYPTOGRAM);
		if (!tlv) {
			// Mandatory field missing; terminate session
			// See EMV 4.4 Book 3, 6.5.5.4
			emv_debug_error("AC not found in GENAC response");
			r = EMV_TAL_ERROR_GENAC_FIELD_NOT_FOUND;
			goto exit;
		}
	} else {
		// Ensure that Signed Dynamic Application Data (field 9F4B) is present
		tlv = emv_tlv_list_find_const(&response_list, EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA);
		if (!tlv) {
			// Mandatory field missing; terminate session
			// See EMV 4.4 Book 3, 6.5.5.4
			emv_debug_error("SDAD not found in GENAC response");
			r = EMV_TAL_ERROR_GENAC_FIELD_NOT_FOUND;
			goto exit;
		}
	}

	r = emv_tlv_list_append(list, &response_list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_TAL_ERROR_INTERNAL;
	}

	// Successful GENERATE APPLICATION CRYPTOGRAM processing
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&response_list);
	return r;
}
