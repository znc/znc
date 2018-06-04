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

#include <znc/IRCSock.h>
#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Server.h>
#include <znc/Query.h>
#include <znc/ZNCDebug.h>
#include <time.h>

using std::set;
using std::vector;
using std::map;

#define IRCSOCKMODULECALL(macFUNC, macEXITER)                              \
    NETWORKMODULECALL(macFUNC, m_pNetwork->GetUser(), m_pNetwork, nullptr, \
                      macEXITER)
// These are used in OnGeneralCTCP()
const time_t CIRCSock::m_uCTCPFloodTime = 5;
const unsigned int CIRCSock::m_uCTCPFloodCount = 5;

// It will be bad if user sets it to 0.00000000000001
// If you want no flood protection, set network's flood rate to -1
// TODO move this constant to CIRCNetwork?
static const double FLOOD_MINIMAL_RATE = 0.3;

class CIRCFloodTimer : public CCron {
    CIRCSock* m_pSock;

  public:
    CIRCFloodTimer(CIRCSock* pSock) : m_pSock(pSock) {
        StartMaxCycles(m_pSock->m_fFloodRate, 0);
    }
    CIRCFloodTimer(const CIRCFloodTimer&) = delete;
    CIRCFloodTimer& operator=(const CIRCFloodTimer&) = delete;
    void RunJob() override {
        if (m_pSock->m_iSendsAllowed < m_pSock->m_uFloodBurst) {
            m_pSock->m_iSendsAllowed++;
        }
        m_pSock->TrySend();
    }
};

bool CIRCSock::IsFloodProtected(double fRate) {
    return fRate > FLOOD_MINIMAL_RATE;
}

CIRCSock::CIRCSock(CIRCNetwork* pNetwork)
    : CIRCSocket(),
      m_bAuthed(false),
      m_bNamesx(false),
      m_bUHNames(false),
      m_bAwayNotify(false),
      m_bAccountNotify(false),
      m_bExtendedJoin(false),
      m_bServerTime(false),
      m_sPerms("*!@%+"),
      m_sPermModes("qaohv"),
      m_scUserModes(),
      m_mceChanModes(),
      m_pNetwork(pNetwork),
      m_Nick(),
      m_sPass(""),
      m_msChans(),
      m_uMaxNickLen(9),
      m_uCapPaused(0),
      m_ssAcceptedCaps(),
      m_ssPendingCaps(),
      m_lastCTCP(0),
      m_uNumCTCP(0),
      m_mISupport(),
      m_vSendQueue(),
      m_iSendsAllowed(pNetwork->GetFloodBurst()),
      m_uFloodBurst(pNetwork->GetFloodBurst()),
      m_fFloodRate(pNetwork->GetFloodRate()),
      m_bFloodProtection(IsFloodProtected(pNetwork->GetFloodRate())) {
    EnableReadLine();
    m_Nick.SetIdent(m_pNetwork->GetIdent());
    m_Nick.SetHost(m_pNetwork->GetBindHost());
    SetEncoding(m_pNetwork->GetEncoding());

    m_mceChanModes['b'] = ListArg;
    m_mceChanModes['e'] = ListArg;
    m_mceChanModes['I'] = ListArg;
    m_mceChanModes['k'] = HasArg;
    m_mceChanModes['l'] = ArgWhenSet;
    m_mceChanModes['p'] = NoArg;
    m_mceChanModes['s'] = NoArg;
    m_mceChanModes['t'] = NoArg;
    m_mceChanModes['i'] = NoArg;
    m_mceChanModes['n'] = NoArg;

    pNetwork->SetIRCSocket(this);

    // RFC says a line can have 512 chars max + 512 chars for message tags, but
    // we don't care ;)
    SetMaxBufferThreshold(2048);
    if (m_bFloodProtection) {
        AddCron(new CIRCFloodTimer(this));
    }
}

CIRCSock::~CIRCSock() {
    if (!m_bAuthed) {
        IRCSOCKMODULECALL(OnIRCConnectionError(this), NOTHING);
    }

    const vector<CChan*>& vChans = m_pNetwork->GetChans();
    for (CChan* pChan : vChans) {
        pChan->Reset();
    }

    m_pNetwork->IRCDisconnected();

    for (const auto& it : m_msChans) {
        delete it.second;
    }

    Quit();
    m_msChans.clear();
    m_pNetwork->AddBytesRead(GetBytesRead());
    m_pNetwork->AddBytesWritten(GetBytesWritten());
}

void CIRCSock::Quit(const CString& sQuitMsg) {
    if (IsClosed()) {
        return;
    }
    if (!m_bAuthed) {
        Close(CLT_NOW);
        return;
    }
    if (!sQuitMsg.empty()) {
        PutIRC("QUIT :" + sQuitMsg);
    } else {
        PutIRC("QUIT :" + m_pNetwork->ExpandString(m_pNetwork->GetQuitMsg()));
    }
    Close(CLT_AFTERWRITE);
}

void CIRCSock::ReadLine(const CString& sData) {
    CString sLine = sData;

    sLine.TrimRight("\n\r");

    DEBUG("(" << m_pNetwork->GetUser()->GetUserName() << "/"
              << m_pNetwork->GetName() << ") IRC -> ZNC [" << sLine << "]");

    bool bReturn = false;
    IRCSOCKMODULECALL(OnRaw(sLine), &bReturn);
    if (bReturn) return;

    CMessage Message(sLine);
    Message.SetNetwork(m_pNetwork);

    IRCSOCKMODULECALL(OnRawMessage(Message), &bReturn);
    if (bReturn) return;

    switch (Message.GetType()) {
        case CMessage::Type::Account:
            bReturn = OnAccountMessage(Message);
            break;
        case CMessage::Type::Action:
            bReturn = OnActionMessage(Message);
            break;
        case CMessage::Type::Away:
            bReturn = OnAwayMessage(Message);
            break;
        case CMessage::Type::Capability:
            bReturn = OnCapabilityMessage(Message);
            break;
        case CMessage::Type::CTCP:
            bReturn = OnCTCPMessage(Message);
            break;
        case CMessage::Type::Error:
            bReturn = OnErrorMessage(Message);
            break;
        case CMessage::Type::Invite:
            bReturn = OnInviteMessage(Message);
            break;
        case CMessage::Type::Join:
            bReturn = OnJoinMessage(Message);
            break;
        case CMessage::Type::Kick:
            bReturn = OnKickMessage(Message);
            break;
        case CMessage::Type::Mode:
            bReturn = OnModeMessage(Message);
            break;
        case CMessage::Type::Nick:
            bReturn = OnNickMessage(Message);
            break;
        case CMessage::Type::Notice:
            bReturn = OnNoticeMessage(Message);
            break;
        case CMessage::Type::Numeric:
            bReturn = OnNumericMessage(Message);
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
        case CMessage::Type::Wallops:
            bReturn = OnWallopsMessage(Message);
            break;
        default:
            break;
    }
    if (bReturn) return;

    m_pNetwork->PutUser(Message);
}

