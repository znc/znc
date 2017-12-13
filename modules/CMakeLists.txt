#
# Copyright (C) 2004-2017 ZNC, see the NOTICE file for details.
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

# This is not recommended, but whatever.
file(GLOB all_modules LIST_DIRECTORIES FALSE "*")

function(add_cxx_module mod modpath)
	znc_add_library("module_${mod}" MODULE "${modpath}")
	set_target_properties("module_${mod}" PROPERTIES
		OUTPUT_NAME "${mod}"
		PREFIX ""
		SUFFIX ".so"
		NO_SONAME true
		CXX_VISIBILITY_PRESET "hidden")
	if(moddef_${mod})
		target_compile_definitions("module_${mod}" ${moddef_${mod}})
	endif()
	if(modcompile_${mod})
		target_compile_options("module_${mod}" ${modcompile_${mod}})
	endif()
	if(modinclude_${mod})
		target_include_directories("module_${mod}" ${modinclude_${mod}})
	endif()
	if(moddepend_${mod})
		add_dependencies("module_${mod}" ${moddepend_${mod}})
	endif()
	# ${znclib_LIB_DEPENDS} is to overcome OSX's need for -undefined suppress
	# when accessing symbols exported by dependencies of znclib (e.g.
	# openssl), but not used in znclib itself
	target_link_libraries("module_${mod}" PRIVATE ZNC ${modlink_${mod}}
		${znclib_LIB_DEPENDS})
	set_target_properties("module_${mod}" PROPERTIES "" "" ${modprop_${mod}})
	install(TARGETS "module_${mod}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc")
endfunction()

function(add_perl_module mod modpath)
endfunction()

function(add_python_module mod modpath)
endfunction()

if(CYRUS_FOUND)
	set(modcompile_cyrusauth PRIVATE ${CYRUS_CFLAGS})
	set(modlink_cyrusauth ${CYRUS_LDFLAGS})
else()
	set(moddisable_cyrusauth true)
endif()

if(PERLLIBS_FOUND)
	add_subdirectory(modperl)
else()
	set(moddisable_modperl true)
endif()

if(PYTHON_FOUND)
	add_subdirectory(modpython)
else()
	set(moddisable_modpython true)
endif()

if(TCL_FOUND)
	add_subdirectory(modtcl)
else()
	set(moddisable_modtcl true)
endif()

set(actual_modules)

foreach(modpath ${all_modules})
	if(NOT "${modpath}" MATCHES "/([-a-zA-Z0-9_]+)\\.([a-z]+)$")
		continue()
	endif()
	set(mod "${CMAKE_MATCH_1}")
	set(modtype "${CMAKE_MATCH_2}")
	if(mod STREQUAL "CMakeLists" OR mod STREQUAL "Makefile")
		continue()
	endif()

	list(APPEND actual_modules "${modpath}")

	set(modenabled true)

	if(moddisable_${mod})
		set(modenabled false)
	endif()

	if(NOT OPENSSL_FOUND)
		file(READ "${modpath}" modcontent)
		string(FIND "${modcontent}" "REQUIRESSL" requiressl)
		if(NOT requiressl EQUAL "-1")
			set(modenabled false)
		endif()
	endif()

	if(modenabled)
		if(modtype STREQUAL "cpp")
			add_cxx_module("${mod}" "${modpath}")
		endif()
		if(modtype STREQUAL "pm")
			add_perl_module("${mod}" "${modpath}")
		endif()
		if(modtype STREQUAL "py")
			add_python_module("${mod}" "${modpath}")
		endif()
	endif()
endforeach()

if(HAVE_I18N)
	add_subdirectory(po)
endif()

install(DIRECTORY data/
	DESTINATION "${CMAKE_INSTALL_DATADIR}/znc/modules")
