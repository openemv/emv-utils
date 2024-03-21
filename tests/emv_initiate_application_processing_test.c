/**
 * @file emv_initiate_application_processing_test.c
 * @brief Unit tests for EMV application processing
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
#include "emv_tags.h"
#include "emv_ttl.h"
#include "emv_app.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Reuse source data for all tests
const struct emv_tlv_t test_source1_data[] = {
	{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ 0x09 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x24, 0x02, 0x17 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x09, 0x87, 0x65, 0x43, 0x21 }, 0 }}, NULL },
};
const struct emv_tlv_t test_source2_data[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0x60, 0xF0, 0xC8 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F37_UNPREDICTABLE_NUMBER, 4, (uint8_t[]){ 0xDE, 0xAD, 0xBE, 0xEF }, 0 }}, NULL },
	{ {{ EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x12, 0x34, 0x55, 0x43, 0x21 }, 0 }}, NULL },
};

static const uint8_t test1_fci[] = { 0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x07, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74 };
static const struct xpdu_t test1_apdu_list[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		18, (uint8_t[]){ 0x80, 0x0E, 0x78, 0x00, 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01, 0x90, 0x00 }, // GPO response format 1
	},
	{ 0 }
};
static const uint8_t test1_aip_verify[] = { 0x78, 0x00 };
static const uint8_t test1_afl_verify[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };

static const uint8_t test2_fci[] = { 0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x07, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74 };
static const struct xpdu_t test2_apdu_list[] = {
	{
		8, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 }, // GPO
		18, (uint8_t[]){ 0x77, 0x0E, 0x82, 0x02, 0x39, 0x00, 0x94, 0x08, 0x18, 0x01, 0x02, 0x01, 0x20, 0x02, 0x04, 0x00, 0x90, 0x00 }, // GPO response format 2
	},
	{ 0 }
};
static const uint8_t test2_aip_verify[] = { 0x39, 0x00 };
static const uint8_t test2_afl_verify[] = { 0x18, 0x01, 0x02, 0x01, 0x20, 0x02, 0x04, 0x00 };

static const uint8_t test3_fci[] = { 0x6F, 0x18, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x0D, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74, 0x9F, 0x38, 0x03, 0x9F, 0x33, 0x03 };
static const struct xpdu_t test3_apdu_list[] = {
	{
		11, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x05, 0x83, 0x03, 0x60, 0xF0, 0xC8, 0x00 }, // GPO
		2, (uint8_t[]){ 0x69, 0x85 }, // Conditions of use not satisfied
	},
	{ 0 }
};

static const uint8_t test4_fci[] = { 0x6F, 0x18, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x0D, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74, 0x9F, 0x38, 0x03, 0x9F, 0x33, 0x03 };
static const struct xpdu_t test4_apdu_list[] = {
	{
		11, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x05, 0x83, 0x03, 0x60, 0xF0, 0xC8, 0x00 }, // GPO
		18, (uint8_t[]){ 0x80, 0x0E, 0x78, 0x00, 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01, 0x90, 0x00 }, // GPO response format 1
	},
	{ 0 }
};
static const uint8_t test4_aip_verify[] = { 0x78, 0x00 };
static const uint8_t test4_afl_verify[] = { 0x08, 0x02, 0x02, 0x00, 0x10, 0x01, 0x04, 0x00, 0x18, 0x01, 0x02, 0x01 };

static const uint8_t test5_fci[] = { 0x6F, 0x18, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x0D, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74, 0x9F, 0x38, 0x03, 0x9F, 0x33, 0xFF };
static const struct xpdu_t test5_apdu_list[] = {
	{ 0 }
};

static const uint8_t test6_fci[] = { 0x6F, 0x18, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0xA5, 0x0D, 0x50, 0x05, 0x44, 0x65, 0x62, 0x69, 0x74, 0x9F, 0x38, 0x03, 0x9F, 0x33, 0x03 };
static const struct xpdu_t test6_apdu_list[] = {
	{
		11, (uint8_t[]){ 0x80, 0xA8, 0x00, 0x00, 0x05, 0x83, 0x03, 0x60, 0xF0, 0xC8, 0x00 }, // GPO
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};

static int populate_source(
	const struct emv_tlv_t* tlv_array,
	size_t tlv_array_count,
	struct emv_tlv_list_t* source
)
{
	int r;

	emv_tlv_list_clear(source);
	for (size_t i = 0; i < tlv_array_count; ++i) {
		r = emv_tlv_list_push(source, tlv_array[i].tag, tlv_array[i].length, tlv_array[i].value, 0);
		if (r) {
			return r;
		}
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_tlv_list_t source1 = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t source2 = EMV_TLV_LIST_INIT;
	struct emv_app_t* app = NULL;
	struct emv_tlv_list_t icc = EMV_TLV_LIST_INIT;
	const struct emv_tlv_t* aip;
	const struct emv_tlv_t* afl;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	// Populate data sources
	r = populate_source(test_source1_data, sizeof(test_source1_data) / sizeof(test_source1_data[0]), &source1);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}
	r = populate_source(test_source2_data, sizeof(test_source2_data) / sizeof(test_source2_data[0]), &source2);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	printf("\nTest 1: No PDOL and GPO response format 1...\n");
	app = emv_app_create_from_fci(test1_fci, sizeof(test1_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test1_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r) {
		fprintf(stderr, "emv_initiate_application_processing() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	aip = emv_tlv_list_find_const(&icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!aip) {
		fprintf(stderr, "Failed to find AIP\n");
		r = 1;
		goto exit;
	}
	if (aip->length != sizeof(test1_aip_verify) ||
		memcmp(aip->value, test1_aip_verify, sizeof(test1_aip_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AIP", aip->value, aip->length);
		print_buf("test1_aip_verify", test1_aip_verify, sizeof(test1_aip_verify));
		r = 1;
		goto exit;
	}
	afl = emv_tlv_list_find_const(&icc, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!afl) {
		fprintf(stderr, "Failed to find AFL\n");
		r = 1;
		goto exit;
	}
	if (afl->length != sizeof(test1_afl_verify) ||
		memcmp(afl->value, test1_afl_verify, sizeof(test1_afl_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AFL", afl->value, afl->length);
		print_buf("test1_afl_verify", test1_afl_verify, sizeof(test1_afl_verify));
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	printf("\nTest 2: No PDOL and GPO response format 2...\n");
	app = emv_app_create_from_fci(test2_fci, sizeof(test2_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test2_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r) {
		fprintf(stderr, "emv_initiate_application_processing() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	aip = emv_tlv_list_find_const(&icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!aip) {
		fprintf(stderr, "Failed to find AIP\n");
		r = 1;
		goto exit;
	}
	if (aip->length != sizeof(test2_aip_verify) ||
		memcmp(aip->value, test2_aip_verify, sizeof(test2_aip_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AIP", aip->value, aip->length);
		print_buf("test2_aip_verify", test2_aip_verify, sizeof(test2_aip_verify));
		r = 1;
		goto exit;
	}
	afl = emv_tlv_list_find_const(&icc, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!afl) {
		fprintf(stderr, "Failed to find AFL\n");
		r = 1;
		goto exit;
	}
	if (afl->length != sizeof(test2_afl_verify) ||
		memcmp(afl->value, test2_afl_verify, sizeof(test2_afl_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AFL", afl->value, afl->length);
		print_buf("test2_afl_verify", test2_afl_verify, sizeof(test2_afl_verify));
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	printf("\nTest 3: PDOL present and GPO status 6985...\n");
	app = emv_app_create_from_fci(test3_fci, sizeof(test3_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test3_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r != EMV_OUTCOME_GPO_NOT_ACCEPTED) {
		fprintf(stderr, "emv_initiate_application_processing() did not return EMV_OUTCOME_GPO_NOT_ACCEPTED; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	printf("\nTest 4: PDOL present and GPO response format 1..\n");
	app = emv_app_create_from_fci(test4_fci, sizeof(test4_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test4_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r) {
		fprintf(stderr, "emv_initiate_application_processing() failed; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly empty\n");
		r = 1;
		goto exit;
	}
	aip = emv_tlv_list_find_const(&icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!aip) {
		fprintf(stderr, "Failed to find AIP\n");
		r = 1;
		goto exit;
	}
	if (aip->length != sizeof(test4_aip_verify) ||
		memcmp(aip->value, test4_aip_verify, sizeof(test4_aip_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AIP", aip->value, aip->length);
		print_buf("test4_aip_verify", test4_aip_verify, sizeof(test4_aip_verify));
		r = 1;
		goto exit;
	}
	afl = emv_tlv_list_find_const(&icc, EMV_TAG_94_APPLICATION_FILE_LOCATOR);
	if (!afl) {
		fprintf(stderr, "Failed to find AFL\n");
		r = 1;
		goto exit;
	}
	if (afl->length != sizeof(test4_afl_verify) ||
		memcmp(afl->value, test4_afl_verify, sizeof(test4_afl_verify)) != 0
	) {
		fprintf(stderr, "Incorrect AIP\n");
		print_buf("AFL", afl->value, afl->length);
		print_buf("test4_afl_verify", test4_afl_verify, sizeof(test4_afl_verify));
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	printf("\nTest 5: Invalid PDOL length and no GPO processing...\n");
	app = emv_app_create_from_fci(test5_fci, sizeof(test5_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test5_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "emv_initiate_application_processing() did not return EMV_OUTCOME_CARD_ERROR; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	if (!emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	printf("\nTest 6: PDOL present and GPO status 6A81...\n");
	app = emv_app_create_from_fci(test6_fci, sizeof(test6_fci));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	emul_ctx.xpdu_list = test6_apdu_list;
	emul_ctx.xpdu_current = NULL;
	emv_tlv_list_clear(&icc);
	r = emv_initiate_application_processing(
		&ttl,
		app,
		&source1,
		&source2,
		&icc
	);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "emv_initiate_application_processing() did not return EMV_OUTCOME_CARD_ERROR; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_tlv_list_is_empty(&icc)) {
		fprintf(stderr, "ICC list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	emv_app_free(app);
	app = NULL;
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&source1);
	emv_tlv_list_clear(&source2);
	if (app) {
		emv_app_free(app);
	}
	emv_tlv_list_clear(&icc);

	return r;
}
