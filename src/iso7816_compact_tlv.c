/**
 * @file iso7816_compact_tlv.c
 * @brief ISO/IEC 7816 COMPACT-TLV implementation
 *
 * Copyright 2021 Leon Lynch
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

#include "iso7816_compact_tlv.h"

int iso7816_compact_tlv_decode(const void* buf, size_t len, struct iso7816_compact_tlv_t* tlv)
{
	const uint8_t* ptr = buf;

	if (!buf || !tlv) {
		return -1;
	}

	if (len == 0) {
		// End of data
		return 0;
	}

	// Decode first byte
	if (len < 1) {
		return -2;
	}
	tlv->tag = *ptr >> 4; // Tag is in upper 4 bits
	tlv->length = *ptr & 0xF; // Length is in lower 4 bits
	ptr += 1;
	len -= 1;

	// Decode value
	if (len < tlv->length) {
		return -3;
	}
	tlv->value = ptr;

	// Return number of bytes consumed
	return tlv->length + 1;
}

int iso7816_compact_tlv_itr_init(const void* buf, size_t len, struct iso7816_compact_tlv_itr_t* itr)
{
	if (!buf || !len || !itr) {
		return -1;
	}

	itr->ptr = buf;
	itr->len = len;

	return 0;
}

int iso7816_compact_tlv_itr_next(struct iso7816_compact_tlv_itr_t* itr, struct iso7816_compact_tlv_t* tlv)
{
	int r;

	if (!itr || !itr->ptr || !tlv) {
		return -1;
	}

	// Decode next element
	r = iso7816_compact_tlv_decode(itr->ptr, itr->len, tlv);
	if (r <= 0) {
		// Error or end of data
		return r;
	}

	// Advance iterator
	itr->ptr += r;
	itr->len -= r;

	return r;
}

const char* iso7816_compact_tlv_tag_get_string(uint8_t tag)
{
	switch (tag) {
		case ISO7816_COMPACT_TLV_COUNTRY_CODE: return "Country code";
		case ISO7816_COMPACT_TLV_IIN: return "Issuer identification number";
		case ISO7816_COMPACT_TLV_CARD_SERVICE_DATA: return "Card service data";
		case ISO7816_COMPACT_TLV_INITIAL_ACCESS_DATA: return "Initial access data";
		case ISO7816_COMPACT_TLV_CARD_ISSUER_DATA: return "Card issuer data";
		case ISO7816_COMPACT_TLV_PRE_ISSUING_DATA: return "Pre-issuing data";
		case ISO7816_COMPACT_TLV_CARD_CAPABILITIES: return "Card capabilities";
		case ISO7816_COMPACT_TLV_SI: return "Status indicator";
		case ISO7816_COMPACT_TLV_AID: return "Application identifier (AID)";
	}

	return "Unknown";
}
