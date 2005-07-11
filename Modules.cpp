#include "main.h"
#include "Modules.h"
#include "znc.h"
#include "Utils.h"
#include "User.h"
#include "Nick.h"
#include "Chan.h"

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
	CString sSockName = "MOD::C::" + m_pModule->GetModName() + "::" + m_pModule->GetUser()->GetUserName();
	return m_pModule->GetManager()->Connect(sHostname, uPort, sSockName, uTimeout, bSSL, m_pModule->GetUser()->GetVHost(), (Csock*) this);
}

bool CSocket::Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	CString sSockName = "MOD::L::" + m_pModule->GetModName() + "::" + m_pModule->GetUser()->GetUserName();
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

CModule::CModule(void* pDLL, CUser* pUser, const CString& sModName) {
	m_pDLL = pDLL;
	m_pZNC = (pUser) ? pUser->GetZNC() : NULL;
	m_pManager = (pUser) ? pUser->GetManager() : NULL;
	m_pUser = pUser;
	m_sModName = sModName;

	if (m_pUser) {
		m_sSavePath = m_pUser->GetUserPath() + "/moddata/" + m_sModName;
		LoadRegistry();
	}
}

CModule::CModule(void* pDLL, CZNC* pZNC, const CString& sModName) {
	m_pDLL = pDLL;
	m_pZNC = pZNC;
	m_pManager = (pZNC) ? &pZNC->GetManager() : NULL;
	m_pUser = NULL;
	m_sModName = sModName;

	if (m_pZNC) {
		m_sSavePath = m_pZNC->GetZNCPath() + "/moddata/" + m_sModName;
		LoadRegistry();
	}
}

CModule::~CModule() {
	while (m_vTimers.size()) {
		RemTimer(m_vTimers[0]->GetName());
	}

	while (m_vSockets.size()) {
		RemSocket(m_vSockets[0]->GetLabel());
	}

	SaveRegistry();
}

void CModule::SetUser(CUser* pUser) { m_pUser = pUser; }
void CModule::Unload() { throw UNLOAD; }

bool CModule::LoadRegistry() {
	if (!m_pZNC) {
		return false;
	}

	//CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";
	return (m_mssRegistry.ReadFromDisk(GetSavePath() + "/.registry", 0600) == MCString::MCS_SUCCESS);
}

bool CModule::SaveRegistry() {
	if (!m_pZNC) {
		return false;
	}

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
		Table.SetCell("Secs", CString::ToString(pTimer->GetInterval()));
		Table.SetCell("Cycles", ((uCycles) ? CString::ToString(uCycles) : "INF"));
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

		Table.SetCell("LocalPort", CString::ToString(pSocket->GetLocalPort()));
		Table.SetCell("SSL", (pSocket->GetSSL() ? "yes" : "no"));
		Table.SetCell("RemoteIP", pSocket->GetRemoteIP());
		Table.SetCell("RemotePort", (pSocket->GetRemotePort()) ? CString::ToString(pSocket->GetRemotePort()) : CString(""));
	}

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutModule(sLine);
		}
	}
}

const CString& CModule::GetModName() const { return m_sModName; }
CString CModule::GetModNick() const { return ((m_pUser) ? m_pUser->GetStatusPrefix() : "*") + m_sModName; }

bool CModule::OnLoad(const CString& sArgs) { return true; }
bool CModule::OnBoot() { return true; }
void CModule::OnUserAttached() {}
void CModule::OnUserDetached() {}
void CModule::OnIRCDisconnected() {}
void CModule::OnIRCConnected() {}

CModule::EModRet CModule::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) { return CONTINUE; }

void CModule::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {}
void CModule::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {}
void CModule::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {}

CModule::EModRet CModule::OnUserRaw(CString& sLine) { return CONTINUE; }
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