void CIRCSock::SendNextCap() {
    if (!m_uCapPaused) {
        if (m_ssPendingCaps.empty()) {
            // We already got all needed ACK/NAK replies.
            PutIRC("CAP END");
        } else {
            CString sCap = *m_ssPendingCaps.begin();
            m_ssPendingCaps.erase(m_ssPendingCaps.begin());
            PutIRC("CAP REQ :" + sCap);
        }
    }
}

void CIRCSock::PauseCap() { ++m_uCapPaused; }

void CIRCSock::ResumeCap() {
    --m_uCapPaused;
    SendNextCap();
}

bool CIRCSock::OnServerCapAvailable(const CString& sCap) {
    bool bResult = false;
    IRCSOCKMODULECALL(OnServerCapAvailable(sCap), &bResult);
    return bResult;
}

// #124: OnChanMsg(): nick doesn't have perms
static void FixupChanNick(CNick& Nick, CChan* pChan) {
    // A channel nick has up-to-date channel perms, but might be
    // lacking (usernames-in-host) the associated ident & host.
    // An incoming message, on the other hand, has normally a full
    // nick!ident@host prefix. Sync the two so that channel nicks
    // get the potentially missing piece of info and module hooks
    // get the perms.
    CNick* pChanNick = pChan->FindNick(Nick.GetNick());
    if (pChanNick) {
        if (!Nick.GetIdent().empty()) {
            pChanNick->SetIdent(Nick.GetIdent());
        }
        if (!Nick.GetHost().empty()) {
            pChanNick->SetHost(Nick.GetHost());
        }
        Nick.Clone(*pChanNick);
    }
}

bool CIRCSock::OnAccountMessage(CMessage& Message) {
    // TODO: IRCSOCKMODULECALL(OnAccountMessage(Message)) ?
    return false;
}

