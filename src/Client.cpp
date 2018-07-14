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

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Query.h>

using std::set;
using std::map;
using std::vector;

#define CALLMOD(MOD, CLIENT, USER, NETWORK, FUNC)                             \
    {                                                                         \
        CModule* pModule = nullptr;                                           \
        if (NETWORK && (pModule = (NETWORK)->GetModules().FindModule(MOD))) { \
            try {                                                             \
                CClient* pOldClient = pModule->GetClient();                   \
                pModule->SetClient(CLIENT);                                   \
                pModule->FUNC;                                                \
                pModule->SetClient(pOldClient);                               \
            } catch (const CModule::EModException& e) {                       \
                if (e == CModule::UNLOAD) {                                   \
                    (NETWORK)->GetModules().UnloadModule(MOD);                \
                }                                                             \
            }                                                                 \
        } else if ((pModule = (USER)->GetModules().FindModule(MOD))) {        \
            try {                                                             \
                CClient* pOldClient = pModule->GetClient();                   \
                CIRCNetwork* pOldNetwork = pModule->GetNetwork();             \
                pModule->SetClient(CLIENT);                                   \
                pModule->SetNetwork(NETWORK);                                 \
                pModule->FUNC;                                                \
                pModule->SetClient(pOldClient);                               \
                pModule->SetNetwork(pOldNetwork);                             \
            } catch (const CModule::EModException& e) {                       \
                if (e == CModule::UNLOAD) {                                   \
                    (USER)->GetModules().UnloadModule(MOD);                   \
                }                                                             \
            }                                                                 \
        } else if ((pModule = CZNC::Get().GetModules().FindModule(MOD))) {    \
            try {                                                             \
                CClient* pOldClient = pModule->GetClient();                   \
                CIRCNetwork* pOldNetwork = pModule->GetNetwork();             \
                CUser* pOldUser = pModule->GetUser();                         \
                pModule->SetClient(CLIENT);                                   \
                pModule->SetNetwork(NETWORK);                                 \
                pModule->SetUser(USER);                                       \
                pModule->FUNC;                                                \
                pModule->SetClient(pOldClient);                               \
                pModule->SetNetwork(pOldNetwork);                             \
                pModule->SetUser(pOldUser);                                   \
            } catch (const CModule::EModException& e) {                       \
                if (e == CModule::UNLOAD) {                                   \
                    CZNC::Get().GetModules().UnloadModule(MOD);               \
                }                                                             \
            }                                                                 \
        } else {                                                              \
            PutStatus(t_f("No such module {1}")(MOD));                        \
        }                                                                     \
    }

CClient::~CClient() {
    if (m_spAuth) {
        CClientAuth* pAuth = (CClientAuth*)&(*m_spAuth);
        pAuth->Invalidate();
    }
    if (m_pUser != nullptr) {
        m_pUser->AddBytesRead(GetBytesRead());
        m_pUser->AddBytesWritten(GetBytesWritten());
    }
}

void CClient::SendRequiredPasswordNotice() {
    PutClient(":irc.znc.in 464 " + GetNick() + " :Password required");
    PutClient(
        ":irc.znc.in NOTICE " + GetNick() + " :*** "
        "You need to send your password. "
        "Configure your client to send a server password.");
    PutClient(
        ":irc.znc.in NOTICE " + GetNick() + " :*** "
        "To connect now, you can use /quote PASS <username>:<password>, "
        "or /quote PASS <username>/<network>:<password> to connect to a "
        "specific network.");
}

void CClient::ReadLine(const CString& sData) {
    CLanguageScope user_lang(GetUser() ? GetUser()->GetLanguage() : "");
    CString sLine = sData;

    sLine.Replace("\n", "");
    sLine.Replace("\r", "");

    DEBUG("(" << GetFullName() << ") CLI -> ZNC ["
        << CDebug::Filter(sLine) << "]");

    bool bReturn = false;
    if (IsAttached()) {
        NETWORKMODULECALL(OnUserRaw(sLine), m_pUser, m_pNetwork, this,
                          &bReturn);
    } else {
        GLOBALMODULECALL(OnUnknownUserRaw(this, sLine), &bReturn);
    }
    if (bReturn) return;

    CMessage Message(sLine);
    Message.SetClient(this);

    if (IsAttached()) {
        NETWORKMODULECALL(OnUserRawMessage(Message), m_pUser, m_pNetwork, this,
                          &bReturn);
    } else {
        GLOBALMODULECALL(OnUnknownUserRawMessage(Message), &bReturn);
    }
    if (bReturn) return;

    CString sCommand = Message.GetCommand();

    if (!IsAttached()) {
        // The following commands happen before authentication with ZNC
        if (sCommand.Equals("PASS")) {
            m_bGotPass = true;

            CString sAuthLine = Message.GetParam(0);
            ParsePass(sAuthLine);

            AuthUser();
            // Don't forward this msg.  ZNC has already registered us.
            return;
        } else if (sCommand.Equals("NICK")) {
            CString sNick = Message.GetParam(0);

            m_sNick = sNick;
            m_bGotNick = true;

            AuthUser();
            // Don't forward this msg.  ZNC will handle nick changes until auth
            // is complete
            return;
        } else if (sCommand.Equals("USER")) {
            CString sAuthLine = Message.GetParam(0);

            if (m_sUser.empty() && !sAuthLine.empty()) {
                ParseUser(sAuthLine);
            }

            m_bGotUser = true;
            if (m_bGotPass) {
                AuthUser();
            } else if (!m_bInCap) {
                SendRequiredPasswordNotice();
            }

            // Don't forward this msg.  ZNC has already registered us.
            return;
        }
    }

    if (Message.GetType() == CMessage::Type::Capability) {
        HandleCap(Message);

        // Don't let the client talk to the server directly about CAP,
        // we don't want anything enabled that ZNC does not support.
        return;
    }

    if (!m_pUser) {
        // Only CAP, NICK, USER and PASS are allowed before login
        return;
    }

    switch (Message.GetType()) {
        case CMessage::Type::Action:
            bReturn = OnActionMessage(Message);
            break;
        case CMessage::Type::CTCP:
            bReturn = OnCTCPMessage(Message);
            break;
        case CMessage::Type::Join:
            bReturn = OnJoinMessage(Message);
            break;
        case CMessage::Type::Mode:
            bReturn = OnModeMessage(Message);
            break;
        case CMessage::Type::Notice:
            bReturn = OnNoticeMessage(Message);
            break;
        case CMessage::Type::Part:
            bReturn = OnPartMessage(Message);
            break;
        case CMessage::Type::Ping:
            bReturn = OnPingMessage(Message);
            break;
        case CMessage::Type::Pong:
            bReturn = OnPongMessage(Message);
            break;
        case CMessage::Type::Quit:
            bReturn = OnQuitMessage(Message);
            break;
        case CMessage::Type::Text:
            bReturn = OnTextMessage(Message);
            break;
        case CMessage::Type::Topic:
            bReturn = OnTopicMessage(Message);
            break;
        default:
            bReturn = OnOtherMessage(Message);
            break;
    }

    if (bReturn) return;

    PutIRC(Message.ToString(CMessage::ExcludePrefix | CMessage::ExcludeTags));
}

