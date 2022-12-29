##############################################################################
# Copyright (c) 2022 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Build command line tools by default
option(BUILD_EMV_DECODE "Build emv-decode" ON)
option(BUILD_EMV_TOOL "Build emv-tool" ON)

# Check for argp
find_package(argp)
if(NOT argp_FOUND)
	if(BUILD_EMV_DECODE OR BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find argp. This is required to build command line tools.")
	endif()
endif()

find_package(PCSCLite 1.8)
if(NOT PCSCLite_FOUND)
	if(BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find PCSCLite. This is required to build emv-tool")
	endif()
endif()

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()

# EMV decode command line tool
if(BUILD_EMV_DECODE)
	add_executable(emv-decode emv-decode.c print_helpers.c)
	target_link_libraries(emv-decode PRIVATE emv iso7816)
	if(TARGET libargp::argp)
		target_link_libraries(emv-decode PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			emv-decode
		RUNTIME
			COMPONENT emv_runtime
	)
endif()

# EMV processing command line tool
if(BUILD_EMV_TOOL)
	add_executable(emv-tool emv-tool.c print_helpers.c ../src/pcsc.c)
	target_link_libraries(emv-tool PRIVATE emv iso7816 PCSCLite::PCSCLite)
	if(TARGET libargp::argp)
		target_link_libraries(emv-tool PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			emv-tool
		RUNTIME
			COMPONENT emv_runtime
	)
endif()
