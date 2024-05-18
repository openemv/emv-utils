/**
 * @file iso8859.h
 * @brief ISO/IEC 8859 implementation
 *
 * Copyright 2023 Leon Lynch
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

#ifndef ISO8859_H
#define ISO8859_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

/**
 * Determine whether ISO/IEC 8859 code page is supported by this implementation
 * @param codepage Code page number. Must be from 1 to 15.
 * @return Boolean indicating whether code page is supported
 */
bool iso8859_is_supported(unsigned int codepage);

/**
 * Convert from ISO/IEC 8859 to UTF-8 using the specified code page.
 * @param codepage Code page number. Must be from 1 to 15.
 * @param iso8859 Buffer containing ISO/IEC 8859 encoded string
 * @param iso8859_len Length of ISO/IEC 8859 buffer in bytes
 * @param utf8 UTF-8 buffer output
 * @param utf8_len Length of UTF-8 buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso8859_to_utf8(
	unsigned int codepage,
	const uint8_t* iso8859,
	size_t iso8859_len,
	char* utf8,
	size_t utf8_len
);

__END_DECLS

#endif
