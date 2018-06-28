#
# Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# No CTest here, and no "make test" either:
# http://public.kitware.com/Bug/view.php?id=8438
# http://public.kitware.com/Bug/view.php?id=8774

include(ExternalProject)
include(FindPackageMessage)

# There is FindGTest.cmake, but it doesn't find gmock
#message(STATUS "Looking for GTest/GMock")
find_path(GTEST_ROOT src/gtest-all.cc
	HINTS ENV GTEST_ROOT
	PATHS "${PROJECT_SOURCE_DIR}/third_party/googletest/googletest")
find_path(GMOCK_ROOT src/gmock-all.cc
	HINTS ENV GMOCK_ROOT
	PATHS "${PROJECT_SOURCE_DIR}/third_party/googletest/googlemock")

set(gtest_error_msg "not found, testing will be disabled.")
set(gtest_error_msg "${gtest_error_msg} You can set environment variables")
set(gtest_error_msg "${gtest_error_msg} GTEST_ROOT and GMOCK_ROOT")
if(GTEST_ROOT)
	find_package_message(gtest "Found GoogleTest: ${GTEST_ROOT}"
		"${GTEST_ROOT}")
else()
	find_package_message(gtest "GoogleTest ${gtest_error_msg}" "${GTEST_ROOT}")
	return()
endif()
if(GMOCK_ROOT)
	find_package_message(gmock "Found GoogleMock: ${GMOCK_ROOT}"
		"${GMOCK_ROOT}")
else()
	find_package_message(gmock "GoogleMock ${gtest_error_msg}" "${GMOCK_ROOT}")
	return()
endif()

# Force the simple internal regex engine to get consistent behavior on all
# platforms. See
# https://code.google.com/p/chromium/issues/detail?id=317224 for more details.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
	"GTEST_HAS_POSIX_RE=0")

add_executable(unittest_bin EXCLUDE_FROM_ALL
	"${GMOCK_ROOT}/src/gmock_main.cc"
	"${GTEST_ROOT}/src/gtest-all.cc"
	"${GMOCK_ROOT}/src/gmock-all.cc"
	"ThreadTest.cpp" "NickTest.cpp" "ClientTest.cpp" "NetworkTest.cpp"
	"MessageTest.cpp" "ModulesTest.cpp" "IRCSockTest.cpp" "QueryTest.cpp"
	"StringTest.cpp" "ConfigTest.cpp" "BufferTest.cpp" "UtilsTest.cpp"
	"UserTest.cpp")
target_link_libraries(unittest_bin PRIVATE znclib)
target_include_directories(unittest_bin PRIVATE
	"${GTEST_ROOT}" "${GTEST_ROOT}/include"
	"${GMOCK_ROOT}" "${GMOCK_ROOT}/include")
add_custom_target(unittest COMMAND unittest_bin)

# Use different compiler flags, because Qt fails with sanitizers,
# and we don't need sanitizers to test the test itself anyway.
externalproject_add(inttest_bin
	EXCLUDE_FROM_ALL true
	BUILD_ALWAYS true
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/integration"
	BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/integration"
	INSTALL_COMMAND ""
	CMAKE_CACHE_ARGS
	# Note the space in the end: protect from CXXFLAGS env var, but
	# still support custom flags
	"-DCMAKE_CXX_FLAGS:string=${INTEGRATION_TEST_CXX_FLAGS} "
	"-DGTEST_ROOT:path=${GTEST_ROOT}"
	"-DGMOCK_ROOT:path=${GMOCK_ROOT}"
	"-DZNC_BIN_DIR:path=${CMAKE_INSTALL_FULL_BINDIR}"
	"-DQt5_HINTS:path=${brew_qt5}")
add_custom_target(inttest COMMAND
	# Prevent a warning from test of znc-buildmod, when inner make
	# discovers that there is an outer make and tries to use it:
	# gmake[4]: warning: jobserver unavailable: using -j1.  Add '+' to parent make rule.
	# This option doesn't affect ninja, which doesn't show that warning anyway.
	${CMAKE_COMMAND} -E env MAKEFLAGS=
	"${CMAKE_CURRENT_BINARY_DIR}/integration/inttest")
add_dependencies(inttest inttest_bin)
