#!/usr/bin/env python3
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

# The purpose of this file is to fix LLVM profiler data in the parallel
# execution of the test - all of them write to the same file, so it races and
# produces invalid data

import os
import sys
import re

test = ''
for arg in sys.argv:
    m = re.match(r'--gtest_filter=(.*)',  arg)
    if m:
        test = m[1]
        break

if test != '' and 'LLVM_PROFILE_FILE' in os.environ:
    os.environ['LLVM_PROFILE_FILE'] += '.' + test

binary = os.environ['INTTEST_BIN']
sys.argv[0] = binary

os.execv(binary, sys.argv)
