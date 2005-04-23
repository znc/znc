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

CUser::CUser(const string& sUserName, CZNC* pZNC) {
	m_sUserName = sUserName;
	m_sNick = sUserName;
	m_sIdent = sUserName;
	m_sRealName = sUserName;
	m_uServerIdx = 0;
	m_pZNC = pZNC;
	m_bPassHashed = false;
	m_bUseClientIP = false;
	m_bKeepNick = false;
	m_bDenyLoadMod = false;
	m_sStatusPrefix = "*";
	m_pZNC->GetManager().AddCron(new CKeepNickTimer(this));
	m_pZNC->GetManager().AddCron(new CJoinTimer(this));
}

CUser::~CUser() {
#ifdef _MODULES
	m_Modules.UnloadAll();
#endif
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		delete m_vServers[a];
	}

	for (unsigned int b = 0; b < m_vChans.size(); b++) {
		delete m_vChans[b];
	}
}

bool CUser::OnBoot() {
#ifdef _MODULES
	return m_Modules.OnBoot();
#endif
	return true;
}

bool CUser::AddAllowedHost(const string& sHostMask) {
	if (m_ssAllowedHosts.find(sHostMask) != m_ssAllowedHosts.end()) {
		return false;
	}

	m_ssAllowedHosts.insert(sHostMask);
	return true;
}

bool CUser::IsHostAllowed(const string& sHostMask) {
	for (set<string>::iterator a = m_ssAllowedHosts.begin(); a != m_ssAllowedHosts.end(); a++) {
		if (CUtils::wildcmp(*a, sHostMask)) {
			return true;
		}
	}

	return false;
}

bool CUser::AddChan(CChan* pChan) {
	if (!pChan) {
		return false;
	}

	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		if (strcasecmp(m_vChans[a]->GetName().c_str(), pChan->GetName().c_str()) == 0) {
			delete pChan;
			return false;
		}
	}

	m_vChans.push_back(pChan);
	return true;
}

bool CUser::AddChan(const string& sName, unsigned int uBufferCount) {
	if (sName.empty()) {
		return false;
	}

	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		if (strcasecmp(sName.c_str(), m_vChans[a]->GetName().c_str()) == 0) {
			return false;
		}
	}

	CChan* pChan = new CChan(sName, this, uBufferCount);
	m_vChans.push_back(pChan);
	return true;
}

bool CUser::DelChan(const string& sName) {
	for (vector<CChan*>::iterator a = m_vChans.begin(); a != m_vChans.end(); a++) {
		if (strcasecmp(sName.c_str(), (*a)->GetName().c_str()) == 0) {
			m_vChans.erase(a);
			return true;
		}
	}

	return false;
}

CChan* CUser::FindChan(const string& sName) {
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		if (strcasecmp(sName.c_str(), m_vChans[a]->GetName().c_str()) == 0) {
			return m_vChans[a];
		}
	}

	return NULL;
}

bool CUser::AddServer(const string& sName) {
	if (sName.empty()) {
		return false;
	}

	bool bSSL = false;
	string sLine = sName;
	CUtils::Trim(sLine);

	string sHost = CUtils::Token(sLine, 0);
	string sPort = CUtils::Token(sLine, 1);

	if (CUtils::Left(sPort, 1) == "+") {
		bSSL = true;
		CUtils::LeftChomp(sPort);
	}

	unsigned short uPort = strtoul(sPort.c_str(), NULL, 10);
	string sPass = CUtils::Token(sLine, 2, true);

	return AddServer(sHost, uPort, sPass, bSSL);
}

