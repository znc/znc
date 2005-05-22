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

CModule::CModule(void* pDLL, CUser* pUser, const CString& sModName) {
	m_pDLL = pDLL;
	m_pZNC = pUser->GetZNC();
	m_pManager = pUser->GetManager();
	m_pUser = pUser;
	m_sModName = sModName;
	LoadRegistry();
}

CModule::CModule(void* pDLL, CZNC* pZNC, const CString& sModName) {
	m_pDLL = pDLL;
	m_pZNC = pZNC;
	m_pManager = &pZNC->GetManager();
	m_pUser = NULL;
	m_sModName = sModName;
	LoadRegistry();
}

CModule::~CModule() {
	while (m_vTimers.size()) {
		RemTimer(m_vTimers[0]->GetName());
	}

	SaveRegistry();
}

void CModule::SetUser(CUser* pUser) { m_pUser = pUser; }
void CModule::Unload() { throw UNLOAD; }

bool CModule::LoadRegistry() {
	CString sRegistryDir = m_pZNC->GetDataPath() + "/" + m_sModName;
	CUtils::MakeDir(sRegistryDir);
	CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";

	return (m_mssRegistry.ReadFromDisk(sRegistryDir + "/" + sPrefix + "-registry.txt", 0600) == MCString::MCS_SUCCESS);
}

bool CModule::SaveRegistry() {
	CString sRegistryDir = m_pZNC->GetDataPath() + "/" + m_sModName;
	CUtils::MakeDir(sRegistryDir);
	CString sPrefix = (m_pUser) ? m_pUser->GetUserName() : ".global";

	return (m_mssRegistry.WriteToDisk(sRegistryDir + "/" + sPrefix + "-registry.txt", 0600) == MCString::MCS_SUCCESS);
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

const CString& CModule::GetModName() { return m_sModName; }
CString CModule::GetModNick() { return ((m_pUser) ? m_pUser->GetStatusPrefix() : "*") + m_sModName; }

CString CModule::GetDescription() { return "Unknown"; }

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

CModule* CModules::FindModule(const CString& sModule) {
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

	/* Assume global for now
	if (!pUser) {
		sRetMsg = "Unable to load module [" + sModule + "] Internal Error 1.";
		return false;
	}*/

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

	if (CModule::GetVersion() != Version()) {
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

void CModules::GetAvailableMods(set<CModInfo>& ssMods, CZNC* pZNC, bool bGlobal) {
	ssMods.clear();

	unsigned int a = 0;
	CDir Dir;

	Dir.FillByWildcard(pZNC->GetModPath(), "*.so");

	for (a = 0; a < Dir.size(); a++) {
		CFile& File = *Dir[a];
		if ((File.GetShortName().Left(2).CaseCmp("g_") == 0) == bGlobal) {
			ssMods.insert(CModInfo(File.GetShortName(), File.GetLongName(), false, bGlobal));
		}
	}

	Dir.FillByWildcard(_MODDIR_, "*.so");
	for (a = 0; a < Dir.size(); a++) {
		CFile& File = *Dir[a];
		if ((File.GetShortName().Left(2).CaseCmp("g_") == 0) == bGlobal) {
			ssMods.insert(CModInfo(File.GetShortName(), File.GetLongName(), true, bGlobal));
		}
	}
}
