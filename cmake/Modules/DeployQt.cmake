##############################################################################
# Copyright 2022-2023 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
#
# This file was inspired by:
# * https://github.com/nitroshare/nitroshare-desktop/blob/master/cmake/DeployQt.cmake
# * https://github.com/miurahr/cmake-qt-packaging-example/blob/master/Packaging.cmake
#
##############################################################################

# Parent context must specify Qt major version using QT_VERSION_MAJOR.
# This variable can either be set explicitly or be generated when finding the
# available Qt versions, for example: find_package(QT NAMES Qt6 Qt5)
if(NOT QT_VERSION_MAJOR)
	message(FATAL_ERROR "QT_VERSION_MAJOR not set")
endif()

# Find Qt installation
find_package(Qt${QT_VERSION_MAJOR}Core REQUIRED)
if(QT_VERSION_MAJOR EQUAL 6)
	get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
elseif(QT_VERSION_MAJOR EQUAL 5)
	get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
else()
	message(FATAL_ERROR "Deployment of Qt${QT_VERSION_MAJOR} not supported")
endif()
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

# Find Qt deployment tool for MacOS
if(APPLE)
	find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
	if(MACDEPLOYQT_EXECUTABLE)
		message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
	else()
		message(FATAL_ERROR "macdeployqt not found")
	endif()
endif()

# Find Qt deployment tool for Windows
if(WIN32)
	find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
	if(WINDEPLOYQT_EXECUTABLE)
		message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
	else()
		message(FATAL_ERROR "windeployqt not found")
	endif()

	# Set options used to exclude modules for different versions of Qt
	if(QT_VERSION_MAJOR EQUAL 6)
		set(WINDEPLOYQT_EXCLUDE_OPTS "--no-translations" "--no-opengl-sw")
	elseif(QT_VERSION_MAJOR EQUAL 5)
		set(WINDEPLOYQT_EXCLUDE_OPTS "--no-translations" "--no-webkit2" "--no-angle" "--no-opengl-sw")
	else()
		message(FATAL_ERROR "Deployment of Qt${QT_VERSION_MAJOR} not supported")
	endif()
endif()

function(macdeployqt target component)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${MACDEPLOYQT_EXECUTABLE}"
			$<TARGET_BUNDLE_DIR:${target}>
			-verbose=1
			-always-overwrite
		COMMENT "Deploying Qt..."
		VERBATIM
	)
endfunction()

function(windeployqt target component)
	# Deploy Qt to output directory from where it can be installed to a component
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/qt-bin/"
		COMMAND "${CMAKE_COMMAND}" -E
			env PATH="${_qt_bin_dir}$<SEMICOLON>%PATH%" "${WINDEPLOYQT_EXECUTABLE}"
			--verbose 3
			--compiler-runtime
			${WINDEPLOYQT_EXCLUDE_OPTS}
			--dir "${CMAKE_CURRENT_BINARY_DIR}/qt-bin/"
			$<TARGET_FILE:${target}>
		COMMENT "Deploying Qt..."
		VERBATIM
	)

	# Install Qt to the appropriate CMake component
	install(
		DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt-bin/"
		TYPE BIN
		COMPONENT ${component}
	)
endfunction()

mark_as_advanced(MACDEPLOYQT_EXECUTABLE WINDEPLOYQT_EXECUTABLE)
