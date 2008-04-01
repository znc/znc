/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifdef _MODULES

#include "Modules.h"
#include "User.h"
#include "znc.h"
#include <dlfcn.h>

#ifndef RTLD_LOCAL
# define RTLD_LOCAL 0
# warning "your crap box doesnt define RTLD_LOCAL !?"
#endif

#define MODUNLOADCHK(func)								\
	for (unsigned int a = 0; a < size(); a++) {			\
		try {											\
			CModule* pMod = (*this)[a];					\
			if (m_pUser) {								\
				pMod->SetUser(m_pUser);					\
				pMod->func;								\
				pMod->SetUser(NULL);					\
			} else {									\
				pMod->func;								\
			}											\
		} catch (CModule::EModException e) {			\
			if (e == CModule::UNLOAD) {					\
				UnloadModule((*this)[a]->GetModName());	\
			}											\
		}												\
	}													\

#define GLOBALMODCALL(func)								\
	for (unsigned int a = 0; a < size(); a++) {			\
		try {											\
			CGlobalModule* pMod = (CGlobalModule*) (*this)[a];			\
			if (m_pUser) {								\
				pMod->SetUser(m_pUser);					\
				pMod->func;								\
				pMod->SetUser(NULL);					\
			} else {									\
				pMod->func;								\
			}											\
		} catch (CModule::EModException e) {			\
			if (e == CModule::UNLOAD) {					\
				UnloadModule((*this)[a]->GetModName());	\
			}											\
		}												\
	}													\

#define GLOBALMODHALTCHK(func)							\
	bool bHaltCore = false;								\
	for (unsigned int a = 0; a < size(); a++) {			\
		try {											\
			CGlobalModule* pMod = (CGlobalModule*) (*this)[a];			\
			CModule::EModRet e = CModule::CONTINUE;		\
			if (m_pUser) {								\
				pMod->SetUser(m_pUser);					\
				e = pMod->func;							\
				pMod->SetUser(NULL);					\
			} else {									\
				e = pMod->func;							\
			}											\
			if (e == CModule::HALTMODS) {				\
				break;									\
			} else if (e == CModule::HALTCORE) {		\
				bHaltCore = true;						\
			} else if (e == CModule::HALT) {			\
				bHaltCore = true;						\
				break;									\
			}											\
		} catch (CModule::EModException e) {			\
			if (e == CModule::UNLOAD) {					\
				UnloadModule((*this)[a]->GetModName());	\
			}											\
		}												\
	}													\
	return bHaltCore;
#define MODHALTCHK(func)								\
	bool bHaltCore = false;								\
	for (unsigned int a = 0; a < size(); a++) {			\
		try {											\
			CModule* pMod = (*this)[a];					\
			CModule::EModRet e = CModule::CONTINUE;		\
			if (m_pUser) {								\
				pMod->SetUser(m_pUser);					\
				e = pMod->func;							\
				pMod->SetUser(NULL);					\
			} else {									\
				e = pMod->func;							\
			}											\
			if (e == CModule::HALTMODS) {				\
				break;									\
			} else if (e == CModule::HALTCORE) {		\
				bHaltCore = true;						\
			} else if (e == CModule::HALT) {			\
				bHaltCore = true;						\
				break;									\
			}											\
		} catch (CModule::EModException e) {			\
			if (e == CModule::UNLOAD) {					\
				UnloadModule((*this)[a]->GetModName());	\
			}											\
		}												\
	}													\
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

/////////////////// Socket ///////////////////
CSocket::CSocket(CModule* pModule, const CString& sLabel) : Csock() {
	m_pModule = pModule;
	m_sLabel = sLabel;
	m_pModule->AddSocket(this);
	EnableReadLine();
}

CSocket::CSocket(CModule* pModule, const CString& sLabel, const CString& sHostname, unsigned short uPort, int iTimeout) : Csock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_sLabel = sLabel;
	m_pModule->AddSocket(this);
	EnableReadLine();
}

CSocket::~CSocket() {
	m_pModule->UnlinkSocket(this);
}

bool CSocket::Connect(const CString& sHostname, unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::C::" + m_pModule->GetModName();
	CString sVHost;

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
		sVHost = m_pModule->GetUser()->GetVHost();
	}

	return m_pModule->GetManager()->Connect(sHostname, uPort, sSockName, uTimeout, bSSL, sVHost, (Csock*) this);
}

