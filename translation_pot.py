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
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--include_dir', action='store')
parser.add_argument('--explicit_sources', action='append')
parser.add_argument('--tmpl_dirs', action='append')
parser.add_argument('--strip_prefix', action='store')
parser.add_argument('--tmp_prefix', action='store')
parser.add_argument('--output', action='store')
args = parser.parse_args()

pot_list = []

# .tmpl
tmpl_pot = args.tmp_prefix + '_tmpl.pot'
tmpl_uniq_pot = args.tmp_prefix + '_tmpl_uniq.pot'
tmpl = []
pattern = re.compile(r'<\?\s*(?:FORMAT|(PLURAL))\s+(?:CTX="([^"]+?)"\s+)?"([^"]+?)"(?(1)\s+"([^"]+?)"|).*?\?>')
for tmpl_dir in args.tmpl_dirs:
    for fname in glob.iglob(tmpl_dir + '/*.tmpl'):
        fbase = fname[len(args.strip_prefix):]
        with open(fname, 'rt', encoding='utf8') as f:
            for linenum, line in enumerate(f):
                for x in pattern.finditer(line):
                    text, plural, context = x.group(3), x.group(4), x.group(2)
                    tmpl.append('#: {}:{}'.format(fbase, linenum + 1))
                    if context:
                        tmpl.append('msgctxt "{}"'.format(context))
                    tmpl.append('msgid "{}"'.format(text))
                    if plural:
                        tmpl.append('msgid_plural "{}"'.format(plural))
                        tmpl.append('msgstr[0] ""')
                        tmpl.append('msgstr[1] ""')
                    else:
                        tmpl.append('msgstr ""')
                    tmpl.append('')
if tmpl:
    with open(tmpl_pot, 'wt', encoding='utf8') as f:
        print('msgid ""', file=f)
        print('msgstr ""', file=f)
        print(r'"Content-Type: text/plain; charset=UTF-8\n"', file=f)
        print(r'"Content-Transfer-Encoding: 8bit\n"', file=f)
        print(file=f)
        for line in tmpl:
            print(line, file=f)
    subprocess.check_call(['msguniq', '-o', tmpl_uniq_pot, tmpl_pot])
    pot_list.append(tmpl_uniq_pot)

# .cpp
main_pot = args.tmp_prefix + '_main.pot'
subprocess.check_call(['xgettext',
    '--omit-header',
    '-D', args.include_dir,
    '-o', main_pot,
    '--keyword=t_s:1,1t',   '--keyword=t_s:1,2c,2t',
    '--keyword=t_f:1,1t',   '--keyword=t_f:1,2c,2t',
    '--keyword=t_p:1,2,3t', '--keyword=t_p:1,2,4c,4t',
    '--keyword=t_d:1,1t',   '--keyword=t_d:1,2c,2t',
] + args.explicit_sources)
if os.path.isfile(main_pot):
    pot_list.append(main_pot)

# combine
if pot_list:
    subprocess.check_call(['msgcat', '-o', args.output] + pot_list)