bool CIRCSock::OnActionMessage(CActionMessage& Message) {
    bool bResult = false;
    CChan* pChan = nullptr;
    CString sTarget = Message.GetTarget();
    if (sTarget.Equals(GetNick())) {
        IRCSOCKMODULECALL(OnPrivCTCPMessage(Message), &bResult);
        if (bResult) return true;
        IRCSOCKMODULECALL(OnPrivActionMessage(Message), &bResult);
        if (bResult) return true;

        if (!m_pNetwork->IsUserOnline() ||
            !m_pNetwork->GetUser()->AutoClearQueryBuffer()) {
            const CNick& Nick = Message.GetNick();
            CQuery* pQuery = m_pNetwork->AddQuery(Nick.GetNick());
            if (pQuery) {
                CActionMessage Format;
                Format.Clone(Message);
                Format.SetNick(_NAMEDFMT(Nick.GetNickMask()));
                Format.SetTarget("{target}");
                Format.SetText("{text}");
                pQuery->AddBuffer(Format, Message.GetText());
            }
        }
    } else {
        pChan = m_pNetwork->FindChan(sTarget);
        if (pChan) {
            Message.SetChan(pChan);
            FixupChanNick(Message.GetNick(), pChan);
            IRCSOCKMODULECALL(OnChanCTCPMessage(Message), &bResult);
            if (bResult) return true;
            IRCSOCKMODULECALL(OnChanActionMessage(Message), &bResult);
            if (bResult) return true;

            if (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline() ||
                pChan->IsDetached()) {
                CActionMessage Format;
                Format.Clone(Message);
                Format.SetNick(_NAMEDFMT(Message.GetNick().GetNickMask()));
                Format.SetTarget(_NAMEDFMT(Message.GetTarget()));
                Format.SetText("{text}");
                pChan->AddBuffer(Format, Message.GetText());
            }
        }
    }

    return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnAwayMessage(CMessage& Message) {
    // TODO: IRCSOCKMODULECALL(OnAwayMessage(Message)) ?
    return false;
}

bool CIRCSock::OnCapabilityMessage(CMessage& Message) {
    // CAPs are supported only before authorization.
    if (!m_bAuthed) {
        // The first parameter is most likely "*". No idea why, the
        // CAP spec don't mention this, but all implementations
        // I've seen add this extra asterisk
        CString sSubCmd = Message.GetParam(1);

        // If the caplist of a reply is too long, it's split
        // into multiple replies. A "*" is prepended to show
        // that the list was split into multiple replies.
        // This is useful mainly for LS. For ACK and NAK
        // replies, there's no real need for this, because
        // we request only 1 capability per line.
        // If we will need to support broken servers or will
        // send several requests per line, need to delay ACK
        // actions until all ACK lines are received and
        // to recognize past request of NAK by 100 chars
        // of this reply.
        CString sArgs;
        if (Message.GetParam(2) == "*") {
            sArgs = Message.GetParam(3);
        } else {
            sArgs = Message.GetParam(2);
        }

        std::map<CString, std::function<void(bool bVal)>> mSupportedCaps = {
            {"multi-prefix", [this](bool bVal) { m_bNamesx = bVal; }},
            {"userhost-in-names", [this](bool bVal) { m_bUHNames = bVal; }},
            {"away-notify", [this](bool bVal) { m_bAwayNotify = bVal; }},
            {"account-notify", [this](bool bVal) { m_bAccountNotify = bVal; }},
            {"extended-join", [this](bool bVal) { m_bExtendedJoin = bVal; }},
            {"server-time", [this](bool bVal) { m_bServerTime = bVal; }},
            {"znc.in/server-time-iso",
             [this](bool bVal) { m_bServerTime = bVal; }},
        };

        if (sSubCmd == "LS") {
            VCString vsTokens;
            sArgs.Split(" ", vsTokens, false);

            for (const CString& sCap : vsTokens) {
                if (OnServerCapAvailable(sCap) || mSupportedCaps.count(sCap)) {
                    m_ssPendingCaps.insert(sCap);
                }
            }
        } else if (sSubCmd == "ACK") {
            sArgs.Trim();
            IRCSOCKMODULECALL(OnServerCapResult(sArgs, true), NOTHING);
            const auto& it = mSupportedCaps.find(sArgs);
            if (it != mSupportedCaps.end()) {
                it->second(true);
            }
            m_ssAcceptedCaps.insert(sArgs);
        } else if (sSubCmd == "NAK") {
            // This should work because there's no [known]
            // capability with length of name more than 100 characters.
            sArgs.Trim();
            IRCSOCKMODULECALL(OnServerCapResult(sArgs, false), NOTHING);
        }

        SendNextCap();
    }
    // Don't forward any CAP stuff to the client
    return true;
}

bool CIRCSock::OnCTCPMessage(CCTCPMessage& Message) {
    bool bResult = false;
    CChan* pChan = nullptr;
    CString sTarget = Message.GetTarget();
    if (sTarget.Equals(GetNick())) {
        if (Message.IsReply()) {
            IRCSOCKMODULECALL(OnCTCPReplyMessage(Message), &bResult);
            return bResult;
        } else {
            IRCSOCKMODULECALL(OnPrivCTCPMessage(Message), &bResult);
            if (bResult) return true;
        }
    } else {
        pChan = m_pNetwork->FindChan(sTarget);
        if (pChan) {
            Message.SetChan(pChan);
            FixupChanNick(Message.GetNick(), pChan);
            IRCSOCKMODULECALL(OnChanCTCPMessage(Message), &bResult);
            if (bResult) return true;
        }
    }

    const CNick& Nick = Message.GetNick();
    const CString& sMessage = Message.GetText();
    const MCString& mssCTCPReplies = m_pNetwork->GetUser()->GetCTCPReplies();
    CString sQuery = sMessage.Token(0).AsUpper();
    MCString::const_iterator it = mssCTCPReplies.find(sQuery);
    bool bHaveReply = false;
    CString sReply;

    if (it != mssCTCPReplies.end()) {
        sReply = m_pNetwork->ExpandString(it->second);
        bHaveReply = true;

        if (sReply.empty()) {
            return true;
        }
    }

    if (!bHaveReply && !m_pNetwork->IsUserAttached()) {
        if (sQuery == "VERSION") {
            sReply = CZNC::GetTag(false);
        } else if (sQuery == "PING") {
            sReply = sMessage.Token(1, true);
        }
    }

    if (!sReply.empty()) {
        time_t now = time(nullptr);
        // If the last CTCP is older than m_uCTCPFloodTime, reset the counter
        if (m_lastCTCP + m_uCTCPFloodTime < now) m_uNumCTCP = 0;
        m_lastCTCP = now;
        // If we are over the limit, don't reply to this CTCP
        if (m_uNumCTCP >= m_uCTCPFloodCount) {
            DEBUG("CTCP flood detected - not replying to query");
            return true;
        }
        m_uNumCTCP++;

        PutIRC("NOTICE " + Nick.GetNick() + " :\001" + sQuery + " " + sReply +
               "\001");
        return true;
    }

    return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnErrorMessage(CMessage& Message) {
    // ERROR :Closing Link: nick[24.24.24.24] (Excess Flood)
    CString sError = Message.GetParam(0);
    m_pNetwork->PutStatus(t_f("Error from server: {1}")(sError));
    return true;
}

bool CIRCSock::OnInviteMessage(CMessage& Message) {
    bool bResult = false;
    IRCSOCKMODULECALL(OnInvite(Message.GetNick(), Message.GetParam(1)),
                      &bResult);
    return bResult;
}

bool CIRCSock::OnJoinMessage(CJoinMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sChan = Message.GetParam(0);
    CChan* pChan = nullptr;

    if (Nick.NickEquals(GetNick())) {
        m_pNetwork->AddChan(sChan, false);
        pChan = m_pNetwork->FindChan(sChan);
        if (pChan) {
            pChan->Enable();
            pChan->SetIsOn(true);
            PutIRC("MODE " + sChan);
        }
    } else {
        pChan = m_pNetwork->FindChan(sChan);
    }

    if (pChan) {
        pChan->AddNick(Nick.GetNickMask());
        Message.SetChan(pChan);
        IRCSOCKMODULECALL(OnJoinMessage(Message), NOTHING);

        if (pChan->IsDetached()) {
            return true;
        }
    }

    return false;
}

bool CIRCSock::OnKickMessage(CKickMessage& Message) {
    CString sChan = Message.GetParam(0);
    CString sKickedNick = Message.GetKickedNick();

    CChan* pChan = m_pNetwork->FindChan(sChan);

    if (pChan) {
        Message.SetChan(pChan);
        IRCSOCKMODULECALL(OnKickMessage(Message), NOTHING);
        // do not remove the nick till after the OnKick call, so modules
        // can do Chan.FindNick or something to get more info.
        pChan->RemNick(sKickedNick);
    }

    if (GetNick().Equals(sKickedNick) && pChan) {
        pChan->SetIsOn(false);

        // Don't try to rejoin!
        pChan->Disable();
    }

    return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnModeMessage(CModeMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sTarget = Message.GetTarget();
    CString sModes = Message.GetModes();

    CChan* pChan = m_pNetwork->FindChan(sTarget);
    if (pChan) {
        pChan->ModeChange(sModes, &Nick);

        if (pChan->IsDetached()) {
            return true;
        }
    } else if (sTarget == m_Nick.GetNick()) {
        CString sModeArg = sModes.Token(0);
        bool bAdd = true;
        /* no module call defined (yet?)
                MODULECALL(OnRawUserMode(*pOpNick, *this, sModeArg, sArgs),
           m_pNetwork->GetUser(), nullptr, );
        */
        for (unsigned int a = 0; a < sModeArg.size(); a++) {
            const char& cMode = sModeArg[a];

            if (cMode == '+') {
                bAdd = true;
            } else if (cMode == '-') {
                bAdd = false;
            } else {
                if (bAdd) {
                    m_scUserModes.insert(cMode);
                } else {
                    m_scUserModes.erase(cMode);
                }
            }
        }
    }
    return false;
}

bool CIRCSock::OnNickMessage(CNickMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sNewNick = Message.GetNewNick();
    bool bIsVisible = false;

    vector<CChan*> vFoundChans;
    const vector<CChan*>& vChans = m_pNetwork->GetChans();

    for (CChan* pChan : vChans) {
        if (pChan->ChangeNick(Nick.GetNick(), sNewNick)) {
            vFoundChans.push_back(pChan);

            if (!pChan->IsDetached()) {
                bIsVisible = true;
            }
        }
    }

    if (Nick.NickEquals(GetNick())) {
        // We are changing our own nick, the clients always must see this!
        bIsVisible = false;
        SetNick(sNewNick);
        m_pNetwork->PutUser(Message);
    }

    IRCSOCKMODULECALL(OnNickMessage(Message, vFoundChans), NOTHING);

    return !bIsVisible;
}

bool CIRCSock::OnNoticeMessage(CNoticeMessage& Message) {
    CString sTarget = Message.GetTarget();
    bool bResult = false;

    if (sTarget.Equals(GetNick())) {
        IRCSOCKMODULECALL(OnPrivNoticeMessage(Message), &bResult);
        if (bResult) return true;

        if (!m_pNetwork->IsUserOnline()) {
            // If the user is detached, add to the buffer
            CNoticeMessage Format;
            Format.Clone(Message);
            Format.SetNick(CNick(_NAMEDFMT(Message.GetNick().GetNickMask())));
            Format.SetTarget("{target}");
            Format.SetText("{text}");
            m_pNetwork->AddNoticeBuffer(Format, Message.GetText());
        }

        return false;
    } else {
        CChan* pChan = m_pNetwork->FindChan(sTarget);
        if (pChan) {
            Message.SetChan(pChan);
            FixupChanNick(Message.GetNick(), pChan);
            IRCSOCKMODULECALL(OnChanNoticeMessage(Message), &bResult);
            if (bResult) return true;

            if (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline() ||
                pChan->IsDetached()) {
                CNoticeMessage Format;
                Format.Clone(Message);
                Format.SetNick(_NAMEDFMT(Message.GetNick().GetNickMask()));
                Format.SetTarget(_NAMEDFMT(Message.GetTarget()));
                Format.SetText("{text}");
                pChan->AddBuffer(Format, Message.GetText());
            }
        }

        return (pChan && pChan->IsDetached());
    }
}

static CMessage BufferMessage(const CNumericMessage& Message) {
    CMessage Format(Message);
    Format.SetNick(CNick(_NAMEDFMT(Message.GetNick().GetHostMask())));
    Format.SetParam(0, "{target}");
    unsigned uParams = Format.GetParams().size();
    for (unsigned int i = 1; i < uParams; ++i) {
        Format.SetParam(i, _NAMEDFMT(Format.GetParam(i)));
    }
    return Format;
}

bool CIRCSock::OnNumericMessage(CNumericMessage& Message) {
    const CString& sCmd = Message.GetCommand();
    CString sServer = Message.GetNick().GetHostMask();
    unsigned int uRaw = Message.GetCode();
    CString sNick = Message.GetParam(0);

    bool bResult = false;
    IRCSOCKMODULECALL(OnNumericMessage(Message), &bResult);
    if (bResult) return true;

    switch (uRaw) {
        case 1: {  // :irc.server.com 001 nick :Welcome to the Internet Relay
            if (m_bAuthed && sServer == "irc.znc.in") {
                // m_bAuthed == true => we already received another 001 => we
                // might be in a traffic loop
                m_pNetwork->PutStatus(t_s(
                    "ZNC seems to be connected to itself, disconnecting..."));
                Quit();
                return true;
            }

            m_pNetwork->SetIRCServer(sServer);
            // Now that we are connected, let nature take its course
            SetTimeout(m_pNetwork->GetUser()->GetNoTrafficTimeout(), TMO_READ);
            PutIRC("WHO " + sNick);

            m_bAuthed = true;
            m_pNetwork->PutStatus("Connected!");

            const vector<CClient*>& vClients = m_pNetwork->GetClients();

            for (CClient* pClient : vClients) {
                CString sClientNick = pClient->GetNick(false);

                if (!sClientNick.Equals(sNick)) {
                    // If they connected with a nick that doesn't match the one
                    // we got on irc, then we need to update them
                    pClient->PutClient(":" + sClientNick + "!" +
                                       m_Nick.GetIdent() + "@" +
                                       m_Nick.GetHost() + " NICK :" + sNick);
                }
            }

            SetNick(sNick);

            IRCSOCKMODULECALL(OnIRCConnected(), NOTHING);

            m_pNetwork->ClearRawBuffer();
            m_pNetwork->AddRawBuffer(BufferMessage(Message));

            m_pNetwork->IRCConnected();

            break;
        }
        case 5:
            ParseISupport(Message);
            m_pNetwork->UpdateExactRawBuffer(BufferMessage(Message));
            break;
        case 10: {  // :irc.server.com 010 nick <hostname> <port> :<info>
            CString sHost = Message.GetParam(1);
            CString sPort = Message.GetParam(2);
            CString sInfo = Message.GetParam(3);
            m_pNetwork->PutStatus(
                t_f("Server {1} redirects us to {2}:{3} with reason: {4}")(
                    m_pNetwork->GetCurrentServer()->GetString(false), sHost,
                    sPort, sInfo));
            m_pNetwork->PutStatus(
                t_s("Perhaps you want to add it as a new server."));
            // Don't send server redirects to the client
            return true;
        }
        case 2:
        case 3:
        case 4:
        case 250:  // highest connection count
        case 251:  // user count
        case 252:  // oper count
        case 254:  // channel count
        case 255:  // client count
        case 265:  // local users
        case 266:  // global users
            m_pNetwork->UpdateRawBuffer(sCmd, BufferMessage(Message));
            break;
        case 305:
            m_pNetwork->SetIRCAway(false);
            break;
        case 306:
            m_pNetwork->SetIRCAway(true);
            break;
        case 324: {  // MODE
            // :irc.server.com 324 nick #chan +nstk key
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                pChan->SetModes(Message.GetParamsColon(2));

                // We don't SetModeKnown(true) here,
                // because a 329 will follow
                if (!pChan->IsModeKnown()) {
                    // When we JOIN, we send a MODE
                    // request. This makes sure the
                    // reply isn't forwarded.
                    return true;
                }
                if (pChan->IsDetached()) {
                    return true;
                }
            }
        } break;
        case 329: {
            // :irc.server.com 329 nick #chan 1234567890
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                unsigned long ulDate = Message.GetParam(2).ToULong();
                pChan->SetCreationDate(ulDate);

                if (!pChan->IsModeKnown()) {
                    pChan->SetModeKnown(true);
                    // When we JOIN, we send a MODE
                    // request. This makes sure the
                    // reply isn't forwarded.
                    return true;
                }
                if (pChan->IsDetached()) {
                    return true;
                }
            }
        } break;
        case 331: {
            // :irc.server.com 331 yournick #chan :No topic is set.
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                pChan->SetTopic("");
                if (pChan->IsDetached()) {
                    return true;
                }
            }

            break;
        }
        case 332: {
            // :irc.server.com 332 yournick #chan :This is a topic
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                CString sTopic = Message.GetParam(2);
                pChan->SetTopic(sTopic);
                if (pChan->IsDetached()) {
                    return true;
                }
            }

            break;
        }
        case 333: {
            // :irc.server.com 333 yournick #chan setternick 1112320796
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                sNick = Message.GetParam(2);
                unsigned long ulDate = Message.GetParam(3).ToULong();

                pChan->SetTopicOwner(sNick);
                pChan->SetTopicDate(ulDate);

                if (pChan->IsDetached()) {
                    return true;
                }
            }

            break;
        }
        case 352: {  // WHO
            // :irc.yourserver.com 352 yournick #chan ident theirhost.com irc.theirserver.com theirnick H :0 Real Name
            sNick = Message.GetParam(5);
            CString sChan = Message.GetParam(1);
            CString sIdent = Message.GetParam(2);
            CString sHost = Message.GetParam(3);

            if (sNick.Equals(GetNick())) {
                m_Nick.SetIdent(sIdent);
                m_Nick.SetHost(sHost);
            }

            m_pNetwork->SetIRCNick(m_Nick);
            m_pNetwork->SetIRCServer(sServer);

            // A nick can only have one ident and hostname. Yes, you can query
            // this information per-channel, but it is still global. For
            // example, if the client supports UHNAMES, but the IRC server does
            // not, then AFAIR "passive snooping of WHO replies" is the only way
            // that ZNC can figure out the ident and host for the UHNAMES
            // replies.
            const vector<CChan*>& vChans = m_pNetwork->GetChans();

            for (CChan* pChan : vChans) {
                pChan->OnWho(sNick, sIdent, sHost);
            }

            CChan* pChan = m_pNetwork->FindChan(sChan);
            if (pChan && pChan->IsDetached()) {
                return true;
            }

            break;
        }
        case 353: {  // NAMES
            // :irc.server.com 353 nick @ #chan :nick1 nick2
            // Todo: allow for non @+= server msgs
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(2));
            // If we don't know that channel, some client might have
            // requested a /names for it and we really should forward this.
            if (pChan) {
                CString sNicks = Message.GetParam(3);
                pChan->AddNicks(sNicks);
                if (pChan->IsDetached()) {
                    return true;
                }
            }

            break;
        }
        case 366: {  // end of names list
            // :irc.server.com 366 nick #chan :End of /NAMES list.
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));

            if (pChan) {
                if (pChan->IsOn()) {
                    // If we are the only one in the chan, set our default modes
                    if (pChan->GetNickCount() == 1) {
                        CString sModes = pChan->GetDefaultModes();

                        if (sModes.empty()) {
                            sModes =
                                m_pNetwork->GetUser()->GetDefaultChanModes();
                        }

                        if (!sModes.empty()) {
                            PutIRC("MODE " + pChan->GetName() + " " + sModes);
                        }
                    }
                }
                if (pChan->IsDetached()) {
                    // don't put it to clients
                    return true;
                }
            }

            break;
        }
        case 375:  // begin motd
        case 422:  // MOTD File is missing
            if (m_pNetwork->GetIRCServer().Equals(sServer)) {
                m_pNetwork->ClearMotdBuffer();
            }
        case 372:  // motd
        case 376:  // end motd
            if (m_pNetwork->GetIRCServer().Equals(sServer)) {
                m_pNetwork->AddMotdBuffer(BufferMessage(Message));
            }
            break;
        case 437:
            // :irc.server.net 437 * badnick :Nick/channel is temporarily unavailable
            // :irc.server.net 437 mynick badnick :Nick/channel is temporarily unavailable
            // :irc.server.net 437 mynick badnick :Cannot change nickname while banned on channel
            if (m_pNetwork->IsChan(Message.GetParam(1)) || sNick != "*") break;
        case 432:
        // :irc.server.com 432 * nick :Erroneous Nickname: Illegal chars
        case 433: {
            CString sBadNick = Message.GetParam(1);

            if (!m_bAuthed) {
                SendAltNick(sBadNick);
                return true;
            }
            break;
        }
        case 451:
            // :irc.server.com 451 CAP :You have not registered
            // Servers that don't support CAP will give us this error, don't send
            // it to the client
            if (sNick.Equals("CAP")) return true;
        case 470: {
            // :irc.unreal.net 470 mynick [Link] #chan1 has become full, so you are automatically being transferred to the linked channel #chan2
            // :mccaffrey.freenode.net 470 mynick #electronics ##electronics :Forwarding to another channel

            // freenode style numeric
            CChan* pChan = m_pNetwork->FindChan(Message.GetParam(1));
            if (!pChan) {
                // unreal style numeric
                pChan = m_pNetwork->FindChan(Message.GetParam(2));
            }
            if (pChan) {
                pChan->Disable();
                m_pNetwork->PutStatus(
                    t_f("Channel {1} is linked to another channel and was thus "
                        "disabled.")(pChan->GetName()));
            }
            break;
        }
        case 670:
            // :hydra.sector5d.org 670 kylef :STARTTLS successful, go ahead with TLS handshake
            //
            // 670 is a response to `STARTTLS` telling the client to switch to
            // TLS
            if (!GetSSL()) {
                StartTLS();
                m_pNetwork->PutStatus(t_s("Switched to SSL (STARTTLS)"));
            }

            return true;
    }

    return false;
}

