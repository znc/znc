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

# The purpose of this file is to fix environment in the parallel execution of
# the test with filenames of test results - otherwise all of them write to the
# same file, so it races and produces invalid data

# See also: https://github.com/google/gtest-parallel/pull/89

import os
import sys
import re
import random

flag = re.compile(r'--gtest_filter=(.*)')

test = ''
for arg in sys.argv:
    m = flag.search(arg)
    if m:
        test = m[1]
        break

# TODO: perhaps it should be hash of test name instead?
pid = os.getpid()
rand = random.randint(0, 1000)
value = f'{pid}-{rand}'
for var in ['GTEST_OUTPUT']:
    if var in os.environ:
        if test == '':
            # Avoid outputting xml during test discovery
            # Otherwise jenkins counts every test twice
            del os.environ[var]
        else:
            os.environ[var] = os.environ[var].replace('XXX', value)

binary = os.environ['INTTEST_BIN']
sys.argv[0] = binary

os.execv(binary, sys.argv)
