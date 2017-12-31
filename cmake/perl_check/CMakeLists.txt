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

cmake_minimum_required(VERSION 3.0)
project(perl_check)
set(CMAKE_VERBOSE_MAKEFILE true)

if(APPLE)
	set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
		"${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

add_library(main MODULE main.cpp)
target_include_directories(main PRIVATE ${PERL_INCLUDE_DIRS})
target_compile_options(main PRIVATE "${PERL_CFLAGS}")
target_link_libraries(main PRIVATE ${PERL_LIBRARIES})
set_target_properties(main PROPERTIES LINK_FLAGS "${PERL_LDFLAGS}")
