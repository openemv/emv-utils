/**
 * @file emv_tal.c
 * @brief EMV Terminal Application Layer (TAL)
 *
 * Copyright (c) 2021 Leon Lynch
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

#include "iso8825_ber.h"

#include <stdint.h>
#include <string.h>

// Helper functions
static int emv_tal_parse_aef_record(
	struct emv_tlv_list_t* pse_tlv_list,
	const void* aef_record,
	size_t aef_record_len,
	struct emv_app_list_t* app_list
);

int emv_tal_read_pse(
	struct emv_ttl_t* ttl,
	struct emv_app_list_t* app_list
)
{
	int r;
	const uint8_t PSE[] = "1PAY.SYS.DDF01";
	uint8_t fci[EMV_RAPDU_DATA_MAX];
	size_t fci_len = sizeof(fci);
	uint16_t sw1sw2;
	struct emv_tlv_list_t pse_tlv_list = EMV_TLV_LIST_INIT;

	const struct emv_tlv_t* pse_sfi;

	if (!ttl || !app_list) {
		// Invalid parameters; terminate session
		return -1;
	}

	// SELECT Payment System Environment (PSE) Directory Definition File (DDF)
	// See EMV 4.3 Book 1, 12.2.2
	// See EMV 4.3 Book 1, 12.3.2
	r = emv_ttl_select_by_df_name(ttl, PSE, sizeof(PSE) - 1, fci, &fci_len, &sw1sw2);
	if (r) {
		// TTL failure; terminate session
		// (bad card or reader)
		return -2;
	}

	if (sw1sw2 == 0x6A81) {
		// Card blocked or SELECT not supported; terminate session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		return -3;
	}

	if (sw1sw2 == 0x6A82) {
		// PSE not found; terminal may continue session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		return 1;
	}

	if (sw1sw2 == 0x6283) {
		// PSE is blocked; terminal may continue session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		return 2;
	}

	if (sw1sw2 != 0x9000) {
		// Failed to SELECT PSE; terminal may continue session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		return 3;
	}

	// TODO: log FCI data here

	// Parse File Control Information (FCI) provided by PSE DDF
	// NOTE: FCI may contain padding (r > 0)
	// See EMV 4.3 Book 1, 11.3.4, table 43
	r = emv_tlv_parse(fci, fci_len, &pse_tlv_list);
	if (r < 0) {
		// Internal error while parsing FCI data; terminal may continue session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		r = 4;
		goto exit;
	}

	// Find Short File Identifier (SFI) for PSE directory Application Elementary File (AEF)
	// See EMV 4.3 Book 1, 11.3.4, table 43
	pse_sfi = emv_tlv_list_find(&pse_tlv_list, EMV_TAG_88_SFI);
	if (!pse_sfi) {
		// Failed to find SFI for PSE records; terminal may continue session
		// See EMV 4.3 Book 1, 12.3.2, step 1
		r = 5;
		goto exit;
	}

	// Read all records from PSE AEF using the SFI
	// See EMV 4.3 Book 1, 12.2.3
	for (uint8_t record_number = 1; ; ++record_number) {
		uint8_t aef_record[EMV_RAPDU_DATA_MAX];
		size_t aef_record_len = sizeof(aef_record);

		// Read PSE AEF record
		// See EMV 4.3 Book 1, 12.2.3, table 47
		r = emv_ttl_read_record(ttl, pse_sfi->value[0], record_number, aef_record, &aef_record_len, &sw1sw2);
		if (r) {
			// TTL failure; terminate session
			// (bad card or reader; infinite loop if we continue)
			r = -4;
			goto exit;
		}

		if (sw1sw2 == 0x6A83) {
			// No more records
			// See EMV 4.3 Book 1, 12.3.2, step 2
			break;
		}

		if (sw1sw2 != 0x9000) {
			// Unexpected error; ignore record and continue
			// See EMV 4.3 Book 1, 12.3.2, step 2
			continue;
		}

		r = emv_tal_parse_aef_record(
			&pse_tlv_list,
			aef_record,
			aef_record_len,
			app_list
		);
		if (r) {
			// Failed to parse PSE AEF record
			goto exit;
		}
	}

	// Successful PSE processing
	r = 0;

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
	struct emv_app_list_t* app_list
)
{
	int r;
	struct iso8825_tlv_t aef_template_tlv;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	// Record should contain AEF template (field 70)
	// See EMV 4.3 Book 1, 12.2.3, table 46
	r = iso8825_ber_decode(aef_record, aef_record_len, &aef_template_tlv);
	if (r <= 0) {
		// Invalid PSE AEF record; ignore and continue
		return 0;
	}
	if (aef_template_tlv.tag != EMV_TAG_70_DATA_TEMPLATE) {
		// Record doesn't contain AEF template; ignore and continue
		return 0;
	}

	// NOTE: AEF template (field 70) may contain multiple Application Templates (field 61)
	// See EMV 4.3 Book 1, 10.1.4
	// See EMV 4.3 Book 1, 12.2.3

	r = iso8825_ber_itr_init(aef_template_tlv.value, aef_template_tlv.length, &itr);
	if (r) {
		// Internal error; terminate session
		return -5;
	}

	// Iterate Application Templates (field 61)
	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		struct emv_app_t* app;

		if (tlv.tag != EMV_TAG_61_APPLICATION_TEMPLATE) {
			// Ignore unexpected data elements in AEF template
			// See EMV 4.3 Book 1, 12.2.3
			continue;
		}

		// Create EMV application object
		app = emv_app_create_from_pse(pse_tlv_list, tlv.value, tlv.length);
		if (!app) {
			// Ignore invalid Application Template (field 61) content
			// See EMV 4.3 Book 1, 12.2.3
			continue;
		}

		emv_app_list_push(app_list, app);
	}

	return 0;
}

int emv_tal_find_supported_apps(
	struct emv_ttl_t* ttl,
	struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
)
{
	int r;
	struct emv_tlv_t* aid;
	bool exact_match;

	if (!ttl || !supported_aids || !app_list) {
		// Invalid parameters; terminate session
		return -1;
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
			// See EMV 4.3 Book 1, 12.3.3, step 1
			r = emv_ttl_select_by_df_name(ttl, aid->value, aid->length, fci, &fci_len, &sw1sw2);
			if (r) {
				// TTL failure; terminate session
				// (bad card or reader)
				return -2;
			}

			if (sw1sw2 == 0x6A81) {
				// Card blocked or SELECT not supported; terminate session
				// See EMV 4.3 Book 1, 12.3.3, step 2
				return -3;
			}

		} else {
			// SELECT next application for partial AID match
			// See EMV 4.3 Book 1, 12.3.3, step 7
			r = emv_ttl_select_by_df_name_next(ttl, aid->value, aid->length, fci, &fci_len, &sw1sw2);
			if (r) {
				// TTL failure; terminate session
				// (bad card or reader)
				return -2;
			}

		}

		if (sw1sw2 != 0x9000 && sw1sw2 != 0x6283) {
			// Unexpected error; ignore app and continue to next supported AID
			// See EMV 4.3 Book 1, 12.3.3, step 3
			aid = aid->next;
			exact_match = true;
			continue;
		}

		// Extract FCI data
		// See EMV 4.3 Book 1, 12.3.3, step 3
		app = emv_app_create_from_fci(fci, fci_len);
		if (!app) {
			// Unexpected error; ignore app and continue to next supported AID
			// See EMV 4.3 Book 1, 12.3.3, step 3
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
			// See EMV 4.3 Book 1, 12.3.3, step 4

			if (sw1sw2 == 0x9000) {
				// Valid app; add app and continue to next supported AID
				// See EMV 4.3 Book 1, 12.3.3, step 4
				emv_app_list_push(app_list, app);
			} else {
				// Blocked app; ignore app and continue to next supported AID
				// See EMV 4.3 Book 1, 12.3.3, step 4
				emv_app_free(app);
				app = NULL;
			}

			// See EMV 4.3 Book 1, 12.3.3, step 5
			aid = aid->next;
			exact_match = true;
			continue;

		} else {
			// Partial match; check Application Selection Indicator (ASI)
			// See EMV 4.3 Book 1, 12.3.3, step 6

			if (aid->flags == EMV_ASI_PARTIAL_MATCH) {
				// Partial match allowed; check whether valid or blocked

				if (sw1sw2 == 0x9000) {
					// Valid app; add app and continue to next partial AID
					// See EMV 4.3 Book 1, 12.3.3, step 6
					emv_app_list_push(app_list, app);
				} else {
					// Blocked app; ignore app and continue to next partial AID
					// See EMV 4.3 Book 1, 12.3.3, step 6
					emv_app_free(app);
					app = NULL;
				}

				// See EMV 4.3 Book 1, 12.3.3, step 7
				exact_match = false;
				continue;

			} else {
				// Partial match not allowed; ignore app and continue to next supported AID
				// See EMV 4.3 Book 1, 12.3.3, step 6
				emv_app_free(app);
				app = NULL;

				// See EMV 4.3 Book 1, 12.3.3, step 5
				aid = aid->next;
				exact_match = true;
				continue;
			}
		}
	}

	return 0;
}

int emv_tal_parse_gpo_response(
	const void* buf,
	size_t len,
	struct emv_tlv_list_t* list,
	struct emv_tlv_t** aip,
	struct emv_tlv_t** afl
)
{
	int r;
	struct iso8825_tlv_t gpo_tlv;
	struct emv_tlv_list_t gpo_list = EMV_TLV_LIST_INIT;
	struct emv_tlv_t* tlv;

	if (!buf || !len || !list) {
		return -1;
	}

	// Determine GPO response format
	r = iso8825_ber_decode(buf, len, &gpo_tlv);
	if (r <= 0) {
		// Parse error
		return 1;
	}

	if (gpo_tlv.tag == EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1) {
		// GPO response format 1
		// See EMV 4.3 Book 3, 6.5.8.4

		// Validate length
		// See EMV 4.3 Book 3, 10.2
		// AIP is 2 bytes
		// AFL is multiples of 4 bytes
		if (gpo_tlv.length < 6 || // AIP and at least one AFL entry
			((gpo_tlv.length - 2) & 0x3) != 0 // AFL is multiple of 4 bytes
		) {
			// Parse error
			return 2;
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
			// Internal error
			emv_tlv_list_clear(&gpo_list);
			return -2;
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
			// Internal error
			emv_tlv_list_clear(&gpo_list);
			return -3;
		}
	}

	if (gpo_tlv.tag == EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2) {
		// GPO response format 2
		// See EMV 4.3 Book 3, 6.5.8.4

		r = emv_tlv_parse(gpo_tlv.value, gpo_tlv.length, &gpo_list);
		if (r < 0) {
			// Internal error
			emv_tlv_list_clear(&gpo_list);
			return -4;
		}
		if (r > 0) {
			// Parse error
			emv_tlv_list_clear(&gpo_list);
			return 3;
		}
	}

	// Populate AIP pointer
	tlv = emv_tlv_list_find(&gpo_list, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (tlv && aip) {
		*aip = tlv;
	}

	// Populate AFL pointer
	tlv = emv_tlv_list_find(&gpo_list, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (tlv && afl) {
		*afl = tlv;
	}

	r = emv_tlv_list_append(list, &gpo_list);
	if (r) {
		// Internal error
		emv_tlv_list_clear(&gpo_list);
		return -5;
	}

	return 0;
}
