/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "Chan.h"
#include "DCCSock.h"
#include "IRCSock.h"
#include "Server.h"
#include "znc.h"

class CUserTimer : public CCron {
public:
	CUserTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		SetName("CUserTimer::" + m_pUser->GetUserName());
		Start(30);
	}
	virtual ~CUserTimer() {}

private:
protected:
	virtual void RunJob() {
		vector<CClient*>& vClients = m_pUser->GetClients();
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock && pIRCSock->GetTimeSinceLastDataTransaction() >= 180) {
			pIRCSock->PutIRC("PING :ZNC");
		}

		for (size_t a = 0; a < vClients.size(); a++) {
			CClient* pClient = vClients[a];

			if (pClient->GetTimeSinceLastDataTransaction() >= 180) {
				pClient->PutClient("PING :ZNC");
			}
		}

		if (m_pUser->IsIRCConnected()) {
			m_pUser->JoinChans();
		}
	}

	CUser* m_pUser;
};

CUser::CUser(const CString& sUserName) {
	m_pIRCSock = NULL;
	m_fTimezoneOffset = 0;
	m_uConnectTime = 0;
	SetUserName(sUserName);
	m_sNick = m_sCleanUserName;
	m_sIdent = m_sCleanUserName;
	m_sRealName = sUserName;
	m_uServerIdx = 0;
	m_uBytesRead = 0;
	m_uBytesWritten = 0;
	m_pModules = new CModules;
	m_RawBuffer.SetLineCount(100);   // This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200);  // This should be more than enough motd lines
	m_QueryBuffer.SetLineCount(250);
	m_bMultiClients = true;
	m_bBounceDCCs = true;
	m_eHashType = HASH_NONE;
	m_bUseClientIP = false;
	m_bDenyLoadMod = false;
	m_bAdmin= false;
	m_bIRCAway = false;
	m_bDenySetVHost= false;
	m_sStatusPrefix = "*";
	m_sChanPrefixes = "";
	m_uBufferCount = 50;
	m_uMaxJoinTries = 10;
	m_uMaxJoins = 5;
	m_bKeepBuffer = false;
	m_bBeingDeleted = false;
	m_sTimestampFormat = "[%H:%M:%S]";
	m_bAppendTimestamp = false;
	m_bPrependTimestamp = true;
	m_bIRCConnectEnabled = true;
	m_pUserTimer = new CUserTimer(this);
	CZNC::Get().GetManager().AddCron(m_pUserTimer);
}

CUser::~CUser() {
	DelClients();

	DelModules();

	DelServers();

	for (unsigned int b = 0; b < m_vChans.size(); b++) {
		delete m_vChans[b];
	}

	// This will cause an endless loop if the destructor doesn't remove the
	// socket from this list / if the socket doesn't exist any more.
	while (!m_sDCCBounces.empty())
		CZNC::Get().GetManager().DelSockByAddr((CZNCSock*) *m_sDCCBounces.begin());
	while (!m_sDCCSocks.empty())
		CZNC::Get().GetManager().DelSockByAddr((CZNCSock*) *m_sDCCSocks.begin());

	CZNC::Get().GetManager().DelCronByAddr(m_pUserTimer);
}

void CUser::DelModules() {
	delete m_pModules;
	m_pModules = NULL;
}

bool CUser::UpdateModule(const CString &sModule) {
	const map<CString,CUser*>& Users = CZNC::Get().GetUserMap();
	map<CString,CUser*>::const_iterator it;
	map<CUser*, CString> Affected;
	map<CUser*, CString>::iterator it2;
	bool error = false;

	for (it = Users.begin(); it != Users.end(); ++it) {
		CModule *pMod = it->second->GetModules().FindModule(sModule);
		if (pMod) {
			Affected[it->second] = pMod->GetArgs();
			it->second->GetModules().UnloadModule(pMod->GetModName());
		}
	}

	CString sErr;
	for (it2 = Affected.begin(); it2 != Affected.end(); ++it2) {
		if (!it2->first->GetModules().LoadModule(sModule, it2->second, it2->first, sErr)) {
			error = true;
			DEBUG("Failed to reload [" << sModule << "] for [" << it2->first->GetUserName()
					<< "]: " << sErr);
		}
	}

	return !error;
}

