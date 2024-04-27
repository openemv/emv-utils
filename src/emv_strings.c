/**
 * @file emv_strings.c
 * @brief EMV string helper functions
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

#include "emv_strings.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "isocodes_lookup.h"
#include "mcc_lookup.h"
#include "iso7816_apdu.h"
#include "iso7816_strings.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h> // for vsnprintf() and snprintf()
#include <ctype.h>

#if defined(__clang__)
	// Check for Clang first because it also defines __GNUC__
	#define ATTRIBUTE_FORMAT_PRINTF(str_idx, va_idx) __attribute__((format(printf, str_idx, va_idx)))
#elif defined(__GNUC__)
	// Use gnu_printf for GCC and MSYS2's MinGW
	#define ATTRIBUTE_FORMAT_PRINTF(str_idx, va_idx) __attribute__((format(gnu_printf, str_idx, va_idx)))
#else
	// Otherwise no format string checks
	#define ATTRIBUTE_FORMAT_PRINTF(str_idx, va_idx)
#endif

struct str_itr_t {
	char* ptr;
	size_t len;
};

// Helper functions
static int emv_tlv_value_get_string(const struct emv_tlv_t* tlv, enum emv_format_t format, size_t max_format_len, char* value_str, size_t value_str_len);
static int emv_uint_to_str(uint32_t value, char* str, size_t str_len);
static void emv_str_list_init(struct str_itr_t* itr, char* buf, size_t len);
static void emv_str_list_add(struct str_itr_t* itr, const char* fmt, ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);
static int emv_country_alpha2_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);
static int emv_country_alpha3_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);
static int emv_country_numeric_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);
static int emv_currency_numeric_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);
static int emv_language_alpha2_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);
static const char* emv_cvm_code_get_string(uint8_t cvm_code);
static int emv_cvm_cond_code_get_string(uint8_t cvm_cond_code, const struct emv_cvmlist_amounts_t* amounts, char* str, size_t str_len);
static int emv_iad_ccd_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr);
static int emv_iad_mchip_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr);
static int emv_iad_vsdc_0_1_3_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr);
static int emv_iad_vsdc_2_4_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr);
static const char* emv_mastercard_device_type_get_string(const char* device_type);
static const char* emv_arc_get_desc(const char* arc);
static int emv_csu_append_string_list(const uint8_t* csu, size_t csu_len, struct str_itr_t* itr);
static int emv_capdu_get_data_get_string(const uint8_t* c_apdu, size_t c_apdu_len, char* str, size_t str_len);

int emv_strings_init(const char* isocodes_path, const char* mcc_path)
{
	int r;

	r = isocodes_init(isocodes_path);
	if (r) {
		return r;
	}

	r = mcc_init(mcc_path);
	if (r) {
		return r;
	}

	return 0;
}

int emv_tlv_get_info(
	const struct emv_tlv_t* tlv,
	struct emv_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
)
{
	int r;

	if (!tlv || !info) {
		return -1;
	}

	memset(info, 0, sizeof(*info));
	if (value_str && value_str_len) {
		value_str[0] = 0; // Default to empty value string
	}

	switch (tlv->tag) {
		case EMV_TAG_42_IIN:
			info->tag_name = "Issuer Identification Number (IIN)";
			info->tag_desc =
				"The number that identifies the major industry and the card "
				"issuer and that forms the first part of the Primary Account "
				"Number (PAN)";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 6, value_str, value_str_len);

		case EMV_TAG_4F_APPLICATION_DF_NAME:
			info->tag_name = "Application Dedicated File (ADF) Name";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
			info->format = EMV_FORMAT_B;
			return emv_aid_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_50_APPLICATION_LABEL:
			info->tag_name = "Application Label";
			info->tag_desc =
				"Mnemonic associated with the AID according to ISO/IEC 7816-4";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 16, value_str, value_str_len);

		case EMV_TAG_56_TRACK1_DATA:
			info->tag_name = "Track 1 Data";
			info->tag_desc =
				"Contains the data objects of the track 1 according to "
				"ISO/IEC 7813 Structure B, excluding start sentinel, end "
				"sentinel and Longitudinal Redundancy Check (LRC)";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 76, value_str, value_str_len);

		case EMV_TAG_57_TRACK2_EQUIVALENT_DATA:
			info->tag_name = "Track 2 Equivalent Data";
			info->tag_desc =
				"Contains the data elements of track 2 according to "
				"ISO/IEC 7813, excluding start sentinel, end sentinel, and "
				"Longitudinal Redundancy Check (LRC)";
			info->format = EMV_FORMAT_B;
			return emv_track2_equivalent_data_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5A_APPLICATION_PAN:
			info->tag_name = "Application Primary Account Number (PAN)";
			info->tag_desc =
				"Valid cardholder account number";
			info->format = EMV_FORMAT_CN;
			return emv_tlv_value_get_string(tlv, info->format, 19, value_str, value_str_len);

		case EMV_TAG_61_APPLICATION_TEMPLATE:
			info->tag_name = "Application Template";
			info->tag_desc =
				"Contains one or more data objects relevant to an application "
				"directory entry according to ISO/IEC 7816-4";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_6F_FCI_TEMPLATE:
			info->tag_name = "File Control Information (FCI) Template";
			info->tag_desc =
				"Identifies the FCI template according to ISO/IEC 7816-4";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_70_DATA_TEMPLATE:
			info->tag_name = "EMV Data Template";
			info->tag_desc = "Contains EMV data";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_71_ISSUER_SCRIPT_TEMPLATE_1:
			info->tag_name = "Issuer Script Template 1";
			info->tag_desc =
				"Contains proprietary issuer data for "
				"transmission to the ICC before the second "
				"GENERATE AC command";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_72_ISSUER_SCRIPT_TEMPLATE_2:
			info->tag_name = "Issuer Script Template 2";
			info->tag_desc =
				"Contains proprietary issuer data for "
				"transmission to the ICC after the second "
				"GENERATE AC command";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_73_DIRECTORY_DISCRETIONARY_TEMPLATE:
			info->tag_name = "Directory Discretionary Template";
			info->tag_desc =
				"Issuer discretionary part of the directory according to "
				"ISO/IEC 7816-4";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2:
			info->tag_name = "Response Message Template Format 2";
			info->tag_desc =
				"Contains the data objects (with tags and lengths) returned "
				"by the ICC in response to a command";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1:
			info->tag_name = "Response Message Template Format 1";
			info->tag_desc =
				"Contains the data objects (without tags and lengths) "
				"returned by the ICC in response to a command";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_81_AMOUNT_AUTHORISED_BINARY:
			info->tag_name = "Amount, Authorised (Binary)";
			info->tag_desc =
				"Authorised amount of the transaction (excluding adjustments)";
			info->format = EMV_FORMAT_B;
			return emv_amount_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE:
			info->tag_name = "Application Interchange Profile (AIP)";
			info->tag_desc =
				"Indicates the capabilities of the card to support specific "
				"functions in the application";
			info->format = EMV_FORMAT_B;
			return emv_aip_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_83_COMMAND_TEMPLATE:
			info->tag_name = "Command Template";
			info->tag_desc =
				"Identifies the data field of a command message";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_84_DF_NAME:
			info->tag_name = "Dedicated File (DF) Name";
			info->tag_desc =
				"Identifies the name of the Dedicated File (DF) as described "
				"in ISO/IEC 7816-4";
			info->format = EMV_FORMAT_B;
			if ((tlv->length == strlen(EMV_PSE) && strncmp((const char*)tlv->value, EMV_PSE, strlen(EMV_PSE))) ||
				(tlv->length == strlen(EMV_PPSE) && strncmp((const char*)tlv->value, EMV_PPSE, strlen(EMV_PPSE)))
			) {
				if (value_str_len > tlv->length) {
					memcpy(value_str, tlv->value, tlv->length);
					value_str[tlv->length] = 0;
				}
				return 0;
			}
			return emv_aid_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_86_ISSUER_SCRIPT_COMMAND:
			info->tag_name = "Issuer Script Command";
			info->tag_desc = "Contains a command for transmission to the ICC";
			info->format = EMV_FORMAT_VAR;
			return emv_capdu_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR:
			info->tag_name = "Application Priority Indicator";
			info->tag_desc =
				"Indicates the priority of a given application or group of "
				"applications in a directory";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_88_SFI:
			info->tag_name = "Short File Identifier (SFI)";
			info->tag_desc =
				"Identifies the Application Elementary File (AEF) referenced "
				"in commands related to a given Application Definition File "
				"or Directory Definition File (DDF). It is a binary data "
				"object having a value in the range 1 - 30 and with the three "
				"high order bits set to zero.";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_89_AUTHORISATION_CODE:
			info->tag_name = "Authorisation Code";
			info->tag_desc =
				"Value generated by the authorisation authority "
				"(issuer) for an approved transaction";
			// EMV 4.4 Book 3 Annex A indicates that the format is defined
			// by the Payment System. M/Chip and VCPS both define this field
			// as format 'ans' with length 6.
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 6, value_str, value_str_len);

		case EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE:
			info->tag_name = "Authorisation Response Code";
			info->tag_desc = "Code that defines the disposition of a message";
			info->format = EMV_FORMAT_AN;
			return emv_auth_response_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_8C_CDOL1:
			info->tag_name = "Card Risk Management Data Object List 1 (CDOL1)";
			info->tag_desc =
				"List of data objects (tag and length) to be passed to the "
				"ICC in the first GENERATE AC command";
			info->format = EMV_FORMAT_DOL;
			return 0;

		case EMV_TAG_8D_CDOL2:
			info->tag_name = "Card Risk Management Data Object List 2 (CDOL2)";
			info->tag_desc =
				"List of data objects (tag and length) to be passed to the "
				"ICC in the second GENERATE AC command";
			info->format = EMV_FORMAT_DOL;
			return 0;

		case EMV_TAG_8E_CVM_LIST:
			info->tag_name = "Cardholder Verification Method (CVM) List";
			info->tag_desc =
				"Identifies a method of verification of the cardholder "
				"supported by the application";
			info->format = EMV_FORMAT_B;
			return emv_cvm_list_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX:
			info->tag_name = "Certification Authority Public Key (CAPK) Index";
			info->tag_desc =
				"Identifies the certification authority's public key in "
				"conjunction with the RID";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE:
			info->tag_name = "Issuer Public Key Certificate";
			info->tag_desc =
				"Issuer public key certified by a certification authority";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_91_ISSUER_AUTHENTICATION_DATA:
			info->tag_name = "Issuer Authentication Data";
			info->tag_desc =
				"Data sent to the ICC for online issuer authentication";
			info->format = EMV_FORMAT_B;
			return emv_issuer_auth_data_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER:
			info->tag_name = "Issuer Public Key Remainder";
			info->tag_desc =
				"Remaining digits of the Issuer Public Key Modulus";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA:
			info->tag_name = "Signed Static Application Data";
			info->tag_desc =
				"Digital signature on critical application "
				"parameters for SDA";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_94_APPLICATION_FILE_LOCATOR:
			info->tag_name = "Application File Locator (AFL)";
			info->tag_desc =
				"Indicates the location (SFI, range of records) of the "
				"Application Elementary Files (AEFs) related to a given "
				"application";
			info->format = EMV_FORMAT_VAR;
			return emv_afl_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS:
			info->tag_name = "Terminal Verification Results (TVR)";
			info->tag_desc =
				"Status of the different functions as seen from the terminal";
			info->format = EMV_FORMAT_B;
			return emv_tvr_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9A_TRANSACTION_DATE:
			info->tag_name = "Transaction Date";
			info->tag_desc =
				"Local date that the transaction was authorised";
			info->format = EMV_FORMAT_N;
			return emv_date_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION:
			info->tag_name = "Transaction Status Information (TSI)";
			info->tag_desc =
				"Indicates the functions performed in a transaction";
			info->format = EMV_FORMAT_B;
			return emv_tsi_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9C_TRANSACTION_TYPE:
			info->tag_name = "Transaction Type";
			info->tag_desc =
				"Indicates the type of financial transaction, represented by "
				"the first two digits of the ISO 8583:1987 Processing Code. "
				"The actual values to be used for the Transaction Type data "
				"element are defined by the relevant payment system.";
			info->format = EMV_FORMAT_N;
			if (!tlv->value) {
				// Cannot use tlv->value[0], even if value_str is NULL.
				// This is typically for Data Object List (DOL) entries that
				// have been packed into TLV entries for this function to use.
				return 0;
			}
			return emv_transaction_type_get_string(tlv->value[0], value_str, value_str_len);

		case EMV_TAG_9D_DDF_NAME:
			info->tag_name = "Directory Definition File (DDF) Name";
			info->tag_desc =
				"Identifies the name of a Dedicated File (DF) associated with "
				"a directory";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_A5_FCI_PROPRIETARY_TEMPLATE:
			info->tag_name = "File Control Information (FCI) Proprietary Template";
			info->tag_desc =
				"Identifies the data object proprietary to this specification "
				"in the File Control Information (FCI) template according to "
				"ISO/IEC 7816-4";
			info->format = EMV_FORMAT_VAR;
			return 0;

		case EMV_TAG_5F20_CARDHOLDER_NAME:
			info->tag_name = "Cardholder Name";
			info->tag_desc =
				"Indicates cardholder name according to ISO 7813";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 26, value_str, value_str_len);

		case EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE:
			info->tag_name = "Application Expiration Date";
			info->tag_desc =
				"Date after which application expires";
			info->format = EMV_FORMAT_N;
			return emv_date_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE:
			info->tag_name = "Application Effective Date";
			info->tag_desc =
				"Date from which the application may be used";
			info->format = EMV_FORMAT_N;
			return emv_date_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5F28_ISSUER_COUNTRY_CODE:
			info->tag_name = "Issuer Country Code";
			info->tag_desc =
				"Indicates the country of the issuer according to ISO 3166";
			info->format = EMV_FORMAT_N;
			return emv_country_numeric_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE:
			info->tag_name = "Transaction Currency Code";
			info->tag_desc =
				"Indicates the currency code of the transaction according to "
				"ISO 4217";
			info->format = EMV_FORMAT_N;
			return emv_currency_numeric_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5F2D_LANGUAGE_PREFERENCE:
			info->tag_name = "Language Preference";
			info->tag_desc =
				"1-4 languages stored in order of preference, each "
				"represented by 2 alphabetical characters according to "
				"ISO 639";
			info->format = EMV_FORMAT_AN;
			return emv_language_preference_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_5F30_SERVICE_CODE:
			info->tag_name = "Service Code";
			info->tag_desc =
				"Service code as defined in ISO/IEC 7813 for "
				"track 1 and track 2";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

		case EMV_TAG_5F34_APPLICATION_PAN_SEQUENCE_NUMBER:
			info->tag_name = "Application Primary Account Number (PAN) Sequence Number";
			info->tag_desc =
				"Identifies and differentiates cards with the same PAN";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:
			info->tag_name = "Transaction Currency Exponent";
			info->tag_desc =
				"Indicates the implied position of the decimal point from the "
				"right of the transaction amount represented according to "
				"ISO 4217";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_5F50_ISSUER_URL:
			info->tag_name = "Issuer URL";
			info->tag_desc =
				"The URL provides the location of the issuer's Library Server "
				"on the Internet";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 0, value_str, value_str_len);

		case EMV_TAG_5F53_IBAN:
			info->tag_name = "International Bank Account Number (IBAN)";
			info->tag_desc =
				"Uniquely identifies the account of a customer at a financial "
				"institution as defined in ISO 13616.";
			info->format = EMV_FORMAT_VAR;
			// EMV 4.4 Book 3 Annex A states that this field has format 'var'
			// and a length of up to 34 bytes while Wikipedia states that an
			// IBAN consists of 34 alphanumeric characters. Therefore this
			// implementation assumes that this field can be interpreted as
			// format 'an'.
			return emv_tlv_value_get_string(tlv, EMV_FORMAT_AN, 34, value_str, value_str_len);

		case EMV_TAG_5F54_BANK_IDENTIFIER_CODE:
			info->tag_name = "Bank Identifier Code (BIC)";
			info->tag_desc =
				"Uniquely identifies a bank as defined in ISO 9362.";
			info->format = EMV_FORMAT_VAR;
			return emv_tlv_value_get_string(tlv, EMV_FORMAT_AN, 11, value_str, value_str_len);

		case EMV_TAG_5F55_ISSUER_COUNTRY_CODE_ALPHA2:
			info->tag_name = "Issuer Country Code (alpha2 format)";
			info->tag_desc =
				"Indicates the country of the issuer as defined in ISO 3166 "
				"(using a 2 character alphabetic code)";
			info->format = EMV_FORMAT_A;
			// Lookup country code; fallback to format "a" string
			r = emv_country_alpha2_code_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r || (value_str && value_str_len && !value_str[0])) {
				return emv_tlv_value_get_string(tlv, info->format, 2, value_str, value_str_len);
			}
			return 0;

		case EMV_TAG_5F56_ISSUER_COUNTRY_CODE_ALPHA3:
			info->tag_name = "Issuer Country Code (alpha3 format)";
			info->tag_desc =
				"Indicates the country of the issuer as defined in ISO 3166 "
				"(using a 3 character alphabetic code)";
			info->format = EMV_FORMAT_A;
			// Lookup country code; fallback to format "a" string
			r = emv_country_alpha3_code_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r || (value_str && value_str_len && !value_str[0])) {
				return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);
			}
			return 0;

		case EMV_TAG_5F57_ACCOUNT_TYPE:
			info->tag_name = "Account Type";
			info->tag_desc =
				"Indicates the type of account selected on the "
				"terminal, coded as specified in Annex G";
			info->format = EMV_FORMAT_N;
			if (!tlv->value) {
				// Cannot use tlv->value[0], even if value_str is NULL.
				// This is typically for Data Object List (DOL) entries that
				// have been packed into TLV entries for this function to use.
				return 0;
			}
			return emv_account_type_get_string(tlv->value[0], value_str, value_str_len);

		case EMV_TAG_9F01_ACQUIRER_IDENTIFIER:
			info->tag_name = "Acquirer Identifier";
			info->tag_desc =
				"Uniquely identifies the acquirer within each payment system";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 11, value_str, value_str_len);

		case EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC:
			info->tag_name = "Amount, Authorised (Numeric)";
			info->tag_desc =
				"Authorised amount of the transaction (excluding adjustments)";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 12, value_str, value_str_len);

		case EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC:
			info->tag_name = "Amount, Other (Numeric)";
			info->tag_desc =
				"Secondary amount associated with the transaction "
				"representing a cashback amount";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 12, value_str, value_str_len);

		case EMV_TAG_9F04_AMOUNT_OTHER_BINARY:
			info->tag_name = "Amount, Other (Binary)";
			info->tag_desc =
				"Secondary amount associated with the transaction "
				"representing a cashback amount";
			info->format = EMV_FORMAT_B;
			return emv_amount_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F05_APPLICATION_DISCRETIONARY_DATA:
			info->tag_name = "Application Discretionary Data";
			info->tag_desc =
				"Issuer or payment system specified data "
				"relating to the application";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F06_AID:
			info->tag_name = "Application Identifier (AID) - terminal";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
			info->format = EMV_FORMAT_B;
			return emv_aid_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F07_APPLICATION_USAGE_CONTROL:
			info->tag_name = "Application Usage Control";
			info->tag_desc =
				"Indicates issuer's specified restrictions on the geographic "
				"usage and services allowed for the application";
			info->format = EMV_FORMAT_B;
			return emv_app_usage_control_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F08_APPLICATION_VERSION_NUMBER:
			info->tag_name = "Application Version Number";
			info->tag_desc =
				"Version number assigned by the payment system for the "
				"application";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL:
			info->tag_name = "Application Version Number - terminal";
			info->tag_desc =
				"Version number assigned by the payment system for the "
				"application";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F0A_ASRPD:
			info->tag_name = "Application Selection Registered Proprietary Data (ASRPD)";
			info->tag_desc =
				"Proprietary data allowing for proprietary processing during "
				"application selection. Proprietary data is identified using "
				"Proprietary Data Identifiers that are managed by EMVCo and "
				"their usage by the Application Selection processing is "
				"according to their intended usage, as agreed by EMVCo during "
				"registration.";
			info->format = EMV_FORMAT_B;
			return emv_asrpd_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F0B_CARDHOLDER_NAME_EXTENDED:
			info->tag_name = "Cardholder Name Extended";
			info->tag_desc =
				"Indicates the whole cardholder name when "
				"greater than 26 characters using the same "
				"coding convention as in ISO/IEC 7813";
			info->format = EMV_FORMAT_ANS;
		return emv_tlv_value_get_string(tlv, info->format, 45, value_str, value_str_len);

		case EMV_TAG_9F0C_IINE:
			info->tag_name = "Issuer Identification Number Extended (IINE)";
			info->tag_desc =
				"The number that identifies the major industry "
				"and the card issuer and that forms the first "
				"part of the Primary Account "
				"Number (PAN).\n"
				"While the first 6 digits of the IINE (tag '9F0C') "
				"and IIN (tag '42') are the same and there is no "
				"need to have both data objects on the card, "
				"cards may have both the IIN and IINE data "
				"objects present.";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

		case EMV_TAG_9F0D_ISSUER_ACTION_CODE_DEFAULT:
			info->tag_name = "Issuer Action Code (IAC) - Default";
			info->tag_desc =
				"Specifies the issuer's conditions that cause a transaction "
				"to be rejected if it might have been approved online, but "
				"the terminal is unable to process the transaction online";
			info->format = EMV_FORMAT_B;
			return emv_tvr_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F0E_ISSUER_ACTION_CODE_DENIAL:
			info->tag_name = "Issuer Action Code (IAC) - Denial";
			info->tag_desc =
				"Specifies the issuer's conditions that cause the denial of a "
				"transaction without attempt to go online";
			info->format = EMV_FORMAT_B;
			return emv_tvr_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F0F_ISSUER_ACTION_CODE_ONLINE:
			info->tag_name = "Issuer Action Code (IAC) - Online";
			info->tag_desc =
				"Specifies the issuer's conditions that cause a transaction "
				"to be transmitted online";
			info->format = EMV_FORMAT_B;
			return emv_tvr_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F10_ISSUER_APPLICATION_DATA:
			info->tag_name = "Issuer Application Data";
			info->tag_desc =
				"Contains proprietary application data for transmission to "
				"the issuer in an online transaction.";
			info->format = EMV_FORMAT_B;
			return emv_iad_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX:
			info->tag_name = "Issuer Code Table Index";
			info->tag_desc =
				"Indicates the code table according to ISO/IEC 8859 for "
				"displaying the Application Preferred Name";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_9F12_APPLICATION_PREFERRED_NAME:
			info->tag_name = "Application Preferred Name";
			info->tag_desc =
				"Preferred mnemonic associated with the AID";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 16, value_str, value_str_len);

		case EMV_TAG_9F13_LAST_ONLINE_ATC_REGISTER:
			info->tag_name =
				"Last Online Application Transaction Counter (ATC) Register";
			info->tag_desc =
				"Application Transaction Counter (ATC) "
				"value of the last transaction that went "
				"online";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT:
			info->tag_name = "Lower Consecutive Offline Limit";
			info->tag_desc =
				"Issuer-specified preference for the maximum "
				"number of consecutive offline transactions for "
				"this ICC application allowed in a terminal "
				"with online capability";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F15_MCC:
			info->tag_name = "Merchant Category Code (MCC)";
			info->tag_desc = "Classifies the type of business being done by "
				"the merchant, represented according to ISO 8583:1993 for "
				"Card Acceptor Business Code.";
			info->format = EMV_FORMAT_N;
			return emv_mcc_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F16_MERCHANT_IDENTIFIER:
			info->tag_name = "Merchant Identifier";
			info->tag_desc =
				"When concatenated with the Acquirer Identifier, uniquely "
				"identifies a given merchant";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 15, value_str, value_str_len);

		case EMV_TAG_9F17_PIN_TRY_COUNTER:
			info->tag_name =
				"Personal Identification Number (PIN) Try Counter";
			info->tag_desc = "Number of PIN tries remaining";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F18_ISSUER_SCRIPT_IDENTIFIER:
			info->tag_name = "Issuer Script Identifier";
			info->tag_desc = "Identification of the Issuer Script";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F19_TOKEN_REQUESTOR_ID:
			info->tag_name = "Token Requestor ID";
			info->tag_desc =
				"Uniquely identifies the pairing of the Token "
				"Requestor with the Token Domain, as defined "
				"in the EMV Payment Tokenisation "
				"Framework";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE:
			info->tag_name = "Terminal Country Code";
			info->tag_desc =
				"Indicates the country of the terminal, represented according "
				"to ISO 3166";
			info->format = EMV_FORMAT_N;
			return emv_country_numeric_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT:
			info->tag_name = "Terminal Floor Limit";
			info->tag_desc =
				"Indicates the floor limit in the terminal in conjunction "
				"with the AID";
			info->format = EMV_FORMAT_B;
			return emv_amount_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F1C_TERMINAL_IDENTIFICATION:
			info->tag_name = "Terminal Identification";
			info->tag_desc =
				"Designates the unique location of a terminal at a merchant";
			info->format = EMV_FORMAT_AN;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

		case EMV_TAG_9F1D_TERMINAL_RISK_MANAGEMENT_DATA:
			info->tag_name = "Terminal Risk Management Data";
			info->tag_desc =
				"Application-specific value used by the contactless card or "
				"payment device for risk management purposes. All RFU bits "
				"must be set to zero.";
			info->format = EMV_FORMAT_B;
			return emv_terminal_risk_management_data_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F1E_IFD_SERIAL_NUMBER:
			info->tag_name = "Interface Device (IFD) Serial Number";
			info->tag_desc =
				"Unique and permanent serial number assigned to the IFD by "
				"the manufacturer";
			info->format = EMV_FORMAT_AN;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

		case EMV_TAG_9F1F_TRACK1_DISCRETIONARY_DATA:
			info->tag_name = "Track 1 Discretionary Data";
			info->tag_desc =
				"Discretionary part of track 1 according to ISO/IEC 7813";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 0, value_str, value_str_len);

		case EMV_TAG_9F20_TRACK2_DISCRETIONARY_DATA:
			info->tag_name = "Track 2 Discretionary Data";
			info->tag_desc =
				"Discretionary part of track 2 according to ISO/IEC 7813";
			info->format = EMV_FORMAT_CN;
			return emv_tlv_value_get_string(tlv, info->format, 0, value_str, value_str_len);

		case EMV_TAG_9F21_TRANSACTION_TIME:
			info->tag_name = "Transaction Time";
			info->tag_desc =
				"Local time that the transaction was authorised";
			info->format = EMV_FORMAT_N;
			return emv_time_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX:
			info->tag_name = "Certification Authority Public Key (CAPK) Index";
			info->tag_desc =
				"Identifies the certification authority's public key in "
				"conjunction with the RID";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT:
			info->tag_name = "Upper Consecutive Offline Limit";
			info->tag_desc =
				"Issuer-specified preference for the maximum "
				"number of consecutive offline transactions for "
				"this ICC application allowed in a terminal "
				"without online capability";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F24_PAYMENT_ACCOUNT_REFERENCE:
			info->tag_name = "Payment Account Reference (PAR)";
			info->tag_desc =
				"A non-financial reference assigned to each "
				"unique PAN and used to link a Payment "
				"Account represented by that PAN to affiliated "
				"Payment Tokens, as defined in the EMV "
				"Tokenisation Framework. The PAR may be "
				"assigned in advance of Payment Token "
				"issuance.";
			info->format = EMV_FORMAT_AN;
			return emv_tlv_value_get_string(tlv, info->format, 29, value_str, value_str_len);

		case EMV_TAG_9F25_LAST_4_DIGITS_OF_PAN:
			info->tag_name = "Last 4 Digits of PAN";
			info->tag_desc =
				"The last four digits of the PAN, as defined in "
				"the EMV Payment Tokenisation Framework";
			info->format = EMV_FORMAT_N;
		return emv_tlv_value_get_string(tlv, info->format, 4, value_str, value_str_len);

		case EMV_TAG_9F26_APPLICATION_CRYPTOGRAM:
			info->tag_name = "Application Cryptogram";
			info->tag_desc =
				"Cryptogram returned by the ICC in response of the "
				"GENERATE AC command";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA:
			info->tag_name = "Cryptogram Information Data";
			info->tag_desc =
				"Indicates the type of cryptogram and the actions to be "
				"performed by the terminal";
			info->format = EMV_FORMAT_B;
			if (!tlv->value) {
				// Cannot use tlv->value[0], even if value_str is NULL.
				// This is typically for Data Object List (DOL) entries that
				// have been packed into TLV entries for this function to use.
				return 0;
			}
			return emv_cid_get_string_list(tlv->value[0], value_str, value_str_len);

		case EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT:
			info->tag_name = "Issuer Public Key Exponent";
			info->tag_desc =
				"Issuer public key exponent used for the verification of the "
				"Signed Static Application Data and the ICC Public Key "
				"Certificate";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F33_TERMINAL_CAPABILITIES:
			info->tag_name = "Terminal Capabilities";
			info->tag_desc =
				"Indicates the card data input, CVM, and security "
				"capabilities of the terminal";
			info->format = EMV_FORMAT_B;
			return emv_term_caps_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F34_CVM_RESULTS:
			info->tag_name = "Cardholder Verification Method (CVM) Results";
			info->tag_desc =
				"Indicates the results of the last CVM performed";
			info->format = EMV_FORMAT_B;
			return emv_cvm_results_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F35_TERMINAL_TYPE:
			info->tag_name = "Terminal Type";
			info->tag_desc =
				"Indicates the environment of the terminal, its "
				"communications capability, and its operational control";
			info->format = EMV_FORMAT_N;
			if (!tlv->value) {
				// Cannot use tlv->value[0], even if value_str is NULL.
				// This is typically for Data Object List (DOL) entries that
				// have been packed into TLV entries for this function to use.
				return 0;
			}
			return emv_term_type_get_string_list(tlv->value[0], value_str, value_str_len);

		case EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER:
			info->tag_name = "Application Transaction Counter (ATC)";
			info->tag_desc =
				"Counter maintained by the application in the ICC "
				"(incrementing the ATC is managed by the ICC)";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F37_UNPREDICTABLE_NUMBER:
			info->tag_name = "Unpredictable Number";
			info->tag_desc =
				"Value to provide variability and uniqueness to the "
				"generation of a cryptogram";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F38_PDOL:
			info->tag_name = "Processing Options Data Object List (PDOL)";
			info->tag_desc =
				"Contains a list of terminal resident data objects (tags and "
				"lengths) needed by the ICC in processing the GET PROCESSING "
				"OPTIONS command";
			info->format = EMV_FORMAT_DOL;
			return 0;

		case EMV_TAG_9F39_POS_ENTRY_MODE:
			info->tag_name = "Point-of-Service (POS) Entry Mode";
			info->tag_desc =
				"Indicates the method by which the PAN was entered, according "
				"to the first two digits of the ISO 8583:1987 POS Entry Mode";
			info->format = EMV_FORMAT_N;
			if (!tlv->value) {
				// Cannot use tlv->value[0], even if value_str is NULL.
				// This is typically for Data Object List (DOL) entries that
				// have been packed into TLV entries for this function to use.
				return 0;
			}
			return emv_pos_entry_mode_get_string(tlv->value[0], value_str, value_str_len);

		case EMV_TAG_9F3A_AMOUNT_REFERENCE_CURRENCY:
			info->tag_name = "Amount, Reference Currency";
			info->tag_desc =
				"Authorised amount expressed in the reference currency";
			info->format = EMV_FORMAT_B;
			return emv_amount_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F3B_APPLICATION_REFERENCE_CURRENCY:
			info->tag_name = "Application Reference Currency";
			info->tag_desc =
				"1-4 currency codes used between the terminal and the ICC "
				"when the Transaction Currency Code is different from the "
				"Application Currency Code; each code is 3 digits according "
				"to ISO 4217";
			info->format = EMV_FORMAT_N;
			return emv_app_reference_currency_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:
			info->tag_name = "Transaction Reference Currency";
			info->tag_desc =
				"Code defining the common currency used by the terminal in "
				"case the Transaction Currency Code is different from the "
				"Application Currency Code";
			info->format = EMV_FORMAT_N;
			return emv_currency_numeric_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT:
			info->tag_name = "Transaction Reference Currency Exponent";
			info->tag_desc =
				"Indicates the implied position of the decimal point from the "
				"right of the transaction amount, with the Transaction "
				"Reference Currency Code represented according to ISO 4217";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
			info->tag_name = "Additional Terminal Capabilities";
			info->tag_desc =
				"Indicates the data input and output capabilities of the "
				"terminal";
			info->format = EMV_FORMAT_B;
			return emv_addl_term_caps_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER:
			info->tag_name = "Transaction Sequence Counter";
			info->tag_desc =
				"Counter maintained by the terminal that is incremented by "
				"one for each transaction";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

		case EMV_TAG_9F42_APPLICATION_CURRENCY_CODE:
			info->tag_name = "Application Currency Code";
			info->tag_desc =
				"Indicates the currency in which the account is managed "
				"according to ISO 4217";
			info->format = EMV_FORMAT_N;
			return emv_currency_numeric_code_get_string(tlv->value, tlv->length, value_str, value_str_len);

		case EMV_TAG_9F43_APPLICATION_REFERENCE_CURRENCY_EXPONENT:
			info->tag_name = "Application Reference Currency Exponent";
			info->tag_desc =
				"Indicates the implied position of the decimal point from the "
				"right of the amount, for each of the 1-4 reference "
				"currencies represented according to ISO 4217";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_9F44_APPLICATION_CURRENCY_EXPONENT:
			info->tag_name = "Application Currency Exponent";
			info->tag_desc =
				"Indicates the implied position of the decimal point from the "
				"right of the amount represented according to ISO 4217";
			info->format = EMV_FORMAT_N;
			return 0;

		case EMV_TAG_9F45_DATA_AUTHENTICATION_CODE:
			info->tag_name = "Data Authentication Code";
			info->tag_desc =
				"An issuer assigned value that is retained by the terminal "
				"during the verification process of the Signed Static "
				"Application Data";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE:
			info->tag_name = "Integrated Circuit Card (ICC) Public Key Certificate";
			info->tag_desc =
				"ICC Public Key certified by the issuer";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT:
			info->tag_name = "Integrated Circuit Card (ICC) Public Key Exponent";
			info->tag_desc =
				"ICC Public Key Exponent used for the verification of the "
				"Signed Dynamic Application Data";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER:
			info->tag_name = "Integrated Circuit Card (ICC) Public Key Remainder";
			info->tag_desc =
				"Remaining digits of the ICC Public Key Modulus";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F49_DDOL:
			info->tag_name = "Dynamic Data Authentication Data Object List (DDOL)";
			info->tag_desc =
				"List of data objects (tag and length) to be passed to the "
				"ICC in the INTERNAL AUTHENTICATE command";
			info->format = EMV_FORMAT_DOL;
			return 0;

		case EMV_TAG_9F4A_SDA_TAG_LIST:
			info->tag_name = "Static Data Authentication (SDA) Tag List";
			info->tag_desc =
				"List of tags of primitive data objects defined in this "
				"specification whose value fields are to be included in the "
				"Signed Static or Dynamic Application Data";
			info->format = EMV_FORMAT_TAG_LIST;
			return 0;

		case EMV_TAG_9F4C_ICC_DYNAMIC_NUMBER:
			info->tag_name = "Integrated Circuit Card (ICC) Dynamic Number";
			info->tag_desc =
				"Time-variant number generated by the ICC, to be captured by "
				"the terminal";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F4D_LOG_ENTRY:
			info->tag_name = "Log Entry";
			info->tag_desc =
				"Provides the SFI of the Transaction Log file and its number "
				"of records";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F4E_MERCHANT_NAME_AND_LOCATION:
			info->tag_name = "Merchant Name and Location";
			info->tag_desc =
				"Indicates the name and location of the merchant";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 0, value_str, value_str_len);

		case EMV_TAG_9F66_TTQ:
			if (tlv->length == 4) {
				// Entry Point kernel as well as kernel 3, 6 and 7 define 9F66
				// as TTQ with a length of 4 bytes
				info->tag_name = "Terminal Transaction Qualifiers (TTQ)";
				info->tag_desc =
					"Indicates the requirements for online and CVM processing "
					"as a result of Entry Point processing. The scope of this "
					"tag is limited to Entry Point. Kernels may use this tag "
					"for different purposes.";
				info->format = EMV_FORMAT_B;
				return emv_ttq_get_string_list(tlv->value, tlv->length, value_str, value_str_len);
			}

			// Same as default case
			info->format = EMV_FORMAT_B;
			return 1;

		case EMV_TAG_9F6C_CTQ:
			info->tag_name = "Card Transaction Qualifiers (CTQ)";
			info->tag_desc =
				"Used to indicate to the device the card CVM requirements, "
				"issuer preferences, and card capabilities.";
			info->format = EMV_FORMAT_B;
			return emv_ctq_get_string_list(tlv->value, tlv->length, value_str, value_str_len);

		case AMEX_TAG_9F6D_CONTACTLESS_READER_CAPABILITIES:
			if (tlv->length == 1) {
				// Kernel 4 defines 9F6D as Contactless Reader Capabilities
				// with a length of 1 byte
				info->tag_name = "Contactless Reader Capabilities";
				info->tag_desc =
					"A proprietary data element with bits 8, 7, and 4 only "
					"used to indicate a terminal's capability to support "
					"Kernel 4 mag-stripe or EMV contactless. This data "
					"element is OR'd with Terminal Type, Tag '9F35', "
					"resulting in a modified Tag '9F35', which is passed to "
					"the card when requested.";
				info->format = EMV_FORMAT_B;
				if (!tlv->value) {
					// Cannot use tlv->value[0], even if value_str is NULL.
					// This is typically for Data Object List (DOL) entries that
					// have been packed into TLV entries for this function to use.
					return 0;
				}
				return emv_amex_cl_reader_caps_get_string(tlv->value[0], value_str, value_str_len);
			}

			// Same as default case
			info->format = EMV_FORMAT_B;
			return 1;

		case 0x9F6E: // Used for different purposes by different kernels
			if (tlv->tag == MASTERCARD_TAG_9F6E_THIRD_PARTY_DATA && // Helps IDE find this case statement
				tlv->length > 4 && tlv->length <= 32
			) {
				// Kernel 2 defines 9F6E as Third Party Data with a length of
				// 5 to 32 bytes
				info->tag_name = "Third Party Data";
				info->tag_desc =
					"The Third Party data object may be used to carry "
					"specific product information to be optionally used by "
					"the terminal in processing transactions.";
				info->format = EMV_FORMAT_B;
				return emv_mastercard_third_party_data_get_string_list(tlv->value, tlv->length, value_str, value_str_len);
			}

			if (tlv->tag == VISA_TAG_9F6E_FORM_FACTOR_INDICATOR && // Helps IDE find this case statement
				tlv->length == 4 &&
				tlv->value &&
				(tlv->value[0] & VISA_FFI_VERSION_MASK) == VISA_FFI_VERSION_NUMBER_1 && // VCPS only defines version number 1
				!tlv->value[2] && // VCPS indicates that byte 3 is RFU and should be zero'd
				tlv->value[3] == VISA_FFI_PAYMENT_TXN_TECHNOLOGY_CONTACTLESS // VCPS only defines contactless
			) {
				// Kernel 3 defines 9F6E as Form Factor Indicator (FFI) with a
				// length of 4 bytes and currently only FFI version number 1 is
				// defined by VCPS.
				info->tag_name = "Form Factor Indicator (FFI)";
				info->tag_desc =
					"Indicates the form factor of the consumer payment device "
					"and thetype of contactless interface over which the "
					"transaction was conducted. This information is made "
					"available to the issuer host.";
				info->format = EMV_FORMAT_B;
				return emv_visa_form_factor_indicator_get_string_list(tlv->value, tlv->length, value_str, value_str_len);
			}

			if (tlv->tag == AMEX_TAG_9F6E_ENHANCED_CONTACTLESS_READER_CAPABILITIES && // Helps IDE find this case statement
				tlv->length == 4 &&
				tlv->value &&
				// Mandatory according to specification
				(tlv->value[0] & AMEX_ENH_CL_READER_CAPS_FULL_ONLINE_MODE_SUPPORTED) == 0 &&
				tlv->value[0] & AMEX_ENH_CL_READER_CAPS_PARTIAL_ONLINE_MODE_SUPPORTED &&
				tlv->value[0] & AMEX_ENH_CL_READER_CAPS_MOBILE_SUPPORTED &&
				(tlv->value[0] & AMEX_ENH_CL_READER_CAPS_BYTE1_RFU) == 0 &&
				tlv->value[1] & AMEX_ENH_CL_READER_CAPS_MOBILE_CVM_SUPPORTED &&
				(tlv->value[1] & AMEX_ENH_CL_READER_CAPS_BYTE2_RFU) == 0 &&
				(tlv->value[2] & AMEX_ENH_CL_READER_CAPS_BYTE3_RFU) == 0 &&
				(tlv->value[3] & AMEX_ENH_CL_READER_CAPS_BYTE4_RFU) == 0 &&
				tlv->value[3] & AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_MASK
			) {
				// Kernel 4 defines 9F6E as Enhanced Contactless Reader
				// Capabilities with a length of 4 bytes and various mandatory
				// bits
				info->tag_name = "Enhanced Contactless Reader Capabilities";
				info->tag_desc =
					"Proprietary Data Element for managing Contactless "
					"transactions and includes Contactless terminal "
					"capabilities (static) and contactless Mobile transaction "
					"(dynamic data) around CVM";
				info->format = EMV_FORMAT_B;
				return emv_amex_enh_cl_reader_caps_get_string_list(tlv->value, tlv->length, value_str, value_str_len);
			}

			// Same as default case
			info->format = EMV_FORMAT_B;
			return 1;

		case EMV_TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA:
			info->tag_name = "File Control Information (FCI) Issuer Discretionary Data";
			info->tag_desc =
				"Issuer discretionary part of the File Control Information (FCI)";
			info->format = EMV_FORMAT_VAR;
			return 0;

		default:
			info->format = EMV_FORMAT_B;
			return 1;
	}
}

/**
 * Internal function to stringify EMV values according to their format
 *
 * @param tlv Decoded EMV TLV structure
 * @param format EMV field format. See @ref emv_format_t
 * @param max_format_len Maximum number of format digits
 * @param value_str Value string buffer output. NULL to ignore.
 * @param value_str_len Length of value string buffer in bytes. Zero to ignore.
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
static int emv_tlv_value_get_string(const struct emv_tlv_t* tlv, enum emv_format_t format, size_t max_format_len, char* value_str, size_t value_str_len)
{
	int r;

	if (!tlv || !value_str || !value_str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Validate max format length
	if (max_format_len) {
		switch (format) {
			case EMV_FORMAT_A:
			case EMV_FORMAT_AN:
			case EMV_FORMAT_ANS:
				// Formats that specify a single digit per byte
				if (tlv->length > max_format_len) {
					// Value exceeds maximum format length
					return -2;
				}
				break;

			case EMV_FORMAT_CN:
			case EMV_FORMAT_N:
				// Formats that specify two digits per byte
				if (tlv->length > (max_format_len + 1) / 2) {
					// Value exceeds maximum format length
					return -3;
				}
				break;

			default:
				// Unknown format
				return -4;
		}

		if (value_str_len < max_format_len + 1) { // +1 is for NULL terminator
			// Value string output buffer is shorter than maximum format length
			return -5;
		}
	}

	switch (format) {
		case EMV_FORMAT_A:
			r = emv_format_a_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Empty string on error
				value_str[0] = 0;
				return r;
			}

			return 0;

		case EMV_FORMAT_AN:
			r = emv_format_an_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Empty string on error
				value_str[0] = 0;
				return r;
			}

			return 0;

		case EMV_FORMAT_ANS:
			if (tlv->tag == EMV_TAG_50_APPLICATION_LABEL) {
				r = emv_format_ans_only_space_get_string(tlv->value, tlv->length, value_str, value_str_len);
			}
			else if (tlv->tag == EMV_TAG_9F12_APPLICATION_PREFERRED_NAME) {
				// TODO: Convert EMV_TAG_9F12_APPLICATION_PREFERRED_NAME from the appropriate ISO/IEC 8859 code page to UTF-8
				memcpy(value_str, tlv->value, tlv->length);
				value_str[tlv->length] = 0; // NULL terminate
				return 0;
			} else {
				r = emv_format_ans_ccs_get_string(tlv->value, tlv->length, value_str, value_str_len);
			}
			if (r) {
				// Empty string on error
				value_str[0] = 0;
				return r;
			}

			return 0;

		case EMV_FORMAT_CN: {
			r = emv_format_cn_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Empty string on error
				value_str[0] = 0;
				return r;
			}

			return 0;
		}

		case EMV_FORMAT_N: {
			r = emv_format_n_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Empty string on error
				value_str[0] = 0;
				return r;
			}

			return 0;
		}

		default:
			// Unknown format
			return -6;
	}
}

int emv_format_a_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	// Minimum string length
	if (str_len < buf_len + 1) {
		return -2;
	}

	while (buf_len) {
		// Validate format "a"
		if (
			(*buf >= 0x41 && *buf <= 0x5A) || // A-Z
			(*buf >= 0x61 && *buf <= 0x7A)    // a-z
		) {
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		} else {
			// Invalid digit
			return 1;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_an_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	// Minimum string length
	if (str_len < buf_len + 1) {
		return -2;
	}

	while (buf_len) {
		// Validate format "an"
		if (
			(*buf >= 0x30 && *buf <= 0x39) || // 0-9
			(*buf >= 0x41 && *buf <= 0x5A) || // A-Z
			(*buf >= 0x61 && *buf <= 0x7A)    // a-z
		) {
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		} else {
			// Invalid digit
			return 1;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_ans_only_space_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	// Minimum string length
	if (str_len < buf_len + 1) {
		return -2;
	}

	while (buf_len) {
		// Validate format "ans" with special characters limited to space
		// character, which is effectively format "an" plus space character
		if (
			(*buf >= 0x30 && *buf <= 0x39) || // 0-9
			(*buf >= 0x41 && *buf <= 0x5A) || // A-Z
			(*buf >= 0x61 && *buf <= 0x7A) || // a-z
			(*buf == 0x20)                    // Space
		) {
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		} else {
			// Invalid digit
			return 1;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_ans_ccs_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	// Minimum string length
	if (str_len < buf_len + 1) {
		return -2;
	}

	while (buf_len) {
		// Validate format "ans" (ISO/IEC 8859 common character set)
		// See EMV 4.4 Book 4, Annex B
		if (*buf >= 0x20 && *buf <= 0x7F) {
			// ISO/IEC 8859 common character set is identical to the first
			// Unicode block and each character is encoded as a single byte
			// UTF-8 character
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		} else {
			// Invalid digit
			return 1;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_cn_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	// Minimum string length
	if (str_len < buf_len * 2 + 1) {
		return -2;
	}

	// Extract two decimal digits per byte
	for (unsigned int i = 0; i < buf_len; ++i) {
		uint8_t digit;

		// Convert most significant nibble
		digit = buf[i] >> 4;
		if (digit > 9) {
			if (digit != 0xF) {
				// Invalid digit
				return 1;
			}

			// Padding; ignore rest of buffer; NULL terminate
			str[(i * 2)] = 0;
			return 0;
		}
		str[(i * 2)] = '0' + digit;

		// Convert least significant nibble
		digit = buf[i] & 0xf;
		if (digit > 9) {
			if (digit != 0xF) {
				// Invalid digit
				return 1;
			}

			// Padding; ignore rest of buffer; NULL terminate
			str[(i * 2) + 1] = 0;
			return 0;
		}
		str[(i * 2) + 1] = '0' + digit;
	}
	str[buf_len * 2] = 0; // NULL terminate

	return 0;
}

int emv_format_n_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	int r;
	uint32_t value;

	r = emv_format_n_to_uint(buf, buf_len, &value);
	if (r) {
		return r;
	}

	return emv_uint_to_str(value, str, str_len);
}

static int emv_uint_to_str(uint32_t value, char* str, size_t str_len)
{
	uint32_t divider;
	bool found_first_digit = false;

	if (str_len < 2) {
		// Minimum length for zero string
		return -2;
	}

	// If value is zero, there's no need to do any division
	if (value == 0) {
		str[0] = '0';
		str[1] = 0;
		return 0;
	}

	divider = 1000000000; // Largest divider for a 32-bit number
	while (divider) {
		uint8_t digit;
		char c;

		digit = value / divider;
		c = '0' + digit;

		if (digit != 0 || found_first_digit) {
			*str = c;
			++str;
			--str_len;
			found_first_digit = true;
		}

		if (str_len == 0) {
			// No space left to NULL terminate
			str[0] = 0;
			return -3;
		}

		value %= divider;
		divider /= 10;
	}

	*str = 0; // NULL terminate string

	return 0;
}

int emv_str_to_format_cn(const char* str, uint8_t* buf, size_t buf_len)
{
	size_t i;

	if (!str || !buf || !buf_len) {
		return -1;
	}

	// Pack digits, left justified
	i = 0;
	while (*str && buf_len) {
		uint8_t nibble;

		// Extract decimal character
		if (*str >= '0' && *str <= '9') {
			// Convert decimal character to nibble
			nibble = *str - '0';
			++str;
		} else {
			// Invalid character
			return 1;
		}

		if ((i & 0x1) == 0) { // i is even
			// Most significant nibble
			*buf = nibble << 4;
		} else { // i is odd
			// Least significant nibble
			*buf |= nibble & 0x0F;
			++buf;
			--buf_len;
		}

		++i;
	}

	// If output buffer is not full, pad with trailing 'F's
	if (buf_len) {
		if ((i & 0x1) == 0x1) { // i is odd
			// Pad least significant nibble
			*buf |= 0x0F;
			++buf;
			--buf_len;
		}

		while (buf_len) {
			*buf = 0xFF;
			++buf;
			--buf_len;
		}
	}

	return 0;
}

int emv_str_to_format_n(const char* str, uint8_t* buf, size_t buf_len)
{
	size_t i;
	size_t str_len;

	if (!str || !buf || !buf_len) {
		return -1;
	}

	// Pack digits, right justified
	i = 0;
	str_len = strlen(str);
	str += str_len - 1; // Parse from the end of the string
	buf += buf_len - 1; // Pack from the end of the buffer
	while (str_len && buf_len) {
		uint8_t nibble;

		// Extract decimal character
		if (*str >= '0' && *str <= '9') {
			// Convert decimal character to nibble
			nibble = *str - '0';
			--str;
			--str_len;
		} else {
			// Invalid character
			return 1;
		}

		if ((i & 0x1) == 0) { // i is even
			// Least significant nibble
			*buf = nibble;
		} else { // i is odd
			// Most significant nibble
			*buf |= nibble << 4;
			--buf;
			--buf_len;
		}

		++i;
	}

	// If output buffer is not full, pad with leading zeros
	if (buf_len) {
		if ((i & 0x1) == 0x1) { // i is odd
			// i was even when loop ended; advance
			--buf;
			--buf_len;
		}

		while (buf_len) {
			*buf = 0;
			--buf;
			--buf_len;
		}
	}

	return 0;
}

int emv_amount_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	int r;
	uint32_t value;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	r = emv_format_b_to_uint(buf, buf_len, &value);
	if (r) {
		return r;
	}

	return emv_uint_to_str(value, str, str_len);
}

int emv_date_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	char date_str[7];
	size_t offset = 0;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 3) {
		// Date field must be 3 bytes
		return 1;
	}

	// Minimum length for YYYY-MM-DD
	if (str_len < 11) {
		return -2;
	}

	// Extract two decimal digits per byte
	for (unsigned int i = 0; i < buf_len; ++i) {
		uint8_t digit;

		// Convert most significant nibble
		digit = buf[i] >> 4;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 2;
		}
		date_str[i * 2] = '0' + digit;

		// Convert least significant nibble
		digit = buf[i] & 0xf;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 3;
		}
		date_str[(i * 2) + 1] = '0' + digit;
	}
	date_str[(buf_len * 2) + 1] = 0;

	// Assume it's the 21st century; if it isn't, then hopefully we've at
	// least addressed climate change...
	str[offset++] = '2';
	str[offset++] = '0';
	memcpy(str + offset, date_str, 2);
	offset += 2;

	// Separator
	str[offset++] = '-';

	// Months
	memcpy(str + offset, date_str + 2, 2);
	offset += 2;

	// Separator
	str[offset++] = '-';

	// Days
	memcpy(str + offset, date_str + 4, 2);
	offset += 2;

	// NULL terminate
	str[offset++] = 0;

	return 0;
}

int emv_time_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	char time_str[7];
	size_t offset = 0;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 3) {
		// Time field must be 3 bytes
		return 1;
	}

	// Minimum length for hh:mm:ss
	if (str_len < 9) {
		return -2;
	}

	// Extract two decimal digits per byte
	for (unsigned int i = 0; i < buf_len; ++i) {
		uint8_t digit;

		// Convert most significant nibble
		digit = buf[i] >> 4;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 2;
		}
		time_str[i * 2] = '0' + digit;

		// Convert least significant nibble
		digit = buf[i] & 0xf;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 3;
		}
		time_str[(i * 2) + 1] = '0' + digit;
	}
	time_str[(buf_len * 2) + 1] = 0;

	// Hours
	memcpy(str + offset, time_str, 2);
	offset += 2;

	// Separator
	str[offset++] = ':';

	// Minutes
	memcpy(str + offset, time_str + 2, 2);
	offset += 2;

	// Separator
	str[offset++] = ':';

	// Seconds
	memcpy(str + offset, time_str + 4, 2);
	offset += 2;

	// NULL terminate
	str[offset++] = 0;

	return 0;
}

int emv_transaction_type_get_string(
	uint8_t txn_type,
	char* str,
	size_t str_len
)
{
	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Transaction Type (field 9C)
	// See ISO 8583:1987, 4.3.8
	// See ISO 8583:1993, A.9
	// See ISO 8583:2021, D.21
	switch (txn_type) {
		case EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES:
			strncpy(str, "Goods and services", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_TRANSACTION_TYPE_CASH:
			strncpy(str, "Cash", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_TRANSACTION_TYPE_CASHBACK:
			strncpy(str, "Cashback", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_TRANSACTION_TYPE_REFUND:
			strncpy(str, "Refund", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_TRANSACTION_TYPE_INQUIRY:
			strncpy(str, "Inquiry", str_len);
			str[str_len - 1] = 0;
			return 0;

		default:
			return 1;
	}
}

int emv_mcc_get_string(
	const uint8_t* mcc,
	size_t mcc_len,
	char* str,
	size_t str_len
)
{
	int r;
	uint32_t mcc_numeric;
	const char* mcc_str;

	if (!mcc || !mcc_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (mcc_len != 2) {
		// Merchant Category Code (MCC) field must be 2 bytes
		return 1;
	}

	r = emv_format_n_to_uint(mcc, mcc_len, &mcc_numeric);
	if (r) {
		return r;
	}

	mcc_str = mcc_lookup(mcc_numeric);
	if (!mcc_str) {
		// Unknown Merchant Category Code (MCC); ignore
		str[0] = 0;
		return 0;
	}

	// Copy Merchant Category Code (MCC) string
	strncpy(str, mcc_str, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

static void emv_str_list_init(struct str_itr_t* itr, char* buf, size_t len)
{
	itr->ptr = buf;
	itr->len = len;
}

ATTRIBUTE_FORMAT_PRINTF(2, 3)
static void emv_str_list_add(struct str_itr_t* itr, const char* fmt, ...)
{
	int r;
	size_t str_len_max;
	size_t str_len;

	// Ensure that the iterator has enough space for at least one character,
	// the delimiter, and the NULL termination
	if (itr->len < 3) {
		return;
	}

	// Leave space for delimiter. This should be used for calling vsnprintf()
	// and evaluating the return value
	str_len_max = itr->len - 1;

	va_list ap;
	va_start(ap, fmt);
	r = vsnprintf(itr->ptr, str_len_max, fmt, ap);
	va_end(ap);
	if (r < 0) {
		// vsnprintf() error; NULL terminate
		*itr->ptr = 0;
		return;
	}
	if (r >= str_len_max) {
		// vsnprintf() output truncated
		// str_len_max already has space for delimiter so leave enough space
		// for NULL termination
		str_len = str_len_max - 1;
	} else {
		// vsnprintf() successful
		str_len = r;
	}

	// Delimiter
	itr->ptr[str_len] = '\n';
	++str_len;

	// Advance iterator
	itr->ptr += str_len;
	itr->len -= str_len;

	// NULL terminate string list
	*itr->ptr = 0;
}

int emv_term_type_get_string_list(
	uint8_t term_type,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!term_type || !str || !str_len) {
		return -1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Operational Control
	// See EMV 4.4 Book 4, Annex A1, table 24
	switch (term_type & EMV_TERM_TYPE_OPERATIONAL_CONTROL_MASK) {
		case EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION:
			emv_str_list_add(&itr, "Operational Control: Financial Institution");
			break;

		case EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT:
			emv_str_list_add(&itr, "Operational Control: Merchant");
			break;

		case EMV_TERM_TYPE_OPERATIONAL_CONTROL_CARDHOLDER:
			emv_str_list_add(&itr, "Operational Control: Cardholder");
			break;

		default:
			emv_str_list_add(&itr, "Operational Control: Unknown");
			break;
	}

	// Environment
	// See EMV 4.4 Book 4, Annex A1, table 24
	switch (term_type & EMV_TERM_TYPE_ENV_MASK) {
		case EMV_TERM_TYPE_ENV_ATTENDED_ONLINE_ONLY:
			emv_str_list_add(&itr, "Environment: Attended, online only");
			break;

		case EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE:
			emv_str_list_add(&itr, "Environment: Attended, offline with online capability");
			break;

		case EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_ONLY:
			emv_str_list_add(&itr, "Environment: Attended, offline only");
			break;

		case EMV_TERM_TYPE_ENV_UNATTENDED_ONLINE_ONLY:
			emv_str_list_add(&itr, "Environment: Unattended, online only");
			break;

		case EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_WITH_ONLINE:
			emv_str_list_add(&itr, "Environment: Unattended, offline with online capability");
			break;

		case EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_ONLY:
			emv_str_list_add(&itr, "Environment: Unattended, offline only");
			break;

		default:
			emv_str_list_add(&itr, "Environment: Unknown");
			break;
	}

	return 0;
}

int emv_term_caps_get_string_list(
	const uint8_t* term_caps,
	size_t term_caps_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!term_caps || !term_caps_len || !str || !str_len) {
		return -1;
	}

	if (term_caps_len != 3) {
		// Terminal Capabilities (field 9F33) must be 3 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Card Data Input Capability
	// See EMV 4.4 Book 4, Annex A2, table 25
	if (!term_caps[0]) {
		emv_str_list_add(&itr, "Card Data Input Capability: None");
	}
	if ((term_caps[0] & EMV_TERM_CAPS_INPUT_MANUAL_KEY_ENTRY)) {
		emv_str_list_add(&itr, "Card Data Input Capability: Manual key entry");
	}
	if ((term_caps[0] & EMV_TERM_CAPS_INPUT_MAGNETIC_STRIPE)) {
		emv_str_list_add(&itr, "Card Data Input Capability: Magnetic stripe");
	}
	if ((term_caps[0] & EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS)) {
		emv_str_list_add(&itr, "Card Data Input Capability: IC with contacts");
	}
	if ((term_caps[0] & EMV_TERM_CAPS_INPUT_RFU)) {
		emv_str_list_add(&itr, "Card Data Input Capability: RFU");
	}

	// CVM Capability
	// See EMV 4.4 Book 4, Annex A2, table 26
	if (!term_caps[1]) {
		emv_str_list_add(&itr, "CVM Capability: None");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE)) {
		emv_str_list_add(&itr, "CVM Capability: Plaintext PIN for ICC verification");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE)) {
		emv_str_list_add(&itr, "CVM Capability: Enciphered PIN for online verification");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_SIGNATURE)) {
		emv_str_list_add(&itr, "CVM Capability: Signature");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_RSA)) {
		emv_str_list_add(&itr, "CVM Capability: Enciphered PIN for offline verification (RSA ODE)");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_NO_CVM)) {
		emv_str_list_add(&itr, "CVM Capability: No CVM required");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_BIOMETRIC_ONLINE)) {
		emv_str_list_add(&itr, "CVM Capability: Online Biometric");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_BIOMETRIC_OFFLINE)) {
		emv_str_list_add(&itr, "CVM Capability: Offline Biometric");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_ECC)) {
		emv_str_list_add(&itr, "CVM Capability: Enciphered PIN for offline verification (ECC ODE)");
	}

	// Security Capability
	// See EMV 4.4 Book 4, Annex A2, table 27
	if (!term_caps[2]) {
		emv_str_list_add(&itr, "Security Capability: None");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_SDA)) {
		emv_str_list_add(&itr, "Security Capability: Static Data Authentication (SDA)");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_DDA)) {
		emv_str_list_add(&itr, "Security Capability: Dynamic Data Authentication (DDA)");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_CARD_CAPTURE)) {
		emv_str_list_add(&itr, "Security Capability: Card capture");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_CDA)) {
		emv_str_list_add(&itr, "Security Capability: Combined DDA/Application Cryptogram Generation (CDA)");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_XDA)) {
		emv_str_list_add(&itr, "Security Capability: Extended Data Authentication (XDA)");
	}
	if ((term_caps[2] & EMV_TERM_CAPS_SECURITY_RFU)) {
		emv_str_list_add(&itr, "Security Capability: RFU");
	}

	return 0;
}

int emv_pos_entry_mode_get_string(
	uint8_t pos_entry_mode,
	char* str,
	size_t str_len
)
{
	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Point-of-Service (POS) Entry Mode (field 9F39)
	// See ISO 8583:1987, 4.3.14
	// See ISO 8583:1993, A.8
	// See ISO 8583:2021, J.2.2.1
	switch (pos_entry_mode) {
		case EMV_POS_ENTRY_MODE_UNKNOWN:
			strncpy(str, "Unknown", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_MANUAL:
			strncpy(str, "Manual PAN entry", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_MAG:
			strncpy(str, "Magnetic stripe", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_OPTICAL_CODE:
			strncpy(str, "Optical Code", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_OCR:
			strncpy(str, "OCR", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_ICC_WITH_CVV:
			strncpy(str, "Integrated circuit card (ICC). CVV can be checked.", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_CONTACTLESS_EMV:
			strncpy(str, "Auto entry via contactless EMV", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_CARDHOLDER_ON_FILE:
			strncpy(str, "Merchant has Cardholder Credentials on File", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_MAG_FALLBACK:
			strncpy(str, "Fallback from integrated circuit card (ICC) to magnetic stripe", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_MAG_WITH_CVV:
			strncpy(str, "Magnetic stripe as read from track 2. CVV can be checked.", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_CONTACTLESS_MAG:
			strncpy(str, "Auto entry via contactless magnetic stripe", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_ICC_WITHOUT_CVV:
			strncpy(str, "Integrated circuit card (ICC). CVV may not be checked.", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_POS_ENTRY_MODE_ORIGINAL_TXN:
			strncpy(str, "Same as original transaction", str_len);
			str[str_len - 1] = 0;
			return 0;

		default:
			return 1;
	}
}

int emv_addl_term_caps_get_string_list(
	const uint8_t* addl_term_caps,
	size_t addl_term_caps_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!addl_term_caps || !addl_term_caps_len || !str || !str_len) {
		return -1;
	}

	if (addl_term_caps_len != 5) {
		// Additional Terminal Capabilities (field 9F40) must be 5 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Transaction Type Capability (byte 1)
	// See EMV 4.4 Book 4, Annex A3, table 28
	if (!addl_term_caps[0] && !addl_term_caps[1]) {
		emv_str_list_add(&itr, "Transaction Type Capability: None");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Cash");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Goods");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Services");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Cashback");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_INQUIRY)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Inquiry");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_TRANSFER)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Transfer");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_PAYMENT)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Payment");
	}
	if ((addl_term_caps[0] & EMV_ADDL_TERM_CAPS_TXN_TYPE_ADMINISTRATIVE)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Administrative");
	}

	// Transaction Type Capability (byte 2)
	// See EMV 4.4 Book 4, Annex A3, table 29
	if ((addl_term_caps[1] & EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH_DEPOSIT)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Cash deposit");
	}
	if ((addl_term_caps[1] & EMV_ADDL_TERM_CAPS_TXN_TYPE_RFU)) {
		emv_str_list_add(&itr, "Transaction Type Capability: RFU");
	}

	// Terminal Data Input Capability (byte 3)
	// See EMV 4.4 Book 4, Annex A3, table 30
	if (!addl_term_caps[2]) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: None");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_NUMERIC_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Numeric keys");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_ALPHABETIC_AND_SPECIAL_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Alphabetic and special characters keys");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_COMMAND_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Command keys");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_FUNCTION_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Function keys");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_RFU)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: RFU");
	}

	// Terminal Data Output Capability (byte 4)
	// See EMV 4.4 Book 4, Annex A3, table 31
	if (!(addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_OR_DISPLAY)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: No print, electronic or display output");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_ATTENDANT)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Print or electronic, attendant");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_CARDHOLDER)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Print or electronic, cardholder");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_ATTENDANT)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Display, attendant");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_CARDHOLDER)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Display cardholder");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_10)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 10");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_9)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 9");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_RFU)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: RFU");
	}

	// Terminal Data Output Capability (byte 5)
	// See EMV 4.4 Book 4, Annex A3, table 32
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_8)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 8");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_7)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 7");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_6)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 6");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_5)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 5");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_4)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 4");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_3)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 3");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_2)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 2");
	}
	if ((addl_term_caps[4] & EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_1)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Code table 1");
	}

	return 0;
}

int emv_aid_get_string(
	const uint8_t* aid,
	size_t aid_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct emv_aid_info_t info;
	const char* info_str;

	if (!aid || !aid_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (aid_len < 5 || aid_len > 16) {
		// Application Identifier (AID) must be 5 to 16 bytes
		return 1;
	}

	r = emv_aid_get_info(aid, aid_len, &info);
	if (r) {
		return r;
	}

	switch (info.product) {
		// Visa
		case EMV_CARD_PRODUCT_VISA_CREDIT_DEBIT: info_str = "Visa Credit/Debit"; break;
		case EMV_CARD_PRODUCT_VISA_ELECTRON: info_str = "Visa Electron"; break;
		case EMV_CARD_PRODUCT_VISA_VPAY: info_str = "V Pay"; break;
		case EMV_CARD_PRODUCT_VISA_PLUS: info_str = "Visa Plus"; break;
		case EMV_CARD_PRODUCT_VISA_USA_DEBIT: info_str = "Visa USA Debit"; break;

		// Mastercard
		case EMV_CARD_PRODUCT_MASTERCARD_CREDIT_DEBIT: info_str = "Mastercard Credit/Debit"; break;
		case EMV_CARD_PRODUCT_MASTERCARD_MAESTRO: info_str = "Maestro"; break;
		case EMV_CARD_PRODUCT_MASTERCARD_CIRRUS: info_str = "Mastercard Cirrus"; break;
		case EMV_CARD_PRODUCT_MASTERCARD_USA_DEBIT: info_str = "Mastercard USA Debit"; break;
		case EMV_CARD_PRODUCT_MASTERCARD_MAESTRO_UK: info_str = "Maestro UK"; break;
		case EMV_CARD_PRODUCT_MASTERCARD_TEST: info_str = "Mastercard Test Card"; break;

		// American Express
		case EMV_CARD_PRODUCT_AMEX_CREDIT_DEBIT: info_str = "American Express Credit/Debit"; break;
		case EMV_CARD_PRODUCT_AMEX_CHINA_CREDIT_DEBIT: info_str = "American Express (China Credit/Debit)"; break;

		// Discover
		case EMV_CARD_PRODUCT_DISCOVER_CARD: info_str = "Discover Card"; break;
		case EMV_CARD_PRODUCT_DISCOVER_USA_DEBIT: info_str = "Discover USA Debit"; break;
		case EMV_CARD_PRODUCT_DISCOVER_ZIP: info_str = "Discover ZIP"; break;

		// Cartes Bancaires (CB)
		case EMV_CARD_PRODUCT_CB_CREDIT_DEBIT: info_str = "Cartes Bancaires (CB) Credit/Debit"; break;
		case EMV_CARD_PRODUCT_CB_DEBIT: info_str = "Cartes Bancaires (CB) Debit"; break;

		// Dankort
		case EMV_CARD_PRODUCT_DANKORT_VISADANKORT: info_str = "Visa/Dankort"; break;
		case EMV_CARD_PRODUCT_DANKORT_JSPEEDY: info_str = "Dankort (J/Speedy)"; break;

		// UnionPay
		case EMV_CARD_PRODUCT_UNIONPAY_DEBIT: info_str = "UnionPay Debit"; break;
		case EMV_CARD_PRODUCT_UNIONPAY_CREDIT: info_str = "UnionPay Credit"; break;
		case EMV_CARD_PRODUCT_UNIONPAY_QUASI_CREDIT: info_str = "UnionPay Quasi-credit"; break;
		case EMV_CARD_PRODUCT_UNIONPAY_ELECTRONIC_CASH: info_str = "UnionPay Electronic Cash"; break;
		case EMV_CARD_PRODUCT_UNIONPAY_USA_DEBIT: info_str = "UnionPay USA Debit"; break;

		// GIM-UEMOA
		case EMV_CARD_PRODUCT_GIMUEMOA_STANDARD: info_str = "GIM-UEMOA Standard"; break;
		case EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_ONLINE: info_str = "GIM-UEMOA Prepaye Online"; break;
		case EMV_CARD_PRODUCT_GIMUEMOA_CLASSIC: info_str = "GIM-UEMOA Classic"; break;
		case EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_OFFLINE: info_str = "GIM-UEMOA Prepaye Possibile Offline"; break;
		case EMV_CARD_PRODUCT_GIMUEMOA_RETRAIT: info_str = "GIM-UEMOA Retrait"; break;
		case EMV_CARD_PRODUCT_GIMUEMOA_ELECTRONIC_WALLET: info_str = "GIM-UEMOA Porte Monnaie Electronique"; break;

		// Deutsche Kreditwirtschaft
		case EMV_CARD_PRODUCT_DK_GIROCARD: info_str = "Deutsche Kreditwirtschaft (DK) Girocard"; break;

		// eftpos (Australia)
		case EMV_CARD_PRODUCT_EFTPOS_SAVINGS: info_str = "eftpos (Australia) savings"; break;
		case EMV_CARD_PRODUCT_EFTPOS_CHEQUE: info_str = "eftpos (Australia) cheque"; break;

		// Mir
		case EMV_CARD_PRODUCT_MIR_CREDIT: info_str = "Mir Credit"; break;
		case EMV_CARD_PRODUCT_MIR_DEBIT: info_str = "Mir Debit"; break;

		default:
			// Unknown product; use scheme string instead
			info_str = NULL;
			break;
	}

	if (!info_str) {
		switch (info.scheme) {
			case EMV_CARD_SCHEME_VISA: info_str = "Visa"; break;
			case EMV_CARD_SCHEME_MASTERCARD: info_str = "Mastercard"; break;
			case EMV_CARD_SCHEME_AMEX: info_str = "American Express"; break;
			case EMV_CARD_SCHEME_DISCOVER: info_str = "Discover"; break;
			case EMV_CARD_SCHEME_CB: info_str = "Cartes Bancaires (CB)"; break;
			case EMV_CARD_SCHEME_JCB: info_str = "JCB"; break;
			case EMV_CARD_SCHEME_DANKORT: info_str = "Dankort"; break;
			case EMV_CARD_SCHEME_UNIONPAY: info_str = "UnionPay"; break;
			case EMV_CARD_SCHEME_GIMUEMOA: info_str = "GIM-UEMOA"; break;
			case EMV_CARD_SCHEME_DK: info_str = "Deutsche Kreditwirtschaft (DK)"; break;
			case EMV_CARD_SCHEME_VERVE: info_str = "Verve"; break;
			case EMV_CARD_SCHEME_EFTPOS: info_str = "eftpos (Australia)"; break;
			case EMV_CARD_SCHEME_RUPAY: info_str = "RuPay"; break;
			case EMV_CARD_SCHEME_MIR: info_str = "Mir"; break;
			case EMV_CARD_SCHEME_MEEZA: info_str = "Meeza"; break;

			default:
				return 1;
		}
	}

	strncpy(str, info_str, str_len - 1);
	str[str_len - 1] = 0;
	return 0;
}

int emv_asrpd_get_string_list(
	const uint8_t* asrpd,
	size_t asrpd_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;


	if (!asrpd || !asrpd_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (asrpd_len < 3) {
		// Application Selection Registered Proprietary Data (ASRPD) must
		// contain at least one ID and a length
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Application Selection Registered Proprietary Data (ASRPD)
	// See EMV 4.4 Book 1, 12.5
	// See https://www.emvco.com/registered-ids/
	while (asrpd_len) {
		uint16_t asrpd_id;
		size_t asrpd_entry_len;

		if (asrpd_len < 3) {
			// Incomplete ASRPD entry
			return 2;
		}

		asrpd_id = (asrpd[0] << 8) + asrpd[1];
		switch (asrpd_id) {
			case EMV_ASRPD_ECSG:
				emv_str_list_add(&itr, "European Cards Stakeholders Group");
				break;

			case EMV_ASRPD_TCEA:
				emv_str_list_add(&itr, "Technical Cooperation ep2 Association");
				break;

			default:
				emv_str_list_add(&itr, "Unknown ASRPD identifier");
		}

		// Validate entry length
		asrpd_entry_len = 2 + 1 + asrpd[2];
		if (asrpd_entry_len > asrpd_len) {
			// Invalid ASRPD length
			return 3;
		}

		// Advance
		asrpd += asrpd_entry_len;
		asrpd_len -= asrpd_entry_len;
	}

	return 0;
}

int emv_aip_get_string_list(
	const uint8_t* aip,
	size_t aip_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!aip || !aip_len || !str || !str_len) {
		return -1;
	}

	if (aip_len != 2) {
		// Application Interchange Profile (field 82) must be 2 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Application Interchange Profile (field 82) byte 1
	// See EMV 4.4 Book 3, Annex C1, Table 41
	// See EMV Contactless Book C-2 v2.10, Annex A.1.16
	if (aip[0] & EMV_AIP_XDA_SUPPORTED) {
		emv_str_list_add(&itr, "Extended Data Authentication (XDA) is supported");
	}
	if (aip[0] & EMV_AIP_SDA_SUPPORTED) {
		emv_str_list_add(&itr, "Static Data Authentication (SDA) is supported");
	}
	if (aip[0] & EMV_AIP_DDA_SUPPORTED) {
		emv_str_list_add(&itr, "Dynamic Data Authentication (DDA) is supported");
	}
	if (aip[0] & EMV_AIP_CV_SUPPORTED) {
		emv_str_list_add(&itr, "Cardholder verification is supported");
	}
	if (aip[0] & EMV_AIP_TERMINAL_RISK_MANAGEMENT_REQUIRED) {
		emv_str_list_add(&itr, "Terminal risk management is to be performed");
	}
	if (aip[0] & EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED) {
		emv_str_list_add(&itr, "Issuer authentication is supported");
	}
	if (aip[0] & EMV_AIP_ODCV_SUPPORTED) {
		emv_str_list_add(&itr, "On device cardholder verification is supported");
	}
	if (aip[0] & EMV_AIP_CDA_SUPPORTED) {
		emv_str_list_add(&itr, "Combined DDA/Application Cryptogram Generation (CDA) is supported");
	}

	// Application Interchange Profile (field 82) byte 2
	// See EMV Contactless Book C-2 v2.10, Annex A.1.16
	// See EMV Contactless Book C-3 v2.10, Annex A.2 (NOTE: byte 2 bit 8 is documented but no longer used by this specification)
	if (aip[1] & EMV_AIP_EMV_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless EMV mode is supported");
	}
	if (aip[1] & EMV_AIP_MOBILE_PHONE) {
		emv_str_list_add(&itr, "Mobile phone");
	}
	if (aip[1] & EMV_AIP_CONTACTLESS_TXN) {
		emv_str_list_add(&itr, "Contactless transaction");
	}
	if (aip[1] & EMV_AIP_RFU) {
		emv_str_list_add(&itr, "RFU");
	}
	if (aip[1] & EMV_AIP_RRP_SUPPORTED) {
		emv_str_list_add(&itr, "Relay Resistance Protocol (RRP) is supported");
	}

	return 0;
}

int emv_afl_get_string_list(
	const uint8_t* afl,
	size_t afl_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct emv_afl_itr_t afl_itr;
	struct emv_afl_entry_t afl_entry;
	struct str_itr_t str_itr;

	if (!afl || !afl_len || !str || !str_len) {
		return -1;
	}

	r = emv_afl_itr_init(afl, afl_len, &afl_itr);
	if (r) {
		return r;
	}

	emv_str_list_init(&str_itr, str, str_len);

	// For each Application File Locator entry, format the string according
	// to the details and add it to the string list
	while ((r = emv_afl_itr_next(&afl_itr, &afl_entry)) > 0) {

		if (afl_entry.first_record == afl_entry.last_record) {
			if (afl_entry.oda_record_count) {
				emv_str_list_add(
					&str_itr,
					"SFI %u, record %u, %u record used for offline data authentication",
					afl_entry.sfi,
					afl_entry.first_record,
					afl_entry.oda_record_count
				);
			} else {
				emv_str_list_add(
					&str_itr,
					"SFI %u, record %u",
					afl_entry.sfi,
					afl_entry.first_record
				);
			}
		} else {
			if (afl_entry.oda_record_count) {
				emv_str_list_add(
					&str_itr,
					"SFI %u, record %u to %u, %u record%s used for offline data authentication",
					afl_entry.sfi,
					afl_entry.first_record,
					afl_entry.last_record,
					afl_entry.oda_record_count,
					afl_entry.oda_record_count > 1 ? "s" : ""
				);
			} else {
				emv_str_list_add(
					&str_itr,
					 "SFI %u, record %u to %u",
					 afl_entry.sfi,
					 afl_entry.first_record,
					 afl_entry.last_record
				);
			}
		}
	}

	if (r < 0) {
		// Parsing error
		str[0] = 0;
		return -r;
	}

	return 0;
}

int emv_app_usage_control_get_string_list(
	const uint8_t* auc,
	size_t auc_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!auc || !auc_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (auc_len != 2) {
		// Application Usage Control (field 9F07) must be 2 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Application Usage Control (field 9F07) byte 1
	// See EMV 4.4 Book 3, Annex C2, Table 42
	if (auc[0] & EMV_AUC_DOMESTIC_CASH) {
		emv_str_list_add(&itr, "Valid for domestic cash transactions");
	}
	if (auc[0] & EMV_AUC_INTERNATIONAL_CASH) {
		emv_str_list_add(&itr, "Valid for international cash transactions");
	}
	if (auc[0] & EMV_AUC_DOMESTIC_GOODS) {
		emv_str_list_add(&itr, "Valid for domestic goods");
	}
	if (auc[0] & EMV_AUC_INTERNATIONAL_GOODS) {
		emv_str_list_add(&itr, "Valid for international goods");
	}
	if (auc[0] & EMV_AUC_DOMESTIC_SERVICES) {
		emv_str_list_add(&itr, "Valid for domestic services");
	}
	if (auc[0] & EMV_AUC_INTERNATIONAL_SERVICES) {
		emv_str_list_add(&itr, "Valid for international services");
	}
	if (auc[0] & EMV_AUC_ATM) {
		emv_str_list_add(&itr, "Valid at ATMs");
	}
	if (auc[0] & EMV_AUC_NON_ATM) {
		emv_str_list_add(&itr, "Valid at terminals other than ATMs");
	}

	// Application Usage Control (field 9F07) byte 2
	// See EMV 4.4 Book 3, Annex C2, Table 42
	if (auc[1] & EMV_AUC_DOMESTIC_CASHBACK) {
		emv_str_list_add(&itr, "Domestic cashback allowed");
	}
	if (auc[1] & EMV_AUC_INTERNATIONAL_CASHBACK) {
		emv_str_list_add(&itr, "International cashback allowed");
	}
	if (auc[1] & EMV_AUC_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	return 0;
}

int emv_track2_equivalent_data_get_string(
	const uint8_t* track2,
	size_t track2_len,
	char* str,
	size_t str_len
)
{
	if (!track2 || !track2_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// The easiest way to convert track2 data to a string is to simply extract
	// each nibble and add 0x30 ('0') to create the equivalent ASCII character
	// All ASCII digits from 0x30 to 0x3F are printable and it is only
	// necessary to check for the padding nibble (0xF)
	for (unsigned int i = 0; i < track2_len; ++i) {
		uint8_t digit;

		// Convert most significant nibble
		digit = track2[i] >> 4;
		if (digit == 0xF) {
			// Padding; ignore rest of buffer; NULL terminate
			str[(i * 2)] = 0;
			return 0;
		}
		str[(i * 2)] = '0' + digit;

		// Convert least significant nibble
		digit = track2[i] & 0xf;
		if (digit == 0xF) {
			// Padding; ignore rest of buffer; NULL terminate
			str[(i * 2) + 1] = 0;
			return 0;
		}
		str[(i * 2) + 1] = '0' + digit;
	}
	str[track2_len * 2] = 0; // NULL terminate

	return 0;
}

static int emv_country_alpha2_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	char country_alpha2[3];
	const char* country;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 2) {
		// Country alpha2 field must be 2 bytes
		return 1;
	}

	memcpy(country_alpha2, buf, 2);
	country_alpha2[2] = 0;

	country = isocodes_lookup_country_by_alpha2(country_alpha2);
	if (!country) {
		// Unknown country code; ignore
		str[0] = 0;
		return 0;
	}

	// Copy country string
	strncpy(str, country, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

static int emv_country_alpha3_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	char country_alpha3[4];
	const char* country;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 3) {
		// Country alpha3 field must be 3 bytes
		return 1;
	}

	memcpy(country_alpha3, buf, 3);
	country_alpha3[3] = 0;

	country = isocodes_lookup_country_by_alpha3(country_alpha3);
	if (!country) {
		// Unknown country code; ignore
		str[0] = 0;
		return 0;
	}

	// Copy country string
	strncpy(str, country, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

static int emv_country_numeric_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	int r;
	uint32_t country_code;
	const char* country;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 2) {
		// Country numeric field must be 2 bytes
		return 1;
	}

	r = emv_format_n_to_uint(buf, buf_len, &country_code);
	if (r) {
		return r;
	}

	country = isocodes_lookup_country_by_numeric(country_code);
	if (!country) {
		// Unknown country code; ignore
		str[0] = 0;
		return 0;
	}

	// Copy country string
	strncpy(str, country, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

static int emv_currency_numeric_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	int r;
	uint32_t currency_code;
	const char* currency;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 2) {
		// Numeric currency code field must be 2 bytes
		return 1;
	}

	r = emv_format_n_to_uint(buf, buf_len, &currency_code);
	if (r) {
		return r;
	}

	currency = isocodes_lookup_currency_by_numeric(currency_code);
	if (!currency) {
		// Unknown currency code; ignore
		str[0] = 0;
		return 0;
	}

	// Copy currency string
	strncpy(str, currency, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

static int emv_language_alpha2_code_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len)
{
	char language_alpha2[3];
	const char* language;

	if (!buf || !buf_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (buf_len != 2) {
		// Language alpha2 field must be 2 bytes
		return 1;
	}

	// EMVCo strongly recommends that terminals accept the data element
	// whether it is coded in upper or lower case
	// See EMV 4.4 Book 3, Annex A
	language_alpha2[0] = tolower(buf[0]);
	language_alpha2[1] = tolower(buf[1]);
	language_alpha2[2] = 0;

	language = isocodes_lookup_language_by_alpha2(language_alpha2);
	if (!language) {
		// Unknown language code; ignore
		str[0] = 0;
		return 0;
	}

	// Copy language string
	strncpy(str, language, str_len - 1);
	str[str_len - 1] = 0;

	return 0;
}

int emv_app_reference_currency_get_string_list(
	const uint8_t* arc,
	size_t arc_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct str_itr_t str_itr;

	if (!arc || !arc_len || !str || !str_len) {
		return -1;
	}

	if ((arc_len & 0x1) != 0) {
		// Application Reference Currency (field 9F3B) must be multiples of 2 bytes
		return 1;
	}

	emv_str_list_init(&str_itr, str, str_len);

	for (size_t i = 0; i < arc_len; i += 2) {
		char currency_str[128];

		r = emv_currency_numeric_code_get_string(arc + i, 2, currency_str, sizeof(currency_str));
		if (r) {
			return r;
		}

		if (currency_str[0]) {
			emv_str_list_add(&str_itr, "%s", currency_str);
		} else {
			emv_str_list_add(&str_itr, "Unknown");
		}
	}

	return 0;
}

int emv_language_preference_get_string_list(
	const uint8_t* lp,
	size_t lp_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct str_itr_t str_itr;

	if (!lp || !lp_len || !str || !str_len) {
		return -1;
	}

	if ((lp_len & 0x1) != 0) {
		// Language Preference (field 5F2D) must be multiples of 2 bytes
		return 1;
	}

	emv_str_list_init(&str_itr, str, str_len);

	for (size_t i = 0; i < lp_len; i += 2) {
		char language_str[128];

		r = emv_language_alpha2_code_get_string(lp + i, 2, language_str, sizeof(language_str));
		if (r) {
			return r;
		}

		if (language_str[0]) {
			emv_str_list_add(&str_itr, "%s", language_str);
		} else {
			emv_str_list_add(&str_itr, "Unknown");
		}
	}

	return 0;
}

int emv_account_type_get_string(
	uint8_t account_type,
	char* str,
	size_t str_len
)
{
	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Account Type (field 5F57)
	// See EMV 4.4 Book 3, Annex G, Table 56
	switch (account_type) {
		case EMV_ACCOUNT_TYPE_DEFAULT:
			strncpy(str, "Default", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_ACCOUNT_TYPE_SAVINGS:
			strncpy(str, "Savings", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_ACCOUNT_TYPE_CHEQUE_OR_DEBIT:
			strncpy(str, "Cheque/Debit", str_len);
			str[str_len - 1] = 0;
			return 0;

		case EMV_ACCOUNT_TYPE_CREDIT:
			strncpy(str, "Credit", str_len);
			str[str_len - 1] = 0;
			return 0;

		default:
			return 1;
	}
}

static const char* emv_cvm_code_get_string(uint8_t cvm_code)
{
	const char* cvm_str;

	// Cardholder Verification Rule Format byte 1 (CVM Codes)
	// See EMV 4.4 Book 3, Annex C3, Table 43
	switch (cvm_code & EMV_CV_RULE_CVM_MASK) {
		case EMV_CV_RULE_CVM_FAIL:
			cvm_str = "Fail CVM processing";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT:
			cvm_str = "Plaintext PIN verification performed by ICC";
			break;

		case EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED:
			cvm_str = "Enciphered PIN verified online";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT_AND_SIGNATURE:
			cvm_str = "Plaintext PIN verification performed by ICC and signature";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED:
			cvm_str = "Enciphered PIN verification performed by ICC";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED_AND_SIGNATURE:
			cvm_str = "Enciphered PIN verification performed by ICC and signature";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FACIAL:
			cvm_str = "Facial biometric verified offline (by ICC)";
			break;

		case EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_FACIAL:
			cvm_str = "Facial biometric verified online";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FINGER:
			cvm_str = "Finger biometric verified offline (by ICC)";
			break;

		case EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_FINGER:
			cvm_str = "Finger biometric verified online";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_PALM:
			cvm_str = "Palm biometric verified offline (by ICC)";
			break;

		case EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_PALM:
			cvm_str = "Palm biometric verified online";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_IRIS:
			cvm_str = "Iris biometric verified offline (by ICC)";
			break;

		case EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_IRIS:
			cvm_str = "Iris biometric verified online";
			break;

		case EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_VOICE:
			cvm_str = "Voice biometric verified offline (by ICC)";
			break;

		case EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_VOICE:
			cvm_str = "Voice biometric verified online";
			break;

		case EMV_CV_RULE_CVM_SIGNATURE:
			cvm_str = "Signature (paper)";
			break;

		case EMV_CV_RULE_NO_CVM:
			cvm_str = "No CVM required";
			break;

		case EMV_CV_RULE_INVALID:
			cvm_str = "Invalid CV Rule";
			break;

		default:
			cvm_str = "Unknown CVM";
			break;
	}

	return cvm_str;
}

static int emv_cvm_cond_code_get_string(
	uint8_t cvm_cond_code,
	const struct emv_cvmlist_amounts_t* amounts,
	char* str,
	size_t str_len
)
{
	int r;
	const char* cond_str;

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Cardholder Verification Rule Format byte 2 (CVM Condition Codes)
	// See EMV 4.4 Book 3, Annex C3, Table 44
	switch (cvm_cond_code) {
		case EMV_CV_RULE_COND_ALWAYS:
			cond_str = "Always";
			break;

		case EMV_CV_RULE_COND_UNATTENDED_CASH:
			cond_str = "If unattended cash";
			break;

		case EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK:
			cond_str = "If not unattended cash and not manual cash and not purchase with cashback";
			break;

		case EMV_CV_RULE_COND_CVM_SUPPORTED:
			cond_str = "If terminal supports the CVM";
			break;

		case EMV_CV_RULE_COND_MANUAL_CASH:
			cond_str = "If manual cash";
			break;

		case EMV_CV_RULE_COND_CASHBACK:
			cond_str = "If purchase with cashback";
			break;

		case EMV_CV_RULE_COND_LESS_THAN_X:
			if (amounts) {
				r = snprintf(
					str,
					str_len,
					"If transaction is in the application currency and is under %u value",
					amounts->X
				);
				if (r < 0) {
					// Unknown error
					str[0] = 0;
					return -1;
				}
				return 0;
			} else {
				cond_str = "If transaction is in the application currency and is under X value";
			}
			break;

		case EMV_CV_RULE_COND_MORE_THAN_X:
			if (amounts) {
				r = snprintf(
					str,
					str_len,
					"If transaction is in the application currency and is over %u value",
					amounts->X
				);
				if (r < 0) {
					// Unknown error
					str[0] = 0;
					return -1;
				}
				return 0;
			} else {
				cond_str = "If transaction is in the application currency and is over X value";
			}
			break;

		case EMV_CV_RULE_COND_LESS_THAN_Y:
			if (amounts) {
				r = snprintf(
					str,
					str_len,
					"If transaction is in the application currency and is under %u value",
					amounts->Y
				);
				if (r < 0) {
					// Unknown error
					str[0] = 0;
					return -1;
				}
				return 0;
			} else {
				cond_str = "If transaction is in the application currency and is under Y value";
			}
			break;

		case EMV_CV_RULE_COND_MORE_THAN_Y:
			if (amounts) {
				r = snprintf(
					str,
					str_len,
					"If transaction is in the application currency and is over %u value",
					amounts->Y
				);
				if (r < 0) {
					// Unknown error
					str[0] = 0;
					return -1;
				}
				return 0;
			} else {
				cond_str = "If transaction is in the application currency and is over Y value";
			}
			break;

		default:
			cond_str = "Unknown condition";
			break;
	}

	strncpy(str, cond_str, str_len);
	str[str_len - 1] = 0;

	return 0;
}

int emv_cvm_list_get_string_list(
	const uint8_t* cvmlist,
	size_t cvmlist_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct emv_cvmlist_amounts_t cvmlist_amounts;
	struct emv_cvmlist_itr_t cvmlist_itr;
	struct emv_cv_rule_t cv_rule;
	struct str_itr_t str_itr;

	if (!cvmlist || !cvmlist_len || !str || !str_len) {
		return -1;
	}

	r = emv_cvmlist_itr_init(cvmlist, cvmlist_len, &cvmlist_amounts, &cvmlist_itr);
	if (r) {
		return r;
	}

	emv_str_list_init(&str_itr, str, str_len);

	// For each CV Rule entry build a string
	while ((r = emv_cvmlist_itr_next(&cvmlist_itr, &cv_rule)) > 0) {
		const char* cvm_str;
		char cond_str[128];
		const char* proc_str;

		// CVM Code string
		cvm_str = emv_cvm_code_get_string(cv_rule.cvm);
		if (!cvm_str) {
			return -2;
		}

		// CVM condition string
		r = emv_cvm_cond_code_get_string(
			cv_rule.cvm_cond,
			&cvmlist_amounts,
			cond_str,
			sizeof(cond_str)
		);
		if (r) {
			return r;
		}

		// CVM processing string
		if (cv_rule.cvm & EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL) {
			proc_str = "Apply succeeding CV Rule if this CVM is unsuccessful";
		} else {
			proc_str = "Fail cardholder verification if this CVM is unsuccessful";
		}

		// Add CV Rule string to list
		emv_str_list_add(
			&str_itr,
			"%s; %s; %s",
			cond_str,
			cvm_str,
			proc_str
		);
	}

	return 0;
}

int emv_cvm_results_get_string_list(
	const uint8_t* cvmresults,
	size_t cvmresults_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct str_itr_t itr;
	const char* cvm_str;
	char cond_str[128];

	if (!cvmresults || !cvmresults_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (cvmresults_len != 3) {
		// Cardholder Verification Method (CVM) Results (field 9F34) must be 3 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Cardholder Verification Method (CVM) Results (field 9F34) byte 1
	// See EMV 4.4 Book 4, Annex A4, Table 33
	if (cvmresults[0] == EMV_CVM_NOT_PERFORMED) {
		// This value is invalid for CV Rules but valid for CVM Results
		cvm_str = "CVM not performed";
	} else {
		cvm_str = emv_cvm_code_get_string(cvmresults[0]);
		if (!cvm_str) {
			return -1;
		}
	}
	emv_str_list_add(&itr, "CVM Performed: %s", cvm_str);

	// Cardholder Verification Method (CVM) Results (field 9F34) byte 2
	// See EMV 4.4 Book 4, Annex A4, Table 33
	r = emv_cvm_cond_code_get_string(
		cvmresults[1],
		NULL,
		cond_str,
		sizeof(cond_str)
	);
	if (r) {
		return r;
	}
	emv_str_list_add(&itr, "CVM Condition: %s", cond_str);

	// Cardholder Verification Method (CVM) Results (field 9F34) byte 3
	// See EMV 4.4 Book 4, Annex A4, Table 33
	switch (cvmresults[2]) {
		case EMV_CVM_RESULT_UNKNOWN:
			emv_str_list_add(&itr, "CVM Result: Unknown");
			break;

		case EMV_CVM_RESULT_FAILED:
			emv_str_list_add(&itr, "CVM Result: Failed");
			break;

		case EMV_CVM_RESULT_SUCCESSFUL:
			emv_str_list_add(&itr, "CVM Result: Successful");
			break;

		default:
			emv_str_list_add(&itr, "CVM Result: %u", cvmresults[2]);
			break;
	}

	return 0;
}

int emv_tvr_get_string_list(
	const uint8_t* tvr,
	size_t tvr_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!tvr || !tvr_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tvr_len != 5) {
		// Terminal Verification Results (field 95) must be 5 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Terminal Verification Results (field 95) byte 1
	// See EMV 4.4 Book 3, Annex C5, Table 46
	if (tvr[0] & EMV_TVR_OFFLINE_DATA_AUTH_NOT_PERFORMED) {
		emv_str_list_add(&itr, "Offline data authentication was not performed");
	}
	if (tvr[0] & EMV_TVR_SDA_FAILED) {
		emv_str_list_add(&itr, "Static Data Authentication (SDA) failed");
	}
	if (tvr[0] & EMV_TVR_ICC_DATA_MISSING) {
		emv_str_list_add(&itr, "Integrated circuit card (ICC) data missing");
	}
	if (tvr[0] & EMV_TVR_CARD_ON_EXCEPTION_FILE) {
		emv_str_list_add(&itr, "Card appears on terminal exception file");
	}
	if (tvr[0] & EMV_TVR_DDA_FAILED) {
		emv_str_list_add(&itr, "Dynamic Data Authentication (DDA) failed");
	}
	if (tvr[0] & EMV_TVR_CDA_FAILED) {
		emv_str_list_add(&itr, "Combined DDA/Application Cryptogram Generation (CDA) failed");
	}
	if (tvr[0] & EMV_TVR_SDA_SELECTED) {
		emv_str_list_add(&itr, "Static Data Authentication (SDA) selected");
	}
	if (tvr[0] & EMV_TVR_XDA_SELECTED) {
		emv_str_list_add(&itr, "Extended Data Authentication (XDA) selected");
	}

	// Terminal Verification Results (field 95) byte 2
	// See EMV 4.4 Book 3, Annex C5, Table 46
	if (tvr[1] & EMV_TVR_APPLICATION_VERSIONS_DIFFERENT) {
		emv_str_list_add(&itr, "ICC and terminal have different application versions");
	}
	if (tvr[1] & EMV_TVR_APPLICATION_EXPIRED) {
		emv_str_list_add(&itr, "Expired application");
	}
	if (tvr[1] & EMV_TVR_APPLICATION_NOT_EFFECTIVE) {
		emv_str_list_add(&itr, "Application not yet effective");
	}
	if (tvr[1] & EMV_TVR_SERVICE_NOT_ALLOWED) {
		emv_str_list_add(&itr, "Requested service not allowed for card product");
	}
	if (tvr[1] & EMV_TVR_NEW_CARD) {
		emv_str_list_add(&itr, "New card");
	}
	if (tvr[1] & EMV_TVR_RFU) {
		emv_str_list_add(&itr, "RFU");
	}
	if (tvr[1] & EMV_TVR_BIOMETRIC_PERFORMED_SUCCESSFUL) {
		emv_str_list_add(&itr, "Biometric performed and successful");
	}
	if (tvr[1] & EMV_TVR_BIOMETRIC_TEMPLATE_FORMAT_NOT_SUPPORTED) {
		emv_str_list_add(&itr, "Biometric template format not supported");
	}

	// Terminal Verification Results (field 95) byte 3
	// See EMV 4.4 Book 3, Annex C5, Table 46
	if (tvr[2] & EMV_TVR_CV_PROCESSING_FAILED) {
		emv_str_list_add(&itr, "Cardholder verification was not successful");
	}
	if (tvr[2] & EMV_TVR_CVM_UNRECOGNISED) {
		emv_str_list_add(&itr, "Unrecognised CVM");
	}
	if (tvr[2] & EMV_TVR_PIN_TRY_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "PIN Try Limit exceeded");
	}
	if (tvr[2] & EMV_TVR_PIN_PAD_FAILED) {
		emv_str_list_add(&itr, "PIN entry required and PIN pad not present or not working");
	}
	if (tvr[2] & EMV_TVR_PIN_NOT_ENTERED) {
		emv_str_list_add(&itr, "PIN entry required, PIN pad present, but PIN was not entered");
	}
	if (tvr[2] & EMV_TVR_ONLINE_CVM_CAPTURED) {
		emv_str_list_add(&itr, "Online CVM captured");
	}
	if (tvr[2] & EMV_TVR_BIOMETRIC_CAPTURE_FAILED) {
		emv_str_list_add(&itr, "Biometric required but Biometric capture device not working");
	}
	if (tvr[2] & EMV_TVR_BIOMETRIC_SUBTYPE_BYPASSED) {
		emv_str_list_add(&itr, "Biometric required, Biometric capture device present, but Biometric Subtype entry was bypassed");
	}

	// Terminal Verification Results (field 95) byte 4
	// See EMV 4.4 Book 3, Annex C5, Table 46
	if (tvr[3] & EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "Transaction exceeds floor limit");
	}
	if (tvr[3] & EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "Lower consecutive offline limit exceeded");
	}
	if (tvr[3] & EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "Upper consecutive offline limit exceeded");
	}
	if (tvr[3] & EMV_TVR_RANDOM_SELECTED_ONLINE) {
		emv_str_list_add(&itr, "Transaction selected randomly for online processing");
	}
	if (tvr[3] & EMV_TVR_MERCHANT_FORCED_ONLINE) {
		emv_str_list_add(&itr, "Merchant forced transaction online");
	}
	if (tvr[3] & EMV_TVR_BIOMETRIC_TRY_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "Biometric Try Limit exceeded");
	}
	if (tvr[3] & EMV_TVR_BIOMETRIC_TYPE_NOT_SUPPORTED) {
		emv_str_list_add(&itr, "A selected Biometric Type not supported");
	}
	if (tvr[3] & EMV_TVR_XDA_FAILED) {
		emv_str_list_add(&itr, "XDA signature verification failed");
	}

	// Terminal Verification Results (field 95) byte 5
	// See EMV 4.4 Book 3, Annex C5, Table 46
	if (tvr[4] & EMV_TVR_DEFAULT_TDOL) {
		emv_str_list_add(&itr, "Default TDOL used");
	}
	if (tvr[4] & EMV_TVR_ISSUER_AUTHENTICATION_FAILED) {
		emv_str_list_add(&itr, "Issuer authentication failed");
	}
	if (tvr[4] & EMV_TVR_SCRIPT_PROCESSING_FAILED_BEFORE_GEN_AC) {
		emv_str_list_add(&itr, "Script processing failed before final GENERATE AC");
	}
	if (tvr[4] & EMV_TVR_SCRIPT_PROCESSING_FAILED_AFTER_GEN_AC) {
		emv_str_list_add(&itr, "Script processing failed after final GENERATE AC");
	}
	if (tvr[4] & EMV_TVR_CA_ECC_KEY_MISSING) {
		emv_str_list_add(&itr, "CA ECC key missing");
	}
	if (tvr[4] & EMV_TVR_ECC_KEY_RECOVERY_FAILED) {
		emv_str_list_add(&itr, "ECC key recovery failed");
	}
	if (tvr[4] & EMV_TVR_RESERVED_FOR_CONTACTLESS) {
		emv_str_list_add(&itr, "Reserved for use by the EMV Contactless Specifications");
	}

	return 0;
}

int emv_tsi_get_string_list(
	const uint8_t* tsi,
	size_t tsi_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!tsi || !tsi_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tsi_len != 2) {
		// Transaction Status Information (field 9B) must be 2 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Transaction Status Information (field 9B)
	// See EMV 4.4 Book 3, Annex C6, Table 47
	if (tsi[0] & EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED) {
		emv_str_list_add(&itr, "Offline data authentication was performed");
	}
	if (tsi[0] & EMV_TSI_CV_PERFORMED) {
		emv_str_list_add(&itr, "Cardholder verification was performed");
	}
	if (tsi[0] & EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED) {
		emv_str_list_add(&itr, "Card risk management was performed");
	}
	if (tsi[0] & EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED) {
		emv_str_list_add(&itr, "Issuer authentication was performed");
	}
	if (tsi[0] & EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED) {
		emv_str_list_add(&itr, "Terminal risk management was performed");
	}
	if (tsi[0] & EMV_TSI_SCRIPT_PROCESSING_PERFORMED) {
		emv_str_list_add(&itr, "Script processing was performed");
	}
	if (tsi[0] & EMV_TSI_BYTE1_RFU || tsi[1] & EMV_TSI_BYTE2_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	return 0;
}

int emv_cid_get_string_list(
	uint8_t cid,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!str || !str_len) {
		return -1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Application Cryptogram (AC) type
	// See EMV 4.4 Book 3, 6.5.5.4, table 15
	switch (cid & EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_MASK) {
		case EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_AAC:
			emv_str_list_add(&itr, "Application Cryptogram (AC) type: Application Authentication Cryptogram (AAC)");
			break;

		case EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_TC:
			emv_str_list_add(&itr, "Application Cryptogram (AC) type: Transaction Certificate (TC)");
			break;

		case EMV_CID_APPLICATION_CRYPTOGRAM_TYPE_ARQC:
			emv_str_list_add(&itr, "Application Cryptogram (AC) type: Authorisation Request Cryptogram (ARQC)");
			break;

		default:
			emv_str_list_add(&itr, "Application Cryptogram (AC) type: RFU");
			break;
	}

	// Payment System-specific cryptogram
	// See EMV 4.4 Book 3, 6.5.5.4, table 15
	if (cid & EMV_CID_PAYMENT_SYSTEM_SPECIFIC_CRYPTOGRAM_MASK) {
		emv_str_list_add(&itr,
			"Payment System-specific cryptogram: 0x%02X",
			cid & EMV_CID_PAYMENT_SYSTEM_SPECIFIC_CRYPTOGRAM_MASK
		);
	}

	// Advice required
	// See EMV 4.4 Book 3, 6.5.5.4, table 15
	if (cid & EMV_CID_ADVICE_REQUIRED) {
		emv_str_list_add(&itr, "Advice required");
	} else if (cid & EMV_CID_ADVICE_CODE_MASK) {
		// Indicate that advice is not required although it is provided
		emv_str_list_add(&itr, "No advice required");
	}

	// Reason/advice code
	// See EMV 4.4 Book 3, 6.5.5.4, table 15
	switch (cid & EMV_CID_ADVICE_CODE_MASK) {
		case EMV_CID_ADVICE_NO_INFO:
			// No information given; ignore
			break;

		case EMV_CID_ADVICE_SERVICE_NOT_ALLOWED:
			emv_str_list_add(&itr, "Advice: Service not allowed");
			break;

		case EMV_CID_ADVICE_PIN_TRY_LIMIT_EXCEEDED:
			emv_str_list_add(&itr, "Advice: PIN Try Limit exceeded");
			break;

		case EMV_CID_ADVICE_ISSUER_AUTHENTICATION_FAILED:
			emv_str_list_add(&itr, "Advice: Issuer authentication failed");
			break;

		default:
			emv_str_list_add(&itr, "Advice: RFU");
			break;
	}

	return 0;
}

static int emv_iad_ccd_append_string_list(
	const uint8_t* iad,
	size_t iad_len,
	struct str_itr_t* itr
)
{
	const uint8_t* cvr;

	// Issuer Application Data for a CCD-Compliant Application
	// See EMV 4.4 Book 3, Annex C9
	if (!iad ||
		iad_len != EMV_IAD_CCD_LEN ||
		iad[0] != EMV_IAD_CCD_BYTE1 ||
		iad[16] != EMV_IAD_CCD_BYTE17
	) {
		return -1;
	}

	// Common Core Identifier, Cryptogram Version
	// See EMV 4.4 Book 3, Annex C9.1
	switch (iad[1] & EMV_IAD_CCD_CCI_CV_MASK) {
		case EMV_IAD_CCD_CCI_CV_4_1_TDES:
			emv_str_list_add(itr, "Cryptogram Version: TDES");
			break;

		case EMV_IAD_CCD_CCI_CV_4_1_AES:
			emv_str_list_add(itr, "Cryptogram Version: AES");
			break;

		default:
			emv_str_list_add(itr, "Cryptogram Version: Unknown");
			return 1;
	}

	// Derivation Key Index
	// See EMV 4.4 Book 3, Annex C9.2
	emv_str_list_add(itr, "Derivation Key Index (DKI): %02X", iad[2]);

	// Card Verification (CVR) Results byte 1
	// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
	cvr = iad + 3;
	switch (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_2GAC_MASK) {
		case EMV_IAD_CCD_CVR_BYTE1_2GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned AAC");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_2GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned TC");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_2GAC_NOT_REQUESTED:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC Not Requested");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_2GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC RFU");
			break;
	}
	switch (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_1GAC_MASK) {
		case EMV_IAD_CCD_CVR_BYTE1_1GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned AAC");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_1GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned TC");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_1GAC_ARQC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned ARQC");
			break;

		case EMV_IAD_CCD_CVR_BYTE1_1GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC RFU");
			break;
	}
	if (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_CDA_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Performed");
	}
	if (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_OFFLINE_DDA_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline Dynamic Data Authentication (DDA) Performed");
	}
	if (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_ISSUER_AUTH_NOT_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication Not Performed");
	}
	if (cvr[0] & EMV_IAD_CCD_CVR_BYTE1_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication Failed");
	}

	// Card Verification Results (CVR) byte 2
	// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
	emv_str_list_add(itr,
		"Card Verification Results (CVR): PIN Try Counter is %u",
		(cvr[1] & EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_COUNTER_MASK) >> EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_COUNTER_SHIFT
	);
	if (cvr[1] & EMV_IAD_CCD_CVR_BYTE2_OFFLINE_PIN_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Performed");
	}
	if (cvr[1] & EMV_IAD_CCD_CVR_BYTE2_OFFLINE_PIN_NOT_SUCCESSFUL) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Performed and PIN Not Successfully Verified");
	}
	if (cvr[1] & EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PIN Try Limit Exceeded");
	}
	if (cvr[1] & EMV_IAD_CCD_CVR_BYTE2_LAST_ONLINE_TXN_NOT_COMPLETED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Last Online Transaction Not Completed");
	}

	// Card Verification Results (CVR) byte 3
	// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_L_OFFLINE_TXN_CNT_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Lower Offline Transaction Count Limit Exceeded");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_U_OFFLINE_TXN_CNT_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Upper Offline Transaction Count Limit Exceeded");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_L_OFFLINE_AMOUNT_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Lower Cumulative Offline Amount Limit Exceeded");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_U_OFFLINE_AMOUNT_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Upper Cumulative Offline Amount Limit Exceeded");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT1) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer-discretionary bit 1");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT2) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer-discretionary bit 2");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT3) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer-discretionary bit 3");
	}
	if (cvr[2] & EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT4) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer-discretionary bit 4");
	}

	// Card Verification Results (CVR) byte 4
	// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
	if (cvr[3] & EMV_IAD_CCD_CVR_BYTE4_SCRIPT_COUNT_MASK) {
		emv_str_list_add(itr,
			"Card Verification Results (CVR): %u Successfully Processed Issuer Script Commands Containing Secure Messaging",
			(cvr[3] & EMV_IAD_CCD_CVR_BYTE4_SCRIPT_COUNT_MASK) >> EMV_IAD_CCD_CVR_BYTE4_SCRIPT_COUNT_SHIFT
		);
	}
	if (cvr[3] & EMV_IAD_CCD_CVR_BYTE4_ISSUER_SCRIPT_PROCESSING_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Script Processing Failed");
	}
	if (cvr[3] & EMV_IAD_CCD_CVR_BYTE4_ODA_FAILED_ON_PREVIOUS_TXN) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline Data Authentication Failed on Previous Transaction");
	}
	if (cvr[3] & EMV_IAD_CCD_CVR_BYTE4_GO_ONLINE_ON_NEXT_TXN) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Go Online on Next Transaction Was Set");
	}
	if (cvr[3] & EMV_IAD_CCD_CVR_BYTE4_UNABLE_TO_GO_ONLINE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Unable to go Online");
	}

	// Card Verification Results (CVR) byte 5
	// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
	if (cvr[4]) {
		emv_str_list_add(itr, "Card Verification Results (CVR): RFU");
	}

	return 0;
}

static int emv_iad_mchip_append_string_list(
	const uint8_t* iad,
	size_t iad_len,
	struct str_itr_t* itr
)
{
	const uint8_t* cvr;

	// Issuer Application Data for M/Chip 4 and M/Chip Advance
	// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Appendix B, Issuer Application Data, 9F10
	if (!iad ||
		(
			iad_len != EMV_IAD_MCHIP4_LEN &&
			iad_len != EMV_IAD_MCHIPADV_LEN_20 &&
			iad_len != EMV_IAD_MCHIPADV_LEN_26 &&
			iad_len != EMV_IAD_MCHIPADV_LEN_28
		) ||
		(iad[1] & EMV_IAD_MCHIP_CVN_MASK) != EMV_IAD_MCHIP_CVN_VERSION_MAGIC ||
		!!(iad[1] & EMV_IAD_MCHIP_CVN_RFU) ||
		!!(iad[1] & 0x02) // Unused bit of EMV_IAD_MCHIP_CVN_SESSION_KEY_MASK
	) {
		return -1;
	}

	// Derivation Key Index
	emv_str_list_add(itr, "Derivation Key Index (DKI): %02X", iad[0]);

	// Cryptogram Version Number
	emv_str_list_add(itr, "Cryptogram Version Number (CVN): %02X", iad[1]);
	switch (iad[1] & EMV_IAD_MCHIP_CVN_SESSION_KEY_MASK) {
		case EMV_IAD_MCHIP_CVN_SESSION_KEY_MASTERCARD_SKD:
			emv_str_list_add(itr, "Cryptogram: Mastercard Proprietary SKD session key");
			break;

		case EMV_IAD_MCHIP_CVN_SESSION_KEY_EMV_CSK:
			emv_str_list_add(itr, "Cryptogram: EMV CSK session key");
			break;

		default:
			emv_str_list_add(itr, "Cryptogram: Unknown session key");
			return 1;
	}
	if (iad[1] & EMV_IAD_MCHIP_CVN_COUNTERS_INCLUDED) {
		emv_str_list_add(itr, "Cryptogram: Counter included in AC data");
	} else {
		emv_str_list_add(itr, "Cryptogram: Counters not included in AC data");
	}

	// Card Verification (CVR) Results byte 1
	cvr = iad + 2;
	switch (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_2GAC_MASK) {
		case EMV_IAD_MCHIP_CVR_BYTE1_2GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned AAC");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_2GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned TC");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_2GAC_NOT_REQUESTED:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC Not Requested");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_2GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC RFU");
			break;
	}
	switch (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_1GAC_MASK) {
		case EMV_IAD_MCHIP_CVR_BYTE1_1GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned AAC");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_1GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned TC");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_1GAC_ARQC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned ARQC");
			break;

		case EMV_IAD_MCHIP_CVR_BYTE1_1GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC RFU");
			break;
	}
	if (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_DATE_CHECK_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Date Check Failed");
	}
	if (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_PIN_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Performed");
	}
	if (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_ENCRYPTED_PIN_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline Encrypted PIN Verification Performed");
	}
	if (cvr[0] & EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_PIN_SUCCESSFUL) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Successful");
	}

	// Card Verification (CVR) Results byte 2
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_DDA) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Dynamic Data Authentication (DDA) Returned");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_1GAC_CDA) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Returned in First GENERATE AC");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_2GAC_CDA) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Returned in Second GENERATE AC");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_ISSUER_AUTH_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication Performed");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_CIAC_SKIPPED_ON_CAT3) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Card Issuer Action Codes (CIAC) Default Skipped on Cardholder Activated Terminal Level 3 (CAT3)");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_OFFLINE_CHANGE_PIN_SUCCESSFUL) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline Change PIN Result Successful");
	}
	if (cvr[1] & EMV_IAD_MCHIP_CVR_BYTE2_ISSUER_DISCRETIONARY) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Discretionary");
	}

	// Card Verification (CVR) Results byte 3
	if (cvr[2] & EMV_IAD_MCHIP_CVR_BYTE3_SCRIPT_COUNTER_MASK) {
		emv_str_list_add(itr,
			"Card Verification Results (CVR): Script Counter is %u",
			(cvr[2] & EMV_IAD_MCHIP_CVR_BYTE3_SCRIPT_COUNTER_MASK) >> EMV_IAD_MCHIP_CVR_BYTE3_SCRIPT_COUNTER_SHIFT
		);
	}
	if (cvr[2] & EMV_IAD_MCHIP_CVR_BYTE3_PIN_TRY_COUNTER_MASK) {
		emv_str_list_add(itr,
			"Card Verification Results (CVR): PIN Try Counter is %u",
			(cvr[2] & EMV_IAD_MCHIP_CVR_BYTE3_PIN_TRY_COUNTER_MASK) >> EMV_IAD_MCHIP_CVR_BYTE3_PIN_TRY_COUNTER_SHIFT
		);
	}

	// Card Verification (CVR) Results byte 4
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_LAST_ONLINE_TXN_NOT_COMPLETED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Last Online Transaction Not Completed");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_UNABLE_TO_GO_ONLINE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Unable To Go Online");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_OFFLINE_PIN_NOT_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Not Performed");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_OFFLINE_PIN_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN Verification Failed");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_PTL_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PTL Exceeded");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_INTERNATIONAL_TXN) {
		emv_str_list_add(itr, "Card Verification Results (CVR): International Transaction");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_DOMESTIC_TXN) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Domestic Transaction");
	}
	if (cvr[3] & EMV_IAD_MCHIP_CVR_BYTE4_ERR_OFFLINE_PIN_OK) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Terminal Erroneously Considers Offline PIN OK");
	}

	// Card Verification (CVR) Results byte 5
	if (iad_len == EMV_IAD_MCHIP4_LEN) {
		// Assume M/Chip 4
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_L_CONSECUTIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Lower Consecutive Offline Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_U_CONSECUTIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Upper Consecutive Offline Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_L_CUMULATIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Lower Cumulative Offline Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_U_CUMULATIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Upper Cumulative Offline Limit Exceeded");
		}
	} else {
		// Assume M/Chip Advance
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_L_CONSECUTIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Lower Consecutive Counter 1 Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_U_CONSECUTIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Upper Consecutive Counter 1 Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_L_CUMULATIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Lower Cumulative Accumulator Limit Exceeded");
		}
		if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_U_CUMULATIVE_LIMIT_EXCEEDED) {
			emv_str_list_add(itr, "Card Verification Results (CVR): Upper Cumulative Accumulator Limit Exceeded");
		}
	}
	if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_GO_ONLINE_ON_NEXT_TXN) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Go Online On Next Transaction Was Set");
	}
	if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication Failed");
	}
	if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_SCRIPT_RECEIVED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Script Received");
	}
	if (cvr[4] & EMV_IAD_MCHIP_CVR_BYTE5_SCRIPT_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Script Failed");
	}

	// Card Verification (CVR) Results byte 6
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_L_CONSECUTIVE_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Lower Consecutive Counter 2 Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_U_CONSECUTIVE_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Upper Consecutive Counter 2 Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_L_CUMULATIVE_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Lower Cumulative Accumulator 2 Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_U_CUMULATIVE_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Upper Cumulative Accumulator 2 Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_MTA_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): MTA Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_NUM_OF_DAYS_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Number Of Days Offline Limit Exceeded");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_MATCH_ADDITIONAL_CHECK_TABLE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Match Found In Additional Check Table");
	}
	if (cvr[5] & EMV_IAD_MCHIP_CVR_BYTE6_NO_MATCH_ADDITIONAL_CHECK_TABLE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): No Match Found In Additional Check Table");
	}

	return 0;
}

static int emv_iad_vsdc_0_1_3_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr)
{
	const uint8_t* cvr;

	// Issuer Application Data (field 9F10) for Visa Smart Debit/Credit (VSDC)
	// applications using IAD format 0/1/3
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Appendix M
	if (!iad ||
		iad_len < EMV_IAD_VSDC_0_1_3_MIN_LEN ||
		iad[0] != EMV_IAD_VSDC_0_1_3_BYTE1 ||
		iad[3] != EMV_IAD_VSDC_0_1_3_CVR_LEN ||
		(
			(iad[2] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT != 0 &&
			(iad[2] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT != 1 &&
			(iad[2] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT != 3
		)
	) {
		return -1;
	}

	// Derivation Key Index
	emv_str_list_add(itr, "Derivation Key Index (DKI): %02X", iad[1]);

	// Cryptogram Version Number
	// VSDC and VCPS documentation uses the CVNxx notation for IAD format 0/1/3
	emv_str_list_add(itr, "Cryptogram Version Number (CVN): %02X (CVN%02u)", iad[2], iad[2]);

	// Card Verification (CVR) Results byte 2
	cvr = iad + 3;
	switch (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_2GAC_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned AAC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned TC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_NOT_REQUESTED:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC Not Requested");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC RFU");
			break;
	}
	switch (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_1GAC_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned AAC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned TC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_ARQC:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC returned ARQC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): First GENERATE AC RFU");
			break;
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication performed and failed");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_OFFLINE_PIN_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN verification performed");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_OFFLINE_PIN_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline PIN verification failed");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_UNABLE_TO_GO_ONLINE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Unable to go online");
	}

	// Card Verification (CVR) Results byte 3
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_TXN_NOT_COMPLETED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Last online transaction not completed");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_PIN_TRY_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PIN Try Limit exceeded");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_VELOCITY_COUNTERS_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Exceeded velocity checking counters");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_NEW_CARD) {
		emv_str_list_add(itr, "Card Verification Results (CVR): New card");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication failure on last online transaction");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_ISSUER_AUTH_NOT_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication not performed after online authorization");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_APPLICATION_BLOCKED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Application blocked by card because PIN Try Limit exceeded");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_OFFLINE_SDA_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline static data authentication failed on last transaction");
	}

	// Card Verification (CVR) Results byte 4
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_MASK) {
		emv_str_list_add(itr,
			"Card Verification Results (CVR): Script Counter is %u",
			(cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_MASK) >> EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_SHIFT
		);
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_ISSUER_SCRIPT_PROCESSING_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Script processing failed");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_LAST_OFFLINE_DDA_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline dynamic data authentication failed on last transaction");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_OFFLINE_DDA_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline dynamic data authentication performed");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_PIN_VERIFICATION_NOT_RECEIVED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PIN verification command not received for a PIN-Expecting card");
	}

	return 0;
}

static int emv_iad_vsdc_2_4_append_string_list(const uint8_t* iad, size_t iad_len, struct str_itr_t* itr)
{
	const uint8_t* cvr;

	// Issuer Application Data (field 9F10) for Visa Smart Debit/Credit (VSDC)
	// applications using IAD format 2/4
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Appendix M
	if (!iad ||
		iad_len != EMV_IAD_VSDC_2_4_LEN ||
		iad[0] != EMV_IAD_VSDC_2_4_BYTE1 ||
		(iad[8] & 0xF0) || // Unused nibble of Issuer Discretionary Data Option ID
		(
			(iad[1] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT != 2 &&
			(iad[1] & EMV_IAD_VSDC_CVN_FORMAT_MASK) >> EMV_IAD_VSDC_CVN_FORMAT_SHIFT != 4
		)
	) {
		return -1;
	}

	// Cryptogram Version Number
	// VSDC and VCPS documentation uses the CVN'xx' notation for IAD format 2/4
	emv_str_list_add(itr, "Cryptogram Version Number (CVN): %02X (CVN'%02X')", iad[1], iad[1]);

	// Derivation Key Index
	emv_str_list_add(itr, "Derivation Key Index (DKI): %02X", iad[2]);

	// Card Verification (CVR) Results byte 1
	cvr = iad + 3;
	if (!cvr[0]) {
		emv_str_list_add(itr, "No CDCVM");
	}
	switch (cvr[0] & EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_VMPA:
			emv_str_list_add(itr, "Card Verification Results (CVR): Visa Mobile Payment Application (VMPA)");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MG:
			emv_str_list_add(itr, "Card Verification Results (CVR): MG");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_SE_APP:
			emv_str_list_add(itr, "Card Verification Results (CVR): Co-residing SE app");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_TEE_APP:
			emv_str_list_add(itr, "Card Verification Results (CVR): TEE app");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MOBILE_APP:
			emv_str_list_add(itr, "Card Verification Results (CVR): Mobile Application");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_TERMINAL:
			emv_str_list_add(itr, "Card Verification Results (CVR): Terminal");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_CLOUD:
			emv_str_list_add(itr, "Card Verification Results (CVR): Verified in the cloud");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MOBILE_DEVICE_OS:
			emv_str_list_add(itr, "Card Verification Results (CVR): Verified by the mobile device OS");
			break;
	}
	switch (cvr[0] & EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_PASSCODE:
			emv_str_list_add(itr, "Card Verification Results (CVR): Passcode");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_FINGER:
			emv_str_list_add(itr, "Card Verification Results (CVR): Finger biometric");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_MOBILE_DEVICE_PATTERN:
			emv_str_list_add(itr, "Card Verification Results (CVR): Mobile device pattern");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_FACIAL:
			emv_str_list_add(itr, "Card Verification Results (CVR): Facial biometric");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_IRIS:
			emv_str_list_add(itr, "Card Verification Results (CVR): Iris biometric");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_VOICE:
			emv_str_list_add(itr, "Card Verification Results (CVR): Voice biometric");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_PALM:
			emv_str_list_add(itr, "Card Verification Results (CVR): Palm biometric");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_SIGNATURE:
			emv_str_list_add(itr, "Card Verification Results (CVR): Signature");
			break;

		case EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_ONLINE_PIN:
			emv_str_list_add(itr, "Card Verification Results (CVR): Online PIN");
			break;
	}

	// Card Verification (CVR) Results byte 2
	switch (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_2GAC_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned AAC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC returned TC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_NOT_REQUESTED:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC Not Requested");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_2GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): Second GENERATE AC RFU");
			break;
	}
	switch (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_1GAC_MASK) {
		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_AAC:
			emv_str_list_add(itr, "Card Verification Results (CVR): GPO returned AAC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_TC:
			emv_str_list_add(itr, "Card Verification Results (CVR): GPO returned TC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_ARQC:
			emv_str_list_add(itr, "Card Verification Results (CVR): GPO returned ARQC");
			break;

		case EMV_IAD_VSDC_CVR_BYTE2_1GAC_RFU:
			emv_str_list_add(itr, "Card Verification Results (CVR): GPO RFU");
			break;
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication performed and failed");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_CDCVM_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): CDCVM successfully performed");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_RFU) {
		emv_str_list_add(itr, "Card Verification Results (CVR): RFU");
	}
	if (cvr[1] & EMV_IAD_VSDC_CVR_BYTE2_UNABLE_TO_GO_ONLINE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Unable to go online");
	}

	// Card Verification (CVR) Results byte 3
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_TXN_NOT_COMPLETED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Last online transaction not completed");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_PIN_TRY_LIMIT_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PIN Try Limit exceeded");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_VELOCITY_COUNTERS_EXCEEDED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Exceeded velocity checking counters");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_NEW_CARD) {
		emv_str_list_add(itr, "Card Verification Results (CVR): New card");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_ISSUER_AUTH_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication failure on last online transaction");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_ISSUER_AUTH_NOT_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Authentication not performed after online authorization");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_APPLICATION_BLOCKED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Application blocked by card because PIN Try Limit exceeded");
	}
	if (cvr[2] & EMV_IAD_VSDC_CVR_BYTE3_LAST_OFFLINE_SDA_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline static data authentication failed on last transaction");
	}

	// Card Verification (CVR) Results byte 4
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_MASK) {
		emv_str_list_add(itr,
			"Card Verification Results (CVR): Script Counter is %u",
			(cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_MASK) >> EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_SHIFT
		);
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_ISSUER_SCRIPT_PROCESSING_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Issuer Script processing failed");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_LAST_OFFLINE_DDA_FAILED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline dynamic data authentication failed on last transaction");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_OFFLINE_DDA_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Offline dynamic data authentication performed");
	}
	if (cvr[3] & EMV_IAD_VSDC_CVR_BYTE4_PIN_VERIFICATION_NOT_RECEIVED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): PIN verification command not received for a PIN-Expecting card");
	}

	// Card Verification (CVR) Results byte 5
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_CD_NOT_DEBUG_MODE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Consumer Device is not in debug mode");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_CD_NOT_ROOTED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Consumer Device is not a rooted device");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_MOBILE_APP_NOT_HOOKED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Mobile Application is not hooked");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_MOBILE_APP_INTEGRITY) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Mobile Application integrity is intact");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_CD_HAS_CONNECTIVITY) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Consumer Device has data connectivity");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_CD_IS_GENUINE) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Consumer Device is genuine");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_CDCVM_PERFORMED) {
		emv_str_list_add(itr, "Card Verification Results (CVR): CDCVM successfully performed");
	}
	if (cvr[4] & EMV_IAD_VSDC_CVR_BYTE5_EMV_SESSION_KEY) {
		emv_str_list_add(itr, "Card Verification Results (CVR): Secure Messaging uses EMV Session key-based derivation");
	}

	return 0;
}

int emv_iad_get_string_list(
	const uint8_t* iad,
	size_t iad_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;
	enum emv_iad_format_t iad_format;

	if (!iad || !iad_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (iad_len > 32) {
		// Issuer Application Data (field 9F10) must be 1 to 32 bytes.
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	iad_format = emv_iad_get_format(iad, iad_len);

	// Stringify issuer application type
	switch (iad_format) {
		case EMV_IAD_FORMAT_INVALID:
			emv_str_list_add(&itr, "Invalid IAD format");
			return -1;

		case EMV_IAD_FORMAT_CCD:
			emv_str_list_add(&itr, "Application: CCD-Compliant");
			break;

		case EMV_IAD_FORMAT_MCHIP4:
			emv_str_list_add(&itr, "Application: M/Chip 4");
			break;

		case EMV_IAD_FORMAT_MCHIP_ADVANCE:
			emv_str_list_add(&itr, "Application: M/Chip Advance");
			break;

		case EMV_IAD_FORMAT_VSDC_0:
		case EMV_IAD_FORMAT_VSDC_1:
		case EMV_IAD_FORMAT_VSDC_2:
		case EMV_IAD_FORMAT_VSDC_3:
		case EMV_IAD_FORMAT_VSDC_4:
			emv_str_list_add(&itr, "Application: Visa Smart Debit/Credit (VSDC)");
			break;

		default:
			emv_str_list_add(&itr, "Unknown IAD format");
			return 0;
	}

	// Stringify IAD format
	switch (iad_format) {
		case EMV_IAD_FORMAT_CCD:
			emv_str_list_add(&itr, "IAD Format: CCD Version 4.1");
			break;

		case EMV_IAD_FORMAT_MCHIP4:
		case EMV_IAD_FORMAT_MCHIP_ADVANCE:
			// No explicit IAD format version for M/Chip
			break;

		case EMV_IAD_FORMAT_VSDC_0:
			emv_str_list_add(&itr, "IAD Format: 0");
			break;

		case EMV_IAD_FORMAT_VSDC_1:
			emv_str_list_add(&itr, "IAD Format: 1");
			break;

		case EMV_IAD_FORMAT_VSDC_2:
			emv_str_list_add(&itr, "IAD Format: 2");
			break;

		case EMV_IAD_FORMAT_VSDC_3:
			emv_str_list_add(&itr, "IAD Format: 3");
			break;

		case EMV_IAD_FORMAT_VSDC_4:
			emv_str_list_add(&itr, "IAD Format: 4");
			break;

		default:
			return -1;
	}

	switch (iad_format) {
		case EMV_IAD_FORMAT_CCD:
			return emv_iad_ccd_append_string_list(iad, iad_len, &itr);

		case EMV_IAD_FORMAT_MCHIP4:
		case EMV_IAD_FORMAT_MCHIP_ADVANCE:
			return emv_iad_mchip_append_string_list(iad, iad_len, &itr);

		case EMV_IAD_FORMAT_VSDC_0:
		case EMV_IAD_FORMAT_VSDC_1:
		case EMV_IAD_FORMAT_VSDC_3:
			return emv_iad_vsdc_0_1_3_append_string_list(iad, iad_len, &itr);

		case EMV_IAD_FORMAT_VSDC_2:
		case EMV_IAD_FORMAT_VSDC_4:
			return emv_iad_vsdc_2_4_append_string_list(iad, iad_len, &itr);

		default:
			return -1;
	}
}

int emv_terminal_risk_management_data_get_string_list(
	const uint8_t* trmd,
	size_t trmd_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!trmd || !trmd_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (trmd_len != 8) {
		// Terminal Risk Management Data (field 9F1D) must be 8 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Terminal Risk Management Data (field 9F1D) byte 1
	// See EMV Contactless Book C-2 v2.11, Annex A.1.161
	// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
	if (trmd[0] & EMV_TRMD_BYTE1_RESTART_SUPPORTED) {
		// NOTE: EMV Contactless Book C-8 v1.1, Annex A.1.129 does not define
		// this bit and it avoids confusion if no string is provided when this
		// bit is unset
		emv_str_list_add(&itr, "Restart supported");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_ENCIPHERED_PIN_ONLINE_CONTACTLESS) {
		emv_str_list_add(&itr, "Enciphered PIN verified online (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_SIGNATURE_CONTACTLESS) {
		emv_str_list_add(&itr, "Signature (paper) (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_ENCIPHERED_PIN_OFFLINE_CONTACTLESS) {
		emv_str_list_add(&itr, "Enciphered PIN verification performed by ICC (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_NO_CVM_CONTACTLESS) {
		emv_str_list_add(&itr, "No CVM required (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_CDCVM_CONTACTLESS) {
		emv_str_list_add(&itr, "CDCVM (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_PLAINTEXT_PIN_OFFLINE_CONTACTLESS) {
		emv_str_list_add(&itr, "Plaintext PIN verification performed by ICC (Contactless)");
	}
	if (trmd[0] & EMV_TRMD_BYTE1_PRESENT_AND_HOLD_SUPPORTED) {
		// NOTE: EMV Contactless Book C-8 v1.1, Annex A.1.129 does not define
		// this bit and it avoids confusion if no string is provided when this
		// bit is unset
		emv_str_list_add(&itr, "Present and Hold supported");
	}

	// Terminal Risk Management Data (field 9F1D) byte 2
	// See EMV Contactless Book C-2 v2.11, Annex A.1.161
	// See EMV Contactless Book C-8 v1.1, Annex A.1.129
	// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
	if (trmd[1] & EMV_TRMD_BYTE2_CVM_LIMIT_EXCEEDED) {
		emv_str_list_add(&itr, "CVM Limit exceeded");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_ENCIPHERED_PIN_ONLINE_CONTACT) {
		emv_str_list_add(&itr, "Enciphered PIN verified online (Contact)");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_SIGNATURE_CONTACT) {
		emv_str_list_add(&itr, "Signature (paper) (Contact)");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_ENCIPHERED_PIN_OFFLINE_CONTACT) {
		emv_str_list_add(&itr, "Enciphered PIN verification performed by ICC (Contact)");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_NO_CVM_CONTACT) {
		emv_str_list_add(&itr, "No CVM required (Contact)");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_CDCVM_CONTACT) {
		emv_str_list_add(&itr, "CDCVM (Contact)");
	}
	if (trmd[1] & EMV_TRMD_BYTE2_PLAINTEXT_PIN_OFFLINE_CONTACT) {
		emv_str_list_add(&itr, "Plaintext PIN verification performed by ICC (Contact)");
	}

	// Terminal Risk Management Data (field 9F1D) byte 3
	// See EMV Contactless Book C-2 v2.11, Annex A.1.161
	// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
	if (trmd[2] & EMV_TRMD_BYTE3_MAGSTRIPE_MODE_CONTACTLESS_NOT_SUPPORTED) {
		// NOTE: EMV Contactless Book C-8 v1.1, Annex A.1.129 does not define
		// this bit and it avoids confusion if no string is provided when this
		// bit is unset
		emv_str_list_add(&itr, "Mag-stripe mode contactless transactions not supported");
	}
	if (trmd[2] & EMV_TRMD_BYTE3_EMV_MODE_CONTACTLESS_NOT_SUPPORTED) {
		// NOTE: EMV Contactless Book C-8 v1.1, Annex A.1.129 does not define
		// this bit and it avoids confusion if no string is provided when this
		// bit is unset
		emv_str_list_add(&itr, "EMV mode contactless transactions not supported");
	}
	if (trmd[2] & EMV_TRMD_BYTE3_CDCVM_WITHOUT_CDA_SUPPORTED) {
		// NOTE: EMV Contactless Book C-8 v1.1, Annex A.1.129 does not define
		// this bit and it avoids confusion if no string is provided when this
		// bit is unset
		emv_str_list_add(&itr, "CDCVM without CDA supported");
	}

	// Terminal Risk Management Data (field 9F1D) byte 4
	// See EMV Contactless Book C-2 v2.11, Annex A.1.161
	// See EMV Contactless Book C-8 v1.1, Annex A.1.129
	if (trmd[3] & EMV_TRMD_BYTE4_CDCVM_BYPASS_REQUESTED) {
		emv_str_list_add(&itr, "CDCVM bypass requested");
	}
	if (trmd[3] & EMV_TRMD_BYTE4_SCA_EXEMPT) {
		emv_str_list_add(&itr, "SCA exempt");
	}

	// Terminal Risk Management Data (field 9F1D) RFU bits
	if ((trmd[1] & EMV_TRMD_BYTE2_RFU) ||
		(trmd[2] & EMV_TRMD_BYTE3_RFU) ||
		(trmd[3] & EMV_TRMD_BYTE4_RFU) ||
		trmd[4] || trmd[5] || trmd[6] || trmd[7]
	) {
		emv_str_list_add(&itr, "RFU");
	}

	return 0;
}

int emv_ttq_get_string_list(
	const uint8_t* ttq,
	size_t ttq_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!ttq || !ttq_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (ttq_len != 4) {
		// Terminal Transaction Qualifiers (field 9F66) must be 4 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Terminal Transaction Qualifiers (field 9F66) byte 1
	// See EMV Contactless Book A v2.10, 5.7, Table 5-4
	if (ttq[0] & EMV_TTQ_MAGSTRIPE_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Mag-stripe mode supported");
	} else {
		emv_str_list_add(&itr, "Mag-stripe mode not supported");
	}
	if (ttq[0] & EMV_TTQ_BYTE1_RFU) {
		emv_str_list_add(&itr, "RFU");
	}
	if (ttq[0] & EMV_TTQ_EMV_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "EMV mode supported");
	} else {
		emv_str_list_add(&itr, "EMV mode not supported");
	}
	if (ttq[0] & EMV_TTQ_EMV_CONTACT_SUPPORTED) {
		emv_str_list_add(&itr, "EMV contact chip supported");
	} else {
		emv_str_list_add(&itr, "EMV contact chip not supported");
	}
	if (ttq[0] & EMV_TTQ_OFFLINE_ONLY_READER) {
		emv_str_list_add(&itr, "Offline-only reader");
	} else {
		emv_str_list_add(&itr, "Online capable reader");
	}
	if (ttq[0] & EMV_TTQ_ONLINE_PIN_SUPPORTED) {
		emv_str_list_add(&itr, "Online PIN supported");
	} else {
		emv_str_list_add(&itr, "Online PIN not supported");
	}
	if (ttq[0] & EMV_TTQ_SIGNATURE_SUPPORTED) {
		emv_str_list_add(&itr, "Signature supported");
	} else {
		emv_str_list_add(&itr, "Signature not supported");
	}
	if (ttq[0] & EMV_TTQ_ODA_FOR_ONLINE_AUTH_SUPPORTED) {
		emv_str_list_add(&itr, "Offline Data Authentication for Online Authorizations supported");
	} else {
		emv_str_list_add(&itr, "Offline Data Authentication for Online Authorizations not supported");
	}

	// Terminal Transaction Qualifiers (field 9F66) byte 2
	// See EMV Contactless Book A v2.10, 5.7, Table 5-4
	if (ttq[1] & EMV_TTQ_ONLINE_CRYPTOGRAM_REQUIRED) {
		emv_str_list_add(&itr, "Online cryptogram required");
	} else {
		emv_str_list_add(&itr, "Online cryptogram not required");
	}
	if (ttq[1] & EMV_TTQ_CVM_REQUIRED) {
		emv_str_list_add(&itr, "CVM required");
	} else {
		emv_str_list_add(&itr, "CVM not required");
	}
	if (ttq[1] & EMV_TTQ_OFFLINE_PIN_SUPPORTED) {
		emv_str_list_add(&itr, "(Contact Chip) Offline PIN supported");
	} else {
		emv_str_list_add(&itr, "(Contact Chip) Offline PIN not supported");
	}
	if (ttq[1] & EMV_TTQ_BYTE2_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	// Terminal Transaction Qualifiers (field 9F66) byte 3
	// See EMV Contactless Book A v2.10, 5.7, Table 5-4
	// See EMV Contactless Book C-6 v2.6, Annex D.11
	if (ttq[2] & EMV_TTQ_ISSUER_UPDATE_PROCESSING_SUPPORTED) {
		emv_str_list_add(&itr, "Issuer Update Processing supported");
	} else {
		emv_str_list_add(&itr, "Issuer Update Processing not supported");
	}
	if (ttq[2] & EMV_TTQ_CDCVM_SUPPORTED) {
		emv_str_list_add(&itr, "Consumer Device CVM supported");
	} else {
		emv_str_list_add(&itr, "Consumer Device CVM not supported");
	}
	if (ttq[2] & EMV_TTQ_CDCVM_REQUIRED) {
		// NOTE: Only EMV Contactless Book C-6 v2.6, Annex D.11 defines this
		// bit and it avoids confusion for other kernels if no string is
		// provided when this bit is unset
		emv_str_list_add(&itr, "Consumer Device CVM required");
	}
	if (ttq[2] & EMV_TTQ_BYTE3_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	// Terminal Transaction Qualifiers (field 9F66) byte 4
	// See EMV Contactless Book A v2.10, 5.7, Table 5-4
	// See EMV Contactless Book C-7 v2.9, 3.2.2, Table 3-1
	if (ttq[3] & EMV_TTQ_FDDA_V1_SUPPORTED) {
		// NOTE: EMV Contactless Book C-7 v2.9, 3.2.2, Table 3-1 does not
		// specify a string when this bit is not set and it avoids confusion
		// for other kernels if no string is provided when this bit is unset
		emv_str_list_add(&itr, "fDDA v1.0 Supported");
	}
	if (ttq[3] & EMV_TTQ_BYTE4_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	return 0;
}

int emv_ctq_get_string_list(
	const uint8_t* ctq,
	size_t ctq_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!ctq || !ctq_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (ctq_len != 2) {
		// Card Transaction Qualifiers (field 9F6C) must be 2 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Card Transaction Qualifiers (field 9F6C) byte 1
	// See EMV Contactless Book C-3 v2.10, Annex A.2
	// See EMV Contactless Book C-7 v2.9, Annex A
	if (ctq[0] & EMV_CTQ_ONLINE_PIN_REQUIRED) {
		emv_str_list_add(&itr, "Online PIN Required");
	}
	if (ctq[0] & EMV_CTQ_SIGNATURE_REQUIRED) {
		emv_str_list_add(&itr, "Signature Required");
	}
	if (ctq[0] & EMV_CTQ_ONLINE_IF_ODA_FAILED) {
		emv_str_list_add(&itr, "Go Online if Offline Data Authentication Fails and Reader is online capable");
	}
	if (ctq[0] & EMV_CTQ_SWITCH_INTERFACE_IF_ODA_FAILED) {
		emv_str_list_add(&itr, "Switch Interface if Offline Data Authentication fails and Reader supports contact chip");
	}
	if (ctq[0] & EMV_CTQ_ONLINE_IF_APPLICATION_EXPIRED) {
		emv_str_list_add(&itr, "Go Online if Application Expired");
	}
	if (ctq[0] & EMV_CTQ_SWITCH_INTERFACE_IF_CASH) {
		emv_str_list_add(&itr, "Switch Interface for Cash Transactions");
	}
	if (ctq[0] & EMV_CTQ_SWITCH_INTERFACE_IF_CASHBACK) {
		emv_str_list_add(&itr, "Switch Interface for Cashback Transactions");
	}
	if (ctq[0] & EMV_CTQ_ATM_NOT_VALID) {
		// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
		emv_str_list_add(&itr, "Not valid for contactless ATM transactions");
	}

	// Card Transaction Qualifiers (field 9F6C) byte 2
	// See EMV Contactless Book C-3 v2.10, Annex A.2
	// See EMV Contactless Book C-7 v2.9, Annex A
	if (ctq[1] & EMV_CTQ_CDCVM_PERFORMED) {
		emv_str_list_add(&itr, "Consumer Device CVM Performed");
	}
	if (ctq[1] & EMV_CTQ_ISSUER_UPDATE_PROCESSING_SUPPORTED) {
		emv_str_list_add(&itr, "Card supports Issuer Update Processing at the POS");
	}
	if (ctq[1] & EMV_CTQ_BYTE2_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	return 0;
}

int emv_amex_cl_reader_caps_get_string(
	uint8_t cl_reader_caps,
	char* str,
	size_t str_len
)
{
	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Amex Contactless Reader Capabilities (field 9F6D)
	// See EMV Contactless Book C-4 v2.10, 4.3.3, Table 4-2
	switch (cl_reader_caps & AMEX_CL_READER_CAPS_MASK) {
		case AMEX_CL_READER_CAPS_DEPRECATED:
			strncpy(str, "Deprecated", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		case AMEX_CL_READER_CAPS_MAGSTRIPE_CVM_NOT_REQUIRED:
			strncpy(str, "Mag-stripe CVM Not Required", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		case AMEX_CL_READER_CAPS_MAGSTRIPE_CVM_REQUIRED:
			strncpy(str, "Mag-stripe CVM Required", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		case AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_DEPRECATED:
			strncpy(str, "Deprecated - EMV and Mag-stripe", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		case AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_NOT_REQUIRED:
			strncpy(str, "EMV and Mag-stripe CVM Not Required", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		case AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_REQUIRED:
			strncpy(str, "EMV and Mag-stripe CVM Required", str_len - 1);
			str[str_len - 1] = 0;
			return 0;

		default:
			strncpy(str, "Not Available for Use", str_len - 1);
			str[str_len - 1] = 0;
			return 0;
	}
}

static const struct {
	char device_type[3];
	const char* str;
} emv_mastercard_device_type_map[] = {
	// See M/Chip Requirements for Contact and Contactless, 28 September 2017, Chapter 5, Third Party Data, Device Type
	// The 2017 version of this document contains Device Types "00" to "19".
	// Newer versions of this document indicate that the Device Type list can
	// be found in the Mastercard Customer Interface Specification manual but
	// that document does not appear to be publically available.
	// Device Types "20" to "33" were obtained from unverified internet sources
	{ "00", "Card" },
	{ "01", "Mobile Phone or Smartphone with Mobile Network Operator (MNO) controlled removable secure element (SIM or UICC) personalized for use with a mobile phone or smartphone" },
	{ "02", "Key Fob" },
	{ "03", "Watch using a contactless chip or a fixed (non-removable) secure element not controlled by the MNO" },
	{ "04", "Mobile Tag" },
	{ "05", "Wristband" },
	{ "06", "Mobile Phone Case or Sleeve" },
	{ "07", "Mobile phone or smartphone with a fixed (non-removable) secure element controlled by the MNO, for example, code division multiple access (CDMA)" },
	{ "08", "Removable secure element not controlled by the MNO, for example, memory card personalized for used with a mobile phone or smartphone" },
	{ "09", "Mobile Phone or smartphone with a fixed (non-removable) secure element not controlled by the MNO" },
	{ "10", "MNO controlled removable secure element (SIM or UICC) personalized for use with a tablet or e-book" },
	{ "11", "Tablet or e-book with a fixed (non-removable) secure element controlled by the MNO" },
	{ "12", "Removable secure element not controlled by the MNO, for example, memory card personalized for use with a tablet or e-book" },
	{ "13", "Tablet or e-book with fixed (non-removable) secure element not controlled by the MNO" },
	{ "14", "Mobile phone or smartphone with a payment application running in a host processor" },
	{ "15", "Tablet or e-book with a payment application running in a host processor" },
	{ "16", "Mobile phone or smartphone with a payment application running in the Trusted Execution Environment (TEE) of a host processor" },
	{ "17", "Tablet or e-book with a payment application running in the TEE of a host processor" },
	{ "18", "Watch with a payment application running in the TEE of a host processor" },
	{ "19", "Watch with a payment application running in a host processor" },
	{ "20", "Card" },
	{ "21", "Phone (Mobile phone)" },
	{ "22", "Tablet/e-reader (Tablet computer or e-reader)" },
	{ "23", "Watch/Wristband (Watch or wristband, including a fitness band, smart strap, disposable band, watch add-on, and security/ID band)" },
	{ "24", "Sticker" },
	{ "25", "PC (PC or laptop)" },
	{ "26", "Device Peripheral (Mobile phone case or sleeve)" },
	{ "27", "Tag (Key fob or mobile tag)" },
	{ "28", "Jewelry (Ring, bracelet, necklace, and cuff links)" },
	{ "29", "Fashion Accessory (Handbag, bag charm, and glasses)" },
	{ "30", "Garment (Dress)" },
	{ "31", "Domestic Appliance (Refrigerator, washing machine)" },
	{ "32", "Vehicle (Vehicle, including vehicle attached devices)" },
	{ "33", "Media/Gaming Device (Media or gaming device, including a set top box, media player, and television)" },
};

static const char* emv_mastercard_device_type_get_string(const char* device_type)
{
	for (size_t i = 0; i < sizeof(emv_mastercard_device_type_map) / sizeof(emv_mastercard_device_type_map[0]); ++i) {
		if (strncmp(emv_mastercard_device_type_map[i].device_type, device_type, 2) == 0) {
			return emv_mastercard_device_type_map[i].str;
		}
	}

	return NULL;
}

int emv_mastercard_third_party_data_get_string_list(
	const uint8_t* tpd,
	size_t tpd_len,
	char* str,
	size_t str_len
)
{
	int r;
	const uint8_t* tpd_ptr;
	struct str_itr_t itr;
	char country_str[128];
	uint16_t unique_id;
	bool is_product_extension = false;
	size_t tpd_len_min;
	size_t remaining_len;

	if (!tpd || !tpd_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tpd_len < 5 || tpd_len > 32) {
		// Mastercard Third Party Data (field 9F6E) must be 5 to 32 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Mastercard Third Party Data (field 9F6E)
	// See EMV Contactless Book C-2 v2.10, Annex A.1.171
	// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Chapter 5, Third Party Data, Table 12
	tpd_ptr = tpd;

	// First two byte are the ISO 3166-1 numeric country code
	memset(country_str, 0, sizeof(country_str));
	r = emv_country_numeric_code_get_string(tpd_ptr, 2, country_str, sizeof(country_str));
	if (r == 0 && country_str[0]) {
		emv_str_list_add(&itr, "Country: %s", country_str);
	} else {
		emv_str_list_add(&itr, "Country: Unknown");
	}
	tpd_ptr += 2;

	// Next two bytes are the Mastercard Unique Identifier
	unique_id = (tpd_ptr[0] << 8) + tpd_ptr[1];
	if (unique_id == 0x0000) {
		// 0000 if Proprietary Data not used
		emv_str_list_add(&itr, "Unique Identifier: Proprietary Data not used");
	} else if (unique_id == 0x0003 || unique_id == 0x8003) {
		// The value 0003 or 8003 indicate that a Mastercard defined
		// Product Extension is present in the Proprietary Data
		is_product_extension = true;
		emv_str_list_add(&itr, "Unique Identifier: Mastercard Product Extension");
	} else {
		emv_str_list_add(&itr, "Unique Identifier: Unknown");
	}
	tpd_ptr += 2;

	// Device Type is present when the most significant bit of the
	// Unique Identifier is unset. The Device Type has format 'an'.
	if ((unique_id & 0x8000) == 0) {
		char device_type[3];
		const char* device_type_str;

		memcpy(device_type, tpd_ptr, 2);
		device_type[2] = 0; // NULL terminate

		// Lookup Device Type string
		device_type_str = emv_mastercard_device_type_get_string(device_type);
		if (device_type_str) {
			emv_str_list_add(&itr, "Device Type: %s - %s", device_type, device_type_str);
		} else {
			emv_str_list_add(&itr, "Device Type: %s", device_type);
		}

		// If Device Type is present, minimum field length is 7
		tpd_len_min = 7;

		tpd_ptr += 2;
	} else {
		// If Device Type is absent, minimum field length is 5
		tpd_len_min = 5;
	}

	if (tpd_len < tpd_len_min) {
		// Invalid Mastercard Third Party Data (field 9F6E) length
		return 1;
	}
	remaining_len = tpd_len - (tpd_ptr - tpd);

	// Verify Proprietary Data length
	if (unique_id == 0x0000 &&
		(tpd_ptr[0] != 0x00 || remaining_len != 1)
	) {
		// If Unique Identifier indicates that Proprietary Data is not used,
		// then the latter should be a single 0x00 byte
		emv_str_list_add(&itr, "Invalid Proprietary Data");
		return 1;
	} else if (is_product_extension && remaining_len < 2) {
		// If Unique Identifier indicates a Mastercard Product Extension,
		// then the Proprietary Data should have at least 2 bytes for the
		// Product Identifier
		emv_str_list_add(&itr, "Invalid Proprietary Data");
		return 1;
	}

	// Decode Mastercard Product Extension
	// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Chapter 5, Third Party Data, Table 13
	if (is_product_extension) {
		// Extract Product Identifier
		uint16_t product_identifier = (tpd_ptr[0] << 8) + tpd_ptr[1];
		if (product_identifier == 0x0001) {
			// Product Extension for Fleet Cards
			// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Chapter 5, Third Party Data, Table 14
			emv_str_list_add(&itr, "Product Identifier: Fleet Card");

			// Product Extension for Fleet Cards has length 8
			if (remaining_len != 8) {
				emv_str_list_add(&itr, "Invalid Proprietary Data length");
				return 1;
			}

			// Product Restriction Code
			if (tpd_ptr[2] == 0x02) {
				emv_str_list_add(&itr, "Product Restriction Code: Good for fuel only");
			} else if (tpd_ptr[2] == 0x01) {
				emv_str_list_add(&itr, "Product Restriction Code: Good for fuel and other products");
			} else if (tpd_ptr[2]) {
				emv_str_list_add(&itr, "Product Restriction Code: RFU");
			}

			// Product Type Code
			if (tpd_ptr[3] & 0x08) {
				emv_str_list_add(&itr, "Product Type Code: Prompt for Odometer");
			}
			if (tpd_ptr[3] & 0x04) {
				emv_str_list_add(&itr, "Product Type Code: Prompt for Driver Number");
			}
			if (tpd_ptr[3] & 0x02) {
				emv_str_list_add(&itr, "Product Type Code: Prompt for Vehicle Number");
			}
			if (tpd_ptr[3] & 0x01) {
				emv_str_list_add(&itr, "Product Type Code: Prompt for ID Number");
			}
			if (tpd_ptr[3] & 0xF0) {
				emv_str_list_add(&itr, "Product Type Code: RFU");
			}

			// Card Type
			if (tpd_ptr[4] == 0x80) {
				emv_str_list_add(&itr, "Card Type: Driver card");
			} else if (tpd_ptr[4] == 0x40) {
				emv_str_list_add(&itr, "Card Type: Vehicle card");
			} else if (tpd_ptr[4]) {
				emv_str_list_add(&itr, "Card Type: RFU");
			}

			return 0;
		}

		if (product_identifier == 0x0002) {
			// Product Extension for Transit
			// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Chapter 5, Third Party Data, Table 15
			emv_str_list_add(&itr, "Product Identifier: Transit");

			// Product Extension for Transit has length 5
			if (remaining_len != 5) {
				emv_str_list_add(&itr, "Invalid Proprietary Data length");
				return 1;
			}

			// Transit Byte 3
			if (tpd_ptr[2] & 0x80) {
				emv_str_list_add(&itr, "Transit: Deferred Authorization Not Supported");
			}
			if (tpd_ptr[2] & 0x7F) {
				emv_str_list_add(&itr, "Transit: RFU");
			}

			// Concession Code
			switch (tpd_ptr[3]) {
				case 1: emv_str_list_add(&itr, "Concession Code: 01 - Senior Citizen, potentially eligible for senior citizen discounts"); break;
				case 2: emv_str_list_add(&itr, "Concession Code: 02 - Student, potentially eligible for student-based discounts"); break;
				case 3: emv_str_list_add(&itr, "Concession Code: 03 - Active military and veterans, potentially eligible for service member discounts"); break;
				case 4: emv_str_list_add(&itr, "Concession Code: 04 - Low Income household, potentially eligible for means-based discounts"); break;
				case 5: emv_str_list_add(&itr, "Concession Code: 05 - Disability, eligible for paratransit and other disability discounts"); break;
				case 6: emv_str_list_add(&itr, "Concession Code: 06 - Minor Child, potentially enables free travel for under 16 and young kids"); break;
				case 7: emv_str_list_add(&itr, "Concession Code: 07 - Transit Staff, potentially enables free travel for transit staff"); break;
				case 8: emv_str_list_add(&itr, "Concession Code: 08 - City/Government/Preferred Employees, potentially enables discounted travel for federal and preferred employees"); break;
				default: emv_str_list_add(&itr, "Concession Code: RFU"); break;
			}

			return 0;
		}

		emv_str_list_add(&itr, "Product Identifier: Unknown");
		return 0;

	} else if (tpd_ptr[0] == 0x00) {
		emv_str_list_add(&itr, "Proprietary Data: Not used");
	}

	return 0;
}

int emv_visa_form_factor_indicator_get_string_list(
	const uint8_t* ffi,
	size_t ffi_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!ffi || !ffi_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (ffi_len != 4) {
		// Visa Form Factor Indicator (field 9F6E) must be 4 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Visa Form Factor Indicator (field 9F6E) byte 1
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
	if ((ffi[0] & VISA_FFI_VERSION_MASK) != VISA_FFI_VERSION_NUMBER_1) {
		emv_str_list_add(&itr, "Form Factor Indicator (FFI): Version Number %u", ffi[0] >> VISA_FFI_VERSION_SHIFT);

		// This implementation only supports version 1
		return 0;
	}
	switch (ffi[0] & VISA_FFI_FORM_FACTOR_MASK) {
		case VISA_FFI_FORM_FACTOR_CARD: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Card"); break;
		case VISA_FFI_FORM_FACTOR_MINI_CARD: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Mini-card"); break;
		case VISA_FFI_FORM_FACTOR_NON_CARD: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Non-card Form Factor"); break;
		case VISA_FFI_FORM_FACTOR_CONSUMER_MOBILE_PHONE: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Consumer mobile phone"); break;
		case VISA_FFI_FORM_FACTOR_WRIST_WORN_DEVICE: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Wrist-worn device"); break;
		default: emv_str_list_add(&itr, "Consumer Payment Device Form Factor: Unknown"); break;
	}

	// Visa Form Factor Indicator (field 9F6E) byte 2
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
	if (ffi[1] & VISA_FFI_FEATURE_PASSCODE) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Passcode Capable");
	}
	if (ffi[1] & VISA_FFI_FEATURE_SIGNATURE) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Signature Panel");
	}
	if (ffi[1] & VISA_FFI_FEATURE_HOLOGRAM) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Hologram");
	}
	if (ffi[1] & VISA_FFI_FEATURE_CVV2) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: CVV2");
	}
	if (ffi[1] & VISA_FFI_FEATURE_TWO_WAY_MESSAGING) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Two-way Messaging");
	}
	if (ffi[1] & VISA_FFI_FEATURE_CLOUD_CREDENTIALS) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Cloud Based Payment Credentials");
	}
	if (ffi[1] & VISA_FFI_FEATURE_BIOMETRIC) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: Biometric Cardholder Verification Capable");
	}
	if (ffi[1] & VISA_FFI_FEATURE_RFU) {
		emv_str_list_add(&itr, "Consumer Payment Device Features: RFU");
	}

	// Visa Form Factor Indicator (field 9F6E) byte 3
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
	if (ffi[2]) {
		emv_str_list_add(&itr, "Form Factor Indicator (FFI) byte 3: RFU");
	}

	// Visa Form Factor Indicator (field 9F6E) byte 4
	// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
	switch (ffi[3] & VISA_FFI_PAYMENT_TXN_TECHNOLOGY_MASK) {
		case VISA_FFI_PAYMENT_TXN_TECHNOLOGY_CONTACTLESS: emv_str_list_add(&itr, "Payment Transaction Technology: Proximity Contactless interface using ISO 14443 (including NFC)"); break;
		case VISA_FFI_PAYMENT_TXN_TECHNOLOGY_NOT_VCPS: emv_str_list_add(&itr, "Payment Transaction Technology: Not used in VCPS"); break;
		default: emv_str_list_add(&itr, "Payment Transaction Technology: Unknown"); break;
	}
	if (ffi[3] & VISA_FFI_PAYMENT_TXN_TECHNOLOGY_RFU) {
		emv_str_list_add(&itr, "Payment Transaction Technology: RFU");
	}

	return 0;
}

int emv_amex_enh_cl_reader_caps_get_string_list(
	const uint8_t* enh_cl_reader_caps,
	size_t enh_cl_reader_caps_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!enh_cl_reader_caps || !enh_cl_reader_caps_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (enh_cl_reader_caps_len != 4) {
		// Amex Enhanced Contactless Reader Capabilities (field 9F6E) must be 4 bytes
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 1
	// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_CONTACT_SUPPORTED) {
		emv_str_list_add(&itr, "Contact mode supported");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_MAGSTRIPE_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless Mag-Stripe Mode supported");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_FULL_ONLINE_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless EMV full online mode supported (legacy feature and no longer supported)");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_PARTIAL_ONLINE_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless EMV partial online mode supported");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_MOBILE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless Mobile Supported");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_TRY_ANOTHER_INTERFACE) {
		emv_str_list_add(&itr, "Try Another Interface after a decline");
	}
	if (enh_cl_reader_caps[0] & AMEX_ENH_CL_READER_CAPS_BYTE1_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 2
	// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
	if (enh_cl_reader_caps[1] & AMEX_ENH_CL_READER_CAPS_MOBILE_CVM_SUPPORTED) {
		emv_str_list_add(&itr, "Mobile CVM supported");
	}
	if (enh_cl_reader_caps[1] & AMEX_ENH_CL_READER_CAPS_ONLINE_PIN_SUPPORTED) {
		emv_str_list_add(&itr, "Online PIN supported");
	}
	if (enh_cl_reader_caps[1] & AMEX_ENH_CL_READER_CAPS_SIGNATURE_SUPPORTED) {
		emv_str_list_add(&itr, "Signature supported");
	}
	if (enh_cl_reader_caps[1] & AMEX_ENH_CL_READER_CAPS_OFFLINE_PIN_SUPPORTED) {
		emv_str_list_add(&itr, "Plaintext Offline PIN supported");
	}
	if (enh_cl_reader_caps[1] & AMEX_ENH_CL_READER_CAPS_BYTE2_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 3
	// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
	if (enh_cl_reader_caps[2] & AMEX_ENH_CL_READER_CAPS_OFFLINE_ONLY_READER) {
		emv_str_list_add(&itr, "Reader is offline only");
	}
	if (enh_cl_reader_caps[2] & AMEX_ENH_CL_READER_CAPS_CVM_REQUIRED) {
		emv_str_list_add(&itr, "CVM Required");
	}
	if (enh_cl_reader_caps[2] & AMEX_ENH_CL_READER_CAPS_BYTE3_RFU) {
		emv_str_list_add(&itr, "RFU");
	}

	// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 4
	// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
	if (enh_cl_reader_caps[3] & AMEX_ENH_CL_READER_CAPS_EXEMPT_FROM_NO_CVM) {
		emv_str_list_add(&itr, "Terminal exempt from No CVM checks");
	}
	if (enh_cl_reader_caps[3] & AMEX_ENH_CL_READER_CAPS_DELAYED_AUTHORISATION) {
		emv_str_list_add(&itr, "Delayed Authorisation Terminal");
	}
	if (enh_cl_reader_caps[3] & AMEX_ENH_CL_READER_CAPS_TRANSIT) {
		emv_str_list_add(&itr, "Transit Terminal");
	}
	if (enh_cl_reader_caps[3] & AMEX_ENH_CL_READER_CAPS_BYTE4_RFU) {
		emv_str_list_add(&itr, "RFU");
	}
	switch (enh_cl_reader_caps[3] & AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_MASK) {
		case AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_22_23: emv_str_list_add(&itr, "C-4 kernel version 2.2 - 2.3"); break;
		case AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_24_26: emv_str_list_add(&itr, "C-4 kernel version 2.4 - 2.6"); break;
		case AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_27: emv_str_list_add(&itr, "C-4 kernel version 2.7"); break;
		default: emv_str_list_add(&itr, "C-4 kernel version unknown"); break;
	}

	return 0;
}

static const struct {
	char arc[3];
	const char* desc;
} emv_auth_response_code_map[] = {
	// See ISO 8583:2021, J.2.2.2, table J.3
	{ "00", "Approved or completed successfully" },
	{ "01", "Refer to card issuer" },
	{ "02", "Refer to card issuer's special conditions" },
	{ "03", "Invalid merchant" },
	{ "04", "Pick-up" },
	{ "05", "Do not honour" },
	{ "06", "Error" },
	{ "07", "Pick-up card, special condition" },
	{ "08", "Honour with identification" },
	{ "09", "Request in progress" },
	{ "0A", "No reason to decline" },
	{ "0B", "Approved but fees disputed" },
	{ "0C", "Approved, unable to process online" },
	{ "0D", "Approved, transaction processed offline" },
	{ "0E", "Approved, transaction processed offline after referral" },
	{ "10", "Approved for partial amount" },
	{ "11", "Approved (VIP)" },
	{ "12", "Invalid transaction" },
	{ "13", "Invalid amount" },
	{ "14", "Invalid card/cardholder number" },
	{ "15", "No such issuer (invalid IIN)" },
	{ "16", "Approved, update track 3" },
	{ "17", "Customer cancellation" },
	{ "18", "Customer dispute" },
	{ "19", "Re-enter transaction" },
	{ "1A", "Additional consumer authentication required" },
	{ "1B", "Cashback not allowed" },
	{ "1C", "Cashback amount exceeded" },
	{ "1D", "Surcharge amount not permitted for card product" },
	{ "1E", "Surcharge not permitted by selected network" },
	{ "1F", "Exceeds pre-authorized amount" },
	{ "1G", "Currency unacceptable to card issuer" },
	{ "1H", "Authorization lifecycle unacceptable" },
	{ "1I", "Authorization lifecycle has expired" },
	{ "1J", "Message sequence number error" },
	{ "1K", "Payment date invalid" },
	{ "1L", "Stop payment order - Specific pre-authorized payment" },
	{ "1M", "Stop payment order - All pre-authorized payments for merchant" },
	{ "1N", "Stop payment order - Account" },
	{ "1O", "Recurring data error" },
	{ "1P", "Scheduled transactions exist" },
	{ "1W", "Cheque already posted" },
	{ "1X", "Declined, unable to process offline" },
	{ "1Y", "Declined, transaction processed offline" },
	{ "1Z", "Declined, transaction processed offline after referral" },
	{ "20", "Invalid response" },
	{ "21", "No action taken" },
	{ "22", "Suspected malfunction" },
	{ "23", "Unacceptable transaction fee" },
	{ "24", "File update not supported by receiver" },
	{ "25", "Unable to locate record on file" },
	{ "26", "Duplicate file update record, old record replaced" },
	{ "27", "File update field edit error" },
	{ "28", "File update file locked out" },
	{ "29", "File update not successful" },
	{ "2A", "Duplicate, new record rejected" },
	{ "2B", "Unknown file" },
	{ "2C", "Invalid security code" },
	{ "2D", "Database error" },
	{ "2E", "Update not allowed" },
	{ "2F", "Not authorized and fees disputed" },
	{ "30", "Format error" },
	{ "31", "Acquirer bank not supported" },
	{ "32", "Completed partially" },
	{ "33", "Expired card" },
	{ "34", "Suspected fraud" },
	{ "35", "Card acceptor contact acquirer" },
	{ "36", "Restricted card" },
	{ "37", "Card acceptor call acquirer security" },
	{ "38", "Allowable PIN tries exceeded" },
	{ "39", "No credit account" },
	{ "3A", "Suspected counterfeit card, pick up card" },
	{ "3B", "Daily withdrawal uses exceeded" },
	{ "3C", "Daily withdrawal amount exceeded" },
	{ "40", "Requested function not supported" },
	{ "41", "Lost card, pick-up" },
	{ "42", "No universal account" },
	{ "43", "Stolen card, pick-up" },
	{ "44", "No investment account" },
	{ "45", "No account of type requested" },
	{ "46", "Closed account, or restricted for closing" },
	{ "47", "From account bad status" },
	{ "48", "To account bad status" },
	{ "49", "Bad debt" },
	{ "4A", "Card not effective" },
	{ "4B", "Closed savings account, or restricted for closing" },
	{ "4C", "Closed credit account or restricted for closing" },
	{ "4D", "Closed credit facility cheque account or restricted for closing" },
	{ "4E", "Closed cheque account or restricted for closing" },
	{ "51", "Not sufficient funds" },
	{ "52", "No chequing account" },
	{ "53", "No savings account" },
	{ "54", "Expired card" },
	{ "55", "Incorrect personal identification number" },
	{ "56", "No card record" },
	{ "57", "Transaction not permitted to cardholder" },
	{ "58", "Transaction not permitted to terminal" },
	{ "59", "Suspected fraud" },
	{ "5A", "Suspected counterfeit card" },
	{ "5B", "Transaction does not fulfill Anti-Money Laundering requirements" },
	{ "5C", "Transaction not supported by the card issuer" },
	{ "60", "Card acceptor contact acquirer" },
	{ "61", "Exceeds withdrawal amount limit" },
	{ "62", "Restricted card" },
	{ "63", "Security violation" },
	{ "64", "Original amount incorrect" },
	{ "65", "Exceeds withdrawal frequency limit" },
	{ "66", "Card acceptor call acquirer's security department" },
	{ "67", "Hard capture (requires that card be picked up at ATM)" },
	{ "68", "Response received too late" },
	{ "6P", "Verification data failed" },
	{ "6Q", "No communication keys available for use" },
	{ "6R", "MAC key sync error" },
	{ "6S", "MAC incorrect" },
	{ "6T", "Security software/hardware error - try again" },
	{ "6U", "Security software/hardware error - do not retry" },
	{ "6V", "Encryption key sync error" },
	{ "6W", "Key verification failed. Key check value does not match" },
	{ "6X", "Key sync error" },
	{ "6Y", "Missing required data to verify/process PIN" },
	{ "6Z", "Invalid PIN block" },
	{ "70", "PIN data required" },
	{ "71", "New PIN invalid" },
	{ "72", "PIN change required" },
	{ "73", "PIN is not allowed for transaction" },
	{ "74", "PIN length error" },
	{ "75", "Allowable number of PIN tries exceeded" },
	{ "8A", "Reconciled, in balance" },
	{ "8B", "Amount not reconciled, totals provided" },
	{ "8C", "Totals not available" },
	{ "8D", "Not reconciled, totals provided" },
	{ "8E", "Ineligible to receive financial position information" },
	{ "8F", "Reconciliation cutover or checkpoint error" },
	{ "8G", "Advice acknowledged, no financial liability accepted" },
	{ "8H", "Advice acknowledged, financial liability accepted" },
	{ "8I", "Message number out of sequence" },
	{ "8W", "Perform Stand-In Processing (STIP)" },
	{ "8X", "Currently unable to perform request; try later" },
	{ "8Y", "Card issuer signed off" },
	{ "8Z", "Card issuer timed out" },
	{ "90", "Cutoff is in process (switch ending a day's business and starting the next. Transaction can be sent again in a few minutes)" },
	{ "91", "Issuer or switch is unavailable or inoperative" },
	{ "92", "Financial institution or intermediate network facility cannot be found for routing" },
	{ "93", "Transaction cannot be completed. Violation of law" },
	{ "94", "Duplicate transmission" },
	{ "95", "Reconcile error" },
	{ "96", "System malfunction" },
	{ "9A", "Violation of business arrangement" },
	{ "9B", "No matching original transaction" },
	{ "9C", "Original transaction was declined" },
	{ "9D", "Bank not found" },
	{ "9E", "Bank not effective" },
	{ "9F", "Information not on file" },
};

static const char* emv_arc_get_desc(const char* arc)
{
	for (size_t i = 0; i < sizeof(emv_auth_response_code_map) / sizeof(emv_auth_response_code_map[0]); ++i) {
		if (strncmp(emv_auth_response_code_map[i].arc, arc, 2) == 0) {
			return emv_auth_response_code_map[i].desc;
		}
	}

	return NULL;
}

int emv_auth_response_code_get_string(
	const void* arc,
	size_t arc_len,
	char* str,
	size_t str_len
)
{
	int r;
	const char* arc_desc;

	if (!arc || !arc_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}
	str[0] = 0;

	if (str_len < 3) {
		// Insufficient length for 2-character Authorisation Response Code
		return -2;
	}

	if (arc_len != 2) {
		// Authorisation Response Code (field 8A) must be 2 bytes
		return 1;
	}

	// Stringify 2-byte Authorisation Response Code
	r = emv_format_an_get_string(arc, 2, str, str_len);
	if (r) {
		// Empty string on error
		str[0] = 0;
		return r;
	}

	if (str_len < 10) {
		// Insufficient length for Authorisation Response Code description
		// Output string contains 2-character Authorisation Response Code
		return 0;
	}

	// Append Authorisation Response Code description
	arc_desc = emv_arc_get_desc(arc);
	if (!arc_desc) {
		// Unknown Authorisation Response Code description
		// Output string contains 2-character Authorisation Response Code
		return 0;
	}
	r = snprintf(str + 2, str_len - 2, " - %s", arc_desc);
	if (r < 0) {
		// Unknown error; preserve 2-character Authorisation Response Code
		str[2] = 0;
		return -1;
	}

	return 0;
}

static int emv_csu_append_string_list(
	const uint8_t* csu,
	size_t csu_len,
	struct str_itr_t* itr
)
{
	if (!csu) {
		return -1;
	}
	if (csu_len != 4) {
		// See EMV 4.4 Book 3, Annex C10
		return -2;
	}

	// Card Status Update (CSU) byte 1
	// See EMV 4.4 Book 3, Annex C10
	if (csu[0] & EMV_CSU_BYTE1_PROPRIETARY_AUTHENTICATION_DATA_INCLUDED) {
		emv_str_list_add(itr, "Card Status Update (CSU): Proprietary Authentication Data Included");
	}
	if (csu[0] & EMV_CSU_BYTE1_PIN_TRY_COUNTER_MASK) {
		emv_str_list_add(itr, "Card Status Update (CSU): PIN Try Counter = %u", csu[0] & EMV_CSU_BYTE1_PIN_TRY_COUNTER_MASK);
	}

	// Card Status Update (CSU) byte 2
	// See EMV 4.4 Book 3, Annex C10
	if (csu[1] & EMV_CSU_BYTE2_ISSUER_APPROVES_ONLINE_TRANSACTION) {
		emv_str_list_add(itr, "Card Status Update (CSU): Issuer Approves Online Transaction");
	}
	if (csu[1] & EMV_CSU_BYTE2_CARD_BLOCK) {
		emv_str_list_add(itr, "Card Status Update (CSU): Card Block");
	}
	if (csu[1] & EMV_CSU_BYTE2_APPLICATION_BLOCK) {
		emv_str_list_add(itr, "Card Status Update (CSU): Application Block");
	}
	if (csu[1] & EMV_CSU_BYTE2_UPDATE_PIN_TRY_COUNTER) {
		emv_str_list_add(itr, "Card Status Update (CSU): Update PIN Try Counter");
	}
	if (csu[1] & EMV_CSU_BYTE2_GO_ONLINE_ON_NEXT_TXN) {
		emv_str_list_add(itr, "Card Status Update (CSU): Set Go Online on Next Transaction");
	}
	if (csu[1] & EMV_CSU_BYTE2_CREATED_BY_PROXY_FOR_ISSUER) {
		emv_str_list_add(itr, "Card Status Update (CSU): CSU Created by Proxy for the Issuer");
	}
	if ((csu[1] & EMV_CSU_BYTE2_UPDATE_COUNTERS_MASK) == EMV_CSU_BYTE2_UPDATE_COUNTERS_DO_NOT_UPDATE) {
		emv_str_list_add(itr, "Card Status Update (CSU): Do Not Update Offline Counters");
	}
	if ((csu[1] & EMV_CSU_BYTE2_UPDATE_COUNTERS_MASK) == EMV_CSU_BYTE2_UPDATE_COUNTERS_UPPER_OFFLINE_LIMIT) {
		emv_str_list_add(itr, "ard Status Update (CSU): Set Offline Counters to Upper Offline Limits");
	}
	if ((csu[1] & EMV_CSU_BYTE2_UPDATE_COUNTERS_MASK) == EMV_CSU_BYTE2_UPDATE_COUNTERS_RESET) {
		emv_str_list_add(itr, "Card Status Update (CSU): Reset Offline Counters to Zero");
	}
	if ((csu[1] & EMV_CSU_BYTE2_UPDATE_COUNTERS_MASK) == EMV_CSU_BYTE2_UPDATE_COUNTERS_ADD_TO_OFFLINE) {
		emv_str_list_add(itr, "Card Status Update (CSU): Add Transaction to Offline Counter");
	}

	// Card Status Update (CSU) byte 4
	// See EMV 4.4 Book 3, Annex C10
	if (csu[3] & EMV_CSU_BYTE4_ISSUER_DISCRETIONARY) {
		emv_str_list_add(itr, "Card Status Update (CSU): Issuer Discretionary 0x%02X", csu[3]);
	}

	// Card Status Update (CSU) RFU bits
	// See EMV 4.4 Book 3, Annex C10
	if (csu[0] & EMV_CSU_BYTE1_RFU ||
		csu[2] & EMV_CSU_BYTE3_RFU
	) {
		emv_str_list_add(itr, "Card Status Update (CSU): RFU");
	}

	return 0;
}

int emv_issuer_auth_data_get_string_list(
	const uint8_t* iad,
	size_t iad_len,
	char* str,
	size_t str_len
)
{
	int r;
	struct str_itr_t itr;
	char arc_str[3];

	if (!iad || !iad_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (iad_len < 8 || iad_len > 32) {
		// Issuer Authentication Data (field 91) must be 8 to 16 bytes.
		return 1;
	}

	emv_str_list_init(&itr, str, str_len);

	// Issuer Authentication Data (field 91) is determined by the issuer
	// while each payment scheme may have one or more different formats.
	// Therefore some guesing may be required to partially decode it.
	if (iad_len == 10) {
		r = emv_format_an_get_string(iad + 8, 2, arc_str, sizeof(arc_str));
		if (r == 0) {
			// Likely Visa CVN10 or Visa CVN17
			// 8-byte ARPC followed by 2-character ARPC Response Code resembling Authorisation Response Code
			// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
			emv_str_list_add(&itr, "Authorisation Response Cryptogram (ARPC): %02X%02X%02X%02X%02X%02X%02X%02X",
				iad[0], iad[1], iad[2], iad[3], iad[4], iad[5], iad[6], iad[7]
			);
			emv_str_list_add(&itr, "Authorisation Response Code: %s", arc_str);

			return 0;

		} else if ((iad[8] & 0xF0) == 0) { // Check for M/Chip RFU bits
			// Likely M/Chip
			// 8-byte ARPC followed by 2-byte ARPC Response code in M/Chip format
			// NOTE: From unverified internet sources
			emv_str_list_add(&itr, "Authorisation Response Cryptogram (ARPC): %02X%02X%02X%02X%02X%02X%02X%02X",
				iad[0], iad[1], iad[2], iad[3], iad[4], iad[5], iad[6], iad[7]
			);
			emv_str_list_add(&itr, "M/Chip ARPC Response Code: %02X%02X", iad[8], iad[9]);

			return 0;
		}
	}
	// Check for Card Status Update (CSU) RFU bits
	if ((iad[4] & EMV_CSU_BYTE1_RFU) == 0 &&
		(iad[6] & EMV_CSU_BYTE3_RFU) == 0
	) {
		// Likely CCD, Visa CVN18 or Visa CVN'22'
		// 4-byte ARPC followed by 4-byte CSU and optional Proprietary Authentication Data
		// See EMV 4.4 Book 2, 8.2.2
		// See EMV 4.4 Book 3, Annex C10
		// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
		emv_str_list_add(&itr, "Authorisation Response Cryptogram (ARPC): %02X%02X%02X%02X",
			iad[0], iad[1], iad[2], iad[3]
		);
		emv_csu_append_string_list(iad + 4, 4, &itr);

		if (iad_len > 8) { // If extra data after CSU
			if ((iad[4] & EMV_CSU_BYTE1_PROPRIETARY_AUTHENTICATION_DATA_INCLUDED)) { // CSU indicates Proprietary Authentication Data
				emv_str_list_add(&itr, "Proprietary Authentication Data: %zu bytes", iad_len - 8);
			} else if (iad_len == 10) {
				// Two extra bytes but not Proprietary Authentication Data
				r = emv_format_an_get_string(iad + 8, 2, arc_str, sizeof(arc_str));
				if (r == 0) {
					// Likely Visa CVN18 or Visa CVN'22' "third map issuers"
					// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
					emv_str_list_add(&itr, "Authorisation Response Code: %s", arc_str);
				}
			}
		}

		return 0;
	}

	return 0;
}

static int emv_capdu_get_data_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	if ((c_apdu[0] & ISO7816_CLA_PROPRIETARY) == 0 ||
		c_apdu[1] != 0xCA
	) {
		// Not GET DATA
		return -3;
	}

	// P1-P2 represents EMV field to retrieve
	// See EMV 4.4 Book 3, 6.5.7.2
	snprintf(str, str_len, "GET DATA field %02X%02X", c_apdu[2], c_apdu[3]);
	return 0;
}

int emv_capdu_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	if (!c_apdu || !c_apdu_len) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	str[0] = 0; // NULL terminate

	if (str_len < 4) {
		// C-APDU must be least 4 bytes
		// See EMV Contact Interface Specification v1.0, 9.4.1
		return -2;
	}

	if (c_apdu[0] == 0xFF) {
		// Class byte 'FF' is invalid
		// See EMV Contact Interface Specification v1.0, 9.4.1
		return 1;
	}

	if ((c_apdu[0] & ISO7816_CLA_PROPRIETARY) != 0) {
		// Proprietary class interpreted as EMV
		// Decode according to EMV 4.4 Book 3, 6.5

		// Decode INS byte
		const char* ins_str = NULL;
		switch (c_apdu[1]) {
			// See EMV 4.4 Book 3, 6.5.1.2
			case 0x1E: ins_str = "APPLICATION BLOCK"; break;
			// See EMV 4.4 Book 3, 6.5.2.2
			case 0x18: ins_str = "APPLICATION UNBLOCK"; break;
			// See EMV 4.4 Book 3, 6.5.3.2
			case 0x16: ins_str = "CARD BLOCK"; break;
			// See EMV 4.4 Book 3, 6.5.5.2
			case 0xAE: ins_str = "GENERATE AC"; break;
			// See EMV 4.4 Book 3, 6.5.7.2
			case 0xCA: return emv_capdu_get_data_get_string(c_apdu, c_apdu_len, str, str_len);
			// See EMV 4.4 Book 3, 6.5.8.2
			case 0xA8: ins_str = "GET PROCESSING OPTIONS"; break;
			// See EMV 4.4 Book 3, 6.5.10.2
			case 0x24: ins_str = "PIN CHANGE/UNBLOCK"; break;
		}

		if (!ins_str) {
			// Unknown command
			return 2;
		}

		strncpy(str, ins_str, str_len);
		str[str_len - 1] = 0;

		return 0;

	} else {
		const char* s;

		s = iso7816_capdu_get_string(
			c_apdu,
			c_apdu_len,
			str,
			str_len
		);
		if (!s) {
			// Failed to stringify ISO 7816 C-APDU
			str[0] = 0;
			return 3;
		}

		return 0;
	}
}
