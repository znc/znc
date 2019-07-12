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

set(modinclude_modtcl PRIVATE ${TCL_INCLUDE_PATH} PARENT_SCOPE)
set(modlink_modtcl PRIVATE ${TCL_LIBRARY} PARENT_SCOPE)

install(FILES "modtcl.tcl" "binds.tcl"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/znc/modtcl")