bool CUser::AddServer(const string& sName, unsigned short uPort, const string& sPass, bool bSSL) {
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

CServer* CUser::GetNextServer() {
	if (m_vServers.empty()) {
		return NULL;
	}

	if (m_uServerIdx >= m_vServers.size()) {
		m_uServerIdx = 0;
	}

	return m_vServers[m_uServerIdx++];	// Todo: cycle through these
}

bool CUser::CheckPass(const string& sPass) {
	if (!m_bPassHashed) {
		return (sPass == m_sPass);
	}

	return (strcasecmp(m_sPass.c_str(), CMD5(sPass)) == 0);
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
	string sSockName = "USR::" + m_sUserName;

	for (unsigned int a = 0; a < Manager.size(); a++) {
		Csock* pSock = Manager[a];
		if (strcasecmp(pSock->GetSockName().c_str(), sSockName.c_str()) == 0) {
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

string CUser::GetLocalIP() {
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

bool CUser::PutIRC(const string& sLine) {
	CIRCSock* pIRCSock = GetIRCSock();

	if (!pIRCSock) {
		return false;
	}

	pIRCSock->PutServ(sLine);
	return true;
}
	
bool CUser::PutUser(const string& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutServ(sLine);
	return true;
}

bool CUser::PutStatus(const string& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutStatus(sLine);
	return true;
}

bool CUser::PutModule(const string& sModule, const string& sLine) {
	CUserSock* pUserSock = GetUserSock();

	if (!pUserSock) {
		return false;
	}

	pUserSock->PutModule(sModule, sLine);
	return true;
}

bool CUser::ResumeFile(const string& sRemoteNick, unsigned short uPort, unsigned long uFileSize) {
	TSocketManager<Csock>& Manager = m_pZNC->GetManager();

	for (unsigned int a = 0; a < Manager.size(); a++) {
		if (strncasecmp(Manager[a]->GetSockName().c_str(), "DCC::LISTEN::", 13) == 0) {
			CDCCSock* pSock = (CDCCSock*) Manager[a];

			if (pSock->GetLocalPort() == uPort) {
				if (pSock->Seek(uFileSize)) {
					PutModule(pSock->GetModuleName(), "DCC -> [" + pSock->GetRemoteNick() + "][" + pSock->GetFileName() + "] - Attempting to resume from file position [" + CUtils::ToString(uFileSize) + "]");
					return true;
				} else {
					return false;
				}
			}
		}
	}

	return false;
}

bool CUser::SendFile(const string& sRemoteNick, const string& sFileName, const string& sModuleName) {
	string sFullPath = CUtils::ChangeDir(GetDLPath(), sFileName, GetHomePath());
	CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath, sModuleName);

	CFile* pFile = pSock->OpenFile(false);

	if (!pFile) {
		delete pSock;
		return false;
	}

	int iPort = GetManager()->ListenAllRand("DCC::LISTEN::" + sRemoteNick, false, SOMAXCONN, pSock, 120);

	if (strcasecmp(GetNick().c_str(), sRemoteNick.c_str()) == 0) {
		PutUser(":" + GetStatusPrefix() + "status!znc@znc.com PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CUtils::ToString(CUtils::GetLongIP(GetLocalIP())) + " "
			   	+ CUtils::ToString(iPort) + " " + CUtils::ToString(pFile->GetSize()) + "\001");
	} else {
		PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CUtils::ToString(CUtils::GetLongIP(GetLocalIP())) + " "
			    + CUtils::ToString(iPort) + " " + CUtils::ToString(pFile->GetSize()) + "\001");
	}

	PutModule(sModuleName, "DCC -> [" + sRemoteNick + "][" + pFile->GetShortName() + "] - Attempting Send.");
	return true;
}

bool CUser::GetFile(const string& sRemoteNick, const string& sRemoteIP, unsigned short uRemotePort, const string& sFileName, unsigned long uFileSize, const string& sModuleName) {
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

string CUser::GetCurNick() {
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
void CUser::SetNick(const string& s) { m_sNick = s; }
void CUser::SetAltNick(const string& s) { m_sAltNick = s; }
void CUser::SetIdent(const string& s) { m_sIdent = s; }
void CUser::SetRealName(const string& s) { m_sRealName = s; }
void CUser::SetVHost(const string& s) { m_sVHost = s; }
void CUser::SetPass(const string& s, bool bHashed) { m_sPass = s; m_bPassHashed = bHashed; }
void CUser::SetUseClientIP(bool b) { m_bUseClientIP = b; }
void CUser::SetKeepNick(bool b) { m_bKeepNick = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetDefaultChanModes(const string& s) { m_sDefaultChanModes = s; }
void CUser::SetIRCNick(const CNick& n) { m_IRCNick = n; }
void CUser::SetIRCServer(const string& s) { m_sIRCServer = s; }
void CUser::SetQuitMsg(const string& s) { m_sQuitMsg = s; }

bool CUser::SetStatusPrefix(const string& s) {
	if ((!s.empty()) && (s.length() < 6) && (s.find(' ') == string::npos)) {
		m_sStatusPrefix = s;
		return true;
	}

	return false;
}
// !Setters

// Getters
const string& CUser::GetUserName() const { return m_sUserName; }
const string& CUser::GetNick() const { return m_sNick; }
const string& CUser::GetAltNick() const { return m_sAltNick; }
const string& CUser::GetIdent() const { return m_sIdent; }
const string& CUser::GetRealName() const { return m_sRealName; }
const string& CUser::GetVHost() const { return m_sVHost; }
const string& CUser::GetPass() const { return m_sPass; }

const string& CUser::GetCurPath() const { return m_pZNC->GetCurPath(); }
const string& CUser::GetDLPath() const { return m_pZNC->GetDLPath(); }
const string& CUser::GetModPath() const { return m_pZNC->GetModPath(); }
const string& CUser::GetHomePath() const { return m_pZNC->GetHomePath(); }
const string& CUser::GetDataPath() const { return m_pZNC->GetDataPath(); }
string CUser::GetPemLocation() const { return m_pZNC->GetPemLocation(); }

bool CUser::UseClientIP() const { return m_bUseClientIP; }
bool CUser::KeepNick() const { return m_bKeepNick; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
const string& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const string& CUser::GetDefaultChanModes() const { return m_sDefaultChanModes; }
const vector<CChan*>& CUser::GetChans() const { return m_vChans; }
const CNick& CUser::GetIRCNick() const { return m_IRCNick; }
const string& CUser::GetIRCServer() const { return m_sIRCServer; }
const string& CUser::GetQuitMsg() const { return m_sQuitMsg; }
// !Getters
