#
# Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

if(nightly)
	set(git_info "${nightly}")
elseif(append_git_version)

	set(git ${gitcmd} "--git-dir=${srcdir}/.git")

	execute_process(COMMAND ${git} describe --abbrev=0 HEAD
		RESULT_VARIABLE git_result
		ERROR_QUIET
		OUTPUT_VARIABLE latest_tag)
	string(STRIP "${latest_tag}" latest_tag)

	if(git_result EQUAL 0)
		execute_process(COMMAND ${git} rev-parse --short HEAD
			OUTPUT_VARIABLE short_id)
		string(STRIP "${short_id}" short_id)

		if(latest_tag STREQUAL "")
			# shallow clone
			set(git_info "-git-${short_id}")
		else()
			# One character "x" per commit, remove newlines from output
			execute_process(COMMAND ${git} log --format=tformat:x
				"${latest_tag}..HEAD"
				OUTPUT_VARIABLE commits_since)
			string(REGEX REPLACE "[^x]" "" commits_since "${commits_since}")
			string(LENGTH "${commits_since}" commits_since)

			if(commits_since EQUAL 0)
				# If this commit is tagged, don't print anything
				# (the assumption here is: this is a release)
				set(git_info "")
				# However, this shouldn't happen, as for releases
				# append_git_version should be set to false
			else()
				set(git_info "-git-${commits_since}-${short_id}")
			endif()
		endif()

		execute_process(COMMAND ${gitcmd} status
			--ignore-submodules=dirty
			--porcelain
			-- third_party/Csocket
			WORKING_DIRECTORY "${srcdir}"
			OUTPUT_VARIABLE franken)
		string(STRIP "${franken}" franken)
		if(NOT franken STREQUAL "")
			message(WARNING " Csocket submodule looks outdated.\n"
				" Run: git submodule update --init --recursive")
			set(git_info "${git_info}-frankenznc")
		endif()
	else()
		# Probably .git/ or git isn't found, or something
		set(git_info "-git-unknown")
	endif()
else()
	set(git_info "")
endif()

configure_file("${srcfile}" "${destfile}")
