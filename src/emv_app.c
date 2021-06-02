/**
 * @file emv_app.c
 * @brief EMV application abstraction and helper functions
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

#include "emv_app.h"
#include "emv_tags.h"
#include "iso8825_ber.h"

#include <stdlib.h> // for malloc() and free()
#include <string.h>

// Helper functions
static int emv_app_extract_display_name(struct emv_app_t* app, struct emv_tlv_list_t* pse_tlv_list);
static int emv_app_extract_priority_indicator(struct emv_app_t* app);

struct emv_app_t* emv_app_create_from_pse(
	struct emv_tlv_list_t* pse_tlv_list,
	const void* pse_dir_entry,
	size_t pse_dir_entry_len
)
{
	int r;
	struct emv_app_t* app;

	app = malloc(sizeof(*app));
	if (!app) {
		return NULL;
	}
	memset(app, 0, sizeof(*app));

	// Parse PSE dir entry
	r = emv_tlv_parse(pse_dir_entry, pse_dir_entry_len, &app->tlv_list);
	if (r < 0) {
		// Internal error
		goto error;
	}
	if (r > 0) {
		// Parse error
		goto error;
	}

	// Use ADF Name field for AID
	app->aid = emv_tlv_list_find(&app->tlv_list, EMV_TAG_4F_APPLICATION_DF_NAME);

	r = emv_app_extract_display_name(app, pse_tlv_list);
	if (r) {
		goto error;
	}

	r = emv_app_extract_priority_indicator(app);
	if (r) {
		goto error;
	}

	return app;

error:
	emv_app_free(app);
	return NULL;
}

struct emv_app_t* emv_app_create_from_fci(const void* fci, size_t fci_len)
{
	int r;
	struct iso8825_tlv_t tlv;
	struct emv_app_t* app;

	app = malloc(sizeof(*app));
	if (!app) {
		return NULL;
	}
	memset(app, 0, sizeof(*app));

	// Parse FCI template
	r = iso8825_ber_decode(fci, fci_len, &tlv);
	if (r != fci_len) {
		// Parse error
		goto error;
	}
	if (tlv.tag != EMV_TAG_6F_FCI_TEMPLATE) {
		// Parse error
		goto error;
	}

	// Parse FCI data
	r = emv_tlv_parse(tlv.value, tlv.length, &app->tlv_list);
	if (r < 0) {
		// Internal error
		goto error;
	}
	if (r > 0) {
		// Parse error
		goto error;
	}

	// Use DF Name field for AID
	app->aid = emv_tlv_list_find(&app->tlv_list, EMV_TAG_84_DF_NAME);

	r = emv_app_extract_display_name(app, NULL);
	if (r) {
		goto error;
	}

	r = emv_app_extract_priority_indicator(app);
	if (r) {
		goto error;
	}

	return app;

error:
	emv_app_free(app);
	return NULL;
}

static int emv_app_extract_display_name(struct emv_app_t* app, struct emv_tlv_list_t* pse_tlv_list)
{
	struct emv_tlv_t* issuer_code_table_index;
	struct emv_tlv_t* tlv;

	/* Find Application Preferred Name and associated Issuer Code Table Index
	 * Both are optional fields but Issuer Code Table Index is required to
	 * interpret Application Preferred Name
	 */
	if (pse_tlv_list) {
		issuer_code_table_index = emv_tlv_list_find(pse_tlv_list, EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX);
	} else {
		issuer_code_table_index = emv_tlv_list_find(&app->tlv_list, EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX);
	}
	if (issuer_code_table_index) {
		// Use Application Preferred Name as display name
		tlv = emv_tlv_list_find(&app->tlv_list, EMV_TAG_9F12_APPLICATION_PREFERRED_NAME);
		if (tlv) {
			// TODO: convert ISO 8859 to UTF-8
			// HACK: just copy it as-is for now
			memcpy(app->display_name, tlv->value, tlv->length);
			app->display_name[tlv->length] = 0;
			return 0;
		}
	}

	// Otherwise use Application Label as display name
	tlv = emv_tlv_list_find(&app->tlv_list, EMV_TAG_50_APPLICATION_LABEL);
	if (tlv) {
		// Application Label is limited to a-z, A-Z, 0-9 and the space
		// character from ISO 8859, so just copy it as-is for now
		memcpy(app->display_name, tlv->value, tlv->length);
		app->display_name[tlv->length] = 0;
		return 0;
	}

	// Application Label is mandatory
	// See EMV 4.3 Book 1, 12.2.3, table 47
	// See EMV 4.3 Book 1, 11.3.4, table 45

	return 1; // Mandatory field not found
}

static int emv_app_extract_priority_indicator(struct emv_app_t* app)
{
	struct emv_tlv_t* tlv;

	app->priority = 0;
	app->confirmation_required = false;

	tlv = emv_tlv_list_find(&app->tlv_list, EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR);
	if (!tlv || tlv->length < 1) {
		// Application Priority Indicator is not available; ignore
		return 0;
	}

	// See EMV 4.3 Book 1, 12.2.3, table 48
	app->priority = tlv->value[0] & 0x0F;
	app->confirmation_required = tlv->value[0] & 0x80;

	return 0;
}

int emv_app_free(struct emv_app_t* app)
{
	if (!app) {
		return -1;
	}

	emv_tlv_list_clear(&app->tlv_list);
	memset(app, 0, sizeof(*app));
	free(app);

	return 0;
}
