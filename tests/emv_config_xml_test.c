/**
 * @file emv_config_xml_test.c
 * @brief Unit tests for EMV XML configuration loading
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

#include "emv_config_xml.h"
#include "emv.h"
#include "emv_config.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "print_helpers.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct verify_data_t {
	unsigned int tag;
	size_t length;
	const uint8_t* value;
};

struct verify_app_t {
	const uint8_t* aid;
	unsigned int aid_len;
	uint8_t asi;
	unsigned int random_selection_percentage;
	unsigned int random_selection_max_percentage;
	unsigned int random_selection_threshold;
	size_t data_count;
	const struct verify_data_t* data;
};

struct test_t {
	const char* name;
	const char* xml;
	int expected_result;
	size_t data_count;
	const struct verify_data_t* data;
	size_t app_count;
	const struct verify_app_t* app;
};

static const struct test_t test[] = {
	{
		.name = "No header",
		.xml =
			"<emv/>\n",
		.expected_result = 0,
	},

	{
		.name = "Malformed XML",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv><data\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "Invalid root element",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<config/>\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "No <data>",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv/>\n",
		.expected_result = 0,
	},

	{
		.name = "Empty <data>",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data/>\n"
			"</emv>\n",
		.expected_result = 0,
	},

	{
		.name = "Valid <tlv> values",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'>0528</tlv>\n"
			"    <tlv id='9F16'>  \"0987654321     \"  </tlv>\n"
			"    <tlv id='9F15'>59 99   </tlv>\n"
			"    <tlv id='9F1B'>   00002710    </tlv>\n"
			"    <tlv id='9F1C'>\"TID12345\"</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = 0,
		.data_count = 5,
		.data = (const struct verify_data_t[]){
			{
				EMV_TAG_9F15_MCC,
				2,
				(const uint8_t[]){ 0x59, 0x99 },
			},
			{
				EMV_TAG_9F16_MERCHANT_IDENTIFIER,
				15,
				(const uint8_t*)"0987654321     ",
			},
			{
				EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE,
				2,
				(const uint8_t[]){ 0x05, 0x28 },
			},
			{
				EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT,
				4,
				(const uint8_t[]){ 0x00, 0x00, 0x27, 0x10 },
			},
			{
				EMV_TAG_9F1C_TERMINAL_IDENTIFICATION,
				8,
				(const uint8_t*)"TID12345",
			},
		},
	},

	{
		.name = "Missing <tlv> id",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv>0528</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "Zero <tlv> id",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='00'>0528</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <tlv> id",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='ZZ'>0528</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Empty <tlv>",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'></tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.data_count = 1,
		.data = (const struct verify_data_t[]){
			{
				EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE,
				0,
				NULL,
			},
		}
	},

	{
		.name = "Odd-length <tlv> content value",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'>052</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Non-hex <tlv> content value",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'>ZZZZ</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		// Value exceeds 256-byte maximum
		.name = "Excessive <tlv> content value",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'>"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"0000000000000000000000000000000000000000000000000000"
			"    </tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <tlv> content string",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1C'>\"TID12345</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <tlv> content string",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1C'>\"TID12345\" asdf</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Valid <app> with partial match",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <data>\n"
			"      <tlv id='9F09'>012C</tlv>\n"
			"    </data>\n"
			"    <random_selection_percentage>25</random_selection_percentage>\n"
			"    <random_selection_max_percentage>50</random_selection_max_percentage>\n"
			"    <random_selection_threshold>5000</random_selection_threshold>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = 0,
		.app_count = 1,
		.app = (const struct verify_app_t[]){
			{
				.aid = (const uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 },
				.aid_len = 7,
				.asi = EMV_ASI_PARTIAL_MATCH,
				.random_selection_percentage = 25,
				.random_selection_max_percentage = 50,
				.random_selection_threshold = 5000,
				.data_count = 1,
				.data = (const struct verify_data_t[]){
					{
						EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL,
						2,
						(const uint8_t[]){ 0x01, 0x2C },
					},
				},
			},
		},
	},

	{
		.name = "Valid <app> with exact match",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='exact'>\n"
			"    <data>\n"
			"      <tlv id='9F09'>012C</tlv>\n"
			"    </data>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = 0,
		.app_count = 1,
		.app = (const struct verify_app_t[]){
			{
				.aid = (const uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 },
				.aid_len = 7,
				.asi = EMV_ASI_EXACT_MATCH,
				.data_count = 1,
				.data = (const struct verify_data_t[]){
					{
						EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL,
						2,
						(const uint8_t[]){ 0x01, 0x2C },
					},
				},
			},
		},
	},

	{
		.name = "Multiple <app> elements in order",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <data>\n"
			"      <tlv id='9F09'>012C</tlv>\n"
			"    </data>\n"
			"    <random_selection_percentage>25</random_selection_percentage>\n"
			"    <random_selection_max_percentage>50</random_selection_max_percentage>\n"
			"    <random_selection_threshold>5000</random_selection_threshold>\n"
			"  </app>\n"
			"  <app aid='A0000000041010' match='partial'>\n"
			"    <data>\n"
			"      <tlv id='9F09'>0002</tlv>\n"
			"    </data>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = 0,
		.app_count = 2,
		.app = (const struct verify_app_t[]){
			{
				.aid = (const uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 },
				.aid_len = 7,
				.asi = EMV_ASI_PARTIAL_MATCH,
				.random_selection_percentage = 25,
				.random_selection_max_percentage = 50,
				.random_selection_threshold = 5000,
				.data_count = 1,
				.data = (const struct verify_data_t[]){
					{
						EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL,
						2,
						(const uint8_t[]){ 0x01, 0x2C },
					},
				},
			},
			{
				.aid = (const uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 },
				.aid_len = 7,
				.asi = EMV_ASI_PARTIAL_MATCH,
				.data_count = 1,
				.data = (const struct verify_data_t[]){
					{
						EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL,
						2,
						(const uint8_t[]){ 0x00, 0x02 },
					},
				},
			},
		},
	},

	{
		.name = "Valid <app> with no children",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A000000025' match='partial'/>\n"
			"</emv>\n",
		.expected_result = 0,
		.app_count = 1,
		.app = (const struct verify_app_t[]){
			{
				.aid = (const uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x25 },
				.aid_len = 5,
				.asi = EMV_ASI_PARTIAL_MATCH,
			},
		},
	},

	{
		.name = "Missing <app> aid attribute",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app match='partial'/>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "Missing <app> match attribute",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010'/>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "Invalid <app> match attribute",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='fuzzy'/>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <app> AID attribute (too short)",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000' match='partial'/>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <app> AID attribute (too long)",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A000000003101000112233445566778899' match='partial'/>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid random_selection_percentage",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <random_selection_percentage>100</random_selection_percentage>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid random_selection_max_percentage",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <random_selection_max_percentage>100</random_selection_max_percentage>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Non-numeric random_selection_percentage",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <random_selection_percentage>abc</random_selection_percentage>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Valid <oid> with quoted UTF-8 content",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='2.5.4.87'>\"example.com\"</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = 0,
		.data_count = 1,
		.data = (const struct verify_data_t[]){
			{
				0x30, // Constructed SEQUENCE
				18,
				(const uint8_t[]){
					0x06, 0x03, 0x55, 0x04, 0x57, // OID 2.5.4.87
					0x0C, 0x0B, // ASN.1 UTF-8 string type
					0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, // "example.com"
				},
			},
		},
	},

	{
		.name = "Valid <oid> with unquoted hex content",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='1.2.840.113549.1.1.1'>05 00</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = 0,
		.data_count = 1,
		.data = (const struct verify_data_t[]){
			{
				0x30, // Constructed SEQUENCE
				13,
				(const uint8_t[]){
					0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, // OID 1.2.840.113549.1.1.1 (rsaEncryption)
					0x05, 0x00, // NULL
				},
			},
		},
	},

	{
		.name = "Two <oid> elements",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='2.5.4.87'>\"example.com\"</oid>\n"
			"    <oid arc='1.2.840.113549.1.1.1'>05 00</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = 0,
	},

	{
		.name = "Missing <oid> arc attribute",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid>\"example.com\"</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_PARSE_ERROR,
	},

	{
		.name = "Invalid <oid> arc (non-numeric)",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='abc.def'>\"example.com\"</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <oid> arc (too few components)",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='2'>\"example.com\"</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	{
		.name = "Invalid <oid> arc (too many components)",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <oid arc='1.2.3.4.5.6.7.8.9.10.11'>\"example.com\"</oid>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_CONFIG_XML_INVALID_DATA,
	},

	// Duplicate tag tests

	{
		.name = "Duplicate <tlv> in <data>",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <data>\n"
			"    <tlv id='9F1A'>0528</tlv>\n"
			"    <tlv id='9F1A'>0826</tlv>\n"
			"  </data>\n"
			"</emv>\n",
		.expected_result = EMV_ERROR_INVALID_CONFIG,
	},

	{
		.name = "Duplicate <tlv> in <app>",
		.xml =
			"<?xml version='1.0' encoding='UTF-8'?>\n"
			"<emv>\n"
			"  <app aid='A0000000031010' match='partial'>\n"
			"    <data>\n"
			"      <tlv id='9F09'>012C</tlv>\n"
			"      <tlv id='9F09'>0002</tlv>\n"
			"    </data>\n"
			"  </app>\n"
			"</emv>\n",
		.expected_result = EMV_ERROR_INVALID_CONFIG,
	},
};

int main(void)
{
	int r;
	struct emv_ctx_t ctx;

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		printf("Test %zu: %s...\n%s", i + 1, test[i].name, test[i].xml);

		r = emv_ctx_init(&ctx, NULL);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		r = emv_config_xml_load_buf(&ctx, test[i].xml, strlen(test[i].xml));
		if (r != test[i].expected_result) {
			fprintf(stderr, "emv_config_xml_load_buf() returned %d; expected %d\n", r, test[i].expected_result);
			r = 1;
			goto exit;
		}

		for (size_t j = 0; j < test[i].data_count; ++j) {
			const struct verify_data_t* verify = &test[i].data[j];
			const struct emv_tlv_t* tlv;

			tlv = emv_config_data_get(&ctx, verify->tag);
			if (!tlv) {
				fprintf(stderr, "emv_config_data_get() failed to find field %04X\n", verify->tag);
				r = 1;
				goto exit;
			}
			if (tlv->length != verify->length ||
				(
					verify->length > 0 &&
					memcmp(tlv->value, verify->value, verify->length) != 0
				)
			) {
				fprintf(stderr, "Incorrect value for field %04X\n", verify->tag);
				print_buf("Found", tlv->value, tlv->length);
				print_buf("Expected", verify->value, verify->length);
				r = 1;
				goto exit;
			}
		}

		if (test[i].app_count) {
			struct emv_config_app_itr_t app_itr;
			const struct emv_config_app_t* config_app;
			size_t app_idx = 0;

			r = emv_config_app_itr_init(&ctx.config, &app_itr);
			if (r) {
				fprintf(stderr, "emv_config_app_itr_init() failed; r=%d\n", r);
				r = 1;
				goto exit;
			}

			while ((config_app = emv_config_app_itr_next(&app_itr)) != NULL) {
				const struct verify_app_t* va;

				if (app_idx >= test[i].app_count) {
					fprintf(stderr, "Too many apps; expected %zu\n", test[i].app_count);
					r = 1;
					goto exit;
				}
				va = &test[i].app[app_idx];

				if (config_app->aid_len != va->aid_len ||
					memcmp(config_app->aid, va->aid, va->aid_len) != 0
				) {
					fprintf(stderr, "App %zu: incorrect AID\n", app_idx);
					print_buf("Found", config_app->aid, config_app->aid_len);
					print_buf("Expected", va->aid, va->aid_len);
					r = 1;
					goto exit;
				}
				if (config_app->asi != va->asi) {
					fprintf(stderr, "App %zu: incorrect ASI; found %02X, expected %02X\n",
						app_idx, config_app->asi, va->asi);
					r = 1;
					goto exit;
				}
				if (config_app->random_selection_percentage != va->random_selection_percentage) {
					fprintf(stderr, "App %zu: incorrect random_selection_percentage; found %u, expected %u\n",
						app_idx, config_app->random_selection_percentage, va->random_selection_percentage);
					r = 1;
					goto exit;
				}
				if (config_app->random_selection_max_percentage != va->random_selection_max_percentage) {
					fprintf(stderr, "App %zu: incorrect random_selection_max_percentage; found %u, expected %u\n",
						app_idx, config_app->random_selection_max_percentage, va->random_selection_max_percentage);
					r = 1;
					goto exit;
				}
				if (config_app->random_selection_threshold != va->random_selection_threshold) {
					fprintf(stderr, "App %zu: incorrect random_selection_threshold; found %u, expected %u\n",
						app_idx, config_app->random_selection_threshold, va->random_selection_threshold);
					r = 1;
					goto exit;
				}

				for (size_t j = 0; j < va->data_count; ++j) {
					const struct verify_data_t* verify = &va->data[j];
					const struct emv_tlv_t* tlv;

					tlv = emv_tlv_list_find_const(&config_app->data, verify->tag);
					if (!tlv) {
						fprintf(stderr, "App %zu: failed to find field %04X\n", app_idx, verify->tag);
						r = 1;
						goto exit;
					}
					if (tlv->length != verify->length ||
						(
							verify->length > 0 &&
							memcmp(tlv->value, verify->value, verify->length) != 0
						)
					) {
						fprintf(stderr, "App %zu: incorrect value for field %04X\n", app_idx, verify->tag);
						print_buf("Found", tlv->value, tlv->length);
						print_buf("Expected", verify->value, verify->length);
						r = 1;
						goto exit;
					}
				}

				++app_idx;
			}

			if (app_idx != test[i].app_count) {
				fprintf(stderr, "Too few apps; found %zu, expected %zu\n",
					app_idx, test[i].app_count);
				r = 1;
				goto exit;
			}
		}

		r = emv_ctx_clear(&ctx);
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
	emv_ctx_clear(&ctx);

	return r;
}
