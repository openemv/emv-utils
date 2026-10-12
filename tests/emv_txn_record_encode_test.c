/**
 * @file emv_txn_record_encode_test.c
 * @brief Unit tests for EMV TLV sources and EMV transaction record encoding
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

#include "emv.h"
#include "emv_app.h"
#include "emv_config.h"
#include "emv_fields.h"
#include "emv_tags.h"
#include "emv_tlv.h"

#include "iso8825_ber.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

static const struct emv_tlv_t test_terminal_fields[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
};

static const struct emv_tlv_t test_icc_fields[] = {
	{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F }, 0 }}, NULL },
};

static const struct emv_tlv_t test_params_fields[] = {
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x26, 0x09, 0x21 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
};

static const uint8_t test_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 };
static const struct emv_tlv_t test_app_config_fields[] = {
	{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0xE0, 0xF8, 0xC8 }, 0 }}, NULL },
};

static const struct emv_tlv_t test_config_fields[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x08, 0x40 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x08, 0x26 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0x00, 0x00, 0x00 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){ 0x22 }, 0 }}, NULL },
};

// Expected EMV data of all five sources:
// - 9F1A overridden by terminal data
// - 5F2A overridden by transaction parameters
// - 9F33 overridden by application dependent data
static const uint8_t test_encoded[] = {
	0x9F, 0x1A, 0x02, 0x05, 0x28,
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
	0x9A, 0x03, 0x26, 0x09, 0x21,
	0x5F, 0x2A, 0x02, 0x09, 0x78,
	0x9F, 0x33, 0x03, 0xE0, 0xF8, 0xC8,
	0x9F, 0x35, 0x01, 0x22,
};

// Expected EMV data with template:
// - Wrapped by 70
// - 9F1A overridden by terminal data
// - 5F2A overridden by transaction parameters
// - 9F33 overridden by application dependent data
static const uint8_t test_encoded_with_template[] = {
	0x70, 0x23,
	0x9F, 0x1A, 0x02, 0x05, 0x28,
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
	0x9A, 0x03, 0x26, 0x09, 0x21,
	0x5F, 0x2A, 0x02, 0x09, 0x78,
	0x9F, 0x33, 0x03, 0xE0, 0xF8, 0xC8,
	0x9F, 0x35, 0x01, 0x22,
};

// Expected EMV data without application dependent data:
// - Wrapped by 70
// - 9F33 is provided by application independent data
static const uint8_t test_encoded_no_app_config[] = {
	0x70, 0x23,
	0x9F, 0x1A, 0x02, 0x05, 0x28,
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
	0x9A, 0x03, 0x26, 0x09, 0x21,
	0x5F, 0x2A, 0x02, 0x09, 0x78,
	0x9F, 0x33, 0x03, 0x00, 0x00, 0x00,
	0x9F, 0x35, 0x01, 0x22,
};

// Empty template field
static const uint8_t test_encoded_empty_template[] = { 0x70, 0x00 };

// Duplicate EMV tag within a single source list
static const struct emv_tlv_t test_duplicate_fields[] = {
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x26, 0x09, 0x21 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x26, 0x10, 0x11 }, 0 }}, NULL },
};
static const uint8_t test_duplicate_encoded[] = { 0x9A, 0x03, 0x26, 0x09, 0x21 };

// ASN.1 object subfields
static const uint8_t test_asn1_abc[] = { 0x0C, 0x03, 0x41, 0x42, 0x43 };
static const uint8_t test_asn1_xy[] = { 0x0C, 0x02, 0x58, 0x59 };
static const uint8_t test_asn1_nl[] = { 0x0C, 0x02, 0x4E, 0x4C };

static const uint8_t test_asn1_encoded_objects[] = {
	0x30, 0x0A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x03, 0x41, 0x42, 0x43,
	0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x0C, 0x02, 0x4E, 0x4C,
};

static const uint8_t test_asn1_encoded_object_with_override[] = {
	0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x02, 0x58, 0x59,
	0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x0C, 0x02, 0x4E, 0x4C,
};

// Minimal File Control Information (FCI) template containing only a DF Name
static const uint8_t test_fci[] = {
	0x6F, 0x09, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10,
};

static int populate_list(
	const struct emv_tlv_t* tlv_array,
	size_t tlv_array_count,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	for (size_t i = 0; i < tlv_array_count; ++i) {
		r = emv_tlv_list_push(list, tlv_array[i].tag, tlv_array[i].length, tlv_array[i].value, 0);
		if (r) {
			return r;
		}
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_tlv_list_t empty = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t terminal = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t icc = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t params = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t app_config = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t config = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t duplicate = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t asn1_distinct = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t asn1_higher_priority = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t asn1_lower_priority = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t parsed_list = EMV_TLV_LIST_INIT;

	// Source lists in the same order as emv_tlv_sources_init_from_ctx()
	const struct emv_tlv_sources_t sources = { 5, { &terminal, &icc, &params, &app_config, &config } };
	// Source lists for tests that do not require all five sources
	const struct emv_tlv_sources_t empty_sources = { 1, { &empty } };
	const struct emv_tlv_sources_t duplicate_sources = { 1, { &duplicate } };
	const struct emv_tlv_sources_t asn1_distinct_sources = { 1, { &asn1_distinct } };
	const struct emv_tlv_sources_t asn1_override_sources = { 2, { &asn1_higher_priority, &asn1_lower_priority } };
	const struct emv_tlv_sources_t no_sources = { 0, { NULL } };

	struct emv_config_app_t* config_app;
	struct emv_ctx_t ctx;
	bool ctx_init = false;
	uint8_t buf[1024];
	size_t buf_len;

	printf("\nTest 1: Compute and encode empty source list without template\n");
	r = emv_tlv_sources_compute_encoded_length(&empty_sources, 0);
	if (r != 0) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&empty_sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != 0) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 2: Compute and encode empty source list with template\n");
	r = emv_tlv_sources_compute_encoded_length(&empty_sources, EMV_TAG_70_DATA_TEMPLATE);
	if (r != (int)sizeof(test_encoded_empty_template)) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&empty_sources, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded_empty_template) ||
		memcmp(buf, test_encoded_empty_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded_empty_template, sizeof(test_encoded_empty_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 3: Compute and encode sources without template\n");
	r = populate_list(test_terminal_fields, sizeof(test_terminal_fields) / sizeof(test_terminal_fields[0]), &terminal);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_icc_fields, sizeof(test_icc_fields) / sizeof(test_icc_fields[0]), &icc);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_params_fields, sizeof(test_params_fields) / sizeof(test_params_fields[0]), &params);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_app_config_fields, sizeof(test_app_config_fields) / sizeof(test_app_config_fields[0]), &app_config);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_config_fields, sizeof(test_config_fields) / sizeof(test_config_fields[0]), &config);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_tlv_sources_compute_encoded_length(&sources, 0);
	if (r != (int)sizeof(test_encoded)) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded) ||
		memcmp(buf, test_encoded, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded, sizeof(test_encoded));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 4: Compute and encode sources with template\n");
	r = emv_tlv_sources_compute_encoded_length(&sources, EMV_TAG_70_DATA_TEMPLATE);
	if (r != (int)sizeof(test_encoded_with_template)) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&sources, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded_with_template) ||
		memcmp(buf, test_encoded_with_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded_with_template, sizeof(test_encoded_with_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 5: Encode sources with an output buffer that is exactly the right size\n");
	buf_len = sizeof(test_encoded);
	r = emv_tlv_sources_encode(&sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded) ||
		memcmp(buf, test_encoded, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded, sizeof(test_encoded));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 6: Encode sources with an output buffer that is too small\n");
	buf_len = sizeof(test_encoded) - 1; // One byte less than what is needed
	r = emv_tlv_sources_encode(&sources, 0, buf, &buf_len);
	if (r != 1) {
		fprintf(stderr, "Unexpected emv_tlv_sources_encode() return value; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 7: Encode sources with a template tag that is not constructed\n");
	r = emv_tlv_sources_compute_encoded_length(&sources, EMV_TAG_50_APPLICATION_LABEL);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&sources, EMV_TAG_50_APPLICATION_LABEL, buf, &buf_len);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_sources_encode() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 8: Encode sources without any source lists\n");
	r = emv_tlv_sources_compute_encoded_length(&no_sources, 0);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&no_sources, 0, buf, &buf_len);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_sources_encode() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 9: Encode sources containing a duplicate EMV tag in a single list\n");
	r = populate_list(test_duplicate_fields, sizeof(test_duplicate_fields) / sizeof(test_duplicate_fields[0]), &duplicate);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&duplicate_sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_duplicate_encoded) ||
		memcmp(buf, test_duplicate_encoded, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_duplicate_encoded, sizeof(test_duplicate_encoded));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 10: Encode sources containing multiple distinct ASN.1 objects\n");
	{
		struct iso8825_oid_t oid = ASN1_OID(commonName);

		r = emv_tlv_list_push_asn1_object(&asn1_distinct, &oid, sizeof(test_asn1_abc), test_asn1_abc);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
	}
	{
		struct iso8825_oid_t oid = ASN1_OID(countryName);

		r = emv_tlv_list_push_asn1_object(&asn1_distinct, &oid, sizeof(test_asn1_nl), test_asn1_nl);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
	}

	// Distinct ASN.1 objects are not duplicates of one another
	if (emv_tlv_list_has_duplicate(&asn1_distinct)) {
		fprintf(stderr, "emv_tlv_list_has_duplicate() unexpectedly reported a duplicate\n");
		print_emv_tlv_list(&asn1_distinct);
		r = 1;
		goto exit;
	}

	r = emv_tlv_sources_compute_encoded_length(&asn1_distinct_sources, 0);
	if (r != (int)sizeof(test_asn1_encoded_objects)) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&asn1_distinct_sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_asn1_encoded_objects) ||
		memcmp(buf, test_asn1_encoded_objects, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_asn1_encoded_objects, sizeof(test_asn1_encoded_objects));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 11: Encode sources containing an overridden ASN.1 object\n");
	{
		// Higher priority source provides commonName
		struct iso8825_oid_t oid = ASN1_OID(commonName);

		r = emv_tlv_list_push_asn1_object(&asn1_higher_priority, &oid, sizeof(test_asn1_xy), test_asn1_xy);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
	}
	{
		// Lower priority source provides the same commonName
		struct iso8825_oid_t oid = ASN1_OID(commonName);

		r = emv_tlv_list_push_asn1_object(&asn1_lower_priority, &oid, sizeof(test_asn1_abc), test_asn1_abc);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
	}
	{
		// Lower priority source also provides a countryName that is not
		// overridden
		struct iso8825_oid_t oid = ASN1_OID(countryName);

		r = emv_tlv_list_push_asn1_object(&asn1_lower_priority, &oid, sizeof(test_asn1_nl), test_asn1_nl);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
	}

	r = emv_tlv_sources_compute_encoded_length(&asn1_override_sources, 0);
	if (r != (int)sizeof(test_asn1_encoded_object_with_override)) {
		fprintf(stderr, "emv_tlv_sources_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_sources_encode(&asn1_override_sources, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_asn1_encoded_object_with_override) ||
		memcmp(buf, test_asn1_encoded_object_with_override, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_sources_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_asn1_encoded_object_with_override, sizeof(test_asn1_encoded_object_with_override));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 12: Compute and encode EMV transaction record\n");
	r = emv_ctx_init(&ctx, NULL);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	ctx_init = true;

	// Application independent configuration
	r = populate_list(test_config_fields, sizeof(test_config_fields) / sizeof(test_config_fields[0]), &config);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_data_set(&ctx, &config);
	if (r) {
		fprintf(stderr, "emv_config_data_set() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Application dependent configuration
	r = populate_list(test_app_config_fields, sizeof(test_app_config_fields) / sizeof(test_app_config_fields[0]), &app_config);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(
		&ctx,
		test_aid,
		sizeof(test_aid),
		EMV_ASI_EXACT_MATCH,
		&app_config,
		&config_app
	);
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Transaction parameters, terminal data and ICC data
	r = populate_list(test_params_fields, sizeof(test_params_fields) / sizeof(test_params_fields[0]), &ctx.params);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_terminal_fields, sizeof(test_terminal_fields) / sizeof(test_terminal_fields[0]), &ctx.terminal);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = populate_list(test_icc_fields, sizeof(test_icc_fields) / sizeof(test_icc_fields[0]), &ctx.icc);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Selected application, as prepared by emv_select_application()
	ctx.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!ctx.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed\n");
		r = 1;
		goto exit;
	}
	ctx.selected_app->config = config_app;

	// The transaction record is framed by a template
	r = emv_txn_record_compute_encoded_length(&ctx);
	if (r != (int)sizeof(test_encoded_with_template)) {
		fprintf(stderr, "emv_txn_record_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_txn_record_encode(&ctx, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_txn_record_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded_with_template) ||
		memcmp(buf, test_encoded_with_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_txn_record_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded_with_template, sizeof(test_encoded_with_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 13: Parse encoded EMV transaction record\n");
	r = emv_tlv_parse(buf, buf_len, &parsed_list);
	if (r) {
		fprintf(stderr, "emv_tlv_parse() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_has_duplicate(&parsed_list)) {
		fprintf(stderr, "Parsed list unexpectedly contains a duplicate EMV field\n");
		print_emv_tlv_list(&parsed_list);
		r = 1;
		goto exit;
	}
	{
		// emv_tlv_parse() omits the template field, so re-applying it must
		// reproduce the encoded transaction record
		uint8_t reencoded[sizeof(test_encoded_with_template)];
		size_t reencoded_len = sizeof(reencoded);

		r = emv_tlv_list_encode(&parsed_list, EMV_TAG_70_DATA_TEMPLATE, reencoded, &reencoded_len);
		if (r) {
			fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		if (reencoded_len != buf_len ||
			memcmp(reencoded, buf, buf_len) != 0
		) {
			fprintf(stderr, "Re-encoded parsed list does not match encoded EMV data\n");
			print_buf("reencoded", reencoded, reencoded_len);
			print_buf("expected", buf, buf_len);
			r = 1;
			goto exit;
		}
	}
	printf("Success\n");

	printf("\nTest 14: Compute and encode EMV transaction record without selected application\n");
	emv_app_free(ctx.selected_app);
	ctx.selected_app = NULL;
	r = emv_txn_record_compute_encoded_length(&ctx);
	if (r != (int)sizeof(test_encoded_no_app_config)) {
		fprintf(stderr, "emv_txn_record_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_txn_record_encode(&ctx, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_txn_record_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test_encoded_no_app_config) ||
		memcmp(buf, test_encoded_no_app_config, buf_len) != 0
	) {
		fprintf(stderr, "emv_txn_record_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_encoded_no_app_config, sizeof(test_encoded_no_app_config));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&empty);
	emv_tlv_list_clear(&terminal);
	emv_tlv_list_clear(&icc);
	emv_tlv_list_clear(&params);
	emv_tlv_list_clear(&app_config);
	emv_tlv_list_clear(&config);
	emv_tlv_list_clear(&duplicate);
	emv_tlv_list_clear(&asn1_distinct);
	emv_tlv_list_clear(&asn1_higher_priority);
	emv_tlv_list_clear(&asn1_lower_priority);
	emv_tlv_list_clear(&parsed_list);
	if (ctx_init) {
		emv_ctx_clear(&ctx);
	}

	return r;
}
