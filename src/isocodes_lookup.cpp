/**
 * @file isocodes_lookup.cpp
 * @brief Wrapper for iso-codes package
 *
 * Copyright (c) 2021, 2023 Leon Lynch
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
#include "isocodes_config.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdio>

// NOTE: Older versions of json-c, like the one for Ubuntu 20.04, don't specify
// C linkage in the headers, which means it must be specified explicitly here
// to avoid C++ linkage and to ensure successful library linkage.
#include <sys/cdefs.h>
__BEGIN_DECLS
#include <json-c/json.h>
#include <json-c/json_visit.h>
__END_DECLS

typedef bool (*isocodes_list_append_func_t)(json_object* jso);

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

struct isocodes_language_t {
	std::string name;
	std::string alpha2;
	std::string alpha3;
};
static std::vector<isocodes_language_t> language_list;
static std::map<std::string,const isocodes_language_t&> language_alpha2_map;
static std::map<std::string,const isocodes_language_t&> language_alpha3_map;


static bool country_list_append(json_object* jso)
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

	// Extract name string
	json_object* name_obj = json_object_object_get(jso, "name");
	if (!name_obj) {
		return false;
	}
	const char* name_str = json_object_get_string(name_obj);
	if (!name_str) {
		return false;
	}

	// Extract alpha_2 string
	json_object* alpha_2_obj = json_object_object_get(jso, "alpha_2");
	if (!alpha_2_obj) {
		return false;
	}
	const char* alpha_2_str = json_object_get_string(alpha_2_obj);
	if (!alpha_2_str) {
		return false;
	}

	// Extract alpha_3 string
	json_object* alpha_3_obj = json_object_object_get(jso, "alpha_3");
	if (!alpha_3_obj) {
		return false;
	}
	const char* alpha_3_str = json_object_get_string(alpha_3_obj);
	if (!alpha_3_str) {
		return false;
	}

	// Extract numeric string
	json_object* numeric_obj = json_object_object_get(jso, "numeric");
	if (!numeric_obj) {
		return false;
	}
	const char* numeric_str = json_object_get_string(numeric_obj);
	if (!numeric_str) {
		return false;
	}

	// Populate country list entry
	country_list.push_back({ name_str, alpha_2_str, alpha_3_str, numeric_str });

	return true;
}

static bool currency_list_append(json_object* jso)
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

	// Extract name string
	json_object* name_obj = json_object_object_get(jso, "name");
	if (!name_obj) {
		return false;
	}
	const char* name_str = json_object_get_string(name_obj);
	if (!name_str) {
		return false;
	}

	// Extract alpha_3 string
	json_object* alpha_3_obj = json_object_object_get(jso, "alpha_3");
	if (!alpha_3_obj) {
		return false;
	}
	const char* alpha_3_str = json_object_get_string(alpha_3_obj);
	if (!alpha_3_str) {
		return false;
	}

	// Extract numeric string
	json_object* numeric_obj = json_object_object_get(jso, "numeric");
	if (!numeric_obj) {
		return false;
	}
	const char* numeric_str = json_object_get_string(numeric_obj);
	if (!numeric_str) {
		return false;
	}

	// Populate currency list entry
	currency_list.push_back({ name_str, alpha_3_str, numeric_str });

	return true;
}

static bool language_list_append(json_object* jso)
{
	/* iso-codes package's iso_639-2.json file should have this structure
	{
		"639-2": [
			...
			{
				"alpha_2": "en",
				"alpha_3": "eng",
				"name": "English"
			},
			...
		]
	}
	*/

	// Extract name string
	json_object* name_obj = json_object_object_get(jso, "name");
	if (!name_obj) {
		return false;
	}
	const char* name_str = json_object_get_string(name_obj);
	if (!name_str) {
		return false;
	}

	// Extract alpha_2 string (optional)
	json_object* alpha_2_obj = json_object_object_get(jso, "alpha_2");
	const char* alpha_2_str;
	if (alpha_2_obj) {
		alpha_2_str = json_object_get_string(alpha_2_obj);
	}
	if (!alpha_2_obj || !alpha_2_str) {
		alpha_2_str = "";
	}

	// Extract alpha_3 string
	json_object* alpha_3_obj = json_object_object_get(jso, "alpha_3");
	if (!alpha_3_obj) {
		return false;
	}
	const char* alpha_3_str = json_object_get_string(alpha_3_obj);
	if (!alpha_3_str) {
		return false;
	}

	// Populate language list entry
	language_list.push_back({ name_str, alpha_2_str, alpha_3_str });

	return true;
}

static int json_array_visit_userfunc(
	json_object* jso,
	int flags,
	json_object* parent_jso,
	const char* jso_key,
	size_t* jso_index,
	void* userarg
)
{
	if (flags & JSON_C_VISIT_SECOND) {
		// Ignore the second visit to the array or object
		return JSON_C_VISIT_RETURN_CONTINUE;
	}

	// If there is no parent and it is an array, then it is the top-level array
	if (!parent_jso && json_object_is_type(jso, json_type_array)) {
		// Ignore top-level array
		return JSON_C_VISIT_RETURN_CONTINUE;
	}

	// If there is an index and it is an object, then it is an array entry
	if (jso_index && json_object_is_type(jso, json_type_object)) {
		isocodes_list_append_func_t isocodes_list_append;
		bool result;

		// Append object to the appropriate using the function pointer provided by userarg
		isocodes_list_append = (isocodes_list_append_func_t)userarg;
		result = isocodes_list_append(jso);
		if (!result) {
			return JSON_C_VISIT_RETURN_ERROR;
		}

		// Continue to next array object
		return JSON_C_VISIT_RETURN_CONTINUE;
	}

	// If there is a key and it is a string, then it is a leaf string
	if (jso_key && json_object_is_type(jso, json_type_string)) {
		// Skip over leaf strings
		return JSON_C_VISIT_RETURN_POP;
	}

	// Unexpected object type
	return JSON_C_VISIT_RETURN_ERROR;
}