void CClient::SetNick(const CString& s) { m_sNick = s; }

void CClient::SetNetwork(CIRCNetwork* pNetwork, bool bDisconnect,
                         bool bReconnect) {
    if (m_pNetwork) {
        m_pNetwork->ClientDisconnected(this);

        if (bDisconnect) {
            ClearServerDependentCaps();
            // Tell the client they are no longer in these channels.
            const vector<CChan*>& vChans = m_pNetwork->GetChans();
            for (const CChan* pChan : vChans) {
                if (!(pChan->IsDetached())) {
                    PutClient(":" + m_pNetwork->GetIRCNick().GetNickMask() +
                              " PART " + pChan->GetName());
                }
            }
        }
    } else if (m_pUser) {
        m_pUser->UserDisconnected(this);
    }

    m_pNetwork = pNetwork;

    if (bReconnect) {
        if (m_pNetwork) {
            m_pNetwork->ClientConnected(this);
        } else if (m_pUser) {
            m_pUser->UserConnected(this);
        }
    }
}

const vector<CClient*>& CClient::GetClients() const {
    if (m_pNetwork) {
        return m_pNetwork->GetClients();
    }

    return m_pUser->GetUserClients();
}

const CIRCSock* CClient::GetIRCSock() const {
    if (m_pNetwork) {
        return m_pNetwork->GetIRCSock();
    }

    return nullptr;
}

CIRCSock* CClient::GetIRCSock() {
    if (m_pNetwork) {
        return m_pNetwork->GetIRCSock();
    }

    return nullptr;
}

void CClient::StatusCTCP(const CString& sLine) {
    CString sCommand = sLine.Token(0);

    if (sCommand.Equals("PING")) {
        PutStatusNotice("\001PING " + sLine.Token(1, true) + "\001");
    } else if (sCommand.Equals("VERSION")) {
        PutStatusNotice("\001VERSION " + CZNC::GetTag() + "\001");
    }
}

bool CClient::SendMotd() {
    const VCString& vsMotd = CZNC::Get().GetMotd();

    if (!vsMotd.size()) {
        return false;
    }

    for (const CString& sLine : vsMotd) {
        if (m_pNetwork) {
            PutStatusNotice(m_pNetwork->ExpandString(sLine));
        } else {
            PutStatusNotice(m_pUser->ExpandString(sLine));
        }
    }

    return true;
}

void CClient::AuthUser() {
    if (!m_bGotNick || !m_bGotUser || !m_bGotPass || m_bInCap || IsAttached())
        return;

    m_spAuth = std::make_shared<CClientAuth>(this, m_sUser, m_sPass);

    CZNC::Get().AuthUser(m_spAuth);
}

CClientAuth::CClientAuth(CClient* pClient, const CString& sUsername,
                         const CString& sPassword)
    : CAuthBase(sUsername, sPassword, pClient), m_pClient(pClient) {}

void CClientAuth::RefusedLogin(const CString& sReason) {
    if (m_pClient) {
        m_pClient->RefuseLogin(sReason);
    }
}

CString CAuthBase::GetRemoteIP() const {
    if (m_pSock) return m_pSock->GetRemoteIP();
    return "";
}

void CAuthBase::Invalidate() { m_pSock = nullptr; }

void CAuthBase::AcceptLogin(CUser& User) {
    if (m_pSock) {
        AcceptedLogin(User);
        Invalidate();
    }
}

void CAuthBase::RefuseLogin(const CString& sReason) {
    if (!m_pSock) return;

    CUser* pUser = CZNC::Get().FindUser(GetUsername());

    // If the username is valid, notify that user that someone tried to
    // login. Use sReason because there are other reasons than "wrong
    // password" for a login to be rejected (e.g. fail2ban).
    if (pUser) {
        pUser->PutStatusNotice(t_f(
            "A client from {1} attempted to login as you, but was rejected: "
            "{2}")(GetRemoteIP(), sReason));
    }

    GLOBALMODULECALL(OnFailedLogin(GetUsername(), GetRemoteIP()), NOTHING);
    RefusedLogin(sReason);
    Invalidate();
}

