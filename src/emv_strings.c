/**
 * @file emv_strings.c
 * @brief EMV string helper functions
 *
 * Copyright (c) 2021 Leon Lynch
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

#include <string.h>
#include <stdarg.h>
#include <stdio.h> // for vsnprintf() and snprintf()

struct str_itr_t {
	char* ptr;
	size_t len;
};

// Helper functions
static int emv_tlv_value_get_string(const struct emv_tlv_t* tlv, enum emv_format_t format, size_t max_format_len, char* value_str, size_t value_str_len);
static int emv_uint_to_str(uint32_t value, char* str, size_t str_len);
static void emv_str_list_init(struct str_itr_t* itr, char* buf, size_t len);
static void emv_str_list_add(struct str_itr_t* itr, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static const char* emv_cvm_code_get_string(uint8_t cvm_code);
static int emv_cvm_cond_code_get_string(uint8_t cvm_cond_code, const struct emv_cvmlist_amounts_t* amounts, char* str, size_t str_len);

int emv_tlv_get_info(
	const struct emv_tlv_t* tlv,
	struct emv_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
)
{
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
			return 0;

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
			return 0;

		case EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR:
			info->tag_name = "Application Priority Indicator";
			info->tag_desc =
				"Indicates the priority of a given application or group of "
				"applications in a directory";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_88_SFI:
			info->tag_name = "Short File Indicator (SFI)";
			info->tag_desc =
				"Identifies the Application Elementary File (AEF) referenced "
				"in commands related to a given Application Definition File "
				"or Directory Definition File (DDF). It is a binary data "
				"object having a value in the range 1 - 30 and with the three "
				"high order bits set to zero.";
			info->format = EMV_FORMAT_B;
			return 0;

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
			info->tag_name = "Certification Authority Public Key Index";
			info->tag_desc =
				"Identifies the certification authority’s public key in "
				"conjunction with the RID";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE:
			info->tag_name = "Issuer Public Key Certificate";
			info->tag_desc =
				"Issuer public key certified by a certification authority";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER:
			info->tag_name = "Issuer Public Key Remainder";
			info->tag_desc =
				"Remaining digits of the Issuer Public Key Modulus";
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
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

		case EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE:
			info->tag_name = "Transaction Currency Code";
			info->tag_desc =
				"Indicates the currency code of the transaction according to "
				"ISO 4217";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

		case EMV_TAG_5F2D_LANGUAGE_PREFERENCE:
			info->tag_name = "Language Preference";
			info->tag_desc =
				"1-4 languages stored in order of preference, each "
				"represented by 2 alphabetical characters according to "
				"ISO 639";
			info->format = EMV_FORMAT_AN;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

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
			return emv_tlv_value_get_string(tlv, EMV_FORMAT_CN, 34, value_str, value_str_len);

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
			return emv_tlv_value_get_string(tlv, info->format, 2, value_str, value_str_len);

		case EMV_TAG_5F56_ISSUER_COUNTRY_CODE_ALPHA3:
			info->tag_name = "Issuer Country Code (alpha3 format)";
			info->tag_desc =
				"Indicates the country of the issuer as defined in ISO 3166 "
				"(using a 3 character alphabetic code)";
			info->format = EMV_FORMAT_A;
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

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

		case EMV_TAG_9F06_AID:
			info->tag_name = "Application Identifier (AID) - terminal";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F07_APPLICATION_USAGE_CONTROL:
			info->tag_name = "Application Usage Control";
			info->tag_desc =
				"Indicates issuer’s specified restrictions on the geographic "
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

		case EMV_TAG_9F16_MERCHANT_IDENTIFIER:
			info->tag_name = "Merchant Identifier";
			info->tag_desc =
				"When concatenated with the Acquirer Identifier, uniquely "
				"identifies a given merchant";
			info->format = EMV_FORMAT_ANS;
			return emv_tlv_value_get_string(tlv, info->format, 15, value_str, value_str_len);

		case EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE:
			info->tag_name = "Terminal Country Code";
			info->tag_desc =
				"Indicates the country of the terminal, represented according "
				"to ISO 3166";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

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

		case EMV_TAG_9F21_TRANSACTION_TIME:
			info->tag_name = "Transaction Time";
			info->tag_desc =
				"Local time that the transaction was authorised";
			info->format = EMV_FORMAT_N;
			return emv_time_get_string(tlv->value, tlv->length, value_str, value_str_len);

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
			return 0;

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
			return emv_tlv_value_get_string(tlv, info->format, 3, value_str, value_str_len);

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
		case EMV_FORMAT_AN:
		case EMV_FORMAT_ANS:
			// TODO: validate format 'a' characters
			// TODO: validate format 'an' characters
			// TODO: validate format 'ans' characters in accordance with ISO/IEC 8859 common character, EMV 4.3 Book 4, Annex B
			// TODO: validate EMV_TAG_50_APPLICATION_LABEL in accordance with EMV 4.3 Book 3, 4.3
			// TODO: convert EMV_TAG_9F12_APPLICATION_PREFERRED_NAME from the appropriate ISO/IEC 8859 code page to UTF-8

			// For now assume that the field bytes are valid ASCII and are
			// only the allowed characters specified in EMV 4.3 Book 3, 4.3
			memcpy(value_str, tlv->value, tlv->length);
			value_str[tlv->length] = 0; // NULL terminate
			return 0;

		case EMV_FORMAT_CN: {
			r = emv_format_cn_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Ignore parse error
				value_str[0] = 0;
				return 0;
			}

			return 0;
		}

		case EMV_FORMAT_N: {
			r = emv_format_n_get_string(tlv->value, tlv->length, value_str, value_str_len);
			if (r) {
				// Ignore parse error
				value_str[0] = 0;
				return 0;
			}

			return 0;
		}

		default:
			// Unknown format
			return -6;
	}
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

static void emv_str_list_init(struct str_itr_t* itr, char* buf, size_t len)
{
	itr->ptr = buf;
	itr->len = len;
}

__attribute__((format(printf, 2, 3)))
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
	// See EMV 4.3 Book 4, Annex A1, table 24
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
	// See EMV 4.3 Book 4, Annex A1, table 24
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
	// See EMV 4.3 Book 4, Annex A2, table 25
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
	// See EMV 4.3 Book 4, Annex A2, table 26
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
		emv_str_list_add(&itr, "CVM Capability: Signature (paper)");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE)) {
		emv_str_list_add(&itr, "CVM Capability: Enciphered PIN for offline verification");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_NO_CVM)) {
		emv_str_list_add(&itr, "CVM Capability: No CVM required");
	}
	if ((term_caps[1] & EMV_TERM_CAPS_CVM_RFU)) {
		emv_str_list_add(&itr, "CVM Capability: RFU");
	}

	// Security Capability
	// See EMV 4.3 Book 4, Annex A2, table 27
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

		case EMV_POS_ENTRY_MODE_BARCODE:
			strncpy(str, "Barcode", str_len);
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
	// See EMV 4.3 Book 4, Annex A3, table 28
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
	// See EMV 4.3 Book 4, Annex A3, table 29
	if ((addl_term_caps[1] & EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH_DEPOSIT)) {
		emv_str_list_add(&itr, "Transaction Type Capability: Cash deposit");
	}
	if ((addl_term_caps[1] & EMV_ADDL_TERM_CAPS_TXN_TYPE_RFU)) {
		emv_str_list_add(&itr, "Transaction Type Capability: RFU");
	}

	// Terminal Data Input Capability (byte 3)
	// See EMV 4.3 Book 4, Annex A3, table 30
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_NUMERIC_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Numeric keys");
	}
	if ((addl_term_caps[2] & EMV_ADDL_TERM_CAPS_INPUT_ALPHABETIC_AND_SPECIAL_KEYS)) {
		emv_str_list_add(&itr, "Terminal Data Input Capability: Alphabetic and special character keys");
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
	// See EMV 4.3 Book 4, Annex A3, table 31
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_ATTENDANT)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Print, attendant");
	}
	if ((addl_term_caps[3] & EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_CARDHOLDER)) {
		emv_str_list_add(&itr, "Terminal Data Output Capability: Print, cardholder");
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
	// See EMV 4.3 Book 4, Annex A3, table 32
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
	// See EMV 4.3 Book 3, Annex C1, Table 37
	// See EMV Contactless Book C-2 v2.10, Annex A.1.16
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
	if (aip[1] & EMV_AIP_EMV_MODE_SUPPORTED) {
		emv_str_list_add(&itr, "Contactless EMV mode is supported");
	}
	if (aip[1] & EMV_RRP_SUPPORTED) {
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
	// See EMV 4.3 Book 3, Annex C2, Table 38
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
	// See EMV 4.3 Book 3, Annex C2, Table 38
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
	cvm_str = emv_cvm_code_get_string(cvmresults[0]);
	if (!cvm_str) {
		return -1;
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
