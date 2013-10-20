/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#include <znc/Modules.h>
#include <znc/FileUtils.h>
#include <znc/Template.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/WebModules.h>
#include <znc/znc.h>
#include <dlfcn.h>

using std::map;
using std::set;
using std::vector;

bool ZNC_NO_NEED_TO_DO_ANYTHING_ON_MODULE_CALL_EXITER;

#ifndef RTLD_LOCAL
# define RTLD_LOCAL 0
# warning "your crap box doesnt define RTLD_LOCAL !?"
#endif

#define MODUNLOADCHK(func)                                        \
	for (unsigned int a = 0; a < size(); a++) {                      \
		try {                                                    \
			CModule* pMod = (CModule *) (*this)[a];                \
			CClient* pOldClient = pMod->GetClient();         \
			pMod->SetClient(m_pClient);                      \
			CUser* pOldUser = NULL;                      \
			if (m_pUser) {                               \
				pOldUser = pMod->GetUser();              \
				pMod->SetUser(m_pUser);                  \
			}                                            \
			CIRCNetwork* pNetwork = NULL;                \
			if (m_pNetwork) {                            \
				pNetwork = pMod->GetNetwork();           \
				pMod->SetNetwork(m_pNetwork);            \
			}                                            \
			pMod->func;                                  \
			if (m_pUser)                                 \
				pMod->SetUser(pOldUser);                 \
			if (m_pNetwork)                              \
				pMod->SetNetwork(pNetwork);              \
			pMod->SetClient(pOldClient);                     \
		} catch (const CModule::EModException& e) {                     \
			if (e == CModule::UNLOAD) {                      \
				UnloadModule((*this)[a]->GetModName());  \
			}                                                \
		}                                                        \
	}


#define MODHALTCHK(func)                                          \
	bool bHaltCore = false;                                          \
	for (unsigned int a = 0; a < size(); a++) {                      \
		try {                                                    \
			CModule* pMod = (CModule*) (*this)[a];                 \
			CModule::EModRet e = CModule::CONTINUE;          \
			CClient* pOldClient = pMod->GetClient();         \
			pMod->SetClient(m_pClient);                      \
			CUser* pOldUser = NULL;                      \
			if (m_pUser) {                               \
				pOldUser = pMod->GetUser();              \
				pMod->SetUser(m_pUser);                  \
			}                                            \
			CIRCNetwork* pNetwork = NULL;                \
			if (m_pNetwork) {                            \
				pNetwork = pMod->GetNetwork();           \
				pMod->SetNetwork(m_pNetwork);            \
			}                                            \
			e = pMod->func;                              \
			if (m_pUser)                                 \
				pMod->SetUser(pOldUser);                 \
			if (m_pNetwork)                              \
				pMod->SetNetwork(pNetwork);              \
			pMod->SetClient(pOldClient);                     \
			if (e == CModule::HALTMODS) {                    \
				break;                                   \
			} else if (e == CModule::HALTCORE) {             \
				bHaltCore = true;                        \
			} else if (e == CModule::HALT) {                 \
				bHaltCore = true;                        \
				break;                                   \
			}                                                \
		} catch (const CModule::EModException& e) {                     \
			if (e == CModule::UNLOAD) {                      \
				UnloadModule((*this)[a]->GetModName());  \
			}                                                \
		}                                                        \
	}                                                                \
	return bHaltCore;

/////////////////// Timer ///////////////////
CTimer::CTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CCron() {
	SetName(sLabel);
	m_sDescription = sDescription;
	m_pModule = pModule;

	if (uCycles) {
		StartMaxCycles(uInterval, uCycles);
	} else {
		Start(uInterval);
	}
}

CTimer::~CTimer() {
	m_pModule->UnlinkTimer(this);
}

void CTimer::SetModule(CModule* p) { m_pModule = p; }
void CTimer::SetDescription(const CString& s) { m_sDescription = s; }
CModule* CTimer::GetModule() const { return m_pModule; }
const CString& CTimer::GetDescription() const { return m_sDescription; }
/////////////////// !Timer ///////////////////


CModule::CModule(ModHandle pDLL, CUser* pUser, CIRCNetwork* pNetwork, const CString& sModName, const CString& sDataDir) {
	m_pDLL = pDLL;
	m_pManager = &(CZNC::Get().GetManager());;
	m_pUser = pUser;
	m_pNetwork = pNetwork;
	m_pClient = NULL;
	m_sModName = sModName;
	m_sDataDir = sDataDir;

	if (m_pNetwork) {
		m_sSavePath = m_pNetwork->GetNetworkPath() + "/moddata/" + m_sModName;
	} else if (m_pUser) {
		m_sSavePath = m_pUser->GetUserPath() + "/moddata/" + m_sModName;
	} else {
		m_sSavePath = CZNC::Get().GetZNCPath() + "/moddata/" + m_sModName;
	}
	LoadRegistry();
}

CModule::~CModule() {
	while (!m_sTimers.empty()) {
		RemTimer(*m_sTimers.begin());
	}

	while (!m_sSockets.empty()) {
		RemSocket(*m_sSockets.begin());
	}

	SaveRegistry();
}

void CModule::SetUser(CUser* pUser) { m_pUser = pUser; }
void CModule::SetNetwork(CIRCNetwork* pNetwork) { m_pNetwork = pNetwork; }
void CModule::SetClient(CClient* pClient) { m_pClient = pClient; }

CString CModule::ExpandString(const CString& sStr) const {
	CString sRet;
	return ExpandString(sStr, sRet);
}

CString& CModule::ExpandString(const CString& sStr, CString& sRet) const {
	sRet = sStr;

	if (m_pNetwork) {
		return m_pNetwork->ExpandString(sRet, sRet);
	}

	if (m_pUser) {
		return m_pUser->ExpandString(sRet, sRet);
	}

	return sRet;
}

