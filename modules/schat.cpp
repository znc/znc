/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 * Author: imaginos <imaginos@imaginos.net>
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

/*
 * Secure chat system
 */

#define REQUIRESSL

#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>

#if !defined(OPENSSL_VERSION_NUMBER) || defined(LIBRESSL_VERSION_NUMBER) || \
    OPENSSL_VERSION_NUMBER < 0x10100007
/* SSL_SESSION was made opaque in OpenSSL 1.1.0, cipher accessor was added 2
weeks before the public release.
See openssl/openssl@e92813234318635639dba0168c7ef5568757449b. */
# define SSL_SESSION_get0_cipher(pSession) ((pSession)->cipher)
#endif

using std::pair;
using std::stringstream;
using std::map;
using std::set;
using std::vector;

class CSChat;

class CRemMarkerJob : public CTimer {
  public:
    CRemMarkerJob(CModule* pModule, unsigned int uInterval,
                  unsigned int uCycles, const CString& sLabel,
                  const CString& sDescription)
        : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

    ~CRemMarkerJob() override {}
    void SetNick(const CString& sNick) { m_sNick = sNick; }

  protected:
    void RunJob() override;
    CString m_sNick;
};

class CSChatSock : public CSocket {
  public:
    CSChatSock(CSChat* pMod, const CString& sChatNick);
    CSChatSock(CSChat* pMod, const CString& sChatNick, const CString& sHost,
               u_short iPort, int iTimeout = 60);
    ~CSChatSock() override {}

    Csock* GetSockObj(const CS_STRING& sHostname, u_short iPort) override {
        CSChatSock* p =
            new CSChatSock(m_pModule, m_sChatNick, sHostname, iPort);
        return (p);
    }

    bool ConnectionFrom(const CS_STRING& sHost, u_short iPort) override {
        Close();  // close the listener after the first connection
        return (true);
    }

    void Connected() override;
    void Timeout() override;

    const CString& GetChatNick() const { return (m_sChatNick); }

    void PutQuery(const CString& sText);

    void ReadLine(const CS_STRING& sLine) override;
    void Disconnected() override;

    void AddLine(const CString& sLine) {
        m_vBuffer.insert(m_vBuffer.begin(), sLine);
        if (m_vBuffer.size() > 200) m_vBuffer.pop_back();
    }

    void DumpBuffer() {
        if (m_vBuffer.empty()) {
            // Always show a message to the user, so he knows
            // this schat still exists.
            ReadLine("*** Reattached.");
        } else {
            // Buffer playback
            vector<CS_STRING>::reverse_iterator it = m_vBuffer.rbegin();
            for (; it != m_vBuffer.rend(); ++it) ReadLine(*it);

            m_vBuffer.clear();
        }
    }

  private:
    CSChat* m_pModule;
    CString m_sChatNick;
    VCString m_vBuffer;
};

class CSChat : public CModule {
  public:
    MODCONSTRUCTOR(CSChat) {}
    ~CSChat() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        if (sArgs.empty()) {
            sMessage = "Argument must be path to PEM file";
            return false;
        }

        m_sPemFile = CDir::CheckPathPrefix(GetSavePath(), sArgs);

        if (!CFile::Exists(m_sPemFile)) {
            sMessage = "Unable to load pem file [" + m_sPemFile + "]";
            return false;
        }

