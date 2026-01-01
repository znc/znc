/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
 * Copyright (C) 2006-2007, CNU <bshalm@broadpark.no>
 *(http://cnu.dieplz.net/znc)
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

#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Server.h>
#include <time.h>
#include <algorithm>

using std::vector;

class CLogRule {
  public:
    CLogRule(const CString& sRule, bool bEnabled = true)
        : m_sRule(sRule), m_bEnabled(bEnabled) {}

    const CString& GetRule() const { return m_sRule; }
    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

    bool Compare(const CString& sTarget) const {
        return sTarget.WildCmp(m_sRule, CString::CaseInsensitive);
    }

    bool operator==(const CLogRule& sOther) const {
        return m_sRule == sOther.GetRule();
    }

    CString ToString() const { return (m_bEnabled ? "" : "!") + m_sRule; }

  private:
    CString m_sRule;
    bool m_bEnabled;
};

class CLogMod : public CModule {
  public:
    MODCONSTRUCTOR(CLogMod) {
        m_bSanitize = false;
        AddHelpCommand();
        AddCommand(
            "SetRules", t_d("<rules>"),
            t_d("Set logging rules, use !#chan or !query to negate and * "),
            [=](const CString& sLine) { SetRulesCmd(sLine); });
        AddCommand("ClearRules", "", t_d("Clear all logging rules"),
                   [=](const CString& sLine) { ClearRulesCmd(sLine); });
        AddCommand("ListRules", "", t_d("List all logging rules"),
                   [=](const CString& sLine) { ListRulesCmd(sLine); });
        AddCommand(
            "Set", t_d("<var> true|false"),
            t_d("Set one of the following options: joins, quits, nickchanges"),
            [=](const CString& sLine) { SetCmd(sLine); });
        AddCommand("ShowSettings", "",
                   t_d("Show current settings set by Set command"),
                   [=](const CString& sLine) { ShowSettingsCmd(sLine); });
    }

    void SetRulesCmd(const CString& sLine);
    void ClearRulesCmd(const CString& sLine);
    void ListRulesCmd(const CString& sLine = "");
    void SetCmd(const CString& sLine);
    void ShowSettingsCmd(const CString& sLine);

    void SetRules(const VCString& vsRules);
    VCString SplitRules(const CString& sRules) const;
    CString JoinRules(const CString& sSeparator) const;
    bool TestRules(const CString& sTarget) const;

    void PutLog(const CString& sLine, const CString& sWindow = "status");
    void PutLog(const CString& sLine, const CChan& Channel);
    void PutLog(const CString& sLine, const CNick& Nick);
    CString GetServer();

    bool OnLoad(const CString& sArgs, CString& sMessage) override;
    void OnIRCConnected() override;
    void OnIRCDisconnected() override;
    EModRet OnBroadcast(CString& sMessage) override;

    void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes,
                    const CString& sArgs) override;
    void OnKickMessage(CKickMessage& Message) override;
    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override;
    void LogQuitHelper(const CNick& Nick, const CString& sMessage,
                       const vector<CChan*>& vChans);

    void OnJoinMessage(CJoinMessage& Message) override;
    void OnPartMessage(CPartMessage& Message) override;
    void OnNickMessage(CNickMessage& Message,
                       const vector<CChan*>& vChans) override;
    EModRet OnTopicMessage(CTopicMessage& Message) override;

    EModRet OnSendToIRCMessage(CMessage& Message) override;

    /* notices */
    EModRet OnUserNoticeMessage(CNoticeMessage& Message) override;
    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override;
    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override;

    /* actions */
    EModRet OnUserActionMessage(CActionMessage& Message) override;
    EModRet OnPrivActionMessage(CActionMessage& Message) override;
    EModRet OnChanActionMessage(CActionMessage& Message) override;

    /* msgs */
    EModRet OnUserTextMessage(CTextMessage& Message) override;
    EModRet OnPrivTextMessage(CTextMessage& Message) override;
    EModRet OnChanTextMessage(CTextMessage& Message) override;

  private:
    bool NeedJoins() const;
    bool NeedQuits() const;
    bool NeedNickChanges() const;

    CString m_sLogPath;
    CString m_sTimestamp;
    bool m_bSanitize;
    vector<CLogRule> m_vRules;
};

