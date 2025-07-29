/**
 * @file iso8825_strings.h
 * @brief ISO/IEC 8825 string helper functions
 *
 * Copyright 2024 Leon Lynch
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

#ifndef ISO8825_STRINGS_H
#define ISO8825_STRINGS_H

#include <sys/cdefs.h>
#include <stddef.h>

// Forward declarations
struct iso8825_tlv_t;

__BEGIN_DECLS

/**
 * ISO 8825 TLV information as human readable strings
 * @remark See ISO 8824-1:2021, 8.4, table 1
 */
struct iso8825_tlv_info_t {
	const char* tag_name;       ///< Tag name, if available. Otherwise NULL.
	const char* tag_desc;       ///< Tag description, if available. Otherwise NULL.
};

/**
 * Retrieve ISO 8825 TLV information, if available, and convert value to human
 * readable UTF-8 string(s), if possible.
 * @note @c value_str output will be empty if human readable string is not available
 *
 * @param tlv Decoded TLV structure
 * @param info TLV information output. See @ref iso8825_tlv_info_t
 * @param value_str Value string buffer output. NULL to ignore.
 * @param value_str_len Length of value string buffer in bytes. Zero to ignore.
 * @return Zero for success. Non-zero for error.
 */
int iso8825_tlv_get_info(
	const struct iso8825_tlv_t* tlv,
	struct iso8825_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
);

#endif
