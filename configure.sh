#!/bin/sh
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

# http://stackoverflow.com/questions/18993438/shebang-env-preferred-python-version
# http://stackoverflow.com/questions/12070516/conditional-shebang-line-for-different-versions-of-python
""":"
which python3 >/dev/null 2>&1 && exec python3 "$0" "$@"
which python  >/dev/null 2>&1 && exec python  "$0" "$@"
which python2 >/dev/null 2>&1 && exec python2 "$0" "$@"
echo "Error: configure wrapper requires python"
exec echo "Either install python, or use cmake directly"
":"""

import argparse
import os
import subprocess
import sys
import re

extra_args = [os.path.dirname(sys.argv[0])]
parser = argparse.ArgumentParser()

def gnu_install_dir(name, cmake=None):
    if cmake is None:
        cmake = name.upper()
    parser.add_argument('--' + name, action='append', metavar=cmake,
            dest='cm_args',
            type=lambda s: '-DCMAKE_INSTALL_{}={}'.format(cmake, s))

gnu_install_dir('prefix')
gnu_install_dir('bindir')
gnu_install_dir('sbindir')
gnu_install_dir('libexecdir')
gnu_install_dir('sysconfdir')
gnu_install_dir('sharedstatedir')
gnu_install_dir('localstatedir')
gnu_install_dir('libdir')
gnu_install_dir('includedir')
gnu_install_dir('oldincludedir')
gnu_install_dir('datarootdir')
gnu_install_dir('datadir')
gnu_install_dir('infodir')
gnu_install_dir('localedir')
gnu_install_dir('mandir')
gnu_install_dir('docdir')


group = parser.add_mutually_exclusive_group()
group.add_argument('--enable-debug', action='store_const', dest='build_type',
        const='Debug', default='Release')
group.add_argument('--disable-debug', action='store_const', dest='build_type',
        const='Release', default='Release')

def tristate(name, cmake=None):
    if cmake is None:
        cmake = name.upper()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--enable-' + name, action='append_const',
            dest='cm_args', const='-DWANT_{}=YES'.format(cmake))
    group.add_argument('--disable-' + name, action='append_const',
            dest='cm_args', const='-DWANT_{}=NO'.format(cmake))

tristate('ipv6')
tristate('openssl')
tristate('zlib')
tristate('perl')
tristate('swig')
tristate('cyrus')
tristate('charset', 'ICU')
tristate('tcl')
tristate('i18n')

class HandlePython(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        extra_args.append('-DWANT_PYTHON=YES')
        if values is not None:
            extra_args.append('-DWANT_PYTHON_VERSION=' + values)

group = parser.add_mutually_exclusive_group()
group.add_argument('--enable-python', action=HandlePython, nargs='?',
        metavar='PYTHON_VERSION')
group.add_argument('--disable-python', action='append_const', dest='cm_args',
        const='-DWANT_PYTHON=NO')

parser.add_argument('--with-gtest', action='store', dest='gtest')
parser.add_argument('--with-gmock', action='store', dest='gmock')

class HandleSystemd(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        extra_args.append('-DWANT_SYSTEMD=YES')
        if values is not None:
            extra_args.append('-DWANT_SYSTEMD_DIR=' + values)

parser.add_argument('--with-systemdsystemunitdir', action=HandleSystemd,
        nargs='?', metavar='UNITDIR')

def env_type(s):
    name, value = s.split('=', 1)
    return (name, value)

parser.add_argument('env', nargs='*', type=env_type, metavar='ENV_VAR=VALUE')

args = parser.parse_args()

cm_args = args.cm_args or []

for env_key, env_value in args.env:
    os.environ[env_key] = env_value
if args.gtest:
    os.environ['GTEST_ROOT'] = args.gtest
if args.gmock:
    os.environ['GMOCK_ROOT'] = args.gmock

if os.environ.get('CXX') is not None:
    extra_args.append('-DCMAKE_CXX_COMPILER=' + os.environ['CXX'])

try:
    os.remove('CMakeCache.txt')
except OSError:
    pass
subprocess.check_call(['cmake', '-DCMAKE_BUILD_TYPE=' + args.build_type]
        + cm_args + extra_args)
