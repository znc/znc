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

#include <znc/znc.h>
#include <znc/User.h>
#include <znc/FileUtils.h>

using std::set;

class CDCCMod;

class CDCCSock : public CSocket {
  public:
    CDCCSock(CDCCMod* pMod, const CString& sRemoteNick,
             const CString& sLocalFile, unsigned long uFileSize = 0,
             CFile* pFile = nullptr);
    CDCCSock(CDCCMod* pMod, const CString& sRemoteNick,
             const CString& sRemoteIP, unsigned short uRemotePort,
             const CString& sLocalFile, unsigned long uFileSize);
    ~CDCCSock() override;

    void ReadData(const char* data, size_t len) override;
    void ConnectionRefused() override;
    void SockError(int iErrno, const CString& sDescription) override;
    void Timeout() override;
    void Connected() override;
    void Disconnected() override;
    void SendPacket();
    Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
    CFile* OpenFile(bool bWrite = true);
    bool Seek(unsigned long int uPos);

    // Setters
    void SetRemoteIP(const CString& s) { m_sRemoteIP = s; }
    void SetRemoteNick(const CString& s) { m_sRemoteNick = s; }
    void SetFileName(const CString& s) { m_sFileName = s; }
    void SetFileOffset(unsigned long u) { m_uBytesSoFar = u; }
    // !Setters

    // Getters
    unsigned short GetUserPort() const { return m_uRemotePort; }
    const CString& GetRemoteNick() const { return m_sRemoteNick; }
    const CString& GetFileName() const { return m_sFileName; }
    const CString& GetLocalFile() const { return m_sLocalFile; }
    CFile* GetFile() { return m_pFile; }
    double GetProgress() const {
        return ((m_uFileSize) && (m_uBytesSoFar))
                   ? (double)(((double)m_uBytesSoFar / (double)m_uFileSize) *
                              100.0)
                   : 0;
    }
    bool IsSend() const { return m_bSend; }
    // const CString& GetRemoteIP() const { return m_sRemoteIP; }
    // !Getters
  private:
  protected:
    CString m_sRemoteNick;
    CString m_sRemoteIP;
    CString m_sFileName;
    CString m_sLocalFile;
    CString m_sSendBuf;
    unsigned short m_uRemotePort;
    unsigned long long m_uFileSize;
    unsigned long long m_uBytesSoFar;
    bool m_bSend;
    bool m_bNoDelFile;
    CFile* m_pFile;
    CDCCMod* m_pModule;
};

class CDCCMod : public CModule {
  public:
    MODCONSTRUCTOR(CDCCMod) {
        AddHelpCommand();
        AddCommand("Send", t_d("<nick> <file>"),
                   t_d("Send a file from ZNC to someone"),
                   [=](const CString& sLine) { SendCommand(sLine); });
        AddCommand("Get", t_d("<file>"),
                   t_d("Send a file from ZNC to your client"),
                   [=](const CString& sLine) { GetCommand(sLine); });
        AddCommand("ListTransfers", "", t_d("List current transfers"),
                   [=](const CString& sLine) { ListTransfersCommand(sLine); });
    }

    ~CDCCMod() override {}

#ifndef MOD_DCC_ALLOW_EVERYONE
    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        if (!GetUser()->IsAdmin()) {
            sMessage = t_s("You must be admin to use the DCC module");
            return false;
        }

        return true;
    }