bool CSocket::Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::L::" + m_pModule->GetModName();

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
	}

	return m_pModule->GetManager()->ListenAll(uPort, sSockName, bSSL, SOMAXCONN, (Csock*) this);
}

bool CSocket::PutIRC(const CString& sLine) {
	return (m_pModule) ? m_pModule->PutIRC(sLine) : false;
}

bool CSocket::PutUser(const CString& sLine) {
	return (m_pModule) ? m_pModule->PutUser(sLine) : false;
}

bool CSocket::PutStatus(const CString& sLine) {
	return (m_pModule) ? m_pModule->PutStatus(sLine) : false;
}

bool CSocket::PutModule(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pModule) ? m_pModule->PutModule(sLine, sIdent, sHost) : false;
}
bool CSocket::PutModNotice(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pModule) ? m_pModule->PutModNotice(sLine, sIdent, sHost) : false;
}

void CSocket::SetModule(CModule* p) { m_pModule = p; }
void CSocket::SetLabel(const CString& s) { m_sLabel = s; }

CModule* CSocket::GetModule() const { return m_pModule; }
const CString& CSocket::GetLabel() const { return m_sLabel; }
/////////////////// !Socket ///////////////////

CModule::CModule(void* pDLL, CUser* pUser, const CString& sModName, const CString& sDataDir) {
	m_bFake = false;
	m_pDLL = pDLL;
	m_pManager = &(CZNC::Get().GetManager());;
	m_pUser = pUser;
	m_pClient = NULL;
	m_sModName = sModName;
	m_sDataDir = sDataDir;

	if (m_pUser) {
		m_sSavePath = m_pUser->GetUserPath() + "/moddata/" + m_sModName;
		LoadRegistry();
	}
}

CModule::CModule(void* pDLL, const CString& sModName, const CString& sDataDir) {
	m_bFake = false;
	m_pDLL = pDLL;
	m_pManager = &(CZNC::Get().GetManager());
	m_pUser = NULL;
	m_pClient = NULL;
	m_sModName = sModName;
	m_sDataDir = sDataDir;

	m_sSavePath = CZNC::Get().GetZNCPath() + "/moddata/" + m_sModName;
	LoadRegistry();
}

CModule::~CModule() {
	while (m_vTimers.size()) {
		RemTimer(m_vTimers[0]->GetName());
	}

	while (m_vSockets.size()) {
		RemSocket(m_vSockets[0]);
	}

	SaveRegistry();
}

void CModule::SetUser(CUser* pUser) { m_pUser = pUser; }
void CModule::SetClient(CClient* pClient) { m_pClient = pClient; }
void CModule::Unload() { throw UNLOAD; }

bool CModule::LoadRegistry() {
	//CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
	return (m_mssRegistry.ReadFromDisk(GetSavePath() + "/.registry", 0600) == MCString::MCS_SUCCESS);
}

