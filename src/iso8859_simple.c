/**
 * @file iso8859_simple.c
 * @brief Simple ISO/IEC 8859-1 implementation
 *
 * Copyright (c) 2024 Leon Lynch
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

#include "iso8859.h"

bool iso8859_is_supported(unsigned int codepage)
{
	if (codepage != 1) {
		// Only ISO 8859 code pages 1 is supported
		return false;
	}

	return true;
}

int iso8859_to_utf8(
	unsigned int codepage,
	const uint8_t* iso8859,
	size_t iso8859_len,
	char* utf8,
	size_t utf8_len
)
{
	size_t utf8_idx;

	if (!iso8859 || !iso8859_len || !utf8 || !utf8_len) {
		return -1;
	}
	utf8[0] = 0;

	if (!iso8859_is_supported(codepage)) {
		return 1;
	}

	// Repack ISO 8859-1 as UTF-8
	utf8_idx = 0;
	for (size_t i = 0; i < iso8859_len; ++i) {
		if (!iso8859[i]) {
			// Null termination
			break;
		}

		// Ensure that at least 2 output bytes are available
		if (utf8_len - utf8_idx < 2) {
			// Unsufficient space left in output buffer
			break;
		}

		if (iso8859[i] < 0x20 || (iso8859[i] > 0x7E && iso8859[i] < 0xA0)) {
			// Reject non-printable characters
			return 2;
		}

		if (iso8859[i] < 0x80) {
			// Copy common character set verbatim to UTF-8
			utf8[utf8_idx++] = iso8859[i];
		} else {
			// Encode higher non-control characters
			utf8[utf8_idx++] = 0xC0 | (iso8859[i] >> 6);
			utf8[utf8_idx++] = 0x80 | (iso8859[i] & 0x3F);
		}
	}

	// Terminate UTF-8 string
	utf8[utf8_idx] = 0;

	return 0;
}