        return true;
    }

    void OnClientLogin() override {
        set<CSocket*>::const_iterator it;
        for (it = BeginSockets(); it != EndSockets(); ++it) {
            CSChatSock* p = (CSChatSock*)*it;

            if (p->GetType() == CSChatSock::LISTENER) continue;

            p->DumpBuffer();
        }
    }

    EModRet OnUserRawMessage(CMessage& msg) override {
        if (!msg.GetCommand().Equals("schat")) return CONTINUE;

        const CString sParams = msg.GetParamsColon(0);
        if (sParams.empty()) {
            PutModule("SChat User Area ...");
            OnModCommand("help");
        } else {
            OnModCommand("chat " + sParams);
        }

        return HALT;
    }

    void OnModCommand(const CString& sCommand) override {
        CString sCom = sCommand.Token(0);
        CString sArgs = sCommand.Token(1, true);

        if (sCom.Equals("chat") && !sArgs.empty()) {
            CString sNick = "(s)" + sArgs;
            set<CSocket*>::const_iterator it;
            for (it = BeginSockets(); it != EndSockets(); ++it) {
                CSChatSock* pSock = (CSChatSock*)*it;

                if (pSock->GetChatNick().Equals(sNick)) {
                    PutModule("Already Connected to [" + sArgs + "]");
                    return;
                }
            }

            CSChatSock* pSock = new CSChatSock(this, sNick);
            pSock->SetCipher("HIGH");
            pSock->SetPemLocation(m_sPemFile);

            u_short iPort = GetManager()->ListenRand(
                pSock->GetSockName() + "::LISTENER", GetUser()->GetLocalDCCIP(),
                true, SOMAXCONN, pSock, 60);

            if (iPort == 0) {
                PutModule("Failed to start chat!");
                return;
            }

            stringstream s;
            s << "PRIVMSG " << sArgs << " :\001";
            s << "DCC SCHAT chat ";
            s << CUtils::GetLongIP(GetUser()->GetLocalDCCIP());
            s << " " << iPort << "\001";

            PutIRC(s.str());

        } else if (sCom.Equals("list")) {
            CTable Table;
            Table.AddColumn("Nick");
            Table.AddColumn("Created");
            Table.AddColumn("Host");
            Table.AddColumn("Port");
            Table.AddColumn("Status");
            Table.AddColumn("Cipher");

            set<CSocket*>::const_iterator it;
            for (it = BeginSockets(); it != EndSockets(); ++it) {
                Table.AddRow();

                CSChatSock* pSock = (CSChatSock*)*it;
                Table.SetCell("Nick", pSock->GetChatNick());
                unsigned long long iStartTime = pSock->GetStartTime();
                time_t iTime = iStartTime / 1000;
                char* pTime = ctime(&iTime);
                if (pTime) {
                    CString sTime = pTime;
                    sTime.Trim();
                    Table.SetCell("Created", sTime);
                }

                if (pSock->GetType() != CSChatSock::LISTENER) {
                    Table.SetCell("Status", "Established");
                    Table.SetCell("Host", pSock->GetRemoteIP());
                    Table.SetCell("Port", CString(pSock->GetRemotePort()));
                    SSL_SESSION* pSession = pSock->GetSSLSession();
                    Table.SetCell(
                        "Cipher",
                        SSL_CIPHER_get_name(
                            pSession ? SSL_SESSION_get0_cipher(pSession)
                                     : nullptr));

                } else {
                    Table.SetCell("Status", "Waiting");
                    Table.SetCell("Port", CString(pSock->GetLocalPort()));
                }
            }
            if (Table.size()) {
                PutModule(Table);
            } else
                PutModule("No SDCCs currently in session");

        } else if (sCom.Equals("close")) {
            if (!sArgs.StartsWith("(s)")) sArgs = "(s)" + sArgs;

            set<CSocket*>::const_iterator it;
            for (it = BeginSockets(); it != EndSockets(); ++it) {
                CSChatSock* pSock = (CSChatSock*)*it;

                if (sArgs.Equals(pSock->GetChatNick())) {
                    pSock->Close();
                    return;
                }
            }
            PutModule("No Such Chat [" + sArgs + "]");
        } else if (sCom.Equals("showsocks") && GetUser()->IsAdmin()) {
            CTable Table;
            Table.AddColumn("SockName");
            Table.AddColumn("Created");
            Table.AddColumn("LocalIP:Port");
            Table.AddColumn("RemoteIP:Port");
            Table.AddColumn("Type");
            Table.AddColumn("Cipher");

            set<CSocket*>::const_iterator it;
            for (it = BeginSockets(); it != EndSockets(); ++it) {
                Table.AddRow();
                Csock* pSock = *it;
                Table.SetCell("SockName", pSock->GetSockName());
                unsigned long long iStartTime = pSock->GetStartTime();
                time_t iTime = iStartTime / 1000;
                char* pTime = ctime(&iTime);
                if (pTime) {
                    CString sTime = pTime;
                    sTime.Trim();
                    Table.SetCell("Created", sTime);
                }

                if (pSock->GetType() != Csock::LISTENER) {
                    if (pSock->GetType() == Csock::OUTBOUND)
                        Table.SetCell("Type", "Outbound");
                    else
                        Table.SetCell("Type", "Inbound");
                    Table.SetCell("LocalIP:Port",
                                  pSock->GetLocalIP() + ":" +
                                      CString(pSock->GetLocalPort()));
                    Table.SetCell("RemoteIP:Port",
                                  pSock->GetRemoteIP() + ":" +
                                      CString(pSock->GetRemotePort()));
                    SSL_SESSION* pSession = pSock->GetSSLSession();
                    Table.SetCell(
                        "Cipher",
                        SSL_CIPHER_get_name(
                            pSession ? SSL_SESSION_get0_cipher(pSession)
                                     : nullptr));

                } else {
                    Table.SetCell("Type", "Listener");
                    Table.SetCell("LocalIP:Port",
                                  pSock->GetLocalIP() + ":" +
                                      CString(pSock->GetLocalPort()));
                    Table.SetCell("RemoteIP:Port", "0.0.0.0:0");
                }
            }
            if (Table.size())
                PutModule(Table);
            else
                PutModule("Error Finding Sockets");

        } else if (sCom.Equals("help")) {
            PutModule("Commands are:");
            PutModule("    help           - This text.");
            PutModule("    chat <nick>    - Chat a nick.");
            PutModule("    list           - List current chats.");
            PutModule("    close <nick>   - Close a chat to a nick.");
            PutModule("    timers         - Shows related timers.");
            if (GetUser()->IsAdmin()) {
                PutModule("    showsocks      - Shows all socket connections.");
            }
        } else if (sCom.Equals("timers"))
            ListTimers();
        else
            PutModule("Unknown command [" + sCom + "] [" + sArgs + "]");
    }

    EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
        if (sMessage.StartsWith("DCC SCHAT ")) {
            // chat ip port
            unsigned long iIP = sMessage.Token(3).ToULong();
            unsigned short iPort = sMessage.Token(4).ToUShort();

            if (iIP > 0 && iPort > 0) {
                pair<u_long, u_short> pTmp;
                CString sMask;

                pTmp.first = iIP;
                pTmp.second = iPort;
                sMask = "(s)" + Nick.GetNick() + "!" + "(s)" + Nick.GetNick() +
                        "@" + CUtils::GetIP(iIP);

                m_siiWaitingChats["(s)" + Nick.GetNick()] = pTmp;
                SendToUser(sMask, "*** Incoming DCC SCHAT, Accept ? (yes/no)");
                CRemMarkerJob* p = new CRemMarkerJob(
                    this, 60, 1, "Remove (s)" + Nick.GetNick(),
                    "Removes this nicks entry for waiting DCC.");
                p->SetNick("(s)" + Nick.GetNick());
                AddTimer(p);
                return (HALT);
            }
        }

        return (CONTINUE);
    }

    void AcceptSDCC(const CString& sNick, u_long iIP, u_short iPort) {
        CSChatSock* p =
            new CSChatSock(this, sNick, CUtils::GetIP(iIP), iPort, 60);
        GetManager()->Connect(CUtils::GetIP(iIP), iPort, p->GetSockName(), 60,
                              true, GetUser()->GetLocalDCCIP(), p);
        RemTimer("Remove " +
                 sNick);  // delete any associated timer to this nick
    }

    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        if (sTarget.Left(3) == "(s)") {
            CString sSockName = GetModName().AsUpper() + "::" + sTarget;
            CSChatSock* p = (CSChatSock*)FindSocket(sSockName);
            if (!p) {
                map<CString, pair<u_long, u_short>>::iterator it;
                it = m_siiWaitingChats.find(sTarget);

                if (it != m_siiWaitingChats.end()) {
                    if (!sMessage.Equals("yes"))
                        SendToUser(sTarget + "!" + sTarget + "@" +
                                       CUtils::GetIP(it->second.first),
                                   "Refusing to accept DCC SCHAT!");
                    else
                        AcceptSDCC(sTarget, it->second.first,
                                   it->second.second);

                    m_siiWaitingChats.erase(it);
                    return (HALT);
                }
                PutModule("No such SCHAT to [" + sTarget + "]");
            } else
                p->Write(sMessage + "\n");

            return (HALT);
        }
        return (CONTINUE);
    }

    void RemoveMarker(const CString& sNick) {
        map<CString, pair<u_long, u_short>>::iterator it =
            m_siiWaitingChats.find(sNick);
        if (it != m_siiWaitingChats.end()) m_siiWaitingChats.erase(it);
    }

    void SendToUser(const CString& sFrom, const CString& sText) {
        //:*schat!znc@znc.in PRIVMSG Jim :
        CString sSend = ":" + sFrom + " PRIVMSG " + GetNetwork()->GetCurNick() +
                        " :" + sText;
        PutUser(sSend);
    }

    bool IsAttached() { return (GetNetwork()->IsUserAttached()); }

  private:
    map<CString, pair<u_long, u_short>> m_siiWaitingChats;
    CString m_sPemFile;
};

