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

copy_csocket(copy_csocket_h
	"${PROJECT_SOURCE_DIR}/third_party/Csocket/Csocket.h"
	"${CMAKE_CURRENT_BINARY_DIR}/Csocket.h")

# __attribute__((__visibility__("default")) macro comes from
# generate_export_header(), which wants a library.
# we don't really care which library is that.
# Macro defined by this generated header is used in Modules.h
# Name of this pseudolibrary matches the define passed to modules.
add_library(znc_export_lib SHARED IMPORTED)
include(GenerateExportHeader)
generate_export_header(znc_export_lib)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/" "${CMAKE_CURRENT_BINARY_DIR}/"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/znc"
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN CMakeFiles EXCLUDE)