const CString& CModule::GetSavePath() const {
	if (!CFile::Exists(m_sSavePath)) {
		CDir::MakeDir(m_sSavePath);
	}
	return m_sSavePath;
}

CString CModule::GetWebPath() {
	switch (m_eType) {
		case CModInfo::GlobalModule: return "/mods/global/" + GetModName() + "/";
		case CModInfo::UserModule: return "/mods/user/" + GetModName() + "/";
		case CModInfo::NetworkModule: return "/mods/network/" + m_pNetwork->GetName() + "/" + GetModName() + "/";
		default: return "/";
	}
}

CString CModule::GetWebFilesPath() {
	switch (m_eType) {
		case CModInfo::GlobalModule: return "/modfiles/global/" + GetModName() + "/";
		case CModInfo::UserModule: return "/modfiles/user/" + GetModName() + "/";
		case CModInfo::NetworkModule: return "/modfiles/network/" + m_pNetwork->GetName() + "/" + GetModName() + "/";
		default: return "/";
	}
}

bool CModule::LoadRegistry() {
	//CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
	return (m_mssRegistry.ReadFromDisk(GetSavePath() + "/.registry") == MCString::MCS_SUCCESS);
}

bool CModule::SaveRegistry() const {
	//CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
	return (m_mssRegistry.WriteToDisk(GetSavePath() + "/.registry", 0600) == MCString::MCS_SUCCESS);
}

bool CModule::SetNV(const CString & sName, const CString & sValue, bool bWriteToDisk) {
	m_mssRegistry[sName] = sValue;
	if (bWriteToDisk) {
		return SaveRegistry();
	}

	return true;
}

CString CModule::GetNV(const CString & sName) const {
	MCString::const_iterator it = m_mssRegistry.find(sName);

	if (it != m_mssRegistry.end()) {
		return it->second;
	}

	return "";
}

bool CModule::DelNV(const CString & sName, bool bWriteToDisk) {
	MCString::iterator it = m_mssRegistry.find(sName);

	if (it != m_mssRegistry.end()) {
		m_mssRegistry.erase(it);
	} else {
		return false;
	}

	if (bWriteToDisk) {
		return SaveRegistry();
	}

	return true;
}

bool CModule::ClearNV(bool bWriteToDisk) {
	m_mssRegistry.clear();

	if (bWriteToDisk) {
		return SaveRegistry();
	}
	return true;
}

bool CModule::AddTimer(CTimer* pTimer) {
	if ((!pTimer) || (!pTimer->GetName().empty() && FindTimer(pTimer->GetName()))) {
		delete pTimer;
		return false;
	}

	if (!m_sTimers.insert(pTimer).second)
		// Was already added
		return true;

	m_pManager->AddCron(pTimer);
	return true;
}

bool CModule::AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles, const CString& sDescription) {
	CFPTimer *pTimer = new CFPTimer(this, uInterval, uCycles, sLabel, sDescription);
	pTimer->SetFPCallback(pFBCallback);

	return AddTimer(pTimer);
}

bool CModule::RemTimer(CTimer* pTimer) {
	if (m_sTimers.erase(pTimer) == 0)
		return false;
	m_pManager->DelCronByAddr(pTimer);
	return true;
}

bool CModule::RemTimer(const CString& sLabel) {
	CTimer *pTimer = FindTimer(sLabel);
	if (!pTimer)
		return false;
	return RemTimer(pTimer);
}

bool CModule::UnlinkTimer(CTimer* pTimer) {
	set<CTimer*>::iterator it;
	for (it = m_sTimers.begin(); it != m_sTimers.end(); ++it) {
		if (pTimer == *it) {
			m_sTimers.erase(it);
			return true;
		}
	}

	return false;
}

CTimer* CModule::FindTimer(const CString& sLabel) {
	if (sLabel.empty()) {
		return NULL;
	}

	set<CTimer*>::iterator it;
	for (it = m_sTimers.begin(); it != m_sTimers.end(); ++it) {
		CTimer* pTimer = *it;
		if (pTimer->GetName().Equals(sLabel)) {
			return pTimer;
		}
	}

	return NULL;
}

void CModule::ListTimers() {
	if (m_sTimers.empty()) {
		PutModule("You have no timers running.");
		return;
	}

	CTable Table;
	Table.AddColumn("Name");
	Table.AddColumn("Secs");
	Table.AddColumn("Cycles");
	Table.AddColumn("Description");

	set<CTimer*>::iterator it;
	for (it = m_sTimers.begin(); it != m_sTimers.end(); ++it) {
		CTimer* pTimer = *it;
		unsigned int uCycles = pTimer->GetCyclesLeft();
		timeval Interval = pTimer->GetInterval();

		Table.AddRow();
		Table.SetCell("Name", pTimer->GetName());
		Table.SetCell("Secs", CString(Interval.tv_sec) + "seconds" + (Interval.tv_usec ? " " + CString(Interval.tv_usec) + " microseconds" : ""));
		Table.SetCell("Cycles", ((uCycles) ? CString(uCycles) : "INF"));
		Table.SetCell("Description", pTimer->GetDescription());
	}

	PutModule(Table);
}

bool CModule::AddSocket(CSocket* pSocket) {
	if (!pSocket) {
		return false;
	}

	m_sSockets.insert(pSocket);
	return true;
}

bool CModule::RemSocket(CSocket* pSocket) {
	set<CSocket*>::iterator it;
	for (it = m_sSockets.begin(); it != m_sSockets.end(); ++it) {
		if (*it == pSocket) {
			m_sSockets.erase(it);
			m_pManager->DelSockByAddr(pSocket);
			return true;
		}
	}

	return false;
}

