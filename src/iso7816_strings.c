/**
 * @file iso7816_strings.c
 * @brief ISO/IEC 7816 string helper functions
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
