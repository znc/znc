#
# Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

cmake_minimum_required(VERSION 3.13)
project(ZNCIntegrationTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_THREAD_PREFER_PTHREAD true)
set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)

find_package(Qt6Network REQUIRED HINTS ${Qt6_HINTS})

# Force the simple internal regex engine to get consistent behavior on all
# platforms. See
# https://code.google.com/p/chromium/issues/detail?id=317224 for more details.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
	"GTEST_HAS_POSIX_RE=0")

add_executable(inttest
	"framework/main.cpp"
	"framework/base.cpp"
	"framework/znctest.cpp"
	"tests/core.cpp"
	"tests/modules.cpp"
	"tests/scripting.cpp"
	"${GTEST_ROOT}/src/gtest-all.cc"
	"${GMOCK_ROOT}/src/gmock-all.cc")

target_link_libraries(inttest Qt6::Network Threads::Threads)
target_include_directories(inttest PUBLIC
	"${PROJECT_SOURCE_DIR}/framework"
	"${PROJECT_BINARY_DIR}"
	"${GTEST_ROOT}" "${GTEST_ROOT}/include"
	"${GMOCK_ROOT}" "${GMOCK_ROOT}/include")
target_compile_definitions(inttest PRIVATE
	"ZNC_BIN_DIR=\"${ZNC_BIN_DIR}\"")