bool CModule::RemSocket(const CString& sSockName) {
	set<CSocket*>::iterator it;
	for (it = m_sSockets.begin(); it != m_sSockets.end(); ++it) {
		CSocket* pSocket = *it;

		if (pSocket->GetSockName().Equals(sSockName)) {
			m_sSockets.erase(it);
			m_pManager->DelSockByAddr(pSocket);
			return true;
		}
	}

	return false;
}

bool CModule::UnlinkSocket(CSocket* pSocket) {
	set<CSocket*>::iterator it;
	for (it = m_sSockets.begin(); it != m_sSockets.end(); ++it) {
		if (pSocket == *it) {
			m_sSockets.erase(it);
			return true;
		}
	}

	return false;
}

CSocket* CModule::FindSocket(const CString& sSockName) {
	set<CSocket*>::iterator it;
	for (it = m_sSockets.begin(); it != m_sSockets.end(); ++it) {
		CSocket* pSocket = *it;
		if (pSocket->GetSockName().Equals(sSockName)) {
			return pSocket;
		}
	}

	return NULL;
}

void CModule::ListSockets() {
	if (m_sSockets.empty()) {
		PutModule("You have no open sockets.");
		return;
	}

	CTable Table;
	Table.AddColumn("Name");
	Table.AddColumn("State");
	Table.AddColumn("LocalPort");
	Table.AddColumn("SSL");
	Table.AddColumn("RemoteIP");
	Table.AddColumn("RemotePort");

	set<CSocket*>::iterator it;
	for (it = m_sSockets.begin(); it != m_sSockets.end(); ++it) {
		CSocket* pSocket = *it;

		Table.AddRow();
		Table.SetCell("Name", pSocket->GetSockName());

		if (pSocket->GetType() == CSocket::LISTENER) {
			Table.SetCell("State", "Listening");
		} else {
			Table.SetCell("State", (pSocket->IsConnected() ? "Connected" : ""));
		}

		Table.SetCell("LocalPort", CString(pSocket->GetLocalPort()));
		Table.SetCell("SSL", (pSocket->GetSSL() ? "yes" : "no"));
		Table.SetCell("RemoteIP", pSocket->GetRemoteIP());
		Table.SetCell("RemotePort", (pSocket->GetRemotePort()) ? CString(pSocket->GetRemotePort()) : CString(""));
	}

	PutModule(Table);
}

bool CModule::AddCommand(const CModCommand& Command)
{
	if (Command.GetFunction() == NULL)
		return false;
	if (Command.GetCommand().find(' ') != CString::npos)
		return false;
	if (FindCommand(Command.GetCommand()) != NULL)
		return false;

	m_mCommands[Command.GetCommand()] = Command;
	return true;
}

bool CModule::AddCommand(const CString& sCmd, CModCommand::ModCmdFunc func, const CString& sArgs, const CString& sDesc)
{
	CModCommand cmd(sCmd, func, sArgs, sDesc);
	return AddCommand(cmd);
}

void CModule::AddHelpCommand()
{
	AddCommand("Help", &CModule::HandleHelpCommand, "search", "Generate this output");
}

bool CModule::RemCommand(const CString& sCmd)
{
	return m_mCommands.erase(sCmd) > 0;
}

const CModCommand* CModule::FindCommand(const CString& sCmd) const
{
	map<CString, CModCommand>::const_iterator it;
	for (it = m_mCommands.begin(); it != m_mCommands.end(); ++it) {
		if (!it->first.Equals(sCmd))
			continue;
		return &it->second;
	}
	return NULL;
}

bool CModule::HandleCommand(const CString& sLine) {
	const CString& sCmd = sLine.Token(0);
	const CModCommand* pCmd = FindCommand(sCmd);

	if (pCmd) {
		pCmd->Call(this, sLine);
		return true;
	}

	OnUnknownModCommand(sLine);

	return false;
}

void CModule::HandleHelpCommand(const CString& sLine) {
	CString sFilter = sLine.Token(1, true);
	CString::size_type  iFilterLength = sFilter.size();
	CTable Table;
	map<CString, CModCommand>::const_iterator it;

	CModCommand::InitHelp(Table);
	for (it = m_mCommands.begin(); it != m_mCommands.end(); ++it) {
		if (sFilter.empty() || (it->second.GetCommand().Equals(sFilter, false, iFilterLength))) {
			it->second.AddHelp(Table);
		}
	}
	PutModule(Table);
}

CString CModule::GetModNick() const { return ((m_pUser) ? m_pUser->GetStatusPrefix() : "*") + m_sModName; }

// Webmods
bool CModule::OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) { return false; }
bool CModule::OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) { return false; }
bool CModule::OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) { return false; }
// !Webmods

bool CModule::OnLoad(const CString& sArgs, CString& sMessage) { sMessage = ""; return true; }
bool CModule::OnBoot() { return true; }
void CModule::OnPreRehash() {}
void CModule::OnPostRehash() {}
void CModule::OnIRCDisconnected() {}
void CModule::OnIRCConnected() {}
CModule::EModRet CModule::OnIRCConnecting(CIRCSock *IRCSock) { return CONTINUE; }
void CModule::OnIRCConnectionError(CIRCSock *IRCSock) {}
CModule::EModRet CModule::OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) { return CONTINUE; }
CModule::EModRet CModule::OnBroadcast(CString& sMessage) { return CONTINUE; }

void CModule::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {}
void CModule::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {}
void CModule::OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) {}

CModule::EModRet CModule::OnRaw(CString& sLine) { return CONTINUE; }

