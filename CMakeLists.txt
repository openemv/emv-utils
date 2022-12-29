##############################################################################
# Copyright (c) 2021, 2022 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

project(emv-tools
	VERSION 0.0.1
	DESCRIPTION "EMV libraries and tools"
	HOMEPAGE_URL "https://github.com/openemv/emv-tools"
	LANGUAGES C
)

# Determine whether this project is the top-level project
if(${CMAKE_VERSION} VERSION_LESS "3.21")
	get_directory_property(EMV_TOOLS_HAS_PARENT PARENT_DIRECTORY)
	if(NOT EMV_TOOLS_HAS_PARENT)
		set(EMV_TOOLS_IS_TOP_LEVEL True)
	endif()
else()
	# CMake >=3.21 provides <PROJECT-NAME>_IS_TOP_LEVEL
	set(EMV_TOOLS_IS_TOP_LEVEL ${emv-tools_IS_TOP_LEVEL})
endif()

# Configure compiler
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
#set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wall -Werror)
	add_compile_options($<$<CONFIG:Debug>:-ggdb>)
	add_compile_options($<$<CONFIG:RelWithDebInfo>:-ggdb>)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

# Configure testing before adding subdirectories
if(EMV_TOOLS_IS_TOP_LEVEL)
	# Configure Valgrind before including CTest module
	find_program(VALGRIND_COMMAND valgrind)
	set(MEMORYCHECK_TYPE Valgrind)
	set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --show-reachable=yes --track-origins=yes --num-callers=100 --show-error-list=yes")

	# Only top-level project should include CTest module
	include(CTest)
endif()

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()
