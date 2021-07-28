##############################################################################
# Copyright (c) 2021 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.11)

if (BUILD_TESTING)
	add_executable(emv_ttl_tpdu_test emv_ttl_tpdu_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_ttl_tpdu_test emv iso7816)
	add_test(emv_ttl_tpdu_test emv_ttl_tpdu_test)

	add_executable(emv_ttl_pcsc_test emv_ttl_pcsc_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_ttl_pcsc_test emv iso7816)
	add_test(emv_ttl_pcsc_test emv_ttl_pcsc_test)

	add_executable(emv_str_parse_test emv_str_parse_test.c ../tools/print_helpers.c)
	target_link_libraries(emv_str_parse_test emv iso7816)
	add_test(emv_str_parse_test emv_str_parse_test)
endif()
