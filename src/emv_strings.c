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

#include <string.h>

// Helper functions
static int emv_tlv_value_get_string(const struct emv_tlv_t* tlv, enum emv_format_t format, size_t max_format_len, char* value_str, size_t value_str_len);

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

		case EMV_TAG_5F2D_LANGUAGE_PREFERENCE:
			info->tag_name = "Language Preference";
			info->tag_desc =
				"1-4 languages stored in order of preference, each "
				"represented by 2 alphabetical characters according to "
				"ISO 639";
			info->format = EMV_FORMAT_AN;
			return emv_tlv_value_get_string(tlv, info->format, 8, value_str, value_str_len);

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

		case EMV_TAG_9F06_AID:
			info->tag_name = "Application Identifier (AID) - terminal";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
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
			return 0;

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

		case EMV_TAG_9F33_TERMINAL_CAPABILITIES:
			info->tag_name = "Terminal Capabilities";
			info->tag_desc =
				"Indicates the card data input, CVM, and security "
				"capabilities of the terminal";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F35_TERMINAL_TYPE:
			info->tag_name = "Terminal Type";
			info->tag_desc =
				"Indicates the environment of the terminal, its "
				"communications capability, and its operational control";
			info->format = EMV_FORMAT_N;
			return emv_tlv_value_get_string(tlv, info->format, 2, value_str, value_str_len);

		case EMV_TAG_9F38_PDOL:
			info->tag_name = "Processing Options Data Object List (PDOL)";
			info->tag_desc =
				"Contains a list of terminal resident data objects (tags and "
				"lengths) needed by the ICC in processing the GET PROCESSING "
				"OPTIONS command";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
			info->tag_name = "Additional Terminal Capabilities";
			info->tag_desc =
				"Indicates the data input and output capabilities of the "
				"terminal";
			info->format = EMV_FORMAT_B;
			return 0;

		case EMV_TAG_9F4D_LOG_ENTRY:
			info->tag_name = "Log Entry";
			info->tag_desc =
				"Provides the SFI of the Transaction Log file and its number "
				"of records";
			info->format = EMV_FORMAT_B;
			return 0;

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

static int emv_tlv_value_get_string(const struct emv_tlv_t* tlv, enum emv_format_t format, size_t max_format_len, char* value_str, size_t value_str_len)
{
	if (!tlv || !value_str || !value_str_len) {
		// Caller didn't want the value string
		return 0;
	}

	// Validate max format length
	if (max_format_len) {
		if (tlv->length > max_format_len) {
			// Value exceeds maximum format length
			return -2;
		}

		if (value_str_len < max_format_len + 1) { // +1 is for NULL terminator
			// Value string output buffer is shorter than maximum format length
			return -3;
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

		case EMV_FORMAT_CN:
		case EMV_FORMAT_N:
			// Convert BCD digits to ASCII numbers
			for (unsigned int i = 0; i < tlv->length; ++i) {
				uint8_t nibble;

				// Convert most significant nibble
				nibble = tlv->value[i] >> 4;
				if (nibble > 9) {
					// Invalid nibble or padding; halt
					break;
				}
				value_str[(i * 2)] = '0' + nibble;

				// Convert least significant nibble
				nibble = tlv->value[i] & 0xf;
				if (nibble > 9) {
					// Invalid nibble or padding; halt
					break;
				}
				value_str[(i * 2) + 1] = '0' + nibble;
			}
			value_str[tlv->length * 2] = 0; // NULL terminate
			return 0;

		default:
			// Unknown format
			return -4;
	}
}