void CClient::RefuseLogin(const CString& sReason) {
    PutStatus("Bad username and/or password.");
    PutClient(":irc.znc.in 464 " + GetNick() + " :" + sReason);
    Close(Csock::CLT_AFTERWRITE);
}

void CClientAuth::AcceptedLogin(CUser& User) {
    if (m_pClient) {
        m_pClient->AcceptLogin(User);
    }
}

void CClient::AcceptLogin(CUser& User) {
    m_sPass = "";
    m_pUser = &User;

    // Set our proper timeout and set back our proper timeout mode
    // (constructor set a different timeout and mode)
    SetTimeout(User.GetNoTrafficTimeout(), TMO_READ);

    SetSockName("USR::" + m_pUser->GetUserName());
    SetEncoding(m_pUser->GetClientEncoding());

    if (!m_sNetwork.empty()) {
        m_pNetwork = m_pUser->FindNetwork(m_sNetwork);
        if (!m_pNetwork) {
            PutStatus(t_f("Network {1} doesn't exist.")(m_sNetwork));
        }
    } else if (!m_pUser->GetNetworks().empty()) {
        // If a user didn't supply a network, and they have a network called
        // "default" then automatically use this network.
        m_pNetwork = m_pUser->FindNetwork("default");
        // If no "default" network, try "user" network. It's for compatibility
        // with early network stuff in ZNC, which converted old configs to
        // "user" network.
        if (!m_pNetwork) m_pNetwork = m_pUser->FindNetwork("user");
        // Otherwise, just try any network of the user.
        if (!m_pNetwork) m_pNetwork = *m_pUser->GetNetworks().begin();
        if (m_pNetwork && m_pUser->GetNetworks().size() > 1) {
            PutStatusNotice(
                t_s("You have several networks configured, but no network was "
                    "specified for the connection."));
            PutStatusNotice(
                t_f("Selecting network {1}. To see list of all configured "
                    "networks, use /znc ListNetworks")(m_pNetwork->GetName()));
            PutStatusNotice(t_f(
                "If you want to choose another network, use /znc JumpNetwork "
                "<network>, or connect to ZNC with username {1}/<network> "
                "(instead of just {1})")(m_pUser->GetUserName()));
        }
    } else {
        PutStatusNotice(
            t_s("You have no networks configured. Use /znc AddNetwork "
                "<network> to add one."));
    }

    SetNetwork(m_pNetwork, false);

    SendMotd();

    NETWORKMODULECALL(OnClientLogin(), m_pUser, m_pNetwork, this, NOTHING);
}

void CClient::Timeout() { PutClient("ERROR :" + t_s("Closing link: Timeout")); }

void CClient::Connected() { DEBUG(GetSockName() << " == Connected();"); }

void CClient::ConnectionRefused() {
    DEBUG(GetSockName() << " == ConnectionRefused()");
}

void CClient::Disconnected() {
    DEBUG(GetSockName() << " == Disconnected()");
    CIRCNetwork* pNetwork = m_pNetwork;
    SetNetwork(nullptr, false, false);

    if (m_pUser) {
        NETWORKMODULECALL(OnClientDisconnect(), m_pUser, pNetwork, this,
                          NOTHING);
    }
}

void CClient::ReachedMaxBuffer() {
    DEBUG(GetSockName() << " == ReachedMaxBuffer()");
    if (IsAttached()) {
        PutClient("ERROR :" + t_s("Closing link: Too long raw line"));
    }
    Close();
}

void CClient::BouncedOff() {
    PutStatusNotice(
        t_s("You are being disconnected because another user just "
            "authenticated as you."));
    Close(Csock::CLT_AFTERWRITE);
}

void CClient::PutIRC(const CString& sLine) {
    if (m_pNetwork) {
        m_pNetwork->PutIRC(sLine);
    }
}

CString CClient::GetFullName() const {
    if (!m_pUser) return GetRemoteIP();
    CString sFullName = m_pUser->GetUserName();
    if (!m_sIdentifier.empty()) sFullName += "@" + m_sIdentifier;
    if (m_pNetwork) sFullName += "/" + m_pNetwork->GetName();
    return sFullName;
}

void CClient::PutClient(const CString& sLine) {
    PutClient(CMessage(sLine));
}

