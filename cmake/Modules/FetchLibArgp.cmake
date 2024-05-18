##############################################################################
# Copyright 2022-2023 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

# This module will define:
#
# argp_FOUND
# argp_INCLUDE_DIRS
# argp_LIBRARIES

# This module also provides the following imported targets:
# - libargp::argp (library)

# This module will use ARGP_OSX_ARCHITECTURES to set the binary architectures
# for the build. ARGP_OSX_ARCHITECTURES has the same format as
# CMAKE_OSX_ARCHITECTURES

include(FetchContent)

message(CHECK_START "Downloading libargp...")
FetchContent_Declare(
	libargp
	GIT_REPOSITORY https://github.com/leonlynch/libargp.git
	GIT_TAG 987d87b98e4cd03abb1107b77ef5d43ad0552e13
)
FetchContent_MakeAvailable(libargp)
message(CHECK_PASS "done")

# libargp is now ready to use
set(argp_FOUND TRUE)
