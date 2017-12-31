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

#ifndef ZNC_EXECSOCK_H
#define ZNC_EXECSOCK_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>
#include <signal.h>

//! @author imaginos@imaginos.net
class CExecSock : public CZNCSock {
  public:
    CExecSock() : CZNCSock(0), m_iPid(-1) {}

    int Execute(const CString& sExec) {
        int iReadFD, iWriteFD;
        m_iPid = popen2(iReadFD, iWriteFD, sExec);
        if (m_iPid != -1) {
            ConnectFD(iReadFD, iWriteFD, "0.0.0.0:0");
        }
        return (m_iPid);
    }
    void Kill(int iSignal) {
        kill(m_iPid, iSignal);
        Close();
    }
    virtual ~CExecSock() {
        close2(m_iPid, GetRSock(), GetWSock());
        SetRSock(-1);
        SetWSock(-1);
    }

    int popen2(int& iReadFD, int& iWriteFD, const CString& sCommand);
    void close2(int iPid, int iReadFD, int iWriteFD);

  private:
    int m_iPid;
};

#endif  // !ZNC_EXECSOCK_H