void CLogMod::SetRulesCmd(const CString& sLine) {
    VCString vsRules = SplitRules(sLine.Token(1, true));

    if (vsRules.empty()) {
        PutModule(t_s("Usage: SetRules <rules>"));
        PutModule(t_s("Wildcards are allowed"));
    } else {
        SetRules(vsRules);
        SetNV("rules", JoinRules(","));
        ListRulesCmd();
    }
}

void CLogMod::ClearRulesCmd(const CString& sLine) {
    size_t uCount = m_vRules.size();

    if (uCount == 0) {
        PutModule(t_s("No logging rules. Everything is logged."));
    } else {
        CString sRules = JoinRules(" ");
        SetRules(VCString());
        DelNV("rules");
        PutModule(t_p("1 rule removed: {2}", "{1} rules removed: {2}", uCount)(
            uCount, sRules));
    }
}

void CLogMod::ListRulesCmd(const CString& sLine) {
    CTable Table;
    Table.AddColumn(t_s("Rule", "listrules"));
    Table.AddColumn(t_s("Logging enabled", "listrules"));
    Table.SetStyle(CTable::ListStyle);

    for (const CLogRule& Rule : m_vRules) {
        Table.AddRow();
        Table.SetCell(t_s("Rule", "listrules"), Rule.GetRule());
        Table.SetCell(t_s("Logging enabled", "listrules"), CString(Rule.IsEnabled()));
    }

    if (Table.empty()) {
        PutModule(t_s("No logging rules. Everything is logged."));
    } else {
        PutModule(Table);
    }
}

void CLogMod::SetCmd(const CString& sLine) {
    const CString sVar = sLine.Token(1).AsLower();
    const CString sValue = sLine.Token(2);
    if (sValue.empty()) {
        PutModule(
            t_s("Usage: Set <var> true|false, where <var> is one of: joins, "
                "quits, nickchanges"));
        return;
    }
    bool b = sLine.Token(2).ToBool();
    const std::unordered_map<CString, std::pair<CString, CString>>
        mssResponses = {
            {"joins", {t_s("Will log joins"), t_s("Will not log joins")}},
            {"quits", {t_s("Will log quits"), t_s("Will not log quits")}},
            {"nickchanges",
             {t_s("Will log nick changes"), t_s("Will not log nick changes")}}};
    auto it = mssResponses.find(sVar);
    if (it == mssResponses.end()) {
        PutModule(t_s(
            "Unknown variable. Known variables: joins, quits, nickchanges"));
        return;
    }
    SetNV(sVar, CString(b));
    PutModule(b ? it->second.first : it->second.second);
}

void CLogMod::ShowSettingsCmd(const CString& sLine) {
    PutModule(NeedJoins() ? t_s("Logging joins") : t_s("Not logging joins"));
    PutModule(NeedQuits() ? t_s("Logging quits") : t_s("Not logging quits"));
    PutModule(NeedNickChanges() ? t_s("Logging nick changes")
                                : t_s("Not logging nick changes"));
}

bool CLogMod::NeedJoins() const {
    return !HasNV("joins") || GetNV("joins").ToBool();
}

bool CLogMod::NeedQuits() const {
    return !HasNV("quits") || GetNV("quits").ToBool();
}

bool CLogMod::NeedNickChanges() const {
    return !HasNV("nickchanges") || GetNV("nickchanges").ToBool();
}

void CLogMod::SetRules(const VCString& vsRules) {
    m_vRules.clear();

    for (CString sRule : vsRules) {
        bool bEnabled = !sRule.TrimPrefix("!");
        m_vRules.push_back(CLogRule(sRule, bEnabled));
    }
}

VCString CLogMod::SplitRules(const CString& sRules) const {
    CString sCopy = sRules;
    sCopy.Replace(",", " ");

    VCString vsRules;
    sCopy.Split(" ", vsRules, false, "", "", true, true);

    return vsRules;
}

CString CLogMod::JoinRules(const CString& sSeparator) const {
    VCString vsRules;
    for (const CLogRule& Rule : m_vRules) {
        vsRules.push_back(Rule.ToString());
    }

    return sSeparator.Join(vsRules.begin(), vsRules.end());
}

