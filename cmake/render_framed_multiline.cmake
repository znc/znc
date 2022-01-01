#
# Copyright (C) 2004-2022 ZNC, see the NOTICE file for details.
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

function(render_framed_multiline lines)
	set(max_len 0)
	foreach(l ${lines})
		string(LENGTH "${l}" l_len)
		if(l_len GREATER max_len)
			set(max_len "${l_len}")
		endif()
	endforeach()
	set(i 0)
	set(header)
	while(i LESS max_len)
		set(header "${header}-")
		math(EXPR i "${i} + 1")
	endwhile()
	message("+-${header}-+")
	foreach(l ${lines})
		string(LENGTH "${l}" l_len)
		while(l_len LESS max_len)
			set(l "${l} ")
			math(EXPR l_len "${l_len} + 1")
		endwhile()
		message("| ${l} |")
	endforeach()
	message("+-${header}-+")
endfunction()

