/*
 * Copyright (C) 2004-2007  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "main.h"
#include "znc.h"
#include "User.h"
#include "Server.h"
#include "IRCSock.h"
#include "Client.h"
#include "DCCBounce.h"
#include "DCCSock.h"
#include "MD5.h"
#include "Timers.h"

CUser::CUser(const CString& sUserName) {
	m_fTimezoneOffset = 0;
	m_uConnectTime = 0;
	SetUserName(sUserName);
	m_sNick = m_sCleanUserName;
	m_sIdent = m_sCleanUserName;
	m_sRealName = sUserName;
	m_uServerIdx = 0;
	m_uBytesRead = 0;
	m_uBytesWritten = 0;
#ifdef _MODULES
	m_pModules = new CModules;
#endif
	m_RawBuffer.SetLineCount(100);		// This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200);		// This should be more than enough motd lines
	m_bIRCConnected = false;
	m_bMultiClients = true;
	m_bBounceDCCs = true;
	m_bPassHashed = false;
	m_bUseClientIP = false;
	m_bKeepNick = false;
	m_bDenyLoadMod = false;
	m_bAdmin= false;
	m_sStatusPrefix = "*";
	m_sChanPrefixes = "#&";
	m_uBufferCount = 50;
	m_uMaxJoinTries = 0;
	m_bKeepBuffer = false;
	m_bAutoCycle = true;
	m_bBeingDeleted = false;
	m_sTimestampFormat = "[%H:%M:%S]";
	m_bAppendTimestamp = false;
	m_bPrependTimestamp = true;
	m_bIRCConnectEnabled = true;
	m_pKeepNickTimer = new CKeepNickTimer(this);
	m_pJoinTimer = new CJoinTimer(this);
	m_pMiscTimer = new CMiscTimer(this);
	CZNC::Get().GetManager().AddCron(m_pKeepNickTimer);
	CZNC::Get().GetManager().AddCron(m_pJoinTimer);
	CZNC::Get().GetManager().AddCron(m_pMiscTimer);
	m_sUserPath = CZNC::Get().GetUserPath() + "/" + sUserName;
	m_sDLPath = GetUserPath() + "/downloads";
}

CUser::~CUser() {
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		delete m_vServers[a];
	}

	for (unsigned int b = 0; b < m_vChans.size(); b++) {
		delete m_vChans[b];
	}

	DelClients();

#ifdef _MODULES
	DelModules();
#endif

	CZNC::Get().GetManager().DelCronByAddr(m_pKeepNickTimer);
	CZNC::Get().GetManager().DelCronByAddr(m_pJoinTimer);
	CZNC::Get().GetManager().DelCronByAddr(m_pMiscTimer);
}

#ifdef _MODULES
void CUser::DelModules() {
	if (m_pModules) {
		delete m_pModules;
		m_pModules = NULL;
	}
}
#endif

void CUser::DelClients() {
	for (unsigned int c = 0; c < m_vClients.size(); c++) {
		CClient* pClient = m_vClients[c];
		CZNC::Get().GetManager().DelSockByAddr(pClient);
	}

	m_vClients.clear();
}

bool CUser::OnBoot() {
#ifdef _MODULES
	return GetModules().OnBoot();
#else
	return true;
#endif
}

void CUser::IRCConnected(CIRCSock* pIRCSock) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->IRCConnected(pIRCSock);
	}
}

void CUser::IRCDisconnected() {
	m_bIRCConnected = false;

	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->IRCDisconnected();
	}

	// Get the reconnect going
	CZNC::Get().EnableConnectUser();
}

CString CUser::ExpandString(const CString& sStr) const {
	CString sRet;
	return ExpandString(sStr, sRet);
}

CString& CUser::ExpandString(const CString& sStr, CString& sRet) const {
	sRet = sStr;
	sRet.Replace("%user%", GetUserName());
	sRet.Replace("%defnick%", GetNick());
	sRet.Replace("%nick%", GetCurNick());
	sRet.Replace("%altnick%", GetAltNick());
	sRet.Replace("%ident%", GetIdent());
	sRet.Replace("%realname%", GetRealName());
	sRet.Replace("%vhost%", GetVHost());

	return sRet;
}

CString CUser::AddTimestamp(const CString& sStr) const {
	CString sRet;
	return AddTimestamp(sStr, sRet);
}

CString& CUser::AddTimestamp(const CString& sStr, CString& sRet) const {
	char szTimestamp[1024];
	time_t tm;

	if(GetTimestampFormat().empty() || (!m_bAppendTimestamp && !m_bPrependTimestamp)) {
		sRet = sStr;
	} else {
		time(&tm);
		tm += (time_t)(m_fTimezoneOffset * 60 * 60); // offset is in hours
		strftime(szTimestamp, sizeof(szTimestamp) / sizeof(char), GetTimestampFormat().c_str(), localtime(&tm));

		sRet = sStr;
		if (m_bPrependTimestamp) {
			sRet = szTimestamp;
			sRet += " " + sStr;
		}
		if (m_bAppendTimestamp) {
			sRet += " ";
			sRet += szTimestamp;
		} 
	}
	return sRet;
}

void CUser::BounceAllClients() {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->BouncedOff();
	}

	m_vClients.clear();
}

void CUser::UserConnected(CClient* pClient) {
	if (!MultiClients()) {
		BounceAllClients();
	}

	PutStatus("Another client authenticated as your user, use the 'ListClients' command to see all clients");
	m_vClients.push_back(pClient);

	if (m_RawBuffer.IsEmpty()) {
		pClient->PutClient(":irc.znc.com 001 " + pClient->GetNick() + " :- Welcome to ZNC -");
	} else {
		unsigned int uIdx = 0;
		CString sLine;

		while (m_RawBuffer.GetLine(GetIRCNick().GetNick(), sLine, uIdx++)) {
			pClient->PutClient(sLine);
		}
	}

	// Send the cached MOTD
	if (m_MotdBuffer.IsEmpty()) {
		PutIRC("MOTD");
	} else {
		unsigned int uIdx = 0;
		CString sLine;

		while (m_MotdBuffer.GetLine(GetIRCNick().GetNick(), sLine, uIdx++)) {
			pClient->PutClient(sLine);
		}
	}

	if(GetIRCSock() != NULL) {
		CString sUserMode("");
		const set<unsigned char>& scUserModes = GetIRCSock()->GetUserModes();
		for (set<unsigned char>::iterator it = scUserModes.begin();
				it != scUserModes.end(); it++) {
			sUserMode += *it;
		}
		if(!sUserMode.empty()) {
			pClient->PutClient(":" + GetIRCNick().GetNick() + " MODE " + GetIRCNick().GetNick() + " :+" + sUserMode);
		}
	}

	const vector<CChan*>& vChans = GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		if ((vChans[a]->IsOn()) && (!vChans[a]->IsDetached())) {
			vChans[a]->JoinUser(true, "", pClient);
		}
	}

	CString sBufLine;
	while (m_QueryBuffer.GetNextLine(GetIRCNick().GetNick(), sBufLine)) {
		pClient->PutClient(sBufLine);
	}

	// Tell them why they won't connect
	if (!GetIRCConnectEnabled())
		PutStatus("You are currently disconnected from IRC. "
				"Use 'connect' to reconnect.");
}

void CUser::UserDisconnected(CClient* pClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if (m_vClients[a] == pClient) {
			m_vClients.erase(m_vClients.begin() + a);
			break;
		}
	}
}

bool CUser::Clone(const CUser& User, CString& sErrorRet) {
	unsigned int a = 0;
	sErrorRet.clear();

	if (!User.IsValid(sErrorRet, true)) {
		return false;
	}

	if (GetUserName() != User.GetUserName()) {
		if (CZNC::Get().FindUser(User.GetUserName())) {
			sErrorRet = "New username already exists";
			return false;
		}

		SetUserName(User.GetUserName());
	}

	if (!User.GetPass().empty()) {
		SetPass(User.GetPass(), User.IsPassHashed());
	}

	SetNick(User.GetNick(false));
	SetAltNick(User.GetAltNick(false));
	SetIdent(User.GetIdent(false));
	SetRealName(User.GetRealName());
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

	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		CClient* pSock = m_vClients[a];

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
		CString sModRet;
		CModule* pNewMod = vNewMods[a];
		CModule* pCurMod = vCurMods.FindModule(pNewMod->GetModName());

		if (!pCurMod) {
			try {
				vCurMods.LoadModule(pNewMod->GetModName(), pNewMod->GetArgs(), this, sModRet);
			} catch (...) {}
		} else if (pNewMod->GetArgs() != pCurMod->GetArgs()) {
			try {
				vCurMods.ReloadModule(pNewMod->GetModName(), pNewMod->GetArgs(), this, sModRet);
			} catch (...) {}
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
				pSock->Quit();
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
	SetMultiClients(User.MultiClients());
	SetBounceDCCs(User.BounceDCCs());
	SetUseClientIP(User.UseClientIP());
	SetDenyLoadMod(User.DenyLoadMod());
	SetAdmin(User.IsAdmin());
	SetTimestampAppend(User.GetTimestampAppend());
	SetTimestampPrepend(User.GetTimestampPrepend());
	SetTimestampFormat(User.GetTimestampFormat());
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

const CString& CUser::GetTimestampFormat() const { return m_sTimestampFormat; }
bool CUser::GetTimestampAppend() const { return m_bAppendTimestamp; }
bool CUser::GetTimestampPrepend() const { return m_bPrependTimestamp; }

bool CUser::IsValidUserName(const CString& sUserName) {
	const char* p = sUserName.c_str();

	if (sUserName.empty()) {
		return false;
	}

	if ((*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z')) {
		return false;
	}

	while (*p) {
		if (*p != '@' && *p != '.' && *p != '-' && *p != '_' && !isalnum(*p)) {
			return false;
		}

		*p++;
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
			delete *a;
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
	PrintLine(File, "VHost", GetVHost());
	PrintLine(File, "QuitMsg", GetQuitMsg());
	PrintLine(File, "StatusPrefix", GetStatusPrefix());
	PrintLine(File, "ChanModes", GetDefaultChanModes());
	PrintLine(File, "Buffer", CString(GetBufferCount()));
	PrintLine(File, "KeepNick", CString((GetKeepNick()) ? "true" : "false"));
	PrintLine(File, "KeepBuffer", CString((KeepBuffer()) ? "true" : "false"));
	PrintLine(File, "MultiClients", CString((MultiClients()) ? "true" : "false"));
	PrintLine(File, "BounceDCCs", CString((BounceDCCs()) ? "true" : "false"));
	PrintLine(File, "AutoCycle", CString((AutoCycle()) ? "true" : "false"));
	PrintLine(File, "DenyLoadMod", CString((DenyLoadMod()) ? "true" : "false"));
	PrintLine(File, "Admin", CString((IsAdmin()) ? "true" : "false"));
	PrintLine(File, "DCCLookupMethod", CString((UseClientIP()) ? "client" : "default"));
	PrintLine(File, "TimestampFormat", GetTimestampFormat());
	PrintLine(File, "AppendTimestamp", CString((GetTimestampAppend()) ? "true" : "false"));
	PrintLine(File, "PrependTimestamp", CString((GetTimestampPrepend()) ? "true" : "false"));
	PrintLine(File, "TimezoneOffset", CString(m_fTimezoneOffset));
	PrintLine(File, "JoinTries", CString(m_uMaxJoinTries));
	File.Write("\r\n");

	// Allow Hosts
	if (!m_ssAllowedHosts.empty()) {
		for (set<CString>::iterator it = m_ssAllowedHosts.begin(); it != m_ssAllowedHosts.end(); it++) {
			PrintLine(File, "Allow", *it);
		}

		File.Write("\r\n");
	}

	// CTCP Replies
	if (!m_mssCTCPReplies.empty()) {
		for (MCString::iterator itb = m_mssCTCPReplies.begin(); itb != m_mssCTCPReplies.end(); itb++) {
			PrintLine(File, "CTCPReply", itb->first.AsUpper() + " " + itb->second);
		}

		File.Write("\r\n");
	}

#ifdef _MODULES
	// Modules
	CModules& Mods = GetModules();

	if (!Mods.empty()) {
		for (unsigned int a = 0; a < Mods.size(); a++) {
			CString sArgs = Mods[a]->GetArgs();

			if (!sArgs.empty()) {
				sArgs = " " + sArgs;
			}

			PrintLine(File, "LoadModule", Mods[a]->GetModName() + sArgs);
		}

		File.Write("\r\n");
	}
#endif

	// Servers
	for (unsigned int b = 0; b < m_vServers.size(); b++) {
		PrintLine(File, "Server", m_vServers[b]->GetString());
	}

	// Chans
	for (unsigned int c = 0; c < m_vChans.size(); c++) {
		CChan* pChan = m_vChans[c];
		if (pChan->InConfig()) {
			File.Write("\r\n");
			if (!pChan->WriteConfig(File)) {
				return false;
			}
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
		if (!pChan->IsOn() && !pChan->IsDisabled()) {
			if (JoinTries() != 0 && pChan->GetJoinTries() >= JoinTries()) {
				PutStatus("The channel " + pChan->GetName() + " could not be joined, disabling it.");
				pChan->Disable();
			} else {
				pChan->IncJoinTries();
				PutIRC("JOIN " + pChan->GetName() + " " + pChan->GetKey());
			}
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

	unsigned int a = 0;

	for (vector<CServer*>::iterator it = m_vServers.begin(); it != m_vServers.end(); it++, a++) {
		CServer* pServer = *it;

		if (pServer->GetName().CaseCmp(sName) == 0) {
			CServer* pCurServer = GetCurrentServer();
			m_vServers.erase(it);

			if (pServer == pCurServer) {
				CIRCSock* pIRCSock = GetIRCSock();

				if (m_uServerIdx) {
					m_uServerIdx--;
				}

				if (pIRCSock) {
					pIRCSock->Quit();
					PutStatus("Your current server was removed, jumping...");
				}
			} else if (m_uServerIdx >= m_vServers.size()) {
				m_uServerIdx = 0;
			}

			delete pServer;

			return true;
		}
	}

	return false;
}

bool CUser::AddServer(const CString& sName, bool bIPV6) {
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

	return AddServer(sHost, uPort, sPass, bSSL, bIPV6);
}

bool CUser::AddServer(const CString& sName, unsigned short uPort, const CString& sPass, bool bSSL, bool bIPV6) {
#ifndef HAVE_LIBSSL
	if (bSSL) {
		return false;
	}
#endif

#ifndef HAVE_IPV6
	if (bIPV6) {
		return false;
	}
#endif

	if (sName.empty()) {
		return false;
	}

	if (!uPort) {
		uPort = 6667;
	}

	CServer* pServer = new CServer(sName, uPort, sPass, bSSL, bIPV6);
	m_vServers.push_back(pServer);

	CheckIRCConnect();

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

/*CClient* CUser::GetClient() {
	// Todo: optimize this by saving a pointer to the sock
	CSockManager& Manager = CZNC::Get().GetManager();
	CString sSockName = "USR::" + m_sUserName;

	for (unsigned int a = 0; a < Manager.size(); a++) {
		Csock* pSock = Manager[a];
		if (pSock->GetSockName().CaseCmp(sSockName) == 0) {
			if (!pSock->IsClosed()) {
				return (CClient*) pSock;
			}
		}
	}

	return (CClient*) CZNC::Get().GetManager().FindSockByName(sSockName);
}*/

