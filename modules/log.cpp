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

#include <stdexcept>

using std::vector;
using std::map;
using std::exception;
using std::runtime_error;
using std::pair;

class CLogMod: public CModule {
public:
	MODCONSTRUCTOR(CLogMod)
	{
		m_bSanitize = false;
		m_bSaveSnotice = true;
		m_bDiagnosticSpam = false;
		AddCommand("diagnostics", static_cast<CModCommand::ModCmdFunc>(&CLogMod::DiagnosticsCommand), "<on/off>", "enables or disables spammy diagnostics messages");
		AddCommand("expire-cache", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ExpireCacheCommand), "", "expires all open logs, frees resources as needed");
		AddCommand("flush-cache", static_cast<CModCommand::ModCmdFunc>(&CLogMod::FlushCacheCommand), "", "expires all open logs, frees resources immediately");
		AddCommand("save-snotice", static_cast<CModCommand::ModCmdFunc>(&CLogMod::SaveSNoticeCommand), "<on/off>", "enables or disables logging of server notices");
		AddCommand("log-msgtype", static_cast<CModCommand::ModCmdFunc>(&CLogMod::LogMessageTypeCommand), "<message type>", "enables logging of a specific type of message (e.g. 'MODE' to log all mode changes, or '391' to log all replies to the TIME command");
		AddCommand("unlog-msgtype", static_cast<CModCommand::ModCmdFunc>(&CLogMod::UnlogMessageTypeCommand), "<message type>", "disables logging of a specific type of message");
		AddCommand("log-wildcard", static_cast<CModCommand::ModCmdFunc>(&CLogMod::LogWildcardCommand), "<wildcard>", "enables logging of any message that matches a specific wildcard");
		AddCommand("unlog-wildcard", static_cast<CModCommand::ModCmdFunc>(&CLogMod::UnlogWildcardCommand), "<wildcard>", "disables logging of any message that matches a specific wildcard");
		AddCommand("unlog-all", static_cast<CModCommand::ModCmdFunc>(&CLogMod::UnlogAllCommand), "", "disables all special logging");
		AddCommand("list-extra-params", static_cast<CModCommand::ModCmdFunc>(&CLogMod::ListExtraCommand), "", "lists all extra logging parameters");
		AddHelpCommand();
	}
	~CLogMod();

	// usable commands
	void DiagnosticsCommand(const CString &sCmd);
	void ExpireCacheCommand(const CString &sCmd);
	void FlushCacheCommand(const CString &sCmd);
	void SaveSNoticeCommand(const CString &sCmd);

	void LogMessageTypeCommand(const CString &sCmd);
	void UnlogMessageTypeCommand(const CString &sCmd);
	void LogWildcardCommand(const CString &sCmd);
	void UnlogWildcardCommand(const CString &sCmd);
	void UnlogAllCommand(const CString &sCmd);
	void ListExtraCommand(const CString &sCmd);

	bool MatchesExtraLogging(const CString &sLine);
	
	void PutLog(const CString& sLine, const CString& sWindow = "(status)");
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
	virtual EModRet OnServerNotice(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);

	/* actions */
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);

	/* msgs */
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);
	
	/* ctcps */
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage);
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage);

	virtual EModRet OnRaw(CString &sLine);

private:
	CString                 m_sLogPath;
	bool                    m_bSanitize;
	
	// user settable state
	bool			m_bDiagnosticSpam;
	bool			m_bSaveSnotice;
	
	// caching state
	CString			m_sLastDay;
	map<CString, CFile *>	m_LogCache;
	map<CString, CFile *>	m_ExpCache;

	// extra logging state
	SCString		m_Wildcards;
	SCString		m_MessageTypes;
	
	// copies a log file in an attempt to remove file fragmentation
	void LogDefrag(const CString &filename);
	
	// cache management
	CFile &CacheLookup(const CString &filename);
	CFile &CacheOpen(const CString &filename);
	void CacheNudge(const CString &filename);
	void CacheProcess(const CString &filename);
	void CacheKill(const CString &filename);
	
	void CacheNudgeAll();
	void CacheKillAll();
	void CacheProcessAll();
	void CacheProcessOne();
};

CFile &CLogMod::CacheOpen(const CString &filename)
{
	CFile new_file(filename);
	CString full_name = new_file.GetLongName();
	if (m_LogCache.find(full_name) != m_LogCache.end()) {} // already open, do nothing
	else if (m_ExpCache.find(full_name) != m_ExpCache.end()) m_LogCache[full_name] = m_ExpCache[full_name]; // go from "dead" to "nudged"
	else // go from gone to live
	{
		CFile *cf = NULL;
		try
		{
			cf = new CFile(new_file);
			m_LogCache[full_name] = cf;
		}
		catch (exception &e)
		{
			m_LogCache.erase(full_name);
			if (cf) delete cf;
			throw;
		}
	}
	return *m_LogCache[full_name]; // return the file
}

