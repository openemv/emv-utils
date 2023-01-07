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

# Check for argp or allow the FETCH_ARGP option to download and build a local
# copy of libargp for monolithic builds on platforms without package managers
# like MacOS and Windows.
option(FETCH_ARGP "Download and build libargp")
if(FETCH_ARGP)
	set(ARGP_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES})
	include(FetchLibArgp)
else()
	find_package(argp)
endif()
if(NOT argp_FOUND)
	if(BUILD_EMV_DECODE OR BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find argp. Enable FETCH_ARGP to download and build libargp. This is required to build command line tools.")
	endif()
endif()

# Check for PC/SC API using PCSCLite on Linux and MacOS or Win32 API on Windows
if(WIN32)
	include(CheckIncludeFile)
	check_include_file(winscard.h HAVE_WINSCARD_H)
	if(NOT HAVE_WINSCARD_H AND BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find winscard.h. This is required to build emv-tool.")
	endif()
	set(PCSC_LIBRARIES WinSCard)
else()
	find_package(PCSCLite 1.8)
	if(NOT PCSCLite_FOUND)
		if(BUILD_EMV_TOOL)
			message(FATAL_ERROR "Could NOT find PCSCLite. This is required to build emv-tool.")
		endif()
	endif()
	set(PCSC_LIBRARIES PCSCLite::PCSCLite)
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
		EXPORT emvUtilsTargets # For use by install(EXPORT) command
		RUNTIME
			COMPONENT emv_runtime
	)
endif()

# EMV processing command line tool
if(BUILD_EMV_TOOL)
	if(PCSCLite_FOUND)
		set_source_files_properties(../src/pcsc.c
			PROPERTIES
				COMPILE_DEFINITIONS USE_PCSCLITE
		)
	endif()

	add_executable(emv-tool emv-tool.c print_helpers.c ../src/pcsc.c)
	target_link_libraries(emv-tool PRIVATE emv iso7816)
	if(TARGET libargp::argp)
		target_link_libraries(emv-tool PRIVATE libargp::argp)
	endif()
	if(PCSC_LIBRARIES)
		target_link_libraries(emv-tool PRIVATE ${PCSC_LIBRARIES})
	endif()

	install(
		TARGETS
			emv-tool
		EXPORT emvUtilsTargets # For use by install(EXPORT) command
		RUNTIME
			COMPONENT emv_runtime
	)
endif()

if(TARGET emv-decode AND BUILD_TESTING)
	add_test(NAME emv_decode_test1
		COMMAND emv-decode --atr 3BDA18FF81B1FE751F030031F573C78A40009000B0
	)
	set_tests_properties(emv_decode_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^ATR: 3BDA18FF81B1FE751F030031F573C78A40009000B0[\r\n]  TS  = 0x3B: Direct convention.*  TCK = 0xB0[\r\n]"
	)

	add_test(NAME emv_decode_test2
		COMMAND emv-decode --tlv 810400000320
	)
	set_tests_properties(emv_decode_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^81 \\| Amount, Authorised \\(Binary\\) : \\[4\\] 00 00 03 20 \\(800\\)[\r\n]"
	)

	add_test(NAME emv_decode_test3
		COMMAND emv-decode --tlv 9A03221215
	)
	set_tests_properties(emv_decode_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9A \\| Transaction Date : \\[3\\] 22 12 15 \\(2022-12-15\\)[\r\n]"
	)

	add_test(NAME emv_decode_test4
		COMMAND emv-decode --tlv 9F2103111542
	)
	set_tests_properties(emv_decode_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F21 \\| Transaction Time : \\[3\\] 11 15 42 \\(11:15:42\\)[\r\n]"
	)

	add_test(NAME emv_decode_test5
		COMMAND emv-decode --tlv 9C0100
	)
	set_tests_properties(emv_decode_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9C \\| Transaction Type : \\[1\\] 00 \\(Goods and services\\)[\r\n]"
	)

	add_test(NAME emv_decode_test6
		COMMAND emv-decode --tlv 9F390105
	)
	set_tests_properties(emv_decode_test6
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F39 \\| Point-of-Service \\(POS\\) Entry Mode : \\[1\\] 05 \\(Integrated circuit card \\(ICC\\)\\. CVV can be checked\\.\\)[\r\n]"
	)

	add_test(NAME emv_decode_test7
		COMMAND emv-decode --tlv 571040123456789095D2512201197339300F
	)
	set_tests_properties(emv_decode_test7
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^57 \\| Track 2 Equivalent Data : \\[16\\] 40 12 34 56 78 90 95 D2 51 22 01 19 73 39 30 0F \\(40123456789095=2512201197339300\\)[\r\n]"
	)

	add_test(NAME emv_decode_test8
		COMMAND emv-decode --tlv 701A9F390105571040123456789095D2512201197339300F82025900
	)
	set_tests_properties(emv_decode_test8
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^70 \\| EMV Data Template : \\[26\\][\r\n]  9F39 \\| Point-of-Service \\(POS\\) Entry Mode : \\[1\\] 05 \\(Integrated circuit card \\(ICC\\)\\. CVV can be checked\\.\\)[\r\n]  57 \\| Track 2 Equivalent Data : \\[16\\] 40 12 34 56 78 90 95 D2 51 22 01 19 73 39 30 0F \\(40123456789095=2512201197339300\\)[\r\n]  82 \\| Application Interchange Profile \\(AIP\\) : \\[2\\] 59 00[\r\n]"
	)

	if(WIN32)
		# Ensure that tests can find required DLLs (if any)
		# Assume that the PATH already contains the compiler runtime DLLs
		set(EMVTOOLS_TEST_PATH "${CMAKE_CURRENT_BINARY_DIR}/../src/;$ENV{PATH}")
		cmake_path(NATIVE_PATH EMVTOOLS_TEST_PATH NORMALIZE EMVTOOLS_TEST_PATH)
		get_property(EMVTOOLS_TEST_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TESTS)
		set_property(TEST ${EMVTOOLS_TEST_TARGETS}
			PROPERTY
				ENVIRONMENT "PATH=${EMVTOOLS_TEST_PATH}"
		)
	endif()
endif()
