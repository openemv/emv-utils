##############################################################################
# Copyright (c) 2022-2024 Leon Lynch
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
		set_source_files_properties(../src/pcsc.c
			PROPERTIES
				COMPILE_DEFINITIONS USE_PCSCLITE
		)
	endif()

	add_executable(emv-tool emv-tool.c ../src/pcsc.c)
	target_link_libraries(emv-tool PRIVATE print_helpers emv emv_strings)
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
		COMMAND emv-decode --tlv 9A03221215
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	set_tests_properties(emv_decode_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^9A \\| Transaction Date : \\[3\\] 22 12 15 \\(2022-12-15\\)[\r\n]"
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
		COMMAND emv-decode --tlv 701F9F390105571040123456789095D2512201197339300F820259009F15025999
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_test8_regex
		"^70 \\| EMV Data Template : \\[31\\][\r\n]"
		"  9F39 \\| Point-of-Service \\(POS\\) Entry Mode : \\[1\\] 05 \\(Integrated circuit card \\(ICC\\)\\. CVV can be checked\\.\\)[\r\n]"
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
		"  - C-4 kernel version 2.7[\r\n]"
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
		"  - Card Status Update \\(CSU\\): 00820000[\r\n]"
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
	add_test(NAME emv_decode_issuer_response_test4
		COMMAND emv-decode --tlv 72459F18048000000086158424000210FEBF34F00B7CE770DC61DA847BFB1E59862504DA8E00200000000000000000420141035E031F020000000000000000AC7F4DF1D624A0ED
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

	# From Implementing American Express EMV Acceptance on a Terminal (Oct 2007), page 17, table 6
	add_test(NAME emv_decode_issuer_response_test5
		COMMAND emv-decode --tlv 72179F180400004000860E04DA9F580900C7356286E3779889
			--mcc-json ${MCC_JSON_BUILD_PATH}
	)
	string(CONCAT emv_decode_issuer_response_test5_regex
		"^72 \\| Issuer Script Template 2 : \\[23\\][\r\n]"
		"  9F18 \\| Issuer Script Identifier : \\[4\\] 00 00 40 00[\r\n]"
		"  86 \\| Issuer Script Command : \\[14\\] 04 DA 9F 58 09 00 C7 35 62 86 E3 77 98 89 \\(PUT DATA for BER-TLV field 9F58\\)[\r\n]"
	)
	set_tests_properties(emv_decode_issuer_response_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${emv_decode_issuer_response_test5_regex}
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
