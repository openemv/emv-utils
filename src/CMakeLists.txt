##############################################################################
# Copyright (c) 2022-2024 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Check whether C99 length modifier 'z' is supported
include(CheckCSourceRuns)
check_c_source_runs("
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#if defined(__clang__)
	__attribute__((format(printf, 1, 4)))
#elif defined(__GNUC__)
	__attribute__((format(gnu_printf, 1, 4)))
#endif
static int test_formatting(const char* fmt, char* s, size_t s_len, ...)
{
	int r;
	va_list ap;
	va_start(ap, s_len);
	r = vsnprintf(s, s_len, fmt, ap);
	va_end(ap);
	return r;
}
int main(void)
{
	char s[16] = { 0 };
	size_t x = 1337;
	return test_formatting(\"s=%zu\", s, sizeof(s), x) != 6 || strcmp(s, \"s=1337\") != 0;
}"
C99_length_modifier_z_is_supported
)
if(NOT C99_length_modifier_z_is_supported)
	message(FATAL_ERROR "C99 length modifier 'z' not supported")
endif()

# Check for time.h, timespec_get() and clock_gettime()
include(CheckIncludeFile)
check_include_file(time.h HAVE_TIME_H)
if(NOT HAVE_TIME_H)
	message(FATAL_ERROR "Failed to find time.h")
endif()
include(CheckSymbolExists)
check_symbol_exists(timespec_get time.h HAVE_TIMESPEC_GET)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
if(NOT HAVE_TIMESPEC_GET AND NOT HAVE_CLOCK_GETTIME)
	message(FATAL_ERROR "Failed to find either timespec_get or clock_gettime")
endif()

find_package(IsoCodes REQUIRED)

# Newer versions of json-c have a CMake config file while older versions, like
# the one for Ubuntu 20.04, do not. Therefore, first try to find the CMake
# config file and then fallback to the custom Findjson-c.cmake module
find_package(json-c CONFIG)
if(json-c_FOUND)
	# The json-c CMake config doesn't print a find_package() message but it is
	# always good to know where it was found
	message(STATUS "Found json-c: ${json-c_CONFIG}")
else()
	# Try custom Findjson-c.cmake module
	find_package(json-c REQUIRED)
endif()
# The EMV_UTILS_PACKAGE_DEPENDENCIES variable is set for the parent scope to
# facilitate the generation of CMake package configuration files.
list(APPEND EMV_UTILS_PACKAGE_DEPENDENCIES "json-c")
set(EMV_UTILS_PACKAGE_DEPENDENCIES ${EMV_UTILS_PACKAGE_DEPENDENCIES} PARENT_SCOPE)

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()

# Find and use mcc-codes submodule
set(MCC_JSON_SOURCE_FILE mcc_codes.json)
set(MCC_JSON_SOURCE_PATH ${PROJECT_SOURCE_DIR}/mcc-codes/${MCC_JSON_SOURCE_FILE})
if(EXISTS ${MCC_JSON_SOURCE_PATH})
	# Copy mcc-codes to build directory
	set(MCC_JSON_BINARY_DIR ${PROJECT_BINARY_DIR}/mcc-codes)
	file(COPY ${MCC_JSON_SOURCE_PATH} DESTINATION ${MCC_JSON_BINARY_DIR})

	# Install mcc-codes to runtime component
	set(EMV_UTILS_INSTALL_MCC_CODES_DIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME} CACHE STRING "Installation location for emv-utils mcc-codes files")
	message(STATUS "Using mcc-codes install location \"${EMV_UTILS_INSTALL_MCC_CODES_DIR}\"")
	install(FILES
		${MCC_JSON_SOURCE_PATH}
		DESTINATION ${EMV_UTILS_INSTALL_MCC_CODES_DIR}
		COMPONENT emv_runtime
	)

	# Default mcc-codes paths to be used by libraries and applications
	# The build path is also set for the parent scope to facilitate testing
	set(MCC_JSON_BUILD_PATH ${MCC_JSON_BINARY_DIR}/${MCC_JSON_SOURCE_FILE})
	set(MCC_JSON_BUILD_PATH ${MCC_JSON_BUILD_PATH} PARENT_SCOPE)
	set(MCC_JSON_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/${EMV_UTILS_INSTALL_MCC_CODES_DIR}/${MCC_JSON_SOURCE_FILE})
else()
	message(FATAL_ERROR "mcc-codes/mcc_codes.json not found")
endif()

# Generate emv-utils build configuration for internal use only
# This file should NOT be installed or used by an installed header
configure_file(emv_utils_config.h.in emv_utils_config.h)

# ISO 7816 library
add_library(iso7816
	iso7816.c
	iso7816_compact_tlv.c
	iso7816_strings.c
)
set(iso7816_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	iso7816.h
	iso7816_compact_tlv.h
	iso7816_strings.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::iso7816 ALIAS iso7816)
set_target_properties(iso7816
	PROPERTIES
		PUBLIC_HEADER "${iso7816_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(iso7816 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/iso7816>
)
install(
	TARGETS
		iso7816
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/iso7816"
		COMPONENT emv_development
	RUNTIME
		COMPONENT emv_runtime
	LIBRARY
		COMPONENT emv_runtime
		NAMELINK_COMPONENT emv_development
	ARCHIVE
		COMPONENT emv_development
)

# ISO 8825 library
add_library(iso8825
	iso8825_ber.c
)
set(iso8825_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	iso8825_ber.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::iso8825 ALIAS iso8825)
set_target_properties(iso8825
	PROPERTIES
		PUBLIC_HEADER "${iso8825_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(iso8825 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/iso8825>
)
install(
	TARGETS
		iso8825
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/iso8825"
		COMPONENT emv_development
	RUNTIME
		COMPONENT emv_runtime
	LIBRARY
		COMPONENT emv_runtime
		NAMELINK_COMPONENT emv_development
	ARCHIVE
		COMPONENT emv_development
)

