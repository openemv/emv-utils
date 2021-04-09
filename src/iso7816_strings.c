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
	}

	return 0;
}
