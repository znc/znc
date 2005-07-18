#include "main.h"
#include "znc.h"
#include "User.h"
#include "Server.h"
#include "IRCSock.h"
#include "UserSock.h"
#include "DCCBounce.h"
#include "DCCSock.h"
#include "md5.h"
#include "Timers.h"

CUser::CUser(const CString& sUserName, CZNC* pZNC) {
	m_uConnectTime = 0;
	m_sUserName = sUserName;
	m_sNick = sUserName;
	m_sIdent = sUserName;
	m_sRealName = sUserName;
	m_uServerIdx = 0;
	m_pZNC = pZNC;
#ifdef _MODULES
	m_pModules = new CModules(pZNC);
#endif
	m_bBounceDCCs = true;
	m_bPassHashed = false;
	m_bUseClientIP = false;
	m_bKeepNick = false;
	m_bDenyLoadMod = false;
	m_sStatusPrefix = "*";
	m_uBufferCount = 50;
	m_bKeepBuffer = false;
	m_bAutoCycle = true;
	m_pKeepNickTimer = new CKeepNickTimer(this);
	m_pJoinTimer = new CJoinTimer(this);
	m_pZNC->GetManager().AddCron(m_pKeepNickTimer);
	m_pZNC->GetManager().AddCron(m_pJoinTimer);
	m_sUserPath = m_pZNC->GetUserPath() + "/" + sUserName;
	m_sDLPath = GetUserPath() + "/downloads";
}

CUser::~CUser() {
#ifdef _MODULES
	delete m_pModules;
#endif
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		delete m_vServers[a];
	}

	for (unsigned int b = 0; b < m_vChans.size(); b++) {
		delete m_vChans[b];
	}

	m_pZNC->GetManager().DelCronByAddr(m_pKeepNickTimer);
	m_pZNC->GetManager().DelCronByAddr(m_pJoinTimer);
}

bool CUser::OnBoot() {
#ifdef _MODULES
	return GetModules().OnBoot();
#endif
	return true;
}