bool CIRCSock::OnPartMessage(CPartMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sChan = Message.GetTarget();

    CChan* pChan = m_pNetwork->FindChan(sChan);
    bool bDetached = false;
    if (pChan) {
        pChan->RemNick(Nick.GetNick());
        Message.SetChan(pChan);
        IRCSOCKMODULECALL(OnPartMessage(Message), NOTHING);

        if (pChan->IsDetached()) bDetached = true;
    }

    if (Nick.NickEquals(GetNick())) {
        m_pNetwork->DelChan(sChan);
    }

    /*
     * We use this boolean because
     * m_pNetwork->DelChan() will delete this channel
     * and thus we would dereference an
     * already-freed pointer!
     */
    return bDetached;
}

bool CIRCSock::OnPingMessage(CMessage& Message) {
    // Generate a reply and don't forward this to any user,
    // we don't want any PING forwarded
    PutIRCQuick("PONG " + Message.GetParam(0));
    return true;
}

bool CIRCSock::OnPongMessage(CMessage& Message) {
    // Block PONGs, we already responded to the pings
    return true;
}

bool CIRCSock::OnQuitMessage(CQuitMessage& Message) {
    const CNick& Nick = Message.GetNick();
    bool bIsVisible = false;

    if (Nick.NickEquals(GetNick())) {
        m_pNetwork->PutStatus(t_f("You quit: {1}")(Message.GetReason()));
        // We don't call module hooks and we don't
        // forward this quit to clients (Some clients
        // disconnect if they receive such a QUIT)
        return true;
    }

    vector<CChan*> vFoundChans;
    const vector<CChan*>& vChans = m_pNetwork->GetChans();

    for (CChan* pChan : vChans) {
        if (pChan->RemNick(Nick.GetNick())) {
            vFoundChans.push_back(pChan);

            if (!pChan->IsDetached()) {
                bIsVisible = true;
            }
        }
    }

    IRCSOCKMODULECALL(OnQuitMessage(Message, vFoundChans), NOTHING);

    return !bIsVisible;
}

