#
# Copyright (C) 2004-2023 ZNC, see the NOTICE file for details.
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

include(CMakeParseArguments)
function(translation)
	cmake_parse_arguments(arg "" "FULL;SHORT" "SOURCES;TMPLDIRS" ${ARGN})
	set(short "${arg_SHORT}")
	file(GLOB all_po "${short}.*.po")

	if(XGETTEXT_EXECUTABLE)
		set(params)
		foreach(i ${arg_SOURCES})
			list(APPEND params "--explicit_sources=${i}")
		endforeach()
		foreach(i ${arg_TMPLDIRS})
			list(APPEND params "--tmpl_dirs=${i}")
		endforeach()
		add_custom_target("translation_${short}"
			COMMAND "${PROJECT_SOURCE_DIR}/translation_pot.py"
			"--include_dir=${CMAKE_CURRENT_SOURCE_DIR}/.."
			"--strip_prefix=${PROJECT_SOURCE_DIR}/"
			"--tmp_prefix=${CMAKE_CURRENT_BINARY_DIR}/${short}"
			"--output=${CMAKE_CURRENT_SOURCE_DIR}/${short}.pot"
			${params}
			VERBATIM)
		foreach(one_po ${all_po})
			add_custom_command(TARGET "translation_${short}" POST_BUILD
				COMMAND "${GETTEXT_MSGMERGE_EXECUTABLE}"
				--update --quiet --backup=none "${one_po}" "${short}.pot"
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				VERBATIM)
		endforeach()
		add_dependencies(translation "translation_${short}")
	endif()

	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${short}.pot")
		return()
	endif()

	znc_add_custom_target("po_${short}")
	foreach(one_po ${all_po})
		get_filename_component(longext "${one_po}" EXT)
		if(NOT longext MATCHES "^\\.([a-zA-Z_]+)\\.po$")
			message(WARNING "Unrecognized translation file ${one_po}")
			continue()
		endif()
		set(lang "${CMAKE_MATCH_1}")

		add_custom_command(OUTPUT "${short}.${lang}.gmo"
			COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}"
			-D "${CMAKE_CURRENT_SOURCE_DIR}"
			-o "${short}.${lang}.gmo"
			"${short}.${lang}.po"
			DEPENDS "${short}.${lang}.po"
			VERBATIM)
		add_custom_target("po_${short}_${lang}" DEPENDS "${short}.${lang}.gmo")
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${short}.${lang}.gmo"
			DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES"
			RENAME "${arg_FULL}.mo")
		add_dependencies("po_${short}" "po_${short}_${lang}")
	endforeach()
endfunction()
