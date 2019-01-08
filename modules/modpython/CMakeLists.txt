#
# Copyright (C) 2004-2019 ZNC, see the NOTICE file for details.
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

set(modinclude_modpython PUBLIC ${PYTHON_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}/.." PARENT_SCOPE)
set(modlink_modpython PUBLIC ${PYTHON_LDFLAGS} PARENT_SCOPE)
set(moddef_modpython PUBLIC "SWIG_TYPE_TABLE=znc" PARENT_SCOPE)
set(moddepend_modpython modpython_functions modpython_swigruntime PARENT_SCOPE)

if(APPLE)
	set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
		"${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

if(SWIG_FOUND)
	add_custom_command(
		OUTPUT "pyfunctions.cpp"
		COMMAND "${PERL_EXECUTABLE}"
		"${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl"
		"${CMAKE_CURRENT_SOURCE_DIR}/functions.in"
		"pyfunctions.cpp"
		VERBATIM
		DEPENDS codegen.pl functions.in)

	add_custom_command(
		OUTPUT "swigpyrun.h"
		COMMAND "${SWIG_EXECUTABLE}" -python -py3 -c++ -shadow -external-runtime
		"swigpyrun.h"
		VERBATIM)

	add_custom_command(
		OUTPUT "modpython_biglib.cpp" "znc_core.py"
		COMMAND "${SWIG_EXECUTABLE}" -python -py3 -c++ -shadow
		"-I${PROJECT_BINARY_DIR}/include"
		"-I${PROJECT_SOURCE_DIR}/include"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/.."
		-DZNC_EXPORT_LIB_EXPORT
		-outdir "${CMAKE_CURRENT_BINARY_DIR}"
		-o "${CMAKE_CURRENT_BINARY_DIR}/modpython_biglib.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/modpython.i"
		DEPENDS "modpython.i" copy_csocket_h
		IMPLICIT_DEPENDS CXX "${CMAKE_CURRENT_SOURCE_DIR}/modpython.i"
		VERBATIM)
else()
	add_custom_command(
		OUTPUT swigpyrun.h znc_core.py modpython_biglib.cpp pyfunctions.cpp
		COMMAND "${CMAKE_COMMAND}" -E tar xz
		"${CMAKE_CURRENT_SOURCE_DIR}/generated.tar.gz"
		VERBATIM)
endif()
add_custom_target(modpython_functions DEPENDS "pyfunctions.cpp")
add_custom_target(modpython_swigruntime DEPENDS "swigpyrun.h")
add_custom_target(modpython_swig DEPENDS "modpython_biglib.cpp" "znc_core.py")

znc_add_library(modpython_lib MODULE modpython_biglib.cpp)
add_dependencies(modpython_lib modpython_swig)
target_include_directories(modpython_lib PRIVATE
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/.."
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
	${PYTHON_INCLUDE_DIRS})
target_link_libraries(modpython_lib ${znc_link} ${PYTHON_LDFLAGS})
set_target_properties(modpython_lib PROPERTIES
	PREFIX "_"
	OUTPUT_NAME "znc_core"
	NO_SONAME true)
target_compile_definitions(modpython_lib PRIVATE "SWIG_TYPE_TABLE=znc")
if(CYGWIN)
	target_link_libraries(modpython_lib module_modpython)
endif()

install(TARGETS modpython_lib
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc/modpython")
install(FILES "znc.py" "${CMAKE_CURRENT_BINARY_DIR}/znc_core.py"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc/modpython")

function(add_python_module mod modpath)
	install(FILES "${modpath}" DESTINATION "${CMAKE_INSTALL_LIBDIR}/znc")
endfunction()

# This target is used to generate tarball which doesn't depend on SWIG.
add_custom_target(modpython_dist
	COMMAND "${CMAKE_COMMAND}" -E tar cz
	"${CMAKE_CURRENT_SOURCE_DIR}/generated.tar.gz"
	"swigpyrun.h" "znc_core.py" "modpython_biglib.cpp" "pyfunctions.cpp"
	DEPENDS swigpyrun.h znc_core.py modpython_biglib.cpp pyfunctions.cpp
	VERBATIM)
add_dependencies(modpython_dist copy_csocket_h)