void CUser::DelClients() {
	for (unsigned int c = 0; c < m_vClients.size(); c++) {
		CClient* pClient = m_vClients[c];
		CZNC::Get().GetManager().DelSockByAddr(pClient);
	}

	m_vClients.clear();
}

void CUser::DelServers()
{
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		delete m_vServers[a];
	}

	m_vServers.clear();
}

void CUser::IRCConnected(CIRCSock* pIRCSock) {
	m_pIRCSock = pIRCSock;
}

void CUser::IRCDisconnected() {
	m_pIRCSock = NULL;

	SetIRCServer("");
	m_bIRCAway = false;

	// Get the reconnect going
	CheckIRCConnect();
}

CString CUser::ExpandString(const CString& sStr) const {
	CString sRet;
	return ExpandString(sStr, sRet);
}

CString& CUser::ExpandString(const CString& sStr, CString& sRet) const {
	// offset is in hours, so * 60 * 60 gets us seconds
	time_t iUserTime = time(NULL) + (time_t)(m_fTimezoneOffset * 60 * 60);
	char *szTime = ctime(&iUserTime);
	CString sTime;

	if (szTime) {
		sTime = szTime;
		// ctime() adds a trailing newline
		sTime.Trim();
	}

	sRet = sStr;
	sRet.Replace("%user%", GetUserName());
	sRet.Replace("%defnick%", GetNick());
	sRet.Replace("%nick%", GetCurNick());
	sRet.Replace("%altnick%", GetAltNick());
	sRet.Replace("%ident%", GetIdent());
	sRet.Replace("%realname%", GetRealName());
	sRet.Replace("%vhost%", GetVHost());
	sRet.Replace("%version%", CZNC::GetVersion());
	sRet.Replace("%time%", sTime);
	sRet.Replace("%uptime%", CZNC::Get().GetUptime());
	// The following lines do not exist. You must be on DrUgS!
	sRet.Replace("%znc%", "All your IRC are belong to ZNC");
	// Chosen by fair zocchihedron dice roll by SilverLeo
	sRet.Replace("%rand%", "42");

	return sRet;
}

CString CUser::AddTimestamp(const CString& sStr) const {
	CString sRet;
	return AddTimestamp(sStr, sRet);
}

