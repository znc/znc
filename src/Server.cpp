/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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

#include <znc/Server.h>

CServer::CServer(const CString& sName, unsigned short uPort,
                 const CString& sPass, bool bSSL, bool bUnixSocket)
    : m_sName(sName),
      m_uPort((uPort) ? uPort : (unsigned short)6667),
      m_sPass(sPass),
      m_bSSL(bSSL),
      m_bUnixSocket(bUnixSocket) {}

CServer::~CServer() {}

bool CServer::IsValidHostName(const CString& sHostName) {
    return (!sHostName.empty() && !sHostName.Contains(" "));
}

const CString& CServer::GetName() const { return m_sName; }
unsigned short CServer::GetPort() const { return m_uPort; }
const CString& CServer::GetPass() const { return m_sPass; }
bool CServer::IsSSL() const { return m_bSSL; }
bool CServer::IsUnixSocket() const { return m_bUnixSocket; }

CString CServer::GetString(bool bIncludePassword) const {
    CString sResult;
    if (m_bUnixSocket) {
        sResult = "unix:" + CString(m_bSSL ? "ssl:" : "") + m_sName;
    } else {
        sResult = m_sName + " " + CString(m_bSSL ? "+" : "") + CString(m_uPort);
    }
    sResult +=
        CString(bIncludePassword ? (m_sPass.empty() ? "" : " " + m_sPass) : "");
    return sResult;
}

CServer CServer::Parse(CString sLine) {
    bool bSSL = false;
    sLine.Trim();

    if (sLine.TrimPrefix("unix:")) {
        if (sLine.TrimPrefix("ssl:")) {
            bSSL = true;
        }

        CString sPath = sLine.Token(0);
        CString sPass = sLine.Token(1, true);
        return CServer(sPath, 0, sPass, bSSL, true);
    }

    CString sHost = sLine.Token(0);
    CString sPort = sLine.Token(1);

    if (sPort.TrimPrefix("+")) {
        bSSL = true;
    }

    unsigned short uPort = sPort.ToUShort();
    CString sPass = sLine.Token(2, true);

    return CServer(sHost, uPort, sPass, bSSL, false);
}

bool CServer::operator==(const CServer& o) const {
    if (m_sName != o.m_sName) return false;
    if (m_uPort != o.m_uPort) return false;
    if (m_sPass != o.m_sPass) return false;
    if (m_bSSL != o.m_bSSL) return false;
    if (m_bUnixSocket != o.m_bUnixSocket) return false;
    return true;
}

bool CServer::operator<(const CServer& o) const {
    if (m_sName < o.m_sName) return true;
    if (m_sName > o.m_sName) return false;
    if (m_uPort < o.m_uPort) return true;
    if (m_uPort > o.m_uPort) return false;
    if (m_sPass < o.m_sPass) return true;
    if (m_sPass > o.m_sPass) return false;
    if (m_bSSL < o.m_bSSL) return true;
    if (m_bSSL > o.m_bSSL) return false;
    if (m_bUnixSocket < o.m_bUnixSocket) return true;
    if (m_bUnixSocket > o.m_bUnixSocket) return false;
    return false;
}
