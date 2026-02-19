##############################################################################
# Copyright 2022-2026 Leon Lynch
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

# Check for PC/SC API using Win32 API on Windows, PCSC.framework on MacOS,
# and PCSCLite on Linux
if(WIN32)
	include(CheckIncludeFile)
	check_include_file(winscard.h HAVE_WINSCARD_H)
	if(NOT HAVE_WINSCARD_H AND BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find winscard.h. This is required to build emv-tool.")
	endif()
	set(PCSC_LIBRARIES WinSCard)
elseif(APPLE)
	# Apple's PCSC framework is incompatible with upstream pcsc-lite
	find_library(PCSC_FRAMEWORK PCSC)
	if(NOT PCSC_FRAMEWORK AND BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find PCSC.framework. This is required to build emv-tool.")
	endif()
	find_path(PCSC_FRAMEWORK_INCLUDE_DIR NAMES winscard.h)
	if(NOT PCSC_FRAMEWORK_INCLUDE_DIR AND BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find winscard.h in PCSC.framework. This is required to build emv-tool.")
	endif()
	if(PCSC_FRAMEWORK AND PCSC_FRAMEWORK_INCLUDE_DIR)
		message(STATUS "Found PCSC.framework: ${PCSC_FRAMEWORK}")
	endif()
	set(PCSC_LIBRARIES "${PCSC_FRAMEWORK}")
else()
	find_package(PCSCLite 1.8)
	if(NOT PCSCLite_FOUND AND BUILD_EMV_TOOL)
		message(FATAL_ERROR "Could NOT find PCSCLite. This is required to build emv-tool.")
	endif()
	set(PCSC_LIBRARIES PCSCLite::PCSCLite)
endif()

# Check for headers that provide ntohl/htonl and friends
include(CheckIncludeFile)
CHECK_INCLUDE_FILE(
	arpa/inet.h
	HAVE_ARPA_INET_H
)
CHECK_INCLUDE_FILE(
	winsock.h
	HAVE_WINSOCK_H
)
if(NOT HAVE_ARPA_INET_H AND NOT HAVE_WINSOCK_H)
	if(BUILD_EMV_TOOL)
		message(FATAL_ERROR "Failed to find either arpa/inet.h or winsock.h. This is required to build emv-tool.")
	endif()
endif()

# Print helpers object library
add_library(print_helpers OBJECT EXCLUDE_FROM_ALL print_helpers.c)
target_include_directories(print_helpers INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(print_helpers PRIVATE iso7816 emv emv_strings)

# EMV decode command line tool
if(BUILD_EMV_DECODE)
	add_executable(emv-decode emv-decode.c)
	target_link_libraries(emv-decode PRIVATE print_helpers iso7816 emv emv_strings iso8859)
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
		set_property(
			SOURCE ../src/pcsc.c
			APPEND PROPERTY COMPILE_DEFINITIONS USE_PCSCLITE
		)
	endif()
	if(HAVE_ARPA_INET_H)
		set_property(
			SOURCE ../src/pcsc.c
			APPEND PROPERTY COMPILE_DEFINITIONS HAVE_ARPA_INET_H
		)
	endif()
	if(HAVE_WINSOCK_H)
		set_property(
			SOURCE ../src/pcsc.c
			APPEND PROPERTY COMPILE_DEFINITIONS HAVE_WINSOCK_H
		)
	endif()

	add_executable(emv-tool emv-tool.c ../src/pcsc.c)
	if(PCSC_FRAMEWORK_INCLUDE_DIR)
		# Avoid conflicts between Apple's PCSC.h and this project's pcsc.h by
		# adding the Apple PCSC framework as a system include directory to
		# allow the project include directories to take precedence.
		target_include_directories(emv-tool SYSTEM PRIVATE ${PCSC_FRAMEWORK_INCLUDE_DIR})
	endif()
	target_link_libraries(emv-tool PRIVATE print_helpers emv emv_strings)
	if(TARGET libargp::argp)
		target_link_libraries(emv-tool PRIVATE libargp::argp)
	endif()
	if(PCSC_LIBRARIES)
		target_link_libraries(emv-tool PRIVATE ${PCSC_LIBRARIES})
	endif()
	if(HAVE_WINSOCK_H AND NOT HAVE_ARPA_INET_H)
		target_link_libraries(emv-tool PRIVATE ws2_32)
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
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_test1_regex
		"^ATR: 3BDA18FF81B1FE751F030031F573C78A40009000B0[\r\n]"
		"  TS  = 0x3B: Direct convention[\r\n]"
		"  T0  = 0xDA: Y1=TA1,TC1,TD1\\\\; K=10[\r\n]"
		"  ----[\r\n]"
		"  TA1 = 0x18: Fi=372\\\\; Di=12\\\\; 31 cycles/ETU @ max 5.0MHz\\\\; max 161290 bit/s[\r\n]"
		"  TB1 absent: Vpp is not connected[\r\n]"
		"  TC1 = 0xFF: N=255\\\\; GT=11[\r\n]"
		"  TD1 = 0x81: Y2=TD2\\\\; Protocol T=1[\r\n]"
		"  ----[\r\n]"
		"  TA2 absent: Negotiable mode[\r\n]"
		"  TB2 absent: Vpp is not connected[\r\n]"
		"  TC2 absent: WI=10\\\\; WT=115200[\r\n]"
		"  TD2 = 0xB1: Y3=TA3,TB3,TD3\\\\; Protocol T=1[\r\n]"
		"  ----[\r\n]"
		"  TA3 = 0xFE: IFSI=254[\r\n]"
		"  TB3 = 0x75: CWT=43\\\\; BWT=1474571[\r\n]"
		"  TD3 = 0x1F: Y4=TA4\\\\; Global \\(T=15\\)[\r\n]"
		"  ----[\r\n]"
		"  TA4 = 0x03: Clock stop: Not supported\\\\; Class: A \\(5V\\), B \\(3V\\)[\r\n]"
		"  ----[\r\n]"
		"  T1  = 0x00: COMPACT-TLV followed by mandatory status indicator[\r\n]"
		".*"
		"  ----[\r\n]"
		"  LCS = 00: No information given[\r\n]"
		"  SW  = 9000: \\(Normal\\)[\r\n]"
		"  ----[\r\n]"
		"  TCK = 0xB0[\r\n]"
	)
	set_tests_properties(emv_decode_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_test1_regex}
	)

	add_test(NAME emv_decode_test2
		COMMAND emv-decode --tlv 810400000320
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^81 \\| Amount, Authorised \\(Binary\\) : \\[4\\] 00 00 03 20 \\(800\\)[\r\n]"
	)

	add_test(NAME emv_decode_test3
		COMMAND emv-decode --tlv 9A032212159A034912319A03500101
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_test3_regex
		"^9A \\| Transaction Date : \\[3\\] 22 12 15 \\(2022-12-15\\)[\r\n]"
		"9A \\| Transaction Date : \\[3\\] 49 12 31 \\(2049-12-31\\)[\r\n]"
		"9A \\| Transaction Date : \\[3\\] 50 01 01 \\(1950-01-01\\)[\r\n]"
	)
	set_tests_properties(emv_decode_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_test3_regex}
	)

	add_test(NAME emv_decode_test4
		COMMAND emv-decode --tlv 9F2103111542
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F21 \\| Transaction Time : \\[3\\] 11 15 42 \\(11:15:42\\)[\r\n]"
	)

	add_test(NAME emv_decode_test5
		COMMAND emv-decode --tlv 9C0100
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9C \\| Transaction Type : \\[1\\] 00 \\(Goods and services\\)[\r\n]"
	)

	add_test(NAME emv_decode_test6
		COMMAND emv-decode --tlv 9F390105
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test6
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9F39 \\| Point-of-Service \\(POS\\) Entry Mode : \\[1\\] 05 \\(Integrated circuit card \\(ICC\\)\\. CVV can be checked\\.\\)[\r\n]"
	)

	add_test(NAME emv_decode_test7
		COMMAND emv-decode --tlv 571040123456789095D2512201197339300F
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test7
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^57 \\| Track 2 Equivalent Data : \\[16\\] 40 12 34 56 78 90 95 D2 51 22 01 19 73 39 30 0F \\(40123456789095=2512201197339300\\)[\r\n]"
	)

	add_test(NAME emv_decode_test8
		COMMAND emv-decode --tlv 702D9F390105500C564953412025435245444954571040123456789095D2512201197339300F820259009F15025999
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_test8_regex
		"^70 \\| EMV Data Template : \\[45\\][\r\n]"
		"  9F39 \\| Point-of-Service \\(POS\\) Entry Mode : \\[1\\] 05 \\(Integrated circuit card \\(ICC\\)\\. CVV can be checked\\.\\)[\r\n]"
		"  50 \\| Application Label : \\[12\\] 56 49 53 41 20 25 43 52 45 44 49 54 \"VISA CREDIT\"[\r\n]" # Value intentionally contains invalid character
		"  57 \\| Track 2 Equivalent Data : \\[16\\] 40 12 34 56 78 90 95 D2 51 22 01 19 73 39 30 0F \\(40123456789095=2512201197339300\\)[\r\n]"
		"  82 \\| Application Interchange Profile \\(AIP\\) : \\[2\\] 59 00[\r\n]"
		"    - Static Data Authentication \\(SDA\\) is supported[\r\n]"
		"    - Cardholder verification is supported[\r\n]"
		"    - Terminal risk management is to be performed[\r\n]"
		"    - Combined DDA/Application Cryptogram Generation \\(CDA\\) is supported[\r\n]"
		"  9F15 \\| Merchant Category Code \\(MCC\\) : \\[2\\] 59 99 \\(Miscellaneous and Specialty Retail Stores\\)[\r\n]"
	)
	set_tests_properties(emv_decode_test8
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_test8_regex}
	)

	add_test(NAME emv_decode_test9
		COMMAND emv-decode --tlv 9F120F6173646631CCCDCECFD0D1D2D3D4D59F1101049F120F6173646632CCCDCECFD0D1D2D3D4D59F1101059F120F6173646633CCCDCECFD0D1D2D3D4D59F11010B9F120F6173646634CCCDCECFD0D1D2D3D4D5
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	if(ISO8859_IMPL STREQUAL "simple")
		string(CONCAT emv_decode_test9_regex
			"^9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 31 CC CD CE CF D0 D1 D2 D3 D4 D5 \"asdf1ÌÍÎÏÐÑÒÓÔÕ\"[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 04[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 32 CC CD CE CF D0 D1 D2 D3 D4 D5[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 05[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 33 CC CD CE CF D0 D1 D2 D3 D4 D5[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 0B[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 34 CC CD CE CF D0 D1 D2 D3 D4 D5[\r\n]$"
		)
	else()
		string(CONCAT emv_decode_test9_regex
			"^9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 31 CC CD CE CF D0 D1 D2 D3 D4 D5 \"asdf1ÌÍÎÏÐÑÒÓÔÕ\"[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 04[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 32 CC CD CE CF D0 D1 D2 D3 D4 D5 \"asdf2ĖÍÎĪĐŅŌĶÔÕ\"[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 05[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 33 CC CD CE CF D0 D1 D2 D3 D4 D5 \"asdf3ЬЭЮЯабвгде\"[\r\n]"
			"9F11 \\| Issuer Code Table Index : \\[1\\] 0B[\r\n]"
			"9F12 \\| Application Preferred Name : \\[15\\] 61 73 64 66 34 CC CD CE CF D0 D1 D2 D3 D4 D5 \"asdf4ฬอฮฯะัาำิี\"[\r\n]$"
		)
	endif()
	set_tests_properties(emv_decode_test9
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_test9_regex}
	)

	add_test(NAME emv_decode_country_test1
		COMMAND emv-decode --country 528
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_country_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Netherlands"
	)

	add_test(NAME emv_decode_country_test2
		COMMAND emv-decode --iso3166-1 GBR
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_country_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^United Kingdom"
	)

	add_test(NAME emv_decode_country_test3
		COMMAND emv-decode --country US
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_country_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^United States"
	)

	add_test(NAME emv_decode_currency_test1
		COMMAND emv-decode --currency 978
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_currency_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Euro"
	)

	add_test(NAME emv_decode_currency_test2
		COMMAND emv-decode --iso4217 HKD
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_currency_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Hong Kong Dollar"
	)

	add_test(NAME emv_decode_currency_test3
		COMMAND emv-decode --currency 826
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_currency_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Pound Sterling"
	)

	add_test(NAME emv_decode_language_test1
		COMMAND emv-decode --language fr
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_language_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^French"
	)

	add_test(NAME emv_decode_language_test2
		COMMAND emv-decode --iso639 zh
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_language_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Chinese"
	)

	add_test(NAME emv_decode_language_test3
		COMMAND emv-decode --language wln
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_language_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^Walloon"
	)

	add_test(NAME emv_decode_9F3B_test1
		COMMAND emv-decode --tlv 9F3B06097808260840
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F3B_test1_regex
		"^9F3B \\| Application Reference Currency : \\[6\\] 09 78 08 26 08 40[\r\n]"
		"  - Euro[\r\n]"
		"  - Pound Sterling[\r\n]"
		"  - US Dollar[\r\n]"
	)
	set_tests_properties(emv_decode_9F3B_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F3B_test1_regex}
	)

	add_test(NAME emv_decode_9F3B_test2
		COMMAND emv-decode --tlv 9F3B06097801230840
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F3B_test2_regex
		"^9F3B \\| Application Reference Currency : \\[6\\] 09 78 01 23 08 40[\r\n]"
		"  - Euro[\r\n]"
		"  - Unknown[\r\n]"
		"  - US Dollar[\r\n]"
	)
	set_tests_properties(emv_decode_9F3B_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F3B_test2_regex}
	)

	add_test(NAME emv_decode_5F2D_test1
		COMMAND emv-decode --tlv 5F2D04656E6672
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_5F2D_test1_regex
		"^5F2D \\| Language Preference : \\[4\\] 65 6E 66 72[\r\n]"
		"  - English[\r\n]"
		"  - French[\r\n]"
	)
	set_tests_properties(emv_decode_5F2D_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_5F2D_test1_regex}
	)

	add_test(NAME emv_decode_5F2D_test2
		COMMAND emv-decode --tlv 5F2D04454E4652
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_5F2D_test2_regex
		"^5F2D \\| Language Preference : \\[4\\] 45 4E 46 52[\r\n]"
		"  - English[\r\n]"
		"  - French[\r\n]"
	)
	set_tests_properties(emv_decode_5F2D_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_5F2D_test2_regex}
	)

	add_test(NAME emv_decode_5F2D_test3
		COMMAND emv-decode --tlv 5F2D04656EFFFF
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_5F2D_test3_regex
		"^5F2D \\| Language Preference : \\[4\\] 65 6E FF FF[\r\n]"
		"  - English[\r\n]"
		"  - Unknown[\r\n]"
	)
	set_tests_properties(emv_decode_5F2D_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_5F2D_test3_regex}
	)

	add_test(NAME emv_decode_9F0A_test1
		COMMAND emv-decode --tlv 9F0A04000101029F0A030001019F0A050003020400
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F0A_test1_regex
		"^9F0A \\| Application Selection Registered Proprietary Data \\(ASRPD\\) : \\[4\\] 00 01 01 02[\r\n]"
		"  - European Cards Stakeholders Group[\r\n]"
		"  - Credit[\r\n]"
		"9F0A \\| Application Selection Registered Proprietary Data \\(ASRPD\\) : \\[3\\] 00 01 01[\r\n]" # No decoding because it is malformed
		"9F0A \\| Application Selection Registered Proprietary Data \\(ASRPD\\) : \\[5\\] 00 03 02 04 00[\r\n]"
		"  - Technical Cooperation ep2 Association[\r\n]"
		"  - Pre-paid[\r\n]$"
	)
	set_tests_properties(emv_decode_9F0A_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F0A_test1_regex}
	)

	add_test(NAME emv_decode_9F6E_test1
		COMMAND emv-decode --tlv 9F6E0E0528000332310001010840000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
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
			--mcc-json ${MCC_JSON_BUILD_PATH}
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
			--mcc-json ${MCC_JSON_BUILD_PATH}
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
			--mcc-json ${MCC_JSON_BUILD_PATH}
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
		"  - C-4 kernel version 2.7 or later[\r\n]"
	)
	set_tests_properties(emv_decode_9F6E_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F6E_test4_regex}
	)

	add_test(NAME emv_decode_IAD_test1
		COMMAND emv-decode --issuer-app-data 0FA526A83100000000000001500000000F240000000000000000000000000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_IAD_test1_regex
		"^Application: CCD-Compliant[\r\n]"
		"IAD Format: CCD Version 4.1[\r\n]"
		"Cryptogram Version: TDES[\r\n]"
		"Derivation Key Index \\(DKI\\): 26[\r\n]"
		"Card Verification Results \\(CVR\\): Second GENERATE AC Not Requested[\r\n]"
		"Card Verification Results \\(CVR\\): First GENERATE AC returned ARQC[\r\n]"
		"Card Verification Results \\(CVR\\): Combined DDA/Application Cryptogram Generation \\(CDA\\) Performed[\r\n]"
		"Card Verification Results \\(CVR\\): PIN Try Counter is 3[\r\n]"
		"Card Verification Results \\(CVR\\): Last Online Transaction Not Completed[\r\n]"
	)
	set_tests_properties(emv_decode_IAD_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_IAD_test1_regex}
	)

	add_test(NAME emv_decode_IAD_test2
		COMMAND emv-decode --issuer-app-data 0110A04003223000000000001126980002FF
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_IAD_test2_regex
		"^Application: M/Chip 4[\r\n]"
		"Derivation Key Index \\(DKI\\): 01[\r\n]"
		"Cryptogram Version Number \\(CVN\\): 10[\r\n]"
		"Cryptogram: Mastercard Proprietary SKD session key[\r\n]"
		"Cryptogram: Counters not included in AC data[\r\n]"
		"Card Verification Results \\(CVR\\): Second GENERATE AC Not Requested[\r\n]"
		"Card Verification Results \\(CVR\\): First GENERATE AC returned ARQC[\r\n]"
		"Card Verification Results \\(CVR\\): Combined DDA/Application Cryptogram Generation \\(CDA\\) Returned in First GENERATE AC[\r\n]"
		"Card Verification Results \\(CVR\\): PIN Try Counter is 3[\r\n]"
		"Card Verification Results \\(CVR\\): Offline PIN Verification Not Performed[\r\n]"
		"Card Verification Results \\(CVR\\): Domestic Transaction[\r\n]"
		"Card Verification Results \\(CVR\\): Lower Cumulative Offline Limit Exceeded[\r\n]"
		"Card Verification Results \\(CVR\\): Upper Cumulative Offline Limit Exceeded[\r\n]"
	)
	set_tests_properties(emv_decode_IAD_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_IAD_test2_regex}
	)

	add_test(NAME emv_decode_IAD_test3
		COMMAND emv-decode --issuer-app-data 0110A04203A20000000000000000000000FF00000000000000FF
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_IAD_test3_regex
		"^Application: M/Chip Advance[\r\n]"
		"Derivation Key Index \\(DKI\\): 01[\r\n]"
		"Cryptogram Version Number \\(CVN\\): 10[\r\n]"
		"Cryptogram: Mastercard Proprietary SKD session key[\r\n]"
		"Cryptogram: Counters not included in AC data[\r\n]"
		"Card Verification Results \\(CVR\\): Second GENERATE AC Not Requested[\r\n]"
		"Card Verification Results \\(CVR\\): First GENERATE AC returned ARQC[\r\n]"
		"Card Verification Results \\(CVR\\): Combined DDA/Application Cryptogram Generation \\(CDA\\) Returned in First GENERATE AC[\r\n]"
		"Card Verification Results \\(CVR\\): Issuer Discretionary[\r\n]"
		"Card Verification Results \\(CVR\\): PIN Try Counter is 3[\r\n]"
		"Card Verification Results \\(CVR\\): Last Online Transaction Not Completed[\r\n]"
		"Card Verification Results \\(CVR\\): Offline PIN Verification Not Performed[\r\n]"
		"Card Verification Results \\(CVR\\): Domestic Transaction[\r\n]"
	)
	set_tests_properties(emv_decode_IAD_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_IAD_test3_regex}
	)

	add_test(NAME emv_decode_IAD_test4
		COMMAND emv-decode --issuer-app-data 06011203A000000F0300001030800000046920AE952C48
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_IAD_test4_regex
		"^Application: Visa Smart Debit/Credit \\(VSDC\\)[\r\n]"
		"IAD Format: 1[\r\n]"
		"Derivation Key Index \\(DKI\\): 01[\r\n]"
		"Cryptogram Version Number \\(CVN\\): 12 \\(CVN18\\)[\r\n]"
		"Card Verification Results \\(CVR\\): Second GENERATE AC Not Requested[\r\n]"
		"Card Verification Results \\(CVR\\): First GENERATE AC returned ARQC[\r\n]"
	)
	set_tests_properties(emv_decode_IAD_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_IAD_test4_regex}
	)

	add_test(NAME emv_decode_IAD_test5
		COMMAND emv-decode --issuer-app-data 1F43FE82A0000000000000000003075300000000000000000000000000000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_IAD_test5_regex
		"^Application: Visa Smart Debit/Credit \\(VSDC\\)[\r\n]"
		"IAD Format: 4[\r\n]"
		"Cryptogram Version Number \\(CVN\\): 43 \\(CVN'43'\\)[\r\n]"
		"Derivation Key Index \\(DKI\\): FE[\r\n]"
		"Card Verification Results \\(CVR\\): Verified by the mobile device OS[\r\n]"
		"Card Verification Results \\(CVR\\): Finger biometric[\r\n]"
		"Card Verification Results \\(CVR\\): Second GENERATE AC Not Requested[\r\n]"
		"Card Verification Results \\(CVR\\): GPO returned ARQC[\r\n]"
	)
	set_tests_properties(emv_decode_IAD_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_IAD_test5_regex}
	)

	add_test(NAME emv_decode_9F1D_test1
		COMMAND emv-decode --terminal-risk-management-data 6C7A800000000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F1D_test1_regex
		"^Enciphered PIN verified online \\(Contactless\\)[\r\n]"
		"Signature \\(paper\\) \\(Contactless\\)[\r\n]"
		"No CVM required \\(Contactless\\)[\r\n]"
		"CDCVM \\(Contactless\\)[\r\n]"
		"Enciphered PIN verified online \\(Contact\\)[\r\n]"
		"Signature \\(paper\\) \\(Contact\\)[\r\n]"
		"Enciphered PIN verification performed by ICC \\(Contact\\)[\r\n]"
		"No CVM required \\(Contact\\)[\r\n]"
		"Plaintext PIN verification performed by ICC \\(Contact\\)[\r\n]"
		"Mag-stripe mode contactless transactions not supported[\r\n]"
	)
	set_tests_properties(emv_decode_9F1D_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F1D_test1_regex}
	)

	add_test(NAME emv_decode_9F1D_test2
		COMMAND emv-decode --9F1D 4C7A800000000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F1D_test2_regex
		"^Enciphered PIN verified online \\(Contactless\\)[\r\n]"
		"No CVM required \\(Contactless\\)[\r\n]"
		"CDCVM \\(Contactless\\)[\r\n]"
		"Enciphered PIN verified online \\(Contact\\)[\r\n]"
		"Signature \\(paper\\) \\(Contact\\)[\r\n]"
		"Enciphered PIN verification performed by ICC \\(Contact\\)[\r\n]"
		"No CVM required \\(Contact\\)[\r\n]"
		"Plaintext PIN verification performed by ICC \\(Contact\\)[\r\n]"
		"Mag-stripe mode contactless transactions not supported[\r\n]"
	)
	set_tests_properties(emv_decode_9F1D_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F1D_test2_regex}
	)

	# From actual Visa transaction
	add_test(NAME emv_decode_issuer_response_test1
		COMMAND emv-decode --tlv 89063130363630398A023030910A50FC6205038200003030
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test1_regex
		"^89 \\| Authorisation Code : \\[6\\] 31 30 36 36 30 39 \"106609\"[\r\n]"
		"8A \\| Authorisation Response Code : \\[2\\] 30 30 \"00 - Approved or completed successfully\"[\r\n]"
		"91 \\| Issuer Authentication Data : \\[10\\] 50 FC 62 05 03 82 00 00 30 30[\r\n]"
		"  - Authorisation Response Cryptogram \\(ARPC\\): 50FC620503820000[\r\n]"
		"  - Authorisation Response Code: 00[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test1_regex}
	)

	# From actual Visa transaction
	add_test(NAME emv_decode_issuer_response_test2
		COMMAND emv-decode --tlv 89063632373132337211860F04DA9F6B0A000000050000919863528A0230309108A584F94900820000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test2_regex
		"^89 \\| Authorisation Code : \\[6\\] 36 32 37 31 32 33 \"627123\"[\r\n]"
		"72 \\| Issuer Script Template 2 : \\[17\\][\r\n]"
		"  86 \\| Issuer Script Command : \\[15\\] 04 DA 9F 6B 0A 00 00 00 05 00 00 91 98 63 52 \\(PUT DATA for BER-TLV field 9F6B\\)[\r\n]"
		"8A \\| Authorisation Response Code : \\[2\\] 30 30 \"00 - Approved or completed successfully\"[\r\n]"
		"91 \\| Issuer Authentication Data : \\[8\\] A5 84 F9 49 00 82 00 00[\r\n]"
		"  - Authorisation Response Cryptogram \\(ARPC\\): A584F949[\r\n]"
		"  - Card Status Update \\(CSU\\): Issuer Approves Online Transaction[\r\n]"
		"  - Card Status Update \\(CSU\\): Reset Offline Counters to Zero[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test2_regex}
	)

	# From actual Mastercard transaction
	add_test(NAME emv_decode_issuer_response_test3
		COMMAND emv-decode --tlv 910AFC436535D614F37300108A0230308906383139353334
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test3_regex
		"^91 \\| Issuer Authentication Data : \\[10\\] FC 43 65 35 D6 14 F3 73 00 10[\r\n]"
		"  - Authorisation Response Cryptogram \\(ARPC\\): FC436535D614F373[\r\n]"
		"  - M/Chip ARPC Response Code: 0010[\r\n]"
		"8A \\| Authorisation Response Code : \\[2\\] 30 30 \"00 - Approved or completed successfully\"[\r\n]"
		"89 \\| Authorisation Code : \\[6\\] 38 31 39 35 33 34 \"819534\"[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test3_regex}
	)

	# From Implementing American Express EMV Acceptance on a Terminal (Oct 2007), page 16, table 5
	# Test with verbosity
	add_test(NAME emv_decode_issuer_response_test4
		COMMAND emv-decode --verbose --tlv 72459F18048000000086158424000210FEBF34F00B7CE770DC61DA847BFB1E59862504DA8E00200000000000000000420141035E031F020000000000000000AC7F4DF1D624A0ED
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test4_regex
		"^72 \\| Issuer Script Template 2 : \\[69\\][\r\n]"
		"  9F18 \\| Issuer Script Identifier : \\[4\\] 80 00 00 00[\r\n]"
		"  86 \\| Issuer Script Command : \\[21\\] 84 24 00 02 10 FE BF 34 F0 0B 7C E7 70 DC 61 DA 84 7B FB 1E 59 \\(PIN CHANGE/UNBLOCK\\)[\r\n]"
		"  86 \\| Issuer Script Command : \\[37\\] 04 DA 8E 00 20 00 00 00 00 00 00 00 00 42 01 41 03 5E 03 1F 02 00 00 00 00 00 00 00 00 AC 7F 4D F1 D6 24 A0 ED \\(PUT DATA for BER-TLV field 8E00\\)[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test4_regex}
	)

	# From Implementing American Express EMV Acceptance on a Terminal (Oct 2007), page 16, table 5
	# Test without verbosity
	add_test(NAME emv_decode_issuer_response_test5
		COMMAND emv-decode --tlv 72459F18048000000086158424000210FEBF34F00B7CE770DC61DA847BFB1E59862504DA8E00200000000000000000420141035E031F020000000000000000AC7F4DF1D624A0ED
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test5_regex
		"^72 \\| Issuer Script Template 2 : \\[69\\][\r\n]"
		"  9F18 \\| Issuer Script Identifier : \\[4\\] 80 00 00 00[\r\n]"
		"  86 \\| Issuer Script Command : \\[21\\] 84 24 00 02 10 FE BF 34 \.\.\. DC 61 DA 84 7B FB 1E 59 \\(PIN CHANGE/UNBLOCK\\)[\r\n]"
		"  86 \\| Issuer Script Command : \\[37\\] 04 DA 8E 00 20 00 00 00 \.\.\. AC 7F 4D F1 D6 24 A0 ED \\(PUT DATA for BER-TLV field 8E00\\)[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test5_regex}
	)

	# From Implementing American Express EMV Acceptance on a Terminal (Oct 2007), page 17, table 6
	add_test(NAME emv_decode_issuer_response_test6
		COMMAND emv-decode --tlv 72179F180400004000860E04DA9F580900C7356286E3779889
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test6_regex
		"^72 \\| Issuer Script Template 2 : \\[23\\][\r\n]"
		"  9F18 \\| Issuer Script Identifier : \\[4\\] 00 00 40 00[\r\n]"
		"  86 \\| Issuer Script Command : \\[14\\] 04 DA 9F 58 09 00 C7 35 62 86 E3 77 98 89 \\(PUT DATA for BER-TLV field 9F58\\)[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test6
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test6_regex}
	)

	add_test(NAME emv_decode_9F27_test1
		COMMAND emv-decode --tlv 9F270160
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F27_test1_regex
		"^9F27 \\| Cryptogram Information Data : \\[1\\] 60[\r\n]"
		"  - Application Cryptogram \\(AC\\) type: Transaction Certificate \\(TC\\)[\r\n]"
		"  - Payment System-specific cryptogram: 0x20[\r\n]"
	)
	set_tests_properties(emv_decode_9F27_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F27_test1_regex}
	)

	add_test(NAME emv_decode_9F27_test2
		COMMAND emv-decode --tlv 9F270101
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F27_test2_regex
		"^9F27 \\| Cryptogram Information Data : \\[1\\] 01[\r\n]"
		"  - Application Cryptogram \\(AC\\) type: Application Authentication Cryptogram \\(AAC\\)[\r\n]"
		"  - No advice required[\r\n]"
		"  - Advice: Service not allowed[\r\n]"
	)
	set_tests_properties(emv_decode_9F27_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F27_test2_regex}
	)

	add_test(NAME emv_decode_9F27_test3
		COMMAND emv-decode --tlv 9F27018A
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F27_test3_regex
		"^9F27 \\| Cryptogram Information Data : \\[1\\] 8A[\r\n]"
		"  - Application Cryptogram \\(AC\\) type: Authorisation Request Cryptogram \\(ARQC\\)[\r\n]"
		"  - Advice required[\r\n]"
		"  - Advice: PIN Try Limit exceeded[\r\n]"
	)
	set_tests_properties(emv_decode_9F27_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F27_test3_regex}
	)

	add_test(NAME emv_decode_padding_test1
		COMMAND emv-decode --ber 701870105F2A0208409F1A0208409F15025999123456789ABCDED002DEADFEDC --ignore-padding
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_padding_test1_regex
		"^70 : \\[24\\][\r\n]"
		"  70 : \\[16\\][\r\n]"
		"    5F2A : \\[2\\] 08 40[\r\n]"
		"    9F1A : \\[2\\] 08 40[\r\n]"
		"    9F15 : \\[2\\] 59 99[\r\n]"
		"    Padding : \\[1\\] 12[\r\n]"
		"  Padding : \\[6\\] 34 56 78 9A BC DE[\r\n]"
		"D0 : \\[2\\] DE AD[\r\n]"
		"Padding : \\[2\\] FE DC[\r\n]$"
	)
	set_tests_properties(emv_decode_padding_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_padding_test1_regex}
	)

	add_test(NAME emv_decode_padding_test2
		COMMAND emv-decode --ber 701870105F2A0208409F1A0208409F15025999123456789ABCDED002DEADFEDC
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_padding_test2_regex
		"^70 : \\[24\\][\r\n]"
		"  70 : \\[16\\][\r\n]"
		"    5F2A : \\[2\\] 08 40[\r\n]"
		"    9F1A : \\[2\\] 08 40[\r\n]"
		"    9F15 : \\[2\\] 59 99[\r\n]"
		"BER decoding error -11 at offset 19\\; remaining invalid data: 12 34 56 78 9A BC DE D0 02 DE AD FE DC[\r\n]$"
	)
	set_tests_properties(emv_decode_padding_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_padding_test2_regex}
	)

	add_test(NAME emv_decode_padding_test3
		COMMAND emv-decode --tlv 701870105F2A0208409F1A0208409F15025999123456789ABCDED002DEADFEDC --ignore-padding
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_padding_test3_regex
		"^70 \\| EMV Data Template : \\[24\\][\r\n]"
		"  70 \\| EMV Data Template : \\[16\\][\r\n]"
		"    5F2A \\| Transaction Currency Code : \\[2\\] 08 40 \\(US Dollar\\)[\r\n]"
		"    9F1A \\| Terminal Country Code : \\[2\\] 08 40 \\(United States\\)[\r\n]"
		"    9F15 \\| Merchant Category Code \\(MCC\\) : \\[2\\] 59 99 \\(Miscellaneous and Specialty Retail Stores\\)[\r\n]"
		"    Padding : \\[1\\] 12[\r\n]"
		"  Padding : \\[6\\] 34 56 78 9A BC DE[\r\n]"
		"D0 : \\[2\\] DE AD[\r\n]"
		"Padding : \\[2\\] FE DC[\r\n]$"
	)
	set_tests_properties(emv_decode_padding_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_padding_test3_regex}
	)

	add_test(NAME emv_decode_padding_test4
		COMMAND emv-decode --tlv 701870105F2A0208409F1A0208409F15025999123456789ABCDED002DEADFEDC
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_padding_test4_regex
		"^70 \\| EMV Data Template : \\[24\\][\r\n]"
		"  70 \\| EMV Data Template : \\[16\\][\r\n]"
		"    5F2A \\| Transaction Currency Code : \\[2\\] 08 40 \\(US Dollar\\)[\r\n]"
		"    9F1A \\| Terminal Country Code : \\[2\\] 08 40 \\(United States\\)[\r\n]"
		"    9F15 \\| Merchant Category Code \\(MCC\\) : \\[2\\] 59 99 \\(Miscellaneous and Specialty Retail Stores\\)[\r\n]"
		"BER decoding error -11 at offset 19\\; remaining invalid data: 12 34 56 78 9A BC DE D0 02 DE AD FE DC[\r\n]$"
	)
	set_tests_properties(emv_decode_padding_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_padding_test4_regex}
	)

	# Test decoding of various ASN.1 types
	add_test(NAME emv_decode_asn1_test1
		COMMAND emv-decode --tlv 305D0101000101FF0203FFFFFF02037FFFFF0500060388370306092A864886F70D01010B0900090140090141090142090143090380FC01090580FE5555550908032B312E30652D330A03FFFFFE0A037FFFFE0D04C27B03021A054A6F6E6573
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_asn1_test1_regex
		"^30 \\| ASN.1 Sequence : \\[93\\][\r\n]"
		"  01 \\| ASN.1 Boolean : \\[1\\] 00 \\(False\\)[\r\n]"
		"  01 \\| ASN.1 Boolean : \\[1\\] FF \\(True\\)[\r\n]"
		"  02 \\| ASN.1 Integer : \\[3\\] FF FF FF \\(-1\\)[\r\n]"
		"  02 \\| ASN.1 Integer : \\[3\\] 7F FF FF \\(8388607\\)[\r\n]"
		"  05 \\| ASN.1 Null : \\[0\\][\r\n]"
		"  06 \\| ASN.1 Object Identifier \\(OID\\) : \\[3\\] 88 37 03 \\(2\\.999\\.3\\)[\r\n]" # See ISO 8825-1:2021, 8.19.5
		"  06 \\| ASN.1 Object Identifier \\(OID\\) : \\[9\\] 2A 86 48 86 F7 0D 01 01 0B \\(1\\.2\\.840\\.113549\\.1\\.1\\.11 sha256WithRSAEncryption\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[0\\] \\(0\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[1\\] 40 \\(PLUS-INFINITY\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[1\\] 41 \\(MINUS-INFINITY\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[1\\] 42 \\(NOT-A-NUMBER\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[1\\] 43 \\(-0\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[3\\] 80 FC 01 \\(0x01 x 2\\^-4\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[5\\] 80 FE 55 55 55 \\(0x555555 x 2\\^-2\\)[\r\n]"
		"  09 \\| ASN.1 Real : \\[8\\] 03 2B 31 2E 30 65 2D 33 \\(\\+1\\.0e-3\\)[\r\n]"
		"  0A \\| ASN.1 Enumerated : \\[3\\] FF FF FE \\(-2\\)[\r\n]"
		"  0A \\| ASN.1 Enumerated : \\[3\\] 7F FF FE \\(8388606\\)[\r\n]"
		"  0D \\| ASN.1 Relative object identifier : \\[4\\] C2 7B 03 02 \\(\\.8571\\.3\\.2\\)[\r\n]" # See ISO 8825-1:2021, 8.20.5
		"  1A \\| ASN.1 Visible \\(ISO 646\\) string : \\[5\\] 4A 6F 6E 65 73 \"Jones\"[\r\n]$" # See ISO 8825-1:2021, 8.23.5.4
	)
	set_tests_properties(emv_decode_asn1_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_asn1_test1_regex}
	)

	# Test decoding of ASN.1 objects as BER
	add_test(NAME emv_decode_asn1_test2
		COMMAND emv-decode --ber 301206035504030C0B6F70656E656D762E6F7267300D06092A864886F70D01010B0500301C060A2B810510864809180101060628CF04000102060628CC45010305
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_asn1_test2_regex
		"^30 : \\[18\\][\r\n]"
		"  06 : \\[3\\] 55 04 03 {2 5 4 3}[\r\n]"
		"  0C : \\[11\\] 6F 70 65 6E 65 6D 76 2E 6F 72 67 \"openemv\\.org\"[\r\n]"
		"30 : \\[13\\][\r\n]"
		"  06 : \\[9\\] 2A 86 48 86 F7 0D 01 01 0B {1 2 840 113549 1 1 11}[\r\n]"
		"  05 : \\[0\\][\r\n]"
		"30 : \\[28\\][\r\n]"
		"  06 : \\[10\\] 2B 81 05 10 86 48 09 18 01 01 {1 3 133 16 840 9 24 1 1}[\r\n]"
		"  06 : \\[6\\] 28 CF 04 00 01 02 {1 0 10116 0 1 2}[\r\n]"
		"  06 : \\[6\\] 28 CC 45 01 03 05 {1 0 9797 1 3 5}[\r\n]$"

	)
	set_tests_properties(emv_decode_asn1_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_asn1_test2_regex}
	)

	# Test fancy decoding of ASN.1 objects
	add_test(NAME emv_decode_asn1_test3
		COMMAND emv-decode --tlv 301206035504030C0B6F70656E656D762E6F7267300D06092A864886F70D01010B0500301C060A2B810510864809180101060628CF04000102060628CC45010305
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_asn1_test3_regex
		"^30 \\| ASN\\.1 Object \\(2\\.5\\.4\\.3 commonName\\) : \\[18\\][\r\n]"
		"  0C \\| ASN\\.1 UTF-8 string : \\[11\\] 6F 70 65 6E 65 6D 76 2E 6F 72 67 \"openemv\\.org\"[\r\n]"
		"30 \\| ASN\\.1 Object \\(1\\.2\\.840\\.113549\\.1\\.1\\.11 sha256WithRSAEncryption\\) : \\[13\\][\r\n]"
		"  05 \\| ASN\\.1 Null : \\[0\\][\r\n]"
		"30 \\| ASN\\.1 Object \\(1\\.3\\.133\\.16\\.840\\.9\\.24\\.1\\.1 dukpt_aes128\\) : \\[28\\][\r\n]"
		"  06 \\| ASN\\.1 Object Identifier \\(OID\\) : \\[6\\] 28 CF 04 00 01 02 \\(1\\.0\\.10116\\.0\\.1\\.2 cbc\\)[\r\n]"
		"  06 \\| ASN\\.1 Object Identifier \\(OID\\) : \\[6\\] 28 CC 45 01 03 05 \\(1\\.0\\.9797\\.1\\.3\\.5 cmac\\)[\r\n]$"
	)
	set_tests_properties(emv_decode_asn1_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_asn1_test3_regex}
	)

	# Test decoding of Offline Data Authentication fields with verbosity:
	# - Issuer Public Key Certificate (field 90)
	# - Signed Static Application Data (field 93)
	# - Integrated Circuit Card (ICC) Public Key Certificate (field 9F46)
	# - Signed Dynamic Application Data (field 9F4B)
	add_test(NAME emv_decode_oda_test1
		COMMAND emv-decode --verbose --tlv "
		9081F8665CD65C20DEAE638C7320EA011E5E2B33FC5070FF7D153D74FE9A01ABFF0B9587B3779C524577F8A57C19923B39CD3F5CCDD457D360DC2619CDBB94328777BB905E1CB79E280458F60C8C5593EFD22D6385512B11B7F2EAFE1184CF9066B9B47A0BF832045066359AE465473D31B9F830A6DE7D88E969CB456033F8073BEC512205920E3DEA773D3E36E1F46C2E8BDDC423FB675CA1710A3D0A06E9C7570919735190BD6ED65BD5EF92C0416BFE4094EA96A218013838EF337151A8BE7222DCF0717399553C4DDA16EBABB2DD386A07BDF313D970C1324CAAB885067691E3EE5E5D8B912799BD53C8E1830237E9EC0A9254D80B2BD3622C
		9381B030661BC4D33D38F913D48429E6760FD9BDF2D9172A22F30418A29138A2D35A473E2AE42A3A6E6EEDFBF99D6C8C21F12EB96FD717D67AE222DB53863257EC8D7D649E40F2A64D18659B2FB45D893A995B88AEC4209975975E8DB3AC516D4CDF4A26681C524F9EA5C3750283A2B8F4569F9A9672DE9B6ED2C529B9611B38F237C8BDCFF088988CB2DD6599B8E62D4E77EE5386B892BE36B7BFB553A2BFDE900FB774C1A544CEB2C5A6CE9968927ED38B87
		9F4681B0A6CDB529A1D8CB538E19A277764C633F3C2F468A6A7CC3B5EF3F5B9838B7900316A627927569F679EAE367E10160D7A1E08D9F20399F8B6FFE77A564B108DD924458A89D966C980D1E52A0093CA1CFA44CB3FCCF102632A799486B409C34C1F6A3DCC3C1C0EE1C0F6E1EA7A33C1CDAEA9852AAA4B954D429C9EEDBF8803D6D159CAB6B8DCAB0694B5850D5F3C4014B45ABCB5B8F69BFC647D94F9E0039439A2D5584DEAD27729C894615F62AE7AC8216
		9F4B81B05538A308C1CB67EAF5835EFD5AE98EF0C12EF0043C633EBC4F612C25C2F88AC66F1A8C8D6974731F5480FF5D45DB4AA9459CB514170FBF2456BA57AFDAFE074DE7D6837F1ECB71D7E567BB6BFFD42FC4B637A270217374BB4DF68A99BF056ED31B6BB85426BD2B4E5C76DB6B6E6C85E5ABC4B067E71D21A53E2F6ABBEB4B603DD79C653CB37B4ED3C235B1663AA46540E239EFFEC7CBB13B0560CBFA69B342C0A2E3E670EB85E4E97A2F5BEDB5904F4A
		9F482A4DD8A50B21B9CBF62BFAD4BB3F4CF6B5239F3FD23F8B93E96C84C9CE67DFD70359153855A8F735CAFBE5
		9081B03312205B0EFA6715BA18134BB2168A9CA250D268853506D925DD72B2A2E0CF107518DE182F0089B255B7D70A189D90859E741E7D79BB5D36339B16D9B1508DDBC35E2A3008B923E14417A42E86D3AD0A1AD6DBB394E7BC0BA812F3D4971412DE4997C8B34BA93F3CE0F5AE27E0A052DB57CD7595C467E831041AC2DBEC91E2EC8D27D4EABAA31322B769415B550B8B18E3B05B32CAE7813AB21D8F3B2BFF3741C6AD967AE694800F5B24A20EF78013E4
		93705A09ECA5B9B89AC19E97631899A3C2AC60D0CA29AA17B6903B9B262712C6E8934769B47A6FB3DDFA54612FF988FAD41F80D0CA46BA65E01E24C02B4B974ACA7485F4B6DEE3A946D3E4FC6BC22360C00F0DFC089C3426742A346C3759A1E82155E6F4E2CB3DF4471419998D2AC82DE243
		9F4670160F46545196AC0E862B3F798E3603FA8C549A6F1F3E81050A85984E16EA7CF7C7FC4A6E568E31459106F62156B462C2D44221533BDBFFE8BF79C0B442B70398C6382DEED8DD9573D1DB5313B493EAC8B264BC338E53296B769CF96C39C3F3F9CB955FB34E1CF8EC15E20F9E674DA461
		9F4B607D6C49A0C5DF0F4F947E5DCF5FAA29D6C63A0691F67B174E19AF0341EB02DDE67F0E928D26B1F10E76159F5D1F5FD04FEF17B01BE14B7AF92051AD0AA25BC3C1BACB0B0A89C3487A54BF7CF03E0812D0AA5576CB3BAE52413D3E955729633B06
		9F481A0000000000000006600000000000000000000000000000000055"
		--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_oda_test1_regex
		"^90 \\| Issuer Public Key Certificate : \\[248\\] 66 5C D6 5C 20 DE AE 63 8C 73 20 EA 01 1E 5E 2B 33 FC 50 70 FF 7D 15 3D 74 FE 9A 01 AB FF 0B 95 87 B3 77 9C 52 45 77 F8 A5 7C 19 92 3B 39 CD 3F 5C CD D4 57 D3 60 DC 26 19 CD BB 94 32 87 77 BB 90 5E 1C B7 9E 28 04 58 F6 0C 8C 55 93 EF D2 2D 63 85 51 2B 11 B7 F2 EA FE 11 84 CF 90 66 B9 B4 7A 0B F8 32 04 50 66 35 9A E4 65 47 3D 31 B9 F8 30 A6 DE 7D 88 E9 69 CB 45 60 33 F8 07 3B EC 51 22 05 92 0E 3D EA 77 3D 3E 36 E1 F4 6C 2E 8B DD C4 23 FB 67 5C A1 71 0A 3D 0A 06 E9 C7 57 09 19 73 51 90 BD 6E D6 5B D5 EF 92 C0 41 6B FE 40 94 EA 96 A2 18 01 38 38 EF 33 71 51 A8 BE 72 22 DC F0 71 73 99 55 3C 4D DA 16 EB AB B2 DD 38 6A 07 BD F3 13 D9 70 C1 32 4C AA B8 85 06 76 91 E3 EE 5E 5D 8B 91 27 99 BD 53 C8 E1 83 02 37 E9 EC 0A 92 54 D8 0B 2B D3 62 2C[\r\n]"
		"  - Retrieved using CAPK A000000003 #94[\r\n]"
		"  - Certificate Format: 02 \\(Issuer Public Key Certificate using RSA\\)[\r\n]"
		"  - Issuer Identifier: 476173FF[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/31[\r\n]"
		"  - Certificate Serial Number: 03DA0A[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 176 bytes / 1408 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"93 \\| Signed Static Application Data \\(SSAD\\) : \\[176\\] 30 66 1B C4 D3 3D 38 F9 13 D4 84 29 E6 76 0F D9 BD F2 D9 17 2A 22 F3 04 18 A2 91 38 A2 D3 5A 47 3E 2A E4 2A 3A 6E 6E ED FB F9 9D 6C 8C 21 F1 2E B9 6F D7 17 D6 7A E2 22 DB 53 86 32 57 EC 8D 7D 64 9E 40 F2 A6 4D 18 65 9B 2F B4 5D 89 3A 99 5B 88 AE C4 20 99 75 97 5E 8D B3 AC 51 6D 4C DF 4A 26 68 1C 52 4F 9E A5 C3 75 02 83 A2 B8 F4 56 9F 9A 96 72 DE 9B 6E D2 C5 29 B9 61 1B 38 F2 37 C8 BD CF F0 88 98 8C B2 DD 65 99 B8 E6 2D 4E 77 EE 53 86 B8 92 BE 36 B7 BF B5 53 A2 BF DE 90 0F B7 74 C1 A5 44 CE B2 C5 A6 CE 99 68 92 7E D3 8B 87[\r\n]"
		"  - Retrieved using issuer certificate 03DA0A[\r\n]"
		"  - Signed Data Format: 03 \\(Signed Static Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Data Authentication Code: DAC0[\r\n]"
		"9F46 \\| Integrated Circuit Card \\(ICC\\) Public Key Certificate : \\[176\\] A6 CD B5 29 A1 D8 CB 53 8E 19 A2 77 76 4C 63 3F 3C 2F 46 8A 6A 7C C3 B5 EF 3F 5B 98 38 B7 90 03 16 A6 27 92 75 69 F6 79 EA E3 67 E1 01 60 D7 A1 E0 8D 9F 20 39 9F 8B 6F FE 77 A5 64 B1 08 DD 92 44 58 A8 9D 96 6C 98 0D 1E 52 A0 09 3C A1 CF A4 4C B3 FC CF 10 26 32 A7 99 48 6B 40 9C 34 C1 F6 A3 DC C3 C1 C0 EE 1C 0F 6E 1E A7 A3 3C 1C DA EA 98 52 AA A4 B9 54 D4 29 C9 EE DB F8 80 3D 6D 15 9C AB 6B 8D CA B0 69 4B 58 50 D5 F3 C4 01 4B 45 AB CB 5B 8F 69 BF C6 47 D9 4F 9E 00 39 43 9A 2D 55 84 DE AD 27 72 9C 89 46 15 F6 2A E7 AC 82 16[\r\n]"
		"  - Retrieved using issuer certificate 03DA0A[\r\n]"
		"  - Certificate Format: 04 \\(ICC Public Key Certificate using RSA\\)[\r\n]"
		"  - Application PAN: 4761739001010119[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/22[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 176 bytes / 1408 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"9F4B \\| Signed Dynamic Application Data \\(SDAD\\) : \\[176\\] 55 38 A3 08 C1 CB 67 EA F5 83 5E FD 5A E9 8E F0 C1 2E F0 04 3C 63 3E BC 4F 61 2C 25 C2 F8 8A C6 6F 1A 8C 8D 69 74 73 1F 54 80 FF 5D 45 DB 4A A9 45 9C B5 14 17 0F BF 24 56 BA 57 AF DA FE 07 4D E7 D6 83 7F 1E CB 71 D7 E5 67 BB 6B FF D4 2F C4 B6 37 A2 70 21 73 74 BB 4D F6 8A 99 BF 05 6E D3 1B 6B B8 54 26 BD 2B 4E 5C 76 DB 6B 6E 6C 85 E5 AB C4 B0 67 E7 1D 21 A5 3E 2F 6A BB EB 4B 60 3D D7 9C 65 3C B3 7B 4E D3 C2 35 B1 66 3A A4 65 40 E2 39 EF FE C7 CB B1 3B 05 60 CB FA 69 B3 42 C0 A2 E3 E6 70 EB 85 E4 E9 7A 2F 5B ED B5 90 4F 4A[\r\n]"
		"  - Retrieved using ICC certificate 000001[\r\n]"
		"  - Signed Data Format: 05 \\(Signed Dynamic Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - ICC Dynamic Number: 0112[\r\n]"
		"9F48 \\| Integrated Circuit Card \\(ICC\\) Public Key Remainder : \\[42\\] 4D D8 A5 0B 21 B9 CB F6 2B FA D4 BB 3F 4C F6 B5 23 9F 3F D2 3F 8B 93 E9 6C 84 C9 CE 67 DF D7 03 59 15 38 55 A8 F7 35 CA FB E5[\r\n]"
		"90 \\| Issuer Public Key Certificate : \\[176\\] 33 12 20 5B 0E FA 67 15 BA 18 13 4B B2 16 8A 9C A2 50 D2 68 85 35 06 D9 25 DD 72 B2 A2 E0 CF 10 75 18 DE 18 2F 00 89 B2 55 B7 D7 0A 18 9D 90 85 9E 74 1E 7D 79 BB 5D 36 33 9B 16 D9 B1 50 8D DB C3 5E 2A 30 08 B9 23 E1 44 17 A4 2E 86 D3 AD 0A 1A D6 DB B3 94 E7 BC 0B A8 12 F3 D4 97 14 12 DE 49 97 C8 B3 4B A9 3F 3C E0 F5 AE 27 E0 A0 52 DB 57 CD 75 95 C4 67 E8 31 04 1A C2 DB EC 91 E2 EC 8D 27 D4 EA BA A3 13 22 B7 69 41 5B 55 0B 8B 18 E3 B0 5B 32 CA E7 81 3A B2 1D 8F 3B 2B FF 37 41 C6 AD 96 7A E6 94 80 0F 5B 24 A2 0E F7 80 13 E4[\r\n]"
		"  - Retrieved using CAPK A000000004 #F1[\r\n]"
		"  - Certificate Format: 02 \\(Issuer Public Key Certificate using RSA\\)[\r\n]"
		"  - Issuer Identifier: 541333FF[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/27[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 112 bytes / 896 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"93 \\| Signed Static Application Data \\(SSAD\\) : \\[112\\] 5A 09 EC A5 B9 B8 9A C1 9E 97 63 18 99 A3 C2 AC 60 D0 CA 29 AA 17 B6 90 3B 9B 26 27 12 C6 E8 93 47 69 B4 7A 6F B3 DD FA 54 61 2F F9 88 FA D4 1F 80 D0 CA 46 BA 65 E0 1E 24 C0 2B 4B 97 4A CA 74 85 F4 B6 DE E3 A9 46 D3 E4 FC 6B C2 23 60 C0 0F 0D FC 08 9C 34 26 74 2A 34 6C 37 59 A1 E8 21 55 E6 F4 E2 CB 3D F4 47 14 19 99 8D 2A C8 2D E2 43[\r\n]"
		"  - Retrieved using issuer certificate 000001[\r\n]"
		"  - Signed Data Format: 03 \\(Signed Static Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Data Authentication Code: DAC0[\r\n]"
		"9F46 \\| Integrated Circuit Card \\(ICC\\) Public Key Certificate : \\[112\\] 16 0F 46 54 51 96 AC 0E 86 2B 3F 79 8E 36 03 FA 8C 54 9A 6F 1F 3E 81 05 0A 85 98 4E 16 EA 7C F7 C7 FC 4A 6E 56 8E 31 45 91 06 F6 21 56 B4 62 C2 D4 42 21 53 3B DB FF E8 BF 79 C0 B4 42 B7 03 98 C6 38 2D EE D8 DD 95 73 D1 DB 53 13 B4 93 EA C8 B2 64 BC 33 8E 53 29 6B 76 9C F9 6C 39 C3 F3 F9 CB 95 5F B3 4E 1C F8 EC 15 E2 0F 9E 67 4D A4 61[\r\n]"
		"  - Retrieved using issuer certificate 000001[\r\n]"
		"  - Certificate Format: 04 \\(ICC Public Key Certificate using RSA\\)[\r\n]"
		"  - Application PAN: 5413330089600036[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/27[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 96 bytes / 768 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"9F4B \\| Signed Dynamic Application Data \\(SDAD\\) : \\[96\\] 7D 6C 49 A0 C5 DF 0F 4F 94 7E 5D CF 5F AA 29 D6 C6 3A 06 91 F6 7B 17 4E 19 AF 03 41 EB 02 DD E6 7F 0E 92 8D 26 B1 F1 0E 76 15 9F 5D 1F 5F D0 4F EF 17 B0 1B E1 4B 7A F9 20 51 AD 0A A2 5B C3 C1 BA CB 0B 0A 89 C3 48 7A 54 BF 7C F0 3E 08 12 D0 AA 55 76 CB 3B AE 52 41 3D 3E 95 57 29 63 3B 06[\r\n]"
		"  - Retrieved using ICC certificate 000001[\r\n]"
		"  - Signed Data Format: 05 \\(Signed Dynamic Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - ICC Dynamic Number: 0644BC1F51F64C04[\r\n]"
		"  - Cryptogram Information Data: 00[\r\n]"
		"  - Application Cryptogram: 68EFA3CD9323C386[\r\n]"
		"9F48 \\| Integrated Circuit Card \\(ICC\\) Public Key Remainder : \\[26\\] 00 00 00 00 00 00 00 06 60 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 55[\r\n]$"
	)
	set_tests_properties(emv_decode_oda_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_oda_test1_regex}
	)

	# Test decoding of Offline Data Authentication fields without verbosity:
	# - Issuer Public Key Certificate (field 90)
	# - Signed Static Application Data (field 93)
	# - Integrated Circuit Card (ICC) Public Key Certificate (field 9F46)
	# - Signed Dynamic Application Data (field 9F4B)
	add_test(NAME emv_decode_oda_test2
		COMMAND emv-decode --tlv "
		9081F8665CD65C20DEAE638C7320EA011E5E2B33FC5070FF7D153D74FE9A01ABFF0B9587B3779C524577F8A57C19923B39CD3F5CCDD457D360DC2619CDBB94328777BB905E1CB79E280458F60C8C5593EFD22D6385512B11B7F2EAFE1184CF9066B9B47A0BF832045066359AE465473D31B9F830A6DE7D88E969CB456033F8073BEC512205920E3DEA773D3E36E1F46C2E8BDDC423FB675CA1710A3D0A06E9C7570919735190BD6ED65BD5EF92C0416BFE4094EA96A218013838EF337151A8BE7222DCF0717399553C4DDA16EBABB2DD386A07BDF313D970C1324CAAB885067691E3EE5E5D8B912799BD53C8E1830237E9EC0A9254D80B2BD3622C
		9381B030661BC4D33D38F913D48429E6760FD9BDF2D9172A22F30418A29138A2D35A473E2AE42A3A6E6EEDFBF99D6C8C21F12EB96FD717D67AE222DB53863257EC8D7D649E40F2A64D18659B2FB45D893A995B88AEC4209975975E8DB3AC516D4CDF4A26681C524F9EA5C3750283A2B8F4569F9A9672DE9B6ED2C529B9611B38F237C8BDCFF088988CB2DD6599B8E62D4E77EE5386B892BE36B7BFB553A2BFDE900FB774C1A544CEB2C5A6CE9968927ED38B87
		9F4681B0A6CDB529A1D8CB538E19A277764C633F3C2F468A6A7CC3B5EF3F5B9838B7900316A627927569F679EAE367E10160D7A1E08D9F20399F8B6FFE77A564B108DD924458A89D966C980D1E52A0093CA1CFA44CB3FCCF102632A799486B409C34C1F6A3DCC3C1C0EE1C0F6E1EA7A33C1CDAEA9852AAA4B954D429C9EEDBF8803D6D159CAB6B8DCAB0694B5850D5F3C4014B45ABCB5B8F69BFC647D94F9E0039439A2D5584DEAD27729C894615F62AE7AC8216
		9F4B81B05538A308C1CB67EAF5835EFD5AE98EF0C12EF0043C633EBC4F612C25C2F88AC66F1A8C8D6974731F5480FF5D45DB4AA9459CB514170FBF2456BA57AFDAFE074DE7D6837F1ECB71D7E567BB6BFFD42FC4B637A270217374BB4DF68A99BF056ED31B6BB85426BD2B4E5C76DB6B6E6C85E5ABC4B067E71D21A53E2F6ABBEB4B603DD79C653CB37B4ED3C235B1663AA46540E239EFFEC7CBB13B0560CBFA69B342C0A2E3E670EB85E4E97A2F5BEDB5904F4A
		9F482A4DD8A50B21B9CBF62BFAD4BB3F4CF6B5239F3FD23F8B93E96C84C9CE67DFD70359153855A8F735CAFBE5
		9081B03312205B0EFA6715BA18134BB2168A9CA250D268853506D925DD72B2A2E0CF107518DE182F0089B255B7D70A189D90859E741E7D79BB5D36339B16D9B1508DDBC35E2A3008B923E14417A42E86D3AD0A1AD6DBB394E7BC0BA812F3D4971412DE4997C8B34BA93F3CE0F5AE27E0A052DB57CD7595C467E831041AC2DBEC91E2EC8D27D4EABAA31322B769415B550B8B18E3B05B32CAE7813AB21D8F3B2BFF3741C6AD967AE694800F5B24A20EF78013E4
		93705A09ECA5B9B89AC19E97631899A3C2AC60D0CA29AA17B6903B9B262712C6E8934769B47A6FB3DDFA54612FF988FAD41F80D0CA46BA65E01E24C02B4B974ACA7485F4B6DEE3A946D3E4FC6BC22360C00F0DFC089C3426742A346C3759A1E82155E6F4E2CB3DF4471419998D2AC82DE243
		9F4670160F46545196AC0E862B3F798E3603FA8C549A6F1F3E81050A85984E16EA7CF7C7FC4A6E568E31459106F62156B462C2D44221533BDBFFE8BF79C0B442B70398C6382DEED8DD9573D1DB5313B493EAC8B264BC338E53296B769CF96C39C3F3F9CB955FB34E1CF8EC15E20F9E674DA461
		9F4B607D6C49A0C5DF0F4F947E5DCF5FAA29D6C63A0691F67B174E19AF0341EB02DDE67F0E928D26B1F10E76159F5D1F5FD04FEF17B01BE14B7AF92051AD0AA25BC3C1BACB0B0A89C3487A54BF7CF03E0812D0AA5576CB3BAE52413D3E955729633B06
		9F481A0000000000000006600000000000000000000000000000000055"
		--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_oda_test2_regex
		"^90 \\| Issuer Public Key Certificate : \\[248\\] 66 5C D6 5C 20 DE AE 63 \.\.\. 92 54 D8 0B 2B D3 62 2C[\r\n]"
		"  - Retrieved using CAPK A000000003 #94[\r\n]"
		"  - Certificate Format: 02 \\(Issuer Public Key Certificate using RSA\\)[\r\n]"
		"  - Issuer Identifier: 476173FF[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/31[\r\n]"
		"  - Certificate Serial Number: 03DA0A[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 176 bytes / 1408 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"93 \\| Signed Static Application Data \\(SSAD\\) : \\[176\\] 30 66 1B C4 D3 3D 38 F9 \.\.\. CE 99 68 92 7E D3 8B 87[\r\n]"
		"  - Retrieved using issuer certificate 03DA0A[\r\n]"
		"  - Signed Data Format: 03 \\(Signed Static Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Data Authentication Code: DAC0[\r\n]"
		"9F46 \\| Integrated Circuit Card \\(ICC\\) Public Key Certificate : \\[176\\] A6 CD B5 29 A1 D8 CB 53 \.\.\. 46 15 F6 2A E7 AC 82 16[\r\n]"
		"  - Retrieved using issuer certificate 03DA0A[\r\n]"
		"  - Certificate Format: 04 \\(ICC Public Key Certificate using RSA\\)[\r\n]"
		"  - Application PAN: 4761739001010119[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/22[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 176 bytes / 1408 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"9F4B \\| Signed Dynamic Application Data \\(SDAD\\) : \\[176\\] 55 38 A3 08 C1 CB 67 EA \.\.\. 7A 2F 5B ED B5 90 4F 4A[\r\n]"
		"  - Retrieved using ICC certificate 000001[\r\n]"
		"  - Signed Data Format: 05 \\(Signed Dynamic Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - ICC Dynamic Number: 0112[\r\n]"
		"9F48 \\| Integrated Circuit Card \\(ICC\\) Public Key Remainder : \\[42\\] 4D D8 A5 0B 21 B9 CB F6 \.\.\. 38 55 A8 F7 35 CA FB E5[\r\n]"
		"90 \\| Issuer Public Key Certificate : \\[176\\] 33 12 20 5B 0E FA 67 15 \.\.\. 5B 24 A2 0E F7 80 13 E4[\r\n]"
		"  - Retrieved using CAPK A000000004 #F1[\r\n]"
		"  - Certificate Format: 02 \\(Issuer Public Key Certificate using RSA\\)[\r\n]"
		"  - Issuer Identifier: 541333FF[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/27[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 112 bytes / 896 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"93 \\| Signed Static Application Data \\(SSAD\\) : \\[112\\] 5A 09 EC A5 B9 B8 9A C1 \.\.\. 19 99 8D 2A C8 2D E2 43[\r\n]"
		"  - Retrieved using issuer certificate 000001[\r\n]"
		"  - Signed Data Format: 03 \\(Signed Static Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Data Authentication Code: DAC0[\r\n]"
		"9F46 \\| Integrated Circuit Card \\(ICC\\) Public Key Certificate : \\[112\\] 16 0F 46 54 51 96 AC 0E \.\.\. 15 E2 0F 9E 67 4D A4 61[\r\n]"
		"  - Retrieved using issuer certificate 000001[\r\n]"
		"  - Certificate Format: 04 \\(ICC Public Key Certificate using RSA\\)[\r\n]"
		"  - Application PAN: 5413330089600036[\r\n]"
		"  - Certificate Expiration \\(MM/YY\\): 12/27[\r\n]"
		"  - Certificate Serial Number: 000001[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - Public Key Algorithm Indicator: 01 \\(RSA/SHA-1\\)[\r\n]"
		"  - Public Key Length: 96 bytes / 768 bits[\r\n]"
		"  - Public Key Exponent Length: 1 bytes[\r\n]"
		"9F4B \\| Signed Dynamic Application Data \\(SDAD\\) : \\[96\\] 7D 6C 49 A0 C5 DF 0F 4F \.\.\. 3D 3E 95 57 29 63 3B 06[\r\n]"
		"  - Retrieved using ICC certificate 000001[\r\n]"
		"  - Signed Data Format: 05 \\(Signed Dynamic Application Data using RSA\\)[\r\n]"
		"  - Hash Algorithm Indicator: 01 \\(SHA-1\\)[\r\n]"
		"  - ICC Dynamic Number: 0644BC1F51F64C04[\r\n]"
		"  - Cryptogram Information Data: 00[\r\n]"
		"  - Application Cryptogram: 68EFA3CD9323C386[\r\n]"
		"9F48 \\| Integrated Circuit Card \\(ICC\\) Public Key Remainder : \\[26\\] 00 00 00 00 00 00 00 06 \.\.\. 00 00 00 00 00 00 00 55[\r\n]$"
	)
	set_tests_properties(emv_decode_oda_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_oda_test2_regex}
	)

	# Test decoding of kernel identifier fields
	add_test(NAME emv_decode_kernel_id_test
		COMMAND emv-decode --tlv 9F2A01029F2A01039F2A038209789F2A01C59F2A0807000000000000009608080000C00000000096080700008000000000
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_kernel_id_test_regex
		"^9F2A \\| Kernel Identifier : \\[1\\] 02 \\(Kernel C-2, International\\)[\r\n]"
		"9F2A \\| Kernel Identifier : \\[1\\] 03 \\(Kernel C-3, International\\)[\r\n]"
		"9F2A \\| Kernel Identifier : \\[3\\] 82 09 78 \\(Kernel C-2, EMVCo, Euro\\)[\r\n]"
		"9F2A \\| Kernel Identifier : \\[1\\] C5 \\(Kernel C-5, Proprietary\\)[\r\n]"
		"9F2A \\| Kernel Identifier : \\[8\\] 07 00 00 00 00 00 00 00 \\(Kernel C-7, International\\)[\r\n]"
		"96 \\| Kernel Identifier - terminal : \\[8\\] 08 00 00 C0 00 00 00 00 \\(Kernel C-8, International, K8 reader\\+transaction support\\)[\r\n]"
		"96 \\| Kernel Identifier - terminal : \\[8\\] 07 00 00 80 00 00 00 00 \\(Kernel C-7, International, K8 reader support\\)[\r\n]$"
	)
	set_tests_properties(emv_decode_kernel_id_test
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_kernel_id_test_regex}
	)

	# Test decoding of POI terminal category fields
	add_test(NAME emv_decode_poi_terminal_category_test
		COMMAND emv-decode --tlv 8B008B0A000102000100010200028B0500010200998B0B0001020001BEEF031212129F3E0200019F3E04000100029F3E020003
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_poi_terminal_category_test_regex
		"^8B \\| POI Information : \\[0\\][\r\n]"
		"8B \\| POI Information : \\[10\\] 00 01 02 00 01 00 01 02 00 02[\r\n]"
		"  - Terminal Category: Transit gate[\r\n]"
		"  - Terminal Category: Loyalty[\r\n]"
		"8B \\| POI Information : \\[5\\] 00 01 02 00 99[\r\n]"
		"  - Terminal Category: 0x0099 \\(RFU\\)[\r\n]"
		"8B \\| POI Information : \\[11\\] 00 01 02 00 01 BE EF 03 12 12 12[\r\n]"
		"  - Terminal Category: Transit gate[\r\n]"
		"  - Unknown POI Information ID 0xBEEF[\r\n]"
		"9F3E \\| Terminal Categories Supported List : \\[2\\] 00 01[\r\n]"
		"  - Terminal Category: Transit gate[\r\n]"
		"9F3E \\| Terminal Categories Supported List : \\[4\\] 00 01 00 02[\r\n]"
		"  - Terminal Category: Transit gate[\r\n]"
		"  - Terminal Category: Loyalty[\r\n]"
		"9F3E \\| Terminal Categories Supported List : \\[2\\] 00 03[\r\n]"
		"  - Terminal Category: 0x0003 \\(RFU\\)[\r\n]$"
	)
	set_tests_properties(emv_decode_poi_terminal_category_test
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_poi_terminal_category_test_regex}
	)

	# Test decoding of 9F69 for kernel C-2
	add_test(NAME emv_decode_9F69_test1
		COMMAND emv-decode --tlv 9F69069F37049F0206
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F69_test1_regex
		"^9F69 \\| UDOL : \\[6\\] 9F 37 04 9F 02 06[\r\n]"
		"  Data Object List:[\r\n]"
		"    9F37 \\| Unpredictable Number \\[4\\][\r\n]"
		"    9F02 \\| Amount, Authorised \\(Numeric\\) \\[6\\][\r\n]$"
	)
	set_tests_properties(emv_decode_9F69_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F69_test1_regex}
	)

	# Test decoding of 9F69 for kernel C-3
	add_test(NAME emv_decode_9F69_test2
		COMMAND emv-decode --tlv 9F690701ABCDEF123600
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_9F69_test2_regex
		"^9F69 \\| Card Authentication Related Data : \\[7\\] 01 AB CD EF 12 36 00[\r\n]"
		"  - fDDA Version Number: 1[\r\n]"
		"  - Card Unpredictable Number: ABCDEF12[\r\n]"
		"  - Go Online if Offline Data Authentication Fails and Reader is online capable[\r\n]"
		"  - Switch Interface if Offline Data Authentication fails and Reader supports contact chip[\r\n]"
		"  - Switch Interface for Cash Transactions[\r\n]"
		"  - Switch Interface for Cashback Transactions[\r\n]$"
	)
	set_tests_properties(emv_decode_9F69_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_9F69_test2_regex}
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