bool CIRCSock::OnTextMessage(CTextMessage& Message) {
    bool bResult = false;
    CChan* pChan = nullptr;
    CString sTarget = Message.GetTarget();

    if (sTarget.Equals(GetNick())) {
        IRCSOCKMODULECALL(OnPrivTextMessage(Message), &bResult);
        if (bResult) return true;

        if (!m_pNetwork->IsUserOnline() ||
            !m_pNetwork->GetUser()->AutoClearQueryBuffer()) {
            const CNick& Nick = Message.GetNick();
            CQuery* pQuery = m_pNetwork->AddQuery(Nick.GetNick());
            if (pQuery) {
                CTextMessage Format;
                Format.Clone(Message);
                Format.SetNick(_NAMEDFMT(Nick.GetNickMask()));
                Format.SetTarget("{target}");
                Format.SetText("{text}");
                pQuery->AddBuffer(Format, Message.GetText());
            }
        }
    } else {
        pChan = m_pNetwork->FindChan(sTarget);
        if (pChan) {
            Message.SetChan(pChan);
            FixupChanNick(Message.GetNick(), pChan);
            IRCSOCKMODULECALL(OnChanTextMessage(Message), &bResult);
            if (bResult) return true;

            if (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline() ||
                pChan->IsDetached()) {
                CTextMessage Format;
                Format.Clone(Message);
                Format.SetNick(_NAMEDFMT(Message.GetNick().GetNickMask()));
                Format.SetTarget(_NAMEDFMT(Message.GetTarget()));
                Format.SetText("{text}");
                pChan->AddBuffer(Format, Message.GetText());
            }
        }
    }

    return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnTopicMessage(CTopicMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CChan* pChan = m_pNetwork->FindChan(Message.GetParam(0));

    if (pChan) {
        Message.SetChan(pChan);
        bool bReturn = false;
        IRCSOCKMODULECALL(OnTopicMessage(Message), &bReturn);
        if (bReturn) return true;

        pChan->SetTopicOwner(Nick.GetNick());
        pChan->SetTopicDate((unsigned long)time(nullptr));
        pChan->SetTopic(Message.GetTopic());
    }

    return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnWallopsMessage(CMessage& Message) {
    // :blub!dummy@rox-8DBEFE92 WALLOPS :this is a test
    CString sMsg = Message.GetParam(0);

    if (!m_pNetwork->IsUserOnline()) {
        CMessage Format(Message);
        Format.SetNick(CNick(_NAMEDFMT(Message.GetNick().GetHostMask())));
        Format.SetParam(0, "{text}");
        m_pNetwork->AddNoticeBuffer(Format, sMsg);
    }
    return false;
}

void CIRCSock::PutIRC(const CString& sLine) {
    PutIRC(CMessage(sLine));
}

void CIRCSock::PutIRC(const CMessage& Message) {
    // Only print if the line won't get sent immediately (same condition as in
    // TrySend()!)
    if (m_bFloodProtection && m_iSendsAllowed <= 0) {
        DEBUG("(" << m_pNetwork->GetUser()->GetUserName() << "/"
                  << m_pNetwork->GetName() << ") ZNC -> IRC ["
                  << CDebug::Filter(Message.ToString()) << "] (queued)");
    }
    m_vSendQueue.push_back(Message);
    TrySend();
}

void CIRCSock::PutIRCQuick(const CString& sLine) {
    // Only print if the line won't get sent immediately (same condition as in
    // TrySend()!)
    if (m_bFloodProtection && m_iSendsAllowed <= 0) {
        DEBUG("(" << m_pNetwork->GetUser()->GetUserName() << "/"
                  << m_pNetwork->GetName() << ") ZNC -> IRC ["
                  << CDebug::Filter(sLine) << "] (queued to front)");
    }
    m_vSendQueue.emplace_front(sLine);
    TrySend();
}

void CIRCSock::TrySend() {
    // This condition must be the same as in PutIRC() and PutIRCQuick()!
    while (!m_vSendQueue.empty() &&
           (!m_bFloodProtection || m_iSendsAllowed > 0)) {
        m_iSendsAllowed--;
        CMessage& Message = m_vSendQueue.front();

        MCString mssTags;
        for (const auto& it : Message.GetTags()) {
            if (IsTagEnabled(it.first)) {
                mssTags[it.first] = it.second;
            }
        }
        Message.SetTags(mssTags);
        Message.SetNetwork(m_pNetwork);

        bool bSkip = false;
        IRCSOCKMODULECALL(OnSendToIRCMessage(Message), &bSkip);

        if (!bSkip) {
            PutIRCRaw(Message.ToString());
        }
        m_vSendQueue.pop_front();
    }
}

void CIRCSock::PutIRCRaw(const CString& sLine) {
    CString sCopy = sLine;
    bool bSkip = false;
    IRCSOCKMODULECALL(OnSendToIRC(sCopy), &bSkip);
    if (!bSkip) {
        DEBUG("(" << m_pNetwork->GetUser()->GetUserName() << "/"
                  << m_pNetwork->GetName() << ") ZNC -> IRC ["
                  << CDebug::Filter(sCopy) << "]");
        Write(sCopy + "\r\n");
    }
}

void CIRCSock::SetNick(const CString& sNick) {
    m_Nick.SetNick(sNick);
    m_pNetwork->SetIRCNick(m_Nick);
}

void CIRCSock::Connected() {
    DEBUG(GetSockName() << " == Connected()");

    CString sPass = m_sPass;
    CString sNick = m_pNetwork->GetNick();
    CString sIdent = m_pNetwork->GetIdent();
    CString sRealName = m_pNetwork->GetRealName();

    bool bReturn = false;
    IRCSOCKMODULECALL(OnIRCRegistration(sPass, sNick, sIdent, sRealName),
                      &bReturn);
    if (bReturn) return;

    PutIRC("CAP LS");

    if (!sPass.empty()) {
        PutIRC("PASS " + sPass);
    }

    PutIRC("NICK " + sNick);
    PutIRC("USER " + sIdent + " \"" + sIdent + "\" \"" + sIdent + "\" :" +
           sRealName);

    // SendAltNick() needs this
    m_Nick.SetNick(sNick);
}

void CIRCSock::Disconnected() {
    IRCSOCKMODULECALL(OnIRCDisconnected(), NOTHING);

    DEBUG(GetSockName() << " == Disconnected()");
    if (!m_pNetwork->GetUser()->IsBeingDeleted() &&
        m_pNetwork->GetIRCConnectEnabled() &&
        m_pNetwork->GetServers().size() != 0) {
        m_pNetwork->PutStatus(t_s("Disconnected from IRC. Reconnecting..."));
    }
    m_pNetwork->ClearRawBuffer();
    m_pNetwork->ClearMotdBuffer();

    ResetChans();

    // send a "reset user modes" cmd to the client.
    // otherwise, on reconnect, it might think it still
    // had user modes that it actually doesn't have.
    CString sUserMode;
    for (char cMode : m_scUserModes) {
        sUserMode += cMode;
    }
    if (!sUserMode.empty()) {
        m_pNetwork->PutUser(":" + m_pNetwork->GetIRCNick().GetNickMask() +
                            " MODE " + m_pNetwork->GetIRCNick().GetNick() +
                            " :-" + sUserMode);
    }

    // also clear the user modes in our space:
    m_scUserModes.clear();
}

void CIRCSock::SockError(int iErrno, const CString& sDescription) {
    CString sError = sDescription;

    DEBUG(GetSockName() << " == SockError(" << iErrno << " " << sError << ")");
    if (!m_pNetwork->GetUser()->IsBeingDeleted()) {
        if (GetConState() != CST_OK) {
            m_pNetwork->PutStatus(
                t_f("Cannot connect to IRC ({1}). Retrying...")(sError));
        } else {
            m_pNetwork->PutStatus(
                t_f("Disconnected from IRC ({1}). Reconnecting...")(sError));
        }
#ifdef HAVE_LIBSSL
        if (iErrno == errnoBadSSLCert) {
            // Stringify bad cert
            X509* pCert = GetX509();
            if (pCert) {
                BIO* mem = BIO_new(BIO_s_mem());
                X509_print(mem, pCert);
                X509_free(pCert);
                char* pCertStr = nullptr;
                long iLen = BIO_get_mem_data(mem, &pCertStr);
                CString sCert(pCertStr, iLen);
                BIO_free(mem);

                VCString vsCert;
                sCert.Split("\n", vsCert);
                for (const CString& s : vsCert) {
                    // It shouldn't contain any bad characters, but let's be
                    // safe...
                    m_pNetwork->PutStatus("|" + s.Escape_n(CString::EDEBUG));
                }
                CString sSHA1;
                if (GetPeerFingerprint(sSHA1))
                    m_pNetwork->PutStatus(
                        "SHA1: " +
                        sSHA1.Escape_n(CString::EHEXCOLON, CString::EHEXCOLON));
                CString sSHA256 = GetSSLPeerFingerprint();
                m_pNetwork->PutStatus("SHA-256: " + sSHA256);
                m_pNetwork->PutStatus(
                    t_f("If you trust this certificate, do /znc "
                        "AddTrustedServerFingerprint {1}")(sSHA256));
            }
        }
#endif
    }
    m_pNetwork->ClearRawBuffer();
    m_pNetwork->ClearMotdBuffer();

    ResetChans();
    m_scUserModes.clear();
}

void CIRCSock::Timeout() {
    DEBUG(GetSockName() << " == Timeout()");
    if (!m_pNetwork->GetUser()->IsBeingDeleted()) {
        m_pNetwork->PutStatus(
            t_s("IRC connection timed out.  Reconnecting..."));
    }
    m_pNetwork->ClearRawBuffer();
    m_pNetwork->ClearMotdBuffer();

    ResetChans();
    m_scUserModes.clear();
}

void CIRCSock::ConnectionRefused() {
    DEBUG(GetSockName() << " == ConnectionRefused()");
    if (!m_pNetwork->GetUser()->IsBeingDeleted()) {
        m_pNetwork->PutStatus(t_s("Connection Refused.  Reconnecting..."));
    }
    m_pNetwork->ClearRawBuffer();
    m_pNetwork->ClearMotdBuffer();
}

void CIRCSock::ReachedMaxBuffer() {
    DEBUG(GetSockName() << " == ReachedMaxBuffer()");
    m_pNetwork->PutStatus(t_s("Received a too long line from the IRC server!"));
    Quit();
}

void CIRCSock::ParseISupport(const CMessage& Message) {
    const VCString vsParams = Message.GetParams();

    for (size_t i = 1; i + 1 < vsParams.size(); ++i) {
        const CString& sParam = vsParams[i];
        CString sName = sParam.Token(0, false, "=");
        CString sValue = sParam.Token(1, true, "=");

        if (0 < sName.length() && ':' == sName[0]) {
            break;
        }

        m_mISupport[sName] = sValue;

        if (sName.Equals("PREFIX")) {
            CString sPrefixes = sValue.Token(1, false, ")");
            CString sPermModes = sValue.Token(0, false, ")");
            sPermModes.TrimLeft("(");

            if (!sPrefixes.empty() && sPermModes.size() == sPrefixes.size()) {
                m_sPerms = sPrefixes;
                m_sPermModes = sPermModes;
            }
        } else if (sName.Equals("CHANTYPES")) {
            m_pNetwork->SetChanPrefixes(sValue);
        } else if (sName.Equals("NICKLEN")) {
            unsigned int uMax = sValue.ToUInt();

            if (uMax) {
                m_uMaxNickLen = uMax;
            }
        } else if (sName.Equals("CHANMODES")) {
            if (!sValue.empty()) {
                m_mceChanModes.clear();

                for (unsigned int a = 0; a < 4; a++) {
                    CString sModes = sValue.Token(a, false, ",");

                    for (unsigned int b = 0; b < sModes.size(); b++) {
                        m_mceChanModes[sModes[b]] = (EChanModeArgs)a;
                    }
                }
            }
        } else if (sName.Equals("NAMESX")) {
            if (m_bNamesx) continue;
            m_bNamesx = true;
            PutIRC("PROTOCTL NAMESX");
        } else if (sName.Equals("UHNAMES")) {
            if (m_bUHNames) continue;
            m_bUHNames = true;
            PutIRC("PROTOCTL UHNAMES");
        }
    }
}

CString CIRCSock::GetISupport(const CString& sKey,
                              const CString& sDefault) const {
    MCString::const_iterator i = m_mISupport.find(sKey.AsUpper());
    if (i == m_mISupport.end()) {
        return sDefault;
    } else {
        return i->second;
    }
}

void CIRCSock::SendAltNick(const CString& sBadNick) {
    const CString& sLastNick = m_Nick.GetNick();

    // We don't know the maximum allowed nick length yet, but we know which
    // nick we sent last. If sBadNick is shorter than that, we assume the
    // server truncated our nick.
    if (sBadNick.length() < sLastNick.length())
        m_uMaxNickLen = (unsigned int)sBadNick.length();

    unsigned int uMax = m_uMaxNickLen;

    const CString& sConfNick = m_pNetwork->GetNick();
    const CString& sAltNick = m_pNetwork->GetAltNick();
    CString sNewNick = sConfNick.Left(uMax - 1);

    if (sLastNick.Equals(sConfNick)) {
        if ((!sAltNick.empty()) && (!sConfNick.Equals(sAltNick))) {
            sNewNick = sAltNick;
        } else {
            sNewNick += "-";
        }
    } else if (sLastNick.Equals(sAltNick) && !sAltNick.Equals(sNewNick + "-")) {
        sNewNick += "-";
    } else if (sLastNick.Equals(sNewNick + "-") &&
               !sAltNick.Equals(sNewNick + "|")) {
        sNewNick += "|";
    } else if (sLastNick.Equals(sNewNick + "|") &&
               !sAltNick.Equals(sNewNick + "^")) {
        sNewNick += "^";
    } else if (sLastNick.Equals(sNewNick + "^") &&
               !sAltNick.Equals(sNewNick + "a")) {
        sNewNick += "a";
    } else {
        char cLetter = 0;
        if (sBadNick.empty()) {
            m_pNetwork->PutUser(t_s("No free nick available"));
            Quit();
            return;
        }

        cLetter = sBadNick.back();

        if (cLetter == 'z') {
            m_pNetwork->PutUser(t_s("No free nick found"));
            Quit();
            return;
        }

        sNewNick = sConfNick.Left(uMax - 1) + ++cLetter;
        if (sNewNick.Equals(sAltNick))
            sNewNick = sConfNick.Left(uMax - 1) + ++cLetter;
    }
    PutIRC("NICK " + sNewNick);
    m_Nick.SetNick(sNewNick);
}

char CIRCSock::GetPermFromMode(char cMode) const {
    if (m_sPermModes.size() == m_sPerms.size()) {
        for (unsigned int a = 0; a < m_sPermModes.size(); a++) {
            if (m_sPermModes[a] == cMode) {
                return m_sPerms[a];
            }
        }
    }

    return 0;
}

CIRCSock::EChanModeArgs CIRCSock::GetModeType(char cMode) const {
    map<char, EChanModeArgs>::const_iterator it =
        m_mceChanModes.find(cMode);

    if (it == m_mceChanModes.end()) {
        return NoArg;
    }

    return it->second;
}

void CIRCSock::ResetChans() {
    for (const auto& it : m_msChans) {
        it.second->Reset();
    }
}

void CIRCSock::SetTagSupport(const CString& sTag, bool bState) {
    if (bState) {
        m_ssSupportedTags.insert(sTag);
    } else {
        m_ssSupportedTags.erase(sTag);
    }
}