bool CModule::SaveRegistry() {
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

CString CModule::GetNV(const CString & sName) {
	MCString::iterator it = m_mssRegistry.find(sName);

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

bool CModule::AddTimer(CTimer* pTimer) {
	if ((!pTimer) || (FindTimer(pTimer->GetName()))) {
		delete pTimer;
		return false;
	}

	m_pManager->AddCron(pTimer);
	m_vTimers.push_back(pTimer);
	return true;
}

bool CModule::AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles, const CString& sDescription) {
	CFPTimer *pTimer = new CFPTimer(this, uInterval, uCycles, sLabel, sDescription);
	pTimer->SetFPCallback(pFBCallback);

	return AddTimer(pTimer);
}

bool CModule::RemTimer(const CString& sLabel) {
	for (unsigned int a = 0; a < m_vTimers.size(); a++) {
		CTimer* pTimer = m_vTimers[a];

		if (pTimer->GetName().CaseCmp(sLabel) == 0) {
			m_vTimers.erase(m_vTimers.begin() +a);
			m_pManager->DelCronByAddr(pTimer);
			return true;
		}
	}

	return false;
}

bool CModule::UnlinkTimer(CTimer* pTimer) {
	for (unsigned int a = 0; a < m_vTimers.size(); a++) {
		if (pTimer == m_vTimers[a]) {
			m_vTimers.erase(m_vTimers.begin() +a);
			return true;
		}
	}

	return false;
}

CTimer* CModule::FindTimer(const CString& sLabel) {
	for (unsigned int a = 0; a < m_vTimers.size(); a++) {
		CTimer* pTimer = m_vTimers[a];
		if (pTimer->GetName().CaseCmp(sLabel) == 0) {
			return pTimer;
		}
	}

	return NULL;
}

void CModule::ListTimers() {
	if (!m_vTimers.size()) {
		PutModule("You have no timers running.");
		return;
	}

	CTable Table;
	Table.AddColumn("Name");
	Table.AddColumn("Secs");
	Table.AddColumn("Cycles");
	Table.AddColumn("Description");

	for (unsigned int a = 0; a < m_vTimers.size(); a++) {
		CTimer* pTimer = (CTimer*) m_vTimers[a];
		unsigned int uCycles = pTimer->GetCyclesLeft();

		Table.AddRow();
		Table.SetCell("Name", pTimer->GetName());
		Table.SetCell("Secs", CString(pTimer->GetInterval()));
		Table.SetCell("Cycles", ((uCycles) ? CString(uCycles) : "INF"));
		Table.SetCell("Description", pTimer->GetDescription());
	}

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutModule(sLine);
		}
	}
}

bool CModule::AddSocket(CSocket* pSocket) {
	if (!pSocket) {
		return false;
	}

	m_vSockets.push_back(pSocket);
	return true;
}

bool CModule::RemSocket(CSocket* pSocket) {
	for (unsigned int a = 0; a < m_vSockets.size(); a++) {
		if (m_vSockets[a] == pSocket) {
			m_vSockets.erase(m_vSockets.begin() +a);
			m_pManager->DelSockByAddr(pSocket);
			return true;
		}
	}

	return false;
}

bool CModule::RemSocket(const CString& sLabel) {
	for (unsigned int a = 0; a < m_vSockets.size(); a++) {
		CSocket* pSocket = m_vSockets[a];

		if (pSocket->GetLabel().CaseCmp(sLabel) == 0) {
			m_vSockets.erase(m_vSockets.begin() +a);
			m_pManager->DelSockByAddr(pSocket);
			return true;
		}
	}

	return false;
}

bool CModule::UnlinkSocket(CSocket* pSocket) {
	for (unsigned int a = 0; a < m_vSockets.size(); a++) {
		if (pSocket == m_vSockets[a]) {
			m_vSockets.erase(m_vSockets.begin() +a);
			return true;
		}
	}

	return false;
}

CSocket* CModule::FindSocket(const CString& sLabel) {
	for (unsigned int a = 0; a < m_vSockets.size(); a++) {
		CSocket* pSocket = m_vSockets[a];
		if (pSocket->GetLabel().CaseCmp(sLabel) == 0) {
			return pSocket;
		}
	}

	return NULL;
}

void CModule::ListSockets() {
	if (!m_vSockets.size()) {
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

	for (unsigned int a = 0; a < m_vSockets.size(); a++) {
		CSocket* pSocket = (CSocket*) m_vSockets[a];

		Table.AddRow();
		Table.SetCell("Name", pSocket->GetLabel());

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

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutModule(sLine);
		}
	}
}

CString CModule::GetModNick() const { return ((m_pUser) ? m_pUser->GetStatusPrefix() : "*") + m_sModName; }

bool CModule::OnLoad(const CString& sArgs, CString& sMessage) { sMessage = ""; return true; }
bool CModule::OnBoot() { return true; }
void CModule::OnRehashDone() {}
void CModule::OnIRCDisconnected() {}
void CModule::OnIRCConnected() {}
CModule::EModRet CModule::OnBroadcast(CString& sMessage) { return CONTINUE; }

CModule::EModRet CModule::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) { return CONTINUE; }

void CModule::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {}
void CModule::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {}

CModule::EModRet CModule::OnRaw(CString& sLine) { return CONTINUE; }

