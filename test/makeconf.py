#!/usr/bin/env python3
#
# Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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


import pexpect
import sys
import tempfile

with tempfile.TemporaryDirectory() as config_dir:
    znc = pexpect.spawnu('./znc', ['--debug', '--datadir', config_dir, '--makeconf'])
    znc.logfile_read = sys.stdout
    znc.expect_exact('Listen on port');          znc.sendline('12345')
    znc.expect_exact('Listen using SSL');        znc.sendline()
    znc.expect_exact('IPv6');                    znc.sendline()
    znc.expect_exact('Username');                znc.sendline('user')
    znc.expect_exact('password');                znc.sendline('hunter2')
    znc.expect_exact('Confirm');                 znc.sendline('hunter2')
    znc.expect_exact('Nick [user]');             znc.sendline()
    znc.expect_exact('Alternate nick [user_]');  znc.sendline()
    znc.expect_exact('Ident [user]');            znc.sendline()
    znc.expect_exact('Real name');               znc.sendline()
    znc.expect_exact('Bind host');               znc.sendline()
    znc.expect_exact('Set up a network?');       znc.sendline()
    znc.expect_exact('Name [freenode]');         znc.sendline('test')
    znc.expect_exact('Server host (host only)'); znc.sendline('localhost')
    znc.expect_exact('Server uses SSL?');        znc.sendline()
    znc.expect_exact('6667');                    znc.sendline()
    znc.expect_exact('password');                znc.sendline()
    znc.expect_exact('channels');                znc.sendline()
    znc.expect_exact('Launch ZNC now?');         znc.sendline('no')
    znc.expect_exact(pexpect.EOF)