CModule::EModRet CModule::OnUserCTCPReply(const CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnCTCPReply(const CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserCTCP(const CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivCTCP(const CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanCTCP(const CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserMsg(const CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivMsg(const CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanMsg(const CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnUserNotice(const CString& sTarget, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnPrivNotice(const CNick& Nick, CString& sMessage) { return CONTINUE; }
CModule::EModRet CModule::OnChanNotice(const CNick& Nick, CChan& Channel, CString& sMessage) { return CONTINUE; }

void* CModule::GetDLL() { return m_pDLL; }
bool CModule::PutIRC(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutIRC(sLine) : false;
}
bool CModule::PutUser(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutUser(sLine) : false;
}
bool CModule::PutStatus(const CString& sLine) {
	return (m_pUser) ? m_pUser->PutStatus(sLine) : false;
}
bool CModule::PutModule(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pUser) ? m_pUser->PutUser(":" + GetModNick() + "!" + sIdent + "@" + sHost + " PRIVMSG " + m_pUser->GetCurNick() + " :" + sLine) : false;
}
bool CModule::PutModNotice(const CString& sLine, const CString& sIdent, const CString& sHost) {
	return (m_pUser) ? m_pUser->PutUser(":" + GetModNick() + "!" + sIdent + "@" + sHost + " NOTICE " + m_pUser->GetCurNick() + " :" + sLine) : false;
}


///////////////////
// CGlobalModule //
///////////////////
CModule::EModRet CGlobalModule::OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan) { return CONTINUE; }

CModules::CModules(CZNC* pZNC) {
	m_pZNC = pZNC;
	m_pUser = NULL;
}

CModules::~CModules() {}

void CModules::UnloadAll() {
	while (size()) {
		CString sRetMsg;
		CString sModName = (*this)[0]->GetModName();
		UnloadModule(sModName, sRetMsg);
	}
}

bool CModules::OnLoad(const CString& sArgs) {
	for (unsigned int a = 0; a < size(); a++) {
		if (!(*this)[a]->OnLoad(sArgs)) {
			return false;
		}
	}

	return true;
}

bool CModules::OnBoot() {
	for (unsigned int a = 0; a < size(); a++) {
		if (!(*this)[a]->OnBoot()) {
			return false;
		}
	}

	return true;
}

void CModules::OnIRCConnected() {
	MODUNLOADCHK(OnIRCConnected());
}

void CModules::OnUserAttached() {
	MODUNLOADCHK(OnUserAttached());
}

void CModules::OnUserDetached() {
	MODUNLOADCHK(OnUserDetached());
}

void CModules::OnIRCDisconnected() {
	MODUNLOADCHK(OnIRCDisconnected());
}

bool CModules::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) {
	MODHALTCHK(OnDCCUserSend(RemoteNick, uLongIP, uPort, sFile, uFileSize));
}

void CModules::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {
	MODUNLOADCHK(OnChanPermission(OpNick, Nick, Channel, uMode, bAdded, bNoChange));
}

void CModules::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	MODUNLOADCHK(OnOp(OpNick, Nick, Channel, bNoChange));
}

void CModules::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	MODUNLOADCHK(OnDeop(OpNick, Nick, Channel, bNoChange));
}

void CModules::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	MODUNLOADCHK(OnVoice(OpNick, Nick, Channel, bNoChange));
}

void CModules::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	MODUNLOADCHK(OnDevoice(OpNick, Nick, Channel, bNoChange));
}

void CModules::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
	MODUNLOADCHK(OnRawMode(OpNick, Channel, sModes, sArgs));
}

bool CModules::OnRaw(CString& sLine) {
	MODHALTCHK(OnRaw(sLine));
}

bool CModules::OnUserRaw(CString& sLine) {
	MODHALTCHK(OnUserRaw(sLine));
}

void CModules::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
	MODUNLOADCHK(OnQuit(Nick, sMessage, vChans));
}

void CModules::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {
	MODUNLOADCHK(OnNick(Nick, sNewNick, vChans));
}

void CModules::OnKick(const CNick& Nick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
	MODUNLOADCHK(OnKick(Nick, sKickedNick, Channel, sMessage));
}

void CModules::OnJoin(const CNick& Nick, CChan& Channel) {
	MODUNLOADCHK(OnJoin(Nick, Channel));
}

void CModules::OnPart(const CNick& Nick, CChan& Channel) {
	MODUNLOADCHK(OnPart(Nick, Channel));
}

bool CModules::OnUserCTCP(const CString& sTarget, CString& sMessage) {
	MODHALTCHK(OnUserCTCP(sTarget, sMessage));
}

bool CModules::OnUserCTCPReply(const CNick& Nick, CString& sMessage) {
	MODHALTCHK(OnUserCTCPReply(Nick, sMessage));
}

bool CModules::OnCTCPReply(const CNick& Nick, CString& sMessage) {
	MODHALTCHK(OnCTCPReply(Nick, sMessage));
}

bool CModules::OnPrivCTCP(const CNick& Nick, CString& sMessage) {
	MODHALTCHK(OnPrivCTCP(Nick, sMessage));
}

bool CModules::OnChanCTCP(const CNick& Nick, CChan& Channel, CString& sMessage) {
	MODHALTCHK(OnChanCTCP(Nick, Channel, sMessage));
}

bool CModules::OnUserMsg(const CString& sTarget, CString& sMessage) {
	MODHALTCHK(OnUserMsg(sTarget, sMessage));
}

