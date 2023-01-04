##############################################################################
# Copyright (c) 2021, 2022 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

project(emv-utils
	VERSION 0.0.1
	DESCRIPTION "EMV libraries and tools"
	HOMEPAGE_URL "https://github.com/openemv/emv-utils"
	LANGUAGES C
)

# Determine whether this project is the top-level project
if(${CMAKE_VERSION} VERSION_LESS "3.21")
	get_directory_property(EMV_UTILS_HAS_PARENT PARENT_DIRECTORY)
	if(NOT EMV_UTILS_HAS_PARENT)
		set(EMV_UTILS_IS_TOP_LEVEL True)
	endif()
else()
	# CMake >=3.21 provides <PROJECT-NAME>_IS_TOP_LEVEL
	set(EMV_UTILS_IS_TOP_LEVEL ${emv-utils_IS_TOP_LEVEL})
endif()

# Configure compiler
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
#set(CMAKE_C_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wall -Werror)
	add_compile_options($<$<CONFIG:Debug>:-ggdb>)
	add_compile_options($<$<CONFIG:RelWithDebInfo>:-ggdb>)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

# Configure testing before adding subdirectories
if(EMV_UTILS_IS_TOP_LEVEL)
	# Configure Valgrind before including CTest module
	find_program(VALGRIND_COMMAND valgrind)
	set(MEMORYCHECK_TYPE Valgrind)
	set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --show-reachable=yes --track-origins=yes --num-callers=100 --show-error-list=yes")

	# Only top-level project should include CTest module
	include(CTest)
endif()

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()

# Install README and LICENSE files to runtime component
install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/README.md"
	"${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
	TYPE DOC
	COMPONENT emv_runtime
)

# Generate and install basic CMake config files
include(CMakePackageConfigHelpers) # Provides CMake config generator macros
set(EMV_UTILS_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} CACHE STRING "Installation location for emv-utils CMake config files")
message(STATUS "Using CMake config install location \"${EMV_UTILS_INSTALL_CMAKEDIR}\"")
configure_package_config_file(cmake/emvUtilsConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emvUtilsConfig.cmake"
	INSTALL_DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emvUtilsConfigVersion.cmake"
	COMPATIBILITY AnyNewerVersion
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emvUtilsConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emvUtilsConfigVersion.cmake"
	DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
	COMPONENT emv_development
)
install(EXPORT emvUtilsTargets
	FILE emvUtilsTargets.cmake
	DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
	NAMESPACE emv::
	COMPONENT emv_development
)
export(EXPORT emvUtilsTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/emvUtilsTargets.cmake"
	NAMESPACE emv::
)

# Generate and install pkgconfig files
set(EMV_UTILS_INSTALL_PKGCONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig CACHE STRING "Installation location for emv-utils pkgconfig files")
message(STATUS "Using pkgconfig install location \"${EMV_UTILS_INSTALL_PKGCONFIG_DIR}\"")
# Generate pkgconfig for iso7816 library
set(EMV_UTILS_PKGCONFIG_LIB_NAME iso7816)
# NOTE: src subdirectory may provide ISO7816_PKGCONFIG_REQ_PRIV and ISO7816_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libiso7816.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso7816.pc"
	@ONLY
)
# Generate pkgconfig for iso8825 library
set(EMV_UTILS_PKGCONFIG_LIB_NAME iso8825)
# NOTE: src subdirectory may provide ISO8825_PKGCONFIG_REQ_PRIV and ISO8825_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libiso8825.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso8825.pc"
	@ONLY
)
# Generate pkgconfig for emv library
set(EMV_UTILS_PKGCONFIG_LIB_NAME emv)
# NOTE: src subdirectory may provide EMV_PKGCONFIG_REQ_PRIV and EMV_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libemv.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv.pc"
	@ONLY
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso7816.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso8825.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv.pc"
	DESTINATION "${EMV_UTILS_INSTALL_PKGCONFIG_DIR}"
	COMPONENT emv_development
)