bool CLogMod::TestRules(const CString& sTarget) const {
    for (const CLogRule& Rule : m_vRules) {
        if (Rule.Compare(sTarget)) {
            return Rule.IsEnabled();
        }
    }

    return true;
}

void CLogMod::PutLog(const CString& sLine,
                     const CString& sWindow /*= "Status"*/) {
    if (!TestRules(sWindow)) {
        return;
    }

    CString sPath;
    timeval curtime;

    gettimeofday(&curtime, nullptr);
    // Generate file name
    sPath = CUtils::FormatTime(curtime, m_sLogPath, GetUser()->GetTimezone());
    if (sPath.empty()) {
        DEBUG("Could not format log path [" << sPath << "]");
        return;
    }

    // TODO: Properly handle IRC case mapping
    // $WINDOW has to be handled last, since it can contain %
    sPath.Replace("$USER",
                  CString((GetUser() ? GetUser()->GetUsername() : "UNKNOWN")));
    sPath.Replace("$NETWORK",
                  CString((GetNetwork() ? GetNetwork()->GetName() : "znc")));
    sPath.Replace("$WINDOW", CString(sWindow.Replace_n("/", "-")
                                         .Replace_n("\\", "-")).AsLower());

    // Check if it's allowed to write in this specific path
    sPath = CDir::CheckPathPrefix(GetSavePath(), sPath);
    if (sPath.empty()) {
        DEBUG("Invalid log path [" << m_sLogPath << "].");
        return;
    }

    CFile LogFile(sPath);
    CString sLogDir = LogFile.GetDir();
    struct stat ModDirInfo;
    CFile::GetInfo(GetSavePath(), ModDirInfo);
    if (!CFile::Exists(sLogDir)) CDir::MakeDir(sLogDir, ModDirInfo.st_mode);
    if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT)) {
        LogFile.Write(CUtils::FormatTime(curtime, m_sTimestamp,
                                         GetUser()->GetTimezone()) +
                      " " + (m_bSanitize ? sLine.StripControls_n() : sLine) +
                      "\n");
    } else
        DEBUG("Could not open log file [" << sPath << "]: " << strerror(errno));
}

void CLogMod::PutLog(const CString& sLine, const CChan& Channel) {
    PutLog(sLine, Channel.GetName());
}

void CLogMod::PutLog(const CString& sLine, const CNick& Nick) {
    PutLog(sLine, Nick.GetNick());
}

CString CLogMod::GetServer() {
    CServer* pServer = GetNetwork()->GetCurrentServer();
    CString sSSL;

    if (!pServer) return "(no server)";

    if (pServer->IsSSL()) sSSL = "+";
    return pServer->GetName() + " " + sSSL + CString(pServer->GetPort());
}