CModule::EModRet CModule::OnStatusCommand(CString& sCommand) { return CONTINUE; }
void CModule::OnModNotice(const CString& sMessage) {}
void CModule::OnModCTCP(const CString& sMessage) {}

void CModule::OnModCommand(const CString& sCommand) {
	HandleCommand(sCommand);
}
void CModule::OnUnknownModCommand(const CString& sLine) {
	if (m_mCommands.empty())
		// This function is only called if OnModCommand wasn't
		// overriden, so no false warnings for modules which don't use
		// CModCommand for command handling.
		PutModule("This module doesn't implement any commands.");
	else
		PutModule("Unknown command!");
}

void CModule::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {}
void CModule::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {}
void CModule::OnKick(const CNick& Nick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {}
void CModule::OnJoin(const CNick& Nick, CChan& Channel) {}
void CModule::OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {}
CModule::EModRet CModule::OnInvite(const CNick& Nick, const CString& sChan) { return CONTINUE; }

CModule::EModRet CModule::OnChanBufferStarting(CChan& Chan, CClient& Client) { return CONTINUE; }
CModule::EModRet CModule::OnChanBufferEnding(CChan& Chan, CClient& Client) { return CONTINUE; }
CModule::EModRet CModule::OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnPrivBufferPlayLine(CClient& Client, CString& sLine) { return CONTINUE; }

void CModule::OnClientLogin() {}
void CModule::OnClientDisconnect() {}
CModule::EModRet CModule::OnUserRaw(CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnUserCTCPReply(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserCTCP(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserAction(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserMsg(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserNotice(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserJoin(CString& sChannel, CString& sKey) { return CONTINUE; }
CModule::EModRet CModule::OnUserPart(CString& sChannel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserTopic(CString& sChannel, CString& sTopic) { return CONTINUE; }
CModule::EModRet CModule::OnUserTopicRequest(CString& sChannel) { return CONTINUE; }

CModule::EModRet CModule::OnCTCPReply(CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivCTCP(CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivAction(CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivMsg(CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivNotice(CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) { return CONTINUE; }
CModule::EModRet CModule::OnTimerAutoJoin(CChan& Channel) { return CONTINUE; }
CModule::EModRet CModule::OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) { return CONTINUE; }
CModule::EModRet CModule::OnDeleteNetwork(CIRCNetwork& Network) { return CONTINUE; }

bool CModule::OnServerCapAvailable(const CString& sCap) { return false; }
void CModule::OnServerCapResult(const CString& sCap, bool bSuccess) {}

bool CModule::PutIRC(const CString& sLine) {
	return (m_pNetwork) ? m_pNetwork->PutIRC(sLine) : false;
}
bool CModule::PutUser(const CString& sLine) {
	return (m_pNetwork) ? m_pNetwork->PutUser(sLine, m_pClient) : false;
}
bool CModule::PutStatus(const CString& sLine) {
	return (m_pNetwork) ? m_pNetwork->PutStatus(sLine, m_pClient) : false;
}
unsigned int CModule::PutModule(const CTable& table) {
	if (!m_pUser)
		return 0;

	unsigned int idx = 0;
	CString sLine;
	while (table.GetLine(idx++, sLine))
		PutModule(sLine);
	return idx - 1;
}
bool CModule::PutModule(const CString& sLine) {
	if (m_pClient) {
		m_pClient->PutModule(GetModName(), sLine);
		return true;
	}

	if (m_pNetwork) {
		return m_pNetwork->PutModule(GetModName(), sLine);
	}

	if (m_pUser) {
		return m_pUser->PutModule(GetModName(), sLine);
	}

	return false;
}
bool CModule::PutModNotice(const CString& sLine) {
	if (!m_pUser)
		return false;

	if (m_pClient) {
		m_pClient->PutModNotice(GetModName(), sLine);
		return true;
	}

	return m_pUser->PutModNotice(GetModName(), sLine);
}

///////////////////
// Global Module //
///////////////////
CModule::EModRet CModule::OnAddUser(CUser& User, CString& sErrorRet) { return CONTINUE; }
CModule::EModRet CModule::OnDeleteUser(CUser& User) { return CONTINUE; }
void CModule::OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) {}
CModule::EModRet CModule::OnLoginAttempt(CSmartPtr<CAuthBase> Auth) { return CONTINUE; }
void CModule::OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {}
CModule::EModRet CModule::OnUnknownUserRaw(CClient* pClient, CString& sLine) { return CONTINUE; }
void CModule::OnClientCapLs(CClient* pClient, SCString& ssCaps) {}
bool CModule::IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState) { return false; }
void CModule::OnClientCapRequest(CClient* pClient, const CString& sCap, bool bState) {}
CModule::EModRet CModule::OnModuleLoading(const CString& sModName, const CString& sArgs,
		CModInfo::EModuleType eType, bool& bSuccess, CString& sRetMsg) { return CONTINUE; }
CModule::EModRet CModule::OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg) {
	return CONTINUE;
}
CModule::EModRet CModule::OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
		bool& bSuccess, CString& sRetMsg) { return CONTINUE; }
void CModule::OnGetAvailableMods(set<CModInfo>& ssMods, CModInfo::EModuleType eType) {}


CModules::CModules() {
	m_pUser = NULL;
	m_pNetwork = NULL;
	m_pClient = NULL;
}

CModules::~CModules() {
	UnloadAll();
}

void CModules::UnloadAll() {
	while (size()) {
		CString sRetMsg;
		CString sModName = back()->GetModName();
		UnloadModule(sModName, sRetMsg);
	}
}

bool CModules::OnBoot() {
	for (unsigned int a = 0; a < size(); a++) {
		try {
			if (!(*this)[a]->OnBoot()) {
				return true;
			}
		} catch (const CModule::EModException& e) {
			if (e == CModule::UNLOAD) {
				UnloadModule((*this)[a]->GetModName());
			}
		}
	}

	return false;
}

bool CModules::OnPreRehash() { MODUNLOADCHK(OnPreRehash()); return false; }
bool CModules::OnPostRehash() { MODUNLOADCHK(OnPostRehash()); return false; }
bool CModules::OnIRCConnected() { MODUNLOADCHK(OnIRCConnected()); return false; }
bool CModules::OnIRCConnecting(CIRCSock *pIRCSock) { MODHALTCHK(OnIRCConnecting(pIRCSock)); }
bool CModules::OnIRCConnectionError(CIRCSock *pIRCSock) { MODUNLOADCHK(OnIRCConnectionError(pIRCSock)); return false; }
bool CModules::OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) { MODHALTCHK(OnIRCRegistration(sPass, sNick, sIdent, sRealName)); }
bool CModules::OnBroadcast(CString& sMessage) { MODHALTCHK(OnBroadcast(sMessage)); }
bool CModules::OnIRCDisconnected() { MODUNLOADCHK(OnIRCDisconnected()); return false; }
bool CModules::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) { MODUNLOADCHK(OnChanPermission(OpNick, Nick, Channel, uMode, bAdded, bNoChange)); return false; }
bool CModules::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnOp(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnDeop(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnVoice(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnDevoice(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) { MODUNLOADCHK(OnRawMode(OpNick, Channel, sModes, sArgs)); return false; }
bool CModules::OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) { MODUNLOADCHK(OnMode(OpNick, Channel, uMode, sArg, bAdded, bNoChange)); return false; }
bool CModules::OnRaw(CString& sLine) { MODHALTCHK(OnRaw(sLine)); }

bool CModules::OnClientLogin() { MODUNLOADCHK(OnClientLogin()); return false; }
bool CModules::OnClientDisconnect() { MODUNLOADCHK(OnClientDisconnect()); return false; }
bool CModules::OnUserRaw(CString& sLine) { MODHALTCHK(OnUserRaw(sLine)); }
bool CModules::OnUserCTCPReply(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserCTCPReply(sTarget, sMessage)); }
bool CModules::OnUserCTCP(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserCTCP(sTarget, sMessage)); }
bool CModules::OnUserAction(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserAction(sTarget, sMessage)); }
bool CModules::OnUserMsg(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserMsg(sTarget, sMessage)); }
bool CModules::OnUserNotice(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserNotice(sTarget, sMessage)); }
bool CModules::OnUserJoin(CString& sChannel, CString& sKey) { MODHALTCHK(OnUserJoin(sChannel, sKey)); }
bool CModules::OnUserPart(CString& sChannel, CString& sMessage) { MODHALTCHK(OnUserPart(sChannel, sMessage)); }
bool CModules::OnUserTopic(CString& sChannel, CString& sTopic) { MODHALTCHK(OnUserTopic(sChannel, sTopic)); }
bool CModules::OnUserTopicRequest(CString& sChannel) { MODHALTCHK(OnUserTopicRequest(sChannel)); }

bool CModules::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) { MODUNLOADCHK(OnQuit(Nick, sMessage, vChans)); return false; }
bool CModules::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) { MODUNLOADCHK(OnNick(Nick, sNewNick, vChans)); return false; }
bool CModules::OnKick(const CNick& Nick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) { MODUNLOADCHK(OnKick(Nick, sKickedNick, Channel, sMessage)); return false; }
bool CModules::OnJoin(const CNick& Nick, CChan& Channel) { MODUNLOADCHK(OnJoin(Nick, Channel)); return false; }
bool CModules::OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) { MODUNLOADCHK(OnPart(Nick, Channel, sMessage)); return false; }
bool CModules::OnInvite(const CNick& Nick, const CString& sChan) { MODHALTCHK(OnInvite(Nick, sChan)); }
bool CModules::OnChanBufferStarting(CChan& Chan, CClient& Client) { MODHALTCHK(OnChanBufferStarting(Chan, Client)); }
bool CModules::OnChanBufferEnding(CChan& Chan, CClient& Client) { MODHALTCHK(OnChanBufferEnding(Chan, Client)); }
bool CModules::OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine) { MODHALTCHK(OnChanBufferPlayLine(Chan, Client, sLine)); }
bool CModules::OnPrivBufferPlayLine(CClient& Client, CString& sLine) { MODHALTCHK(OnPrivBufferPlayLine(Client, sLine)); }
bool CModules::OnCTCPReply(CNick& Nick, CString& sMessage) { MODHALTCHK(OnCTCPReply(Nick, sMessage)); }
bool CModules::OnPrivCTCP(CNick& Nick, CString& sMessage) { MODHALTCHK(OnPrivCTCP(Nick, sMessage)); }
bool CModules::OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) { MODHALTCHK(OnChanCTCP(Nick, Channel, sMessage)); }
bool CModules::OnPrivAction(CNick& Nick, CString& sMessage) { MODHALTCHK(OnPrivAction(Nick, sMessage)); }
bool CModules::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) { MODHALTCHK(OnChanAction(Nick, Channel, sMessage)); }
bool CModules::OnPrivMsg(CNick& Nick, CString& sMessage) { MODHALTCHK(OnPrivMsg(Nick, sMessage)); }
bool CModules::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) { MODHALTCHK(OnChanMsg(Nick, Channel, sMessage)); }
bool CModules::OnPrivNotice(CNick& Nick, CString& sMessage) { MODHALTCHK(OnPrivNotice(Nick, sMessage)); }
bool CModules::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) { MODHALTCHK(OnChanNotice(Nick, Channel, sMessage)); }
bool CModules::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) { MODHALTCHK(OnTopic(Nick, Channel, sTopic)); }
bool CModules::OnTimerAutoJoin(CChan& Channel) { MODHALTCHK(OnTimerAutoJoin(Channel)); }
bool CModules::OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) { MODHALTCHK(OnAddNetwork(Network, sErrorRet)); }
bool CModules::OnDeleteNetwork(CIRCNetwork& Network) { MODHALTCHK(OnDeleteNetwork(Network)); }
bool CModules::OnStatusCommand(CString& sCommand) { MODHALTCHK(OnStatusCommand(sCommand)); }
bool CModules::OnModCommand(const CString& sCommand) { MODUNLOADCHK(OnModCommand(sCommand)); return false; }
bool CModules::OnModNotice(const CString& sMessage) { MODUNLOADCHK(OnModNotice(sMessage)); return false; }
bool CModules::OnModCTCP(const CString& sMessage) { MODUNLOADCHK(OnModCTCP(sMessage)); return false; }

