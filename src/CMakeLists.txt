#
# Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

set(znc_cpp "ZNCString.cpp" "znc.cpp" "IRCNetwork.cpp" "Translation.cpp"
	"IRCSock.cpp" "Client.cpp" "Chan.cpp" "Nick.cpp" "Server.cpp"
	"Modules.cpp" "MD5.cpp" "Buffer.cpp" "Utils.cpp" "FileUtils.cpp"
	"HTTPSock.cpp" "Template.cpp" "ClientCommand.cpp" "Socket.cpp"
	"SHA256.cpp" "WebModules.cpp" "Listener.cpp" "Config.cpp" "ZNCDebug.cpp"
	"Threads.cpp" "Query.cpp" "SSLVerifyHost.cpp" "Message.cpp" "User.cpp")
znc_add_library(znclib ${lib_type} ${znc_cpp} "Csocket.cpp" "versionc.cpp"
	${cctz_cc})
znc_add_executable(znc "main.cpp")
target_link_libraries(znc PRIVATE znclib)

copy_csocket(copy_csocket_cc
	"${PROJECT_SOURCE_DIR}/third_party/Csocket/Csocket.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/Csocket.cpp")

# make-tarball.sh can create this file, it'll contain -nightly-20151221-71eaf94
if(EXISTS "${PROJECT_SOURCE_DIR}/.nightly")
	file(READ "${PROJECT_SOURCE_DIR}/.nightly" nightly)
	string(STRIP "${nightly}" nightly)
else()
	set(nightly "")
endif()

add_custom_target(version
	COMMAND "${CMAKE_COMMAND}"
	-D "nightly=${nightly}"
	-D "alpha_version=${alpha_version}"
	-D "append_git_version=${append_git_version}"
	-D "gitcmd=${GIT_EXECUTABLE}"
	-D "srcdir=${PROJECT_SOURCE_DIR}"
	-D "srcfile=${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in"
	-D "destfile=${CMAKE_CURRENT_BINARY_DIR}/versionc.cpp"
	-P "${PROJECT_SOURCE_DIR}/cmake/gen_version.cmake"
	# This is versionc to avoid possible conflict with version.cpp generated
	# by make-tarball.sh
	BYPRODUCTS "versionc.cpp"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	VERBATIM)

add_dependencies(znclib copy_csocket_h copy_csocket_cc version)

set(znc_include_dirs
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>")
target_link_libraries(znclib PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
if(ZLIB_FOUND)
	target_link_libraries(znclib PRIVATE ZLIB::ZLIB)
endif()
if(Boost_LOCALE_FOUND)
	target_link_libraries(znclib PRIVATE Boost::locale)
endif()
if(ZNC_HAVE_ARGON)
	target_link_libraries(znclib PRIVATE PkgConfig::ARGON)
endif()
target_link_libraries(znclib PUBLIC ${zncpubdeps})
target_link_libraries(znclib PRIVATE cctz::cctz)
target_include_directories(znc PUBLIC ${znc_include_dirs})
target_include_directories(znclib PUBLIC ${znc_include_dirs})

set_target_properties(znclib ${znc_link} PROPERTIES
	COMPILE_OPTIONS "-include;znc/zncconfig.h"
	INTERFACE_COMPILE_OPTIONS "-include;znc/zncconfig.h"
	POSITION_INDEPENDENT_CODE true
	INTERFACE_POSITION_INDEPENDENT_CODE true
	# This option is relevant only on non-cygwin
	ENABLE_EXPORTS true)
# The following options are relevant only on cygwin
set_target_properties(znclib PROPERTIES
	OUTPUT_NAME "znc"
	SOVERSION "${ZNC_VERSION}")

add_library(ZNC INTERFACE)
target_link_libraries(ZNC INTERFACE ${znc_link} ${zncpubdeps})
target_compile_definitions(ZNC INTERFACE "znc_export_lib_EXPORTS")
target_compile_features(ZNC INTERFACE cxx_std_17)
target_compile_features(znclib PUBLIC cxx_std_17)

if(HAVE_I18N)
	add_subdirectory(po)
endif()


install(TARGETS znc ${install_lib}
	EXPORT znc_internal
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	# this one is libznc.dll.a for cygwin
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(TARGETS ZNC EXPORT znc_public)
install(EXPORT znc_internal
	DESTINATION "${CMAKE_INSTALL_DATADIR}/znc/cmake"
	NAMESPACE ZNC::internal::)
install(EXPORT znc_public
	DESTINATION "${CMAKE_INSTALL_DATADIR}/znc/cmake"
	NAMESPACE ZNC::)