CModule::EModRet CModule::OnStatusCommand(const CString& sCommand) { return CONTINUE; }
void CModule::OnModCommand(const CString& sCommand) {}
void CModule::OnModNotice(const CString& sMessage) {}
void CModule::OnModCTCP(const CString& sMessage) {}

void CModule::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {}
void CModule::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {}
void CModule::OnKick(const CNick& Nick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {}
void CModule::OnJoin(const CNick& Nick, CChan& Channel) {}
void CModule::OnPart(const CNick& Nick, CChan& Channel) {}

void CModule::OnUserAttached() {}
void CModule::OnUserDetached() {}
CModule::EModRet CModule::OnUserRaw(CString& sLine) { return CONTINUE; }
CModule::EModRet CModule::OnUserCTCPReply(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserCTCP(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserAction(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserMsg(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserNotice(CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserJoin(CString& sChannel, CString& sKey) { return CONTINUE; }
CModule::EModRet CModule::OnUserPart(CString& sChannel, CString& sMessage) { return CONTINUE; }

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

void* CModule::GetDLL() { return m_pDLL; }
bool CModule::PutIRC(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutIRC(sLine) : false;
}
bool CModule::PutUser(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutUser(sLine, m_pClient) : false;
}
bool CModule::PutStatus(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutStatus(sLine, m_pClient) : false;
}
bool CModule::PutModule(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pUser) ? m_pUser->PutUser(":" + GetModNick() + "!" + sIdent + "@" + sHost + " PRIVMSG " + m_pUser->GetCurNick() + " :" + sLine, m_pClient) : false;
}
bool CModule::PutModNotice(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pUser) ? m_pUser->PutUser(":" + GetModNick() + "!" + sIdent + "@" + sHost + " NOTICE " + m_pUser->GetCurNick() + " :" + sLine, m_pClient) : false;
}


///////////////////
// CGlobalModule //
///////////////////
CModule::EModRet CGlobalModule::OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan) { return CONTINUE; }
CModule::EModRet CGlobalModule::OnDeleteUser(CUser& User) { return CONTINUE; }
CModule::EModRet CGlobalModule::OnLoginAttempt(CSmartPtr<CAuthBase> Auth) { return CONTINUE; }
void CGlobalModule::OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {}


CModules::CModules() {
	m_pUser = NULL;
}

CModules::~CModules() {
	UnloadAll();
}

void CModules::UnloadAll() {
	while (size()) {
		CString sRetMsg;
		CString sModName = (*this)[0]->GetModName();
		UnloadModule(sModName, sRetMsg);
	}
}

void CModules::SetClient(CClient* pClient) {
	for (unsigned int a = 0; a < size(); a++) {
		(*this)[a]->SetClient(pClient);
	}
}

bool CModules::OnBoot() {
	for (unsigned int a = 0; a < size(); a++) {
		if (!(*this)[a]->OnBoot()) {
			return false;
		}
	}

	return true;
}

bool CModules::OnRehashDone() { MODUNLOADCHK(OnRehashDone()); return false; }
bool CModules::OnIRCConnected() { MODUNLOADCHK(OnIRCConnected()); return false; }
bool CModules::OnBroadcast(CString& sMessage) { MODHALTCHK(OnBroadcast(sMessage)); }
bool CModules::OnIRCDisconnected() { MODUNLOADCHK(OnIRCDisconnected()); return false; }
bool CModules::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) { MODHALTCHK(OnDCCUserSend(RemoteNick, uLongIP, uPort, sFile, uFileSize)); }
bool CModules::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) { MODUNLOADCHK(OnChanPermission(OpNick, Nick, Channel, uMode, bAdded, bNoChange)); return false; }
bool CModules::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnOp(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnDeop(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnVoice(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) { MODUNLOADCHK(OnDevoice(OpNick, Nick, Channel, bNoChange)); return false; }
bool CModules::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) { MODUNLOADCHK(OnRawMode(OpNick, Channel, sModes, sArgs)); return false; }
bool CModules::OnRaw(CString& sLine) { MODHALTCHK(OnRaw(sLine)); }

bool CModules::OnUserAttached() { MODUNLOADCHK(OnUserAttached()); return false; }
bool CModules::OnUserDetached() { MODUNLOADCHK(OnUserDetached()); return false; }
bool CModules::OnUserRaw(CString& sLine) { MODHALTCHK(OnUserRaw(sLine)); }
bool CModules::OnUserCTCPReply(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserCTCPReply(sTarget, sMessage)); }
bool CModules::OnUserCTCP(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserCTCP(sTarget, sMessage)); }
bool CModules::OnUserAction(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserAction(sTarget, sMessage)); }
bool CModules::OnUserMsg(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserMsg(sTarget, sMessage)); }
bool CModules::OnUserNotice(CString& sTarget, CString& sMessage) { MODHALTCHK(OnUserNotice(sTarget, sMessage)); }
bool CModules::OnUserJoin(CString& sChannel, CString& sKey) { MODHALTCHK(OnUserJoin(sChannel, sKey)); }
bool CModules::OnUserPart(CString& sChannel, CString& sMessage) { MODHALTCHK(OnUserPart(sChannel, sMessage)); }