// Why MODHALTCHK works only with functions returning EModRet ? :(
bool CModules::OnServerCapAvailable(const CString& sCap) {
	bool bResult = false;
	for (unsigned int a = 0; a < size(); ++a) {
		try {
			CModule* pMod = (*this)[a];
			CClient* pOldClient = pMod->GetClient();
			pMod->SetClient(m_pClient);
			if (m_pUser) {
				CUser* pOldUser = pMod->GetUser();
				pMod->SetUser(m_pUser);
				bResult |= pMod->OnServerCapAvailable(sCap);
				pMod->SetUser(pOldUser);
			} else {
				// WTF? Is that possible?
				bResult |= pMod->OnServerCapAvailable(sCap);
			}
			pMod->SetClient(pOldClient);
		} catch (const CModule::EModException& e) {
			if (CModule::UNLOAD == e) {
				UnloadModule((*this)[a]->GetModName());
			}
		}
	}
	return bResult;
}

bool CModules::OnServerCapResult(const CString& sCap, bool bSuccess) { MODUNLOADCHK(OnServerCapResult(sCap, bSuccess)); return false; }

////////////////////
// Global Modules //
////////////////////
bool CModules::OnAddUser(CUser& User, CString& sErrorRet) {
	MODHALTCHK(OnAddUser(User, sErrorRet));
}

