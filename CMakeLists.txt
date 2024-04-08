##############################################################################
# Copyright (c) 2021-2024 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

project(emv-utils
	VERSION 0.1.0
	DESCRIPTION "EMV libraries and tools"
	HOMEPAGE_URL "https://github.com/openemv/emv-utils"
	LANGUAGES C CXX
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
set(CMAKE_C_EXTENSIONS ON) # Various POSIX functions are required
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wall -Werror)
	add_compile_options($<$<CONFIG:Debug>:-ggdb>)
	add_compile_options($<$<CONFIG:RelWithDebInfo>:-ggdb>)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-Wall -Werror)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	add_compile_options(-Wall -Werror)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

# Determine version string (use git describe string if possible)
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.git)
	find_package(Git)

	if(GIT_FOUND)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} describe --always --dirty
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			RESULT_VARIABLE GIT_DESCRIBE_FAILED
			OUTPUT_VARIABLE GIT_DESCRIBE_STRING
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(GIT_DESCRIBE_FAILED)
			message(WARNING "Failed to obtain git describe string (${GIT_DESCRIBE_FAILED})")
		else()
			message(STATUS "Using git describe string \"${GIT_DESCRIBE_STRING}\"")
			set(EMV_UTILS_VERSION_STRING ${GIT_DESCRIBE_STRING})
		endif()
	endif()
endif()
if(NOT EMV_UTILS_VERSION_STRING)
	# Otherwise use project version
	set(EMV_UTILS_VERSION_STRING ${emv-utils_VERSION})
endif()

# Configure testing before adding subdirectories
if(EMV_UTILS_IS_TOP_LEVEL)
	# Configure Valgrind before including CTest module
	find_program(VALGRIND_COMMAND valgrind)
	set(MEMORYCHECK_TYPE Valgrind)
	set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --show-reachable=yes --track-origins=yes --num-callers=100 --show-error-list=yes --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind/emv-utils.supp")

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
# NOTE: src subdirectory provides EMV_UTILS_PACKAGE_DEPENDENCIES
foreach(pkg IN LISTS EMV_UTILS_PACKAGE_DEPENDENCIES)
	# Build dependency string for use in CMake config file
	string(APPEND EMV_UTILS_CONFIG_PACKAGE_DEPENDENCIES "find_dependency(${pkg})\n")
endforeach()
set(EMV_UTILS_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} CACHE STRING "Installation location for emv-utils CMake config files")
message(STATUS "Using CMake config install location \"${EMV_UTILS_INSTALL_CMAKEDIR}\"")
configure_package_config_file(cmake/emv-utils-config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emv-utils-config.cmake"
	INSTALL_DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emv-utils-config-version.cmake"
	COMPATIBILITY SameMinorVersion
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emv-utils-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/emv-utils-config-version.cmake"
	DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
	COMPONENT emv_development
)
install(EXPORT emvUtilsTargets
	FILE emv-utils-config-targets.cmake
	DESTINATION "${EMV_UTILS_INSTALL_CMAKEDIR}"
	NAMESPACE emv::
	COMPONENT emv_development
)
export(EXPORT emvUtilsTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/emv-utils-config-targets.cmake"
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
# Generate pkgconfig for iso8859 library
set(EMV_UTILS_PKGCONFIG_LIB_NAME iso8859)
# NOTE: src subdirectory may provide ISO8859_PKGCONFIG_REQ_PRIV and ISO8859_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libiso8859.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso8859.pc"
	@ONLY
)
# Generate pkgconfig for emv library
set(EMV_UTILS_PKGCONFIG_LIB_NAME emv)
# NOTE: src subdirectory may provide EMV_PKGCONFIG_REQ_PRIV and EMV_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libemv.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv.pc"
	@ONLY
)
# Generate pkgconfig for emv_strings library
set(EMV_UTILS_PKGCONFIG_LIB_NAME emv_strings)
# NOTE: src subdirectory may provide EMVSTRINGS_PKGCONFIG_REQ_PRIV and EMVSTRINGS_PKGCONFIG_LIBS_PRIV
configure_file(pkgconfig/libemv_strings.pc.in
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv_strings.pc"
	@ONLY
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso7816.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso8825.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libiso8859.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv.pc"
	"${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/libemv_strings.pc"
	DESTINATION "${EMV_UTILS_INSTALL_PKGCONFIG_DIR}"
	COMPONENT emv_development
)

# Install bash-completion files
find_package(bash-completion CONFIG) # Optional for providing bash-completion files
if(BASH_COMPLETION_FOUND)
	set(EMV_UTILS_INSTALL_BASH_COMPLETION_DIR ${CMAKE_INSTALL_DATADIR}/bash-completion/completions CACHE STRING "Installation location for emv-utils bash-completion files")
	message(STATUS "Using bash-completion install location \"${EMV_UTILS_INSTALL_BASH_COMPLETION_DIR}\"")

	# Install bash completion files
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/bash-completion/emv-decode"
		DESTINATION "${EMV_UTILS_INSTALL_BASH_COMPLETION_DIR}"
		COMPONENT emv_runtime
	)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/bash-completion/emv-tool"
		DESTINATION "${EMV_UTILS_INSTALL_BASH_COMPLETION_DIR}"
		COMPONENT emv_runtime
	)
endif()

