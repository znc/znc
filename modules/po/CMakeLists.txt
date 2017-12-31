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

include(translation)

foreach(modpath ${actual_modules})
	get_filename_component(mod "${modpath}" NAME_WE)
	get_filename_component(module "${modpath}" NAME)
	translation(SHORT "${mod}" FULL "znc-${mod}" SOURCES "${module}"
		TMPLDIRS "${CMAKE_CURRENT_SOURCE_DIR}/../data/${mod}/tmpl")
endforeach()
