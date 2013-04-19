/*
 * Copyright (C) 2008-2013  See the AUTHORS file for details.
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
	void SearchCommand(const CString& sLine);
	void ListCommand(const CString& sLine);
	void SeenCommand(const CString& sLine);

	MODCONSTRUCTOR(CLogMod) {
		AddCommand("Help", static_cast<CModCommand::ModCmdFunc>(&CLogMod::HelpCommand));
		AddCommand("Search", static_cast<CModCommand::ModCmdFunc>(&CLogMod::SearchCommand),
			"<channel> <nick> <limit> <search terms>", "Lists up to <limit> lines from <channel>'s logs containing one of <search terms> said/done by <nick>");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ListCommand),
			"<channel> <num> [date] [time]", "Lists the last <num> lines from <channel> on [date]");
		AddCommand("Seen", static_cast<CModCommand::ModCmdFunc>(&CLogMod::SeenCommand),
			"<nick> <channel> [ignore parts/joins]", "Lists the last time <nick> appeared in any log");
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
	bool LineMatches(const CString& line, const CString& sNick, const bool fAnyTerms, const VCString& vsSearch, bool fIgnoreJoinsParts = false);
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

// We assume nicks cannot contain spaces
bool CLogMod::LineMatches(const CString& line, const CString& sNick, const bool fAnyTerms, const VCString& vsSearch, bool fIgnoreJoinsParts) {
	CString first = line.Token(1);
	// eg PutLog("<" + Nick.GetNick() + "> " + sMessage, Channel);
	if (first.length() > 2 && first[0] == '<' && first[first.length() - 1] == '>') {
		if (!first.WildCmp("<"+sNick+">"))
			return false;
	// eg PutLog("-" + Nick.GetNick() + "- " + sMessage, Channel);
	}else if (first.length() > 2 && first[0] == '-' && first[first.length() - 1] == '-') {
		if (!first.WildCmp("-"+sNick+"-"))
			return false;
	}else if (first == "***") {
		CString second = line.Token(2);
		// eg PutLog("*** Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ")", Channel);
		if (second == "Quits:" || second == "Joins:" || second == "Parts:") {
			if (fIgnoreJoinsParts)
				return false;
			CString third = line.Token(3);
			if (!third.WildCmp(sNick))
				return false;
		}else{
			// PutLog("*** " + OldNick.GetNick() + " is now known as " + sNewNick, **pChan);
			if (line.Token(3, true).StrCmp("is now known as ", 16)) {
				if (!second.WildCmp(sNick) && !line.Token(7).WildCmp(sNick))
					return false;
			// PutLog("*** " + sKickedNick + " was kicked by " + OpNick.GetNick() + " (" + sMessage + ")", Channel);
			} else if (line.Token(3, true).StrCmp("was kicked by ", 14)) {
				if (!second.WildCmp(sNick) && !line.Token(6).WildCmp(sNick))
					return false;
			// eg PutLog("*** " + Nick.GetNick() + " changes topic to '" + sTopic + "'", Channel);
			} else if (!second.WildCmp(sNick))
				return false;
		}
	// eg PutLog("* " + Nick.GetNick() + " " + sMessage, Channel);
	}else if (first == "*"){
		CString second = line.Token(2);
		if (!second.WildCmp(sNick))
			return false;
	}else if (sNick != "*") {
		return false; // Broadcast/Connect/Disconnect
	}
	if (fAnyTerms)
		return true;
	else {
		for (VCString::const_iterator it = vsSearch.begin(); it < vsSearch.end(); it++) {
			if (line.WildCmp("*" + *it + "*"))
				return true;
		}
	}
	return false;
}

void CLogMod::HelpCommand(const CString& sLine) {
	PutModule("Commands: search <channel> <nick> <limit> <search terms>, list <channel> <num> [date] [time], seen <nick> <channel> [ignore parts/joins]");
	PutModule("search returns up to <limit> lines matching the given criteria");
	PutModule("list lists up the last <num> lines from <channel> on [date] if [time] isnt set");
	PutModule("or the first <num> lines from <channel> on [date] after [time]");
	PutModule("seen shows the last line logged from <nick> on <channel>");
	PutModule("search results are listed in chronological order for a given date, but dates are returned in reverse order");
	PutModule("<channel> must be either an exact channel name (including #), an exact nick to search privmsg or *");
	PutModule("<nick> must be either a nick or *");
	PutModule("<search terms> may be any number of terms to be interpreted as or, or a single *");
	PutModule("For <search terms> with multiple words, enclose the entire string in double quotes");
	PutModule("<nick> and <search terms> may contain ? to match any single character, or * to match any number of characters");
	PutModule("[date] must be in the form YYYYMMDD (defaults to today)");
	PutModule("[time] must be in the form HH:MM:SS in 24-hour time");
	PutModule("[ignore parts/joins] must be either \"true\" or \"false\" and tells seen to ignore <nick> joining/parting (default is false)");
	PutModule("WARNING: A high <limit> or a search which does not return any valid lines can take a long time!");
}

static const int DATE_LENGTH = 8; // eg "20120404"

struct fileNameOlderThan {
	fileNameOlderThan(bool fStartFromBackIn, int nDateStartIn) {
		fStartFromBack = fStartFromBackIn;
		nDateStart = nDateStartIn;
	}

	bool operator() (const CFile* sOne, const CFile* sTwo) {
		if (fStartFromBack)
			return sOne->GetShortName().Right(nDateStart).StrCmp(sTwo->GetShortName().Right(nDateStart), DATE_LENGTH) > 0;
		else
			return sOne->GetShortName().StrCmp(sTwo->GetShortName(), DATE_LENGTH + nDateStart) > 0;
	}

private:
	bool fStartFromBack;
	int nDateStart;
};

// Log formats other than "%Y%m%d" fall under "undefined behavior" here
void CLogMod::SearchCommand(const CString& sLine) {
	// Would be better if we didn't use a ? as it doubles as a wildcard
	CString sChan = sLine.Token(1).Replace_n("/", "?");
	CString sNick = sLine.Token(2);
	CString sLimit = sLine.Token(3);
	CString sSearch = sLine.Token(4, true);

	if (sChan.empty() || sLimit.empty() || sNick.empty() || sSearch.empty()) {
		PutModule("Usage: search <channel> <nick> <limit> <search terms>");
		return;
	}

	unsigned int nLimit = sLimit.ToUInt();
	if (nLimit == 0) {
		PutModule("Reached limit 0");
		return;
	}
	VCString vsSearch;
	sSearch.Split(" ", vsSearch, false, "\"", "\"");
	bool fAnyTerms = false;
	for (VCString::iterator it = vsSearch.begin(); it < vsSearch.end(); it++) {
		if (*it == "*") {
			fAnyTerms = true;
			break;
		}
	}

	unsigned int nLinesFound = 0;

	if (sChan == "*") {
		if (sNick == "*" && fAnyTerms) {
			PutModule("Error: Must supply at least one non-* search parameter (keep in mind that <search terms> are evaluated as OR)");
			return;
		}

		//Open all files
		CString sPath = FormatLogPath("*");
		CFile file(sPath);

		CString sFile = file.GetShortName();
		CString sFileWildcard = sFile.Replace_n("%Y%m%d", "*");

		CString sDir = file.GetDir();
		sDir.Replace("%Y%m%d", "*");
		if (sDir != file.GetDir()) {
			PutModule("Error: Cannot search with a date in the folder path.");
			return;
		}

		CDir dir;
		dir.FillByWildcard(sDir, sFileWildcard);

		// Sort in reverse order (ie search more recent files first)
		CString::size_type datePos = sFile.find("%Y%m%d");
		bool fDateStartFromBack = false;
		int nDateStart = 0;
		if (datePos != CString::npos) {
			if (sFile.find("*") < datePos) {
				if(sFile.find_last_of("*") > datePos) {
					PutModule("Error: Cannot search with a date in between two variables");
					return;
				} else {
					fDateStartFromBack = true;
					nDateStart = sFile.length() - datePos + 2;
				}
			} else {
				fDateStartFromBack = false;
				nDateStart = datePos;
			}
		}
		fileNameOlderThan olderThan(fDateStartFromBack, nDateStart);
		std::sort(dir.begin(), dir.end(), olderThan);

		for (CDir::iterator it = dir.begin(); it < dir.end(); it++) {
			if ((*it)->Open()) {
				CString date;
				if (fDateStartFromBack) {
					date = (*it)->GetShortName().Right(nDateStart);
					date.RightChomp(nDateStart - DATE_LENGTH);
				} else {
					date = (*it)->GetShortName().Left(nDateStart + DATE_LENGTH);
					date.LeftChomp(nDateStart);
				}
				CString line;
				while ((*it)->ReadLine(line)) {
					line.Trim();
					if (LineMatches(line, sNick, fAnyTerms, vsSearch)) {
						PutModule(date.RightChomp_n(4) + "-" + date.RightChomp_n(2).LeftChomp_n(4) + "-" + date.LeftChomp_n(6) + ": " + line);
						nLinesFound++;
						if (nLinesFound >= nLimit) {
							(*it)->Close();
							PutModule("Reached limit.");
							return;
						}
					}
				}
				(*it)->Close();
			}else
				DEBUG("Could not open log file [" << (*it)->GetLongName() << "]: " << strerror(errno));
		}
	}else{
		//Open only chan
		CString sPath = FormatLogPath(sChan);
		CFile file(sPath);

		CString sFile = file.GetShortName();
		CString sFileWildcard = sFile.Replace_n("%Y%m%d", "*");

		CDir dir;
		dir.FillByWildcard(file.GetDir(), sFileWildcard);

		// Sort in reverse order (ie search more recent files first)
		CString::size_type datePos = sFile.find("%Y%m%d");
		bool fDateStartFromBack = false;
		int nDateStart = 0;
		if (datePos != CString::npos) {
			if (sFile.find("*") < datePos) {
				if(sFile.find_last_of("*") > datePos) {
					PutModule("Error: Cannot search with a date in between two variables");
					return;
				} else {
					fDateStartFromBack = true;
					nDateStart = sFile.length() - datePos + 2;
				}
			} else {
				fDateStartFromBack = false;
				nDateStart = datePos;
			}
		}
		fileNameOlderThan olderThan(fDateStartFromBack, nDateStart);
		std::sort(dir.begin(), dir.end(), olderThan);

		for (CDir::iterator it = dir.begin(); it < dir.end(); it++) {
			if ((*it)->Open()) {
				CString date;
				if (fDateStartFromBack) {
					date = (*it)->GetShortName().Right(nDateStart);
					date.RightChomp(nDateStart - DATE_LENGTH);
				} else {
					date = (*it)->GetShortName().Left(nDateStart + DATE_LENGTH);
					date.LeftChomp(nDateStart);
				}
				CString line;
				while ((*it)->ReadLine(line)) {
					line.Trim();
					if (LineMatches(line, sNick, fAnyTerms, vsSearch)) {
						PutModule(date.RightChomp_n(4) + "-" + date.RightChomp_n(2).LeftChomp_n(4) + "-" + date.LeftChomp_n(6) + ": " + line);
						nLinesFound++;
						if (nLinesFound >= nLimit) {
							(*it)->Close();
							PutModule("Reached limit.");
							return;
						}
					}
				}
				(*it)->Close();
			}else
				DEBUG("Could not open log file [" << (*it)->GetLongName() << "]: " << strerror(errno));
		}
	}
	if (nLinesFound == 0)
		PutModule("No matching lines found.");
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

void CLogMod::SeenCommand(const CString& sLine) {
	CString sNick = sLine.Token(1);
	CString sChan = sLine.Token(2);
	CString sIgnore = sLine.Token(3);

	if (sNick.empty() || sChan.empty()) {
		PutModule("Usage: seen <nick> <channel> [ignore parts/joins]");
		return;
	}

	bool fIgnoreJoinsParts;
	if (sIgnore.empty())
		fIgnoreJoinsParts = false;
	else if (sIgnore.Equals("true"))
		fIgnoreJoinsParts = true;
	else if (sIgnore.Equals("false"))
		fIgnoreJoinsParts = false;
	else {
		PutModule("Error: second argument must be either \"true\" or \"false\"");
		return;
	}

	//Open all files
	CString sPath = FormatLogPath(sChan);
	CFile file(sPath);

	CString sFile = file.GetShortName();
	CString sFileWildcard = sFile.Replace_n("%Y%m%d", "*");

	CString sDir = file.GetDir();
	sDir.Replace("%Y%m%d", "*");
	if (sDir != file.GetDir()) {
		PutModule("Error: Cannot search with a date in the folder path.");
		return;
	}

	CDir dir;
	dir.FillByWildcard(sDir, sFileWildcard);

	// Sort in reverse order (ie search more recent files first)
	CString::size_type datePos = sFile.find("%Y%m%d");
	bool fDateStartFromBack = false;
	int nDateStart = 0;
	if (datePos != CString::npos) {
		if (sFile.find("*") < datePos) {
			if(sFile.find_last_of("*") > datePos) {
				PutModule("Error: Cannot search with a date in between two variables");
				return;
			} else {
				fDateStartFromBack = true;
				nDateStart = sFile.length() - datePos + 2;
			}
		} else {
			fDateStartFromBack = false;
			nDateStart = datePos;
		}
	}
	fileNameOlderThan olderThan(fDateStartFromBack, nDateStart);
	std::sort(dir.begin(), dir.end(), olderThan);

	VCString vsSearch;
	CString sLineResult = CString("[99:99:99]");
	CFile* newestFile = NULL;
	for (CDir::iterator it = dir.begin(); it < dir.end(); it++) {
		if (((newestFile && !olderThan(*it, newestFile)) || !newestFile) && (*it)->Open()) {
			(*it)->Seek((*it)->GetSize());
			CString date;
			if (fDateStartFromBack) {
				date = (*it)->GetShortName().Right(nDateStart);
				date.RightChomp(nDateStart - DATE_LENGTH);
			} else {
				date = (*it)->GetShortName().Left(nDateStart + DATE_LENGTH);
				date.LeftChomp(nDateStart);
			}

			CString line;
			while ((*it)->ReadLine(line, "\n", true)) {
				line.Trim();
				if (LineMatches(line, sNick, true, vsSearch, fIgnoreJoinsParts) && lineOlderThan(sLineResult, line)) {
					sLineResult = date.RightChomp_n(4) + "-" + date.RightChomp_n(2).LeftChomp_n(4) + "-" + date.LeftChomp_n(6) + ": " + line;
					newestFile = *it;
					break;
				}
			}
			(*it)->Close();
		}else
			DEBUG("Could not open log file [" << (*it)->GetLongName() << "]: " << strerror(errno));
	}
	if (sLineResult != "[99:99:99]")
		PutModule(sLineResult);
	else
		PutModule("Have not seen " + sNick);
}

USERMODULEDEFS(CLogMod, "Write IRC logs")