bool CClient::PutClient(const CMessage& Message) {
    if (!m_bAwayNotify && Message.GetType() == CMessage::Type::Away) {
        return false;
    } else if (!m_bAccountNotify &&
               Message.GetType() == CMessage::Type::Account) {
        return false;
    }

    CMessage Msg(Message);

    const CIRCSock* pIRCSock = GetIRCSock();
    if (pIRCSock) {
        if (Msg.GetType() == CMessage::Type::Numeric) {
            unsigned int uCode = Msg.As<CNumericMessage>().GetCode();

            if (uCode == 352) {  // RPL_WHOREPLY
                if (!m_bNamesx && pIRCSock->HasNamesx()) {
                    // The server has NAMESX, but the client doesn't, so we need
                    // to remove extra prefixes
                    CString sNick = Msg.GetParam(6);
                    if (sNick.size() > 1 && pIRCSock->IsPermChar(sNick[1])) {
                        CString sNewNick = sNick;
                        size_t pos =
                            sNick.find_first_not_of(pIRCSock->GetPerms());
                        if (pos >= 2 && pos != CString::npos) {
                            sNewNick = sNick[0] + sNick.substr(pos);
                        }
                        Msg.SetParam(6, sNewNick);
                    }
                }
            } else if (uCode == 353) {  // RPL_NAMES
                if ((!m_bNamesx && pIRCSock->HasNamesx()) ||
                    (!m_bUHNames && pIRCSock->HasUHNames())) {
                    // The server has either UHNAMES or NAMESX, but the client
                    // is missing either or both
                    CString sNicks = Msg.GetParam(3);
                    VCString vsNicks;
                    sNicks.Split(" ", vsNicks, false);

                    for (CString& sNick : vsNicks) {
                        if (sNick.empty()) break;

                        if (!m_bNamesx && pIRCSock->HasNamesx() &&
                            pIRCSock->IsPermChar(sNick[0])) {
                            // The server has NAMESX, but the client doesn't, so
                            // we just use the first perm char
                            size_t pos =
                                sNick.find_first_not_of(pIRCSock->GetPerms());
                            if (pos >= 2 && pos != CString::npos) {
                                sNick = sNick[0] + sNick.substr(pos);
                            }
                        }

                        if (!m_bUHNames && pIRCSock->HasUHNames()) {
                            // The server has UHNAMES, but the client doesn't,
                            // so we strip away ident and host
                            sNick = sNick.Token(0, false, "!");
                        }
                    }

                    Msg.SetParam(
                        3, CString(" ").Join(vsNicks.begin(), vsNicks.end()));
                }
            }
        } else if (Msg.GetType() == CMessage::Type::Join) {
            if (!m_bExtendedJoin && pIRCSock->HasExtendedJoin()) {
                Msg.SetParams({Msg.As<CJoinMessage>().GetTarget()});
            }
        }
    }

    MCString mssTags;

    for (const auto& it : Msg.GetTags()) {
        if (IsTagEnabled(it.first)) {
            mssTags[it.first] = it.second;
        }
    }

    if (HasServerTime()) {
        // If the server didn't set the time tag, manually set it
        mssTags.emplace("time", CUtils::FormatServerTime(Msg.GetTime()));
    }

    Msg.SetTags(mssTags);
    Msg.SetClient(this);
    Msg.SetNetwork(m_pNetwork);

    bool bReturn = false;
    NETWORKMODULECALL(OnSendToClientMessage(Msg), m_pUser, m_pNetwork, this,
                      &bReturn);
    if (bReturn) return false;

    return PutClientRaw(Msg.ToString());
}

bool CClient::PutClientRaw(const CString& sLine) {
    CString sCopy = sLine;
    bool bReturn = false;
    NETWORKMODULECALL(OnSendToClient(sCopy, *this), m_pUser, m_pNetwork, this,
                      &bReturn);
    if (bReturn) return false;

    DEBUG("(" << GetFullName() << ") ZNC -> CLI ["
        << CDebug::Filter(sCopy) << "]");
    Write(sCopy + "\r\n");
    return true;
}

void CClient::PutStatusNotice(const CString& sLine) {
    PutModNotice("status", sLine);
}

unsigned int CClient::PutStatus(const CTable& table) {
    unsigned int idx = 0;
    CString sLine;
    while (table.GetLine(idx++, sLine)) PutStatus(sLine);
    return idx - 1;
}

void CClient::PutStatus(const CString& sLine) { PutModule("status", sLine); }