# ISO 8859 library
set(ISO8859_IMPL "boost" CACHE STRING "ISO 8859 implementation to use")
set_property(CACHE ISO8859_IMPL PROPERTY STRINGS boost iconv simple)
add_library(iso8859)
add_library(emv::iso8859 ALIAS iso8859)
set(iso8859_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	iso8859.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
set_target_properties(iso8859
	PROPERTIES
		PUBLIC_HEADER "${iso8859_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(iso8859 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/iso8859>
)
if(ISO8859_IMPL STREQUAL "boost")
	find_package(Boost REQUIRED COMPONENTS locale)
	message(STATUS "Using Boost.Locale for iso8859 implementation")
	target_sources(iso8859 PRIVATE iso8859_boost.cpp)
	target_link_libraries(iso8859 PRIVATE Boost::locale)

	# The EMV_UTILS_PACKAGE_DEPENDENCIES variable is set for the parent scope to
	# facilitate the generation of CMake package configuration files.
	list(APPEND EMV_UTILS_PACKAGE_DEPENDENCIES "Boost COMPONENTS locale")
	set(EMV_UTILS_PACKAGE_DEPENDENCIES ${EMV_UTILS_PACKAGE_DEPENDENCIES} PARENT_SCOPE)
endif()
if(ISO8859_IMPL STREQUAL "iconv")
	find_package(Iconv REQUIRED)

	message(STATUS "Using iconv for iso8859 implementation")
	target_sources(iso8859 PRIVATE iso8859_iconv.c)
	if(Iconv_LIBRARIES)
		target_link_libraries(iso8859 PRIVATE Iconv::Iconv)

		# The EMV_UTILS_PACKAGE_DEPENDENCIES variable is set for the parent
		# scope to facilitate the generation of CMake package configuration
		# files.
		list(APPEND EMV_UTILS_PACKAGE_DEPENDENCIES "Iconv")
		set(EMV_UTILS_PACKAGE_DEPENDENCIES ${EMV_UTILS_PACKAGE_DEPENDENCIES} PARENT_SCOPE)
	endif()
endif()
if(ISO8859_IMPL STREQUAL "simple")
	message(STATUS "Using simple iso8859 implementation")
	target_sources(iso8859 PRIVATE iso8859_simple.c)
endif()
install(
	TARGETS
		iso8859
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/iso8859"
		COMPONENT emv_development
	RUNTIME
		COMPONENT emv_runtime
	LIBRARY
		COMPONENT emv_runtime
		NAMELINK_COMPONENT emv_development
	ARCHIVE
		COMPONENT emv_development
)

# EMV library
add_library(emv
	emv.c
	emv_fields.c
	emv_tlv.c
	emv_dol.c
	emv_debug.c
	emv_ttl.c
	emv_app.c
	emv_tal.c
)
set(emv_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	emv.h
	emv_tags.h
	emv_fields.h
	emv_tlv.h
	emv_dol.h
	emv_debug.h
	emv_ttl.h
	emv_app.h
	emv_tal.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::emv ALIAS emv)
target_compile_definitions(emv PUBLIC EMV_DEBUG_ENABLED) # Enabled all debugging
target_link_libraries(emv
	PUBLIC
		iso8825
	PRIVATE
		iso7816
		iso8859
)
# The EMV_PKGCONFIG_REQ_PRIV and EMV_PKGCONFIG_LIBS_PRIV variables are set
# for the parent scope to facilitate the generation of pkgconfig files.
# NOTE: It is not necessary to set EMV_PKGCONFIG_LIBS_PRIV for dependencies
# that are mentioned in EMV_PKGCONFIG_REQ_PRIV
set(EMV_PKGCONFIG_REQ_PRIV "libiso8825 libiso8859" PARENT_SCOPE)
set_target_properties(emv
	PROPERTIES
		PUBLIC_HEADER "${emv_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(emv
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}" # For generated headers
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:include/emv>
)
install(
	TARGETS
		emv
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/emv"
		COMPONENT emv_development
	RUNTIME
		COMPONENT emv_runtime
	LIBRARY
		COMPONENT emv_runtime
		NAMELINK_COMPONENT emv_development
	ARCHIVE
		COMPONENT emv_development
)

# EMV strings library
add_library(emv_strings
	emv_strings.c
	isocodes_lookup.cpp
	mcc_lookup.cpp
)
set(emv_strings_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	emv_strings.h
	isocodes_lookup.h
	mcc_lookup.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::emv_strings ALIAS emv_strings)
# The EMVSTRINGS_PKGCONFIG_REQ_PRIV and EMVSTRINGS_PKGCONFIG_LIBS_PRIV variables are set
# for the parent scope to facilitate the generation of pkgconfig files.
# NOTE: It is not necessary to set EMVSTRINGS_PKGCONFIG_LIBS_PRIV for linking json-c
set(EMVSTRINGS_PKGCONFIG_REQ_PRIV "json-c" PARENT_SCOPE)
set_target_properties(emv_strings
	PROPERTIES
		PUBLIC_HEADER "${emv_strings_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(emv_strings
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}" # For generated headers
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:include/emv>
)
target_link_libraries(emv_strings
	PRIVATE
		iso7816 # Used by emv_strings.c
		emv # Used by emv_strings.c
		json-c::json-c # Used by isocodes_lookup.c
)
install(
	TARGETS
		emv_strings
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/emv"
		COMPONENT emv_development
	RUNTIME
		COMPONENT emv_runtime
	LIBRARY
		COMPONENT emv_runtime
		NAMELINK_COMPONENT emv_development
	ARCHIVE
		COMPONENT emv_development
)