bool CModules::OnDeleteUser(CUser& User) {
	MODHALTCHK(OnDeleteUser(User));
}

bool CModules::OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) {
	MODUNLOADCHK(OnClientConnect(pClient, sHost, uPort));
	return false;
}

bool CModules::OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
	MODHALTCHK(OnLoginAttempt(Auth));
}

bool CModules::OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
	MODUNLOADCHK(OnFailedLogin(sUsername, sRemoteIP));
	return false;
}

bool CModules::OnUnknownUserRaw(CClient* pClient, CString& sLine) {
	MODHALTCHK(OnUnknownUserRaw(pClient, sLine));
}

bool CModules::OnClientCapLs(CClient* pClient, SCString& ssCaps) {
	MODUNLOADCHK(OnClientCapLs(pClient, ssCaps));
	return false;
}

// Maybe create new macro for this?
bool CModules::IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState) {
	bool bResult = false;
	for (unsigned int a = 0; a < size(); ++a) {
		try {
			CModule* pMod = (CModule*) (*this)[a];
			CClient* pOldClient = pMod->GetClient();
			pMod->SetClient(m_pClient);
			if (m_pUser) {
				CUser* pOldUser = pMod->GetUser();
				pMod->SetUser(m_pUser);
				bResult |= pMod->IsClientCapSupported(pClient, sCap, bState);
				pMod->SetUser(pOldUser);
			} else {
				// WTF? Is that possible?
				bResult |= pMod->IsClientCapSupported(pClient, sCap, bState);
			}
			pMod->SetClient(pOldClient);
		} catch (const CModule::EModException& e) {
			if (CModule::UNLOAD == e) {
				UnloadModule((*this)[a]->GetModName());
			}
		}
	}
	return bResult;
}

bool CModules::OnClientCapRequest(CClient* pClient, const CString& sCap, bool bState) {
	MODUNLOADCHK(OnClientCapRequest(pClient, sCap, bState));
	return false;
}

bool CModules::OnModuleLoading(const CString& sModName, const CString& sArgs,
		CModInfo::EModuleType eType, bool& bSuccess, CString& sRetMsg) {
	MODHALTCHK(OnModuleLoading(sModName, sArgs, eType, bSuccess, sRetMsg));
}

bool CModules::OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg) {
	MODHALTCHK(OnModuleUnloading(pModule, bSuccess, sRetMsg));
}

bool CModules::OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
		bool& bSuccess, CString& sRetMsg) {
	MODHALTCHK(OnGetModInfo(ModInfo, sModule, bSuccess, sRetMsg));
}

bool CModules::OnGetAvailableMods(set<CModInfo>& ssMods, CModInfo::EModuleType eType) {
	MODUNLOADCHK(OnGetAvailableMods(ssMods, eType));
	return false;
}


CModule* CModules::FindModule(const CString& sModule) const {
	for (unsigned int a = 0; a < size(); a++) {
		if (sModule.Equals((*this)[a]->GetModName())) {
			return (*this)[a];
		}
	}

	return NULL;
}