CString& CUser::AddTimestamp(const CString& sStr, CString& sRet) const {
	char szTimestamp[1024];
	time_t tm;
	sRet = sStr;

	if (!GetTimestampFormat().empty() && (m_bAppendTimestamp || m_bPrependTimestamp)) {
		time(&tm);
		tm += (time_t)(m_fTimezoneOffset * 60 * 60); // offset is in hours
		size_t i = strftime(szTimestamp, sizeof(szTimestamp), GetTimestampFormat().c_str(), localtime(&tm));
		// If strftime returns 0, an error occured in format, or result is empty
		// In both cases just don't prepend/append anything to our string
		if (0 == i) {
			return sRet;
		}

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

	m_vClients.push_back(pClient);

	if (m_RawBuffer.IsEmpty()) {
		pClient->PutClient(":irc.znc.in 001 " + pClient->GetNick() + " :- Welcome to ZNC -");
	} else {
		unsigned int uIdx = 0;
		CString sLine;

		while (m_RawBuffer.GetLine(GetIRCNick().GetNick(), sLine, uIdx++)) {
			pClient->PutClient(sLine);
		}

		// The assumption is that the client got this nick from the 001 reply
		pClient->SetNick(GetIRCNick().GetNick());
	}

	// Send the cached MOTD
	unsigned int uIdx = 0;
	CString sLine;

	while (m_MotdBuffer.GetLine(GetIRCNick().GetNick(), sLine, uIdx++)) {
		pClient->PutClient(sLine);
	}

	if (GetIRCSock() != NULL) {
		CString sUserMode("");
		const set<unsigned char>& scUserModes = GetIRCSock()->GetUserModes();
		for (set<unsigned char>::const_iterator it = scUserModes.begin();
				it != scUserModes.end(); ++it) {
			sUserMode += *it;
		}
		if (!sUserMode.empty()) {
			pClient->PutClient(":" + GetIRCNick().GetNickMask() + " MODE " + GetIRCNick().GetNick() + " :+" + sUserMode);
		}
	}

	if (m_bIRCAway) {
		// If they want to know their away reason they'll have to whois
		// themselves. At least we can tell them their away status...
		pClient->PutClient(":irc.znc.in 306 " + GetIRCNick().GetNick() + " :You have been marked as being away");
	}

	const vector<CChan*>& vChans = GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		if ((vChans[a]->IsOn()) && (!vChans[a]->IsDetached())) {
			vChans[a]->JoinUser(true, "", pClient);
		}
	}

	CString sBufLine;
	while (m_QueryBuffer.GetNextLine(GetIRCNick().GetNick(), sBufLine)) {
		MODULECALL(OnPrivBufferPlayLine(*pClient, sBufLine), this, NULL, continue);
		pClient->PutClient(sBufLine);
	}

	// Tell them why they won't connect
	if (!GetIRCConnectEnabled())
		pClient->PutStatus("You are currently disconnected from IRC. "
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

bool CUser::Clone(const CUser& User, CString& sErrorRet, bool bCloneChans) {
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
		SetPass(User.GetPass(), User.GetPassHashType(), User.GetPassSalt());
	}

	SetNick(User.GetNick(false));
	SetAltNick(User.GetAltNick(false));
	SetIdent(User.GetIdent(false));
	SetRealName(User.GetRealName());
	SetStatusPrefix(User.GetStatusPrefix());
	SetVHost(User.GetVHost());
	SetDCCVHost(User.GetDCCVHost());
	SetQuitMsg(User.GetQuitMsg());
	SetSkinName(User.GetSkinName());
	SetDefaultChanModes(User.GetDefaultChanModes());
	SetBufferCount(User.GetBufferCount(), true);
	SetJoinTries(User.JoinTries());
	SetMaxJoins(User.MaxJoins());

	// Allowed Hosts
	m_ssAllowedHosts.clear();
	const set<CString>& ssHosts = User.GetAllowedHosts();
	for (set<CString>::const_iterator it = ssHosts.begin(); it != ssHosts.end(); ++it) {
		AddAllowedHost(*it);
	}

	for (a = 0; a < m_vClients.size(); a++) {
		CClient* pSock = m_vClients[a];

		if (!IsHostAllowed(pSock->GetRemoteIP())) {
			pSock->PutStatusNotice("You are being disconnected because your IP is no longer allowed to connect to this user");
			pSock->Close();
		}
	}

	// !Allowed Hosts

	// Servers
	const vector<CServer*>& vServers = User.GetServers();
	CString sServer;
	CServer* pCurServ = GetCurrentServer();

	if (pCurServ) {
		sServer = pCurServ->GetName();
	}

	DelServers();

	for (a = 0; a < vServers.size(); a++) {
		CServer* pServer = vServers[a];
		AddServer(pServer->GetName(), pServer->GetPort(), pServer->GetPass(), pServer->IsSSL());
	}

	m_uServerIdx = 0;
	for (a = 0; a < m_vServers.size(); a++) {
		if (sServer.Equals(m_vServers[a]->GetName())) {
			m_uServerIdx = a + 1;
			break;
		}
	}
	if (m_uServerIdx == 0) {
		m_uServerIdx = m_vServers.size();
		CIRCSock* pSock = GetIRCSock();

		if (pSock) {
			PutStatus("Jumping servers because this server is no longer in the list");
			pSock->Quit();
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
		CChan* pNewChan = User.FindChan(pChan->GetName());

		if (!pNewChan) {
			pChan->SetInConfig(false);
		} else {
			if (bCloneChans)
				pChan->Clone(*pNewChan);
		}
	}
	// !Chans

	// CTCP Replies
	m_mssCTCPReplies.clear();
	const MCString& msReplies = User.GetCTCPReplies();
	for (MCString::const_iterator it = msReplies.begin(); it != msReplies.end(); ++it) {
		AddCTCPReply(it->first, it->second);
	}
	// !CTCP Replies

	// Flags
	SetIRCConnectEnabled(User.GetIRCConnectEnabled());
	SetKeepBuffer(User.KeepBuffer());
	SetMultiClients(User.MultiClients());
	SetBounceDCCs(User.BounceDCCs());
	SetUseClientIP(User.UseClientIP());
	SetDenyLoadMod(User.DenyLoadMod());
	SetAdmin(User.IsAdmin());
	SetDenySetVHost(User.DenySetVHost());
	SetTimestampAppend(User.GetTimestampAppend());
	SetTimestampPrepend(User.GetTimestampPrepend());
	SetTimestampFormat(User.GetTimestampFormat());
	SetTimezoneOffset(User.GetTimezoneOffset());
	// !Flags

	// Modules
	set<CString> ssUnloadMods;
	CModules& vCurMods = GetModules();
	const CModules& vNewMods = User.GetModules();

	for (a = 0; a < vNewMods.size(); a++) {
		CString sModRet;
		CModule* pNewMod = vNewMods[a];
		CModule* pCurMod = vCurMods.FindModule(pNewMod->GetModName());

		if (!pCurMod) {
			vCurMods.LoadModule(pNewMod->GetModName(), pNewMod->GetArgs(), this, sModRet);
		} else if (pNewMod->GetArgs() != pCurMod->GetArgs()) {
			vCurMods.ReloadModule(pNewMod->GetModName(), pNewMod->GetArgs(), this, sModRet);
		}
	}

	for (a = 0; a < vCurMods.size(); a++) {
		CModule* pCurMod = vCurMods[a];
		CModule* pNewMod = vNewMods.FindModule(pCurMod->GetModName());

		if (!pNewMod) {
			ssUnloadMods.insert(pCurMod->GetModName());
		}
	}

	for (set<CString>::iterator it = ssUnloadMods.begin(); it != ssUnloadMods.end(); ++it) {
		vCurMods.UnloadModule(*it);
	}
	// !Modules

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

bool CUser::IsHostAllowed(const CString& sHostMask) const {
	if (m_ssAllowedHosts.empty()) {
		return true;
	}

	for (set<CString>::const_iterator a = m_ssAllowedHosts.begin(); a != m_ssAllowedHosts.end(); ++a) {
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

		p++;
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
		if (m_vChans[a]->GetName().Equals(pChan->GetName())) {
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
	for (vector<CChan*>::iterator a = m_vChans.begin(); a != m_vChans.end(); ++a) {
		if (sName.Equals((*a)->GetName())) {
			delete *a;
			m_vChans.erase(a);
			return true;
		}
	}

	return false;
}

bool CUser::PrintLine(CFile& File, CString sName, CString sValue) const {
	sName.Trim();
	sValue.Trim();

	if (sName.empty() || sValue.empty()) {
		DEBUG("Refused writing an invalid line to a user config. ["
			<< sName << "] [" << sValue << "]");
		return false;
	}

	// FirstLine() so that no one can inject new lines to the config if he
	// manages to add "\n" to e.g. sValue.
	CString sLine = "\t" + sName.FirstLine() + " = " + sValue.FirstLine() + "\n";
	if (File.Write(sLine) <= 0)
		return false;
	return true;
}

bool CUser::WriteConfig(CFile& File) {
	File.Write("<User " + GetUserName().FirstLine() + ">\n");

	if (m_eHashType != HASH_NONE) {
		CString sHash = "md5";
		if (m_eHashType == HASH_SHA256)
			sHash = "sha256";
		if (m_sPassSalt.empty()) {
			PrintLine(File, "Pass", sHash + "#" + GetPass());
		} else {
			PrintLine(File, "Pass", sHash + "#" + GetPass() + "#" + m_sPassSalt + "#");
		}
	} else {
		PrintLine(File, "Pass", "plain#" + GetPass());
	}
	PrintLine(File, "Nick", GetNick());
	PrintLine(File, "AltNick", GetAltNick());
	PrintLine(File, "Ident", GetIdent());
	PrintLine(File, "RealName", GetRealName());
	PrintLine(File, "VHost", GetVHost());
	PrintLine(File, "DCCVHost", GetDCCVHost());
	PrintLine(File, "QuitMsg", GetQuitMsg());
	if (CZNC::Get().GetStatusPrefix() != GetStatusPrefix())
		PrintLine(File, "StatusPrefix", GetStatusPrefix());
	PrintLine(File, "Skin", GetSkinName());
	PrintLine(File, "ChanModes", GetDefaultChanModes());
	PrintLine(File, "Buffer", CString(GetBufferCount()));
	PrintLine(File, "KeepBuffer", CString(KeepBuffer()));
	PrintLine(File, "MultiClients", CString(MultiClients()));
	PrintLine(File, "BounceDCCs", CString(BounceDCCs()));
	PrintLine(File, "DenyLoadMod", CString(DenyLoadMod()));
	PrintLine(File, "Admin", CString(IsAdmin()));
	PrintLine(File, "DenySetVHost", CString(DenySetVHost()));
	PrintLine(File, "DCCLookupMethod", CString((UseClientIP()) ? "client" : "default"));
	PrintLine(File, "TimestampFormat", GetTimestampFormat());
	PrintLine(File, "AppendTimestamp", CString(GetTimestampAppend()));
	PrintLine(File, "PrependTimestamp", CString(GetTimestampPrepend()));
	PrintLine(File, "TimezoneOffset", CString(m_fTimezoneOffset));
	PrintLine(File, "JoinTries", CString(m_uMaxJoinTries));
	PrintLine(File, "MaxJoins", CString(m_uMaxJoins));
	PrintLine(File, "IRCConnectEnabled", CString(GetIRCConnectEnabled()));
	File.Write("\n");

	// Allow Hosts
	if (!m_ssAllowedHosts.empty()) {
		for (set<CString>::iterator it = m_ssAllowedHosts.begin(); it != m_ssAllowedHosts.end(); ++it) {
			PrintLine(File, "Allow", *it);
		}

		File.Write("\n");
	}

	// CTCP Replies
	if (!m_mssCTCPReplies.empty()) {
		for (MCString::const_iterator itb = m_mssCTCPReplies.begin(); itb != m_mssCTCPReplies.end(); ++itb) {
			PrintLine(File, "CTCPReply", itb->first.AsUpper() + " " + itb->second);
		}

		File.Write("\n");
	}

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

		File.Write("\n");
	}

	// Servers
	for (unsigned int b = 0; b < m_vServers.size(); b++) {
		PrintLine(File, "Server", m_vServers[b]->GetString());
	}

	// Chans
	for (unsigned int c = 0; c < m_vChans.size(); c++) {
		CChan* pChan = m_vChans[c];
		if (pChan->InConfig()) {
			File.Write("\n");
			if (!pChan->WriteConfig(File)) {
				return false;
			}
		}
	}

	MODULECALL(OnWriteUserConfig(File), this, NULL,);

	File.Write("</User>\n");

	return true;
}

CChan* CUser::FindChan(const CString& sName) const {
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		CChan* pChan = m_vChans[a];
		if (sName.Equals(pChan->GetName())) {
			return pChan;
		}
	}

	return NULL;
}

void CUser::JoinChans() {
	// Avoid divsion by zero, it's bad!
	if (m_vChans.empty())
		return;

	// We start at a random offset into the channel list so that if your
	// first 3 channels are invite-only and you got MaxJoins == 3, ZNC will
	// still be able to join the rest of your channels.
	unsigned int start = rand() % m_vChans.size();
	unsigned int uJoins = m_uMaxJoins;
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		unsigned int idx = (start + a) % m_vChans.size();
		CChan* pChan = m_vChans[idx];
		if (!pChan->IsOn() && !pChan->IsDisabled()) {
			if (!JoinChan(pChan))
				continue;

			// Limit the number of joins
			if (uJoins != 0 && --uJoins == 0)
				return;
		}
	}
}

bool CUser::JoinChan(CChan* pChan) {
	if (JoinTries() != 0 && pChan->GetJoinTries() >= JoinTries()) {
		PutStatus("The channel " + pChan->GetName() + " could not be joined, disabling it.");
		pChan->Disable();
	} else {
		pChan->IncJoinTries();
		MODULECALL(OnTimerAutoJoin(*pChan), this, NULL, return false);

		PutIRC("JOIN " + pChan->GetName() + " " + pChan->GetKey());
		return true;
	}
	return false;
}

CServer* CUser::FindServer(const CString& sName) const {
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		CServer* pServer = m_vServers[a];
		if (sName.Equals(pServer->GetName())) {
			return pServer;
		}
	}

	return NULL;
}

bool CUser::DelServer(const CString& sName, unsigned short uPort, const CString& sPass) {
	if (sName.empty()) {
		return false;
	}

	unsigned int a = 0;
	bool bSawCurrentServer = false;
	CServer* pCurServer = GetCurrentServer();

	for (vector<CServer*>::iterator it = m_vServers.begin(); it != m_vServers.end(); it++, a++) {
		CServer* pServer = *it;

		if (pServer == pCurServer)
			bSawCurrentServer = true;

		if (!pServer->GetName().Equals(sName))
			continue;

		if (uPort != 0 && pServer->GetPort() != uPort)
			continue;

		if (!sPass.empty() && pServer->GetPass() != sPass)
			continue;

		m_vServers.erase(it);

		if (pServer == pCurServer) {
			CIRCSock* pIRCSock = GetIRCSock();

			// Make sure we don't skip the next server in the list!
			if (m_uServerIdx) {
				m_uServerIdx--;
			}

			if (pIRCSock) {
				pIRCSock->Quit();
				PutStatus("Your current server was removed, jumping...");
			}
		} else if (!bSawCurrentServer) {
			// Our current server comes after the server which we
			// are removing. This means that it now got a different
			// index in m_vServers!
			m_uServerIdx--;
		}

		delete pServer;

		return true;
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

	unsigned short uPort = sPort.ToUShort();
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

	// Check if server is already added
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		CServer* pServer = m_vServers[a];

		if (!sName.Equals(pServer->GetName()))
			continue;

		if (uPort != pServer->GetPort())
			continue;

		if (sPass != pServer->GetPass())
			continue;

		if (bSSL != pServer->IsSSL())
			continue;

		// Server is already added
		return false;
	}

	CServer* pServer = new CServer(sName, uPort, sPass, bSSL);
	m_vServers.push_back(pServer);

	CheckIRCConnect();

	return true;
}

bool CUser::IsLastServer() const {
	return (m_uServerIdx >= m_vServers.size());
}

CServer* CUser::GetNextServer() {
	if (m_vServers.empty()) {
		return NULL;
	}

	if (m_uServerIdx >= m_vServers.size()) {
		m_uServerIdx = 0;
	}

	return m_vServers[m_uServerIdx++];
}

CServer* CUser::GetCurrentServer() const {
	unsigned int uIdx = (m_uServerIdx) ? m_uServerIdx -1 : 0;

	if (uIdx >= m_vServers.size()) {
		return NULL;
	}

	return m_vServers[uIdx];
}

bool CUser::CheckPass(const CString& sPass) const {
	switch (m_eHashType)
	{
	case HASH_MD5:
		return m_sPass.Equals(CUtils::SaltedMD5Hash(sPass, m_sPassSalt));
	case HASH_SHA256:
		return m_sPass.Equals(CUtils::SaltedSHA256Hash(sPass, m_sPassSalt));
	case HASH_NONE:
	default:
		return (sPass == m_sPass);
	}
}

/*CClient* CUser::GetClient() {
	// Todo: optimize this by saving a pointer to the sock
	CSockManager& Manager = CZNC::Get().GetManager();
	CString sSockName = "USR::" + m_sUserName;

	for (unsigned int a = 0; a < Manager.size(); a++) {
		Csock* pSock = Manager[a];
		if (pSock->GetSockName().Equals(sSockName)) {
			if (!pSock->IsClosed()) {
				return (CClient*) pSock;
			}
		}
	}

	return (CClient*) CZNC::Get().GetManager().FindSockByName(sSockName);
}*/

CString CUser::GetLocalIP() {
	CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetLocalIP();
	}

	if (!m_vClients.empty()) {
		return m_vClients[0]->GetLocalIP();
	}

	return "";
}

