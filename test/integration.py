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
import subprocess
import sys
import tempfile
import unittest

from contextlib import contextmanager


def write_config(config_dir):
    znc = pexpect.spawnu('./znc', ['--debug', '--datadir', config_dir, '--makeconf'])
    znc.timeout = 180
    try:
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
        znc.expect_exact('Server host (host only)'); znc.sendline('::1')
        znc.expect_exact('Server uses SSL?');        znc.sendline()
        znc.expect_exact('6667');                    znc.sendline()
        znc.expect_exact('password');                znc.sendline()
        znc.expect_exact('channels');                znc.sendline()
        znc.expect_exact('Launch ZNC now?');         znc.sendline('no')
        znc.expect_exact(pexpect.EOF)
    finally:
        znc.terminate()


class TestZNC(unittest.TestCase):
    def setUp(self):
        config = tempfile.TemporaryDirectory()
        self.addCleanup(config.cleanup)
        write_config(config.name)
        self.config = config.name


    @contextmanager
    def run_znc(self):
        znc = subprocess.Popen(['./znc', '--debug', '--datadir', self.config])
        yield
        znc.terminate()
        # TODO: bump python requirements to 3.3 and use znc.wait(timeout=30) instead.
        # Ubuntu Precise on Travis has too old python.
        self.assertEqual(0, znc.wait())


    @contextmanager
    def run_ircd(self):
        ircd = pexpect.spawnu('socat', ['stdio', 'tcp6-listen:6667,reuseaddr'])
        ircd.timeout = 180
        yield ircd
        ircd.terminate()


    @contextmanager
    def run_client(self):
        # if server didn't start yet, try again, up to 30 times.
        client = pexpect.spawnu('socat', ['stdio', 'tcp6:[::1]:12345,retry=30'])
        client.timeout = 180
        yield client
        client.terminate()


    def test_connect(self):
        with self.run_ircd() as ircd:
            with self.run_znc():
                ircd.expect_exact('CAP LS')
                with self.run_client() as client:
                    client.sendline('PASS :hunter2')
                    client.sendline('NICK :nick')
                    client.sendline('USER user/test x x :x')
                    client.expect_exact('Welcome')


if __name__ == '__main__':
    unittest.main()

