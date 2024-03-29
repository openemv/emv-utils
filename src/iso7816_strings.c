/**
 * @file iso7816_strings.c
 * @brief ISO/IEC 7816 string helper functions
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

#include "iso7816_strings.h"

#include <stdio.h>
#include <string.h>

struct str_itr_t {
	char* ptr;
	size_t len;
};

static void iso7816_str_list_init(struct str_itr_t* itr, char* buf, size_t len)
{
	itr->ptr = buf;
	itr->len = len;
}

static void iso7816_str_list_add(struct str_itr_t* itr, const char* str)
{
	size_t str_len = strlen(str);

	// Ensure that the string, delimiter and NULL termination do not exceed
	// the remaining buffer length
	if (str_len + 2 > itr->len) {
		return;
	}

	// This is safe because we know str_len < itr->len
	memcpy(itr->ptr, str, str_len);

	// Delimiter
	itr->ptr[str_len] = '\n';
	++str_len;

	// Advance iterator
	itr->ptr += str_len;
	itr->len -= str_len;

	// NULL terminate string list
	*itr->ptr = 0;
}

const char* iso7816_sw1sw2_get_string(uint8_t SW1, uint8_t SW2, char* str, size_t str_len)
{
	int r;
	char* str_ptr = str;

	if (!str || !str_len) {
		// Invalid parameters
		return NULL;
	}

	// Normal processing (see ISO 7816-4:2005, 5.1.3)
	if (SW1 == 0x90 && SW2 == 0x00) {
		snprintf(str, str_len, "Normal");
		return str;
	}
	if (SW1 == 0x61) {
		snprintf(str, str_len, "Normal: %u data bytes remaining", SW2);
		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3:
	// Any value different from 6XXX and 9XXX is invalid
	// and any value 60XX is also invalid
	// Also see ISO 7816-3:2006, 10.3.3
	if ((SW1 & 0xF0) != 0x60 && (SW1 & 0xF0) != 0x90) {
		snprintf(str, str_len, "Invalid");
		return str;
	}
	if (SW1 == 0x60) {
		snprintf(str, str_len, "Invalid");
		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3:
	// 67XX, 6BXX, 6DXX, 6EXX, 6FXX and 9XXX are proprietary, except for
	// 6700, 6B00, 6D00, 6E00, 6F00 and 9000 that are interindustry
	if (((
			SW1 == 0x67 ||
			SW1 == 0x6B ||
			SW1 == 0x6D ||
			SW1 == 0x6E ||
			SW1 == 0x6F
		) && SW2 != 0x00) ||
		(SW1 & 0xF0) == 0x90 // 9000 is checked for earlier
	) {
		snprintf(str, str_len, "Proprietary");
		return str;
	}

	// Add prefix for high level meaning of SW1
	// See ISO 7816-4:2005, 5.1.3, table 5
	switch (SW1) {
		case 0x62:
		case 0x63:
			r = snprintf(str_ptr, str_len, "Warning: ");
			break;

		case 0x64:
		case 0x65:
		case 0x66:
			r = snprintf(str_ptr, str_len, "Execution error: ");
			break;

		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
			r = snprintf(str_ptr, str_len, "Checking error: ");
			break;

		default:
			// This should never happen
			return NULL;
	}
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str;
	}
	str_ptr += r;
	str_len -= r;

	// Warning processing (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x62) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory is unchanged"); break;
			case 0x81: snprintf(str_ptr, str_len, "Part of returned data may be corrupted"); break;
			case 0x82: snprintf(str_ptr, str_len, "End of file or record reached before reading Ne bytes"); break;
			case 0x83: snprintf(str_ptr, str_len, "Selected file deactivated"); break;
			case 0x84: snprintf(str_ptr, str_len, "File control information not formatted according to ISO 7816-4:2005, 5.3.3"); break;
			case 0x85: snprintf(str_ptr, str_len, "Selected file in termination state"); break;
			case 0x86: snprintf(str_ptr, str_len, "No input data available from a sensor on the card"); break;
			default:
				// Card-originated queries (see ISO 7816-4:2005, 8.6.1)
				if (SW2 >= 0x02 && SW2 <= 0x80) {
					snprintf(str_ptr, str_len, "Card-originated query of %u bytes", SW2);
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Warning processing (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x63) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory has changed"); break;
			case 0x81: snprintf(str_ptr, str_len, "File filled up by the last write"); break;
			default:
				if ((SW2 & 0xF0) == 0xC0) {
					snprintf(str_ptr, str_len, "Counter is %u", (SW2 & 0x0F));
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x64) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory is unchanged"); break;
			case 0x01: snprintf(str_ptr, str_len, "Immediate response required by card"); break;
			default:
				// Card-originated queries (see ISO 7816-4:2005, 8.6.1)
				if (SW2 >= 0x02 && SW2 <= 0x80) {
					snprintf(str_ptr, str_len, "Card-originated query of %u bytes", SW2);
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x65) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory has changed"); break;
			case 0x81: snprintf(str_ptr, str_len, "Memory failure"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x66) {
		snprintf(str_ptr, str_len, "Security error 0x%02X", SW2);
		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x67) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong length"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x68) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Functions in CLA not supported"); break;
			case 0x81: snprintf(str_ptr, str_len, "Logical channel not supported"); break;
			case 0x82: snprintf(str_ptr, str_len, "Secure messaging not supported"); break;
			case 0x83: snprintf(str_ptr, str_len, "Last command of the chain expected"); break;
			case 0x84: snprintf(str_ptr, str_len, "Command chaining not supported"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x69) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Command not allowed"); break;
			case 0x81: snprintf(str_ptr, str_len, "Command incompatible with file structure"); break;
			case 0x82: snprintf(str_ptr, str_len, "Security status not satisfied"); break;
			case 0x83: snprintf(str_ptr, str_len, "Authentication method blocked"); break;
			case 0x84: snprintf(str_ptr, str_len, "Reference data not usable"); break;
			case 0x85: snprintf(str_ptr, str_len, "Conditions of use not satisfied"); break;
			case 0x86: snprintf(str_ptr, str_len, "Command not allowed (no current EF)"); break;
			case 0x87: snprintf(str_ptr, str_len, "Expected secure messaging data objects missing"); break;
			case 0x88: snprintf(str_ptr, str_len, "Incorrect secure messaging data objects"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x6A) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong parameters P1-P2"); break;
			case 0x80: snprintf(str_ptr, str_len, "Incorrect parameters in the command data field"); break;
			case 0x81: snprintf(str_ptr, str_len, "Function not supported"); break;
			case 0x82: snprintf(str_ptr, str_len, "File or application not found"); break;
			case 0x83: snprintf(str_ptr, str_len, "Record not found"); break;
			case 0x84: snprintf(str_ptr, str_len, "Not enough memory space in the file"); break;
			case 0x85: snprintf(str_ptr, str_len, "Nc inconsistent with TLV structure"); break;
			case 0x86: snprintf(str_ptr, str_len, "Incorrect parameters P1-P2"); break;
			case 0x87: snprintf(str_ptr, str_len, "Nc inconsistent with parameters P1-P2"); break;
			case 0x88: snprintf(str_ptr, str_len, "Referenced data or reference data not found"); break;
			case 0x89: snprintf(str_ptr, str_len, "File already exists"); break;
			case 0x8A: snprintf(str_ptr, str_len, "DF name already exists"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6B) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong parameters P1-P2"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6C) {
		snprintf(str_ptr, str_len, "Wrong Le field (%u data bytes available)", SW2);
		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6D) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Instruction code not supported or invalid"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6E) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Class not supported"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6F) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "No precise diagnosis"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3, all other values are reserved
	snprintf(str, str_len, "Unknown error");
	return str;
}

const char* iso7816_lcs_get_string(uint8_t lcs)
{
	// See ISO 7816-4:2005, 5.3.3.2, table 13

	if (lcs == ISO7816_LCS_NONE) {
		return "No information given";
	}

	if (lcs == ISO7816_LCS_CREATION) {
		return "Creation state";
	}

	if (lcs == ISO7816_LCS_INITIALISATION) {
		return "Initialisation state";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_ACTIVATED) {
		return "Operational state (activated)";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_DEACTIVATED) {
		return "Operational state (deactivated)";
	}

	if ((lcs & ISO7816_LCS_TERMINATION_MASK) == ISO7816_LCS_TERMINATION) {
		return "Termination state";
	}

	return "Proprietary";
}

int iso7816_card_service_data_get_string_list(uint8_t card_service_data, char* str, size_t str_len)
{
	struct str_itr_t itr;

	iso7816_str_list_init(&itr, str, str_len);

	// Application selection (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	if ((card_service_data & ISO7816_CARD_SERVICE_APP_SEL_FULL_DF)) {
		iso7816_str_list_add(&itr, "Application selection: by full DF name");
	}
	if ((card_service_data & ISO7816_CARD_SERVICE_APP_SEL_PARTIAL_DF)) {
		iso7816_str_list_add(&itr, "Application selection: by partial DF name");
	}

	// BER-TLV data objects availability (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	if ((card_service_data & ISO7816_CARD_SERVICE_BER_TLV_EF_DIR)) {
		iso7816_str_list_add(&itr, "BER-TLV data objects available in EF.DIR");
	}
	if ((card_service_data & ISO7816_CARD_SERVICE_BER_TLV_EF_ATR)) {
		iso7816_str_list_add(&itr, "BER-TLV data objects available in EF.ATR");
	}

	// EF.DIR and EF.ATR access services (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	switch ((card_service_data & ISO7816_CARD_SERVICE_ACCESS_MASK)) {
		case ISO7816_CARD_SERVICE_ACCESS_READ_BINARY:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by READ BINARY command (transparent structure)");
			break;

		case ISO7816_CARD_SERVICE_ACCESS_READ_RECORD:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by READ RECORD(S) command (record structure)");
			break;

		case ISO7816_CARD_SERVICE_ACCESS_GET_DATA:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by GET DATA command (TLV structure)");
			break;

		default:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: Unknown value");
			break;
	}

	// Master file presence (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	switch ((card_service_data & ISO7816_CARD_SERVICE_MF_MASK)) {
		case ISO7816_CARD_SERVICE_WITHOUT_MF:
			iso7816_str_list_add(&itr, "Card without MF");
			break;

		case ISO7816_CARD_SERVICE_WITH_MF:
			iso7816_str_list_add(&itr, "Card with MF");
			break;

		default:
			return -1;
	}

	return 0;
}

int iso7816_card_capabilities_get_string_list(
	const uint8_t* card_capabilities,
	size_t card_capabilities_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!card_capabilities || !card_capabilities_len) {
		return -1;
	}

	iso7816_str_list_init(&itr, str, str_len);

	// DF selection (see ISO 7816-4:2005, 8.1.1.2.7, table 86)
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_FULL_DF)) {
		iso7816_str_list_add(&itr, "DF selection: by full DF NAME");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_PARTIAL_DF)) {
		iso7816_str_list_add(&itr, "DF selection: by partial DF NAME");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_PATH)) {
		iso7816_str_list_add(&itr, "DF selection: by path");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_FILE_ID)) {
		iso7816_str_list_add(&itr, "DF selection: by file identifier");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_IMPLICIT)) {
		iso7816_str_list_add(&itr, "DF selection: implicit");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_SHORT_EF_ID)) {
		iso7816_str_list_add(&itr, "Short EF identifier supported");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_RECORD_NUMBER)) {
		iso7816_str_list_add(&itr, "Record number supported");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_RECORD_ID)) {
		iso7816_str_list_add(&itr, "Record identifier supported");
	}

	// Data coding byte (see ISO 7816-4:2005, 8.1.1.2.7, table 86)
	if (card_capabilities_len < 2) {
		return 0;
	}
	if ((card_capabilities[1] & ISO7816_CARD_CAPS_EF_TLV)) {
		iso7816_str_list_add(&itr, "EFs of TLV structure supported");
	}

	switch ((card_capabilities[1] & ISO7816_CARD_CAPS_WRITE_FUNC_MASK)) {
		case ISO7816_CARD_CAPS_WRITE_FUNC_ONE_TIME:
			iso7816_str_list_add(&itr, "Behaviour of write functions: one-time write");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_PROPRIETARY:
			iso7816_str_list_add(&itr, "Behaviour of write functions: proprietary");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_OR:
			iso7816_str_list_add(&itr, "Behaviour of write functions: write OR");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_AND:
			iso7816_str_list_add(&itr, "Behaviour of write functions: write AND");
			break;

		default:
			return -1;
	}

	switch ((card_capabilities[1] & ISO7816_CARD_CAPS_BER_TLV_FF_MASK)) {
		case ISO7816_CARD_CAPS_BER_TLV_FF_VALID:
			iso7816_str_list_add(&itr, "FF as first byte of BER-TLV tag is valid");
			break;

		case ISO7816_CARD_CAPS_BER_TLV_FF_INVALID:
			iso7816_str_list_add(&itr, "FF as first byte of BER-TLV tag is invalid / padding");
			break;

		default:
			return -1;
	}

	unsigned int data_unit_size_field;
	data_unit_size_field = card_capabilities[1] & ISO7816_CARD_CAPS_DATA_UNIT_SIZE_MASK;
	if (data_unit_size_field) {
		unsigned int data_unit_size;

		// See ISO 7816-4:2005, 8.1.1.2.7, table 86
		// data_unit_size = 2 ^ data_unit_size_field quartets
		//                = (2 ^ data_unit_size_field) / 2 octets
		//                = 1 << (data_unit_size_field - 1) bytes
		data_unit_size = 1 << (data_unit_size_field - 1);

		// Stringify
		char tmp[32];
		snprintf(tmp, sizeof(tmp), "Data unit size: %u bytes", data_unit_size);

		iso7816_str_list_add(&itr, tmp);
	}

	// Command chaining, length fields, logical channels (see ISO 7816-4:2005, 8.1.1.2.7, table 87)
	if (card_capabilities_len < 3) {
		return 0;
	}
	if ((card_capabilities[2] & ISO7816_CARD_CAPS_COMMAND_CHAINING)) {
		iso7816_str_list_add(&itr, "Command chaining");
	}
	if ((card_capabilities[2] & ISO7816_CARD_CAPS_EXTENDED_LC_LE)) {
		iso7816_str_list_add(&itr, "Extended Lc and Le fields");
	}

	switch ((card_capabilities[2] & ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_MASK)) {
		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_CARD:
			iso7816_str_list_add(&itr, "Logical channel number assignment: by the card");
			break;

		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_IFD:
			iso7816_str_list_add(&itr, "Logical channel number assignment: by the interface device");
			break;

		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_NONE:
			iso7816_str_list_add(&itr, "No logical channel");
			break;

		default:
			return -1;
	}

	unsigned int max_logical_channels;
	max_logical_channels = card_capabilities[2] & ISO7816_CARD_CAPS_MAX_CHAN_MASK;
	if (max_logical_channels == 0x7) {
		iso7816_str_list_add(&itr, "Maximum number of logical channels: 8 or more");
	} else {
		// Stringify
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "Maximum number of logical channels: %u", max_logical_channels + 1);

		iso7816_str_list_add(&itr, tmp);
	}

	return 0;
}
