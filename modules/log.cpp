/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 * Copyright (C) 2006-2007, CNU <bshalm@broadpark.no> (http://cnu.dieplz.net/znc)
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
#include <algorithm>

using std::vector;

class CLogRule {
public:
	CLogRule(const CString& sRule, bool bEnabled = true) : m_sRule(sRule), m_bEnabled(bEnabled) {}

	const CString& GetRule() const { return m_sRule; }
	bool IsEnabled() const { return m_bEnabled; }
	void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

	bool Compare(const CString& sTarget) const {
		return sTarget.WildCmp(m_sRule, CString::CaseInsensitive);
	}

	bool operator==(const CLogRule& sOther) const {
		return m_sRule == sOther.GetRule();
	}

	CString ToString() const {
		return (m_bEnabled ? "" : "!") + m_sRule;
	}

private:
	CString m_sRule;
	bool m_bEnabled;
};

class CLogMod: public CModule {
public:
	MODCONSTRUCTOR(CLogMod)
	{
		m_bSanitize = false;
		AddHelpCommand();
		AddCommand("SetRules", static_cast<CModCommand::ModCmdFunc>(&CLogMod::SetRulesCmd),
				   "<rules>", "Set logging rules, use !#chan or !query to negate and * for wildcards");
		AddCommand("ClearRules", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ClearRulesCmd),
				   "", "Clear all logging rules");
		AddCommand("ListRules", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ListRulesCmd),
				   "", "List all logging rules");
	}

	void SetRulesCmd(const CString& sLine);
	void ClearRulesCmd(const CString& sLine);
	void ListRulesCmd(const CString& sLine = "");
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

	void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override;
	void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) override;
	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override;
	void OnJoin(const CNick& Nick, CChan& Channel) override;
	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override;
	void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) override;
	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override;

	/* notices */
	EModRet OnUserNotice(CString& sTarget, CString& sMessage) override;
	EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override;
	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override;

	/* actions */
	EModRet OnUserAction(CString& sTarget, CString& sMessage) override;
	EModRet OnPrivAction(CNick& Nick, CString& sMessage) override;
	EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override;

	/* msgs */
	EModRet OnUserMsg(CString& sTarget, CString& sMessage) override;
	EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override;
	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;

private:
	CString                 m_sLogPath;
	bool                    m_bSanitize;
	vector<CLogRule>        m_vRules;
};

void CLogMod::SetRulesCmd(const CString& sLine)
{
	VCString vsRules = SplitRules(sLine.Token(1, true));

	if (vsRules.empty()) {
		PutModule("Usage: SetRules <rules>");
		PutModule("Wildcards are allowed");
	} else {
		SetRules(vsRules);
		SetNV("rules", JoinRules(","));
		ListRulesCmd();
	}
}

void CLogMod::ClearRulesCmd(const CString& sLine)
{
	size_t uCount = m_vRules.size();

	if (uCount == 0) {
		PutModule("No logging rules. Everything is logged.");
	} else {
		CString sRules = JoinRules(" ");
		SetRules(VCString());
		DelNV("rules");
		PutModule(CString(uCount) + " rule(s) removed: " + sRules);
	}
}

void CLogMod::ListRulesCmd(const CString& sLine)
{
	CTable Table;
	Table.AddColumn("Rule");
	Table.AddColumn("Logging enabled");

	for (const CLogRule& Rule : m_vRules) {
		Table.AddRow();
		Table.SetCell("Rule", Rule.GetRule());
		Table.SetCell("Logging enabled", CString(Rule.IsEnabled()));
	}

	if (Table.empty()) {
		PutModule("No logging rules. Everything is logged.");
	} else {
		PutModule(Table);
	}
}

void CLogMod::SetRules(const VCString& vsRules)
{
	m_vRules.clear();

	for (CString sRule : vsRules) {
		bool bEnabled = !sRule.TrimPrefix("!");
		m_vRules.push_back(CLogRule(sRule, bEnabled));
	}
}

VCString CLogMod::SplitRules(const CString& sRules) const
{
	CString sCopy = sRules;
	sCopy.Replace(",", " ");

	VCString vsRules;
	sCopy.Split(" ", vsRules, false, "", "", true, true);

	return vsRules;
}

