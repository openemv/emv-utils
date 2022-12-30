##############################################################################
# Copyright (c) 2021, 2022 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# PCSCLite_FOUND
# PCSCLite_VERSION
# PCSCLite_DEFINITIONS
# PCSCLite_INCLUDE_DIRS
# PCSCLite_LIBRARIES

# This module also provides the following imported targets, if found:
# PCSCLite::PCSCLite

# Use PkgConfig to find PCSCLite
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_PCSCLite QUIET libpcsclite)

find_path(PCSCLite_INCLUDE_DIR
	NAMES pcsclite.h winscard.h
	PATHS ${PC_PCSCLite_INCLUDE_DIRS}
	NO_CMAKE_SYSTEM_PATH # Ignore Apple's PCSC Framework
)

find_library(PCSCLite_LIBRARY
	NAMES pcsclite
	PATHS ${PC_PCSCLite_LIBRARY_DIRS}
	NO_CMAKE_SYSTEM_PATH # Ignore Apple's PCSC Framework
)

set(PCSCLite_VERSION ${PC_PCSCLite_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCSCLite
	REQUIRED_VARS
		PCSCLite_LIBRARY
		PCSCLite_INCLUDE_DIR
	VERSION_VAR PCSCLite_VERSION
)

if(PCSCLite_FOUND)
	set(PCSCLite_DEFINITIONS ${PC_PCSCLite_CFLAGS_OTHER})
	set(PCSCLite_INCLUDE_DIRS ${PCSCLite_INCLUDE_DIR})
	set(PCSCLite_LIBRARIES ${PCSCLite_LIBRARY})

	if (NOT TARGET PCSCLite::PCSCLite)
		add_library(PCSCLite::PCSCLite UNKNOWN IMPORTED)
		set_target_properties(PCSCLite::PCSCLite
			PROPERTIES
				IMPORTED_LOCATION "${PCSCLite_LIBRARY}"
				INTERFACE_COMPILE_OPTIONS "${PC_PCSCLite_CFLAGS_OTHER}"
				INTERFACE_INCLUDE_DIRECTORIES "${PCSCLite_INCLUDE_DIR}"
		)
	endif()
endif()

mark_as_advanced(PCSCLite_INCLUDE_DIR PCSCLite_LIBRARY)
