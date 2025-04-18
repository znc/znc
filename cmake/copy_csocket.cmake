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

function(copy_csocket tgt source destination)
	add_custom_command(OUTPUT "${destination}"
		COMMAND "${CMAKE_COMMAND}"
		-D "source=${source}"
		-D "destination=${destination}"
		-P "${PROJECT_SOURCE_DIR}/cmake/copy_csocket_cmd.cmake"
		DEPENDS "${source}" "${PROJECT_SOURCE_DIR}/cmake/copy_csocket_cmd.cmake"
		VERBATIM)
	add_custom_target("${tgt}" DEPENDS "${destination}")
endfunction()
