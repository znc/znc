#
# Copyright (C) 2004-2023 ZNC, see the NOTICE file for details.
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

if(NOT DEFINED cxx17check)
	message(STATUS "Checking for C++17 support")
	get_filename_component(_CXX17Check_dir "${CMAKE_CURRENT_LIST_FILE}"
		DIRECTORY)
	try_compile(cxx17_supported
		"${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/cxx17check"
		"${_CXX17Check_dir}/cxx17check" cxx17check
		OUTPUT_VARIABLE _CXX17Check_tryout)
	if(cxx17_supported)
		message(STATUS "Checking for C++17 support - supported")
		SET(cxx17check 1 CACHE INTERNAL "C++17 support")
		file(APPEND
			"${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log"
			"Output of C++17 check:\n${_CXX17Check_tryout}\n")
	else()
		file(APPEND
			"${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log"
			"Error in C++17 check:\n${_CXX17Check_tryout}\n")
		message(STATUS "Checking for C++17 support - not supported")
		message(FATAL_ERROR " Upgrade your compiler.\n"
			" GCC 8+ and Clang 5+ should work.")
	endif()
endif()