bool CUser::Clone(const CUser& User, CString& sErrorRet) {
	unsigned int a = 0;
	sErrorRet.clear();

	if (!User.IsValid(sErrorRet, true)) {
		return false;
	}

	if (GetUserName() != User.GetUserName()) {
		if (m_pZNC->FindUser(User.GetUserName())) {
			sErrorRet = "New username already exists";
			return false;
		}

		SetUserName(User.GetUserName());
	}

	if (!User.GetPass().empty()) {
		SetPass(User.GetPass(), User.IsPassHashed());
	}

	SetNick(User.GetNick());
	SetAltNick(User.GetAltNick());
	SetIdent(User.GetIdent());
	SetRealName(User.GetRealName());
	SetAwaySuffix(User.GetAwaySuffix());
	SetStatusPrefix(User.GetStatusPrefix());
	SetVHost(User.GetVHost());
	SetQuitMsg(User.GetQuitMsg());
	SetDefaultChanModes(User.GetDefaultChanModes());
	SetBufferCount(User.GetBufferCount());

	// Allowed Hosts
	m_ssAllowedHosts.clear();
	const set<CString>& ssHosts = User.GetAllowedHosts();
	for (set<CString>::const_iterator it = ssHosts.begin(); it != ssHosts.end(); it++) {
		AddAllowedHost(*it);
	}

	CUserSock* pSock = GetUserSock();
	if (pSock) {
		if (!IsHostAllowed(pSock->GetRemoteIP())) {
			pSock->PutStatusNotice("You are being disconnected because your IP is no longer allowed to connect to this user");
			pSock->Close();
		}
	}

	// !Allowed Hosts

#ifdef _MODULES
	// Modules
	set<CString> ssUnloadMods;
	CModules& vCurMods = GetModules();
	const CModules& vNewMods = User.GetModules();

	for (a = 0; a < vNewMods.size(); a++) {
		CModule* pNewMod = vNewMods[a];
		CModule* pCurMod = vCurMods.FindModule(pNewMod->GetModName());

		if (!pCurMod) {
			CString sModRet;
			vCurMods.LoadModule(pNewMod->GetModName(), "", this, sModRet);
		}
	}

	for (a = 0; a < vCurMods.size(); a++) {
		CModule* pCurMod = vCurMods[a];
		CModule* pNewMod = vNewMods.FindModule(pCurMod->GetModName());

		if (!pNewMod) {
			ssUnloadMods.insert(pCurMod->GetModName());
		}
	}

	for (set<CString>::iterator it = ssUnloadMods.begin(); it != ssUnloadMods.end(); it++) {
		vCurMods.UnloadModule(*it);
	}
	// !Modules
#endif // !_MODULES

	// Servers
	const vector<CServer*>& vServers = User.GetServers();
	CString sServer;
	CServer* pCurServ = GetCurrentServer();

	if (pCurServ) {
		sServer = pCurServ->GetName();
	}

	m_vServers.clear();

	for (a = 0; a < vServers.size(); a++) {
		CServer* pServer = vServers[a];
		AddServer(pServer->GetName(), pServer->GetPort(), pServer->GetPass(), pServer->IsSSL());
	}

	for (a = 0; a < m_vServers.size(); a++) {
		if (sServer.CaseCmp(m_vServers[a]->GetName()) == 0) {
			m_uServerIdx = a +1;
			break;
		}

		if (a == m_vServers.size() -1) {
			m_uServerIdx = m_vServers.size();
			CIRCSock* pSock = GetIRCSock();

			if (pSock) {
				PutStatus("Jumping servers because this server is no longer in the list");
				pSock->Close();
			}
		}
	}
	// !Servers

	// Chans
	const vector<CChan*>& vChans = User.GetChans();
	for (a = 0; a < vChans.size(); a++) {
		CChan* pNewChan = vChans[a];
		CChan* pChan = FindChan(pNewChan->GetName());

		if (pChan) {
			pChan->SetInConfig(pNewChan->InConfig());
		} else {
			AddChan(pNewChan->GetName(), pNewChan->InConfig());
		}
	}

	for (a = 0; a < m_vChans.size(); a++) {
		CChan* pChan = m_vChans[a];

		if (!User.FindChan(pChan->GetName())) {
			pChan->SetInConfig(false);
		}
	}

	JoinChans();
	// !Chans

	// CTCP Replies
	m_mssCTCPReplies.clear();
	const MCString& msReplies = User.GetCTCPReplies();
	for (MCString::const_iterator it = msReplies.begin(); it != msReplies.end(); it++) {
		AddCTCPReply(it->first, it->second);
	}
	// !CTCP Replies

	// Flags
	SetKeepBuffer(User.KeepBuffer());
	SetAutoCycle(User.AutoCycle());
	SetKeepNick(User.GetKeepNick());
	SetBounceDCCs(User.BounceDCCs());
	SetUseClientIP(User.UseClientIP());
	SetDenyLoadMod(User.DenyLoadMod());
	// !Flags

	return true;
}

const set<CString>& CUser::GetAllowedHosts() const { return m_ssAllowedHosts; }
bool CUser::AddAllowedHost(const CString& sHostMask) {
	if (sHostMask.empty() || m_ssAllowedHosts.find(sHostMask) != m_ssAllowedHosts.end()) {
		return false;
	}

	m_ssAllowedHosts.insert(sHostMask);
	return true;
}

bool CUser::IsHostAllowed(const CString& sHostMask) {
	if (m_ssAllowedHosts.empty()) {
		return true;
	}

	for (set<CString>::iterator a = m_ssAllowedHosts.begin(); a != m_ssAllowedHosts.end(); a++) {
		if (sHostMask.WildCmp(*a)) {
			return true;
		}
	}

	return false;
}

bool CUser::IsValidUserName(const CString& sUserName) {
	const char* p = sUserName.c_str();

	if (sUserName.empty()) {
		return false;
	}

	while (*p) {
		if (!isalnum(*p++)) {
			return false;
		}
	}

	return true;
}

bool CUser::IsValid(CString& sErrMsg, bool bSkipPass) const {
	sErrMsg.clear();

	if (!bSkipPass && m_sPass.empty()) {
		sErrMsg = "Pass is empty";
		return false;
	}

	if (m_sUserName.empty()) {
		sErrMsg = "Username is empty";
		return false;
	}

	if (!CUser::IsValidUserName(m_sUserName)) {
		sErrMsg = "Username is invalid";
		return false;
	}

	if (m_vServers.empty()) {
		sErrMsg = "No servers defined";
		return false;
	}

	return true;
}

