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

include(FindPackageHandleStandardArgs) # Provides find_package() messages
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
find_package_handle_standard_args(Qt${QT_VERSION_MAJOR}Widgets CONFIG_MODE)
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
	qt5_add_resources(QRC_SRCS icons.qrc)
	add_library(Qt::Widgets ALIAS Qt5::Widgets)
else()
	# Qt-5.15 and Qt-6 provide these version-less commands
	qt_wrap_ui(UI_SRCS emv-viewer-mainwindow.ui)
	qt_wrap_cpp(MOC_SRCS ${EMV_VIEWER_MOC_HEADERS})
	qt_add_resources(QRC_SRCS icons.qrc)
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

option(BUILD_MACOSX_BUNDLE "Build MacOS bundle containing emv-viewer, emv-decode and emv-tool")
if(APPLE AND BUILD_MACOSX_BUNDLE)
	# Set properties needed for bundle applications on MacOS
	set_target_properties(
		emv-viewer
		PROPERTIES
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_BUNDLE_NAME "EMV Viewer"
			MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
			MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
			MACOSX_BUNDLE_COPYRIGHT "Copyright 2021-2024 Leon Lynch"
			MACOSX_BUNDLE_GUI_IDENTIFIER org.openemv.${CMAKE_PROJECT_NAME}
			MACOSX_BUNDLE_ICON_FILE "openemv_emv_utils.icns"
			RESOURCE icons/openemv_emv_utils.icns
			OUTPUT_NAME "EMV Viewer"
	)

	# Add application icon for MacOS
	target_sources(emv-viewer PRIVATE icons/openemv_emv_utils.icns)

	# Install emv-viewer symlink into bundle for MacOS
	install(CODE
		"execute_process(COMMAND \"${CMAKE_COMMAND}\" -E make_directory \"$<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/bin/\")"
		COMPONENT emv_viewer_bundle
	)
	install(CODE
		"execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink \"../MacOS/$<TARGET_FILE_NAME:emv-viewer>\" \"$<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/bin/emv-viewer\")"
		COMPONENT emv_viewer_bundle
	)

	# Install README and LICENSE.gpl into bundle for MacOS
	if(EXISTS ${CMAKE_SOURCE_DIR}/README.md)
		install(FILES
			${CMAKE_SOURCE_DIR}/README.md
			COMPONENT emv_viewer_bundle
			DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/doc/
		)
	endif()
	install(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.gpl
		COMPONENT emv_viewer_bundle
		DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/doc/
	)

	# Install other tools into bundle for MacOS
	if(TARGET emv-decode)
		install(PROGRAMS
			$<TARGET_FILE:emv-decode>
			COMPONENT emv_viewer_bundle
			DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/bin/
		)
	endif()
	if(TARGET emv-tool)
		install(PROGRAMS
			$<TARGET_FILE:emv-tool>
			COMPONENT emv_viewer_bundle
			DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:emv-viewer>/bin/
		)
	endif()

	# Deploy Qt into bundle for MacOS
	include(DeployQt)
	macdeployqt(emv-viewer emv_viewer_bundle)
endif()

if(WIN32)
	# Set properties needed for GUI applications on Windows
	set_target_properties(
		emv-viewer
		PROPERTIES
			WIN32_EXECUTABLE TRUE
	)

	# Add application icon for Windows
	target_sources(emv-viewer PRIVATE icon.rc)

	# Deploy Qt for Windows
	include(DeployQt)
	windeployqt(emv-viewer emv_viewer_runtime)

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

if(NOT APPLE AND NOT WIN32)
	# Install icon file to UI runtime component for Linux
	set(EMV_UTILS_INSTALL_ICONSDIR ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps CACHE STRING "Installation location for emv-viewer icon files")
	install(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/icons/openemv_emv_utils_512x512.png
		DESTINATION ${EMV_UTILS_INSTALL_ICONSDIR}
		COMPONENT emv_viewer_runtime
		RENAME emv-viewer.png
	)

	# Install desktop entry file to UI runtime component for Linux
	set(EMV_UTILS_INSTALL_DESKTOPENTRYDIR ${CMAKE_INSTALL_DATADIR}/applications CACHE STRING "Installation location for emv-viewer desktop entry files")
	install(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/emv-viewer.desktop
		DESTINATION ${EMV_UTILS_INSTALL_DESKTOPENTRYDIR}
		COMPONENT emv_viewer_runtime
	)

	# Install appstream file to UI runtime component for Linux
	set(EMV_UTILS_INSTALL_APPSTREAMDIR ${CMAKE_INSTALL_DATADIR}/metainfo CACHE STRING "Installation location for emv-viewer appstream files")
	install(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/emv-viewer.appdata.xml
		DESTINATION ${EMV_UTILS_INSTALL_APPSTREAMDIR}
		COMPONENT emv_viewer_runtime
	)
endif()

install(
	TARGETS
		emv-viewer
	EXPORT emvUtilsTargets # For use by install(EXPORT) command
	RUNTIME
		COMPONENT emv_viewer_runtime
	BUNDLE
		COMPONENT emv_viewer_bundle
		DESTINATION . # Install to root of MacOS bundle
)