CString CLogMod::JoinRules(const CString& sSeparator) const
{
	VCString vsRules;
	for (const CLogRule& Rule : m_vRules) {
		vsRules.push_back(Rule.ToString());
	}

	return sSeparator.Join(vsRules.begin(), vsRules.end());
}

bool CLogMod::TestRules(const CString& sTarget) const
{
	for (const CLogRule& Rule : m_vRules) {
		if (Rule.Compare(sTarget)) {
			return Rule.IsEnabled();
		}
	}

	return true;
}

void CLogMod::PutLog(const CString& sLine, const CString& sWindow /*= "Status"*/)
{
	if (!TestRules(sWindow)) {
		return;
	}

	CString sPath;
	time_t curtime;

	time(&curtime);
	// Generate file name
	sPath = CUtils::FormatTime(curtime, m_sLogPath, GetUser()->GetTimezone());
	if (sPath.empty())
	{
		DEBUG("Could not format log path [" << sPath << "]");
		return;
	}

	// TODO: Properly handle IRC case mapping
	// $WINDOW has to be handled last, since it can contain %
	sPath.Replace("$USER", CString((GetUser() ? GetUser()->GetUserName() : "UNKNOWN")).AsLower());
	sPath.Replace("$NETWORK", CString((GetNetwork() ? GetNetwork()->GetName() : "znc")).AsLower());
	sPath.Replace("$WINDOW", CString(sWindow.Replace_n("/", "-").Replace_n("\\", "-")).AsLower());

	// Check if it's allowed to write in this specific path
	sPath = CDir::CheckPathPrefix(GetSavePath(), sPath);
	if (sPath.empty())
	{
		DEBUG("Invalid log path ["<<m_sLogPath<<"].");
		return;
	}

	CFile LogFile(sPath);
	CString sLogDir = LogFile.GetDir();
	struct stat ModDirInfo;
	CFile::GetInfo(GetSavePath(), ModDirInfo);
	if (!CFile::Exists(sLogDir)) CDir::MakeDir(sLogDir, ModDirInfo.st_mode);
	if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT))
	{
		LogFile.Write(CUtils::FormatTime(curtime, "[%H:%M:%S] ", GetUser()->GetTimezone()) + (m_bSanitize ? sLine.StripControls_n() : sLine) + "\n");
	} else
		DEBUG("Could not open log file [" << sPath << "]: " << strerror(errno));
}

void CLogMod::PutLog(const CString& sLine, const CChan& Channel)
{
	PutLog(sLine, Channel.GetName());
}

void CLogMod::PutLog(const CString& sLine, const CNick& Nick)
{
	PutLog(sLine, Nick.GetNick());
}

CString CLogMod::GetServer()
{
	CServer* pServer = GetNetwork()->GetCurrentServer();
	CString sSSL;

	if (!pServer)
		return "(no server)";

	if (pServer->IsSSL())
		sSSL = "+";
	return pServer->GetName() + " " + sSSL + CString(pServer->GetPort());
}

bool CLogMod::OnLoad(const CString& sArgs, CString& sMessage)
{
	size_t uIndex = 0;
	if (sArgs.Token(0).Equals("-sanitize"))
	{
		m_bSanitize = true;
		++uIndex;
	}

	// Use load parameter as save path
	m_sLogPath = sArgs.Token(uIndex);

	// Add default filename to path if it's a folder
	if (GetType() == CModInfo::UserModule) {
		if (m_sLogPath.Right(1) == "/" || m_sLogPath.find("$WINDOW") == CString::npos || m_sLogPath.find("$NETWORK") == CString::npos) {
			if (!m_sLogPath.empty()) {
				m_sLogPath += "/";
			}
			m_sLogPath += "$NETWORK/$WINDOW/%Y-%m-%d.log";
		}
	} else if (GetType() == CModInfo::NetworkModule) {
		if (m_sLogPath.Right(1) == "/" || m_sLogPath.find("$WINDOW") == CString::npos) {
			if (!m_sLogPath.empty()) {
				m_sLogPath += "/";
			}
			m_sLogPath += "$WINDOW/%Y-%m-%d.log";
		}
	} else {
		if (m_sLogPath.Right(1) == "/" || m_sLogPath.find("$USER") == CString::npos || m_sLogPath.find("$WINDOW") == CString::npos || m_sLogPath.find("$NETWORK") == CString::npos) {
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
	if (m_sLogPath.empty())
	{
		sMessage = "Invalid log path ["+m_sLogPath+"].";
		return false;
	} else {
		sMessage = "Logging to ["+m_sLogPath+"].";
		return true;
	}
}


void CLogMod::OnIRCConnected()
{
	PutLog("Connected to IRC (" + GetServer() + ")");
}

void CLogMod::OnIRCDisconnected()
{
	PutLog("Disconnected from IRC (" + GetServer() + ")");
}

CModule::EModRet CLogMod::OnBroadcast(CString& sMessage)
{
	PutLog("Broadcast: " + sMessage);
	return CONTINUE;
}

void CLogMod::OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs)
{
	const CString sNick = pOpNick ? pOpNick->GetNick() : "Server";
	PutLog("*** " + sNick + " sets mode: " + sModes + " " + sArgs, Channel);
}

void CLogMod::OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage)
{
	PutLog("*** " + sKickedNick + " was kicked by " + OpNick.GetNick() + " (" + sMessage + ")", Channel);
}