bool CUser::AddChan(CChan* pChan) {
	if (!pChan) {
		return false;
	}

	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		if (m_vChans[a]->GetName().CaseCmp(pChan->GetName()) == 0) {
			delete pChan;
			return false;
		}
	}

	m_vChans.push_back(pChan);
	return true;
}

bool CUser::AddChan(const CString& sName, bool bInConfig) {
	if (sName.empty() || FindChan(sName)) {
		return false;
	}

	CChan* pChan = new CChan(sName, this, bInConfig);
	m_vChans.push_back(pChan);
	return true;
}

bool CUser::DelChan(const CString& sName) {
	for (vector<CChan*>::iterator a = m_vChans.begin(); a != m_vChans.end(); a++) {
		if (sName.CaseCmp((*a)->GetName()) == 0) {
			m_vChans.erase(a);
			return true;
		}
	}

	return false;
}

bool CUser::PrintLine(CFile& File, const CString& sName, const CString& sValue) {
	if (sName.empty() || sValue.empty()) {
		return false;
	}

	return File.Write("\t" + sName + " = " + sValue + "\r\n");
}

bool CUser::WriteConfig(CFile& File) {
 	File.Write("<User " + GetUserName() + ">\r\n");

	PrintLine(File, "Pass", GetPass() + ((IsPassHashed()) ? " -" : ""));
	PrintLine(File, "Nick", GetNick());
	PrintLine(File, "AltNick", GetAltNick());
	PrintLine(File, "Ident", GetIdent());
	PrintLine(File, "RealName", GetRealName());
	PrintLine(File, "AwaySuffix", GetAwaySuffix());
	PrintLine(File, "StatusPrefix", GetStatusPrefix());
	PrintLine(File, "KeepNick", CString((GetKeepNick()) ? "true" : "false"));
	PrintLine(File, "Buffer", CString::ToString(GetBufferCount()));
	PrintLine(File, "KeepBuffer", CString((KeepBuffer()) ? "true" : "false"));
	PrintLine(File, "ChanModes", GetDefaultChanModes());
	PrintLine(File, "BounceDCCs", CString((BounceDCCs()) ? "true" : "false"));
	PrintLine(File, "AutoCycle", CString((AutoCycle()) ? "true" : "false"));
	PrintLine(File, "DCCLookupMethod", CString((UseClientIP()) ? "client" : "default"));
	File.Write("\r\n");

	// Allow Hosts
	for (set<CString>::iterator it = m_ssAllowedHosts.begin(); it != m_ssAllowedHosts.end(); it++) {
		PrintLine(File, "Allow", *it);
	}

	File.Write("\r\n");

	// CTCP Replies
	for (MCString::iterator itb = m_mssCTCPReplies.begin(); itb != m_mssCTCPReplies.end(); itb++) {
		PrintLine(File, "CTCPReply", itb->first.AsUpper() + " " + itb->second);
	}
	File.Write("\r\n");

	// Modules
	CModules& Mods = GetModules();

	for (unsigned int a = 0; a < Mods.size(); a++) {
		CString sArgs = Mods[a]->GetArgs();

		if (!sArgs.empty()) {
			sArgs = " " + sArgs;
		}

		PrintLine(File, "LoadModule", Mods[a]->GetModName() + sArgs);
	}
	File.Write("\r\n");

	// Servers
	for (unsigned int b = 0; b < m_vServers.size(); b++) {
		PrintLine(File, "Server", m_vServers[b]->GetString());
	}
	File.Write("\r\n");

	// Chans
	for (unsigned int c = 0; c < m_vChans.size(); c++) {
		if (!m_vChans[c]->WriteConfig(File)) {
			return false;
		}
	}

	File.Write("</User>\r\n");

	return true;
}

CChan* CUser::FindChan(const CString& sName) const {
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		CChan* pChan = m_vChans[a];
		if (sName.CaseCmp(pChan->GetName()) == 0) {
			return pChan;
		}
	}

	return NULL;
}

void CUser::JoinChans() {
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		CChan* pChan = m_vChans[a];
		if (!pChan->IsOn()) {
			PutIRC("JOIN " + pChan->GetName() + " " + pChan->GetKey());
		}
	}
}