void CClient::PutModNotice(const CString& sModule, const CString& sLine) {
    if (!m_pUser) {
        return;
    }

    DEBUG("(" << GetFullName()
              << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() +
                     ((sModule.empty()) ? "status" : sModule) +
                     "!znc@znc.in NOTICE " << GetNick() << " :" << sLine
              << "]");
    Write(":" + m_pUser->GetStatusPrefix() +
          ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " +
          GetNick() + " :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
    if (!m_pUser) {
        return;
    }

    DEBUG("(" << GetFullName()
              << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() +
                     ((sModule.empty()) ? "status" : sModule) +
                     "!znc@znc.in PRIVMSG " << GetNick() << " :" << sLine
              << "]");

    VCString vsLines;
    sLine.Split("\n", vsLines);
    for (const CString& s : vsLines) {
        Write(":" + m_pUser->GetStatusPrefix() +
              ((sModule.empty()) ? "status" : sModule) +
              "!znc@znc.in PRIVMSG " + GetNick() + " :" + s + "\r\n");
    }
}

CString CClient::GetNick(bool bAllowIRCNick) const {
    CString sRet;

    const CIRCSock* pSock = GetIRCSock();
    if (bAllowIRCNick && pSock && pSock->IsAuthed()) {
        sRet = pSock->GetNick();
    }

    return (sRet.empty()) ? m_sNick : sRet;
}

CString CClient::GetNickMask() const {
    if (GetIRCSock() && GetIRCSock()->IsAuthed()) {
        return GetIRCSock()->GetNickMask();
    }

    CString sHost =
        m_pNetwork ? m_pNetwork->GetBindHost() : m_pUser->GetBindHost();
    if (sHost.empty()) {
        sHost = "irc.znc.in";
    }

    return GetNick() + "!" +
           (m_pNetwork ? m_pNetwork->GetIdent() : m_pUser->GetIdent()) + "@" +
           sHost;
}

bool CClient::IsValidIdentifier(const CString& sIdentifier) {
    // ^[-\w]+$

    if (sIdentifier.empty()) {
        return false;
    }

    const char* p = sIdentifier.c_str();
    while (*p) {
        if (*p != '_' && *p != '-' && !isalnum(*p)) {
            return false;
        }

        p++;
    }

    return true;
}

void CClient::RespondCap(const CString& sResponse) {
    PutClient(":irc.znc.in CAP " + GetNick() + " " + sResponse);
}

void CClient::HandleCap(const CMessage& Message) {
    CString sSubCmd = Message.GetParam(0);

    if (sSubCmd.Equals("LS")) {
        SCString ssOfferCaps;
        for (const auto& it : m_mCoreCaps) {
            bool bServerDependent = std::get<0>(it.second);
            if (!bServerDependent ||
                m_ssServerDependentCaps.count(it.first) > 0)
                ssOfferCaps.insert(it.first);
        }
        GLOBALMODULECALL(OnClientCapLs(this, ssOfferCaps), NOTHING);
        CString sRes =
            CString(" ").Join(ssOfferCaps.begin(), ssOfferCaps.end());
        RespondCap("LS :" + sRes);
        m_bInCap = true;
        if (Message.GetParam(1).ToInt() >= 302) {
            m_bCapNotify = true;
        }
    } else if (sSubCmd.Equals("END")) {
        m_bInCap = false;
        if (!IsAttached()) {
            if (!m_pUser && m_bGotUser && !m_bGotPass) {
                SendRequiredPasswordNotice();
            } else {
                AuthUser();
            }
        }
    } else if (sSubCmd.Equals("REQ")) {
        VCString vsTokens;
        Message.GetParam(1).Split(" ", vsTokens, false);

        for (const CString& sToken : vsTokens) {
            bool bVal = true;
            CString sCap = sToken;
            if (sCap.TrimPrefix("-")) bVal = false;

            bool bAccepted = false;
            const auto& it = m_mCoreCaps.find(sCap);
            if (m_mCoreCaps.end() != it) {
                bool bServerDependent = std::get<0>(it->second);
                bAccepted = !bServerDependent ||
                            m_ssServerDependentCaps.count(sCap) > 0;
            }
            GLOBALMODULECALL(IsClientCapSupported(this, sCap, bVal),
                             &bAccepted);

            if (!bAccepted) {
                // Some unsupported capability is requested
                RespondCap("NAK :" + Message.GetParam(1));
                return;
            }
        }

        // All is fine, we support what was requested
        for (const CString& sToken : vsTokens) {
            bool bVal = true;
            CString sCap = sToken;
            if (sCap.TrimPrefix("-")) bVal = false;

            auto handler_it = m_mCoreCaps.find(sCap);
            if (m_mCoreCaps.end() != handler_it) {
                const auto& handler = std::get<1>(handler_it->second);
                handler(bVal);
            }
            GLOBALMODULECALL(OnClientCapRequest(this, sCap, bVal), NOTHING);

            if (bVal) {
                m_ssAcceptedCaps.insert(sCap);
            } else {
                m_ssAcceptedCaps.erase(sCap);
            }
        }

        RespondCap("ACK :" + Message.GetParam(1));
    } else if (sSubCmd.Equals("LIST")) {
        CString sList =
            CString(" ").Join(m_ssAcceptedCaps.begin(), m_ssAcceptedCaps.end());
        RespondCap("LIST :" + sList);
    } else {
        PutClient(":irc.znc.in 410 " + GetNick() + " " + sSubCmd +
                  " :Invalid CAP subcommand");
    }
}

void CClient::ParsePass(const CString& sAuthLine) {
    // [user[@identifier][/network]:]password

    const size_t uColon = sAuthLine.find(":");
    if (uColon != CString::npos) {
        m_sPass = sAuthLine.substr(uColon + 1);

        ParseUser(sAuthLine.substr(0, uColon));
    } else {
        m_sPass = sAuthLine;
    }
}

void CClient::ParseUser(const CString& sAuthLine) {
    // user[@identifier][/network]

    const size_t uSlash = sAuthLine.rfind("/");
    if (uSlash != CString::npos) {
        m_sNetwork = sAuthLine.substr(uSlash + 1);

        ParseIdentifier(sAuthLine.substr(0, uSlash));
    } else {
        ParseIdentifier(sAuthLine);
    }
}

void CClient::ParseIdentifier(const CString& sAuthLine) {
    // user[@identifier]

    const size_t uAt = sAuthLine.rfind("@");
    if (uAt != CString::npos) {
        const CString sId = sAuthLine.substr(uAt + 1);

        if (IsValidIdentifier(sId)) {
            m_sIdentifier = sId;
            m_sUser = sAuthLine.substr(0, uAt);
        } else {
            m_sUser = sAuthLine;
        }
    } else {
        m_sUser = sAuthLine;
    }
}

void CClient::SetTagSupport(const CString& sTag, bool bState) {
    if (bState) {
        m_ssSupportedTags.insert(sTag);
    } else {
        m_ssSupportedTags.erase(sTag);
    }
}

void CClient::NotifyServerDependentCaps(const SCString& ssCaps) {
    for (const CString& sCap : ssCaps) {
        const auto& it = m_mCoreCaps.find(sCap);
        if (m_mCoreCaps.end() != it) {
            bool bServerDependent = std::get<0>(it->second);
            if (bServerDependent) {
                m_ssServerDependentCaps.insert(sCap);
            }
        }
    }

    if (HasCapNotify() && !m_ssServerDependentCaps.empty()) {
        CString sCaps = CString(" ").Join(m_ssServerDependentCaps.begin(),
                                          m_ssServerDependentCaps.end());
        PutClient(":irc.znc.in CAP " + GetNick() + " NEW :" + sCaps);
    }
}

void CClient::ClearServerDependentCaps() {
    if (HasCapNotify() && !m_ssServerDependentCaps.empty()) {
        CString sCaps = CString(" ").Join(m_ssServerDependentCaps.begin(),
                                          m_ssServerDependentCaps.end());
        PutClient(":irc.znc.in CAP " + GetNick() + " DEL :" + sCaps);

        for (const CString& sCap : m_ssServerDependentCaps) {
            const auto& it = m_mCoreCaps.find(sCap);
            if (m_mCoreCaps.end() != it) {
                const auto& handler = std::get<1>(it->second);
                handler(false);
            }
        }
    }

    m_ssServerDependentCaps.clear();
}

template <typename T>
void CClient::AddBuffer(const T& Message) {
    const CString sTarget = Message.GetTarget();

    T Format;
    Format.Clone(Message);
    Format.SetNick(CNick(_NAMEDFMT(GetNickMask())));
    Format.SetTarget(_NAMEDFMT(sTarget));
    Format.SetText("{text}");

    CChan* pChan = m_pNetwork->FindChan(sTarget);
    if (pChan) {
        if (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline()) {
            pChan->AddBuffer(Format, Message.GetText());
        }
    } else if (Message.GetType() != CMessage::Type::Notice) {
        if (!m_pUser->AutoClearQueryBuffer() || !m_pNetwork->IsUserOnline()) {
            CQuery* pQuery = m_pNetwork->AddQuery(sTarget);
            if (pQuery) {
                pQuery->AddBuffer(Format, Message.GetText());
            }
        }
    }
}

void CClient::EchoMessage(const CMessage& Message) {
    CMessage EchoedMessage = Message;
    for (CClient* pClient : GetClients()) {
        if (pClient->HasEchoMessage() ||
            (pClient != this && (m_pNetwork->IsChan(Message.GetParam(0)) ||
                                 pClient->HasSelfMessage()))) {
            EchoedMessage.SetNick(GetNickMask());
            pClient->PutClient(EchoedMessage);
        }
    }
}

set<CChan*> CClient::MatchChans(const CString& sPatterns) const {
    VCString vsPatterns;
    sPatterns.Replace_n(",", " ")
        .Split(" ", vsPatterns, false, "", "", true, true);

    set<CChan*> sChans;
    for (const CString& sPattern : vsPatterns) {
        vector<CChan*> vChans = m_pNetwork->FindChans(sPattern);
        sChans.insert(vChans.begin(), vChans.end());
    }
    return sChans;
}

unsigned int CClient::AttachChans(const std::set<CChan*>& sChans) {
    unsigned int uAttached = 0;
    for (CChan* pChan : sChans) {
        if (!pChan->IsDetached()) continue;
        uAttached++;
        pChan->AttachUser();
    }
    return uAttached;
}

unsigned int CClient::DetachChans(const std::set<CChan*>& sChans) {
    unsigned int uDetached = 0;
    for (CChan* pChan : sChans) {
        if (pChan->IsDetached()) continue;
        uDetached++;
        pChan->DetachUser();
    }
    return uDetached;
}

bool CClient::OnActionMessage(CActionMessage& Message) {
    CString sTargets = Message.GetTarget();

    VCString vTargets;
    sTargets.Split(",", vTargets, false);

    for (CString& sTarget : vTargets) {
        Message.SetTarget(sTarget);
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(sTarget));
        }

        bool bContinue = false;
        NETWORKMODULECALL(OnUserActionMessage(Message), m_pUser, m_pNetwork,
                          this, &bContinue);
        if (bContinue) continue;

        if (m_pNetwork) {
            AddBuffer(Message);
            EchoMessage(Message);
            PutIRC(Message.ToString(CMessage::ExcludePrefix |
                                    CMessage::ExcludeTags));
        }
    }

    return true;
}

