/**
 * @file emv_tlv.h
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

#ifndef EMV_TLV_H
#define EMV_TLV_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * EMV TLV field
 * @note This structure is intentionally identical to @ref iso8825_tlv_t
 */
typedef struct iso8825_tlv_t emv_tlv_t;

/**
 * EMV data element formats
 * @remark See EMV 4.3 Book 1, 4.3
 */
enum emv_format_t {
	/**
	 * Alphabetic data.
	 * Single character per byte.
	 * a-z, A-Z only.
	 */
	EMV_FORMAT_A,

	/**
	 * Alphanumeric data.
	 * Single character per byte.
	 * a-z, A-Z, 0-9 only.
	 */
	EMV_FORMAT_AN,

	/**
	 * Alphanumeric special data.
	 * Single character per byte.
	 * Encoded using ISO/IEC 8859 common character set.
	 *
	 * @note The Application Label
	 *       (@ref EMV_TAG_50_APPLICATION_LABEL) field is limited to
	 *       a-z, A-Z, 0-9 and the space special character.
	 *
	 * @note The Application Preferred Name
	 *       (@ref EMV_TAG_9F12_APPLICATION_PREFERRED_NAME) field
	 *       may encode non-control characters from the ISO/IEC 8859 part
	 *       designated by the Issuer Code Table Index field
	 *       (@ref EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX).
	 */
	EMV_FORMAT_ANS,

	/**
	 * Fixed length binary data.
	 * Encoded according to the field type.
	 */
	EMV_FORMAT_B,

	/**
	 * Compressed numeric data.
	 * Two decimal digits per byte.
	 * Left justified and padded with trailing 'F's.
	 */
	EMV_FORMAT_CN,

	/**
	 * Numeric data.
	 * Two decimal digits per byte.
	 * Right justified and padded with leading zeroes.
	 * Also referred to as Binary Coded Decimal (BCD) or Unsigned Packed
	 */
	EMV_FORMAT_N,

	/**
	 * Variable length binary data.
	 * Encoded according to the field type.
	 */
	EMV_FORMAT_VAR,
};

/**
 * EMV TLV information as human readable strings
 * @remark See EMV 4.3 Book 1, Annex B
 * @remark See EMV 4.3 Book 3, Annex A
 * @remark See ISO 7816-4:2005, 5.2.4
 */
struct emv_tlv_info_t {
	const char* tag_name;                       ///< Tag name, if available. Otherwise NULL.
	const char* tag_desc;                       ///< Tag description, if available. Otherwise NULL.
	enum emv_format_t format;                   ///< Value format. @see emv_format_t
};

/**
 * Retrieve EMV TLV information, if available
 * @param tlv Decoded EMV TLV structure
 * @param info EMV TLV information output. See @ref emv_tlv_info_t
 * @return Zero for success. Less than zero for error.
 */
int emv_tlv_get_info(const emv_tlv_t* tlv, struct emv_tlv_info_t* info);

__END_DECLS

#endif