bool CLogMod::OnLoad(const CString& sArgs, CString& sMessage) {
    VCString vsArgs;
    sArgs.QuoteSplit(vsArgs);

    bool bReadingTimestamp = false;
    bool bHaveLogPath = false;

    for (CString& sArg : vsArgs) {
        if (bReadingTimestamp) {
            m_sTimestamp = sArg;
            bReadingTimestamp = false;
        } else if (sArg.Equals("-sanitize")) {
            m_bSanitize = true;
        } else if (sArg.Equals("-timestamp")) {
            bReadingTimestamp = true;
        } else {
            // Only one arg may be LogPath
            if (bHaveLogPath) {
                sMessage =
                    t_f("Invalid args [{1}]. Only one log path allowed.  Check "
                        "that there are no spaces in the path.")(sArgs);
                return false;
            }
            m_sLogPath = sArg;
            bHaveLogPath = true;
        }
    }

    if (m_sTimestamp.empty()) {
        m_sTimestamp = "[%H:%M:%S]";
    }

    // Add default filename to path if it's a folder
    if (GetType() == CModInfo::UserModule) {
        if (m_sLogPath.Right(1) == "/" ||
            m_sLogPath.find("$WINDOW") == CString::npos ||
            m_sLogPath.find("$NETWORK") == CString::npos) {
            if (!m_sLogPath.empty()) {
                m_sLogPath += "/";
            }
            m_sLogPath += "$NETWORK/$WINDOW/%Y-%m-%d.log";
        }
    } else if (GetType() == CModInfo::NetworkModule) {
        if (m_sLogPath.Right(1) == "/" ||
            m_sLogPath.find("$WINDOW") == CString::npos) {
            if (!m_sLogPath.empty()) {
                m_sLogPath += "/";
            }
            m_sLogPath += "$WINDOW/%Y-%m-%d.log";
        }
    } else {
        if (m_sLogPath.Right(1) == "/" ||
            m_sLogPath.find("$USER") == CString::npos ||
            m_sLogPath.find("$WINDOW") == CString::npos ||
            m_sLogPath.find("$NETWORK") == CString::npos) {
            if (!m_sLogPath.empty()) {
                m_sLogPath += "/";
            }
            m_sLogPath += "$USER/$NETWORK/$WINDOW/%Y-%m-%d.log";
        }
    }

    CString sRules = GetNV("rules");
    VCString vsRules = SplitRules(sRules);
    SetRules(vsRules);

    // Check if it's allowed to write in this path in general
    m_sLogPath = CDir::CheckPathPrefix(GetSavePath(), m_sLogPath);
    if (m_sLogPath.empty()) {
        sMessage = t_f("Invalid log path [{1}]")(m_sLogPath);
        return false;
    } else {
        sMessage = t_f("Logging to [{1}]. Using timestamp format '{2}'")(
            m_sLogPath, m_sTimestamp);
        return true;
    }
}

// TODO consider writing translated strings to log. Currently user language
// affects only UI.
void CLogMod::OnIRCConnected() {
    PutLog("Connected to IRC (" + GetServer() + ")");
}

void CLogMod::OnIRCDisconnected() {
    PutLog("Disconnected from IRC (" + GetServer() + ")");
}

CModule::EModRet CLogMod::OnBroadcast(CString& sMessage) {
    PutLog("Broadcast: " + sMessage);
    return CONTINUE;
}

void CLogMod::OnRawMode2(const CNick* pOpNick, CChan& Channel,
                         const CString& sModes, const CString& sArgs) {
    const CString sNick = pOpNick ? pOpNick->GetNick() : "Server";
    PutLog("*** " + sNick + " sets mode: " + sModes + " " + sArgs, Channel);
}

void CLogMod::OnKickMessage(CKickMessage& Message) {
    const CNick& OpNick = Message.GetNick();
    const CString sKickedNick = Message.GetKickedNick();
    CChan& Channel = *Message.GetChan();
    const CString sMessage = Message.GetReason();
    PutLog("*** " + sKickedNick + " was kicked by " + OpNick.GetNick() + " (" +
               sMessage + ")",
           Channel);
}

void CLogMod::LogQuitHelper(const CNick& Nick, const CString& sMessage,
                           const vector<CChan*>& vChans) {
    if (NeedQuits()) {
        for (CChan* pChan : vChans) {
            // Core calls this only for enabled channels, but
            // OnSendToIRCMessage() below calls OnQuitMessage() for all channels.
            if (pChan->IsDisabled()) continue;
            PutLog("*** Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() +
                       "@" + Nick.GetHost() + ") (" + sMessage + ")",
                   *pChan);
        }
    }
}

void CLogMod::OnQuitMessage(CQuitMessage& Message,
                            const vector<CChan*>& vChans) {
    LogQuitHelper(Message.GetNick(), Message.GetReason(), vChans);
}

CModule::EModRet CLogMod::OnSendToIRCMessage(CMessage& Message) {
    if (Message.GetType() != CMessage::Type::Quit) {
        return CONTINUE;
    }
    CIRCNetwork* pNetwork = Message.GetNetwork();
    CQuitMessage& QuitMsg = Message.As<CQuitMessage>();

    LogQuitHelper(pNetwork->GetIRCNick(), QuitMsg.GetReason(), pNetwork->GetChans());

    return CONTINUE;
}


