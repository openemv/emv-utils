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
	set_source_files_properties(emv-decode.c
		PROPERTIES
			COMPILE_DEFINITIONS EMV_UTILS_VERSION_STRING="${EMV_UTILS_VERSION_STRING}"
	)
	target_link_libraries(emv-decode PRIVATE emv_strings emv iso7816)
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
	set_source_files_properties(emv-tool.c
		PROPERTIES
			COMPILE_DEFINITIONS EMV_UTILS_VERSION_STRING="${EMV_UTILS_VERSION_STRING}"
	)
	target_link_libraries(emv-tool PRIVATE emv_strings emv iso7816)
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

	add_test(NAME emv_decode_test9
		COMMAND emv-decode --country 528
	)
	set_tests_properties(emv_decode_test9
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Netherlands"
	)

	add_test(NAME emv_decode_test10
		COMMAND emv-decode --iso3166-1 GBR
	)
	set_tests_properties(emv_decode_test10
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^United Kingdom"
	)

	add_test(NAME emv_decode_test11
		COMMAND emv-decode --country US
	)
	set_tests_properties(emv_decode_test11
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^United States"
	)

	add_test(NAME emv_decode_test12
		COMMAND emv-decode --currency 978
	)
	set_tests_properties(emv_decode_test12
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Euro"
	)

	add_test(NAME emv_decode_test13
		COMMAND emv-decode --iso4217 HKD
	)
	set_tests_properties(emv_decode_test13
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Hong Kong Dollar"
	)

	add_test(NAME emv_decode_test14
		COMMAND emv-decode --currency 826
	)
	set_tests_properties(emv_decode_test14
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Pound Sterling"
	)

	add_test(NAME emv_decode_test15
		COMMAND emv-decode --language fr
	)
	set_tests_properties(emv_decode_test15
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^French"
	)

	add_test(NAME emv_decode_test16
		COMMAND emv-decode --iso639 zh
	)
	set_tests_properties(emv_decode_test16
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Chinese"
	)

	add_test(NAME emv_decode_test17
		COMMAND emv-decode --language wln
	)
	set_tests_properties(emv_decode_test17
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Walloon"
	)

	add_test(NAME emv_decode_test18
		COMMAND emv-decode --tlv 9F3B06097808260840
	)
	set_tests_properties(emv_decode_test18
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F3B \\| Application Reference Currency : \\[6\\] 09 78 08 26 08 40[\r\n]  - Euro[\r\n]  - Pound Sterling[\r\n]  - US Dollar[\r\n]"
	)

	add_test(NAME emv_decode_test19
		COMMAND emv-decode --tlv 9F3B06097801230840
	)
	set_tests_properties(emv_decode_test19
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F3B \\| Application Reference Currency : \\[6\\] 09 78 01 23 08 40[\r\n]  - Euro[\r\n]  - Unknown[\r\n]  - US Dollar[\r\n]"
	)

	add_test(NAME emv_decode_test20
		COMMAND emv-decode --tlv 5F2D04656E6672
	)
	set_tests_properties(emv_decode_test20
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^5F2D \\| Language Preference : \\[4\\] 65 6E 66 72[\r\n]  - English[\r\n]  - French[\r\n]"
	)

	add_test(NAME emv_decode_test21
		COMMAND emv-decode --tlv 5F2D04454E4652
	)
	set_tests_properties(emv_decode_test21
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^5F2D \\| Language Preference : \\[4\\] 45 4E 46 52[\r\n]  - English[\r\n]  - French[\r\n]"
	)

	add_test(NAME emv_decode_test22
		COMMAND emv-decode --tlv 5F2D04656EFFFF
	)
	set_tests_properties(emv_decode_test22
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^5F2D \\| Language Preference : \\[4\\] 65 6E FF FF[\r\n]  - English[\r\n]  - Unknown[\r\n]"
	)

	add_test(NAME emv_decode_9F6E_test1
		COMMAND emv-decode --tlv 9F6E0E0528000332310001010840000000
	)
	string(CONCAT emv_decode_9F6E_test1_regex
		"^9F6E \\| Third Party Data : \\[14\\] 05 28 00 03 32 31 00 01 01 08 40 00 00 00[\r\n]"
		"  - Country: Netherlands[\r\n]"
		"  - Unique Identifier: Mastercard Product Extension[\r\n]"
		"  - Device Type: 21 - Phone \\(Mobile phone\\)[\r\n]"
		"  - Product Identifier: Fleet Card[\r\n]"
		"  - Product Restriction Code: Good for fuel and other products[\r\n]"
		"  - Product Type Code: Prompt for Odometer[\r\n]"
		"  - Card Type: Vehicle card[\r\n]"
	)
	set_tests_properties(emv_decode_9F6E_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F6E_test1_regex}
	)

	add_test(NAME emv_decode_9F6E_test2
		COMMAND emv-decode --tlv 9F6E0B0250000330350002000200
	)
	string(CONCAT emv_decode_9F6E_test2_regex
		"^9F6E \\| Third Party Data : \\[11\\] 02 50 00 03 30 35 00 02 00 02 00[\r\n]"
		"  - Country: France[\r\n]"
		"  - Unique Identifier: Mastercard Product Extension[\r\n]"
		"  - Device Type: 05 - Wristband[\r\n]"
		"  - Product Identifier: Transit[\r\n]"
		"  - Concession Code: 02 - Student, potentially eligible for student-based discounts[\r\n]"
	)
	set_tests_properties(emv_decode_9F6E_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F6E_test2_regex}
	)

	add_test(NAME emv_decode_9F6E_test3
		COMMAND emv-decode --tlv 9F6E04238C0000
	)
	string(CONCAT emv_decode_9F6E_test3_regex
		"^9F6E \\| Form Factor Indicator \\(FFI\\) : \\[4\\] 23 8C 00 00[\r\n]"
		"  - Consumer Payment Device Form Factor: Consumer mobile phone[\r\n]"
		"  - Consumer Payment Device Features: Passcode Capable[\r\n]"
		"  - Consumer Payment Device Features: Two-way Messaging[\r\n]"
		"  - Consumer Payment Device Features: Cloud Based Payment Credentials[\r\n]"
	)
	set_tests_properties(emv_decode_9F6E_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F6E_test3_regex}
	)

	add_test(NAME emv_decode_9F6E_test4
		COMMAND emv-decode --tlv 9F6E049CD04003
	)
	string(CONCAT emv_decode_9F6E_test4_regex
		"^9F6E \\| Enhanced Contactless Reader Capabilities : \\[4\\] 9C D0 40 03[\r\n]"
		"  - Contact mode supported[\r\n]"
		"  - Contactless EMV partial online mode supported[\r\n]"
		"  - Contactless Mobile Supported[\r\n]"
		"  - Try Another Interface after a decline[\r\n]"
		"  - Mobile CVM supported[\r\n]"
		"  - Online PIN supported[\r\n]"
		"  - Plaintext Offline PIN supported[\r\n]"
		"  - CVM Required[\r\n]"
		"  - C-4 kernel version 2.7[\r\n]"
	)
	set_tests_properties(emv_decode_9F6E_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F6E_test4_regex}
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