CServer* CUser::FindServer(const CString& sName) {
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		CServer* pServer = m_vServers[a];
		if (sName.CaseCmp(pServer->GetName()) == 0) {
			return pServer;
		}
	}

	return NULL;
}

bool CUser::DelServer(const CString& sName) {
	if (sName.empty()) {
		return false;
	}

	for (vector<CServer*>::iterator it = m_vServers.begin(); it != m_vServers.end(); it++) {
		if ((*it)->GetName().CaseCmp(sName) == 0) {
			m_vServers.erase(it);
			return true;
		}
	}

	return false;
}

bool CUser::AddServer(const CString& sName) {
	if (sName.empty()) {
		return false;
	}

	bool bSSL = false;
	CString sLine = sName;
	sLine.Trim();

	CString sHost = sLine.Token(0);
	CString sPort = sLine.Token(1);

	if (sPort.Left(1) == "+") {
		bSSL = true;
		sPort.LeftChomp();
	}

	unsigned short uPort = strtoul(sPort.c_str(), NULL, 10);
	CString sPass = sLine.Token(2, true);

	return AddServer(sHost, uPort, sPass, bSSL);
}

bool CUser::AddServer(const CString& sName, unsigned short uPort, const CString& sPass, bool bSSL) {
#ifndef HAVE_LIBSSL
	if (bSSL) {
		return false;
	}
#endif
	if (sName.empty()) {
		return false;
	}

	if (!uPort) {
		uPort = 6667;
	}

	CServer* pServer = new CServer(sName, uPort, sPass, bSSL);
	m_vServers.push_back(pServer);

	return true;
}

bool CUser::IsLastServer() {
	return (m_uServerIdx >= m_vServers.size());
}

CServer* CUser::GetNextServer() {
	if (m_vServers.empty()) {
		return NULL;
	}

	if (m_uServerIdx >= m_vServers.size()) {
		m_uServerIdx = 0;
	}

	return m_vServers[m_uServerIdx++];	// Todo: cycle through these
}

CServer* CUser::GetCurrentServer() {
	unsigned int uIdx = (m_uServerIdx) ? m_uServerIdx -1 : 0;

	if (uIdx >= m_vServers.size()) {
		return NULL;
	}

	return m_vServers[uIdx];
}

bool CUser::CheckPass(const CString& sPass) {
	if (!m_bPassHashed) {
		return (sPass == m_sPass);
	}

	return (m_sPass.CaseCmp((char*) CMD5(sPass)) == 0);
}

TSocketManager<Csock>* CUser::GetManager() {
	return &m_pZNC->GetManager();
}

CZNC* CUser::GetZNC() {
	return m_pZNC;
}

CUserSock* CUser::GetUserSock() {
	// Todo: optimize this by saving a pointer to the sock
	TSocketManager<Csock>& Manager = m_pZNC->GetManager();
	CString sSockName = "USR::" + m_sUserName;

	for (unsigned int a = 0; a < Manager.size(); a++) {
		Csock* pSock = Manager[a];
		if (pSock->GetSockName().CaseCmp(sSockName) == 0) {
			if (!pSock->isClosed()) {
				return (CUserSock*) pSock;
			}
		}
	}

	return (CUserSock*) m_pZNC->GetManager().FindSockByName(sSockName);
}

bool CUser::IsUserAttached() {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	return pUserSock->IsAttached();
}

CIRCSock* CUser::GetIRCSock() {
	// Todo: optimize this by saving a pointer to the sock
	return (CIRCSock*) m_pZNC->GetManager().FindSockByName("IRC::" + m_sUserName);
}

CString CUser::GetLocalIP() {
	CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetLocalIP();
	}

	CUserSock* pUserSock = GetUserSock();

	if (pUserSock) {
		return pUserSock->GetLocalIP();
	}

	return "";
}

bool CUser::PutIRC(const CString& sLine) {
	CIRCSock* pIRCSock = GetIRCSock();

	if (!pIRCSock) {
		return false;
	}

	pIRCSock->PutServ(sLine);
	return true;
}

bool CUser::PutUser(const CString& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutServ(sLine);
	return true;
}

bool CUser::PutStatus(const CString& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutStatus(sLine);
	return true;
}

bool CUser::PutModule(const CString& sModule, const CString& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutModule(sModule, sLine);
	return true;
}

