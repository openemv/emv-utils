/**
 * @file emv_ep_preprocess_test.c
 * @brief Unit tests for EMV Entry Point pre-processing
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_ep.h"
#include "emv.h"
#include "emv_config.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Reuse source data for all tests
static const uint8_t test_aid_visa[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
static const uint8_t test_aid_mastercard[] = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 };
static const uint8_t test_aid_maestro[] = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60 };
static const uint8_t test_kernel_id_c2[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t test_kernel_id_c3[] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

struct test_app_t {
	const uint8_t* aid;
	unsigned int aid_len;
	uint8_t asi;
	bool disabled;

	const struct emv_tlv_t* data;

	bool contactless_transaction_limit_enabled;
	unsigned int contactless_transaction_limit;
};

struct test_combination_t {
	const uint8_t* aid;
	unsigned int aid_len;
	const uint8_t* kernel_id;
};

struct test_t {
	const char* name;

	const struct test_app_t* apps;

	uint32_t amount;

	int result;
	const struct test_combination_t* combinations;
};

static const struct test_t test[] = {
	{
		.name = "No supported applications",

		.apps = NULL,

		.amount = 3000,

		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.combinations = NULL,
	},

	{
		.name = "Application without Kernel Identifier - Terminal (96)",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
			},
			{ 0 }
		},

		.amount = 3000,

		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.combinations = NULL,
	},

	{
		.name = "Application with invalid Kernel Identifier - Terminal (96) length",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 3, (uint8_t[]){ 0x03, 0x00, 0x00 }, 0 }}, NULL },
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.combinations = NULL,
	},

	{
		.name = "Disabled application",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.disabled = true,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.combinations = NULL,
	},

	{
		.name = "Contact and contactless applications",

		.apps = (const struct test_app_t[]){
			{
				// Omit kernel identifier to disable contactless support
				.aid = test_aid_maestro,
				.aid_len = sizeof(test_aid_maestro),
				.asi = EMV_ASI_PARTIAL_MATCH,
			},
			{
				.aid = test_aid_mastercard,
				.aid_len = sizeof(test_aid_mastercard),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-2
					{ {{ 0 }} },
				},
			},
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_mastercard, sizeof(test_aid_mastercard), test_kernel_id_c2 },
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Contactless Transaction Limit not reached",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 3000,
			},
			{ 0 }
		},

		.amount = 2999,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Contactless Transaction Limit reached",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 3000,
			},
			{ 0 }
		},

		// Contactless Transaction Limit is exceeded when the amount is equal
		// to the limit
		.amount = 3000,

		.result = EMV_OUTCOME_TRY_ANOTHER_INTERFACE,
		.combinations = NULL,
	},

	{
		.name = "Contactless Transaction Limit exceeded",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 3000,
			},
			{ 0 }
		},

		.amount = 3001,

		.result = EMV_OUTCOME_TRY_ANOTHER_INTERFACE,
		.combinations = NULL,
	},

	{
		.name = "Contactless Transaction Limit of zero",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				// Zero is a valid limit and blocks every amount
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 0,
			},
			{ 0 }
		},

		.amount = 0,

		.result = EMV_OUTCOME_TRY_ANOTHER_INTERFACE,
		.combinations = NULL,
	},

	{
		.name = "Contactless Transaction Limit disabled",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				// Limit value is ignored while disabled
				.contactless_transaction_limit_enabled = false,
				.contactless_transaction_limit = 3000,
			},
			{ 0 }
		},

		.amount = 1000000,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Contactless Transaction Limit exceeded for one of two applications",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_mastercard,
				.aid_len = sizeof(test_aid_mastercard),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-2
					{ {{ 0 }} },
				},
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 3000,
			},
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Invalid Terminal Transaction Qualifiers (9F66) length",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ EMV_TAG_9F66_TTQ, 3, (uint8_t[]){ 0x37, 0x00, 0x40 }, 0 }}, NULL },
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		// Invalid configuration does not set the Contactless Application Not
		// Allowed indicator and therefore the outcome is End Application
		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.combinations = NULL,
	},

	{
		.name = "Zero amount for offline-only reader",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x3F, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Offline-only reader
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 0,

		.result = EMV_OUTCOME_TRY_ANOTHER_INTERFACE,
		.combinations = NULL,
	},

	{
		.name = "Zero amount for online-capable reader",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 0,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Non-zero amount for offline-only reader",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x3F, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Offline-only reader
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 3000,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_visa, sizeof(test_aid_visa), test_kernel_id_c3 },
			{ 0 }
		},
	},

	{
		.name = "Zero amount for application without Terminal Transaction Qualifiers (9F66)",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_mastercard,
				.aid_len = sizeof(test_aid_mastercard),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-2
					{ {{ 0 }} },
				},
			},
			{ 0 }
		},

		.amount = 0,

		.result = 0,
		.combinations = (const struct test_combination_t[]){
			{ test_aid_mastercard, sizeof(test_aid_mastercard), test_kernel_id_c2 },
			{ 0 }
		},
	},

	{
		.name = "Contactless Application Not Allowed together with invalid configuration",

		.apps = (const struct test_app_t[]){
			{
				.aid = test_aid_mastercard,
				.aid_len = sizeof(test_aid_mastercard),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-2
					{ {{ EMV_TAG_9F66_TTQ, 3, (uint8_t[]){ 0x37, 0x00, 0x40 }, 0 }}, NULL },
					{ {{ 0 }} },
				},
			},
			{
				.aid = test_aid_visa,
				.aid_len = sizeof(test_aid_visa),
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data = (const struct emv_tlv_t[]){
					{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
					{ {{ 0 }} },
				},
				.contactless_transaction_limit_enabled = true,
				.contactless_transaction_limit = 3000,
			},
			{ 0 }
		},

		.amount = 3000,

		// Contactless Application Not Allowed indicator takes precedence over
		// the invalid configuration of the other application
		.result = EMV_OUTCOME_TRY_ANOTHER_INTERFACE,
		.combinations = NULL,
	},
};

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	while (tlv_array && tlv_array->tag) {
		r = emv_tlv_list_push(list, tlv_array->tag, tlv_array->length, tlv_array->value, 0);
		if (r) {
			return r;
		}

		++tlv_array;
	}

	return 0;
}

static int create_config_apps(
	struct emv_ctx_t* ctx,
	const struct test_app_t* app_array
)
{
	int r;
	struct emv_tlv_list_t data = EMV_TLV_LIST_INIT;

	while (app_array && app_array->aid) {
		struct emv_config_app_t* config_app;

		r = populate_tlv_list(app_array->data, &data);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			goto error;
		}

		r = emv_config_app_create(
			ctx,
			app_array->aid,
			app_array->aid_len,
			app_array->asi,
			&data,
			&config_app
		);
		if (r) {
			fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
			goto error;
		}

		if (app_array->disabled) {
			r = emv_config_app_set_enable(config_app, false);
			if (r) {
				fprintf(stderr, "emv_config_app_set_enable() failed; r=%d\n", r);
				goto error;
			}
		}

		config_app->contactless_transaction_limit_enabled =
			app_array->contactless_transaction_limit_enabled;
		config_app->contactless_transaction_limit =
			app_array->contactless_transaction_limit;

		++app_array;
	}

	return 0;

error:
	emv_tlv_list_clear(&data);

	return r;
}

static int verify_ep_app_list(
	const struct emv_ep_app_list_t* list,
	const struct test_combination_t* combination_array
)
{
	const struct emv_ep_app_t* app;

	app = list->front;
	while (combination_array && combination_array->aid) {
		if (!app) {
			fprintf(stderr, "Combination list unexpectedly short\n");
			return 1;
		}
		if (!app->config) {
			fprintf(stderr, "Combination unexpectedly has no config\n");
			return 1;
		}

		if (app->config->aid_len != combination_array->aid_len ||
			memcmp(app->config->aid, combination_array->aid, combination_array->aid_len) != 0
		) {
			fprintf(stderr, "Incorrect combination AID\n");
			print_buf("AID", app->config->aid, app->config->aid_len);
			print_buf("Expected", combination_array->aid, combination_array->aid_len);
			return 1;
		}

		if (memcmp(app->kernel_id, combination_array->kernel_id, sizeof(app->kernel_id)) != 0) {
			fprintf(stderr, "Incorrect combination Kernel Identifier - Terminal (96)\n");
			print_buf("Kernel Identifier - Terminal", app->kernel_id, sizeof(app->kernel_id));
			print_buf("Expected", combination_array->kernel_id, sizeof(app->kernel_id));
			return 1;
		}

		app = app->next;
		++combination_array;
	}
	if (app) {
		fprintf(stderr, "Combination list unexpectedly long\n");
		return 1;
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_ctx_t emv;
	struct emv_ep_app_list_t ep_list = EMV_EP_APP_LIST_INIT;

	// Entry Point pre-processing does not require a Terminal Transport Layer
	// (TTL) context because it does not interact with the card
	r = emv_ctx_init(&emv, NULL);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		r = 1;
		goto exit;
	}

	printf("Testing combination list helper functions...\n");
	if (!emv_ep_app_list_is_empty(&ep_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	if (!emv_ep_app_list_is_empty(NULL)) {
		fprintf(stderr, "Invalid combination list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	r = emv_ep_app_list_push(NULL, NULL, test_kernel_id_c3);
	if (r != -1) {
		fprintf(stderr, "Unexpected emv_ep_app_list_push() result; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_ep_app_list_push(&ep_list, NULL, NULL);
	if (r != -2) {
		fprintf(stderr, "Unexpected emv_ep_app_list_push() result; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_ep_app_list_push(&ep_list, NULL, test_kernel_id_c2);
	if (r) {
		fprintf(stderr, "emv_ep_app_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_ep_app_list_push(&ep_list, NULL, test_kernel_id_c3);
	if (r) {
		fprintf(stderr, "emv_ep_app_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (emv_ep_app_list_is_empty(&ep_list)) {
		fprintf(stderr, "Combination list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	emv_ep_app_list_clear(&ep_list);
	if (!emv_ep_app_list_is_empty(&ep_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}

	// Clearing an empty combination list must be safe
	emv_ep_app_list_clear(&ep_list);
	printf("Passed!\n\n");

	printf("Testing invalid parameters...\n");
	r = emv_ep_preprocess(NULL, 3000, &ep_list);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_preprocess() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	r = emv_ep_preprocess(&emv.config, 3000, NULL);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_preprocess() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	r = emv_ep_app_list_push(&ep_list, NULL, test_kernel_id_c3);
	if (r) {
		fprintf(stderr, "emv_ep_app_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_ep_preprocess(&emv.config, 3000, &ep_list);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_preprocess() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	emv_ep_app_list_clear(&ep_list);
	printf("Passed!\n\n");

	r = emv_ctx_clear(&emv);
	if (r) {
		fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		printf("Test %zu (%s)...\n", i + 1, test[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, NULL);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = create_config_apps(&emv, test[i].apps);
		if (r) {
			r = 1;
			goto exit;
		}

		// Test Entry Point pre-processing...
		r = emv_ep_preprocess(&emv.config, test[i].amount, &ep_list);
		if (r != test[i].result) {
			fprintf(stderr, "Unexpected emv_ep_preprocess() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}
		r = verify_ep_app_list(&ep_list, test[i].combinations);
		if (r) {
			fprintf(stderr, "Invalid combination list\n");
			r = 1;
			goto exit;
		}

		emv_ep_app_list_clear(&ep_list);
		r = emv_ctx_clear(&emv);
		if (r) {
			fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		printf("Passed!\n\n");
	}

	// Success
	printf("Success!\n");
	r = 0;
	goto exit;

exit:
	emv_ctx_clear(&emv);
	emv_ep_app_list_clear(&ep_list);

	return r;
}