#endif

    bool SendFile(const CString& sRemoteNick, const CString& sFileName) {
        CString sFullPath = CDir::ChangeDir(GetSavePath(), sFileName,
                                            CZNC::Get().GetHomePath());
        CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath);

        CFile* pFile = pSock->OpenFile(false);

        if (!pFile) {
            delete pSock;
            return false;
        }

        CString sLocalDCCIP = GetUser()->GetLocalDCCIP();
        unsigned short uPort = CZNC::Get().GetManager().ListenRand(
            "DCC::LISTEN::" + sRemoteNick, sLocalDCCIP, false, SOMAXCONN, pSock,
            120);

        if (GetUser()->GetNick().Equals(sRemoteNick)) {
            PutUser(":*dcc!znc@znc.in PRIVMSG " + sRemoteNick +
                    " :\001DCC SEND " + pFile->GetShortName() + " " +
                    CString(CUtils::GetLongIP(sLocalDCCIP)) + " " +
                    CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
        } else {
            PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " +
                   pFile->GetShortName() + " " +
                   CString(CUtils::GetLongIP(sLocalDCCIP)) + " " +
                   CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
        }

        PutModule(t_f("Attempting to send [{1}] to [{2}].")(
            pFile->GetShortName(), sRemoteNick));
        return true;
    }

    bool GetFile(const CString& sRemoteNick, const CString& sRemoteIP,
                 unsigned short uRemotePort, const CString& sFileName,
                 unsigned long uFileSize) {
        if (CFile::Exists(sFileName)) {
            PutModule(t_f("Receiving [{1}] from [{2}]: File already exists.")(
                sFileName, sRemoteNick));
            return false;
        }

        CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sRemoteIP,
                                       uRemotePort, sFileName, uFileSize);

        if (!pSock->OpenFile()) {
            delete pSock;
            return false;
        }

        CZNC::Get().GetManager().Connect(sRemoteIP, uRemotePort,
                                         "DCC::GET::" + sRemoteNick, 60, false,
                                         GetUser()->GetLocalDCCIP(), pSock);

        PutModule(
            t_f("Attempting to connect to [{1} {2}] in order to download [{3}] "
                "from [{4}].")(sRemoteIP, uRemotePort, sFileName, sRemoteNick));
        return true;
    }

    void SendCommand(const CString& sLine) {
        CString sToNick = sLine.Token(1);
        CString sFile = sLine.Token(2);
        CString sAllowedPath = GetSavePath();
        CString sAbsolutePath;

        if ((sToNick.empty()) || (sFile.empty())) {
            PutModule(t_s("Usage: Send <nick> <file>"));
            return;
        }

        sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

        if (sAbsolutePath.empty()) {
            PutStatus(t_s("Illegal path."));
            return;
        }

        SendFile(sToNick, sFile);
    }

    void GetCommand(const CString& sLine) {
        CString sFile = sLine.Token(1);
        CString sAllowedPath = GetSavePath();
        CString sAbsolutePath;

        if (sFile.empty()) {
            PutModule(t_s("Usage: Get <file>"));
            return;
        }

        sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

        if (sAbsolutePath.empty()) {
            PutModule(t_s("Illegal path."));
            return;
        }

        SendFile(GetUser()->GetNick(), sFile);
    }

    void ListTransfersCommand(const CString& sLine) {
        CTable Table;
        Table.AddColumn(t_s("Type", "list"));
        Table.AddColumn(t_s("State", "list"));
        Table.AddColumn(t_s("Speed", "list"));
        Table.AddColumn(t_s("Nick", "list"));
        Table.AddColumn(t_s("IP", "list"));
        Table.AddColumn(t_s("File", "list"));

        set<CSocket*>::const_iterator it;
        for (it = BeginSockets(); it != EndSockets(); ++it) {
            CDCCSock* pSock = (CDCCSock*)*it;

            Table.AddRow();
            Table.SetCell(t_s("Nick", "list"), pSock->GetRemoteNick());
            Table.SetCell(t_s("IP", "list"), pSock->GetRemoteIP());
            Table.SetCell(t_s("File", "list"), pSock->GetFileName());

            if (pSock->IsSend()) {
                Table.SetCell(t_s("Type", "list"), t_s("Sending", "list-type"));
            } else {
                Table.SetCell(t_s("Type", "list"), t_s("Getting", "list-type"));
            }

            if (pSock->GetType() == Csock::LISTENER) {
                Table.SetCell(t_s("State", "list"),
                              t_s("Waiting", "list-state"));
            } else {
                Table.SetCell(t_s("State", "list"),
                              CString::ToPercent(pSock->GetProgress()));
                Table.SetCell(t_s("Speed", "list"),
                              t_f("{1} KiB/s")(static_cast<int>(
                                  pSock->GetAvgRead() / 1024.0)));
            }
        }

        if (PutModule(Table) == 0) {
            PutModule(t_s("You have no active DCC transfers."));
        }
    }

    void OnModCTCP(const CString& sMessage) override {
        if (sMessage.StartsWith("DCC RESUME ")) {
            CString sFile = sMessage.Token(2);
            unsigned short uResumePort = sMessage.Token(3).ToUShort();
            unsigned long uResumeSize = sMessage.Token(4).ToULong();

            set<CSocket*>::const_iterator it;
            for (it = BeginSockets(); it != EndSockets(); ++it) {
                CDCCSock* pSock = (CDCCSock*)*it;

                if (pSock->GetLocalPort() == uResumePort) {
                    if (pSock->Seek(uResumeSize)) {
                        PutModule(
                            t_f("Attempting to resume send from position {1} "
                                "of file [{2}] for [{3}]")(
                                uResumeSize, pSock->GetFileName(),
                                pSock->GetRemoteNick()));
                        PutUser(":*dcc!znc@znc.in PRIVMSG " +
                                GetUser()->GetNick() + " :\001DCC ACCEPT " +
                                sFile + " " + CString(uResumePort) + " " +
                                CString(uResumeSize) + "\001");
                    } else {
                        PutModule(t_f(
                            "Couldn't resume file [{1}] for [{2}]: not sending "
                            "anything.")(sFile, GetUser()->GetNick()));
                    }
                }
            }
        } else if (sMessage.StartsWith("DCC SEND ")) {
            CString sLocalFile =
                CDir::CheckPathPrefix(GetSavePath(), sMessage.Token(2));
            if (sLocalFile.empty()) {
                PutModule(t_f("Bad DCC file: {1}")(sMessage.Token(2)));
            }
            unsigned long uLongIP = sMessage.Token(3).ToULong();
            unsigned short uPort = sMessage.Token(4).ToUShort();
            unsigned long uFileSize = sMessage.Token(5).ToULong();
            GetFile(GetClient()->GetNick(), CUtils::GetIP(uLongIP), uPort,
                    sLocalFile, uFileSize);
        }
    }
};