bool CModules::LoadModule(const CString& sModule, const CString& sArgs, CModInfo::EModuleType eType, CUser* pUser, CIRCNetwork *pNetwork, CString& sRetMsg) {
	sRetMsg = "";

	if (FindModule(sModule) != NULL) {
		sRetMsg = "Module [" + sModule + "] already loaded.";
		return false;
	}

	bool bSuccess;
	bool bHandled = false;
	_GLOBALMODULECALL(OnModuleLoading(sModule, sArgs, eType, bSuccess, sRetMsg), pUser, pNetwork, NULL, &bHandled);
	if (bHandled) return bSuccess;

	CString sModPath, sDataPath;
	bool bVersionMismatch;
	CModInfo Info;

	if (!FindModPath(sModule, sModPath, sDataPath)) {
		sRetMsg = "Unable to find module [" + sModule + "]";
		return false;
	}

	ModHandle p = OpenModule(sModule, sModPath, bVersionMismatch, Info, sRetMsg);

	if (!p)
		return false;

	if (bVersionMismatch) {
		dlclose(p);
		sRetMsg = "Version mismatch, recompile this module.";
		return false;
	}

	if (!Info.SupportsType(eType)) {
		dlclose(p);
		sRetMsg = "Module [" + sModule + "] does not support module type ["
			+ CModInfo::ModuleTypeToString(eType) + "].";
		return false;
	}

	if (!pUser && eType == CModInfo::UserModule) {
		dlclose(p);
		sRetMsg = "Module [" + sModule + "] requires a user.";
		return false;
	}

	if (!pNetwork && eType == CModInfo::NetworkModule) {
		dlclose(p);
		sRetMsg = "Module [" + sModule + "] requires a network.";
		return false;
	}

	CModule* pModule = Info.GetLoader()(p, pUser, pNetwork, sModule, sDataPath);
	pModule->SetDescription(Info.GetDescription());
	pModule->SetType(eType);
	pModule->SetArgs(sArgs);
	pModule->SetModPath(CDir::ChangeDir(CZNC::Get().GetCurPath(), sModPath));
	push_back(pModule);

	bool bLoaded;
	try {
		bLoaded = pModule->OnLoad(sArgs, sRetMsg);
	} catch (const CModule::EModException&) {
		bLoaded = false;
		sRetMsg = "Caught an exception";
	}

	if (!bLoaded) {
		UnloadModule(sModule, sModPath);
		if (!sRetMsg.empty())
			sRetMsg = "Module [" + sModule + "] aborted: " + sRetMsg;
		else
			sRetMsg = "Module [" + sModule + "] aborted.";
		return false;
	}

	if (!sRetMsg.empty()) {
		sRetMsg += "[" + sRetMsg + "] ";
	}
	sRetMsg += "[" + sModPath + "]";
	return true;
}

bool CModules::UnloadModule(const CString& sModule) {
	CString s;
	return UnloadModule(sModule, s);
}

bool CModules::UnloadModule(const CString& sModule, CString& sRetMsg) {
	CString sMod = sModule;  // Make a copy incase the reference passed in is from CModule::GetModName()
	CModule* pModule = FindModule(sMod);
	sRetMsg = "";

	if (!pModule) {
		sRetMsg = "Module [" + sMod + "] not loaded.";
		return false;
	}

	bool bSuccess;
	bool bHandled = false;
	_GLOBALMODULECALL(OnModuleUnloading(pModule, bSuccess, sRetMsg), pModule->GetUser(), pModule->GetNetwork(), NULL, &bHandled);
	if (bHandled) return bSuccess;

	ModHandle p = pModule->GetDLL();

	if (p) {
		delete pModule;

		for (iterator it = begin(); it != end(); ++it) {
			if (*it == pModule) {
				erase(it);
				break;
			}
		}

		dlclose(p);
		sRetMsg = "Module [" + sMod + "] unloaded";

		return true;
	}

	sRetMsg = "Unable to unload module [" + sMod + "]";
	return false;
}

bool CModules::ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CIRCNetwork* pNetwork, CString& sRetMsg) {
	CString sMod = sModule;  // Make a copy incase the reference passed in is from CModule::GetModName()
	CModule *pModule = FindModule(sMod);

	if (!pModule) {
		sRetMsg = "Module [" + sMod + "] not loaded";
		return false;
	}

	CModInfo::EModuleType eType = pModule->GetType();
	pModule = NULL;

	sRetMsg = "";
	if (!UnloadModule(sMod, sRetMsg)) {
		return false;
	}

	if (!LoadModule(sMod, sArgs, eType, pUser, pNetwork, sRetMsg)) {
		return false;
	}

	sRetMsg = "Reloaded module [" + sMod + "]";
	return true;
}

bool CModules::GetModInfo(CModInfo& ModInfo, const CString& sModule, CString& sRetMsg) {
	CString sModPath, sTmp;

	bool bSuccess;
	bool bHandled = false;
	GLOBALMODULECALL(OnGetModInfo(ModInfo, sModule, bSuccess, sRetMsg), &bHandled);
	if (bHandled) return bSuccess;

	if (!FindModPath(sModule, sModPath, sTmp)) {
		sRetMsg = "Unable to find module [" + sModule + "]";
		return false;
	}

	return GetModPathInfo(ModInfo, sModule, sModPath, sRetMsg);
}

bool CModules::GetModPathInfo(CModInfo& ModInfo, const CString& sModule, const CString& sModPath, CString& sRetMsg) {
	bool bVersionMismatch;

	ModHandle p = OpenModule(sModule, sModPath, bVersionMismatch, ModInfo, sRetMsg);

	if (!p)
		return false;

	ModInfo.SetName(sModule);
	ModInfo.SetPath(sModPath);

	if (bVersionMismatch) {
		ModInfo.SetDescription("--- Version mismatch, recompile this module. ---");
	}

	dlclose(p);

	return true;
}

