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

# perl 5.20 will fix this warning:
# https://rt.perl.org/Public/Bug/Display.html?id=120670
set(PERL_CFLAGS -Wno-reserved-user-defined-literal -Wno-literal-suffix
	CACHE STRING "Perl compiler flags")

if(PerlLibs_FIND_VERSION_EXACT)
	set(_PerlLibs_exact EXACT)
else()
	set(_PerlLibs_exact)
endif()
find_package(Perl ${PerlLibs_FIND_VERSION} ${PerlLibs_exact} QUIET)

if(PERL_FOUND)
	if(PERL_INCLUDE_DIR AND PERL_LIBRARIES)
	else()
		execute_process(
			COMMAND "${PERL_EXECUTABLE}" -MExtUtils::Embed -e perl_inc
			OUTPUT_VARIABLE _PerlLibs_inc_output
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		string(REGEX REPLACE "^ *-I" "" _PerlLibs_include
			"${_PerlLibs_inc_output}")

		execute_process(
			COMMAND "${PERL_EXECUTABLE}" -MExtUtils::Embed -e ldopts
			OUTPUT_VARIABLE _PerlLibs_ld_output
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		separate_arguments(_PerlLibs_ld_output)
		set(_PerlLibs_ldflags)
		set(_PerlLibs_libraries)
		foreach(_PerlLibs_i ${_PerlLibs_ld_output})
			if("${_PerlLibs_i}" MATCHES "^-l")
				list(APPEND _PerlLibs_libraries "${_PerlLibs_i}")
			else()
				set(_PerlLibs_ldflags "${_PerlLibs_ldflags} ${_PerlLibs_i}")
			endif()
		endforeach()

		get_filename_component(_PerlLibs_dir "${CMAKE_CURRENT_LIST_FILE}"
			DIRECTORY)
		try_compile(_PerlLibs_try
			"${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/perl_check"
			"${_PerlLibs_dir}/perl_check" perl_check
			CMAKE_FLAGS
			"-DPERL_INCLUDE_DIRS=${_PerlLibs_include}"
			"-DPERL_LDFLAGS=${_PerlLibs_ldflags}"
			"-DPERL_LIBRARIES=${_PerlLibs_libraries}"
			"-DPERL_CFLAGS=${PERL_CFLAGS}"
			OUTPUT_VARIABLE _PerlLibs_tryout)

		if(_PerlLibs_try)
			set(PERL_INCLUDE_DIR "${_PerlLibs_include}" CACHE PATH
				"Perl include dir")
			set(PERL_LDFLAGS "${_PerlLibs_ldflags}" CACHE STRING
				"Perl linker flags")
			set(PERL_LIBRARIES "${_PerlLibs_libraries}" CACHE STRING
				"Perl libraries")
			file(APPEND
				"${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log"
				"Output using Perl:\n${_PerlLibs_tryout}\n")
		else()
			set(_PerlLibs_failmsg FAIL_MESSAGE "Attempt to use Perl failed")
			file(APPEND
				"${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log"
				"Error using Perl:\n${_PerlLibs_tryout}\n")
		endif()
	endif()
else()
	set(_PerlLibs_failmsg FAIL_MESSAGE "Perl not found")
endif()

find_package_handle_standard_args(PerlLibs
	REQUIRED_VARS PERL_INCLUDE_DIR PERL_LIBRARIES
	VERSION_VAR PERL_VERSION_STRING
	${_PerlLibs_failmsg})

set(PERL_INCLUDE_DIRS "${PERL_INCLUDE_DIR}")

mark_as_advanced(PERL_INCLUDE_DIR PERL_LDFLAGS PERL_LIBRARIES PERL_CFLAGS)