void CLogMod::OnJoinMessage(CJoinMessage& Message) {
    if (!NeedJoins())
        return;

    const CNick& Nick = Message.GetNick();
    CChan& Channel = *Message.GetChan();

    // TODO: Move account logic to a separate Message method.
    CString sAccount = Message.GetTag("account");
    const char* s = " ";

    if (sAccount.empty())
        sAccount = Message.GetParam(1);

    if (sAccount.empty() || sAccount == "*") {
        sAccount = "";
        s = "";
    }

    PutLog("*** Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
           Nick.GetHost() + ")" + s + sAccount,
           Channel);
}

void CLogMod::OnPartMessage(CPartMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CChan& Channel = *Message.GetChan();
    const CString sMessage = Message.GetReason();
    PutLog("*** Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
               Nick.GetHost() + ") (" + sMessage + ")",
           Channel);
}

void CLogMod::OnNickMessage(CNickMessage& Message,
                            const vector<CChan*>& vChans) {
    const CNick& OldNick = Message.GetNick();
    const CString sNewNick = Message.GetNewNick();
    if (NeedNickChanges()) {
        for (CChan* pChan : vChans)
            PutLog("*** " + OldNick.GetNick() + " is now known as " + sNewNick,
                   *pChan);
    }
}

CModule::EModRet CLogMod::OnTopicMessage(CTopicMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CChan& Channel = *Message.GetChan();
    CString sTopic = Message.GetTopic();
    PutLog("*** " + Nick.GetNick() + " changes topic to '" + sTopic + "'",
           Channel);
    return CONTINUE;
}

/* notices */
CModule::EModRet CLogMod::OnUserNoticeMessage(CNoticeMessage& Message) {
    CIRCNetwork* pNetwork = GetNetwork();
    CString sTarget = Message.GetTarget();
    CString sMessage = Message.GetText();
    if (pNetwork) {
        PutLog("-" + pNetwork->GetCurNick() + "- " + sMessage, sTarget);
    }

    return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivNoticeMessage(CNoticeMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    PutLog("-" + Nick.GetNick() + "- " + sMessage, Nick);
    return CONTINUE;
}

CModule::EModRet CLogMod::OnChanNoticeMessage(CNoticeMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    CChan& Channel = *Message.GetChan();
    PutLog("-" + Nick.GetNick() + "- " + sMessage, Channel);
    return CONTINUE;
}

/* actions */
CModule::EModRet CLogMod::OnUserActionMessage(CActionMessage& Message) {
    CIRCNetwork* pNetwork = GetNetwork();
    CString sMessage = Message.GetText();
    CString sTarget = Message.GetTarget();
    if (pNetwork) {
        PutLog("* " + pNetwork->GetCurNick() + " " + sMessage, sTarget);
    }

    return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivActionMessage(CActionMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    PutLog("* " + Nick.GetNick() + " " + sMessage, Nick);
    return CONTINUE;
}

CModule::EModRet CLogMod::OnChanActionMessage(CActionMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    CChan& Channel = *Message.GetChan();
    PutLog("* " + Nick.GetNick() + " " + sMessage, Channel);
    return CONTINUE;
}

/* msgs */
CModule::EModRet CLogMod::OnUserTextMessage(CTextMessage& Message) {
    CIRCNetwork* pNetwork = GetNetwork();
    CString sTarget = Message.GetTarget();
    CString sMessage = Message.GetText();
    if (pNetwork) {
        PutLog("<" + pNetwork->GetCurNick() + "> " + sMessage, sTarget);
    }

    return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivTextMessage(CTextMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    PutLog("<" + Nick.GetNick() + "> " + sMessage, Nick);
    return CONTINUE;
}

CModule::EModRet CLogMod::OnChanTextMessage(CTextMessage& Message) {
    const CNick& Nick = Message.GetNick();
    CString sMessage = Message.GetText();
    CChan& Channel = *Message.GetChan();
    PutLog("<" + Nick.GetNick() + "> " + sMessage, Channel);
    return CONTINUE;
}

template <>
void TModInfo<CLogMod>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
    Info.AddType(CModInfo::GlobalModule);
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("[-sanitize] Optional path where to store logs."));
    Info.SetWikiPage("log");
}

USERMODULEDEFS(CLogMod, t_s("Writes IRC logs."))