void CLogMod::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans)
{
	for (std::vector<CChan*>::const_iterator pChan = vChans.begin(); pChan != vChans.end(); ++pChan)
		PutLog("*** Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") (" + sMessage + ")", **pChan);
}

void CLogMod::OnJoin(const CNick& Nick, CChan& Channel)
{
	PutLog("*** Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ")", Channel);
}

void CLogMod::OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage)
{
	PutLog("*** Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") (" + sMessage + ")", Channel);
}

void CLogMod::OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans)
{
	for (std::vector<CChan*>::const_iterator pChan = vChans.begin(); pChan != vChans.end(); ++pChan)
		PutLog("*** " + OldNick.GetNick() + " is now known as " + sNewNick, **pChan);
}

CModule::EModRet CLogMod::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic)
{
	PutLog("*** " + Nick.GetNick() + " changes topic to '" + sTopic + "'", Channel);
	return CONTINUE;
}

/* notices */
CModule::EModRet CLogMod::OnUserNotice(CString& sTarget, CString& sMessage)
{
	CIRCNetwork* pNetwork = GetNetwork();
	if (pNetwork) {
		PutLog("-" + pNetwork->GetCurNick() + "- " + sMessage, sTarget);
	}

	return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivNotice(CNick& Nick, CString& sMessage)
{
	PutLog("-" + Nick.GetNick() + "- " + sMessage, Nick);
	return CONTINUE;
}

CModule::EModRet CLogMod::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage)
{
	PutLog("-" + Nick.GetNick() + "- " + sMessage, Channel);
	return CONTINUE;
}

/* actions */
CModule::EModRet CLogMod::OnUserAction(CString& sTarget, CString& sMessage)
{
	CIRCNetwork* pNetwork = GetNetwork();
	if (pNetwork) {
		PutLog("* " + pNetwork->GetCurNick() + " " + sMessage, sTarget);
	}

	return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivAction(CNick& Nick, CString& sMessage)
{
	PutLog("* " + Nick.GetNick() + " " + sMessage, Nick);
	return CONTINUE;
}

CModule::EModRet CLogMod::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage)
{
	PutLog("* " + Nick.GetNick() + " " + sMessage, Channel);
	return CONTINUE;
}

/* msgs */
CModule::EModRet CLogMod::OnUserMsg(CString& sTarget, CString& sMessage)
{
	CIRCNetwork* pNetwork = GetNetwork();
	if (pNetwork) {
		PutLog("<" + pNetwork->GetCurNick() + "> " + sMessage, sTarget);
	}

	return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivMsg(CNick& Nick, CString& sMessage)
{
	PutLog("<" + Nick.GetNick() + "> " + sMessage, Nick);
	return CONTINUE;
}

CModule::EModRet CLogMod::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage)
{
	PutLog("<" + Nick.GetNick() + "> " + sMessage, Channel);
	return CONTINUE;
}

template<> void TModInfo<CLogMod>(CModInfo& Info) {
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("[-sanitize] Optional path where to store logs.");
	Info.SetWikiPage("log");
}

USERMODULEDEFS(CLogMod, "Write IRC logs.")
