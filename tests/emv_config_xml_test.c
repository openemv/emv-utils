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
#include "emv_tlv.h"
#include "emv_tags.h"
#include "print_helpers.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct verify_t {
	unsigned int tag;
	size_t length;
	const uint8_t* value;
};

struct test_t {
	const char* name;
	const char* xml;
	int expected_result;
	size_t verify_count;
	const struct verify_t* verify;
};

static const struct test_t test[] = {
	{
		.name = "No header",
		.xml =
			"<emv/>\n",
		.expected_result = 0,
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
		.verify_count = 5,
		.verify = (const struct verify_t[]){
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
		.verify_count = 1,
		.verify = (const struct verify_t[]){
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

		for (size_t j = 0; j < test[i].verify_count; ++j) {
			const struct verify_t* verify = &test[i].verify[j];
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