bool CModules::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) { MODUNLOADCHK(OnQuit(Nick, sMessage, vChans)); return false; }
bool CModules::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) { MODUNLOADCHK(OnNick(Nick, sNewNick, vChans)); return false; }
bool CModules::OnKick(const CNick& Nick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) { MODUNLOADCHK(OnKick(Nick, sKickedNick, Channel, sMessage)); return false; }
bool CModules::OnJoin(const CNick& Nick, CChan& Channel) { MODUNLOADCHK(OnJoin(Nick, Channel)); return false; }
bool CModules::OnPart(const CNick& Nick, CChan& Channel) { MODUNLOADCHK(OnPart(Nick, Channel)); return false; }
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
bool CModules::OnStatusCommand(const CString& sCommand) { MODHALTCHK(OnStatusCommand(sCommand)); }
bool CModules::OnModCommand(const CString& sCommand) { MODUNLOADCHK(OnModCommand(sCommand)); return false; }
bool CModules::OnModNotice(const CString& sMessage) { MODUNLOADCHK(OnModNotice(sMessage)); return false; }
bool CModules::OnModCTCP(const CString& sMessage) { MODUNLOADCHK(OnModCTCP(sMessage)); return false; }

////////////////////
// CGlobalModules //
////////////////////
bool CGlobalModules::OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan) {
	GLOBALMODHALTCHK(OnConfigLine(sName, sValue, pUser, pChan));
}

bool CGlobalModules::OnDeleteUser(CUser& User) {
	GLOBALMODHALTCHK(OnDeleteUser(User));
}

bool CGlobalModules::OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
	GLOBALMODHALTCHK(OnLoginAttempt(Auth));
}

void CGlobalModules::OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
	GLOBALMODCALL(OnFailedLogin(sUsername, sRemoteIP));
}

CModule* CModules::FindModule(const CString& sModule) const {
	for (unsigned int a = 0; a < size(); a++) {
		if (sModule.CaseCmp((*this)[a]->GetModName()) == 0) {
			return (*this)[a];
		}
	}

	return NULL;
}

