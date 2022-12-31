##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

if (BUILD_TESTING)
	add_executable(emv_ttl_tpdu_test emv_ttl_tpdu_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_ttl_tpdu_test PRIVATE emv iso7816)
	add_test(emv_ttl_tpdu_test emv_ttl_tpdu_test)

	add_executable(emv_ttl_pcsc_test emv_ttl_pcsc_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_ttl_pcsc_test PRIVATE emv iso7816)
	add_test(emv_ttl_pcsc_test emv_ttl_pcsc_test)

	add_executable(emv_str_parse_test emv_str_parse_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_str_parse_test PRIVATE emv iso7816)
	add_test(emv_str_parse_test emv_str_parse_test)

	add_executable(emv_cvmlist_test emv_cvmlist_test.c)
	target_link_libraries(emv_cvmlist_test emv)
	add_test(emv_cvmlist_test emv_cvmlist_test)

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
