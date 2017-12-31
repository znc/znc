/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ZNCDEBUG_H
#define ZNCDEBUG_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <sstream>

/** Output a debug info if debugging is enabled.
 *  If ZNC was compiled with <code>--enable-debug</code> or was started with
 *  <code>--debug</code>, the given argument will be sent to stdout.
 *
 *  You can use all the features of C++ streams:
 *  @code
 *  DEBUG("I had " << errors << " errors");
 *  @endcode
 *
 *  @param f The expression you want to display.
 */
#define DEBUG(f)                 \
    do {                         \
        if (CDebug::Debug()) {   \
            CDebugStream sDebug; \
            sDebug << f;         \
        }                        \
    } while (0)

class CDebug {
  public:
    static void SetStdoutIsTTY(bool b) { stdoutIsTTY = b; }
    static bool StdoutIsTTY() { return stdoutIsTTY; }
    static void SetDebug(bool b) { debug = b; }
    static bool Debug() { return debug; }

    static CString Filter(const CString& sUnfilteredLine);

  protected:
    static bool stdoutIsTTY;
    static bool debug;
};

class CDebugStream : public std::ostringstream {
  public:
    ~CDebugStream();
};

#endif  // !ZNCDEBUG_H