bool CUser::ResumeFile(const CString& sRemoteNick, unsigned short uPort, unsigned long uFileSize) {
	TSocketManager<Csock>& Manager = m_pZNC->GetManager();

	for (unsigned int a = 0; a < Manager.size(); a++) {
		if (strncasecmp(Manager[a]->GetSockName().c_str(), "DCC::LISTEN::", 13) == 0) {
			CDCCSock* pSock = (CDCCSock*) Manager[a];

			if (pSock->GetLocalPort() == uPort) {
				if (pSock->Seek(uFileSize)) {
					PutModule(pSock->GetModuleName(), "DCC -> [" + pSock->GetRemoteNick() + "][" + pSock->GetFileName() + "] - Attempting to resume from file position [" + CString::ToString(uFileSize) + "]");
					return true;
				} else {
					return false;
				}
			}
		}
	}

	return false;
}

bool CUser::SendFile(const CString& sRemoteNick, const CString& sFileName, const CString& sModuleName) {
	CString sFullPath = CUtils::ChangeDir(GetDLPath(), sFileName, GetHomePath());
	CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath, sModuleName);

	CFile* pFile = pSock->OpenFile(false);

	if (!pFile) {
		delete pSock;
		return false;
	}

	unsigned short uPort = GetManager()->ListenAllRand("DCC::LISTEN::" + sRemoteNick, false, SOMAXCONN, pSock, 120);

	if (GetNick().CaseCmp(sRemoteNick) == 0) {
		PutUser(":" + GetStatusPrefix() + "status!znc@znc.com PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString::ToString(CUtils::GetLongIP(GetLocalIP())) + " "
			   	+ CString::ToString(uPort) + " " + CString::ToString(pFile->GetSize()) + "\001");
	} else {
		PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString::ToString(CUtils::GetLongIP(GetLocalIP())) + " "
			    + CString::ToString(uPort) + " " + CString::ToString(pFile->GetSize()) + "\001");
	}

	PutModule(sModuleName, "DCC -> [" + sRemoteNick + "][" + pFile->GetShortName() + "] - Attempting Send.");
	return true;
}

bool CUser::GetFile(const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sFileName, unsigned long uFileSize, const CString& sModuleName) {
	if (CFile::Exists(sFileName)) {
		PutModule(sModuleName, "DCC <- [" + sRemoteNick + "][" + sFileName + "] - File already exists.");
		return false;
	}

	CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sRemoteIP, uRemotePort, sFileName, uFileSize, sModuleName);

	if (!pSock->OpenFile()) {
		delete pSock;
		return false;
	}

	if (!GetManager()->Connect(sRemoteIP, uRemotePort, "DCC::GET::" + sRemoteNick, 60, false, GetLocalIP(), pSock)) {
		PutModule(sModuleName, "DCC <- [" + sRemoteNick + "][" + sFileName + "] - Unable to connect.");
		return false;
	}

	PutModule(sModuleName, "DCC <- [" + sRemoteNick + "][" + sFileName + "] - Attempting to connect to [" + sRemoteIP + "]");
	return true;
}

CString CUser::GetCurNick() {
	CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetNick();
	}

	CUserSock* pUserSock = GetUserSock();

	if (pUserSock) {
		return pUserSock->GetNick();
	}

	return "";
}

