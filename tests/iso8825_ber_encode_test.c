/**
 * @file iso8825_ber_encode_test.c
 * @brief Unit tests for ISO 8825-1 BER encoding
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

#include "iso8825_ber.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "print_helpers.h"

struct test_tag_t {
	unsigned int tag;                   ///< BER encoded tag
	unsigned int encoded_len;           ///< Expected number of tag octets
	const uint8_t* encoded;             ///< Expected tag octets
	bool constructed;                   ///< Expected primitive/constructed bit
};

static const struct test_tag_t test_tags[] = {
	{ 0x00, 1, (uint8_t[]){ 0x00 }, false }, // End-of-content
	{ 0x30, 1, (uint8_t[]){ 0x30 }, true }, // ASN.1 sequence
	{ 0x5A, 1, (uint8_t[]){ 0x5A }, false }, // Application PAN
	{ 0x70, 1, (uint8_t[]){ 0x70 }, true }, // EMV Data Template
	{ 0x9A, 1, (uint8_t[]){ 0x9A }, false }, // Transaction Date
	{ 0xA5, 1, (uint8_t[]){ 0xA5 }, true }, // FCI Proprietary Template
	{ 0x5F2A, 2, (uint8_t[]){ 0x5F, 0x2A }, false }, // Transaction Currency Code
	{ 0x9F02, 2, (uint8_t[]){ 0x9F, 0x02 }, false }, // Amount, Authorised (Numeric)
	{ 0xBF0C, 2, (uint8_t[]){ 0xBF, 0x0C }, true }, // FCI Issuer Discretionary Data
	{ 0xDF8101, 3, (uint8_t[]){ 0xDF, 0x81, 0x01 }, false }, // Proprietary tag
	{ 0xBF818101, 4, (uint8_t[]){ 0xBF, 0x81, 0x81, 0x01 }, true }, // Proprietary tag
};

static const unsigned int test_malformed_tags[] = {
	0x1F, // High tag number form without subsequent octets
	0x5F, // High tag number form without subsequent octets
	0x9F82, // Last octet indicates that more octets follow
	0x5002, // Low tag number form with subsequent octets
	0xDF0101, // Second octet does not indicate that more octets follow
};

struct test_len_t {
	unsigned int length;                ///< BER length value
	unsigned int encoded_len;           ///< Expected number of length octets
	const uint8_t* encoded;             ///< Expected length octets
};

static const struct test_len_t test_lens[] = {
	{ 0, 1, (uint8_t[]){ 0x00 } },
	{ 1, 1, (uint8_t[]){ 0x01 } },
	{ 0x7F, 1, (uint8_t[]){ 0x7F } }, // Largest short length form value
	{ 0x80, 2, (uint8_t[]){ 0x81, 0x80 } }, // Smallest long length form value
	{ 0xFF, 2, (uint8_t[]){ 0x81, 0xFF } },
	{ 0x100, 3, (uint8_t[]){ 0x82, 0x01, 0x00 } },
	{ 0xFFFF, 3, (uint8_t[]){ 0x82, 0xFF, 0xFF } },
	{ 0x10000, 4, (uint8_t[]){ 0x83, 0x01, 0x00, 0x00 } },
	{ 0xFFFFFF, 4, (uint8_t[]){ 0x83, 0xFF, 0xFF, 0xFF } },
	{ 0x1000000, 5, (uint8_t[]){ 0x84, 0x01, 0x00, 0x00, 0x00 } },
	{ 0xFFFFFFFF, 5, (uint8_t[]){ 0x84, 0xFF, 0xFF, 0xFF, 0xFF } },
};

// Application PAN (field 5A)
static const uint8_t test_5A_value[] = { 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F };
static const uint8_t test_5A_encoded[] = {
	0x5A, 0x08, 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x1F,
};

// Empty Command Template (field 83)
static const uint8_t test_83_encoded[] = { 0x83, 0x00 };

// EMV Data Template (field 70) containing pre-encoded subfields
static const uint8_t test_70_value[] = {
	0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34,
	0x9A, 0x03, 0x26, 0x09, 0x21,
};
static const uint8_t test_70_encoded[] = {
	0x70, 0x0E,
	0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34,
	0x9A, 0x03, 0x26, 0x09, 0x21,
};

// Issuer Public Key Certificate (field 90) requiring the long length form
static const uint8_t test_90_encoded_header[] = { 0x90, 0x81, 0xB0 };

int main(void)
{
	int r;
	uint8_t buf[512];
	size_t buf_len;
	struct iso8825_tlv_t tlv;
	uint8_t test_90_value[176];

	printf("\nTest 1: Compute and encode tag octets\n");
	for (size_t i = 0; i < sizeof(test_tags) / sizeof(test_tags[0]); ++i) {
		r = iso8825_ber_tag_compute_encoded_length(test_tags[i].tag);
		if (r != (int)test_tags[i].encoded_len) {
			fprintf(stderr, "iso8825_ber_tag_compute_encoded_length() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}

		buf_len = sizeof(buf);
		r = iso8825_ber_tag_encode(test_tags[i].tag, buf, &buf_len);
		if (r) {
			fprintf(stderr, "iso8825_ber_tag_encode() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}
		if (buf_len != test_tags[i].encoded_len ||
			memcmp(buf, test_tags[i].encoded, buf_len) != 0
		) {
			fprintf(stderr, "Encoded tag octets are incorrect; i=%zu\n", i);
			print_buf("encoded", buf, buf_len);
			print_buf("expected", test_tags[i].encoded, test_tags[i].encoded_len);
			return 1;
		}
	}
	printf("Success\n");

	printf("\nTest 2: Determine whether tag is constructed\n");
	for (size_t i = 0; i < sizeof(test_tags) / sizeof(test_tags[0]); ++i) {
		if (iso8825_ber_tag_is_constructed(test_tags[i].tag) != test_tags[i].constructed) {
			fprintf(stderr, "iso8825_ber_tag_is_constructed() is incorrect; i=%zu\n", i);
			return 1;
		}
	}
	printf("Success\n");

	printf("\nTest 3: Compute and encode malformed tag octets\n");
	for (size_t i = 0; i < sizeof(test_malformed_tags) / sizeof(test_malformed_tags[0]); ++i) {
		r = iso8825_ber_tag_compute_encoded_length(test_malformed_tags[i]);
		if (r >= 0) {
			fprintf(stderr, "iso8825_ber_tag_compute_encoded_length() unexpectedly did not report error; i=%zu; r=%d\n", i, r);
			return 1;
		}

		buf_len = sizeof(buf);
		r = iso8825_ber_tag_encode(test_malformed_tags[i], buf, &buf_len);
		if (r != -2) {
			fprintf(stderr, "Unexpected iso8825_ber_tag_encode() return value; i=%zu; r=%d\n", i, r);
			return 1;
		}
	}
	printf("Success\n");

	printf("\nTest 4: Encode tag octets with an output buffer that is too small\n");
	buf_len = 1; // One byte less than what is needed to encode tag 9F02
	r = iso8825_ber_tag_encode(0x9F02, buf, &buf_len);
	if (r != -3) {
		fprintf(stderr, "Unexpected iso8825_ber_tag_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 5: Compute and encode length octets\n");
	for (size_t i = 0; i < sizeof(test_lens) / sizeof(test_lens[0]); ++i) {
		r = iso8825_ber_len_compute_encoded_length(test_lens[i].length);
		if (r != (int)test_lens[i].encoded_len) {
			fprintf(stderr, "iso8825_ber_len_compute_encoded_length() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}

		buf_len = sizeof(buf);
		r = iso8825_ber_len_encode(test_lens[i].length, buf, &buf_len);
		if (r) {
			fprintf(stderr, "iso8825_ber_len_encode() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}
		if (buf_len != test_lens[i].encoded_len ||
			memcmp(buf, test_lens[i].encoded, buf_len) != 0
		) {
			fprintf(stderr, "Encoded length octets are incorrect; i=%zu\n", i);
			print_buf("encoded", buf, buf_len);
			print_buf("expected", test_lens[i].encoded, test_lens[i].encoded_len);
			return 1;
		}
	}
	printf("Success\n");

	printf("\nTest 6: Encode length octets with an output buffer that is too small\n");
	buf_len = 2; // One byte less than what is needed to encode length 0x100
	r = iso8825_ber_len_encode(0x100, buf, &buf_len);
	if (r != -2) {
		fprintf(stderr, "Unexpected iso8825_ber_len_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 7: Compute and encode primitive field\n");
	tlv.tag = 0x5A;
	tlv.length = sizeof(test_5A_value);
	tlv.value = test_5A_value;
	tlv.flags = 0;
	r = iso8825_ber_compute_encoded_length(&tlv);
	if (r != (int)sizeof(test_5A_encoded)) {
		fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
		return 1;
	}
	buf_len = sizeof(buf);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		return 1;
	}
	if (buf_len != sizeof(test_5A_encoded) ||
		memcmp(buf, test_5A_encoded, buf_len) != 0
	) {
		fprintf(stderr, "Encoded field is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_5A_encoded, sizeof(test_5A_encoded));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 8: Compute and encode zero length field\n");
	tlv.tag = 0x83;
	tlv.length = 0;
	tlv.value = NULL;
	tlv.flags = 0;
	r = iso8825_ber_compute_encoded_length(&tlv);
	if (r != (int)sizeof(test_83_encoded)) {
		fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
		return 1;
	}
	buf_len = sizeof(buf);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		return 1;
	}
	if (buf_len != sizeof(test_83_encoded) ||
		memcmp(buf, test_83_encoded, buf_len) != 0
	) {
		fprintf(stderr, "Encoded field is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_83_encoded, sizeof(test_83_encoded));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 9: Compute and encode constructed field\n");
	tlv.tag = 0x70;
	tlv.length = sizeof(test_70_value);
	tlv.value = test_70_value;
	tlv.flags = 0; // Intentionally not set to confirm that the tag is used
	r = iso8825_ber_compute_encoded_length(&tlv);
	if (r != (int)sizeof(test_70_encoded)) {
		fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
		return 1;
	}
	buf_len = sizeof(buf);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		return 1;
	}
	if (buf_len != sizeof(test_70_encoded) ||
		memcmp(buf, test_70_encoded, buf_len) != 0
	) {
		fprintf(stderr, "Encoded field is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_70_encoded, sizeof(test_70_encoded));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 10: Compute and encode field using long length form\n");
	for (size_t i = 0; i < sizeof(test_90_value); ++i) { test_90_value[i] = i; }
	tlv.tag = 0x90;
	tlv.length = sizeof(test_90_value);
	tlv.value = test_90_value;
	tlv.flags = 0;
	r = iso8825_ber_compute_encoded_length(&tlv);
	if (r != (int)(sizeof(test_90_encoded_header) + sizeof(test_90_value))) {
		fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
		return 1;
	}
	buf_len = sizeof(buf);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		return 1;
	}
	if (buf_len != sizeof(test_90_encoded_header) + sizeof(test_90_value) ||
		memcmp(buf, test_90_encoded_header, sizeof(test_90_encoded_header)) != 0 ||
		memcmp(buf + sizeof(test_90_encoded_header), test_90_value, sizeof(test_90_value)) != 0
	) {
		fprintf(stderr, "Encoded field is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_90_encoded_header, sizeof(test_90_encoded_header));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 11: Encode field with an output buffer that is exactly the right size\n");
	tlv.tag = 0x5A;
	tlv.length = sizeof(test_5A_value);
	tlv.value = test_5A_value;
	tlv.flags = 0;
	buf_len = sizeof(test_5A_encoded);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		return 1;
	}
	if (buf_len != sizeof(test_5A_encoded) ||
		memcmp(buf, test_5A_encoded, buf_len) != 0
	) {
		fprintf(stderr, "Encoded field is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", test_5A_encoded, sizeof(test_5A_encoded));
		return 1;
	}
	printf("Success\n");

	printf("\nTest 12: Encode field with an output buffer that is too small\n");
	buf_len = sizeof(test_5A_encoded) - 1; // One byte less than what is needed
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r != -4) {
		fprintf(stderr, "Unexpected iso8825_ber_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 13: Encode field without value\n");
	tlv.tag = 0x5A;
	tlv.length = sizeof(test_5A_value);
	tlv.value = NULL;
	tlv.flags = 0;
	buf_len = sizeof(buf);
	r = iso8825_ber_encode(&tlv, buf, &buf_len);
	if (r != -2) {
		fprintf(stderr, "Unexpected iso8825_ber_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTest 14: Decode encoded fields\n");
	for (size_t i = 0; i < sizeof(test_tags) / sizeof(test_tags[0]); ++i) {
		struct iso8825_tlv_t decoded_tlv;

		if (test_tags[i].tag == ASN1_EOC) {
			// End-of-content cannot be encoded as an ordinary field
			continue;
		}

		tlv.tag = test_tags[i].tag;
		tlv.length = sizeof(test_5A_value);
		tlv.value = test_5A_value;
		tlv.flags = 0;
		buf_len = sizeof(buf);
		r = iso8825_ber_encode(&tlv, buf, &buf_len);
		if (r) {
			fprintf(stderr, "iso8825_ber_encode() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}

		r = iso8825_ber_decode(buf, buf_len, &decoded_tlv);
		if (r != (int)buf_len) {
			fprintf(stderr, "iso8825_ber_decode() failed; i=%zu; r=%d\n", i, r);
			return 1;
		}
		if (decoded_tlv.tag != tlv.tag ||
			decoded_tlv.length != tlv.length ||
			memcmp(decoded_tlv.value, tlv.value, tlv.length) != 0
		) {
			fprintf(stderr, "Decoded field does not match encoded field; i=%zu\n", i);
			print_buf("encoded", buf, buf_len);
			return 1;
		}
		if (iso8825_ber_is_constructed(&decoded_tlv) != test_tags[i].constructed) {
			fprintf(stderr, "Decoded field has unexpected primitive/constructed bit; i=%zu\n", i);
			return 1;
		}
	}
	printf("Success\n");

	return 0;
}