//////////////////// methods ////////////////

CSChatSock::CSChatSock(CSChat* pMod, const CString& sChatNick) : CSocket(pMod) {
    m_pModule = pMod;
    m_sChatNick = sChatNick;
    SetSockName(pMod->GetModName().AsUpper() + "::" + m_sChatNick);
}

CSChatSock::CSChatSock(CSChat* pMod, const CString& sChatNick,
                       const CString& sHost, u_short iPort, int iTimeout)
    : CSocket(pMod, sHost, iPort, iTimeout) {
    m_pModule = pMod;
    EnableReadLine();
    m_sChatNick = sChatNick;
    SetSockName(pMod->GetModName().AsUpper() + "::" + m_sChatNick);
}

void CSChatSock::PutQuery(const CString& sText) {
    m_pModule->SendToUser(m_sChatNick + "!" + m_sChatNick + "@" + GetRemoteIP(),
                          sText);
}

void CSChatSock::ReadLine(const CS_STRING& sLine) {
    if (m_pModule) {
        CString sText = sLine;

        sText.TrimRight("\r\n");

        if (m_pModule->IsAttached())
            PutQuery(sText);
        else
            AddLine(m_pModule->GetUser()->AddTimestamp(sText));
    }
}

void CSChatSock::Disconnected() {
    if (m_pModule) PutQuery("*** Disconnected.");
}

void CSChatSock::Connected() {
    SetTimeout(0);
    if (m_pModule) PutQuery("*** Connected.");
}

void CSChatSock::Timeout() {
    if (m_pModule) {
        if (GetType() == LISTENER)
            m_pModule->PutModule("Timeout while waiting for [" + m_sChatNick +
                                 "]");
        else
            PutQuery("*** Connection Timed out.");
    }
}

void CRemMarkerJob::RunJob() {
    CSChat* p = (CSChat*)GetModule();
    p->RemoveMarker(m_sNick);

    // store buffer
}

template <>
void TModInfo<CSChat>(CModInfo& Info) {
    Info.SetWikiPage("schat");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText("Path to .pem file, if differs from main ZNC's one");
}

NETWORKMODULEDEFS(CSChat, "Secure cross platform (:P) chat system")
