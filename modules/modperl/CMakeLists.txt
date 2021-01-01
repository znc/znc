#
# Copyright (C) 2004-2021 ZNC, see the NOTICE file for details.
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

# TODO: consider switching to swig_add_library() after bumping CMake
# requirements to 3.8, when that command started using IMPLICIT_DEPENDS

set(modinclude_modperl PUBLIC ${PERL_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}/.." PARENT_SCOPE)
set(modcompile_modperl PUBLIC "${PERL_CFLAGS}" PARENT_SCOPE)
set(modlink_modperl PUBLIC ${PERL_LIBRARIES} PARENT_SCOPE)
set(modprop_modperl LINK_FLAGS "${PERL_LDFLAGS}" PARENT_SCOPE)
set(moddef_modperl PUBLIC "SWIG_TYPE_TABLE=znc" PARENT_SCOPE)
set(moddepend_modperl modperl_functions modperl_swigruntime PARENT_SCOPE)

if(APPLE)
	set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
		"${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

if(SWIG_FOUND)
	add_custom_command(
		OUTPUT perlfunctions.cpp
		COMMAND "${PERL_EXECUTABLE}"
		"${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl"
		"${CMAKE_CURRENT_SOURCE_DIR}/functions.in"
		"perlfunctions.cpp"
		VERBATIM
		DEPENDS codegen.pl functions.in)

	add_custom_command(
		OUTPUT swigperlrun.h
		COMMAND "${SWIG_EXECUTABLE}" -perl -c++ -shadow -external-runtime
		"swigperlrun.h"
		VERBATIM)

	add_custom_command(
		OUTPUT modperl_biglib.cpp ZNC.pm
		COMMAND "${SWIG_EXECUTABLE}" -perl -c++ -shadow
		"-I${PROJECT_BINARY_DIR}/include"
		"-I${PROJECT_SOURCE_DIR}/include"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/.."
		"-I${CMAKE_CURRENT_SOURCE_DIR}/include"
		-DZNC_EXPORT_LIB_EXPORT
		-outdir "${CMAKE_CURRENT_BINARY_DIR}"
		-o "${CMAKE_CURRENT_BINARY_DIR}/modperl_biglib.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/modperl.i"
		DEPENDS "modperl.i" copy_csocket_h
		IMPLICIT_DEPENDS CXX "${CMAKE_CURRENT_SOURCE_DIR}/modperl.i"
		VERBATIM)
else()
	add_custom_command(
		OUTPUT swigperlrun.h ZNC.pm modperl_biglib.cpp perlfunctions.cpp
		COMMAND "${CMAKE_COMMAND}" -E tar xz
		"${CMAKE_CURRENT_SOURCE_DIR}/generated.tar.gz"
		VERBATIM)
endif()
add_custom_target(modperl_functions DEPENDS "perlfunctions.cpp")
add_custom_target(modperl_swigruntime DEPENDS "swigperlrun.h")
add_custom_target(modperl_swig DEPENDS "modperl_biglib.cpp" "ZNC.pm")

execute_process(COMMAND "${PERL_EXECUTABLE}"
	-MConfig "-eprint $Config::Config{dlext}"
	OUTPUT_VARIABLE perl_ext)

znc_add_library(modperl_lib MODULE modperl_biglib.cpp)
add_dependencies(modperl_lib modperl_swig)
target_include_directories(modperl_lib PRIVATE
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/.."
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
	${PERL_INCLUDE_DIRS})
target_link_libraries(modperl_lib ${znc_link})
set_target_properties(modperl_lib PROPERTIES
	PREFIX ""
	SUFFIX ".${perl_ext}"
	OUTPUT_NAME "ZNC"
	NO_SONAME true
	LINK_FLAGS "${PERL_LDFLAGS}")
target_compile_options(modperl_lib PRIVATE "${PERL_CFLAGS}")
target_compile_definitions(modperl_lib PRIVATE "SWIG_TYPE_TABLE=znc")
if(CYGWIN)
	target_link_libraries(modperl_lib module_modperl)
endif()

install(TARGETS modperl_lib
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc/modperl")
install(FILES "startup.pl" "${CMAKE_CURRENT_BINARY_DIR}/ZNC.pm"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc/modperl")

function(add_perl_module mod modpath)
	install(FILES "${modpath}" DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc")
endfunction()

# This target is used to generate tarball which doesn't depend on SWIG.
add_custom_target(modperl_dist
	COMMAND "${CMAKE_COMMAND}" -E tar cz
	"${CMAKE_CURRENT_SOURCE_DIR}/generated.tar.gz"
	"swigperlrun.h" "ZNC.pm" "modperl_biglib.cpp" "perlfunctions.cpp"
	DEPENDS swigperlrun.h ZNC.pm modperl_biglib.cpp perlfunctions.cpp
	VERBATIM)
add_dependencies(modperl_dist copy_csocket_h)