CDCCSock::CDCCSock(CDCCMod* pMod, const CString& sRemoteNick,
                   const CString& sLocalFile, unsigned long uFileSize,
                   CFile* pFile)
    : CSocket(pMod) {
    m_sRemoteNick = sRemoteNick;
    m_uFileSize = uFileSize;
    m_uRemotePort = 0;
    m_uBytesSoFar = 0;
    m_pModule = pMod;
    m_pFile = pFile;
    m_sLocalFile = sLocalFile;
    m_bSend = true;
    m_bNoDelFile = false;
    SetMaxBufferThreshold(0);
}

CDCCSock::CDCCSock(CDCCMod* pMod, const CString& sRemoteNick,
                   const CString& sRemoteIP, unsigned short uRemotePort,
                   const CString& sLocalFile, unsigned long uFileSize)
    : CSocket(pMod) {
    m_sRemoteNick = sRemoteNick;
    m_sRemoteIP = sRemoteIP;
    m_uRemotePort = uRemotePort;
    m_uFileSize = uFileSize;
    m_uBytesSoFar = 0;
    m_pModule = pMod;
    m_pFile = nullptr;
    m_sLocalFile = sLocalFile;
    m_bSend = false;
    m_bNoDelFile = false;
    SetMaxBufferThreshold(0);
}

CDCCSock::~CDCCSock() {
    if ((m_pFile) && (!m_bNoDelFile)) {
        m_pFile->Close();
        delete m_pFile;
    }
}

