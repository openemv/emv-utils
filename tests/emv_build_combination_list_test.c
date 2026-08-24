/**
 * @file emv_build_combination_list_test.c
 * @brief Unit tests for EMV PPSE processing and combination list building
 *
 * Copyright 2024-2026 Leon Lynch
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

static const struct xpdu_t test_ppse_card_blocked[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_not_found[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		2, (uint8_t[]){ 0x6A, 0x82 }, // File or application not found
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_blocked[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		2, (uint8_t[]){ 0x62, 0x83 }, // Selected file deactivated
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x00 }, // GET RESPONSE
		2, (uint8_t[]){ 0x6C, 0x2F }, // 47 bytes available
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x2F }, // GET RESPONSE Le=47
		49, (uint8_t[]){ 0x6F, 0x2D, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x1B, 0xBF, 0x0C, 0x18, 0x61, 0x16, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45, 0x44, 0x49, 0x54, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_app_not_supported[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		43, (uint8_t[]){ 0x6F, 0x27, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x15, 0xBF, 0x0C, 0x12, 0x61, 0x10, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x09, 0x99, 0x99, 0x50, 0x05, 0x4F, 0x54, 0x48, 0x45, 0x52, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_app_supported[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		49, (uint8_t[]){ 0x6F, 0x2D, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x1B, 0xBF, 0x0C, 0x18, 0x61, 0x16, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45, 0x44, 0x49, 0x54, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_multi_app_supported[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		67, (uint8_t[]){
			0x6F, 0x3F, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53,
			0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x2D, 0xBF, 0x0C, 0x2A, 0x61,
			0x13, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20, 0x50, 0x05,
			0x56, 0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x01, 0x61, 0x13, 0x4F, 0x07,
			0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10, 0x50, 0x05, 0x56, 0x20, 0x50,
			0x41, 0x59, 0x87, 0x01, 0x02, 0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_priority_sorting[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		106, (uint8_t[]){
			0x6F, 0x66, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53,
			0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x54, 0xBF, 0x0C, 0x51,
			// Application Priority Indicator 0xF
			0x61, 0x13, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x87, 0x01, 0x0F,
			// Application Priority Indicator 0
			0x61, 0x13, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x00,
			// No Application Priority Indicator
			0x61, 0x10, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x34,
			// Application Priority Indicator 1; highest priority
			0x61, 0x13, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_ppse_confirmation_bit_ignored[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		52, (uint8_t[]){
			0x6F, 0x30, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53,
			0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x1E, 0xBF, 0x0C, 0x1B,
			// Application Priority Indicator has cardholder confirmation bit set
			0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x50,
			0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45, 0x44, 0x49, 0x54,
			0x87, 0x01, 0x81,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT;
	size_t app_count;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	r = emv_ctx_init(&emv, &ttl);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Supported applications
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 }, 7, EMV_ASI_PARTIAL_MATCH, NULL, NULL); // Visa Credit/Debit
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10 }, 7, EMV_ASI_EXACT_MATCH, NULL, NULL); // Visa Electron
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20 }, 7, EMV_ASI_EXACT_MATCH, NULL, NULL); // V Pay
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 }, 7, EMV_ASI_PARTIAL_MATCH, NULL, NULL); // Mastercard
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60 }, 7, EMV_ASI_PARTIAL_MATCH, NULL, NULL); // Maestro
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
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
		return 1;
	}

	printf("\nTesting PPSE card blocked or SELECT not supported...\n");
	emul_ctx.xpdu_list = test_ppse_card_blocked;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r != EMV_OUTCOME_CARD_BLOCKED) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting PPSE not found...\n");
	emul_ctx.xpdu_list = test_ppse_not_found;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting PPSE blocked...\n");
	emul_ctx.xpdu_list = test_ppse_blocked;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting PPSE app not supported...\n");
	emul_ctx.xpdu_list = test_ppse_app_not_supported;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly NOT empty\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting PPSE app supported...\n");
	emul_ctx.xpdu_list = test_ppse_app_supported;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
	}
	printf("Success\n");

	printf("\nTesting PPSE multiple apps supported...\n");
	emul_ctx.xpdu_list = test_ppse_multi_app_supported;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
	}
	printf("Success\n");

	printf("\nTesting PPSE application priority sorting...\n");
	emul_ctx.xpdu_list = test_ppse_priority_sorting;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	app_count = 0;
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
		++app_count;

		// Use application display name to validate sorted app order
		char tmp[] = "APP x";
		tmp[4] = '0' + app_count;
		if (strcmp(tmp, app->display_name) != 0) {
			fprintf(stderr, "Invalid combination list order\n");
			r = 1;
			goto exit;
		}
	}
	printf("Success\n");

	printf("\nTesting PPSE cardholder confirmation required bit ignored...\n");
	emul_ctx.xpdu_list = test_ppse_confirmation_bit_ignored;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	r = emv_build_combination_list(&emv, &app_list);
	if (r) {
		fprintf(stderr, "Unexpected emv_build_combination_list() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Combination list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
	}
	if (app_list.front != app_list.back) {
		fprintf(stderr, "Combination list unexpectedly contains more than one app\n");
		r = 1;
		goto exit;
	}
	if (app_list.front->confirmation_required) {
		fprintf(stderr, "Confirmation bit unexpectedly set\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_ctx_clear(&emv);
	emv_app_list_clear(&app_list);

	return r;
}
