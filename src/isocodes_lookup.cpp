/**
 * @file isocodes_lookup.cpp
 * @brief Wrapper for iso-codes package
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

#include "isocodes_lookup.h"

#include <memory>
#include <cstdio>

#include <boost/json.hpp>

static boost::json::value parse_json_file(char const* filename)
{
	std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(filename, "rb"), &std::fclose);
	if (!file) {
		// Failed to open file
		return nullptr;
	}

	boost::json::stream_parser p;
	boost::json::error_code ec;
	while (!std::feof(file.get())) {
		char buf[4096];
		size_t len;

		// Read from file
		len = std::fread(buf, 1, sizeof(buf), file.get());

		// JSON parsing
		p.write(buf, len, ec);
		if (ec) {
			// JSON parsing error
			return nullptr;
		}
	}

	p.finish(ec);
	if(ec) {
		// JSON validation error
		return nullptr;
	}

	// Caller takes ownership of JSON object
	return p.release();
}

int isocodes_init(void)
{
	boost::json::value jv;

	try {
		jv = parse_json_file("/usr/share/iso-codes/json/iso_4217.json");
		if (jv.kind() == boost::json::kind::null) {
			return 1;
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Exception: %s\n", e.what());
		return -1;
	}

	return 0;
}