static bool build_country_list(json_object* json_root) noexcept
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

	json_bool result;
	int r;
	json_object* iso3166_1_obj;
	json_type iso3166_1_type;
	size_t iso3166_1_array_length;

	result = json_object_object_get_ex(json_root, "3166-1", &iso3166_1_obj);
	if (!result) {
		return false;
	}
	iso3166_1_type = json_object_get_type(iso3166_1_obj);
	if (iso3166_1_type != json_type_array) {
		return false;
	}
	iso3166_1_array_length = json_object_array_length(iso3166_1_obj);
	if (!iso3166_1_array_length) {
		return false;
	}

	country_list.reserve(iso3166_1_array_length);
	r = json_c_visit(iso3166_1_obj, 0, &json_array_visit_userfunc, (void*)&country_list_append);
	if (r) {
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

static bool build_currency_list(json_object* json_root) noexcept
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

	json_bool result;
	int r;
	json_object* iso4217_obj;
	json_type iso4217_type;
	size_t iso4217_array_length;

	result = json_object_object_get_ex(json_root, "4217", &iso4217_obj);
	if (!result) {
		return false;
	}
	iso4217_type = json_object_get_type(iso4217_obj);
	if (iso4217_type != json_type_array) {
		return false;
	}
	iso4217_array_length = json_object_array_length(iso4217_obj);
	if (!iso4217_array_length) {
		return false;
	}

	currency_list.reserve(iso4217_array_length);
	r = json_c_visit(iso4217_obj, 0, &json_array_visit_userfunc, (void*)&currency_list_append);
	if (r) {
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

static bool build_language_list(json_object* json_root) noexcept
{
	/* iso-codes package's iso_639-2.json file should have this structure
	{
		"639-2": [
			...
			{
				"alpha_2": "en",
				"alpha_3": "eng",
				"name": "English"
			},
			...
		]
	}
	*/

	json_bool result;
	int r;
	json_object* iso_639_2_obj;
	json_type iso_639_2_type;
	size_t iso_639_2_array_length;

	result = json_object_object_get_ex(json_root, "639-2", &iso_639_2_obj);
	if (!result) {
		return false;
	}
	iso_639_2_type = json_object_get_type(iso_639_2_obj);
	if (iso_639_2_type != json_type_array) {
		return false;
	}
	iso_639_2_array_length = json_object_array_length(iso_639_2_obj);
	if (!iso_639_2_array_length) {
		return false;
	}

	language_list.reserve(iso_639_2_array_length);
	r = json_c_visit(iso_639_2_obj, 0, &json_array_visit_userfunc, (void*)language_list_append);
	if (r) {
		return false;
	}

	// Build alpha2->language map
	for (auto&& language : language_list) {
		if (!language.alpha2.empty()) {
			language_alpha2_map.emplace(language.alpha2, language);
		}
	}

	// Build alpha3->language map
	for (auto&& language : language_list) {
		language_alpha3_map.emplace(language.alpha3, language);
	}

	return true;
}

int isocodes_init(const char* path)
{
	bool result;
	std::string path_str;
	json_object* json_root;
	std::string filename;

	if (path) {
		path_str = path;
	} else {
		path_str = ISOCODES_JSON_PATH;
	}
	if (path_str.back() != '/') {
		path_str += "/";
	}

	// Parse iso_3166-1.json and build country list
	filename = path_str + "iso_3166-1.json";
	json_root = json_object_from_file(filename.c_str());
	if (!json_root) {
		std::fprintf(stderr, "%s\n", json_util_get_last_err());
		return 1;
	}
	result = build_country_list(json_root);
	json_object_put(json_root);
	if (!result) {
		std::fprintf(stderr, "Failed to parse %s\n", filename.c_str());
		return -1;
	}

	// Parse iso_4217.json and build currency list
	filename = path_str + "iso_4217.json";
	json_root = json_object_from_file(filename.c_str());
	if (!json_root) {
		std::fprintf(stderr, "%s\n", json_util_get_last_err());
		return 2;
	}
	result = build_currency_list(json_root);
	json_object_put(json_root);
	if (!result) {
		std::fprintf(stderr, "Failed to parse %s\n", filename.c_str());
		return -2;
	}

	// Parse iso_639-2.json and build language list
	filename = path_str + "iso_639-2.json";
	json_root = json_object_from_file(filename.c_str());
	if (!json_root) {
		std::fprintf(stderr, "%s\n", json_util_get_last_err());
		return 3;
	}
	result = build_language_list(json_root);
	json_object_put(json_root);
	if (!result) {
		std::fprintf(stderr, "Failed to parse %s\n", filename.c_str());
		return -3;
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

const char* isocodes_lookup_language_by_alpha2(const char* alpha2)
{
	auto itr = language_alpha2_map.find(alpha2);
	if (itr == language_alpha2_map.end()) {
		// Alpha2 language code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}

const char* isocodes_lookup_language_by_alpha3(const char* alpha3)
{
	auto itr = language_alpha3_map.find(alpha3);
	if (itr == language_alpha3_map.end()) {
		// Alpha3 language code not found
		return nullptr;
	}

	return itr->second.name.c_str();
}