bool CClient::OnCTCPMessage(CCTCPMessage& Message) {
    CString sTargets = Message.GetTarget();

    VCString vTargets;
    sTargets.Split(",", vTargets, false);

    if (Message.IsReply()) {
        CString sCTCP = Message.GetText();
        if (sCTCP.Token(0) == "VERSION") {
            // There are 2 different scenarios:
            //
            // a) CTCP reply for VERSION is not set.
            // 1. ZNC receives CTCP VERSION from someone
            // 2. ZNC forwards CTCP VERSION to client
            // 3. Client replies with something
            // 4. ZNC adds itself to the reply
            // 5. ZNC sends the modified reply to whoever asked
            //
            // b) CTCP reply for VERSION is set.
            // 1. ZNC receives CTCP VERSION from someone
            // 2. ZNC replies with the configured reply (or just drops it if
            //    empty), without forwarding anything to client
            // 3. Client does not see any CTCP request, and does not reply
            //
            // So, if user doesn't want "via ZNC" in CTCP VERSION reply, they
            // can set custom reply.
            //
            // See more bikeshedding at github issues #820 and #1012
            Message.SetText(sCTCP + " via " + CZNC::GetTag(false));
        }
    }

    for (CString& sTarget : vTargets) {
        Message.SetTarget(sTarget);
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(sTarget));
        }

        bool bContinue = false;
        if (Message.IsReply()) {
            NETWORKMODULECALL(OnUserCTCPReplyMessage(Message), m_pUser,
                              m_pNetwork, this, &bContinue);
        } else {
            NETWORKMODULECALL(OnUserCTCPMessage(Message), m_pUser, m_pNetwork,
                              this, &bContinue);
        }
        if (bContinue) continue;

        if (!GetIRCSock()) {
            // Some lagmeters do a NOTICE to their own nick, ignore those.
            if (!sTarget.Equals(m_sNick))
                PutStatus(t_f(
                    "Your CTCP to {1} got lost, you are not connected to IRC!")(
                    Message.GetTarget()));
            continue;
        }

        if (m_pNetwork) {
            PutIRC(Message.ToString(CMessage::ExcludePrefix |
                                    CMessage::ExcludeTags));
        }
    }

    return true;
}

