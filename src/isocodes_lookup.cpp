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

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdio>

#include <boost/json.hpp>

struct isocodes_country_t {
	std::string name;
	std::string alpha2;
	std::string alpha3;
	std::string numeric;
};
static std::vector<isocodes_country_t> country_list;
static std::map<std::string,const isocodes_country_t&> country_alpha2_map;
static std::map<std::string,const isocodes_country_t&> country_alpha3_map;
static std::map<unsigned int,const isocodes_country_t&> country_numeric_map;

struct isocodes_currency_t {
	std::string name;
	std::string alpha3;
	std::string numeric;
};
static std::vector<isocodes_currency_t> currency_list;
static std::map<std::string,const isocodes_currency_t&> currency_alpha3_map;
static std::map<unsigned int,const isocodes_currency_t&> currency_numeric_map;


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

// Conversion function invoked by Boost to build isocodes_country_t from JSON object
static isocodes_country_t tag_invoke(boost::json::value_to_tag<isocodes_country_t>, const boost::json::value& jv)
{
	const auto& obj = jv.as_object();
	return isocodes_country_t {
		boost::json::value_to<std::string>(obj.at("name")),
		boost::json::value_to<std::string>(obj.at("alpha_2")),
		boost::json::value_to<std::string>(obj.at("alpha_3")),
		boost::json::value_to<std::string>(obj.at("numeric")),
	};
}

static bool build_country_list(const boost::json::value& jv) noexcept
{
	/* iso-codes package's iso_3166-1.json file should have this structure
	{
		"3166-1": [
			...
			{
				"alpha_2": "NL",
				"alpha_3": "NLD",
				"name": "Netherlands",
				"numeric": "528",
				"official_name": "Kingdom of the Netherlands"
			},
			...
		]
	}
	*/

	try {
		const auto& iso3166_1 = jv.as_object();
		const auto& iso3166_1_list = iso3166_1.at("3166-1");
		country_list = boost::json::value_to<decltype(country_list)>(iso3166_1_list);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Exception: %s\n", e.what());
		return false;
	}

	// Build alpha2->country map
	for (auto&& country : country_list) {
		country_alpha2_map.emplace(country.alpha2, country);
	}

	// Build alpha3->country map
	for (auto&& country : country_list) {
		country_alpha3_map.emplace(country.alpha3, country);
	}

	// Build numeric->country map
	for (auto&& country : country_list) {
		country_numeric_map.emplace(std::stoul(country.numeric), country);
	}

	return true;
}

// Conversion function invoked by Boost to build isocodes_currency_t from JSON object
static isocodes_currency_t tag_invoke(boost::json::value_to_tag<isocodes_currency_t>, const boost::json::value& jv)
{
	const auto& obj = jv.as_object();
	return isocodes_currency_t {
		boost::json::value_to<std::string>(obj.at("name")),
		boost::json::value_to<std::string>(obj.at("alpha_3")),
		boost::json::value_to<std::string>(obj.at("numeric")),
	};
}

static bool build_currency_list(const boost::json::value& jv) noexcept
{
	/* iso-codes package's iso_4217.json file should have this structure
	{
		"4217": [
			...
			{
				"alpha_3": "EUR",
				"name": "Euro",
				"numeric": "978"
			},
			...
		]
	}
	*/

	try {
		const auto& iso4217 = jv.as_object();
		const auto& iso4217_list = iso4217.at("4217");
		currency_list = boost::json::value_to<decltype(currency_list)>(iso4217_list);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Exception: %s\n", e.what());
		return false;
	}

	// Build alpha3->currency map
	for (auto&& currency : currency_list) {
		currency_alpha3_map.emplace(currency.alpha3, currency);
	}

	// Build numeric->currency map
	for (auto&& currency : currency_list) {
		currency_numeric_map.emplace(std::stoul(currency.numeric), currency);
	}

	return true;
}

int isocodes_init(void)
{
	boost::json::value jv;
	bool result;

	try {
		jv = parse_json_file("/usr/share/iso-codes/json/iso_3166-1.json");
		if (jv.kind() == boost::json::kind::null) {
			return 1;
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Exception: %s\n", e.what());
		return -1;
	}

	result = build_country_list(jv);
	if (!result) {
		return -2;
	}

	try {
		jv = parse_json_file("/usr/share/iso-codes/json/iso_4217.json");
		if (jv.kind() == boost::json::kind::null) {
			return 2;
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Exception: %s\n", e.what());
		return -3;
	}

	result = build_currency_list(jv);
	if (!result) {
		return -4;
	}

	return 0;
}

const char* isocodes_lookup_country_by_alpha2(const char* alpha2)
{
	auto itr = country_alpha2_map.find(alpha2);
	if (itr == country_alpha2_map.end()) {
		// Alpha2 country code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}

const char* isocodes_lookup_country_by_alpha3(const char* alpha3)
{
	auto itr = country_alpha3_map.find(alpha3);
	if (itr == country_alpha3_map.end()) {
		// Alpha3 country code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}

const char* isocodes_lookup_country_by_numeric(unsigned int numeric)
{
	auto itr = country_numeric_map.find(numeric);
	if (itr == country_numeric_map.end()) {
		// Numeric country code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}

const char* isocodes_lookup_currency_by_alpha3(const char* alpha3)
{
	auto itr = currency_alpha3_map.find(alpha3);
	if (itr == currency_alpha3_map.end()) {
		// Alpha3 currency code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}

const char* isocodes_lookup_currency_by_numeric(unsigned int numeric)
{
	auto itr = currency_numeric_map.find(numeric);
	if (itr == currency_numeric_map.end()) {
		// Numeric currency code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}