void CModules::GetAvailableMods(set<CModInfo>& ssMods, CModInfo::EModuleType eType) {
	ssMods.clear();

	unsigned int a = 0;
	CDir Dir;

	ModDirList dirs = GetModDirs();

	while (!dirs.empty()) {
		Dir.FillByWildcard(dirs.front().first, "*.so");
		dirs.pop();

		for (a = 0; a < Dir.size(); a++) {
			CFile& File = *Dir[a];
			CString sName = File.GetShortName();
			CString sPath = File.GetLongName();
			CModInfo ModInfo;
			sName.RightChomp(3);

			CString sIgnoreRetMsg;
			if (GetModPathInfo(ModInfo, sName, sPath, sIgnoreRetMsg)) {
				if (ModInfo.SupportsType(eType)) {
					ssMods.insert(ModInfo);
				}
			}
		}
	}

	GLOBALMODULECALL(OnGetAvailableMods(ssMods, eType), NOTHING);
}

bool CModules::FindModPath(const CString& sModule, CString& sModPath,
		CString& sDataPath) {
	CString sMod = sModule;
	CString sDir = sMod;
	if (sModule.find(".") == CString::npos)
		sMod += ".so";

	ModDirList dirs = GetModDirs();

	while (!dirs.empty()) {
		sModPath = dirs.front().first + sMod;
		sDataPath = dirs.front().second;
		dirs.pop();

		if (CFile::Exists(sModPath)) {
			sDataPath += sDir;
			return true;
		}
	}

	return false;
}

CModules::ModDirList CModules::GetModDirs() {
	ModDirList ret;
	CString sDir;

#ifdef RUN_FROM_SOURCE
	// ./modules
	sDir = CZNC::Get().GetCurPath() + "/modules/";
	ret.push(std::make_pair(sDir, sDir + "data/"));
#endif

	// ~/.znc/modules
	sDir = CZNC::Get().GetModPath() + "/";
	ret.push(std::make_pair(sDir, sDir));

	// <moduledir> and <datadir> (<prefix>/lib/znc)
	ret.push(std::make_pair(_MODDIR_ + CString("/"), _DATADIR_ + CString("/modules/")));

	return ret;
}

ModHandle CModules::OpenModule(const CString& sModule, const CString& sModPath, bool &bVersionMismatch,
		CModInfo& Info, CString& sRetMsg) {
	// Some sane defaults in case anything errors out below
	bVersionMismatch = false;
	sRetMsg.clear();

	for (unsigned int a = 0; a < sModule.length(); a++) {
		if (((sModule[a] < '0') || (sModule[a] > '9')) && ((sModule[a] < 'a') || (sModule[a] > 'z')) && ((sModule[a] < 'A') || (sModule[a] > 'Z')) && (sModule[a] != '_')) {
			sRetMsg = "Module names can only contain letters, numbers and underscores, [" + sModule + "] is invalid.";
			return NULL;
		}
	}

	// The second argument to dlopen() has a long history. It seems clear
	// that (despite what the man page says) we must include either of
	// RTLD_NOW and RTLD_LAZY and either of RTLD_GLOBAL and RTLD_LOCAL.
	//
	// RTLD_NOW vs. RTLD_LAZY: We use RTLD_NOW to avoid ZNC dying due to
	// failed symbol lookups later on. Doesn't really seem to have much of a
	// performance impact.
	//
	// RTLD_GLOBAL vs. RTLD_LOCAL: If perl is loaded with RTLD_LOCAL and later on
	// loads own modules (which it apparently does with RTLD_LAZY), we will die in a
	// name lookup since one of perl's symbols isn't found. That's worse
	// than any theoretical issue with RTLD_GLOBAL.
	ModHandle p = dlopen((sModPath).c_str(), RTLD_NOW | RTLD_GLOBAL);

	if (!p) {
		// dlerror() returns pointer to static buffer, which may be overwritten very soon with another dl call
		// also it may just return null.
		const char* cDlError = dlerror();
		CString sDlError = cDlError ? cDlError : "Unknown error";
		sRetMsg = "Unable to open module [" + sModule + "] [" + sDlError + "]";
		return NULL;
	}

	typedef bool (*InfoFP)(double, CModInfo&);
	InfoFP ZNCModInfo = (InfoFP) dlsym(p, "ZNCModInfo");

	if (!ZNCModInfo) {
		dlclose(p);
		sRetMsg = "Could not find ZNCModInfo() in module [" + sModule + "]";
		return NULL;
	}

	if (ZNCModInfo(CModule::GetCoreVersion(), Info)) {
		sRetMsg = "";
		bVersionMismatch = false;
	} else {
		bVersionMismatch = true;
		sRetMsg = "Version mismatch, recompile this module.";
	}

	return p;
}

CModCommand::CModCommand()
	: m_sCmd(), m_pFunc(NULL), m_sArgs(), m_sDesc()
{
}

CModCommand::CModCommand(const CString& sCmd, ModCmdFunc func, const CString& sArgs, const CString& sDesc)
	: m_sCmd(sCmd), m_pFunc(func), m_sArgs(sArgs), m_sDesc(sDesc)
{
}

CModCommand::CModCommand(const CModCommand& other)
	: m_sCmd(other.m_sCmd), m_pFunc(other.m_pFunc), m_sArgs(other.m_sArgs), m_sDesc(other.m_sDesc)
{
}

CModCommand& CModCommand::operator=(const CModCommand& other)
{
	m_sCmd = other.m_sCmd;
	m_pFunc = other.m_pFunc;
	m_sArgs = other.m_sArgs;
	m_sDesc = other.m_sDesc;
	return *this;
}

void CModCommand::InitHelp(CTable& Table) {
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");
}

void CModCommand::AddHelp(CTable& Table) const {
	Table.AddRow();
	Table.SetCell("Command", GetCommand());
	Table.SetCell("Arguments", GetArgs());
	Table.SetCell("Description", GetDescription());
}