// Setters
void CUser::SetUserName(const CString& s) { m_sUserName = s; }
void CUser::SetNick(const CString& s) { m_sNick = s; }
void CUser::SetAltNick(const CString& s) { m_sAltNick = s; }
void CUser::SetAwaySuffix(const CString& s) { m_sAwaySuffix = s; }
void CUser::SetIdent(const CString& s) { m_sIdent = s; }
void CUser::SetRealName(const CString& s) { m_sRealName = s; }
void CUser::SetVHost(const CString& s) { m_sVHost = s; }
void CUser::SetPass(const CString& s, bool bHashed) { m_sPass = s; m_bPassHashed = bHashed; }
void CUser::SetBounceDCCs(bool b) { m_bBounceDCCs = b; }
void CUser::SetUseClientIP(bool b) { m_bUseClientIP = b; }
void CUser::SetKeepNick(bool b) { m_bKeepNick = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetDefaultChanModes(const CString& s) { m_sDefaultChanModes = s; }
void CUser::SetIRCNick(const CNick& n) { m_IRCNick = n; }
void CUser::SetIRCServer(const CString& s) { m_sIRCServer = s; }
void CUser::SetQuitMsg(const CString& s) { m_sQuitMsg = s; }
bool CUser::AddCTCPReply(const CString& sCTCP, const CString& sReply) {
	if (sCTCP.empty() || sReply.empty()) {
		return false;
	}

	m_mssCTCPReplies[sCTCP.AsUpper()] = sReply;
	return true;
}
void CUser::SetBufferCount(unsigned int u) { m_uBufferCount = u; }
void CUser::SetKeepBuffer(bool b) { m_bKeepBuffer = b; }
void CUser::SetAutoCycle(bool b) { m_bAutoCycle = b; }

bool CUser::SetStatusPrefix(const CString& s) {
	if ((!s.empty()) && (s.length() < 6) && (s.find(' ') == CString::npos)) {
		m_sStatusPrefix = (s.empty()) ? "*" : s;
		return true;
	}

	return false;
}
// !Setters

// Getters
const CString& CUser::GetUserName() const { return m_sUserName; }
const CString& CUser::GetNick() const { return m_sNick.empty() ? m_sUserName : m_sNick; }
const CString& CUser::GetAltNick() const { return m_sAltNick.empty() ? m_sUserName : m_sAltNick; }
const CString& CUser::GetAwaySuffix() const { return m_sAwaySuffix; }
const CString& CUser::GetIdent() const { return m_sIdent.empty() ? m_sUserName : m_sIdent; }
const CString& CUser::GetRealName() const { return m_sRealName.empty() ? m_sUserName : m_sRealName; }
const CString& CUser::GetVHost() const { return m_sVHost; }
const CString& CUser::GetPass() const { return m_sPass; }
bool CUser::IsPassHashed() const { return m_bPassHashed; }

CString CUser::FindModPath(const CString& sModule) const {
	CString sModPath = GetCurPath() + "/modules/" + sModule;
	sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

	if (!CFile::Exists(sModPath)) {
		DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
		sModPath = GetModPath() + "/" + sModule;
		sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

		if (!CFile::Exists(sModPath)) {
			DEBUG_ONLY(cout << "[" << sModPath << "] Not found..." << endl);
			sModPath = _MODDIR_ + CString("/") + sModule;
			sModPath += (sModule.find(".") == CString::npos) ? ".so" : "";

			if (!CFile::Exists(sModPath)) {
				DEBUG_ONLY(cout << "[" << sModPath << "] Not found... giving up!" << endl);
				return "";
			}
		}
	}

	return sModPath;
}

bool CUser::ConnectPaused() {
	if (!m_uConnectTime) {
		m_uConnectTime = time(NULL);
		return false;
	}

	if (time(NULL) - m_uConnectTime >= 5) {
		m_uConnectTime = time(NULL);
		return false;
	}

	return true;
}

const CString& CUser::GetCurPath() const { return m_pZNC->GetCurPath(); }
const CString& CUser::GetModPath() const { return m_pZNC->GetModPath(); }
const CString& CUser::GetHomePath() const { return m_pZNC->GetHomePath(); }
CString CUser::GetPemLocation() const { return m_pZNC->GetPemLocation(); }

bool CUser::UseClientIP() const { return m_bUseClientIP; }
bool CUser::GetKeepNick() const { return m_bKeepNick; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
bool CUser::BounceDCCs() const { return m_bBounceDCCs; }
const CString& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const CString& CUser::GetDefaultChanModes() const { return m_sDefaultChanModes; }
const vector<CChan*>& CUser::GetChans() const { return m_vChans; }
const vector<CServer*>& CUser::GetServers() const { return m_vServers; }
const CNick& CUser::GetIRCNick() const { return m_IRCNick; }
const CString& CUser::GetIRCServer() const { return m_sIRCServer; }
CString CUser::GetQuitMsg() const { return (!m_sQuitMsg.empty()) ? m_sQuitMsg : "ZNC by prozac - http://znc.sourceforge.net"; }
const MCString& CUser::GetCTCPReplies() const { return m_mssCTCPReplies; }
unsigned int CUser::GetBufferCount() const { return m_uBufferCount; }
bool CUser::KeepBuffer() const { return m_bKeepBuffer; }
bool CUser::AutoCycle() const { return m_bAutoCycle; }
// !Getters