void CDCCSock::ReadData(const char* data, size_t len) {
    if (!m_pFile) {
        DEBUG("File not open! closing get.");
        if (m_bSend) {
            m_pModule->PutModule(t_f("Sending [{1}] to [{2}]: File not open!")(
                m_sFileName, m_sRemoteNick));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: File not open!")(
                    m_sFileName, m_sRemoteNick));
        }
        Close();
        return;
    }

    // DCC specs says the receiving end sends the number of bytes it
    // received so far as a 4 byte integer in network byte order, so we need
    // uint32_t to do the job portably. This also means that the maximum
    // file that we can transfer is 4 GiB big (see OpenFile()).
    if (m_bSend) {
        m_sSendBuf.append(data, len);

        while (m_sSendBuf.size() >= 4) {
            uint32_t iRemoteSoFar;
            memcpy(&iRemoteSoFar, m_sSendBuf.data(), sizeof(iRemoteSoFar));
            iRemoteSoFar = ntohl(iRemoteSoFar);

            if ((iRemoteSoFar + 65536) >= m_uBytesSoFar) {
                SendPacket();
            }

            m_sSendBuf.erase(0, 4);
        }
    } else {
        m_pFile->Write(data, len);
        m_uBytesSoFar += len;
        uint32_t uSoFar = htonl((uint32_t)m_uBytesSoFar);
        Write((char*)&uSoFar, sizeof(uSoFar));

        if (m_uBytesSoFar >= m_uFileSize) {
            Close();
        }
    }
}

void CDCCSock::ConnectionRefused() {
    DEBUG(GetSockName() << " == ConnectionRefused()");
    if (m_bSend) {
        m_pModule->PutModule(t_f("Sending [{1}] to [{2}]: Connection refused.")(
            m_sFileName, m_sRemoteNick));
    } else {
        m_pModule->PutModule(
            t_f("Receiving [{1}] from [{2}]: Connection refused.")(
                m_sFileName, m_sRemoteNick));
    }
}

void CDCCSock::Timeout() {
    DEBUG(GetSockName() << " == Timeout()");
    if (m_bSend) {
        m_pModule->PutModule(t_f("Sending [{1}] to [{2}]: Timeout.")(
            m_sFileName, m_sRemoteNick));
    } else {
        m_pModule->PutModule(
            t_f("Receiving [{1}] from [{2}]: Timeout.")(
                m_sFileName, m_sRemoteNick));
    }
}

void CDCCSock::SockError(int iErrno, const CString& sDescription) {
    DEBUG(GetSockName() << " == SockError(" << iErrno << ", " << sDescription
                        << ")");
    if (m_bSend) {
        m_pModule->PutModule(
            t_f("Sending [{1}] to [{2}]: Socket error {3}: {4}")(
                m_sFileName, m_sRemoteNick, iErrno, sDescription));
    } else {
        m_pModule->PutModule(
            t_f("Receiving [{1}] from [{2}]: Socket error {3}: {4}")(
                m_sFileName, m_sRemoteNick, iErrno, sDescription));
    }
}

void CDCCSock::Connected() {
    DEBUG(GetSockName() << " == Connected(" << GetRemoteIP() << ")");
    if (m_bSend) {
        m_pModule->PutModule(t_f("Sending [{1}] to [{2}]: Transfer started.")(
            m_sFileName, m_sRemoteNick));
    } else {
        m_pModule->PutModule(
            t_f("Receiving [{1}] from [{2}]: Transfer started.")(
                m_sFileName, m_sRemoteNick));
    }

    if (m_bSend) {
        SendPacket();
    }

    SetTimeout(120);
}

void CDCCSock::Disconnected() {
    const CString sStart = ((m_bSend) ? "DCC -> [" : "DCC <- [") +
                           m_sRemoteNick + "][" + m_sFileName + "] - ";

    DEBUG(GetSockName() << " == Disconnected()");

    if (m_uBytesSoFar > m_uFileSize) {
        if (m_bSend) {
            m_pModule->PutModule(t_f("Sending [{1}] to [{2}]: Too much data!")(
                m_sFileName, m_sRemoteNick));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: Too much data!")(
                    m_sFileName, m_sRemoteNick));
        }
    } else if (m_uBytesSoFar == m_uFileSize) {
        if (m_bSend) {
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}] completed at {3} KiB/s")(
                    m_sFileName, m_sRemoteNick,
                    static_cast<int>(GetAvgWrite() / 1024.0)));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}] completed at {3} KiB/s")(
                    m_sFileName, m_sRemoteNick,
                    static_cast<int>(GetAvgRead() / 1024.0)));
        }
    } else {
        m_pModule->PutModule(sStart + "Incomplete!");
    }
}

