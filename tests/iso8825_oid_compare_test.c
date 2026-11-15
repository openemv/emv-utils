/**
 * @file iso8825_oid_compare_test.c
 * @brief Unit tests for ISO 8825-1 BER OID comparison
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

int main(void)
{
	struct iso8825_oid_t oid;
	struct iso8825_oid_t other;

	printf("\nTesting comparison of equal OIDs...\n");
	oid = ASN1_OID(dukpt_aes128);
	other = ASN1_OID(dukpt_aes128);
	if (!iso8825_ber_oid_is_equal(&oid, &other)) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for equal OIDs\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting comparison of OIDs with different last component...\n");
	oid = ASN1_OID(dukpt_tdes);
	other = ASN1_OID(dukpt_aes128);
	if (iso8825_ber_oid_is_equal(&oid, &other)) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for different OIDs\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting comparison of OIDs with different length...\n");
	oid = ASN1_OID(pad);
	other = ASN1_OID(pad);
	other.value[other.length] = 1;
	++other.length;
	if (iso8825_ber_oid_is_equal(&oid, &other)) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for OIDs with different length\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting comparison of OIDs with equal components beyond length...\n");
	oid = ASN1_OID(commonName);
	other = ASN1_OID(commonName);
	oid.value[oid.length] = 1;
	other.value[other.length] = 2;
	if (!iso8825_ber_oid_is_equal(&oid, &other)) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for OIDs with different components beyond length\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting comparison of OIDs with invalid length...\n");
	oid = ASN1_OID(commonName);
	oid.length = sizeof(oid.value) / sizeof(oid.value[0]) + 1;
	other = oid;
	if (iso8825_ber_oid_is_equal(&oid, &other)) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for OIDs with invalid length\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTesting comparison of NULL OIDs...\n");
	oid = ASN1_OID(commonName);
	if (iso8825_ber_oid_is_equal(&oid, NULL) ||
		iso8825_ber_oid_is_equal(NULL, &oid) ||
		iso8825_ber_oid_is_equal(NULL, NULL)
	) {
		fprintf(stderr, "iso8825_ber_oid_is_equal() failed for NULL OIDs\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
