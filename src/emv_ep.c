/**
 * @file emv_ep.c
 * @brief EMV contactless Entry Point helper functions
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
#include "emv_app.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h> // For malloc() and free()
#include <string.h>
#include <assert.h>

static struct emv_ep_app_t* emv_ep_app_alloc(
	const uint8_t* aid,
	unsigned int aid_len,
	const uint8_t* kernel_id,
	const struct emv_config_app_t* config_app
)
{
	struct emv_ep_app_t* app;

	app = malloc(sizeof(*app));
	if (!app) {
		return NULL;
	}
	memset(app, 0, sizeof(*app));

	memcpy(app->aid, aid, aid_len);
	app->aid_len = aid_len;
	memcpy(app->kernel_id, kernel_id, sizeof(app->kernel_id));
	app->config = config_app;

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
	const uint8_t* aid,
	unsigned int aid_len,
	const uint8_t* kernel_id,
	const struct emv_config_app_t* config_app
)
{
	struct emv_ep_app_t* app;

	if (!emv_ep_app_list_is_valid(list)) {
		return -1;
	}

	if (!aid || aid_len < 5 || aid_len > 16) {
		return -2;
	}
	if (!kernel_id) {
		return -3;
	}

	app = emv_ep_app_alloc(aid, aid_len, kernel_id, config_app);
	if (!app) {
		return -4;
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

	if (!emv_ep_app_list_is_empty(list)) {
		// Internal error; terminate session
		emv_debug_error("Internal error");
		return EMV_ERROR_INTERNAL;
	}

	r = emv_config_app_itr_init(config, &itr);
	if (r) {
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
		r = emv_ep_app_list_push(
			list,
			config_app->aid,
			config_app->aid_len,
			kernel_id_config->value,
			config_app
		);
		if (r) {
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

const struct emv_config_app_t* emv_ep_find_supported_combination(
	const struct emv_ep_app_list_t* list,
	const struct emv_app_t* app
)
{
	int r;
	const struct emv_tlv_t* kernel_id_icc;
	uint8_t requested_kernel_id[3];
	const struct emv_ep_app_t* combination;

	if (!app || !app->aid) {
		// Invalid app; not supported
		return NULL;
	}

	// Extract Requested Kernel ID
	// See EMV Contactless Book B v2.11, 3.3.2.5, step 2C
	memset(requested_kernel_id, 0, sizeof(requested_kernel_id));
	kernel_id_icc = emv_tlv_list_find_const(
		&app->tlv_list,
		EMV_TAG_9F2A_KERNEL_IDENTIFIER
	);
	if (kernel_id_icc &&
		kernel_id_icc->length > 0 &&
		kernel_id_icc->value[0] != 0
	) {
		if ((kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_INTERNATIONAL ||
			(kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_RFU
		) {
			requested_kernel_id[0] = kernel_id_icc->value[0];
		}

		if ((kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_DOMESTIC_EMVCO ||
			(kernel_id_icc->value[0] & EMV_KERNEL_ID_TYPE_MASK) == EMV_KERNEL_ID_TYPE_DOMESTIC_PROPRIETARY
		) {
			if (kernel_id_icc->length < 3) {
				emv_debug_error("Invalid PPSE directory entry domestic kernel ID");
				// Skip application
				return NULL;
			}
			if ((kernel_id_icc->value[0] & EMV_KERNEL_ID_SHORT_MASK) == 0) {
				emv_debug_error("Proprietary PPSE directory entry domestic kernel ID");
				// Skip application
				return NULL;
			}

			memcpy(requested_kernel_id, kernel_id_icc->value, 3);
		}
	} else {
		struct emv_aid_info_t aid_info;
		r = emv_aid_get_info(app->aid->value, app->aid->length, &aid_info);
		if (r) {
			emv_debug_trace_msg("emv_aid_get_info() failed; r=%d", r);
			emv_debug_error("Invalid PPSE directory entry AID");

			// Skip application
			return NULL;
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

	// Find matching contactless application combination
	// See EMV Contactless Book B v2.11, 3.3.2.5
	for (
		combination = list->front;
		combination != NULL;
		combination = combination->next
	) {
		const struct emv_config_app_t* config_app;

		if (!combination->config) {
			// Skip invalid contactless application combination
			continue;
		}
		config_app = combination->config;

		if ((config_app->asi & EMV_ASI_DISABLED) != 0) {
			// Skip disabled EMV application configuration
			continue;
		}

		// See EMV Contactless Book B v2.11, 3.3.2.5, step 2B
		// See EMV 4.4 Book 1, 12.3.1
		if (config_app->asi == EMV_ASI_EXACT_MATCH) {
			if (config_app->aid_len != app->aid->length ||
				memcmp(config_app->aid, app->aid->value, config_app->aid_len) != 0
			) {
				// Exact match failed; skip combination
				continue;
			}
		} else if (config_app->asi == EMV_ASI_PARTIAL_MATCH) {
			if (config_app->aid_len > app->aid->length ||
				memcmp(config_app->aid, app->aid->value, config_app->aid_len) != 0
			) {
				// Partial match failed; skip combination
				continue;
			}
		}

		// See EMV Contactless Book B v2.11, 3.3.2.5, step 2D
		if (requested_kernel_id[0] == 0 ||
			memcmp(requested_kernel_id, combination->kernel_id, 3) != 0
		) {
			return combination->config;
		}
	}

	return NULL;
}