CString CUser::GetLocalDCCIP() {
	if (!GetDCCVHost().empty())
		return GetDCCVHost();
	return GetLocalIP();
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

bool CUser::ResumeFile(unsigned short uPort, unsigned long uFileSize) {
	CSockManager& Manager = CZNC::Get().GetManager();

	for (unsigned int a = 0; a < Manager.size(); a++) {
		if (Manager[a]->GetSockName().Equals("DCC::LISTEN::", false, 13)) {
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
	CString sFullPath = CDir::ChangeDir(GetDLPath(), sFileName, CZNC::Get().GetHomePath());
	CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath, sModuleName);

	CFile* pFile = pSock->OpenFile(false);

	if (!pFile) {
		delete pSock;
		return false;
	}

	unsigned short uPort = CZNC::Get().GetManager().ListenRand("DCC::LISTEN::" + sRemoteNick, GetLocalDCCIP(), false, SOMAXCONN, pSock, 120);

	if (GetNick().Equals(sRemoteNick)) {
		PutUser(":" + GetStatusPrefix() + "status!znc@znc.in PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(GetLocalDCCIP())) + " "
				+ CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
	} else {
		PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(GetLocalDCCIP())) + " "
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

	if (!CZNC::Get().GetManager().Connect(sRemoteIP, uRemotePort, "DCC::GET::" + sRemoteNick, 60, false, GetLocalDCCIP(), pSock)) {
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

	if (!m_vClients.empty()) {
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

	// set paths that depend on the user name:
	m_sUserPath = CZNC::Get().GetUserPath() + "/" + m_sUserName;
	m_sDLPath = GetUserPath() + "/downloads";
}

bool CUser::IsChan(const CString& sChan) const {
	if (sChan.empty())
		return false; // There is no way this is a chan
	if (GetChanPrefixes().empty())
		return true; // We can't know, so we allow everything
	// Thanks to the above if (empty), we can do sChan[0]
	return GetChanPrefixes().find(sChan[0]) != CString::npos;
}

void CUser::SetNick(const CString& s) { m_sNick = s; }
void CUser::SetAltNick(const CString& s) { m_sAltNick = s; }
void CUser::SetIdent(const CString& s) { m_sIdent = s; }
void CUser::SetRealName(const CString& s) { m_sRealName = s; }
void CUser::SetVHost(const CString& s) { m_sVHost = s; }
void CUser::SetDCCVHost(const CString& s) { m_sDCCVHost = s; }
void CUser::SetPass(const CString& s, eHashType eHash, const CString& sSalt) {
	m_sPass = s;
	m_eHashType = eHash;
	m_sPassSalt = sSalt;
}
void CUser::SetMultiClients(bool b) { m_bMultiClients = b; }
void CUser::SetBounceDCCs(bool b) { m_bBounceDCCs = b; }
void CUser::SetUseClientIP(bool b) { m_bUseClientIP = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetAdmin(bool b) { m_bAdmin = b; }
void CUser::SetDenySetVHost(bool b) { m_bDenySetVHost = b; }
void CUser::SetDefaultChanModes(const CString& s) { m_sDefaultChanModes = s; }
void CUser::SetIRCServer(const CString& s) { m_sIRCServer = s; }
void CUser::SetQuitMsg(const CString& s) { m_sQuitMsg = s; }
void CUser::SetKeepBuffer(bool b) { m_bKeepBuffer = b; }

bool CUser::SetBufferCount(unsigned int u, bool bForce) {
	if (!bForce && u > CZNC::Get().GetMaxBufferSize())
		return false;
	m_uBufferCount = u;
	return true;
}

void CUser::CheckIRCConnect() {
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
const CString& CUser::GetDCCVHost() const { return m_sDCCVHost; }
const CString& CUser::GetPass() const { return m_sPass; }
CUser::eHashType CUser::GetPassHashType() const { return m_eHashType; }
const CString& CUser::GetPassSalt() const { return m_sPassSalt; }

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

bool CUser::UseClientIP() const { return m_bUseClientIP; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
bool CUser::IsAdmin() const { return m_bAdmin; }
bool CUser::DenySetVHost() const { return m_bDenySetVHost; }
bool CUser::MultiClients() const { return m_bMultiClients; }
bool CUser::BounceDCCs() const { return m_bBounceDCCs; }
const CString& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const CString& CUser::GetDefaultChanModes() const { return m_sDefaultChanModes; }
const vector<CChan*>& CUser::GetChans() const { return m_vChans; }
const vector<CServer*>& CUser::GetServers() const { return m_vServers; }
const CNick& CUser::GetIRCNick() const { return m_IRCNick; }
const CString& CUser::GetIRCServer() const { return m_sIRCServer; }
CString CUser::GetQuitMsg() const { return (!m_sQuitMsg.Trim_n().empty()) ? m_sQuitMsg : CZNC::GetTag(false); }
const MCString& CUser::GetCTCPReplies() const { return m_mssCTCPReplies; }
unsigned int CUser::GetBufferCount() const { return m_uBufferCount; }
bool CUser::KeepBuffer() const { return m_bKeepBuffer; }
//CString CUser::GetSkinName() const { return (!m_sSkinName.empty()) ? m_sSkinName : CZNC::Get().GetSkinName(); }
CString CUser::GetSkinName() const { return m_sSkinName; }
// !Getters
