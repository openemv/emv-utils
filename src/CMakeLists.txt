##############################################################################
# Copyright (c) 2022, 2023 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED COMPONENTS json)
find_package(IsoCodes REQUIRED)

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()

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

# iso-codes lookup library
add_library(isocodes_lookup
	isocodes_lookup.cpp
)
set(isocodes_lookup_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	isocodes_lookup.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::isocodes_lookup ALIAS isocodes_lookup)
set_target_properties(isocodes_lookup
	PROPERTIES
		PUBLIC_HEADER "${isocodes_lookup_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
configure_file(isocodes_config.h.in isocodes_config.h)
target_include_directories(isocodes_lookup
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}" # for generated isocodes_config.h
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:include/emv>
)
target_link_libraries(isocodes_lookup PRIVATE Boost::json)
install(
	TARGETS
		isocodes_lookup
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

# EMV library
add_library(emv
	emv_fields.c
	emv_tlv.c
	emv_dol.c
	emv_debug.c
	emv_ttl.c
	emv_app.c
	emv_tal.c
	emv_strings.c
)
set(emv_HEADERS # PUBLIC_HEADER property requires a list instead of individual entries
	emv_tags.h
	emv_fields.h
	emv_tlv.h
	emv_dol.h
	emv_debug.h
	emv_ttl.h
	emv_app.h
	emv_tal.h
	emv_strings.h
	PARENT_SCOPE # Doxygen generator requires a list of headers
)
add_library(emv::emv ALIAS emv)
target_compile_definitions(emv PUBLIC EMV_DEBUG_ENABLED) # Enabled all debugging
target_link_libraries(emv PUBLIC iso8825)
# The EMV_PKGCONFIG_REQ_PRIV and EMV_PKGCONFIG_LIBS_PRIV variables are set
# for the parent scope to facilitate the generation of pkgconfig files.
set(EMV_PKGCONFIG_REQ_PRIV "libiso8825" PARENT_SCOPE)
set(EMV_PKGCONFIG_LIBS_PRIV "-liso8825" PARENT_SCOPE)
set_target_properties(emv
	PROPERTIES
		PUBLIC_HEADER "${emv_HEADERS}"
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(emv INTERFACE
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
