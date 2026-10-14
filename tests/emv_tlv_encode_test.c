/**
 * @file emv_tlv_encode_test.c
 * @brief Unit tests for EMV TLV list encoding
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

#include "emv_tlv.h"
#include "emv_tags.h"
#include "iso8825_ber.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

// Mixture of one-byte and two-byte EMV tags
static const struct emv_tlv_t test1_fields[] = {
	{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F }, 0 }}, NULL },
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x12, 0x34 }, 0 }}, NULL },
	{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x26, 0x09, 0x21 }, 0 }}, NULL },
	{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
};
static const uint8_t test1_encoded[] = {
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
	0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34,
	0x9A, 0x03, 0x26, 0x09, 0x21,
	0x9F, 0x1A, 0x02, 0x05, 0x28,
};
static const uint8_t test1_encoded_template[] = {
	0x70, 0x1D,
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
	0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34,
	0x9A, 0x03, 0x26, 0x09, 0x21,
	0x9F, 0x1A, 0x02, 0x05, 0x28,
};

// Field large enough to require the long length form for the template field
static uint8_t test2_certificate[176];
static const struct emv_tlv_t test2_fields[] = {
	{ {{ EMV_TAG_50_APPLICATION_LABEL, 4, (uint8_t[]){ 0x56, 0x49, 0x53, 0x41 }, 0 }}, NULL },
	{ {{ EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE, sizeof(test2_certificate), test2_certificate, 0 }}, NULL },
};
static const uint8_t test2_encoded_header[] = { 0x70, 0x81, 0xB9, 0x50, 0x04, 0x56, 0x49, 0x53, 0x41, 0x90, 0x81, 0xB0 };

// Empty template field, as used for the Command Template (field 83)
static const uint8_t test3_encoded_template[] = { 0x70, 0x00 };

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
	struct emv_tlv_list_t list = EMV_TLV_LIST_INIT;
	struct emv_tlv_list_t parsed_list = EMV_TLV_LIST_INIT;
	struct iso8825_oid_t oid;
	uint8_t buf[1024];
	size_t buf_len;

	printf("\nTest 1: Compute and encode empty list without template\n");
	r = emv_tlv_list_compute_encoded_length(&list, 0);
	if (r != 0) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != 0) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 2: Compute and encode empty list with template\n");
	r = emv_tlv_list_compute_encoded_length(&list, EMV_TAG_70_DATA_TEMPLATE);
	if (r != (int)sizeof(test3_encoded_template)) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test3_encoded_template) ||
		memcmp(buf, test3_encoded_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test3_encoded_template, sizeof(test3_encoded_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 3: Compute and encode list without template\n");
	r = populate_list(test1_fields, sizeof(test1_fields) / sizeof(test1_fields[0]), &list);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_tlv_list_compute_encoded_length(&list, 0);
	if (r != (int)sizeof(test1_encoded)) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test1_encoded) ||
		memcmp(buf, test1_encoded, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test1_encoded, sizeof(test1_encoded));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 4: Compute and encode list with template\n");
	r = emv_tlv_list_compute_encoded_length(&list, EMV_TAG_70_DATA_TEMPLATE);
	if (r != (int)sizeof(test1_encoded_template)) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test1_encoded_template) ||
		memcmp(buf, test1_encoded_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test1_encoded_template, sizeof(test1_encoded_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 5: Encode list with an output buffer that is exactly the right size\n");
	buf_len = sizeof(test1_encoded_template);
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test1_encoded_template) ||
		memcmp(buf, test1_encoded_template, buf_len) != 0
	) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test1_encoded_template, sizeof(test1_encoded_template));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 6: Encode list with an output buffer that is too small\n");
	buf_len = sizeof(test1_encoded_template) - 1; // One byte less than what is needed
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r != 1) {
		fprintf(stderr, "Unexpected emv_tlv_list_encode() return value; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 7: Encode list with a template tag that is not constructed\n");
	r = emv_tlv_list_compute_encoded_length(&list, EMV_TAG_50_APPLICATION_LABEL);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, EMV_TAG_50_APPLICATION_LABEL, buf, &buf_len);
	if (r >= 0) {
		fprintf(stderr, "emv_tlv_list_encode() unexpectedly did not report error; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 8: Parse encoded list with template\n");
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_tlv_parse(buf, buf_len, &parsed_list);
	if (r) {
		fprintf(stderr, "emv_tlv_parse() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	{
		const struct emv_tlv_t* tlv = list.front;
		const struct emv_tlv_t* parsed_tlv = parsed_list.front;

		while (tlv && parsed_tlv) {
			if (tlv->tag != parsed_tlv->tag ||
				tlv->length != parsed_tlv->length ||
				memcmp(tlv->value, parsed_tlv->value, tlv->length) != 0
			) {
				fprintf(stderr, "Parsed field does not match encoded field\n");
				print_emv_tlv_list(&parsed_list);
				r = 1;
				goto exit;
			}

			tlv = tlv->next;
			parsed_tlv = parsed_tlv->next;
		}
		if (tlv || parsed_tlv) {
			fprintf(stderr, "Parsed list does not match encoded list\n");
			print_emv_tlv_list(&parsed_list);
			r = 1;
			goto exit;
		}
	}
	emv_tlv_list_clear(&parsed_list);
	printf("Success\n");

	printf("\nTest 9: Compute and encode list using long length form for template\n");
	for (size_t i = 0; i < sizeof(test2_certificate); ++i) { test2_certificate[i] = i; }
	r = populate_list(test2_fields, sizeof(test2_fields) / sizeof(test2_fields[0]), &list);
	if (r) {
		fprintf(stderr, "populate_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_tlv_list_compute_encoded_length(&list, EMV_TAG_70_DATA_TEMPLATE);
	if (r != (int)(sizeof(test2_encoded_header) + sizeof(test2_certificate))) {
		fprintf(stderr, "emv_tlv_list_compute_encoded_length() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, EMV_TAG_70_DATA_TEMPLATE, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(test2_encoded_header) + sizeof(test2_certificate) ||
		memcmp(buf, test2_encoded_header, sizeof(test2_encoded_header)) != 0
	) {
		fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test2_encoded_header, sizeof(test2_encoded_header));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 10: Encode list containing ASN.1 object\n");
	emv_tlv_list_clear(&list);
	oid = ASN1_OID(commonName);
	r = emv_tlv_list_push_asn1_object(&list, &oid, 0, NULL);
	if (r) {
		fprintf(stderr, "emv_tlv_list_push_asn1_object() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	buf_len = sizeof(buf);
	r = emv_tlv_list_encode(&list, 0, buf, &buf_len);
	if (r) {
		fprintf(stderr, "emv_tlv_list_encode() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	{
		// Constructed sequence containing OID commonName, encoded as-is
		const uint8_t expected[] = { 0x30, 0x05, 0x06, 0x03, 0x55, 0x04, 0x03 };

		if (buf_len != sizeof(expected) ||
			memcmp(buf, expected, buf_len) != 0
		) {
			fprintf(stderr, "emv_tlv_list_encode() failed; incorrect output data\n");
			print_buf("encoded", buf, buf_len);
			print_buf("expected", expected, sizeof(expected));
			r = 1;
			goto exit;
		}
	}
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&list);
	emv_tlv_list_clear(&parsed_list);

	return r;
}
