/**
 * @file emv_app.c
 * @brief EMV application abstraction and helper functions
 *
 * Copyright (c) 2021-2024 Leon Lynch
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
#include "emv_fields.h"
#include "iso8825_ber.h"
#include "iso8859.h"

#include <stdbool.h>
#include <stdlib.h> // for malloc() and free()
#include <string.h>
#include <assert.h>

// Helper functions
static int emv_app_extract_display_name(struct emv_app_t* app, const struct emv_tlv_list_t* pse_tlv_list);
static int emv_app_extract_priority_indicator(struct emv_app_t* app);
static inline bool emv_app_list_is_valid(const struct emv_app_list_t* list);

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
	app->aid = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_4F_APPLICATION_DF_NAME);
	if (!app->aid) {
		// Invalid FCI
		goto error;
	}

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
	app->aid = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_84_DF_NAME);
	if (!app->aid) {
		// Invalid FCI
		goto error;
	}

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

static int emv_app_extract_display_name(struct emv_app_t* app, const struct emv_tlv_list_t* pse_tlv_list)
{
	int r;
	const struct emv_tlv_t* issuer_code_table_index;
	unsigned int issuer_code_table = 0;
	const struct emv_tlv_t* tlv;

	if (!app) {
		return -1;
	}

	/* Find Application Preferred Name and associated Issuer Code Table Index
	 * Both are optional fields but Issuer Code Table Index is required to
	 * interpret Application Preferred Name
	 */
	if (pse_tlv_list) {
		issuer_code_table_index = emv_tlv_list_find_const(pse_tlv_list, EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX);
	} else {
		issuer_code_table_index = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX);
	}
	if (issuer_code_table_index && issuer_code_table_index->length == 1) {
		// Assume Additional Terminal Capabilities (field 9F40) was correctly
		// configured to indicate the supported code tables
		if (iso8859_is_supported(issuer_code_table_index->value[0])) {
			// Issuer Code Table Index is valid and supported
			issuer_code_table = issuer_code_table_index->value[0];
		}
	}
	if (issuer_code_table) {
		// Use Application Preferred Name as display name
		tlv = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_9F12_APPLICATION_PREFERRED_NAME);
		if (tlv) {
			// Application Preferred Name is limited to non-control characters
			// defined in the ISO/IEC 8859 part designated in the Issuer Code
			// Table
			// See EMV 4.4 Book 1, 4.3
			// See EMV 4.4 Book 1, Annex B
			char app_preferred_name[16 + 1]; // Ensure enough space for NULL termination

			// Copy only non-control characters
			r = emv_format_ans_to_non_control_str(
				tlv->value,
				tlv->length,
				app_preferred_name,
				sizeof(app_preferred_name)
			);
			if (r == 0) {
				// Convert ISO 8859 to UTF-8
				r = iso8859_to_utf8(
					issuer_code_table,
					(const uint8_t*)app_preferred_name,
					strlen(app_preferred_name),
					app->display_name,
					sizeof(app->display_name)
				);
				if (r == 0) {
					// Success
					return 0;
				}
			}
		}
	}

	// Otherwise use Application Label as display name
	tlv = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_50_APPLICATION_LABEL);
	if (tlv) {
		// Application Label is limited to a-z, A-Z, 0-9 and the space
		// See EMV 4.4 Book 1, 4.3
		// See EMV 4.4 Book 1, Annex B

		// Copy only a-z, A-Z, 0-9 and the space character
		r = emv_format_ans_to_alnum_space_str(
			tlv->value,
			tlv->length,
			app->display_name,
			sizeof(app->display_name)
		);
		return 0;
	}

	// Although the Application Label field is mandatory, the terminal shall
	// proceed if it is missing.
	// See EMV 4.4 Book 1, 12.2.4

	// Use Application Identifier (AID) as display name
	if (app->aid) {
		return emv_format_b_to_str(
			app->aid->value,
			app->aid->length,
			app->display_name,
			sizeof(app->display_name)
		);
	}

	return 1; // Mandatory field not found
}

static int emv_app_extract_priority_indicator(struct emv_app_t* app)
{
	const struct emv_tlv_t* tlv;

	if (!app) {
		return -1;
	}

	app->priority = 0;
	app->confirmation_required = false;

	tlv = emv_tlv_list_find_const(&app->tlv_list, EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR);
	if (!tlv || tlv->length < 1) {
		// Application Priority Indicator is not available; ignore
		return 0;
	}

	// See EMV 4.4 Book 1, 12.2.3, table 13
	app->priority = tlv->value[0] & EMV_APP_PRIORITY_INDICATOR_MASK;
	app->confirmation_required = tlv->value[0] & EMV_APP_PRIORITY_INDICATOR_CONF_REQUIRED;

	return 0;
}

int emv_app_free(struct emv_app_t* app)
{
	if (!app) {
		return -1;
	}
	if (app->next) {
		// EMV application is part of a list; unsafe to free
		return 1;
	}

	emv_tlv_list_clear(&app->tlv_list);
	memset(app, 0, sizeof(*app));
	free(app);

	return 0;
}

bool emv_app_is_supported(
	const struct emv_app_t* app,
	const struct emv_tlv_list_t* supported_aids
)
{
	struct emv_tlv_t* tlv;

	if (!app || !app->aid) {
		// Invalid app; not supported
		return false;
	}

	// See EMV 4.4 Book 1, 12.3.1
	for (tlv = supported_aids->front; tlv != NULL; tlv = tlv->next) {
		if (tlv->flags == EMV_ASI_EXACT_MATCH &&
			tlv->length == app->aid->length &&
			memcmp(tlv->value, app->aid->value, tlv->length) == 0
		) {
			// Exact match found; supported
			return true;
		}

		if (tlv->flags == EMV_ASI_PARTIAL_MATCH &&
			tlv->length <= app->aid->length &&
			memcmp(tlv->value, app->aid->value, tlv->length) == 0
		) {
			// Partial match found; supported
			return true;
		}
	}

	return false;
}