bool CClient::OnJoinMessage(CJoinMessage& Message) {
    CString sChans = Message.GetTarget();
    CString sKeys = Message.GetKey();

    VCString vsChans;
    sChans.Split(",", vsChans, false);
    sChans.clear();

    VCString vsKeys;
    sKeys.Split(",", vsKeys, true);
    sKeys.clear();

    for (unsigned int a = 0; a < vsChans.size(); a++) {
        Message.SetTarget(vsChans[a]);
        Message.SetKey((a < vsKeys.size()) ? vsKeys[a] : "");
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(vsChans[a]));
        }
        bool bContinue = false;
        NETWORKMODULECALL(OnUserJoinMessage(Message), m_pUser, m_pNetwork, this,
                          &bContinue);
        if (bContinue) continue;

        CString sChannel = Message.GetTarget();
        CString sKey = Message.GetKey();

        if (m_pNetwork) {
            CChan* pChan = m_pNetwork->FindChan(sChannel);
            if (pChan) {
                if (pChan->IsDetached())
                    pChan->AttachUser(this);
                else
                    pChan->JoinUser(sKey);
                continue;
            } else if (!sChannel.empty()) {
                pChan = new CChan(sChannel, m_pNetwork, false);
                if (m_pNetwork->AddChan(pChan)) {
                    pChan->SetKey(sKey);
                }
            }
        }

        if (!sChannel.empty()) {
            sChans += (sChans.empty()) ? sChannel : CString("," + sChannel);

            if (!vsKeys.empty()) {
                sKeys += (sKeys.empty()) ? sKey : CString("," + sKey);
            }
        }
    }

    Message.SetTarget(sChans);
    Message.SetKey(sKeys);

    return sChans.empty();
}

bool CClient::OnModeMessage(CModeMessage& Message) {
    CString sTarget = Message.GetTarget();
    CString sModes = Message.GetModes();

    if (m_pNetwork && m_pNetwork->IsChan(sTarget) && sModes.empty()) {
        // If we are on that channel and already received a
        // /mode reply from the server, we can answer this
        // request ourself.

        CChan* pChan = m_pNetwork->FindChan(sTarget);
        if (pChan && pChan->IsOn() && !pChan->GetModeString().empty()) {
            PutClient(":" + m_pNetwork->GetIRCServer() + " 324 " + GetNick() +
                      " " + sTarget + " " + pChan->GetModeString());
            if (pChan->GetCreationDate() > 0) {
                PutClient(":" + m_pNetwork->GetIRCServer() + " 329 " +
                          GetNick() + " " + sTarget + " " +
                          CString(pChan->GetCreationDate()));
            }
            return true;
        }
    }

    return false;
}

bool CClient::OnNoticeMessage(CNoticeMessage& Message) {
    CString sTargets = Message.GetTarget();

    VCString vTargets;
    sTargets.Split(",", vTargets, false);

    for (CString& sTarget : vTargets) {
        Message.SetTarget(sTarget);
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(sTarget));
        }

        if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
            if (!sTarget.Equals("status")) {
                CALLMOD(sTarget, this, m_pUser, m_pNetwork,
                        OnModNotice(Message.GetText()));
            }
            continue;
        }

        bool bContinue = false;
        NETWORKMODULECALL(OnUserNoticeMessage(Message), m_pUser, m_pNetwork,
                          this, &bContinue);
        if (bContinue) continue;

        if (!GetIRCSock()) {
            // Some lagmeters do a NOTICE to their own nick, ignore those.
            if (!sTarget.Equals(m_sNick))
                PutStatus(
                    t_f("Your notice to {1} got lost, you are not connected to "
                        "IRC!")(Message.GetTarget()));
            continue;
        }

        if (m_pNetwork) {
            AddBuffer(Message);
            EchoMessage(Message);
            PutIRC(Message.ToString(CMessage::ExcludePrefix |
                                    CMessage::ExcludeTags));
        }
    }

    return true;
}

bool CClient::OnPartMessage(CPartMessage& Message) {
    CString sChans = Message.GetTarget();

    VCString vsChans;
    sChans.Split(",", vsChans, false);
    sChans.clear();

    for (CString& sChan : vsChans) {
        bool bContinue = false;
        Message.SetTarget(sChan);
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(sChan));
        }
        NETWORKMODULECALL(OnUserPartMessage(Message), m_pUser, m_pNetwork, this,
                          &bContinue);
        if (bContinue) continue;

        sChan = Message.GetTarget();

        CChan* pChan = m_pNetwork ? m_pNetwork->FindChan(sChan) : nullptr;

        if (pChan && !pChan->IsOn()) {
            PutStatusNotice(t_f("Removing channel {1}")(sChan));
            m_pNetwork->DelChan(sChan);
        } else {
            sChans += (sChans.empty()) ? sChan : CString("," + sChan);
        }
    }

    if (sChans.empty()) {
        return true;
    }

    Message.SetTarget(sChans);

    return false;
}

bool CClient::OnPingMessage(CMessage& Message) {
    // All PONGs are generated by ZNC. We will still forward this to
    // the ircd, but all PONGs from irc will be blocked.
    if (!Message.GetParams().empty())
        PutClient(":irc.znc.in PONG irc.znc.in " + Message.GetParamsColon(0));
    else
        PutClient(":irc.znc.in PONG irc.znc.in");
    return false;
}

