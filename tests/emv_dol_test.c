/**
 * @file emv_dol_test.c
 * @brief Unit tests for Data Object List (DOL) processing
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

#include "emv_dol.h"
#include "emv_tlv.h"
#include "emv_tags.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

const uint8_t test1_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04 };
const struct emv_dol_entry_t test1_dol_entries[] = {
	{ 0x9F02, 6 },
	{ 0x9F03, 6 },
	{ 0x9F1A, 2 },
	{ 0x95, 5 },
	{ 0x5F2A, 2 },
	{ 0x9A, 3 },
	{ 0x9C, 1 },
	{ 0x9F37, 4 },
};

const uint8_t test2_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95 };
const struct emv_dol_entry_t test2_dol_entries[] = {
	{ 0x9F02, 6 },
	{ 0x9F03, 6 },
	{ 0x9F1A, 2 },
};

const uint8_t test3_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37 };
const struct emv_dol_entry_t test3_dol_entries[] = {
	{ 0x9F02, 6 },
	{ 0x9F03, 6 },
	{ 0x9F1A, 2 },
	{ 0x95, 5 },
	{ 0x5F2A, 2 },
	{ 0x9A, 3 },
	{ 0x9C, 1 },
};

const uint8_t test4_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04 };
const int test4_data_len = 6 + 6 + 2 + 5 + 2 + 3 + 1 + 4;

const uint8_t test5_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37 };

const uint8_t test6_dol[] = { 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02, 0x95, 0x05, 0x5F, 0x2A, 0x02, 0x9A, 0x03, 0x9C, 0x01, 0x9F, 0x37, 0x04 };
const struct emv_tlv_t test6_source1[] = {
	{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ 0x09 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x24, 0x02, 0x17 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x09, 0x87, 0x65, 0x43, 0x21 }, 0 }}, NULL },
};
const struct emv_tlv_t test6_source2[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F37_UNPREDICTABLE_NUMBER, 4, (uint8_t[]){ 0xDE, 0xAD, 0xBE, 0xEF }, 0 }}, NULL },
	{ {{ EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x12, 0x34, 0x55, 0x43, 0x21 }, 0 }}, NULL },
};
const uint8_t test6_data[] = {
	0x00, 0x01, 0x23, 0x45, 0x67, 0x89,
	0x00, 0x09, 0x87, 0x65, 0x43, 0x21,
	0x05, 0x28,
	0x12, 0x34, 0x55, 0x43, 0x21,
	0x09, 0x78,
	0x24, 0x02, 0x17,
	0x09,
	0xDE, 0xAD, 0xBE, 0xEF,
};

const uint8_t test7_dol[] = {
	0x9F, 0x02, 0x03, // Shorter than original data length
	0x9F, 0x03, 0x07, // Longer than original length
	0x9F, 0x1A, 0x02,
	0x95, 0x06, // Longer than original length
	0x5F, 0x2A, 0x02,
	0x9A, 0x00, // Zero length
	0x9C, 0x03, // Longer than original length
	0x9F, 0x37, 0x03, // Shorter than original data length
};
const struct emv_tlv_t test7_source1[] = {
	{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ 0x09 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x24, 0x02, 0x17 }, 0 }}, NULL },
	{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x09, 0x87, 0x65, 0x43, 0x21 }, 0 }}, NULL },
};
const struct emv_tlv_t test7_source2[] = {
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F37_UNPREDICTABLE_NUMBER, 4, (uint8_t[]){ 0xDE, 0xAD, 0xBE, 0xEF }, 0 }}, NULL },
	{ {{ EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x12, 0x34, 0x55, 0x43, 0x21 }, 0 }}, NULL },
};
const uint8_t test7_data[] = {
	0x45, 0x67, 0x89, // Shorter than original data length
	0x00, 0x00, 0x09, 0x87, 0x65, 0x43, 0x21, // Longer than original length
	0x05, 0x28,
	0x12, 0x34, 0x55, 0x43, 0x21, 0x00, // Longer than original length
	0x09, 0x78,
	// Removed due to zero length
	0x00, 0x00, 0x09, // Longer than original length
	0xDE, 0xAD, 0xBE, // Shorter than original data length
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
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;
	uint8_t data[256];
	size_t data_len;
	struct emv_tlv_list_t source1 = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t source2 = EMV_TLV_LIST_INIT;

	printf("\nTest 1: Iterate valid DOL\n");
	r = emv_dol_itr_init(test1_dol, sizeof(test1_dol), &itr);
	if (r) {
		fprintf(stderr, "emv_dol_itr_init() failed; r=%d\n", r);
		return 1;
	}
	for (size_t i = 0; i < sizeof(test1_dol_entries) / sizeof(test1_dol_entries[0]); ++i) {
		r = emv_dol_itr_next(&itr, &entry);
		if (r <= 0) {
			fprintf(stderr, "emv_dol_itr_next() failed; r=%d\n", r);
			return 1;
		}

		if (memcmp(&entry, &test1_dol_entries[i], sizeof(entry)) != 0) {
			fprintf(stderr, "DOL entry mismatch; i=%zu\n", i);
			return 1;
		}
	}
	r = emv_dol_itr_next(&itr, &entry);
	if (r != 0) {
		fprintf(stderr, "emv_dol_itr_next() unexpectedly did not report end-of-data; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 2: Iterate invalid DOL (malformed entry)\n");
	r = emv_dol_itr_init(test2_dol, sizeof(test2_dol), &itr);
	if (r) {
		fprintf(stderr, "emv_dol_itr_init() failed; r=%d\n", r);
		return 1;
	}
	for (size_t i = 0; i < sizeof(test2_dol_entries) / sizeof(test2_dol_entries[0]); ++i) {
		r = emv_dol_itr_next(&itr, &entry);
		if (r <= 0) {
			fprintf(stderr, "emv_dol_itr_next() failed; r=%d\n", r);
			return 1;
		}

		if (memcmp(&entry, &test2_dol_entries[i], sizeof(entry)) != 0) {
			fprintf(stderr, "DOL entry mismatch; i=%zu\n", i);
			return 1;
		}
	}
	r = emv_dol_itr_next(&itr, &entry);
	if (r >= 0) {
		fprintf(stderr, "emv_dol_itr_next() unexpectedly did not report error; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 3: Iterate invalid DOL (insufficient bytes remaining)\n");
	r = emv_dol_itr_init(test3_dol, sizeof(test3_dol), &itr);
	if (r) {
		fprintf(stderr, "emv_dol_itr_init() failed; r=%d\n", r);
		return 1;
	}
	for (size_t i = 0; i < sizeof(test3_dol_entries) / sizeof(test3_dol_entries[0]); ++i) {
		r = emv_dol_itr_next(&itr, &entry);
		if (r <= 0) {
			fprintf(stderr, "emv_dol_itr_next() failed; r=%d\n", r);
			return 1;
		}

		if (memcmp(&entry, &test3_dol_entries[i], sizeof(entry)) != 0) {
			fprintf(stderr, "DOL entry mismatch; i=%zu\n", i);
			return 1;
		}
	}
	r = emv_dol_itr_next(&itr, &entry);
	if (r >= 0) {
		fprintf(stderr, "emv_dol_itr_next() unexpectedly did not report error; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 4: Compute data length\n");
	r = emv_dol_compute_data_length(test4_dol, sizeof(test4_dol));
	if (r != test4_data_len) {
		fprintf(stderr, "emv_dol_compute_data_length() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 5: Compute data length for invalid DOL\n");
	r = emv_dol_compute_data_length(test5_dol, sizeof(test5_dol));
	if (r >= 0) {
		fprintf(stderr, "emv_dol_compute_data_length() unexpectedly did not report error; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 6: Build DOL data with exact lengths\n");
	r = populate_source(test6_source1, sizeof(test6_source1) / sizeof(test6_source1[0]), &source1);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}
	r = populate_source(test6_source2, sizeof(test6_source2) / sizeof(test6_source2[0]), &source2);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}
	for (size_t i = 0; i < sizeof(data); ++i) { data[i] = i; }
	data_len = sizeof(data);
	r = emv_dol_build_data(
		test6_dol,
		sizeof(test6_dol),
		&source1,
		&source2,
		data,
		&data_len
	);
	if (r) {
		fprintf(stderr, "emv_dol_build_data() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test6_data) ||
		memcmp(data, test6_data, sizeof(test6_data)) != 0
	) {
		fprintf(stderr, "emv_dol_build_data() failed; incorrect output data\n");
		print_buf("data", data, data_len);
		print_buf("test6_data", test6_data, sizeof(test6_data));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 7: Build DOL data with differing lengths\n");
	r = populate_source(test7_source1, sizeof(test7_source1) / sizeof(test7_source1[0]), &source1);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}
	r = populate_source(test7_source2, sizeof(test7_source2) / sizeof(test7_source2[0]), &source2);
	if (r) {
		fprintf(stderr, "populate_source() failed; r=%d\n", r);
		return 1;
	}
	for (size_t i = 0; i < sizeof(data); ++i) { data[i] = i; }
	data_len = sizeof(data);
	r = emv_dol_build_data(
		test7_dol,
		sizeof(test7_dol),
		&source1,
		&source2,
		data,
		&data_len
	);
	if (r) {
		fprintf(stderr, "emv_dol_build_data() failed; r=%d\n", r);
		return 1;
	}
	if (data_len != sizeof(test7_data) ||
		memcmp(data, test7_data, sizeof(test7_data)) != 0
	) {
		fprintf(stderr, "emv_dol_build_data() failed; incorrect output data\n");
		print_buf("data", data, data_len);
		print_buf("test7_data", test7_data, sizeof(test7_data));
		return 1;
	}
	printf("Success\n");

	emv_tlv_list_clear(&source1);
	emv_tlv_list_clear(&source2);
	return 0;
}
