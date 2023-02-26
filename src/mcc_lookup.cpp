/**
 * @file mcc_lookup.cpp
 * @brief ISO 18245 Merchant Category Code (MCC) lookup helper functions
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

#include "mcc_lookup.h"
#include "mcc_config.h"

#include <string>
#include <map>

// NOTE: Older versions of json-c, like the one for Ubuntu 20.04, don't specify
// C linkage in the headers, which means it must be specified explicitly here
// to avoid C++ linkage and to ensure successful library linkage.
#include <sys/cdefs.h>
__BEGIN_DECLS
#include <json-c/json.h>
#include <json-c/json_visit.h>
__END_DECLS

typedef bool (*mcc_map_add_func_t)(json_object* jso);

static std::map<unsigned int,std::string> mcc_map;

static bool mcc_map_add(json_object* jso)
{
	/* mcc-codes submodule's mcc_codes.json file should have this structure
	{
		[
			...
			{
				"mcc": "5999",
				"edited_description": "Miscellaneous and Specialty Retail Stores",
				"combined_description": "Miscellaneous and Specialty Retail Stores",
				"usda_description": "Miscellaneous and Specialty Retail Stores",
				"irs_description": "Miscellaneous Specialty Retail",
				"irs_reportable": "No1.6041-3(c)",
				"id": 854
			},
			...
		]
	}
	*/

	// Extract mcc number
	json_object* mcc_obj = json_object_object_get(jso, "mcc");
	if (!mcc_obj) {
		return false;
	}
	int64_t mcc_number = json_object_get_int64(mcc_obj);
	if (mcc_number <= 0) {
		return false;
	}

	// Extract edited_description string
	json_object* desc_obj = json_object_object_get(jso, "edited_description");
	if (!desc_obj) {
		return false;
	}
	const char* desc_str = json_object_get_string(desc_obj);
	if (!desc_str) {
		return false;
	}

	// Add to map
	mcc_map[mcc_number] = desc_str;

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
		mcc_map_add_func_t mcc_map_add;
		bool result;

		// Append object to the appropriate using the function pointer provided by userarg
		mcc_map_add = (mcc_map_add_func_t)userarg;
		result = mcc_map_add(jso);
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

static bool build_mcc_list(json_object* json_root) noexcept
{
	/* mcc-codes submodule's mcc_codes.json file should have this structure
	{
		[
			...
			{
				"mcc": "5999",
				"edited_description": "Miscellaneous and Specialty Retail Stores",
				"combined_description": "Miscellaneous and Specialty Retail Stores",
				"usda_description": "Miscellaneous and Specialty Retail Stores",
				"irs_description": "Miscellaneous Specialty Retail",
				"irs_reportable": "No1.6041-3(c)",
				"id": 854
			},
			...
		]
	}
	*/

	int r;
	json_type json_root_type;
	size_t json_root_array_length;

	json_root_type = json_object_get_type(json_root);
	if (json_root_type != json_type_array) {
		return false;
	}
	json_root_array_length = json_object_array_length(json_root);
	if (!json_root_array_length) {
		return false;
	}

	r = json_c_visit(json_root, 0, &json_array_visit_userfunc, (void*)&mcc_map_add);
	if (r) {
		return false;
	}

	return true;
}

int mcc_init(const char* path)
{
	bool result;
	std::string filename;
	json_object* json_root;

	if (path) {
		filename = path;
	} else {
		filename = MCC_JSON_INSTALL_PATH;
	}

	// Parse JSON file and build MCC list
	json_root = json_object_from_file(filename.c_str());
	if (!json_root) {
		std::fprintf(stderr, "%s\n", json_util_get_last_err());
		return 1;
	}
	result = build_mcc_list(json_root);
	json_object_put(json_root);
	if (!result) {
		std::fprintf(stderr, "Failed to parse %s\n", filename.c_str());
		return -1;
	}

	return 0;
}

const char* mcc_lookup(unsigned int mcc)
{
	auto itr = mcc_map.find(mcc);
	if (itr == mcc_map.end()) {
		// Merchant Category Code (MCC) not found
		return nullptr;
	}

	return itr->second.c_str();
}
