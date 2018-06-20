/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

#include <gmock/gmock.h>
#include "base.h"

using testing::AnyOf;
using testing::Eq;

namespace znc_inttest {

Process::Process(QString cmd, QStringList args,
                 std::function<void(QProcess*)> setup)
    : IO(&m_proc, true) {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("ZNC_DEBUG_TIMER", "1");
    // Default exit codes of sanitizers upon error:
    // ASAN - 1
    // LSAN - 23 (part of ASAN, but uses a different value)
    // TSAN - 66
    //
    // ZNC uses 1 too to report startup failure.
    // But we don't want to confuse expected startup failure with ASAN
    // error.
    env.insert("ASAN_OPTIONS", "exitcode=57");
    m_proc.setProcessEnvironment(env);
    setup(&m_proc);
    m_proc.start(cmd, args);
    EXPECT_TRUE(m_proc.waitForStarted())
        << "Failed to start ZNC, did you install it?";
}

Process::~Process() {
    if (m_kill) m_proc.terminate();
    bool bFinished = m_proc.waitForFinished();
    EXPECT_TRUE(bFinished);
    if (!bFinished) return;
    if (!m_allowDie) {
        EXPECT_EQ(m_proc.exitStatus(), QProcess::NormalExit);
        if (m_allowLeak) {
            EXPECT_THAT(m_proc.exitStatus(), AnyOf(Eq(23), Eq(m_exit)));
        } else {
            EXPECT_EQ(m_proc.exitCode(), m_exit);
        }
    }
}

}  // namespace znc_inttest
