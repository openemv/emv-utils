##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# rpmbuild_FOUND
# rpmbuild_VERSION
# rpmbuild_EXECUTABLE

find_program(rpmbuild_EXECUTABLE rpmbuild)
if(rpmbuild_EXECUTABLE)
	execute_process(
		COMMAND ${rpmbuild_EXECUTABLE} --version
		OUTPUT_VARIABLE rpmbuild_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	string(REGEX MATCH "RPM version ([0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?)"
		rpmbuild_VERSION "${rpmbuild_VERSION}"
	)
	set(rpmbuild_VERSION "${CMAKE_MATCH_1}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rpmbuild
	REQUIRED_VARS
		rpmbuild_EXECUTABLE
	VERSION_VAR rpmbuild_VERSION
)

mark_as_advanced(rpmbuild_EXECUTABLE)
