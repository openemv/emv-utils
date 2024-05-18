/**
 * @file emv_aid_info_test.c
 * @brief Unit tests for AID info helper function
 *
 * Copyright 2023 Leon Lynch
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

#include "emv_fields.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	int r;
	struct emv_aid_info_t info;

	// Test V Pay
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20 }, 7, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_VISA || info.product != EMV_CARD_PRODUCT_VISA_VPAY) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test Cirrus
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x60, 0x00 }, 7, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_MASTERCARD || info.product != EMV_CARD_PRODUCT_MASTERCARD_CIRRUS) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test Discover USA Debit
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x01, 0x52, 0x40, 0x10 }, 7, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_DISCOVER || info.product != EMV_CARD_PRODUCT_DISCOVER_USA_DEBIT) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test Discover ZIP
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x03, 0x24, 0x10, 0x10 }, 7, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_DISCOVER || info.product != EMV_CARD_PRODUCT_DISCOVER_ZIP) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test UnionPay Quasi-credit
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x03, 0x33, 0x01, 0x01, 0x03 }, 8, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_UNIONPAY || info.product != EMV_CARD_PRODUCT_UNIONPAY_QUASI_CREDIT) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test eftpos (Australia) cheque
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x03, 0x84, 0x20 }, 6, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_EFTPOS || info.product != EMV_CARD_PRODUCT_EFTPOS_CHEQUE) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}

	// Test unknown GIM-UEMOA product
	r = emv_aid_get_info((uint8_t[]){ 0xA0, 0x00, 0x00, 0x03, 0x37, 0x20, 0x20 }, 7, &info);
	if (r) {
		fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
		return 1;
	}
	if (info.scheme != EMV_CARD_SCHEME_GIMUEMOA || info.product != EMV_CARD_PRODUCT_UNKNOWN) {
		fprintf(stderr, "emv_aid_get_info() failed to identify scheme or product\n");
		return 1;
	}
}