bool CModules::OnPrivMsg(const CNick& Nick, CString& sMessage) {
	MODHALTCHK(OnPrivMsg(Nick, sMessage));
}

bool CModules::OnChanMsg(const CNick& Nick, CChan& Channel, CString& sMessage) {
	MODHALTCHK(OnChanMsg(Nick, Channel, sMessage));
}

bool CModules::OnUserNotice(const CString& sTarget, CString& sMessage) {
	MODHALTCHK(OnUserNotice(sTarget, sMessage));
}

bool CModules::OnPrivNotice(const CNick& Nick, CString& sMessage) {
	MODHALTCHK(OnPrivNotice(Nick, sMessage));
}

bool CModules::OnChanNotice(const CNick& Nick, CChan& Channel, CString& sMessage) {
	MODHALTCHK(OnChanNotice(Nick, Channel, sMessage));
}

bool CModules::OnStatusCommand(const CString& sCommand) {
	MODHALTCHK(OnStatusCommand(sCommand));
}

void CModules::OnModCommand(const CString& sCommand) {
	MODUNLOADCHK(OnModCommand(sCommand));
}

void CModules::OnModNotice(const CString& sMessage) {
	MODUNLOADCHK(OnModNotice(sMessage));
}

void CModules::OnModCTCP(const CString& sMessage) {
	MODUNLOADCHK(OnModCTCP(sMessage));
}

////////////////////
// CGlobalModules //
////////////////////
bool CGlobalModules::OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan) {
	GLOBALMODHALTCHK(OnConfigLine(sName, sValue, pUser, pChan));
}

CModule* CModules::FindModule(const CString& sModule) const {
	for (unsigned int a = 0; a < size(); a++) {
		if (sModule.CaseCmp((*this)[a]->GetModName()) == 0) {
			return (*this)[a];
		}
	}

	return NULL;
}

bool CModules::LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg) {
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

	CString sModPath = FindModPath(sModule, pUser);

	if (sModPath.empty()) {
		sRetMsg = "Unable to find module [" + sModule + "]";
		return false;
	}

	unsigned int uDLFlags = RTLD_LAZY;

	if (!pUser) {
		uDLFlags |= RTLD_GLOBAL;
	}

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
		typedef CModule* (*fp)(void*, CUser* pUser, const CString& sModName);
		fp Load = (fp) dlsym(p, "Load");

		if (!Load) {
			dlclose(p);
			sRetMsg = "Could not find Load() in module [" + sModule + "]";
			return false;
		}

		pModule = Load(p, pUser, sModule);
	} else {
		typedef CModule* (*fp)(void*, CZNC* pZNC, const CString& sModName);
		fp Load = (fp) dlsym(p, "Load");

		if (!Load) {
			dlclose(p);
			sRetMsg = "Could not find Load() in module [" + sModule + "]";
			return false;
		}

		pModule = Load(p, m_pZNC, sModule);
	}

	pModule->SetDescription(GetDesc());
	push_back(pModule);

	if (!pModule->OnLoad(sArgs)) {
		UnloadModule(sModule, sRetMsg);
		sRetMsg = "Module [" + sModule + "] aborted.";
		return false;
	}

	sRetMsg = "Loaded module [" + sModule + "] [" + sModPath + "]";
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

	if (!LoadModule(sMod, sArgs, pUser, sRetMsg)) {
		return false;
	}

	sRetMsg = "Reloaded module [" + sMod + "]";
	return true;
}

CString CModules::FindModPath(const CString& sModule, CUser* pUser) {
	if (pUser) {
		return pUser->FindModPath(sModule);
	}

	if (!m_pZNC) {
		DEBUG_ONLY(cerr << "CModules::FindModPath() m_pZNC is NULL!" << endl);
	}

	return (m_pZNC) ? m_pZNC->FindModPath(sModule) : "";
}

bool CModules::GetModInfo(CModInfo& ModInfo, const CString& sModule) {
	for (unsigned int a = 0; a < sModule.length(); a++) {
		const char& c = sModule[a];

		if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
			return false;
		}
	}

	CString sModPath = FindModPath(sModule, NULL);

	if (sModPath.empty()) {
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

void CModules::GetAvailableMods(set<CModInfo>& ssMods, CZNC* pZNC, bool bGlobal) {
	ssMods.clear();

	unsigned int a = 0;
	CDir Dir;

	Dir.FillByWildcard(pZNC->GetCurPath() + "/modules", "*.so");
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

	Dir.FillByWildcard(pZNC->GetModPath(), "*.so");
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
