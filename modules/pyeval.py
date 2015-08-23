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


import sys
import re
from code import InteractiveInterpreter

import znc


class pyeval(znc.Module, InteractiveInterpreter):
    module_types = [znc.CModInfo.UserModule, znc.CModInfo.NetworkModule]
    description = 'Evaluates python code'

    def write(self, data):
        for line in data.split('\n'):
            if len(line):
                self.PutModule(line)

    def resetbuffer(self):
        """Reset the input buffer."""
        self.buffer = []

    def push(self, line):
        self.buffer.append(line)
        source = "\n".join(self.buffer)
        more = self.runsource(source, self.filename)
        if not more:
            self.resetbuffer()
        return more

    def OnLoad(self, args, message):
        if not self.GetUser().IsAdmin():
            message.s = 'You must have admin privileges to load this module.'
            return False

        self.filename = "<console>"
        self.resetbuffer()
        self.locals['znc'] = znc
        self.locals['module'] = self
        self.locals['user'] = self.GetUser()
        self.indent = re.compile(r'^>+')

        return True

    def OnModCommand(self, line):
        self.locals['client'] = self.GetClient()
        self.locals['network'] = self.GetNetwork()

        # Hijack sys.stdout.write
        stdout_write = sys.stdout.write
        sys.stdout.write = self.write

        m = self.indent.match(line)
        if m:
            self.push(('    ' * len(m.group())) + line[len(m.group()):])
        elif line == ' ' or line == '<':
            self.push('')
        else:
            self.push(line)

        # Revert sys.stdout.write
        sys.stdout.write = stdout_write
        del self.locals['client']
        del self.locals['network']