void CLogMod::CacheNudge(const CString &filename)
{
	CFile new_file(filename);
	CString full_name = new_file.GetLongName();
	if (m_LogCache.find(full_name) != m_LogCache.end()) m_ExpCache[full_name] = m_LogCache[full_name]; // go from live/nudged to nudged
	return;
}

void CLogMod::CacheKill(const CString &filename)
{
	CFile new_file(filename);
	CString full_name = new_file.GetLongName();
	if (m_LogCache.find(full_name) != m_LogCache.end())
	{
		m_ExpCache[full_name] = m_LogCache[full_name]; // go from live/nudged to nudged
		m_LogCache.erase(full_name); // go from nudged to dead
	}
	return;
}

void CLogMod::CacheProcess(const CString &filename)
{
	CFile new_file(filename);
	CString full_name = new_file.GetLongName();
	if (m_ExpCache.find(full_name) != m_ExpCache.end())
	{
		LogDefrag(full_name);
		if (m_LogCache.find(full_name) == m_LogCache.end()) delete m_ExpCache[full_name];
		m_ExpCache.erase(full_name);
	}
	return;
}

void CLogMod::CacheNudgeAll()
{
	m_ExpCache.insert(m_LogCache.begin(), m_LogCache.end()); // move all live/nudged to nudged
	if (m_bDiagnosticSpam) PutModule("Nudged all cached log files!");
}

void CLogMod::CacheKillAll()
{
	CacheNudgeAll();
	m_LogCache.clear();
	if (m_bDiagnosticSpam) PutModule("Killed all cached log files!");
}

void CLogMod::CacheProcessAll()
{
	while (!m_ExpCache.empty()) CacheProcess(m_ExpCache.begin()->first);
	if (m_bDiagnosticSpam) PutModule("Processed all pending cache changes!");
}

void CLogMod::CacheProcessOne()
{
	if (!m_ExpCache.empty()) CacheProcess(m_ExpCache.begin()->first);
}

void CLogMod::LogDefrag(const CString &filename)
{
	CString newfilename(filename + ".new");
	if (!CFile::Copy(filename, newfilename, true))
	{
		PutModule("Log defragment failed: copy! (" + filename + ")");
	}
	else if (!CFile::Move(newfilename, filename, true))
	{
		PutModule("Log defragment failed: move! (" + filename + ")");
	}
	if (m_bDiagnosticSpam) PutModule("Defragmented log file: " + filename);
}

void CLogMod::DiagnosticsCommand(const CString &sCmd)
{
	const CString sArg(sCmd.Token(1, true));
	m_bDiagnosticSpam = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
	PutModule(CString("Diagnostic messages ") + (m_bDiagnosticSpam ? "enabled." : "disabled."));
}

void CLogMod::ExpireCacheCommand(const CString &sCmd)
{
	CacheNudgeAll();
	PutModule("Logfile cache manually expired.");
}

void CLogMod::FlushCacheCommand(const CString &sCmd)
{
	CacheKillAll();
	CacheProcessAll();
	PutModule("Logfile cache manually flushed.");
}

void CLogMod::SaveSNoticeCommand(const CString &sCmd)
{
	const CString sArg(sCmd.Token(1, true));
	m_bSaveSnotice = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
	PutModule(CString("Now ") + (m_bSaveSnotice ? "logging" : "ignoring") + " server notices.");
}

void CLogMod::LogMessageTypeCommand(const CString &sCmd)
{
	const CString sMessageType(sCmd.Token(1).MakeUpper());
	if (m_MessageTypes.size() + m_Wildcards.size() <= 50)
	{
		m_MessageTypes.insert(sMessageType);
		PutModule("Now logging messages of type " + sMessageType + ".");
	}
	else
		PutModule("Too many extra logging conditions already exist.  Remove some and try again.");
}

void CLogMod::UnlogMessageTypeCommand(const CString &sCmd)
{
	const CString sMessageType(sCmd.Token(1).MakeUpper());
	m_MessageTypes.erase(sMessageType);
	PutModule("No longer logging messages of type " + sMessageType + ".");
}

void CLogMod::LogWildcardCommand(const CString &sCmd)
{
	const CString sWildcard(sCmd.Token(1, true));
	if (m_MessageTypes.size() + m_Wildcards.size() <= 50)
	{
		m_Wildcards.insert(sWildcard);
		PutModule("Now logging messages that match wildcard '" + sWildcard + "'.");
	}
	else
		PutModule("Too many extra logging conditions already exist.  Remove some and try again.");
}

void CLogMod::UnlogWildcardCommand(const CString &sCmd)
{
	const CString sWildcard(sCmd.Token(1, true));
	m_Wildcards.erase(sWildcard);
	PutModule("No longer logging messages that match wildcard '" + sWildcard + "'.");
}