bool CModules::LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg, bool bFake) {
#ifndef _MODULES
	sRetMsg = "Unable to load module [" + sModule + "] module support was not enabled.";
	return false;
#else
	sRetMsg = "";

	for (unsigned int a = 0; a < sModule.length(); a++) {
		if (((sModule[a] < '0') || (sModule[a] > '9')) && ((sModule[a] < 'a') || (sModule[a] > 'z')) && ((sModule[a] < 'A') || (sModule[a] > 'Z')) && (sModule[a] != '_')) {
			sRetMsg = "Unable to load module [" + sModule + "] module names can only be letters, numbers, or underscores.";
			return false;
		}
	}

	if (FindModule(sModule) != NULL) {
		sRetMsg = "Module [" + sModule + "] already loaded.";
		return false;
	}

	CString sModPath, sDataPath;

	if (!CZNC::Get().FindModPath(sModule, sModPath, sDataPath)) {
		sRetMsg = "Unable to find module [" + sModule + "]";
		return false;
	}

	if (bFake) {
		CModule* pModule = new CModule(NULL, sModule, sDataPath);
		pModule->SetArgs(sArgs);
		pModule->SetDescription("<<Fake Module>>");
		pModule->SetFake(true);
		push_back(pModule);
		sRetMsg = "Loaded fake module [" + sModule + "] [" + sModPath + "]";
		return true;
	}

	unsigned int uDLFlags = RTLD_LAZY;
	uDLFlags |= (pUser) ? RTLD_LOCAL : RTLD_GLOBAL;

	void* p = dlopen((sModPath).c_str(), uDLFlags);

	if (!p) {
		sRetMsg = "Unable to load module [" + sModule + "] [" + dlerror() + "]";
		return false;
	}

	typedef double (*dFP)();
	dFP Version = (dFP) dlsym(p, "GetVersion");

	if (!Version) {
		dlclose(p);
		sRetMsg = "Could not find Version() in module [" + sModule + "]";
		return false;
	}

	if (CModule::GetCoreVersion() != Version()) {
		dlclose(p);
		sRetMsg = "Version mismatch, recompile this module.";
		throw CException(CException::EX_BadModVersion);
		return false;
	}

	typedef bool (*bFP)();
	bFP IsGlobal = (bFP) dlsym(p, "IsGlobal");

	if (!IsGlobal) {
		dlclose(p);
		sRetMsg = "Could not find IsGlobal() in module [" + sModule + "]";
		return false;
	}

	typedef CString (*sFP)();
	sFP GetDesc = (sFP) dlsym(p, "GetDescription");

	if (!GetDesc) {
		dlclose(p);
		sRetMsg = "Could not find GetDescription() in module [" + sModule + "]";
		return false;
	}

	bool bIsGlobal = IsGlobal();
	if ((pUser == NULL) != bIsGlobal) {
		dlclose(p);
		sRetMsg = "Module [" + sModule + "] is ";
		sRetMsg += (bIsGlobal) ? "" : "not ";
		sRetMsg += "a global module.";
		return false;
	}

	CModule* pModule = NULL;

	if (pUser) {
		typedef CModule* (*fp)(void*, CUser* pUser,
				const CString& sModName, const CString& sDataPath);
		fp Load = (fp) dlsym(p, "Load");

		if (!Load) {
			dlclose(p);
			sRetMsg = "Could not find Load() in module [" + sModule + "]";
			return false;
		}

		pModule = Load(p, pUser, sModule, sDataPath);
	} else {
		typedef CModule* (*fp)(void*, const CString& sModName,
				const CString& sDataPath);
		fp Load = (fp) dlsym(p, "Load");

		if (!Load) {
			dlclose(p);
			sRetMsg = "Could not find Load() in module [" + sModule + "]";
			return false;
		}

		pModule = Load(p, sModule, sDataPath);
	}

	pModule->SetDescription(GetDesc());
	push_back(pModule);

	if (!pModule->OnLoad(sArgs, sRetMsg)) {
		UnloadModule(sModule, sModPath);
		if (!sRetMsg.empty())
			sRetMsg = "Module [" + sModule + "] aborted: " + sRetMsg;
		else
			sRetMsg = "Module [" + sModule + "] aborted.";
		return false;
	}

	pModule->SetArgs(sArgs);

	if (!sRetMsg.empty()) {
		sRetMsg = "Loaded module [" + sModule + "] [" + sRetMsg + "] [" + sModPath + "]";
	} else {
		sRetMsg = "Loaded module [" + sModule + "] [" + sModPath + "]";
	}
	return true;
#endif // !_MODULES
}

bool CModules::UnloadModule(const CString& sModule) {
	CString s;
	return UnloadModule(sModule, s);
}

bool CModules::UnloadModule(const CString& sModule, CString& sRetMsg) {
	CString sMod = sModule;	// Make a copy incase the reference passed in is from CModule::GetModName()
#ifndef _MODULES
	sRetMsg = "Unable to unload module [" + sMod + "] module support was not enabled.";
	return false;
#else
	CModule* pModule = FindModule(sMod);
	sRetMsg = "";

	if (!pModule) {
		sRetMsg = "Module [" + sMod + "] not loaded.";
		return false;
	}

	if (pModule->IsFake()) {
		for (iterator it = begin(); it != end(); it++) {
			if (*it == pModule) {
				erase(it);
				sRetMsg = "Fake module [" + sMod + "] unloaded";
				return true;
			}
		}

		sRetMsg = "Fake module [" + sMod + "] not loaded.";
		return false;
	}

	void* p = pModule->GetDLL();

	if (p) {
		typedef void (*fp)(CModule*);
		fp Unload = (fp)dlsym(p, "Unload");

		if (Unload) {
			Unload(pModule);

			for (iterator it = begin(); it != end(); it++) {
				if (*it == pModule) {
					erase(it);
					break;
				}
			}

			dlclose(p);
			sRetMsg = "Module [" + sMod + "] unloaded";

			return true;
		} else {
			sRetMsg = "Unable to unload module [" + sMod + "] could not find Unload()";
			return false;
		}
	}

	sRetMsg = "Unable to unload module [" + sMod + "]";
	return false;
#endif // !_MODULES
}

