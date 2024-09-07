##############################################################################
# Copyright 2024 Leon Lynch
#
# This file is licensed under the terms of the GPL v3 license.
# See LICENSE.gpl file.
##############################################################################

cmake_minimum_required(VERSION 3.16)

project(emv-viewer
	VERSION 0.1.0
	DESCRIPTION "EMV Viewer using Qt"
	HOMEPAGE_URL "https://github.com/openemv/emv-utils"
	LANGUAGES CXX
)

# Determine whether this project is the top-level project
if(${CMAKE_VERSION} VERSION_LESS "3.21")
	get_directory_property(EMV_VIEWER_HAS_PARENT PARENT_DIRECTORY)
	if(NOT EMV_VIEWER_HAS_PARENT)
		set(EMV_VIEWER_IS_TOP_LEVEL True)
	endif()
else()
	# CMake >=3.21 provides <PROJECT-NAME>_IS_TOP_LEVEL
	set(EMV_VIEWER_IS_TOP_LEVEL ${emv-viewer_IS_TOP_LEVEL})
endif()

# Configure compiler
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wall)
	add_compile_options($<$<CONFIG:Debug>:-ggdb>)
	add_compile_options($<$<CONFIG:RelWithDebInfo>:-ggdb>)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-Wall)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	add_compile_options(-Wall)
endif()

if(EMV_VIEWER_IS_TOP_LEVEL)
	# If this is the top-level project, look for the platform emv-utils libraries
	find_package(emv-viewer 0.1.0 REQUIRED)
else()
	# Otherwise the parent project must provide the emv-utils targets
	if(NOT TARGET emv::emv_strings)
		message(FATAL_ERROR "Parent project must provide emv-utils libraries")
	endif()
endif()

include(GNUInstallDirs) # Provides CMAKE_INSTALL_* variables and good defaults for install()

# Generate config file for internal use only
# This file should NOT be installed or used by an installed header
if(NOT EMV_UTILS_VERSION_STRING)
	set(EMV_UTILS_VERSION_STRING ${emv-viewer_VERSION})
endif()
configure_file(
	emv_viewer_config.h.in
	emv_viewer_config.h
)

# See https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html#supporting-older-qt-5-versions
# Note that CMAKE_DISABLE_FIND_PACKAGE_<PackageName> only applies to the
# primary package name and not the alternative names and therefore it is
# necessary to build the list of alternative names to ensure that either Qt5 or
# Qt6 can be disabled, otherwise CMAKE_DISABLE_FIND_PACKAGE_<PackageName> will
# be ignored.
if(NOT CMAKE_DISABLE_FIND_PACKAGE_Qt5)
	list(APPEND Qt_NAMES Qt5)
endif()
if(NOT CMAKE_DISABLE_FIND_PACKAGE_Qt6)
	list(APPEND Qt_NAMES Qt6)
endif()
if(NOT Qt_NAMES)
	message(FATAL_ERROR "Either Qt5 or Qt6 are required to build emv-viewer")
endif()

find_package(QT 5.12 NAMES ${Qt_NAMES} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PATHS ${QT_DIR})
message(STATUS "Found Qt${QT_VERSION_MAJOR} Widgets: ${QT_CONFIG} (found suitable version \"${QT_VERSION}\")")
set(EMV_VIEWER_MOC_HEADERS
	emv-viewer-mainwindow.h
	emvhighlighter.h
	emvtreeview.h
	betterplaintextedit.h
)
if(QT_VERSION VERSION_LESS 5.15)
	# Qt-5.12 provides these versioned commands
	qt5_wrap_ui(UI_SRCS emv-viewer-mainwindow.ui)
	qt5_wrap_cpp(MOC_SRCS ${EMV_VIEWER_MOC_HEADERS})
	add_library(Qt::Widgets ALIAS Qt5::Widgets)
else()
	# Qt-5.15 and Qt-6 provide these version-less commands
	qt_wrap_ui(UI_SRCS emv-viewer-mainwindow.ui)
	qt_wrap_cpp(MOC_SRCS ${EMV_VIEWER_MOC_HEADERS})
endif()

add_executable(emv-viewer
	emv-viewer.cpp
	emv-viewer-mainwindow.cpp
	emvhighlighter.cpp
	emvtreeitem.cpp
	emvtreeview.cpp
	${UI_SRCS} ${MOC_SRCS} ${QRC_SRCS}
)
target_include_directories(emv-viewer PRIVATE
	${CMAKE_CURRENT_BINARY_DIR} # For generated files
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For generated files to include source headers
)
target_link_libraries(emv-viewer
	PRIVATE
		Qt::Widgets
		emv::emv_strings
		emv::emv
)

if(WIN32)
	# Set properties needed for GUI applications on Windows
	set_target_properties(
		emv-viewer
		PROPERTIES
			WIN32_EXECUTABLE TRUE
	)

	# Deploy runtime libraries for Windows
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT emv_viewer_runtime)
	include(InstallRequiredSystemLibraries)
	if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
		message(STATUS "Required system libraries: ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
	endif()
endif()

# Install LICENSE.gpl file to UI runtime component
install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.gpl
	TYPE DOC
	COMPONENT emv_viewer_runtime
)

install(
	TARGETS
		emv-viewer
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	RUNTIME
		COMPONENT emv_viewer_runtime
)