# Build and install doxygen documentation
option(BUILD_DOCS "Build documentation")
find_package(Doxygen) # Optional for generating doxygen files
if(Doxygen_FOUND)
	# Use doxygen to generate HTML
	set(DOXYGEN_GENERATE_HTML YES)
	# Allow static functions and variables
	set(DOXYGEN_EXTRACT_STATIC YES)
	# Allow inline functions
	set(DOXYGEN_INLINE_SOURCES YES)
	# Ignore uses of GCC __attribute__
	set(DOXYGEN_MACRO_EXPANSION YES)
	set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
	set(DOXYGEN_PREDEFINED "__attribute__(x)=")

	# Generate docs for public headers
	if(BUILD_DOCS)
		doxygen_add_docs(docs
			${iso7816_HEADERS}
			${iso8825_HEADERS}
			${iso8859_HEADERS}
			${emv_HEADERS}
			${emv_strings_HEADERS}
			ALL # Build by default
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
		)

		# Install generated docs by default
		install(
			DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
			TYPE DOC
			COMPONENT emv_docs
		)
	else()
		doxygen_add_docs(docs
			${iso7816_HEADERS}
			${iso8825_HEADERS}
			${iso8859_HEADERS}
			${emv_HEADERS}
			${emv_strings_HEADERS}
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
		)
	endif()
endif()

# Configure CPack to generate .tar.gz package
set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_VERSION "${EMV_UTILS_VERSION_STRING}")
set(CPACK_PACKAGE_VENDOR "OpenEMV")
set(CPACK_PACKAGE_CONTACT "lynch.leon@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_VERBATIM_VARIABLES ON)

# Configure generic component packaging attributes
set(CPACK_COMPONENT_EMV_RUNTIME_DISPLAY_NAME "EMV libraries and tools")
set(CPACK_COMPONENT_EMV_RUNTIME_DESCRIPTION "EMV runtime libraries (iso7816, iso8825, iso8859, emv) and command line tools (emv-decode, emv-tool)")
set(CPACK_COMPONENT_EMV_DEVELOPMENT_DISPLAY_NAME "EMV development files")
set(CPACK_COMPONENT_EMV_DEVELOPMENT_DESCRIPTION "Development files (headers, pkgconfig files, CMake config files, etc) for EMV libraries")
set(CPACK_COMPONENT_EMV_DOCS_DISPLAY_NAME "Documentation for EMV libraries")
set(CPACK_COMPONENT_EMV_DOCS_DESCRIPTION ${CPACK_COMPONENT_EMV_DOCS_DISPLAY_NAME})
if(BUILD_SHARED_LIBS)
	set(CPACK_COMPONENT_EMV_DEVELOPMENT_DEPENDS emv_runtime)
endif()

# NOTE: for monolithic packages, uncomment the line below or specify it when
# invoking CMake
#set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

# Generate Debian/Ubuntu packages if dpkg is available
find_package(dpkg) # Optional for building Debian/Ubuntu packages
if(dpkg_FOUND)
	# Configure CPack to generate .deb package
	list(APPEND CPACK_GENERATOR "DEB")
	# Generate component packages
	# NOTE: This is overridden by CPACK_COMPONENTS_GROUPING above
	set(CPACK_DEB_COMPONENT_INSTALL ON)
	set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
	# Use default debian package naming
	set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
	# Assign package names to components
	set(CPACK_DEBIAN_EMV_RUNTIME_PACKAGE_NAME "emv-utils-runtime")
	set(CPACK_DEBIAN_EMV_DEVELOPMENT_PACKAGE_NAME "emv-utils-dev")
	set(CPACK_DEBIAN_EMV_DOCS_PACKAGE_NAME "emv-utils-doc")
	# Assign package architecture to documentation component
	set(CPACK_DEBIAN_EMV_DOCS_PACKAGE_ARCHITECTURE "all")
	# Assign package sections to components
	set(CPACK_DEBIAN_EMV_RUNTIME_PACKAGE_SECTION "utils")
	set(CPACK_DEBIAN_EMV_DEVELOPMENT_PACKAGE_SECTION "devel")
	set(CPACK_DEBIAN_EMV_DOCS_PACKAGE_SECTION "doc")
	# Assign package suggestions
	set(CPACK_DEBIAN_EMV_DOCS_PACKAGE_SUGGESTS "emv-utils-dev")
	# Generate package dependency list
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "iso-codes") # Ensure that iso-codes package is a dependency
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/src/) # dpkg-shlibdeps cannot find dependencies that aren't installed, so it needs some help
endif()

# Generate RedHat/Fedora packages if rpmbuild is available
find_package(rpmbuild) # Optional for building RedHat/Fedora packages
if(rpmbuild_FOUND)
	list(APPEND CPACK_GENERATOR "RPM")
	# Generate component packages
	# NOTE: This is overridden by CPACK_COMPONENTS_GROUPING above
	set(CPACK_RPM_COMPONENT_INSTALL ON)
	# Use default rpm package naming
	set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
	set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
	# Configure license and changelog
	set(CPACK_RPM_PACKAGE_LICENSE "LGPLv2+")
	set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_changelog.txt")
	# Assign package names to components
	set(CPACK_RPM_EMV_RUNTIME_PACKAGE_NAME "emv-utils-runtime")
	set(CPACK_RPM_EMV_DEVELOPMENT_PACKAGE_NAME "emv-utils-devel")
	set(CPACK_RPM_EMV_DOCS_PACKAGE_NAME "emv-utils-doc")
	# Assign package architecture to documentation component
	set(CPACK_RPM_EMV_DOCS_PACKAGE_ARCHITECTURE "noarch")
	# Assign package suggestions
	set(CPACK_RPM_EMV_DOCS_PACKAGE_SUGGESTS "emv-utils-devel")
	# Generate package dependency list
	set(CPACK_RPM_PACKAGE_REQUIRES "iso-codes") # Ensure that iso-codes package is a dependency
	# NOTE: RPM generator automatically detects library dependencies
endif()

include(CPack)
