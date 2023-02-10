/**
 * @file emv_fields.c
 * @brief EMV field helper functions
 *
 * Copyright (c) 2021, 2022 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_fields.h"

int emv_afl_itr_init(const void* afl, size_t afl_len, struct emv_afl_itr_t* itr)
{
	if (!afl || !afl_len || !itr) {
		return -1;
	}

	if ((afl_len & 0x3) != 0) {
		// Application File Locator (field 94) must be multiples of 4 bytes
		return 1;
	}

	if (afl_len > 252) {
		// Application File Locator (field 94) may be up to 252 bytes
		// See EMV 4.4 Book 3, Annex A
		return 2;
	}

	itr->ptr = afl;
	itr->len = afl_len;

	return 0;
}

int emv_afl_itr_next(struct emv_afl_itr_t* itr, struct emv_afl_entry_t* entry)
{
	const uint8_t* afl;

	if (!itr || !entry) {
		return -1;
	}

	if (!itr->len) {
		// End of field
		return 0;
	}

	if ((itr->len & 0x3) != 0) {
		// Application File Locator (field 94) must be multiples of 4 bytes
		return -2;
	}

	// Validate AFL entry
	// See EMV 4.4 Book 3, 10.2
	afl = itr->ptr;
	if ((afl[0] & ~EMV_AFL_SFI_MASK) != 0) {
		// Remaining bits of AFL byte 1 must be zero
		return -3;
	}
	if (afl[1] == 0) {
		// AFL byte 2 must never be zero
		return -4;
	}
	if (afl[2] < afl[1]) {
		// AFL byte 3 must be greater than or equal to AFL byte 2
		return -5;
	}
	if (afl[3] > afl[2] - afl[1] + 1) {
		// AFL byte 4 must not exceed the number of records indicated by AFL byte 2 and byte 3
		return -6;
	}

	// Decode AFL entry
	// See EMV 4.4 Book 3, 10.2
	entry->sfi = (afl[0] & EMV_AFL_SFI_MASK) >> EMV_AFL_SFI_SHIFT;
	entry->first_record = afl[1];
	entry->last_record = afl[2];
	entry->oda_record_count = afl[3];

	// Advance iterator
	itr->ptr += 4;
	itr->len -= 4;

	// Return number of bytes consumed
	return 4;
}

int emv_cvmlist_itr_init(
	const void* cvmlist,
	size_t cvmlist_len,
	struct emv_cvmlist_amounts_t* amounts,
	struct emv_cvmlist_itr_t* itr
)
{
	const uint8_t* amount;

	if (!cvmlist || !cvmlist_len || !amounts || !itr) {
		return -1;
	}

	if ((cvmlist_len & 0x1) != 0) {
		// CVM List (field 8E) must be a multiple of 2 bytes
		// See EMV 4.4 Book 3, 10.5
		return 1;
	}

	if (cvmlist_len < 4 + 4 + 2) {
		// CVM List (field 8E) must contain two 4-byte amounts and at least
		// one 2-byte CV Rule
		// See EMV 4.4 Book 3, 10.5
		// See EMV 4.4 Book 3, Annex A
		return 2;
	}

	if ((cvmlist_len - 4 - 4) & 0x1) {
		// CVM List (field 8E) must contain two 4-byte amounts and a list of
		// 2-byte data elements
		// See EMV 4.4 Book 3, 10.5
		return 3;
	}

	if (cvmlist_len > 252) {
		// CVM List (field 8E) may be up to 252 bytes
		// See EMV 4.4 Book 3, Annex A
		return 4;
	}

	// Extract first amount as big endian
	amounts->X = 0;
	amount = cvmlist;
	for (unsigned int i = 0; i < 4; ++i) {
		amounts->X <<= 8;
		amounts->X |= amount[i];
	}

	// Extract second amount as big endian
	amounts->Y = 0;
	amount = cvmlist + 4;
	for (unsigned int i = 0; i < 4; ++i) {
		amounts->Y <<= 8;
		amounts->Y |= amount[i];
	}

	itr->ptr = cvmlist + 8;
	itr->len = cvmlist_len - 8;

	return 0;
}

int emv_cvmlist_itr_next(struct emv_cvmlist_itr_t* itr, struct emv_cv_rule_t* rule)
{
	const uint8_t* data;

	if (!itr || !rule) {
		return -1;
	}

	if (!itr->len) {
		// End of field
		return 0;
	}

	if ((itr->len & 0x1) != 0) {
		// Cardholder Verification (CV) Rule list must be multiples of 2 bytes
		return -2;
	}

	// Extract next CV Rule
	data = itr->ptr;
	rule->cvm = data[0];
	rule->cvm_cond = data[1];

	// Advance iterator
	itr->ptr += 2;
	itr->len -= 2;

	// Return number of bytes consumed
	return 2;
}

enum emv_iad_format_t emv_iad_get_format(const uint8_t* iad, size_t iad_len)
{
	if (!iad || !iad_len) {
		return EMV_IAD_FORMAT_INVALID;
	}

	// Issuer Application Data (field 9F10) for Common Core Definitions (CCD) compliant applications
	// See EMV 4.4 Book 3, Annex C9
	if (iad_len == EMV_IAD_CCD_LEN &&
		iad[0] == EMV_IAD_CCD_BYTE1 &&
		(iad[1] & EMV_IAD_CCD_CCI_FC_MASK) == EMV_IAD_CCD_CCI_FC_4_1 &&
		(
			(iad[1] & EMV_IAD_CCD_CCI_CV_MASK) == EMV_IAD_CCD_CCI_CV_4_1_TDES ||
			(iad[1] & EMV_IAD_CCD_CCI_CV_MASK) == EMV_IAD_CCD_CCI_CV_4_1_AES
		) &&
		iad[16] == EMV_IAD_CCD_BYTE17
	) {
		return EMV_IAD_FORMAT_CCD;
	}

	// Issuer Application Data (field 9F10) for Mastercard M/Chip applications
	// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Appendix B, Issuer Application Data, 9F10
	if (iad_len >= EMV_IAD_MCHIP4_LEN &&
		(iad[1] & EMV_IAD_MCHIP_CVN_MASK) == EMV_IAD_MCHIP_CVN_VERSION_MAGIC &&
		!(iad[1] & EMV_IAD_MCHIP_CVN_RFU) &&
		!(iad[1] & 0x02) // Unused bit of EMV_IAD_MCHIP_CVN_SESSION_KEY_MASK
	) {
		if (iad_len == EMV_IAD_MCHIP4_LEN) {
			return EMV_IAD_FORMAT_MCHIP4;
		}

		if (iad_len == EMV_IAD_MCHIPADV_LEN_20 ||
			iad_len == EMV_IAD_MCHIPADV_LEN_26 ||
			iad_len == EMV_IAD_MCHIPADV_LEN_28)
		{
			return EMV_IAD_FORMAT_MCHIP_ADVANCE;
		}
	}

	// Issuer Application Data (field 9F10) for Visa applications
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Appendix M
	if (iad_len >= EMV_IAD_VSDC_0_1_3_MIN_LEN &&
		iad[0] == EMV_IAD_VSDC_0_1_3_BYTE1 &&
		iad[3] == EMV_IAD_VSDC_0_1_3_CVR_LEN
	) {
		switch ((iad[2] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT) {
			case 0: return EMV_IAD_FORMAT_VSDC_0;
			case 1: return EMV_IAD_FORMAT_VSDC_1;
			case 3: return EMV_IAD_FORMAT_VSDC_3;
			default: return EMV_IAD_FORMAT_INVALID;
		}
	}
	if (iad_len == EMV_IAD_VSDC_2_4_LEN &&
		iad[0] == EMV_IAD_VSDC_2_4_BYTE1 &&
		!(iad[8] & 0xF0) // Unused nibble of Issuer Discretionary Data Option ID
	) {
		switch ((iad[1] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT) {
			case 2: return EMV_IAD_FORMAT_VSDC_2;
			case 4: return EMV_IAD_FORMAT_VSDC_4;
			default: return EMV_IAD_FORMAT_INVALID;
		}
	}

	return EMV_IAD_FORMAT_UNKNOWN;
}
