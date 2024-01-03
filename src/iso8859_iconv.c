/**
 * @file iso8859_iconv.c
 * @brief ISO/IEC 8859 implementation using iconv
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

#include <stdio.h>
#include <iconv.h>

bool iso8859_is_supported(unsigned int codepage)
{
	if (codepage < 1 ||
		codepage > 15 ||
		codepage == 12
	) {
		// ISO 8859 code pages 1 to 15 are supported
		// ISO 8859-12 for Devanagari was officially abandoned in 1997
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
	char fromcode[12]; // ISO-8859-XX\0
	iconv_t cd;
	size_t r;
	char* inbuf = (char*)iso8859;
	size_t inbytesleft = iso8859_len;
	char* outbuf = utf8;
	size_t outbytesleft = utf8_len;

	if (!iso8859 || !iso8859_len || !utf8 || !utf8_len) {
		return -1;
	}
	utf8[0] = 0;

	if (!iso8859_is_supported(codepage)) {
		return 1;
	}

	snprintf(fromcode, sizeof(fromcode), "ISO-8859-%u", codepage);
	cd = iconv_open("UTF-8", fromcode);
	if (cd == (iconv_t)-1) {
		return -2;
	}
	r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	iconv_close(cd);
	if (r == (size_t)-1) {
		return 2;
	}

	return 0;
}