void CDCCSock::SendPacket() {
    if (!m_pFile) {
        if (m_bSend) {
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: File closed prematurely.")(
                    m_sFileName, m_sRemoteNick));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: File closed prematurely.")(
                    m_sFileName, m_sRemoteNick));
        }

        Close();
        return;
    }

    if (GetInternalWriteBuffer().size() > 1024 * 1024) {
        // There is still enough data to be written, don't add more
        // stuff to that buffer.
        DEBUG("SendPacket(): Skipping send, buffer still full enough ["
              << GetInternalWriteBuffer().size() << "][" << m_sRemoteNick
              << "][" << m_sFileName << "]");
        return;
    }

    char szBuf[4096];
    ssize_t iLen = m_pFile->Read(szBuf, 4096);

    if (iLen < 0) {
        if (m_bSend) {
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: Error reading from file.")(
                    m_sFileName, m_sRemoteNick));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: Error reading from file.")(
                    m_sFileName, m_sRemoteNick));
        }

        Close();
        return;
    }

    if (iLen > 0) {
        Write(szBuf, iLen);
        m_uBytesSoFar += iLen;
    }
}

Csock* CDCCSock::GetSockObj(const CString& sHost, unsigned short uPort) {
    Close();

    CDCCSock* pSock = new CDCCSock(m_pModule, m_sRemoteNick, m_sLocalFile,
                                   m_uFileSize, m_pFile);
    pSock->SetSockName("DCC::SEND::" + m_sRemoteNick);
    pSock->SetTimeout(120);
    pSock->SetFileName(m_sFileName);
    pSock->SetFileOffset(m_uBytesSoFar);
    m_bNoDelFile = true;

    return pSock;
}

CFile* CDCCSock::OpenFile(bool bWrite) {
    if ((m_pFile) || (m_sLocalFile.empty())) {
        if (m_bSend) {
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: Unable to open file.")(
                    m_sFileName, m_sRemoteNick));
        } else {
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: Unable to open file.")(
                    m_sFileName, m_sRemoteNick));
        }
        return nullptr;
    }

    m_pFile = new CFile(m_sLocalFile);

    if (bWrite) {
        if (m_pFile->Exists()) {
            delete m_pFile;
            m_pFile = nullptr;
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: File already exists.")(
                    m_sFileName, m_sRemoteNick));
            return nullptr;
        }

        if (!m_pFile->Open(O_WRONLY | O_TRUNC | O_CREAT)) {
            delete m_pFile;
            m_pFile = nullptr;
            m_pModule->PutModule(
                t_f("Receiving [{1}] from [{2}]: Could not open file.")(
                    m_sFileName, m_sRemoteNick));
            return nullptr;
        }
    } else {
        if (!m_pFile->IsReg()) {
            delete m_pFile;
            m_pFile = nullptr;
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: Not a file.")(
                    m_sFileName, m_sRemoteNick));
            return nullptr;
        }

        if (!m_pFile->Open()) {
            delete m_pFile;
            m_pFile = nullptr;
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: Could not open file.")(
                    m_sFileName, m_sRemoteNick));
            return nullptr;
        }

        // The DCC specs only allow file transfers with files smaller
        // than 4GiB (see ReadData()).
        unsigned long long uFileSize = m_pFile->GetSize();
        if (uFileSize > (unsigned long long)0xffffffffULL) {
            delete m_pFile;
            m_pFile = nullptr;
            m_pModule->PutModule(
                t_f("Sending [{1}] to [{2}]: File too large (>4 GiB).")(
                    m_sFileName, m_sRemoteNick));
            return nullptr;
        }

        m_uFileSize = uFileSize;
    }

    m_sFileName = m_pFile->GetShortName();

    return m_pFile;
}

bool CDCCSock::Seek(unsigned long int uPos) {
    if (m_pFile) {
        if (m_pFile->Seek(uPos)) {
            m_uBytesSoFar = uPos;
            return true;
        }
    }

    return false;
}

template <>
void TModInfo<CDCCMod>(CModInfo& Info) {
    Info.SetWikiPage("dcc");
}

USERMODULEDEFS(CDCCMod,
               t_s("This module allows you to transfer files to and from ZNC"))
