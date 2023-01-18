##############################################################################
# Copyright (c) 2023 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# json-c_FOUND
# json-c_VERSION
# json-c_DEFINITIONS
# json-c_INCLUDE_DIRS
# json-c_LIBRARIES

# This module also provides the following imported targets, if found:
# json-c::json-c

# Use PkgConfig to find json-c
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_json-c QUIET json-c)

find_path(json-c_INCLUDE_DIR
	NAMES json.h json_c_version.h
	PATHS ${PC_json-c_INCLUDE_DIRS}
)

find_library(json-c_LIBRARY
	NAMES json-c
	PATHS ${PC_json-c_LIBRARY_DIRS}
)

set(json-c_VERSION ${PC_json-c_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(json-c
	REQUIRED_VARS
		json-c_LIBRARY
		json-c_INCLUDE_DIR
	VERSION_VAR json-c_VERSION
)

if(json-c_FOUND)
	set(json-c_DEFINITIONS ${PC_json-c_CFLAGS_OTHER})
	set(json-c_INCLUDE_DIRS ${json-c_INCLUDE_DIR})
	set(json-c_LIBRARIES ${json-c_LIBRARY})

	if (NOT TARGET json-c::json-c)
		add_library(json-c::json-c UNKNOWN IMPORTED)
		set_target_properties(json-c::json-c
			PROPERTIES
				IMPORTED_LOCATION "${json-c_LIBRARY}"
				INTERFACE_COMPILE_OPTIONS "${PC_json-c_CFLAGS_OTHER}"
				INTERFACE_INCLUDE_DIRECTORIES "${json-c_INCLUDE_DIR}"
		)
	endif()
endif()

mark_as_advanced(json-c_INCLUDE_DIR json-c_LIBRARY)
