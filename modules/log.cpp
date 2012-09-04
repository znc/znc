/*
 * Copyright (C) 2008-2012  See the AUTHORS file for details.
 * Copyright (C) 2006-2007, CNU <bshalm@broadpark.no> (http://cnu.dieplz.net/znc)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Server.h>

using std::vector;
using std::list;
#include <algorithm>

class CLogMod: public CModule {
public:
	void HelpCommand(const CString& sLine);
	void ListCommand(const CString& sLine);

	MODCONSTRUCTOR(CLogMod) {
		AddCommand("Help", static_cast<CModCommand::ModCmdFunc>(&CLogMod::HelpCommand));
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ListCommand),
			"<channel> <num> [date] [time]", "Lists the last <num> lines from <channel> on [date]");
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

	CString FormatLogPath(const CString& sWindow);
};

CString CLogMod::FormatLogPath(const CString& sWindow)
{
	CString sRet = m_sLogPath;

	// $WINDOW has to be handled last, since it can contain %
	sRet.Replace("$NETWORK", (m_pNetwork ? m_pNetwork->GetName() : "znc"));
	sRet.Replace("$WINDOW", sWindow.Replace_n("/", "?"));
	sRet.Replace("$USER", (m_pUser ? m_pUser->GetUserName() : "UNKNOWN"));

	return sRet;
}

void CLogMod::PutLog(const CString& sLine, const CString& sWindow /*= "Status"*/)
{
	CString sPath = FormatLogPath(sWindow);
	time_t curtime;

	time(&curtime);
	// Generate file name
	sPath = CUtils::FormatTime(curtime, sPath, m_pUser->GetTimezone());
	if (sPath.empty())
	{
		DEBUG("Could not format log path [" << m_sLogPath << "]");
		return;
	}

	// Check if it's allowed to write in this specific path
	sPath = CDir::CheckPathPrefix(GetSavePath(), sPath);
	if (sPath.empty())
	{
		DEBUG("Invalid log path ["<<m_sLogPath<<"].");
		return;
	}

	CFile LogFile(sPath);
	CString sLogDir = LogFile.GetDir();
	if (!CFile::Exists(sLogDir)) CDir::MakeDir(sLogDir);
	if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT))
	{
		LogFile.Write(CUtils::FormatTime(curtime, "[%H:%M:%S] ", m_pUser->GetTimezone()) + sLine + "\n");
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
	// Use load parameter as save path
	m_sLogPath = sArgs;

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
	Info.SetArgsHelpText("Optional path where to store logs.");
	Info.SetWikiPage("log");
}

void CLogMod::HelpCommand(const CString& sLine) {
	PutModule("Commands: list <channel> <num> [date] [time]");
	PutModule("list lists up the last <num> lines from <channel> on [date] if [time] isnt set");
	PutModule("or the first <num> lines from <channel> on [date] after [time]");
	PutModule("<channel> must be either an exact channel name (including #), an exact nick to search privmsg");
	PutModule("[date] must be in the form YYYYMMDD (defaults to today)");
	PutModule("[time] must be in the form HH:MM:SS in 24-hour time");
}

static const int TIME_LENGTH = 9; // "[%H:%M:%S"
bool lineOlderThan(const CString& sOne, const CString& sTwo) {
	return sOne.StrCmp(sTwo, TIME_LENGTH) > 0;
}

void CLogMod::ListCommand(const CString& sLine) {
	CString sChan = sLine.Token(1).Replace_n("/", "?");
	CString sNum = sLine.Token(2);

	if (sChan.empty() || sNum.empty()) {
		PutModule("Usage: list <channel> <num> [date] [time]");
		return;
	}

	unsigned int nNum = sNum.ToUInt();
	CString sDate = sLine.Token(3);

	bool fUseTime;
	CString sTime = sLine.Token(4);
	if (sTime.empty()) {
		fUseTime = false;
	} else {
		fUseTime = true;
		sTime = "[" + sTime;
	}

	CString sPath = FormatLogPath(sChan);

	if (sDate.empty()) {
		time_t curtime;
		time(&curtime);
		sPath = CUtils::FormatTime(curtime, sPath, m_pUser->GetTimezone());
	} else {
		sPath.Replace("%Y%m%d", sDate);
	}

	CFile file(sPath);
	if (file.Open()) {
		list<CString> lLines;
		CString line;
		file.Seek(file.GetSize());
		if (!fUseTime)
			while (lLines.size() < nNum && file.ReadLine(line, "\n", true)) {
				line.Trim();
				lLines.push_front(line);
			}
		else
			while (file.ReadLine(line, "\n", true) && lineOlderThan(line, sTime)) {
				line.Trim();
				lLines.push_front(line);
				if (lLines.size() > nNum)
					lLines.pop_back();
			}
		for (list<CString>::iterator it = lLines.begin(); it != lLines.end(); it++)
			PutModule(*it);
	}else
		PutModule("Error: could not find log for channel " + sChan + " on date " + sDate);
}

USERMODULEDEFS(CLogMod, "Write IRC logs")
