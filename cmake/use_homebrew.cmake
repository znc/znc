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

if(NOT APPLE)
	return()
endif()

include(FindPackageMessage)
find_program(brew brew)
if(brew)
	find_package_message(brew "Homebrew found: ${brew}" "1;${brew}")
else()
	find_package_message(brew "Homebrew not found" "0")
	return()
endif()

execute_process(COMMAND "${brew}" --prefix icu4c
	RESULT_VARIABLE brew_icu_f
	OUTPUT_VARIABLE brew_icu OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(brew_icu_f EQUAL 0)
	find_package_message(brew_icu "ICU via Homebrew: ${brew_icu}" "${brew_icu}")
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${brew_icu}/lib/pkgconfig")
endif()

execute_process(COMMAND "${brew}" --prefix openssl
	RESULT_VARIABLE brew_ssl_f
	OUTPUT_VARIABLE brew_ssl OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(brew_ssl_f EQUAL 0)
	find_package_message(brew_ssl "OpenSSL via Homebrew: ${brew_ssl}"
		"${brew_ssl}")
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${brew_ssl}/lib/pkgconfig")
endif()

execute_process(COMMAND "${brew}" --prefix python3
	RESULT_VARIABLE brew_python_f
	OUTPUT_VARIABLE brew_python OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(brew_python_f EQUAL 0)
	find_package_message(brew_python "Python via Homebrew: ${brew_python}"
		"${brew_python}")
	list(APPEND Python3_FRAMEWORKS_ADDITIONAL
		"${brew_python}/Frameworks/Python.framework")
endif()

execute_process(COMMAND "${brew}" --prefix qt5
	RESULT_VARIABLE brew_qt5_f
	OUTPUT_VARIABLE brew_qt5 OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(brew_qt5_f EQUAL 0)
	find_package_message(brew_qt5 "Qt5 via Homebrew: ${brew_qt5}"
		"${brew_qt5}")
endif()

execute_process(COMMAND "${brew}" --prefix gettext
	RESULT_VARIABLE brew_gettext_f
	OUTPUT_VARIABLE brew_gettext OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(brew_gettext_f EQUAL 0)
	find_package_message(brew_gettext "Gettext via homebrew: ${brew_gettext}"
		"${brew_gettext}")
	set(ENV{PATH} "$ENV{PATH}:${brew_gettext}/bin")
endif()
