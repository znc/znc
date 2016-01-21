#!/usr/bin/env python3
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

import argparse
import glob
import os
import re

parser = argparse.ArgumentParser(
        description='Extract translateable strings from .tmpl files')
parser.add_argument('--directory', action='store')
parser.add_argument('--output', action='store')
args = parser.parse_args()

pattern = re.compile(r'<\?\s*(?:FORMAT|(PLURAL))\s+(?:CTX="([^"]+?)"\s+)?"([^"]+?)"(?(1)\s+"([^"]+?)"|).*?\?>')

result = []

for fname in glob.iglob(args.directory + '/*.tmpl'):
    fbase = os.path.basename(fname)
    with open(fname) as f:
        for linenum, line in enumerate(f):
            for x in pattern.finditer(line):
                text, plural, context = x.group(3), x.group(4), x.group(2)
                result.append('#: {}:{}'.format(fbase, linenum + 1))
                if context:
                    result.append('msgctxt "{}"'.format(context))
                result.append('msgid "{}"'.format(text))
                if plural:
                    result.append('msgid_plural "{}"'.format(plural))
                    result.append('msgstr[0] ""')
                    result.append('msgstr[1] ""')
                else:
                    result.append('msgstr ""')
                result.append('')

if result:
    with open(args.output, 'w') as f:
        for line in result:
            print(line, file=f)
