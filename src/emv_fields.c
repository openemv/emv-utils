/**
 * @file emv_fields.c
 * @brief EMV field helper functions
 *
 * Copyright (c) 2021-2024 Leon Lynch
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Visa
static const uint8_t visa_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x03 };
static const uint8_t visa_credit_debit_pix[] = { 0x10, 0x10 };
static const uint8_t visa_electron_pix[] = { 0x20, 0x10 };
static const uint8_t visa_vpay_pix[] = { 0x20, 0x20 };
static const uint8_t visa_plus_pix[] = { 0x80, 0x10 };
static const uint8_t visa_usa_debit_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x98 };

// Mastercard
static const uint8_t mastercard_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x04 };
static const uint8_t mastercard_credit_debit_pix[] = { 0x10, 0x10 };
static const uint8_t mastercard_maestro_pix[] = { 0x30, 0x60 };
static const uint8_t mastercard_cirrus_pix[] = { 0x60, 0x00 };
static const uint8_t mastercard_usa_debit_pix[] = { 0x22, 0x03 };
static const uint8_t mastercard_maestro_uk_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x05 };
static const uint8_t mastercard_test_aid[] = { 0xB0, 0x12, 0x34, 0x56, 0x78 };

// American Express
static const uint8_t amex_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x25 };
static const uint8_t amex_china_credit_debit_aid[] = { 0xA0, 0x00, 0x00, 0x07, 0x90 };

// Discover
static const uint8_t discover_aid[] = { 0xA0, 0x00, 0x00, 0x01, 0x52 };
static const uint8_t discover_card_pix[] = { 0x30, 0x10 };
static const uint8_t discover_usa_debit_pix[] = { 0x40, 0x10 };
static const uint8_t discover_zip_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x24 };

// Cartes Bancaires (CB)
static const uint8_t cb_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x42 };
static const uint8_t cb_credit_debit_pix[] = { 0x10, 0x10 };
static const uint8_t cb_debit_pix[] = { 0x20, 0x10 };

// JCB
static const uint8_t jcb_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x65 };

// Dankort
static const uint8_t dankort_aid[] = { 0xA0, 0x00, 0x00, 0x01, 0x21 };
static const uint8_t dankort_visadankort_pix[] = { 0x47, 0x11 };
static const uint8_t dankort_jspeedy_pix[] = { 0x47, 0x12 };

// UnionPay
static const uint8_t unionpay_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x33 };
static const uint8_t unionpay_debit_pix[] = { 0x01, 0x01, 0x01 };
static const uint8_t unionpay_credit_pix[] = { 0x01, 0x01, 0x02 };
static const uint8_t unionpay_quasi_credit_pix[] = { 0x01, 0x01, 0x03 };
static const uint8_t unionpay_electronic_cash_pix[] = { 0x01, 0x01, 0x06 };
static const uint8_t unionpay_usa_debit_pix[] = { 0x01, 0x01, 0x08 };

// GIM-UEMOA
static const uint8_t gimuemoa_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x37 };
static const uint8_t gimuemoa_standard_pix[] = { 0x10, 0x10, 0x00 };
static const uint8_t gimuemoa_prepaid_online_pix[] = { 0x10, 0x10, 0x01 };
static const uint8_t gimuemoa_classic_pix[] = { 0x10, 0x20, 0x00 };
static const uint8_t gimuemoa_prepaid_offline_pix[] = { 0x10, 0x20, 0x01 };
static const uint8_t gimuemoa_retrait_pix[] = { 0x30, 0x10, 0x00 };
static const uint8_t gimuemoa_electronic_wallet_pix[] = { 0x60, 0x10, 0x01 };

// Deutsche Kreditwirtschaft
static const uint8_t dk_girocard_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x59, 0x10, 0x10, 0x02, 0x80, 0x01 };

// Verve
static const uint8_t verve_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x71 };

// eftpos (Australia)
static const uint8_t eftpos_aid[] = { 0xA0, 0x00, 0x00, 0x03, 0x84 };
static const uint8_t eftpos_savings_pix[] = { 0x10 };
static const uint8_t eftpos_cheque_pix[] = { 0x20 };

// RuPay
static const uint8_t rupay_aid[] = { 0xA0, 0x00, 0x00, 0x05, 0x24 };

// Mir
static const uint8_t mir_aid[] = { 0xA0, 0x00, 0x00, 0x06, 0x58 };
static const uint8_t mir_credit_pix[] = { 0x10, 0x10 };
static const uint8_t mir_debit_pix[] = { 0x20, 0x10 };

// Meeza
static const uint8_t meeza_aid[] = { 0xA0, 0x00, 0x00, 0x07, 0x32 };

#define emv_aid_match(aid, aid_len, aid_buf) \
	(aid_len >= sizeof(aid_buf) && memcmp(aid, aid_buf, sizeof(aid_buf)) == 0)

#define emv_pix_match(aid, aid_len, pix_buf) \
	(aid_len >= 5 + sizeof(pix_buf) && memcmp(aid + 5, pix_buf, sizeof(pix_buf)) == 0)

int emv_aid_get_info(
	const uint8_t* aid,
	size_t aid_len,
	struct emv_aid_info_t* info
)
{
	if (!aid || !aid_len || !info) {
		return -1;
	}
	memset(info, 0, sizeof(*info));

	if (aid_len < 5 || aid_len > 16) {
		// Application Identifier (AID) must be 5 to 16 bytes
		return 1;
	}

	// Visa
	if (emv_aid_match(aid, aid_len, visa_aid)) {
		info->scheme = EMV_CARD_SCHEME_VISA;

		if (emv_pix_match(aid, aid_len, visa_credit_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_VISA_CREDIT_DEBIT;
		} else if (emv_pix_match(aid, aid_len, visa_electron_pix)) {
			info->product = EMV_CARD_PRODUCT_VISA_ELECTRON;
		} else if (emv_pix_match(aid, aid_len, visa_vpay_pix)) {
			info->product = EMV_CARD_PRODUCT_VISA_VPAY;
		} else if (emv_pix_match(aid, aid_len, visa_plus_pix)) {
			info->product = EMV_CARD_PRODUCT_VISA_PLUS;
		}
		return 0;

	} else if (emv_aid_match(aid, aid_len, visa_usa_debit_aid)) {
		info->scheme = EMV_CARD_SCHEME_VISA;
		info->product = EMV_CARD_PRODUCT_VISA_USA_DEBIT;
		return 0;
	}

	// Mastercard
	if (emv_aid_match(aid, aid_len, mastercard_aid)) {
		info->scheme = EMV_CARD_SCHEME_MASTERCARD;

		if (emv_pix_match(aid, aid_len, mastercard_credit_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_MASTERCARD_CREDIT_DEBIT;
		} else if (emv_pix_match(aid, aid_len, mastercard_maestro_pix)) {
			info->product = EMV_CARD_PRODUCT_MASTERCARD_MAESTRO;
		} else if (emv_pix_match(aid, aid_len, mastercard_cirrus_pix)) {
			info->product = EMV_CARD_PRODUCT_MASTERCARD_CIRRUS;
		} else if (emv_pix_match(aid, aid_len, mastercard_usa_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_MASTERCARD_USA_DEBIT;
		}
		return 0;

	} else if (emv_aid_match(aid, aid_len, mastercard_maestro_uk_aid)) {
		info->scheme = EMV_CARD_SCHEME_MASTERCARD;
		info->product = EMV_CARD_PRODUCT_MASTERCARD_MAESTRO_UK;
		return 0;

	} else if (emv_aid_match(aid, aid_len, mastercard_test_aid)) {
		info->scheme = EMV_CARD_SCHEME_MASTERCARD;
		info->product = EMV_CARD_PRODUCT_MASTERCARD_TEST;
		return 0;
	}

	// American Express
	if (emv_aid_match(aid, aid_len, amex_aid)) {
		info->scheme = EMV_CARD_SCHEME_AMEX;
		info->product = EMV_CARD_PRODUCT_AMEX_CREDIT_DEBIT;
		return 0;

	} else if (emv_aid_match(aid, aid_len, amex_china_credit_debit_aid)) {
		info->scheme = EMV_CARD_SCHEME_AMEX;
		info->product = EMV_CARD_PRODUCT_AMEX_CHINA_CREDIT_DEBIT;
		return 0;
	}

	// Discover
	if (emv_aid_match(aid, aid_len, discover_aid)) {
		info->scheme = EMV_CARD_SCHEME_DISCOVER;

		if (emv_pix_match(aid, aid_len, discover_card_pix)) {
			info->product = EMV_CARD_PRODUCT_DISCOVER_CARD;
		} else if (emv_pix_match(aid, aid_len, discover_usa_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_DISCOVER_USA_DEBIT;
		}
		return 0;

	} else if (emv_aid_match(aid, aid_len, discover_zip_aid)) {
		info->scheme = EMV_CARD_SCHEME_DISCOVER;
		info->product = EMV_CARD_PRODUCT_DISCOVER_ZIP;
		return 0;
	}

	// Cartes Bancaires (CB)
	if (emv_aid_match(aid, aid_len, cb_aid)) {
		info->scheme = EMV_CARD_SCHEME_CB;

		if (emv_pix_match(aid, aid_len, cb_credit_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_CB_CREDIT_DEBIT;
		} else if (emv_pix_match(aid, aid_len, cb_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_CB_DEBIT;
		}
		return 0;
	}

	// JCB
	if (emv_aid_match(aid, aid_len, jcb_aid)) {
		info->scheme = EMV_CARD_SCHEME_JCB;
	}

	// Dankort
	if (emv_aid_match(aid, aid_len, dankort_aid)) {
		info->scheme = EMV_CARD_SCHEME_DANKORT;

		if (emv_pix_match(aid, aid_len, dankort_visadankort_pix)) {
			info->product = EMV_CARD_PRODUCT_DANKORT_VISADANKORT;
		} else if (emv_pix_match(aid, aid_len, dankort_jspeedy_pix)) {
			info->product = EMV_CARD_PRODUCT_DANKORT_JSPEEDY;
		}
		return 0;
	}

	// UnionPay
	if (emv_aid_match(aid, aid_len, unionpay_aid)) {
		info->scheme = EMV_CARD_SCHEME_UNIONPAY;

		if (emv_pix_match(aid, aid_len, unionpay_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_UNIONPAY_DEBIT;
		} else if (emv_pix_match(aid, aid_len, unionpay_credit_pix)) {
			info->product = EMV_CARD_PRODUCT_UNIONPAY_CREDIT;
		} else if (emv_pix_match(aid, aid_len, unionpay_quasi_credit_pix)) {
			info->product = EMV_CARD_PRODUCT_UNIONPAY_QUASI_CREDIT;
		} else if (emv_pix_match(aid, aid_len, unionpay_electronic_cash_pix)) {
			info->product = EMV_CARD_PRODUCT_UNIONPAY_ELECTRONIC_CASH;
		} else if (emv_pix_match(aid, aid_len, unionpay_usa_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_UNIONPAY_USA_DEBIT;
		}
		return 0;
	}

	// GIM-UEMOA
	if (emv_aid_match(aid, aid_len, gimuemoa_aid)) {
		info->scheme = EMV_CARD_SCHEME_GIMUEMOA;

		if (emv_pix_match(aid, aid_len, gimuemoa_standard_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_STANDARD;
		} else if (emv_pix_match(aid, aid_len, gimuemoa_prepaid_online_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_ONLINE;
		} else if (emv_pix_match(aid, aid_len, gimuemoa_classic_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_CLASSIC;
		} else if (emv_pix_match(aid, aid_len, gimuemoa_prepaid_offline_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_OFFLINE;
		} else if (emv_pix_match(aid, aid_len, gimuemoa_retrait_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_RETRAIT;
		} else if (emv_pix_match(aid, aid_len, gimuemoa_electronic_wallet_pix)) {
			info->product = EMV_CARD_PRODUCT_GIMUEMOA_ELECTRONIC_WALLET;
		}
		return 0;
	}

	// Deutsche Kreditwirtschaft
	if (emv_aid_match(aid, aid_len, dk_girocard_aid)) {
		info->scheme = EMV_CARD_SCHEME_DK;
		info->product = EMV_CARD_PRODUCT_DK_GIROCARD;
		return 0;
	}

	// Verve
	if (emv_aid_match(aid, aid_len, verve_aid)) {
		info->scheme = EMV_CARD_SCHEME_VERVE;
		// Individual Verve card products are not reflected here
		return 0;
	}

	// eftpos (Australia)
	if (emv_aid_match(aid, aid_len, eftpos_aid)) {
		info->scheme = EMV_CARD_SCHEME_EFTPOS;

		if (emv_pix_match(aid, aid_len, eftpos_savings_pix)) {
			info->product = EMV_CARD_PRODUCT_EFTPOS_SAVINGS;
		} else if (emv_pix_match(aid, aid_len, eftpos_cheque_pix)) {
			info->product = EMV_CARD_PRODUCT_EFTPOS_CHEQUE;
		}
		return 0;
	}

	// RuPay
	if (emv_aid_match(aid, aid_len, rupay_aid)) {
		info->scheme = EMV_CARD_SCHEME_RUPAY;
		// Individual RuPay card products are not reflected here
		return 0;
	}

	// Mir
	if (emv_aid_match(aid, aid_len, mir_aid)) {
		info->scheme = EMV_CARD_SCHEME_MIR;

		if (emv_pix_match(aid, aid_len, mir_credit_pix)) {
			info->product = EMV_CARD_PRODUCT_MIR_CREDIT;
		} else if (emv_pix_match(aid, aid_len, mir_debit_pix)) {
			info->product = EMV_CARD_PRODUCT_MIR_DEBIT;
		}
		return 0;
	}

	// Meeza
	if (emv_aid_match(aid, aid_len, meeza_aid)) {
		info->scheme = EMV_CARD_SCHEME_MEEZA;
		// Individual Meeza card products are not reflected here
		return 0;
	}

	return -1;
}

int emv_afl_itr_init(const void* afl, size_t afl_len, struct emv_afl_itr_t* itr)
{
	if (!afl || !afl_len || !itr) {
		return -1;
	}

	if ((afl_len & 0x3) != 0) {
		// Application File Locator (field 94) must be multiples of 4 bytes
		// See EMV 4.4 Book 3, 10.2
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
	if ((afl[0] & EMV_AFL_SFI_MASK) == 0 || (afl[0] & EMV_AFL_SFI_MASK) == EMV_AFL_SFI_MASK) {
		// SFI must not be 0 or 31
		// See EMV 4.4 Book 3, 7.5
		return -4;
	}
	if (afl[1] == 0) {
		// AFL byte 2 must never be zero
		return -5;
	}
	if (afl[2] < afl[1]) {
		// AFL byte 3 must be greater than or equal to AFL byte 2
		// See EMV 4.4 Book 3, 7.5
		return -6;
	}
	if (afl[3] > afl[2] - afl[1] + 1) {
		// AFL byte 4 must not exceed the number of records indicated by AFL byte 2 and byte 3
		return -7;
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