static inline bool emv_app_list_is_valid(const struct emv_app_list_t* list)
{
	if (!list) {
		return false;
	}

	if (list->front && !list->back) {
		return false;
	}

	if (!list->front && list->back) {
		return false;
	}

	return true;
}

bool emv_app_list_is_empty(const struct emv_app_list_t* list)
{
	if (!emv_app_list_is_valid(list)) {
		// Indicate that the list is empty to dissuade the caller from
		// attempting to access it
		return true;
	}

	return !list->front;
}

void emv_app_list_clear(struct emv_app_list_t* list)
{
	if (!emv_app_list_is_valid(list)) {
		list->front = NULL;
		list->back = NULL;
		return;
	}

	while (list->front) {
		struct emv_app_t* app;
		int r;
		int emv_app_is_safe_to_free __attribute__((unused));

		app = emv_app_list_pop(list);
		r = emv_app_free(app);

		emv_app_is_safe_to_free = r;
		assert(emv_app_is_safe_to_free == 0);
	}
	assert(list->front == NULL);
	assert(list->back == NULL);
}

int emv_app_list_push(struct emv_app_list_t* list, struct emv_app_t* app)
{
	if (!emv_app_list_is_valid(list)) {
		return -1;
	}
	if (!app) {
		return -2;
	}

	if (list->back) {
		list->back->next = app;
		list->back = app;
	} else {
		list->front = app;
		list->back = app;
	}
	app->next = NULL;

	return 0;
}

struct emv_app_t* emv_app_list_pop(struct emv_app_list_t* list)
{
	struct emv_app_t* app = NULL;

	if (!emv_app_list_is_valid(list)) {
		return NULL;
	}

	if (list->front) {
		app = list->front;
		list->front = app->next;
		if (!list->front) {
			list->back = NULL;
		}

		app->next = NULL;
	}

	return app;
}

struct emv_app_t* emv_app_list_remove_index(
	struct emv_app_list_t* list,
	unsigned int index
)
{
	struct emv_app_t* prev = NULL;

	if (!emv_app_list_is_valid(list)) {
		return NULL;
	}

	for (struct emv_app_t* app = list->front; app != NULL; app = app->next) {
		if (index == 0) {
			if (!prev) {
				// Remove app from front of list
				return emv_app_list_pop(list);
			}

			prev->next = app->next;
			if (!app->next) {
				// App at back of list
				list->back = prev;
			}
			app->next = NULL;
			return app;
		}

		// Advance and remember previous app
		--index;
		prev = app;
	}

	return NULL;
}

static int emv_app_list_insert(
	struct emv_app_list_t* list,
	struct emv_app_t* pos,
	struct emv_app_t* app
)
{
	if (!emv_app_list_is_valid(list)) {
		return -1;
	}
	if (!pos) {
		return -2;
	}
	if (!app) {
		return -3;
	}

	if (list->back == pos) {
		// Insert at back of list
		return emv_app_list_push(list, app);
	} else if (pos->next) {
		// Insert in middle of list
		app->next = pos->next;
		pos->next = app;
	} else {
		// Invalid list or position
		return -4;
	}

	return 0;
}

static int emv_app_list_push_front(
	struct emv_app_list_t* list,
	struct emv_app_t* app
)
{
	if (!emv_app_list_is_valid(list)) {
		return -1;
	}
	if (!app) {
		return -2;
	}

	if (list->front) {
		app->next = list->front;
		list->front = app;
	} else {
		list->front = app;
		list->back = app;
	}

	return 0;
}

int emv_app_list_sort_priority(struct emv_app_list_t* list)
{
	int r;
	struct emv_app_t* app;
	struct emv_app_list_t sorted_list = EMV_APP_LIST_INIT;

	if (!emv_app_list_is_valid(list)) {
		return -1;
	}

	// Given that the app list should be very short, an insertion sort is a
	// reasonable approach
	while ((app = emv_app_list_pop(list))) {
		struct emv_app_t* pos = NULL;

		// Value of 1 is the highest priority
		// See EMV 4.4 Book 1, 12.2.3, table 13
		// However, the EMV specification does not state how an application
		// without a priority indicator should be prioritised relative to an
		// application with a priority indicator, and therefore this
		// implementation chooses to favour applications with a priority
		// indicator over those without.
		for (struct emv_app_t* cur = sorted_list.front; cur != NULL; cur = cur->next) {
			if (!cur->priority) {
				break;
			}
			if (app->priority && app->priority < cur->priority) {
				break;
			}
			pos = cur;
		}
		if (pos) {
			r = emv_app_list_insert(&sorted_list, pos, app);
		} else {
			r = emv_app_list_push_front(&sorted_list, app);
		}
		if (r) {
			emv_app_list_clear(&sorted_list);
			return -2;
		}
	}

	*list = sorted_list;
	return 0;
}

bool emv_app_list_selection_is_required(const struct emv_app_list_t* list)
{
	size_t app_count = 0;

	if (!emv_app_list_is_valid(list)) {
		return false;
	}

	for (const struct emv_app_t* app = list->front; app != NULL; app = app->next) {
		if (app->confirmation_required) {
			return true;
		}
		++app_count;

		if (app_count > 1) {
			return true;
		}
	}

	return false;
}
