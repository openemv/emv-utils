##############################################################################
# Copyright (c) 2021-2023 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

if (BUILD_TESTING)
	add_executable(emv_atr_parse_test emv_atr_parse_test.c)
	target_link_libraries(emv_atr_parse_test PRIVATE print_helpers emv)
	add_test(emv_atr_parse_test emv_atr_parse_test)

	add_executable(emv_ttl_tpdu_test emv_ttl_tpdu_test.c)
	target_link_libraries(emv_ttl_tpdu_test PRIVATE print_helpers emv)
	add_test(emv_ttl_tpdu_test emv_ttl_tpdu_test)

	add_executable(emv_ttl_pcsc_test emv_ttl_pcsc_test.c)
	target_link_libraries(emv_ttl_pcsc_test PRIVATE print_helpers emv)
	add_test(emv_ttl_pcsc_test emv_ttl_pcsc_test)

	add_executable(emv_str_parse_test emv_str_parse_test.c)
	target_link_libraries(emv_str_parse_test PRIVATE print_helpers emv_strings)
	add_test(emv_str_parse_test emv_str_parse_test)

	add_executable(emv_aid_info_test emv_aid_info_test.c)
	target_link_libraries(emv_aid_info_test PRIVATE emv)
	add_test(emv_aid_info_test emv_aid_info_test)

	add_executable(emv_cvmlist_test emv_cvmlist_test.c)
	target_link_libraries(emv_cvmlist_test PRIVATE emv)
	add_test(emv_cvmlist_test emv_cvmlist_test)

	add_executable(isocodes_test isocodes_test.c)
	find_package(Intl)
	if(Intl_FOUND)
		target_compile_definitions(isocodes_test PRIVATE HAVE_LIBINTL_H)
		if(NOT Intl_IS_BUILT_IN)
			target_include_directories(isocodes_test PRIVATE ${Intl_INCLUDE_DIRS})
			target_link_libraries(isocodes_test PRIVATE ${Intl_LIBRARIES})
		endif()
	endif()
	target_link_libraries(isocodes_test PRIVATE emv_strings)
	add_test(isocodes_test isocodes_test)

	add_executable(mcc_test mcc_test.c)
	target_include_directories(mcc_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../src/) # For generated headers
	target_link_libraries(mcc_test PRIVATE emv_strings)
	add_test(mcc_test mcc_test)

	add_executable(iso8859_test iso8859_test.c)
	target_link_libraries(iso8859_test PRIVATE iso8859)
	add_test(iso8859_test iso8859_test)

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