CIRCSock* CUser::GetIRCSock() {
	// Todo: optimize this by saving a pointer to the sock
	return (CIRCSock*) CZNC::Get().GetManager().FindSockByName("IRC::" + m_sUserName);
}

const CIRCSock* CUser::GetIRCSock() const {
	// Todo: same as above
	return (CIRCSock*) CZNC::Get().GetManager().FindSockByName("IRC::" + m_sUserName);
}

CString CUser::GetLocalIP() {
	CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetLocalIP();
	}

	if (m_vClients.size()) {
		return m_vClients[0]->GetLocalIP();
	}

	return "";
}

bool CUser::PutIRC(const CString& sLine) {
	CIRCSock* pIRCSock = GetIRCSock();

	if (!pIRCSock) {
		return false;
	}

	pIRCSock->PutIRC(sLine);
	return true;
}

bool CUser::PutUser(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if ((!pClient || pClient == m_vClients[a]) && pSkipClient != m_vClients[a]) {
			m_vClients[a]->PutClient(sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

bool CUser::PutStatus(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if ((!pClient || pClient == m_vClients[a]) && pSkipClient != m_vClients[a]) {
			m_vClients[a]->PutStatus(sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

bool CUser::PutStatusNotice(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if ((!pClient || pClient == m_vClients[a]) && pSkipClient != m_vClients[a]) {
			m_vClients[a]->PutStatusNotice(sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

bool CUser::PutModule(const CString& sModule, const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if ((!pClient || pClient == m_vClients[a]) && pSkipClient != m_vClients[a]) {
			m_vClients[a]->PutModule(sModule, sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

bool CUser::ResumeFile(const CString& sRemoteNick, unsigned short uPort, unsigned long uFileSize) {
	CSockManager& Manager = CZNC::Get().GetManager();

	for (unsigned int a = 0; a < Manager.size(); a++) {
		if (strncasecmp(Manager[a]->GetSockName().c_str(), "DCC::LISTEN::", 13) == 0) {
			CDCCSock* pSock = (CDCCSock*) Manager[a];

			if (pSock->GetLocalPort() == uPort) {
				if (pSock->Seek(uFileSize)) {
					PutModule(pSock->GetModuleName(), "DCC -> [" + pSock->GetRemoteNick() + "][" + pSock->GetFileName() + "] - Attempting to resume from file position [" + CString(uFileSize) + "]");
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
	CString sFullPath = CUtils::ChangeDir(GetDLPath(), sFileName, CZNC::Get().GetHomePath());
	CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath, sModuleName);

	CFile* pFile = pSock->OpenFile(false);

	if (!pFile) {
		delete pSock;
		return false;
	}

	unsigned short uPort = CZNC::Get().GetManager().ListenRand("DCC::LISTEN::" + sRemoteNick, GetLocalIP(), false, SOMAXCONN, pSock, 120);

	if (GetNick().CaseCmp(sRemoteNick) == 0) {
		PutUser(":" + GetStatusPrefix() + "status!znc@znc.com PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(GetLocalIP())) + " "
			   	+ CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
	} else {
		PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(GetLocalIP())) + " "
			    + CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
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

	if (!CZNC::Get().GetManager().Connect(sRemoteIP, uRemotePort, "DCC::GET::" + sRemoteNick, 60, false, GetLocalIP(), pSock)) {
		PutModule(sModuleName, "DCC <- [" + sRemoteNick + "][" + sFileName + "] - Unable to connect.");
		return false;
	}

	PutModule(sModuleName, "DCC <- [" + sRemoteNick + "][" + sFileName + "] - Attempting to connect to [" + sRemoteIP + "]");
	return true;
}

CString CUser::GetCurNick() const {
	const CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetNick();
	}

	if (m_vClients.size()) {
		return m_vClients[0]->GetNick();
	}

	return "";
}

CString CUser::MakeCleanUserName(const CString& sUserName) {
	return sUserName.Token(0, false, "@").Replace_n(".", "");
}

// Setters
void CUser::SetUserName(const CString& s) {
	m_sCleanUserName = CUser::MakeCleanUserName(s);
	m_sUserName = s;
}

void CUser::SetNick(const CString& s) { m_sNick = s; }
void CUser::SetAltNick(const CString& s) { m_sAltNick = s; }
void CUser::SetIdent(const CString& s) { m_sIdent = s; }
void CUser::SetRealName(const CString& s) { m_sRealName = s; }
void CUser::SetVHost(const CString& s) { m_sVHost = s; }
void CUser::SetPass(const CString& s, bool bHashed) { m_sPass = s; m_bPassHashed = bHashed; }
void CUser::SetMultiClients(bool b) { m_bMultiClients = b; }
void CUser::SetBounceDCCs(bool b) { m_bBounceDCCs = b; }
void CUser::SetUseClientIP(bool b) { m_bUseClientIP = b; }
void CUser::SetKeepNick(bool b) { m_bKeepNick = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetAdmin(bool b) { m_bAdmin = b; }
void CUser::SetDefaultChanModes(const CString& s) { m_sDefaultChanModes = s; }
void CUser::SetIRCServer(const CString& s) { m_sIRCServer = s; m_bIRCConnected = true; }
void CUser::SetQuitMsg(const CString& s) { m_sQuitMsg = s; }
void CUser::SetBufferCount(unsigned int u) { m_uBufferCount = u; }
void CUser::SetKeepBuffer(bool b) { m_bKeepBuffer = b; }
void CUser::SetAutoCycle(bool b) { m_bAutoCycle = b; }

void CUser::CheckIRCConnect()
{
	// Do we want to connect?
	if (m_bIRCConnectEnabled && GetIRCSock() == NULL)
		CZNC::Get().EnableConnectUser();
}

void CUser::SetIRCNick(const CNick& n) {
	m_IRCNick = n;

	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->SetNick(n.GetNick());
	}
}

bool CUser::AddCTCPReply(const CString& sCTCP, const CString& sReply) {
	if (sCTCP.empty()) {
		return false;
	}

	m_mssCTCPReplies[sCTCP.AsUpper()] = sReply;
	return true;
}

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
const CString& CUser::GetCleanUserName() const { return m_sCleanUserName; }
const CString& CUser::GetNick(bool bAllowDefault) const { return (bAllowDefault && m_sNick.empty()) ? GetCleanUserName() : m_sNick; }
const CString& CUser::GetAltNick(bool bAllowDefault) const { return (bAllowDefault && m_sAltNick.empty()) ? GetCleanUserName() : m_sAltNick; }
const CString& CUser::GetIdent(bool bAllowDefault) const { return (bAllowDefault && m_sIdent.empty()) ? GetCleanUserName() : m_sIdent; }
const CString& CUser::GetRealName() const { return m_sRealName.empty() ? m_sUserName : m_sRealName; }
const CString& CUser::GetVHost() const { return m_sVHost; }
const CString& CUser::GetPass() const { return m_sPass; }
bool CUser::IsPassHashed() const { return m_bPassHashed; }

bool CUser::ConnectPaused() {
	if (!m_uConnectTime) {
		m_uConnectTime = time(NULL);
		return !m_bIRCConnectEnabled;
	}

	if (time(NULL) - m_uConnectTime >= 5) {
		m_uConnectTime = time(NULL);
		return !m_bIRCConnectEnabled;
	}

	return true;
}

bool CUser::UseClientIP() const { return m_bUseClientIP; }
bool CUser::GetKeepNick() const { return m_bKeepNick; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
bool CUser::IsAdmin() const { return m_bAdmin; }
bool CUser::MultiClients() const { return m_bMultiClients; }
bool CUser::BounceDCCs() const { return m_bBounceDCCs; }
const CString& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const CString& CUser::GetDefaultChanModes() const { return m_sDefaultChanModes; }
const vector<CChan*>& CUser::GetChans() const { return m_vChans; }
const vector<CServer*>& CUser::GetServers() const { return m_vServers; }
const CNick& CUser::GetIRCNick() const { return m_IRCNick; }
const CString& CUser::GetIRCServer() const { return m_sIRCServer; }
CString CUser::GetQuitMsg() const { return (!m_sQuitMsg.empty()) ? m_sQuitMsg : CZNC::GetTag(false); }
const MCString& CUser::GetCTCPReplies() const { return m_mssCTCPReplies; }
unsigned int CUser::GetBufferCount() const { return m_uBufferCount; }
bool CUser::KeepBuffer() const { return m_bKeepBuffer; }
bool CUser::AutoCycle() const { return m_bAutoCycle; }
// !Getters
