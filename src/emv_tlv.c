/**
 * @file emv_tlv.c
 * @brief EMV TLV structures and helper functions
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

#include "emv_tlv.h"
#include "emv_tags.h"
#include "iso8825_ber.h"

#include <string.h>

int emv_tlv_get_info(const emv_tlv_t* tlv, struct emv_tlv_info_t* info)
{
	if (!tlv || !info) {
		return -1;
	}

	memset(info, 0, sizeof(*info));

	switch (tlv->tag) {
		case EMV_TAG_42_IIN:
			info->tag_name = "Issuer Identification Number (IIN)";
			info->tag_desc =
				"The number that identifies the major industry and the card "
				"issuer and that forms the first part of the Primary Account "
				"Number (PAN)";
			return 0;

		case EMV_TAG_4F_APPLICATION_DF_NAME:
			info->tag_name = "Application Dedicated File (ADF) Name";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_50_APPLICATION_LABEL:
			info->tag_name = "Application Label";
			info->tag_desc =
				"Mnemonic associated with the AID according to ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_61_APPLICATION_TEMPLATE:
			info->tag_name = "Application Template";
			info->tag_desc =
				"Contains one or more data objects relevant to an application "
				"directory entry according to ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_6F_FCI_TEMPLATE:
			info->tag_name = "File Control Information (FCI) Template";
			info->tag_desc =
				"Identifies the FCI template according to ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_70_DATA_TEMPLATE:
			info->tag_name = "EMV Data Template";
			info->tag_desc = "Contains EMV data";
			return 0;

		case EMV_TAG_73_DIRECTORY_DISCRETIONARY_TEMPLATE:
			info->tag_name = "Directory Discretionary Template";
			info->tag_desc =
				"Issuer discretionary part of the directory according to "
				"ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_84_DF_NAME:
			info->tag_name = "Dedicated File (DF) Name";
			info->tag_desc =
				"Identifies the name of the Dedicated File (DF) as described "
				"in ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR:
			info->tag_name = "Application Priority Indicator";
			info->tag_desc =
				"Indicates the priority of a given application or group of "
				"applications in a directory";
			return 0;

		case EMV_TAG_88_SFI:
			info->tag_name = "Short File Indicator (SFI)";
			info->tag_desc =
				"Identifies the Application Elementary File (AEF) referenced "
				"in commands related to a given Application Definition File "
				"or Directory Definition File (DDF). It is a binary data "
				"object having a value in the range 1 - 30 and with the three "
				"high order bits set to zero.";
			return 0;

		case EMV_TAG_9D_DDF_NAME:
			info->tag_name = "Directory Definition File (DDF) Name";
			info->tag_desc =
				"Identifies the name of a Dedicated File (DF) associated with "
				"a directory";
			return 0;

		case EMV_TAG_A5_FCI_PROPRIETARY_TEMPLATE:
			info->tag_name = "File Control Information (FCI) Proprietary Template";
			info->tag_desc =
				"Identifies the data object proprietary to this specification "
				"in the File Control Information (FCI) template according to "
				"ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_5F2D_LANGUAGE_PREFERENCE:
			info->tag_name = "Language Preference";
			info->tag_desc =
				"1-4 languages stored in order of preference, each "
				"represented by 2 alphabetical characters according to "
				"ISO 639";
			return 0;

		case EMV_TAG_5F50_ISSUER_URL:
			info->tag_name = "Issuer URL";
			info->tag_desc =
				"The URL provides the location of the issuer's Library Server "
				"on the Internet";
			return 0;

		case EMV_TAG_5F53_IBAN:
			info->tag_name = "International Bank Account Number (IBAN)";
			info->tag_desc =
				"Uniquely identifies the account of a customer at a financial "
				"institution as defined in ISO 13616.";
			return 0;

		case EMV_TAG_5F54_BANK_IDENTIFIER_CODE:
			info->tag_name = "Bank Identifier Code (BIC)";
			info->tag_desc =
				"Uniquely identifies a bank as defined in ISO 9362.";
			return 0;

		case EMV_TAG_5F55_ISSUER_COUNTRY_CODE_ALPHA2:
			info->tag_name = "Issuer Country Code (alpha2 format)";
			info->tag_desc =
				"Indicates the country of the issuer as defined in ISO 3166 "
				"(using a 2 character alphabetic code)";
			return 0;

		case EMV_TAG_5F56_ISSUER_COUNTRY_CODE_ALPHA3:
			info->tag_name = "Issuer Country Code (alpha3 format)";
			info->tag_desc =
				"Indicates the country of the issuer as defined in ISO 3166 "
				"(using a 3 character alphabetic code)";
			return 0;

		case EMV_TAG_9F06_AID:
			info->tag_name = "Application Identifier (AID) - terminal";
			info->tag_desc =
				"Identifies the application as described in ISO/IEC 7816-4";
			return 0;

		case EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX:
			info->tag_name = "Issuer Code Table Index";
			info->tag_desc =
				"Indicates the code table according to ISO/IEC 8859 for "
				"displaying the Application Preferred Name";
			return 0;

		case EMV_TAG_9F12_APPLICATION_PREFERRED_NAME:
			info->tag_name = "Application Preferred Name";
			info->tag_desc =
				"Preferred mnemonic associated with the AID";
			return 0;

		case EMV_TAG_9F38_PDOL:
			info->tag_name = "Processing Options Data Object List (PDOL)";
			info->tag_desc =
				"Contains a list of terminal resident data objects (tags and "
				"lengths) needed by the ICC in processing the GET PROCESSING "
				"OPTIONS command";
			return 0;

		case EMV_TAG_9F4D_LOG_ENTRY:
			info->tag_name = "Log Entry";
			info->tag_desc =
				"Provides the SFI of the Transaction Log file and its number "
				"of records";
			return 0;

		case EMV_TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA:
			info->tag_name = "File Control Information (FCI) Issuer Discretionary Data";
			info->tag_desc =
				"Issuer discretionary part of the File Control Information (FCI)";
			return 0;

		default:
			return 1;
	}
}