void CLogMod::UnlogAllCommand(const CString &sCmd)
{
	m_Wildcards.clear();
	m_MessageTypes.clear();
	PutModule("Discarded all extra logging parameters.");
}

void CLogMod::ListExtraCommand(const CString &sCmd)
{
	PutModule("Message types to log seperately:");
	for (SCString::iterator i = m_MessageTypes.begin(); i != m_MessageTypes.end(); ++i) PutModule(*i);
	PutModule("Wildcard matches to log seperately:");
	for (SCString::iterator i = m_Wildcards.begin(); i != m_Wildcards.end(); ++i) PutModule(*i);
}

CLogMod::~CLogMod()
{
	CacheKillAll();
	CacheProcessAll();
}

CFile &CLogMod::CacheLookup(const CString &logname)
{
	map<CString, CFile *>::iterator entry = m_LogCache.find(logname);
	if (entry == m_LogCache.end())
	{
		if (m_bDiagnosticSpam) PutModule("Cache miss! (" + logname + ")");
	}
	else
	{
		if (m_bDiagnosticSpam) PutModule("Cache hit! (" + logname + ")");
	}
	
	return CacheOpen(logname); // this can throw, so we can throw from this function
}

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
	
	if (sPath != m_sLastDay) // depends on sPath with the data imprinted onto it, updates whenever the date changes
	{
		m_sLastDay = sPath;
		CacheKillAll();
	}

	// $WINDOW has to be handled last, since it can contain %
	sPath.Replace("$NETWORK", (m_pNetwork ? m_pNetwork->GetName() : "znc"));
	sPath.Replace("$WINDOW", sWindow.Replace_n("/", "?"));
	sPath.Replace("$USER", (m_pUser ? m_pUser->GetUserName() : "UNKNOWN"));

	// Check if it's allowed to write in this specific path
	sPath = CDir::CheckPathPrefix(GetSavePath(), sPath);
	if (sPath.empty())
	{
		DEBUG("Invalid log path ["<<m_sLogPath<<"].");
		return;
	}

	CacheProcessOne(); // expire old cache one item at a time rather than all at once which would cause an unpleasant lagspike
	try
	{
		CFile &LogFile = CacheLookup(sPath);
		if (!LogFile.Open(O_WRONLY | O_APPEND | O_CREAT)) throw runtime_error("Could not open log file: " + sPath);
		LogFile.Write(CUtils::FormatTime(curtime, "[%H:%M:%S] ", m_pUser->GetTimezone()) + (m_bSanitize ? sLine.StripControls_n() : sLine) + "\n");
		LogFile.Close();
	}
	catch (exception &e)
	{
		DEBUG("Could not write to log file " + sPath + ": " + e.what());
	}
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

CModule::EModRet CLogMod::OnServerNotice(CNick& Nick, CString& sMessage)
{
	if (m_bSaveSnotice) PutLog("-" + Nick.GetNick() + "- " + sMessage, "(server)");
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

CModule::EModRet CLogMod::OnCTCPReply(CNick& Nick, CString& sMessage)
{
	PutLog("*** " + Nick.GetNick() + " CTCP-REPLY " + sMessage.Trim_n("\x001"), Nick);
	return CONTINUE;
}

CModule::EModRet CLogMod::OnPrivCTCP(CNick& Nick, CString& sMessage)
{
	if (sMessage.substr(0, 7) != "ACTION ") // skip this case since we already log it with OnPrivAction
		PutLog("*** " + Nick.GetNick() + " CTCP " + sMessage.Trim_n("\x001"), Nick);
	return CONTINUE;
}

CModule::EModRet CLogMod::OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage)
{
	if (sMessage.substr(0, 7) != "ACTION ") // skip this case since we already log it with OnChanAction
		PutLog("*** " + Nick.GetNick() + " CTCP " + sMessage.Trim_n("\x001"), Channel);
	return CONTINUE;
}

bool CLogMod::MatchesExtraLogging(const CString &sLine)
{
	CString messageType((sLine.find(':') == 0 ? sLine.Token(1) : sLine.Token(0)).MakeUpper());
	for (SCString::iterator i = m_MessageTypes.begin(); i != m_MessageTypes.end(); ++i)
		if (messageType == *i)
			return true; // the line matches a message type
	for (SCString::iterator i = m_Wildcards.begin(); i != m_Wildcards.end(); ++i)
		if (sLine.WildCmp(*i))
			return true; // the line matches a wildcard
	return false; // the line doesn't match
}

CModule::EModRet CLogMod::OnRaw(CString &sLine)
{
	if (MatchesExtraLogging(sLine)) // if this line matches something in the list of extra messages we're supposed to log
		PutLog(sLine, "(extra)");
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
