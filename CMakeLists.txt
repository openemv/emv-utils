##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.11)

project(emv-tools)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
	add_compile_options(-Wall -Werror)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(PCSCLite 1.8 REQUIRED)

add_library(iso7816
	src/iso7816.c
	src/iso7816_compact_tlv.c
	src/iso7816_strings.c
)
target_include_directories(iso7816 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

add_library(iso8825
	src/iso8825_ber.c
)
target_include_directories(iso8825 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

add_library(emv
	src/emv_ttl.c
	src/emv_tlv.c
	src/emv_app.c
	src/emv_tal.c
	src/emv_strings.c
)
target_link_libraries(emv iso8825)
target_include_directories(emv INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

add_executable(emv-decode tools/emv-decode.c tools/print_helpers.c)
target_link_libraries(emv-decode emv iso7816)

add_executable(emv-tool tools/emv-tool.c tools/print_helpers.c src/pcsc.c)
target_link_libraries(emv-tool emv iso7816 PCSCLite::PCSCLite)
