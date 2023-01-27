##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# IsoCodes_FOUND
# IsoCodes_VERSION
# IsoCodes_PREFIX
# IsoCodes_DOMAINS
# IsoCodes_JSON_PATH

cmake_minimum_required(VERSION 3.12) # For list(TRANSFORM...)

# Use PkgConfig to find iso-codes
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_iso_codes QUIET iso-codes)

set(IsoCodes_VERSION ${PC_iso_codes_VERSION})
set(IsoCodes_PREFIX ${PC_iso_codes_PREFIX})
pkg_get_variable(IsoCodes_DOMAINS iso-codes domains)

# Build JSON file list from domains list
list(TRANSFORM
	IsoCodes_DOMAINS
	APPEND ".json"
	OUTPUT_VARIABLE IsoCodes_JSON_FILES
)

# Validate JSON path
find_path(IsoCodes_JSON_PATH
	NAMES ${IsoCodes_JSON_FILES}
	PATHS "${IsoCodes_PREFIX}/share/iso-codes/json/"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IsoCodes
	FOUND_VAR IsoCodes_FOUND
	REQUIRED_VARS
		IsoCodes_JSON_PATH
		IsoCodes_PREFIX
		IsoCodes_DOMAINS
	VERSION_VAR IsoCodes_VERSION
)

mark_as_advanced(IsoCodes_JSON_FILES)
