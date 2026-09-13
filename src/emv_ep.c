/**
 * @file emv_ep.c
 * @brief High level EMV contactless Entry Point interface
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

#include "emv_ep.h"
#include "emv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_tlv.h"
#include "emv_dol.h"
#include "emv_app.h"
#include "emv_ttl.h"
#include "emv_tal.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h> // For malloc() and free()
#include <string.h>
#include <assert.h>

static struct emv_ep_app_t* emv_ep_app_alloc(
	const struct emv_config_app_t* config_app,
	const uint8_t* kernel_id
)
{
	struct emv_ep_app_t* app;

	app = malloc(sizeof(*app));
	if (!app) {
		return NULL;
	}
	memset(app, 0, sizeof(*app));

	app->config = config_app;
	memcpy(app->kernel_id, kernel_id, sizeof(app->kernel_id));

	return app;
}

static int emv_ep_app_free(struct emv_ep_app_t* app)
{
	if (!app) {
		return -1;
	}
	if (app->next) {
		// EMV contactless application combination is part of a list; unsafe to free
		return 1;
	}

	free(app);

	return 0;
}

static inline bool emv_ep_app_list_is_valid(const struct emv_ep_app_list_t* list)
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

static struct emv_ep_app_t* emv_ep_app_list_pop(struct emv_ep_app_list_t* list)
{
	struct emv_ep_app_t* app = NULL;

	if (!emv_ep_app_list_is_valid(list)) {
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

int emv_ep_app_list_push(
	struct emv_ep_app_list_t* list,
	const struct emv_config_app_t* config_app,
	const uint8_t* kernel_id
)
{
	struct emv_ep_app_t* app;

	if (!emv_ep_app_list_is_valid(list)) {
		return -1;
	}

	if (!kernel_id) {
		return -2;
	}

	app = emv_ep_app_alloc(config_app, kernel_id);
	if (!app) {
		return -3;
	}

	if (list->back) {
		list->back->next = app;
		list->back = app;
	} else {
		list->front = app;
		list->back = app;
	}

	return 0;
}

bool emv_ep_app_list_is_empty(const struct emv_ep_app_list_t* list)
{
	if (!emv_ep_app_list_is_valid(list)) {
		// Indicate that the list is empty to dissuade the caller from
		// attempting to access it
		return true;
	}

	return !list->front;
}

void emv_ep_app_list_clear(struct emv_ep_app_list_t* list)
{
	if (!emv_ep_app_list_is_valid(list)) {
		list->front = NULL;
		list->back = NULL;
		return;
	}

	while (list->front) {
		struct emv_ep_app_t* app;
		int r;
		int emv_ep_app_is_safe_to_free __attribute__((unused));

		app = emv_ep_app_list_pop(list);
		r = emv_ep_app_free(app);

		emv_ep_app_is_safe_to_free = r;
		assert(emv_ep_app_is_safe_to_free == 0);
	}
	assert(list->front == NULL);
	assert(list->back == NULL);
}

static inline bool emv_card_is_contactless(const struct emv_ctx_t* ctx)
{
	return (ctx && ctx->ttl && ctx->ttl->contactless);
}

int emv_ep_preprocess(
	const struct emv_config_t* config,
	uint32_t amount,
	struct emv_ep_app_list_t* list
)
{
	int r;
	struct emv_config_app_itr_t itr;
	const struct emv_config_app_t* config_app;
	unsigned int contactless_not_allowed = 0;

	if (!config || !list) {
		emv_debug_trace_msg("config=%p, list=%p", config, list);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!emv_ep_app_list_is_empty(list)) {
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	r = emv_config_app_itr_init(config, &itr);
	if (r) {
		emv_debug_trace_msg("emv_config_app_itr_init() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	while ((config_app = emv_config_app_itr_next(&itr)) != NULL) {
		const struct emv_tlv_t* kernel_id_config;
		const struct emv_tlv_t* ttq_config;

		// Ignore non-contactless applications
		kernel_id_config = emv_tlv_list_find_const(
			&config_app->data,
			EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL
		);
		if (!kernel_id_config || kernel_id_config->length != 8) {
			emv_debug_trace_data("Combination does not support contactless",
				config_app->aid, config_app->aid_len
			);

			// Ignore app and continue
			continue;
		}

		// Pre-process Contactless Transaction Limit
		if (config_app->contactless_transaction_limit_enabled) {
			emv_debug_trace_msg("Contactless Transaction Limit value is %u",
				config_app->contactless_transaction_limit
			);
			// See EMV Contactless Book B v2.11, 3.1.1.5
			if (amount >= config_app->contactless_transaction_limit) {
				emv_debug_trace_data("Contactless Transaction Limit exceeded for combination",
					config_app->aid, config_app->aid_len
				);

				// Contactless Application Not Allowed indicator
				contactless_not_allowed++;
				continue;
			}
		}

		// Pre-process TTQ for applications that configure it
		// See EMV Contactless Book B v2.11, 3.1.1.2
		ttq_config = emv_tlv_list_find_const(
			&config_app->data,
			EMV_TAG_9F66_TTQ
		);
		if (ttq_config) {
			if (ttq_config->length != 4) {
				emv_debug_trace_data("Terminal Transaction Qualifiers (9F66) invalid",
					config_app->aid, config_app->aid_len
				);

				// Ignore app and continue
				continue;
			}

			// See EMV Contactless Book B v2.11, 3.1.1.11
			if (amount == 0 &&
				(ttq_config->value[0] & EMV_TTQ_OFFLINE_ONLY_READER)
			) {
				emv_debug_trace_data("Contactless Zero Amount not allowed for combination",
					config_app->aid, config_app->aid_len
				);

				// Contactless Application Not Allowed indicator
				contactless_not_allowed++;
				continue;
			}
		}

		// Valid combination
		emv_debug_info_data("Combination is valid for kernel 0x%02X",
			config_app->aid,
			config_app->aid_len,
			kernel_id_config->value[0]
		);
		r = emv_ep_app_list_push(
			list,
			config_app,
			kernel_id_config->value
		);
		if (r) {
			emv_debug_trace_msg("emv_ep_app_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;
		}
	}

	// If there are no supported combinations, terminate session
	if (emv_ep_app_list_is_empty(list)) {
		if (contactless_not_allowed > 0) {
			// If due to Contactless Application Not Allowed indicator, outcome
			// is Try Another Interface
			// See EMV Contactless Book B v2.11, 3.1.1.13
			emv_debug_info("Contactless application not allowed; try another interface");
			return EMV_OUTCOME_TRY_ANOTHER_INTERFACE;
		} else {
			// If no supported combinations for other reasons, outcome is
			// End Application
			// See EMV Contactless Book B v2.11, 3.3.2.7
			emv_debug_info("Combination list empty; try another card");
			return EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		}
	}

	return 0;
}

static int emv_ep_extract_requested_kernel_id(
	const struct emv_app_t* app,
	uint8_t* requested_kernel_id
)
{
	int r;
	const struct emv_tlv_t* kernel_id_icc;

	if (!app || !requested_kernel_id) {
		emv_debug_trace_msg("app=%p, requested_kernel_id=%p", app, requested_kernel_id);
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	emv_debug_trace_data("app",
		app->aid->value, app->aid->length
	);

	// Extract Requested Kernel ID
	// See EMV Contactless Book B v2.11, 3.3.2.5, step 2C
	memset(requested_kernel_id, 0, 3);
	kernel_id_icc = emv_tlv_list_find_const(
		&app->tlv_list,
		EMV_TAG_9F2A_KERNEL_IDENTIFIER
	);
	if (kernel_id_icc && kernel_id_icc->length > 8) {
		emv_debug_trace_msg("Kernel ID length %u", kernel_id_icc->length);
		emv_debug_error("Invalid PPSE directory entry kernel ID length");
		return EMV_OUTCOME_CARD_ERROR;
	}
	if (kernel_id_icc &&
		kernel_id_icc->length > 0 &&
		(kernel_id_icc->length > 1 || kernel_id_icc->value[0] != 0)
	) {
		uint8_t kernel_id_type = kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK;

		switch (kernel_id_type) {
			case EMV_KERNEL_ID_TYPE_INTERNATIONAL:
			case EMV_KERNEL_ID_TYPE_RFU:
				requested_kernel_id[0] = kernel_id_icc->value[0];
				break;

			case EMV_KERNEL_ID_TYPE_DOMESTIC_EMVCO:
			case EMV_KERNEL_ID_TYPE_DOMESTIC_PROPRIETARY:
				if (kernel_id_icc->length < 3) {
					emv_debug_error("Invalid PPSE directory entry domestic kernel ID");
					return EMV_OUTCOME_CARD_ERROR;
				}
				if ((kernel_id_icc->value[0] & EMV_KERNEL_ID_SHORT_MASK) == 0) {
					emv_debug_error("Proprietary PPSE directory entry domestic kernel ID");
					return EMV_OUTCOME_CARD_ERROR;
				}

				memcpy(requested_kernel_id, kernel_id_icc->value, 3);
				break;

			default:
				emv_debug_error("Internal error");
				return EMV_ERROR_INTERNAL;
		}
	} else {
		struct emv_aid_info_t aid_info;
		r = emv_aid_get_info(app->aid->value, app->aid->length, &aid_info);
		if (r) {
			emv_debug_trace_msg("emv_aid_get_info() failed; r=%d", r);
			emv_debug_error("Invalid PPSE directory entry AID");
			return EMV_ERROR_INTERNAL;
		}

		// See EMV Contactless Book B v2.11, 3.3.2.5, table 3-6
		switch (aid_info.scheme) {
			case EMV_CARD_SCHEME_MASTERCARD: requested_kernel_id[0] = 2; break;
			case EMV_CARD_SCHEME_VISA: requested_kernel_id[0] = 3; break;
			case EMV_CARD_SCHEME_AMEX: requested_kernel_id[0] = 4; break;
			case EMV_CARD_SCHEME_JCB: requested_kernel_id[0] = 5; break;
			case EMV_CARD_SCHEME_DISCOVER: requested_kernel_id[0] = 6; break;
			case EMV_CARD_SCHEME_UNIONPAY: requested_kernel_id[0] = 7; break;
			default: requested_kernel_id[0] = 0; break;
		}
	}
	emv_debug_trace_data("requested_kernel_id", requested_kernel_id, 3);

	return 0;
}

static bool emv_ep_combination_is_supported(
	const struct emv_ep_app_t* combination,
	const struct emv_tlv_t* aid,
	const uint8_t* requested_kernel_id
)
{
	const struct emv_config_app_t* config_app;

	if (!combination || !aid || !requested_kernel_id) {
		emv_debug_trace_msg("combination=%p, aid=%p, requested_kernel_id=%p",
			combination, aid, requested_kernel_id);
		emv_debug_error("Internal error");
		return false;
	}

	if (!combination->config) {
		// Skip invalid contactless application combination
		emv_debug_error("Application combination has no config");
		return false;
	}
	config_app = combination->config;

	// See EMV Contactless Book B v2.11, 3.3.2.5, step 2B
	// See EMV 4.4 Book 1, 12.3.1
	if (config_app->asi == EMV_ASI_EXACT_MATCH) {
		if (config_app->aid_len != aid->length ||
			memcmp(config_app->aid, aid->value, config_app->aid_len) != 0
		) {
			// Exact match failed; skip combination
			return false;
		}
	} else if (config_app->asi == EMV_ASI_PARTIAL_MATCH) {
		if (config_app->aid_len > aid->length ||
			memcmp(config_app->aid, aid->value, config_app->aid_len) != 0
		) {
			// Partial match failed; skip combination
			return false;
		}
	} else {
		// Invalid Application Selection Indicator (ASI); skip combination
		emv_debug_error("Application combination has invalid ASI 0x%02X",
			config_app->asi
		);
		return false;
	}

	// See EMV Contactless Book B v2.11, 3.3.2.5, step 2D
	if (requested_kernel_id[0] == 0 ||
		memcmp(requested_kernel_id, combination->kernel_id, 3) == 0
	) {
		return true;
	}

	return false;
}

int emv_ep_build_candidate_list(
	const struct emv_ctx_t* ctx,
	const struct emv_ep_app_list_t* ep_list,
	struct emv_app_list_t* app_list
)
{
	int r;
	struct emv_app_list_t ppse_list = EMV_APP_LIST_INIT;
	struct emv_app_t* app = NULL;
	struct emv_app_t* candidate = NULL;

	if (!ctx || !ep_list || !app_list) {
		emv_debug_trace_msg("ctx=%p, ep_list=%p, app_list=%p",
			ctx, ep_list, app_list);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (!emv_card_is_contactless(ctx)) {
		emv_debug_trace_msg("emv_ep_build_candidate_list() called for non-contactless");
		emv_debug_error("Entry Point not supported for non-contactless");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("Select Proximity Payment System Environment (PPSE)");
	r = emv_tal_read_ppse(ctx->ttl, &ppse_list);
	if (r < 0) {
		emv_debug_trace_msg("emv_tal_read_ppse() failed; r=%d", r);
		emv_debug_error("Failed to read PPSE; terminate session");
		r = EMV_OUTCOME_CARD_ERROR;
		goto exit;
	}
	if (r > 0) {
		emv_debug_trace_msg("emv_tal_read_ppse() failed; r=%d", r);

		// If PPSE failed, outcome is End Application
		// See EMV Contactless Book B v2.11, 3.3.2.3
		emv_debug_info("Failed to process PPSE; try another card");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto exit;
	}

	// See EMV Contactless Book B v2.11, 3.3.2.5
	while ((app = emv_app_list_pop(&ppse_list))) {
		uint8_t requested_kernel_id[3];
		const struct emv_ep_app_t* combination;

		// See EMV Contactless Book B v2.11, 3.3.2.5, step 2C
		r = emv_ep_extract_requested_kernel_id(app, requested_kernel_id);
		if (r < 0) {
			// Internal error; terminate session
			emv_app_free(app);
			app = NULL;
			goto exit;
		}
		if (r > 0) {
			// Failed to extract requested kernel id; skip application
			emv_app_free(app);
			app = NULL;
			continue;
		}

		// For each application, add every supported combination to the
		// candidate list
		for (
			combination = ep_list->front;
			combination != NULL;
			combination = combination->next
		) {
			bool supported;

			// See EMV Contactless Book B v2.11, 3.3.2.5, step 2D
			supported = emv_ep_combination_is_supported(
				combination,
				app->aid,
				requested_kernel_id
			);

			if (!supported) {
				emv_debug_info("Combination is not supported");

				// Ignore combination and continue
				continue;
			}

			// See EMV Contactless Book B v2.11, 3.3.2.5, step 2E
			emv_debug_info_data("Combination is supported for kernel 0x%02X",
				combination->config->aid,
				combination->config->aid_len,
				combination->kernel_id[0]
			);
			candidate = emv_app_clone(app);
			if (!candidate) {
				emv_debug_trace_msg("emv_app_clone() failed");
				emv_debug_error("Internal error");
				r = EMV_ERROR_INTERNAL;
				goto exit;
			}
			candidate->config = combination->config;
			r = emv_app_list_push(app_list, candidate);
			if (r) {
				emv_debug_trace_msg("emv_app_list_push() failed; r=%d", r);
				emv_debug_error("Internal error");
				r = EMV_ERROR_INTERNAL;
				goto exit;
			}
			candidate = NULL;
		}

		// Cleanup
		emv_app_free(app);
		app = NULL;
	}

	// If there are no mutually supported applications, outcome is
	// End Application
	// See EMV Contactless Book B v2.11, 3.3.2.7
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Candidate list empty; try another card");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
		goto exit;
	}

	// Sort application list according to priority
	// See EMV Contactless Book B v2.11, 3.3.3.2
	r = emv_app_list_sort_priority(app_list);
	if (r) {
		emv_debug_trace_msg("emv_app_list_sort_priority() failed; r=%d", r);
		emv_debug_error("Failed to sort application list; terminate session");
		r = EMV_ERROR_INTERNAL;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	if (candidate) {
		emv_app_free(candidate);
		candidate = NULL;
	}
	if (app) {
		emv_app_free(app);
		app = NULL;
	}
	emv_app_list_clear(&ppse_list);

	return r;
}

static int emv_ep_extract_selected_kernel_id(
	const struct emv_app_t* selected_app,
	uint8_t* kernel_id
)
{
	const struct emv_tlv_t* kernel_id_icc;
	const struct emv_tlv_t* kernel_id_config;

	if (!selected_app || !kernel_id) {
		emv_debug_trace_msg("selected_app=%p, kernel_id=%p",
			selected_app, kernel_id);
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}
	memset(kernel_id, 0, 3);

	// NOTE: emv_ep_select_application() copies Kernel Identifier (field 9F2A)
	// when available
	kernel_id_icc = emv_tlv_list_find_const(
		&selected_app->tlv_list,
		EMV_TAG_9F2A_KERNEL_IDENTIFIER
	);

	kernel_id_config = emv_tlv_list_find_const(
		&selected_app->config->data,
		EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL
	);
	if (!kernel_id_config || kernel_id_config->length != 8) {
		emv_debug_error("Kernel Identifier - Terminal (96) not found or invalid");
		return EMV_ERROR_INVALID_CONFIG;
	}

	// See EMV Contactless Book B v2.11, 3.4.1.4
	memcpy(kernel_id, kernel_id_config->value, 3);
	if (!kernel_id_icc ||
		kernel_id_icc->length < 3 ||
		(kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_INTERNATIONAL ||
		(kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_RFU
	) {
		// Kernel Identifier (field 9F2A) not found or
		// Extended Kernel ID not present
		kernel_id[1] = 0;
		kernel_id[2] = 0;
	}

	return 0;
}

int emv_ep_select_application(
	struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list,
	uint8_t* kernel_id
)
{
	int r;
	struct emv_app_t* current_app = NULL;
	struct emv_aid_info_t aid_info;
	const struct emv_tlv_t* kernel_id_icc;

	if (!ctx || !app_list) {
		emv_debug_trace_msg("ctx=%p, app_list=%p", ctx, app_list);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (!emv_card_is_contactless(ctx)) {
		emv_debug_trace_msg("emv_ep_select_application() called for non-contactless");
		emv_debug_error("Entry Point not supported for non-contactless");
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

	// See EMV Contactless Book B v2.11, 3.3.3.2
	current_app = emv_app_list_pop(app_list);
	if (!current_app) {
		emv_debug_trace_msg("emv_app_list_pop() failed");
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!current_app->config) {
		emv_debug_error("Candidate application has no config");
		goto select_next;
	}

	// See EMV Contactless Book B v2.11, 3.3.3.4
	r = emv_tal_select_app(
		ctx->ttl,
		current_app->aid->value,
		current_app->aid->length,
		&ctx->selected_app
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_select_app() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Error during application selection");

			if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
				r = EMV_ERROR_INTERNAL;
			} else {
				// See EMV Contactless Book B v2.11, 3.3.3.7
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto error;
		}
		if (r > 0) {
			emv_debug_info("Failed to select application");

			// See EMV Contactless Book B v2.11, 3.3.3.5
			goto select_next;
		}
	}
	if (!ctx->selected_app) {
		emv_debug_trace_msg("emv_tal_select_app() failed to populate selected_app");
		emv_debug_error("Internal error");
		r = EMV_ERROR_INTERNAL;
		goto error;
	}

	// Populate matching application dependent data
	emv_debug_info_tlv_list("Application dependent data", &current_app->config->data);
	ctx->selected_app->config = current_app->config;

	// Kernel Identifier (field 9F2A) is only available in the PPSE
	// directory entry, not the FCI response of the application selection.
	// But it is needed for Kernel Identifier - Terminal (field 96) later.
	kernel_id_icc = emv_tlv_list_find_const(
		&current_app->tlv_list,
		EMV_TAG_9F2A_KERNEL_IDENTIFIER
	);
	if (kernel_id_icc &&
		kernel_id_icc->length > 0 &&
		kernel_id_icc->length <= 8
	) {
		r = emv_tlv_list_push(
			&ctx->selected_app->tlv_list,
			EMV_TAG_9F2A_KERNEL_IDENTIFIER,
			kernel_id_icc->length,
			kernel_id_icc->value,
			0
		);
		if (r) {
			emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);

			// Internal error; terminate session
			emv_debug_error("Internal error");
			r = EMV_ERROR_INTERNAL;
			goto error;
		}
	}

	// Populate selected kernel ID to inform caller of which contactless kernel
	// to invoke
	r = emv_ep_extract_selected_kernel_id(
		ctx->selected_app,
		kernel_id
	);
	if (r) {
		emv_debug_trace_msg("emv_ep_extract_selected_kernel_id() failed; r=%d", r);
		emv_debug_error("Failed to extract selected kernel ID");

		// Return error as-is
		goto error;
	}

	// When Visa kernel 3 is selected, ensure that PDOL contains
	// TTQ (field 9F66)
	// See EMV Contactless Book B v2.11, 3.3.3.6
	r = emv_aid_get_info(
		ctx->selected_app->aid->value,
		ctx->selected_app->aid->length,
		&aid_info
	);
	if (r) {
		emv_debug_trace_msg("emv_aid_get_info() failed; r=%d", r);
		emv_debug_error("Invalid selected AID");
		r = EMV_ERROR_INTERNAL;
		goto error;
	}
	if (aid_info.scheme == EMV_CARD_SCHEME_VISA &&
		kernel_id[0] == 0x03
	) {
		const struct emv_tlv_t* pdol;

		pdol = emv_tlv_list_find_const(&ctx->selected_app->tlv_list, EMV_TAG_9F38_PDOL);
		if (!pdol) {
			emv_debug_error("Visa Kernel 3 has no PDOL");
			goto select_next;
		}

		r = emv_dol_find_tag(
			pdol->value,
			pdol->length,
			EMV_TAG_9F66_TTQ,
			NULL
		);
		if (r < 0) {
			emv_debug_trace_msg("emv_dol_find_tag() failed; r=%d", r);
			emv_debug_error("Invalid Processing Options Data Object List (PDOL)");
			goto select_next;
		}
		if (r > 0) {
			emv_debug_error("Visa Kernel 3 PDOL does not contain TTQ (9F66)");
			goto select_next;
		}
	}

	// Success
	r = 0;
	goto exit;

select_next:
	if (emv_app_list_is_empty(app_list)) {
		// If no applications remain, outcome is End Application
		// See EMV Contactless Book B v2.11, 3.3.2.7
		emv_debug_info("Candidate list empty; try another card");
		r = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD;
	} else {
		// Otherwise, outcome is Select Next
		// See EMV Contactless Book B v2.11, 3.3.2.6
		r = EMV_OUTCOME_SELECT_NEXT;
	}

error:
	if (ctx->selected_app) {
		emv_app_free(ctx->selected_app);
		ctx->selected_app = NULL;
	}

exit:
	if (current_app) {
		emv_app_free(current_app);
		current_app = NULL;
	}

	return r;
}