bool CModules::ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg) {
	CString sMod = sModule;	// Make a copy incase the reference passed in is from CModule::GetModName()
	sRetMsg = "";
	if (!UnloadModule(sMod, sRetMsg)) {
		return false;
	}

	try {
		if (!LoadModule(sMod, sArgs, pUser, sRetMsg)) {
			return false;
		}
	} catch(...) {
		return false;
	}

	sRetMsg = "Reloaded module [" + sMod + "]";
	return true;
}

bool CModules::GetModInfo(CModInfo& ModInfo, const CString& sModule) {
	for (unsigned int a = 0; a < sModule.length(); a++) {
		const char& c = sModule[a];

		if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
			return false;
		}
	}

	CString sModPath, sTmp;

	if (!CZNC::Get().FindModPath(sModule, sModPath, sTmp)) {
		return false;
	}

	unsigned int uDLFlags = RTLD_LAZY | RTLD_GLOBAL;

	void* p = dlopen((sModPath).c_str(), uDLFlags);

	if (!p) {
		return false;
	}

	typedef double (*dFP)();
	dFP Version = (dFP) dlsym(p, "GetVersion");

	if (!Version) {
		dlclose(p);
		return false;
	}

	typedef bool (*bFP)();
	bFP IsGlobal = (bFP) dlsym(p, "IsGlobal");

	if (!IsGlobal) {
		dlclose(p);
		return false;
	}

	typedef CString (*sFP)();
	sFP GetDescription = (sFP) dlsym(p, "GetDescription");

	if (!GetDescription) {
		dlclose(p);
		return false;
	}

	ModInfo.SetGlobal(IsGlobal());
	ModInfo.SetDescription(GetDescription());
	ModInfo.SetName(sModule);
	ModInfo.SetPath(sModPath);
	dlclose(p);

	return true;
}

void CModules::GetAvailableMods(set<CModInfo>& ssMods, bool bGlobal) {
	ssMods.clear();

	unsigned int a = 0;
	CDir Dir;

	Dir.FillByWildcard(CZNC::Get().GetCurPath() + "/modules", "*.so");
	for (a = 0; a < Dir.size(); a++) {
		CFile& File = *Dir[a];
		CString sName = File.GetShortName();
		CModInfo ModInfo;
		sName.RightChomp(3);

		if (GetModInfo(ModInfo, sName)) {
			if (ModInfo.IsGlobal() == bGlobal) {
				ModInfo.SetSystem(false);
				ssMods.insert(ModInfo);
			}
		}
	}

	Dir.FillByWildcard(CZNC::Get().GetModPath(), "*.so");
	for (a = 0; a < Dir.size(); a++) {
		CFile& File = *Dir[a];
		CString sName = File.GetShortName();
		CModInfo ModInfo;
		sName.RightChomp(3);

		if (GetModInfo(ModInfo, sName)) {
			if (ModInfo.IsGlobal() == bGlobal) {
				ModInfo.SetSystem(false);
				ssMods.insert(ModInfo);
			}
		}
	}

	Dir.FillByWildcard(_MODDIR_, "*.so");
	for (a = 0; a < Dir.size(); a++) {
		CFile& File = *Dir[a];
		CString sName = File.GetShortName();
		CModInfo ModInfo;
		sName.RightChomp(3);

		if (GetModInfo(ModInfo, sName)) {
			if (ModInfo.IsGlobal() == bGlobal) {
				ModInfo.SetSystem(true);
				ssMods.insert(ModInfo);
			}
		}
	}
}

#endif // _MODULES