bool CClient::OnPongMessage(CMessage& Message) {
    // Block PONGs, we already responded to the pings
    return true;
}

bool CClient::OnQuitMessage(CQuitMessage& Message) {
    bool bReturn = false;
    NETWORKMODULECALL(OnUserQuitMessage(Message), m_pUser, m_pNetwork, this,
                      &bReturn);
    if (!bReturn) {
        Close(Csock::CLT_AFTERWRITE);  // Treat a client quit as a detach
    }
    // Don't forward this msg.  We don't want the client getting us
    // disconnected.
    return true;
}

bool CClient::OnTextMessage(CTextMessage& Message) {
    CString sTargets = Message.GetTarget();

    VCString vTargets;
    sTargets.Split(",", vTargets, false);

    for (CString& sTarget : vTargets) {
        Message.SetTarget(sTarget);
        if (m_pNetwork) {
            // May be nullptr.
            Message.SetChan(m_pNetwork->FindChan(sTarget));
        }

        if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
            if (sTarget.Equals("status")) {
                CString sMsg = Message.GetText();
                UserCommand(sMsg);
            } else {
                CALLMOD(sTarget, this, m_pUser, m_pNetwork,
                        OnModCommand(Message.GetText()));
            }
            continue;
        }

        bool bContinue = false;
        NETWORKMODULECALL(OnUserTextMessage(Message), m_pUser, m_pNetwork, this,
                          &bContinue);
        if (bContinue) continue;

        if (!GetIRCSock()) {
            // Some lagmeters do a PRIVMSG to their own nick, ignore those.
            if (!sTarget.Equals(m_sNick))
                PutStatus(
                    t_f("Your message to {1} got lost, you are not connected "
                        "to IRC!")(Message.GetTarget()));
            continue;
        }

        if (m_pNetwork) {
            AddBuffer(Message);
            EchoMessage(Message);
            PutIRC(Message.ToString(CMessage::ExcludePrefix |
                                    CMessage::ExcludeTags));
        }
    }

    return true;
}

bool CClient::OnTopicMessage(CTopicMessage& Message) {
    bool bReturn = false;
    CString sChan = Message.GetTarget();
    CString sTopic = Message.GetTopic();
    if (m_pNetwork) {
        // May be nullptr.
        Message.SetChan(m_pNetwork->FindChan(sChan));
    }

    if (!sTopic.empty()) {
        NETWORKMODULECALL(OnUserTopicMessage(Message), m_pUser, m_pNetwork,
                          this, &bReturn);
    } else {
        NETWORKMODULECALL(OnUserTopicRequest(sChan), m_pUser, m_pNetwork, this,
                          &bReturn);
        Message.SetTarget(sChan);
    }

    return bReturn;
}

bool CClient::OnOtherMessage(CMessage& Message) {
    const CString& sCommand = Message.GetCommand();

    if (sCommand.Equals("ZNC")) {
        CString sTarget = Message.GetParam(0);
        CString sModCommand;

        if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
            sModCommand = Message.GetParamsColon(1);
        } else {
            sTarget = "status";
            sModCommand = Message.GetParamsColon(0);
        }

        if (sTarget.Equals("status")) {
            if (sModCommand.empty())
                PutStatus(t_s("Hello. How may I help you?"));
            else
                UserCommand(sModCommand);
        } else {
            if (sModCommand.empty())
                CALLMOD(sTarget, this, m_pUser, m_pNetwork,
                        PutModule(t_s("Hello. How may I help you?")))
            else
                CALLMOD(sTarget, this, m_pUser, m_pNetwork,
                        OnModCommand(sModCommand))
        }
        return true;
    } else if (sCommand.Equals("ATTACH")) {
        if (!m_pNetwork) {
            return true;
        }

        CString sPatterns = Message.GetParamsColon(0);

        if (sPatterns.empty()) {
            PutStatusNotice(t_s("Usage: /attach <#chans>"));
            return true;
        }

        set<CChan*> sChans = MatchChans(sPatterns);
        unsigned int uAttachedChans = AttachChans(sChans);

        PutStatusNotice(t_p("There was {1} channel matching [{2}]",
                            "There were {1} channels matching [{2}]",
                            sChans.size())(sChans.size(), sPatterns));
        PutStatusNotice(t_p("Attached {1} channel", "Attached {1} channels",
                            uAttachedChans)(uAttachedChans));

        return true;
    } else if (sCommand.Equals("DETACH")) {
        if (!m_pNetwork) {
            return true;
        }

        CString sPatterns = Message.GetParamsColon(0);

        if (sPatterns.empty()) {
            PutStatusNotice(t_s("Usage: /detach <#chans>"));
            return true;
        }

        set<CChan*> sChans = MatchChans(sPatterns);
        unsigned int uDetached = DetachChans(sChans);

        PutStatusNotice(t_p("There was {1} channel matching [{2}]",
                            "There were {1} channels matching [{2}]",
                            sChans.size())(sChans.size(), sPatterns));
        PutStatusNotice(t_p("Detached {1} channel", "Detached {1} channels",
                            uDetached)(uDetached));

        return true;
    } else if (sCommand.Equals("PROTOCTL")) {
        for (const CString& sParam : Message.GetParams()) {
            if (sParam == "NAMESX") {
                m_bNamesx = true;
            } else if (sParam == "UHNAMES") {
                m_bUHNames = true;
            }
        }
        return true;  // If the server understands it, we already enabled namesx
                      // / uhnames
    }

    return false;
}
