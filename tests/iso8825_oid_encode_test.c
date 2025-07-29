/**
 * @file iso8825_oid_encode_test.c
 * @brief Unit tests for ISO 8825-1 BER OID encoding
 *
 * Copyright 2025 Leon Lynch
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

int main(void)
{
	int r;
	struct iso8825_oid_t oid;
	uint8_t buf[32];
	size_t buf_len;

	printf("\nTesting encoding of OID commonName...\n");
	oid = ASN1_OID(commonName);
	buf_len = sizeof(buf);
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		return 1;
	}
	const uint8_t encoded_commonName[] = { 0x55, 0x04, 0x03 };
	if (buf_len != sizeof(encoded_commonName) ||
		memcmp(buf, encoded_commonName, buf_len) != 0
	) {
		fprintf(stderr, "Encoded of OID commonName is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", encoded_commonName, sizeof(encoded_commonName));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID sha256WithRSAEncryption...\n");
	oid = ASN1_OID(sha256WithRSAEncryption);
	buf_len = sizeof(buf);
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		return 1;
	}
	const uint8_t encoded_sha256WithRSAEncryption[] = { 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B };
	if (buf_len != sizeof(encoded_sha256WithRSAEncryption) ||
		memcmp(buf, encoded_sha256WithRSAEncryption, buf_len) != 0
	) {
		fprintf(stderr, "Encoded of OID sha256WithRSAEncryption is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", encoded_sha256WithRSAEncryption, sizeof(encoded_sha256WithRSAEncryption));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID cmac containing a zero subidentifier...\n");
	oid = ASN1_OID(cmac);
	buf_len = sizeof(buf);
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		return 1;
	}
	const uint8_t encoded_cmac[] = { 0x28, 0xCC, 0x45, 0x01, 0x03, 0x05 };
	if (buf_len != sizeof(encoded_cmac) ||
		memcmp(buf, encoded_cmac, buf_len) != 0
	) {
		fprintf(stderr, "Encoded of OID cmac is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", encoded_cmac, sizeof(encoded_cmac));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID padNull containing multiple zero subidentifiers...\n");
	oid = ASN1_OID(padNull);
	buf_len = sizeof(buf);
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		return 1;
	}
	const uint8_t encoded_padNull[] = { 0x28, 0xCF, 0x04, 0x00, 0x02, 0x00 };
	if (buf_len != sizeof(encoded_padNull) ||
		memcmp(buf, encoded_padNull, buf_len) != 0
	) {
		fprintf(stderr, "Encoded of OID padNull is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", encoded_padNull, sizeof(encoded_padNull));
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID prime256v1 with an output buffer that is too small...\n");
	oid = ASN1_OID(prime256v1);
	buf_len = 6; // One byte less than the number of subidentifiers in 1.2.840.10045.3.1.7
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r != -3) {
		fprintf(stderr, "Unexpected iso8825_ber_oid_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID prime256v1 with an output buffer that is too small...\n");
	oid = ASN1_OID(prime256v1);
	buf_len = 7; // One byte less than what is needed to encode 1.2.840.10045.3.1.7
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r != -4) {
		fprintf(stderr, "Unexpected iso8825_ber_oid_encode() return value; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("\nTesting encoding of OID prime256v1 with an output buffer that is exactly the right size...\n");
	oid = ASN1_OID(prime256v1);
	buf_len = 8; // Exact number of bytes needed to encode 1.2.840.10045.3.1.7
	r = iso8825_ber_oid_encode(&oid, buf, &buf_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		return 1;
	}
	const uint8_t encoded_prime256v1[] = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07 };
	if (buf_len != sizeof(encoded_prime256v1) ||
		memcmp(buf, encoded_prime256v1, buf_len) != 0
	) {
		fprintf(stderr, "Encoded of OID prime256v1 is incorrect\n");
		print_buf("encoded", buf, buf_len);
		print_buf("expected", encoded_prime256v1, sizeof(encoded_prime256v1));
		return 1;
	}
	printf("Success\n");

	return 0;
}
