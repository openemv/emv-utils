/**
 * @file iso8859_boost.c
 * @brief ISO/IEC 8859 implementation using Boost.Locale
 *
 * Copyright (c) 2023 Leon Lynch
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

#include <boost/locale.hpp>

#include <sstream>
#include <string>
#include <cstring>

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
	if (!iso8859 || !iso8859_len || !utf8 || !utf8_len) {
		return -1;
	}
	utf8[0] = 0;

	if (!iso8859_is_supported(codepage)) {
		return 1;
	}

	try {
		std::string iso8859_str(reinterpret_cast<const char*>(iso8859), iso8859_len);
		std::stringstream charset_stream;
		charset_stream << "ISO-8859-" << codepage;
		std::string utf8_str = boost::locale::conv::to_utf<char>(iso8859_str, charset_stream.str());
		if (utf8_str.empty()) {
			return 2;
		}

		std::strncpy(utf8, utf8_str.c_str(), utf8_len - 1);
		utf8[utf8_len - 1] = 0;

	} catch (const boost::locale::conv::invalid_charset_error &) {
		return -2;
	} catch (...) {
		return 3;
	}

	return 0;
}
