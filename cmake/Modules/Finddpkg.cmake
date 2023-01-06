##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# dpkg_FOUND
# dpkg_VERSION
# dpkg_EXECUTABLE

find_program(dpkg_EXECUTABLE dpkg)
if(dpkg_EXECUTABLE)
	execute_process(
		COMMAND ${dpkg_EXECUTABLE} --version
		OUTPUT_VARIABLE dpkg_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	string(REGEX MATCH "version ([0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?)"
		dpkg_VERSION "${dpkg_VERSION}"
	)
	set(dpkg_VERSION "${CMAKE_MATCH_1}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(dpkg
	REQUIRED_VARS
		dpkg_EXECUTABLE
	VERSION_VAR dpkg_VERSION
)

mark_as_advanced(dpkg_EXECUTABLE)
