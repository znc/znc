/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

using std::vector;

class CLogMod: public CModule {
public:
	MODCONSTRUCTOR(CLogMod)
	{
		m_bSanitize = false;
	}

	void PutLog(const CString& sLine, const CString& sWindow = "status");
	void PutLog(const CString& sLine, const CChan& Channel);
	void PutLog(const CString& sLine, const CNick& Nick);
	CString GetServer();

	virtual bool OnLoad(const CString& sArgs, CString& sMessage);
	virtual void OnIRCConnected();
	virtual void OnIRCDisconnected();
	virtual EModRet OnBroadcast(CString& sMessage);

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);
	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage);
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage);
	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic);

	/* notices */
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);

	/* actions */
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);

	/* msgs */
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);

private:
	CString                 m_sLogPath;
	bool                    m_bSanitize;
};

void CLogMod::PutLog(const CString& sLine, const CString& sWindow /*= "Status"*/)
{
	CString sPath;
	time_t curtime;

	time(&curtime);
	// Generate file name
	sPath = CUtils::FormatTime(curtime, m_sLogPath, m_pUser->GetTimezone());
	if (sPath.empty())
	{
		DEBUG("Could not format log path [" << sPath << "]");
		return;
	}

	// $WINDOW has to be handled last, since it can contain %
	sPath.Replace("$NETWORK", (m_pNetwork ? m_pNetwork->GetName() : "znc"));
	sPath.Replace("$WINDOW", sWindow.Replace_n("/", "-").Replace_n("\\", "-"));
	sPath.Replace("$USER", (m_pUser ? m_pUser->GetUserName() : "UNKNOWN"));

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
		LogFile.Write(CUtils::FormatTime(curtime, "[%H:%M:%S] ", m_pUser->GetTimezone()) + (m_bSanitize ? sLine.StripControls_n() : sLine) + "\n");
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
	CServer* pServer = m_pNetwork->GetCurrentServer();
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
			m_sLogPath += "$NETWORK_$WINDOW_%Y%m%d.log";
		}
	} else if (GetType() == CModInfo::NetworkModule) {
		if (m_sLogPath.Right(1) == "/" || m_sLogPath.find("$WINDOW") == CString::npos) {
			if (!m_sLogPath.empty()) {
				m_sLogPath += "/";
			}
			m_sLogPath += "$WINDOW_%Y%m%d.log";
		}
	} else {
		if (m_sLogPath.Right(1) == "/" || m_sLogPath.find("$USER") == CString::npos || m_sLogPath.find("$WINDOW") == CString::npos || m_sLogPath.find("$NETWORK") == CString::npos) {
			if (!m_sLogPath.empty()) {
				m_sLogPath += "/";
			}
			m_sLogPath += "$USER_$NETWORK_$WINDOW_%Y%m%d.log";
		}
	}

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

void CLogMod::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs)
{
	PutLog("*** " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs, Channel);
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
	if (m_pNetwork) {
		PutLog("-" + m_pNetwork->GetCurNick() + "- " + sMessage, sTarget);
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
	if (m_pNetwork) {
		PutLog("* " + m_pNetwork->GetCurNick() + " " + sMessage, sTarget);
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
	if (m_pNetwork) {
		PutLog("<" + m_pNetwork->GetCurNick() + "> " + sMessage, sTarget);
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

USERMODULEDEFS(CLogMod, "Write IRC logs")
