/**
 * @file emv_select_application_test.c
 * @brief Unit tests for EMV application selection
 *
 * Copyright (c) 2024 Leon Lynch
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

#include "emv.h"
#include "emv_cardreader_emul.h"
#include "emv_ttl.h"
#include "emv_tlv.h"
#include "emv_app.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Test data taken from test_sorted_app_priority[] in emv_build_candidate_list_test.c
static const struct xpdu_t test_pse[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 1PAY.SYS.DDF01
		36, (uint8_t[]){ 0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E, 0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x9F, 0x11, 0x01, 0x01, 0x90, 0x00 }, // FCI
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x01, 0x0C, 0x00 }, // READ RECORD 1,1
		72, (uint8_t[]){ 0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x05, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35, 0x87, 0x01, 0x05, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x04, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00 }, // AEF
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x02, 0x0C, 0x00 }, // READ RECORD 1,2
		58, (uint8_t[]){ 0x70, 0x36, 0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x05, 0x10, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x38, 0x73, 0x07, 0x9F, 0x0A, 0x04, 0x00, 0x01, 0x01, 0x04, 0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x37, 0x73, 0x07, 0x9F, 0x0A, 0x04, 0x00, 0x01, 0x01, 0x04, 0x90, 0x00 }, // AEF without application priority indicator, of which one AID is not supported
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x03, 0x0C, 0x00 }, // READ RECORD 1,3
		72, (uint8_t[]){ 0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x01, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x06, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x36, 0x87, 0x01, 0x07, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00 }, // AEF
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x04, 0x0C, 0x00 }, // READ RECORD 1,4
		72, (uint8_t[]){ 0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x02, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x87, 0x01, 0x01, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x04, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x34, 0x87, 0x01, 0x04, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00 }, // AEF
	},
	{
		5, (uint8_t[]){ 0x00, 0xB2, 0x05, 0x0C, 0x00 }, // READ RECORD 1,5
		2, (uint8_t[]){ 0x6A, 0x83 }, // Record not found
	},
	{ 0 }
};

static const struct xpdu_t test_select_app1[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x01, 0x00 }, // SELECT A0000000031001
		2, (uint8_t[]){ 0x6A, 0x82 }, // File or application not found
	},
	{ 0 }
};

static const struct xpdu_t test_select_app7[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x00 }, // SELECT A0000000041010
		2, (uint8_t[]){ 0x62, 0x83 }, // Selected file deactivated
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x00 }, // GET RESPONSE
		2, (uint8_t[]){ 0x6C, 0x61 }, // 97 bytes available
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x61 }, // GET RESPONSE Le=97
		51, (uint8_t[]){ 0x6F, 0x5D, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0xA5, 0x52, 0x50, 0x10, 0x44, 0x45, 0x42, 0x49, 0x54, 0x20, 0x4D, 0x41, 0x53, 0x54, 0x45, 0x52, 0x43, 0x41, 0x52, 0x44, 0x9F, 0x12, 0x10, 0x44, 0x65, 0x62, 0x69, 0x74, 0x20, 0x4D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x43, 0x61, 0x72, 0x64, 0x87, 0x01, 0x01, 0x9F, 0x11, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0xBF, 0x0C, 0x1C, 0x9F, 0x5D, 0x03, 0x01, 0x00, 0x00, 0x9F, 0x0A, 0x04, 0x00, 0x01, 0x01, 0x01, 0x9F, 0x4D, 0x02, 0x0B, 0x0A, 0x9F, 0x6E, 0x07, 0x05, 0x28, 0x00, 0x00, 0x30, 0x30, 0x00, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app4[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x04, 0x00 }, // SELECT A0000000031004
		1, (uint8_t[]){ 0x00 }, // Invalid response
	},
	{ 0 }
};

static const struct xpdu_t test_select_app3[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0x00 }, // SELECT A0000000031003
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};

static const struct xpdu_t test_select_app5[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x05, 0x00 }, // SELECT A0000000031005
		32, (uint8_t[]){ 0x6F, 0x1C, 0x85, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x05, 0xA5, 0x11, 0x50, 0x05, 0x56, 0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x90, 0x00 }, // Invalid FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app2[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x02, 0x00 }, // SELECT A0000000031002
		32, (uint8_t[]){ 0x6F, 0x1C, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x05, 0xA5, 0x11, 0x50, 0x05, 0x56, 0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x90, 0x00 }, // FCI for A0000000031005
	},
	{ 0 }
};

static const struct xpdu_t test_select_app6[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x06, 0x00 }, // SELECT A0000000031006
		32, (uint8_t[]){ 0x6F, 0x1C, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x05, 0xA5, 0x11, 0x50, 0x05, 0x56, 0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x90, 0x00 }, // FCI for A0000000031005
	},
	{ 0 }
};

static const struct xpdu_t test_select_app_success[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0x00 }, // SELECT A0000000031003
		32, (uint8_t[]){ 0x6F, 0x1C, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x11, 0x50, 0x05, 0x56, 0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static bool verify_app_list_numbers(
	const struct emv_app_list_t* app_list,
	const unsigned int* numbers,
	size_t numbers_len
)
{
	struct emv_app_t* app;
	size_t i;

	i = 0;
	for (app = app_list->front; app != NULL; app = app->next) {
		if (i >= numbers_len) {
			return false;
		}

		// Use application display name to validate sorted app order
		char tmp[] = "APP x";
		tmp[4] = '0' + numbers[i];
		if (strcmp(tmp, app->display_name) != 0) {
			return false;
		}

		++i;
	}

	if (i != numbers_len) {
		return false;
	}

	return true;
}

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_tlv_list_t supported_aids = EMV_TLV_LIST_INIT;
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT;
	struct emv_app_t* selected_app = (void*)42;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	// Supported applications
	emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Visa
	emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10 }, EMV_ASI_EXACT_MATCH); // Visa Electron
	emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20 }, EMV_ASI_EXACT_MATCH); // V Pay
	emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Mastercard
	emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30 }, EMV_ASI_PARTIAL_MATCH); // Maestro

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	// Prepare candidate application list to facilitate further testing
	// See emv_build_candidate_list_test.c for thorough candidate application
	// list testing
	printf("\nPrepare candidate application list...\n");
	emul_ctx.xpdu_list = test_pse;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_candidate_list(
		&ttl,
		&supported_aids,
		&app_list
	);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_candidate_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3, 4, 5, 6, 7 }, 7)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	if (!emv_app_list_selection_is_required(&app_list)) {
		fprintf(stderr, "Cardholder application selection unexpectedly NOT required\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest selection of invalid application index...\n");
	r = emv_select_application(&ttl, &app_list, 13, &selected_app);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3, 4, 5, 6, 7 }, 7)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest selection of first application index and application not found...\n");
	emul_ctx.xpdu_list = test_select_app1;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 0, &selected_app);
	if (r != EMV_OUTCOME_TRY_AGAIN) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 3, 4, 5, 6, 7 }, 6)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest selection of last application index and application blocked...\n");
	emul_ctx.xpdu_list = test_select_app7;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 5, &selected_app);
	if (r != EMV_OUTCOME_TRY_AGAIN) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 3, 4, 5, 6 }, 5)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest card error during application selection...\n");
	emul_ctx.xpdu_list = test_select_app4;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 2, &selected_app);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 3, 5, 6 }, 4)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest selection of application with card blocked...\n");
	emul_ctx.xpdu_list = test_select_app3;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 1, &selected_app);
	if (r != EMV_OUTCOME_CARD_BLOCKED) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 5, 6 }, 3)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest invalid FCI during application selection...\n");
	emul_ctx.xpdu_list = test_select_app5;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 1, &selected_app);
	if (r != EMV_OUTCOME_TRY_AGAIN) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 6 }, 2)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest DF Name mismatch during application selection...\n");
	emul_ctx.xpdu_list = test_select_app2;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 0, &selected_app);
	if (r != EMV_OUTCOME_TRY_AGAIN) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 6 }, 1)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest DF Name mismatch during application selection and expecting empty candidate application list...\n");
	emul_ctx.xpdu_list = test_select_app6;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 0, &selected_app);
	if (r != EMV_OUTCOME_NOT_ACCEPTED) {
		fprintf(stderr, "Unexpected emv_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (selected_app != NULL) {
		fprintf(stderr, "emv_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	// Silence debugging logs for rebuilding candidate application list
	r = emv_debug_init(
		EMV_DEBUG_SOURCE_NONE,
		EMV_DEBUG_NONE,
		NULL
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	// Prepare candidate application list to facilitate further testing
	// See emv_build_candidate_list_test.c for thorough candidate application
	// list testing
	printf("\nPrepare candidate application list...\n");
	emul_ctx.xpdu_list = test_pse;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_candidate_list(
		&ttl,
		&supported_aids,
		&app_list
	);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_candidate_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3, 4, 5, 6, 7 }, 7)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	if (!emv_app_list_selection_is_required(&app_list)) {
		fprintf(stderr, "Cardholder application selection unexpectedly NOT required\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	// Reset debuggin logs
	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	printf("\nTest successful application selection...\n");
	emul_ctx.xpdu_list = test_select_app_success;
	emul_ctx.xpdu_current = NULL;
	r = emv_select_application(&ttl, &app_list, 2, &selected_app);
	if (r) {
		fprintf(stderr, "emv_select_application() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!selected_app) {
		fprintf(stderr, "emv_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	r = emv_app_free(selected_app);
	if (r) {
		fprintf(stderr, "emv_app_free() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 4, 5, 6, 7 }, 6)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&supported_aids);
	emv_app_list_clear(&app_list);

	return r;
}
