#
# Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

# This is subproject because of the following error in FreeBSD after run of
# pkg autoremove so only runtime dependencies are installed.
#
# CMake Error at /usr/local/lib/cmake/Qt5Core/Qt5CoreConfig.cmake:15 (message):
#  The imported target "Qt5::Core" references the file
#     "/usr/local/lib/qt5/bin/qmake"
#  but this file does not exist.  Possible reasons include:
#  * The file was deleted, renamed, or moved to another location.
#  * An install or uninstall procedure did not complete successfully.
#  * The installation package was faulty and contained
#     "/usr/local/lib/cmake/Qt5Core/Qt5CoreConfigExtras.cmake"
#  but not all the files it references.

cmake_minimum_required(VERSION 3.0)
project(qt5check)
set(CMAKE_VERBOSE_MAKEFILE true)

find_package(Qt5Network 5.4 REQUIRED HINTS ${Qt5_HINTS})

add_executable(main main.cpp)
target_link_libraries(main Qt5::Network)

add_custom_target(msg ALL COMMAND "${CMAKE_COMMAND}" -E echo
	"QT5_VERSION_BEGIN:${Qt5Network_VERSION}:QT5_VERSION_END" VERBATIM)
