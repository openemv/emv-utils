/**
 * @file iso7816_compact_tlv.h
 * @brief ISO/IEC 7816 COMPACT-TLV implementation
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

#ifndef ISO7816_COMPACT_TLV_H
#define ISO7816_COMPACT_TLV_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

#define ISO7816_COMPACT_TLV_COUNTRY_CODE        (0x1) ///< Country code (see ISO 3166-1)
#define ISO7816_COMPACT_TLV_IIN                 (0x2) ///< Issuer identification number (see ISO 7812-1)
#define ISO7816_COMPACT_TLV_CARD_SERVICE_DATA   (0x3) ///< Card service data (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
#define ISO7816_COMPACT_TLV_INITIAL_ACCESS_DATA (0x4) ///< Initial access data (see ISO 7816-4:2005, 8.1.1.2.4)
#define ISO7816_COMPACT_TLV_CARD_ISSUER_DATA    (0x5) ///< Card issuer data
#define ISO7816_COMPACT_TLV_PRE_ISSUING_DATA    (0x6) ///< Pre-issuing data
#define ISO7816_COMPACT_TLV_CARD_CAPABILITIES   (0x7) ///< Card capabilities (see ISO 7816-4:2005, 8.1.1.2.7, table 86/87/88)
#define ISO7816_COMPACT_TLV_SI                  (0x8) ///< Status indicator (see ISO 7816-4:2005, 8.1.1.3)
#define ISO7816_COMPACT_TLV_AID                 (0xF) ///< Application identifier (see ISO 7816-4:2005, 8.1.1.2.2)

/// ISO/IEC 7816 COMPACT-TLV iterator
struct iso7816_compact_tlv_itr_t {
	const void* ptr;
	size_t len;
};

/// ISO/IEC 7816 COMPACT-TLV element
struct iso7816_compact_tlv_t {
	uint8_t tag; ///< Tag number
	uint8_t length; ///< Length of @c value in bytes
	const uint8_t* value; ///< Pointer to value
};

/**
 * Decode COMPACT-TLV element
 * @param buf COMPACT-TLV buffer
 * @param len Length of COMPACT-TLV buffer in bytes
 * @param tlv Decoded COMPACT-TLV element output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int iso7816_compact_tlv_decode(const void* buf, size_t len, struct iso7816_compact_tlv_t* tlv);

/**
 * Initialise COMPACT-TLV iterator
 * @param buf COMPACT-TLV buffer
 * @param len Length of COMPACT-TLV buffer in bytes
 * @param itr Iterator output
 * @return Zero for success. Non-zero for error.
 */
int iso7816_compact_tlv_itr_init(const void* buf, size_t len, struct iso7816_compact_tlv_itr_t* itr);

/**
 * Retrieve next COMPACT-TLV element and advance iterator
 * @param itr Iterator
 * @param tlv COMPACT-TLV element output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int iso7816_compact_tlv_itr_next(struct iso7816_compact_tlv_itr_t* itr, struct iso7816_compact_tlv_t* tlv);

/**
 * Stringify COMPACT-TLV element's tag
 * @param tag COMPACT-TLV tag
 * @return String. NULL for error.
 */
const char* iso7816_compact_tlv_tag_get_string(uint8_t tag);

__END_DECLS

#endif
