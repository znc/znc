/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/FileUtils.h>
#include <znc/Config.h>
#include <znc/Client.h>
#include <znc/IRCSock.h>
#include <znc/Server.h>
#include <znc/Chan.h>
#include <znc/znc.h>

bool CIRCNetwork::IsValidNetwork(const CString& sNetwork) {
	// ^[-\w]+$

	if (sNetwork.empty()) {
		return false;
	}

	const char *p = sNetwork.c_str();
	while (*p) {
		if (*p != '_' && *p != '-' && !isalnum(*p)) {
			return false;
		}

		p++;
	}

	return true;
}

CIRCNetwork::CIRCNetwork(CUser *pUser, const CString& sName) {
	m_pUser = NULL;
	SetUser(pUser);
	m_sName = sName;

	m_pModules = new CModules;

	m_pIRCSock = NULL;
	m_uServerIdx = 0;

	m_sChanPrefixes = "";
	m_bIRCAway = false;

	m_RawBuffer.SetLineCount(100);   // This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200);  // This should be more than enough motd lines
	m_QueryBuffer.SetLineCount(250);
}

CIRCNetwork::CIRCNetwork(CUser *pUser, const CIRCNetwork *pNetwork, bool bCloneChans) {
	m_pUser = NULL;
	SetUser(pUser);
	m_sName = pNetwork->GetName();

	m_pModules = new CModules;

	m_pIRCSock = NULL;
	m_uServerIdx = 0;

	m_sChanPrefixes = "";
	m_bIRCAway = false;

	m_RawBuffer.SetLineCount(100);   // This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200);  // This should be more than enough motd lines
	m_QueryBuffer.SetLineCount(250);

	// Servers
	const vector<CServer*>& vServers = pNetwork->GetServers();
	CString sServer;
	CServer* pCurServ = GetCurrentServer();

	if (pCurServ) {
		sServer = pCurServ->GetName();
	}

	DelServers();

	unsigned int a;
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
	const vector<CChan*>& vChans = pNetwork->GetChans();
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
		CChan* pNewChan = pNetwork->FindChan(pChan->GetName());

		if (!pNewChan) {
			pChan->SetInConfig(false);
		} else {
			if (bCloneChans)
				pChan->Clone(*pNewChan);
		}
	}
	// !Chans
}

CIRCNetwork::~CIRCNetwork() {
	if (m_pIRCSock) {
		CZNC::Get().GetManager().DelSockByAddr(m_pIRCSock);
		m_pIRCSock = NULL;
	}

	// Delete clients
	for (vector<CClient*>::const_iterator it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CZNC::Get().GetManager().DelSockByAddr(*it);
	}
	m_vClients.clear();

	// Delete servers
	DelServers();

	// Delete modules (this unloads all modules)
	delete m_pModules;
	m_pModules = NULL;

	// Delete Channels
	for (vector<CChan*>::const_iterator it = m_vChans.begin(); it != m_vChans.end(); ++it) {
		delete *it;
	}
	m_vChans.clear();

	SetUser(NULL);

	// Make sure we are not in the connection queue
	CZNC::Get().GetConnectionQueue().remove(this);
}

void CIRCNetwork::DelServers() {
	for (vector<CServer*>::const_iterator it = m_vServers.begin(); it != m_vServers.end(); ++it) {
		delete *it;
	}
	m_vServers.clear();
}

CString CIRCNetwork::GetNetworkPath() {
	CString sNetworkPath = m_pUser->GetUserPath() + "/networks/" + m_sName;

	if (!CFile::Exists(sNetworkPath)) {
		CDir::MakeDir(sNetworkPath);
	}

	return sNetworkPath;
}

template<class T>
struct TOption {
	const char *name;
	void (CIRCNetwork::*pSetter)(T);
};

bool CIRCNetwork::ParseConfig(CConfig *pConfig, CString& sError, bool bUpgrade) {
	VCString vsList;
	VCString::const_iterator vit;

	if (!bUpgrade) {
		TOption<const CString&> StringOptions[] = {
			{ "nick", &CIRCNetwork::SetNick },
			{ "altnick", &CIRCNetwork::SetAltNick },
			{ "ident", &CIRCNetwork::SetIdent },
			{ "realname", &CIRCNetwork::SetRealName }
		};
		size_t numStringOptions = sizeof(StringOptions) / sizeof(StringOptions[0]);

		for (size_t i = 0; i < numStringOptions; i++) {
			CString sValue;
			if (pConfig->FindStringEntry(StringOptions[i].name, sValue))
				(this->*StringOptions[i].pSetter)(sValue);
		}

		pConfig->FindStringVector("loadmodule", vsList);
		for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
			CString sValue = *vit;
			CString sModName = sValue.Token(0);

			CUtils::PrintAction("Loading Module [" + sModName + "]");
			CString sModRet;
			CString sArgs = sValue.Token(1, true);

			bool bModRet = GetModules().LoadModule(sModName, sArgs, CModInfo::NetworkModule, GetUser(), this, sModRet);

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}
		}
	}

	pConfig->FindStringVector("server", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		CUtils::PrintAction("Adding Server [" + *vit + "]");
		CUtils::PrintStatus(AddServer(*vit));
	}

	pConfig->FindStringVector("chan", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		AddChan(*vit, true);
	}

	CConfig::SubConfig subConf;
	CConfig::SubConfig::const_iterator subIt;

	pConfig->FindSubConfig("chan", subConf);
	for (subIt = subConf.begin(); subIt != subConf.end(); ++subIt) {
		const CString& sChanName = subIt->first;
		CConfig* pSubConf = subIt->second.m_pSubConfig;
		CChan* pChan = new CChan(sChanName, this, true, pSubConf);

		if (!pSubConf->empty()) {
			sError = "Unhandled lines in config for User [" + m_pUser->GetUserName() + "], Network [" + GetName() + "], Channel [" + sChanName + "]!";
			CUtils::PrintError(sError);

			CZNC::DumpConfig(pSubConf);
			return false;
		}

		// Save the channel name, because AddChan
		// deletes the CChannel*, if adding fails
		sError = pChan->GetName();
		if (!AddChan(pChan)) {
			sError = "Channel [" + sError + "] defined more than once";
			CUtils::PrintError(sError);
			return false;
		}
		sError.clear();
	}

	return true;
}

CConfig CIRCNetwork::ToConfig() {
	CConfig config;

	if (!m_sNick.empty()) {
		config.AddKeyValuePair("Nick", m_sNick);
	}

	if (!m_sAltNick.empty()) {
		config.AddKeyValuePair("AltNick", m_sAltNick);
	}

	if (!m_sIdent.empty()) {
		config.AddKeyValuePair("Ident", m_sIdent);
	}

	if (!m_sRealName.empty()) {
		config.AddKeyValuePair("RealName", m_sRealName);
	}

	// Modules
	CModules& Mods = GetModules();

	if (!Mods.empty()) {
		for (unsigned int a = 0; a < Mods.size(); a++) {
			CString sArgs = Mods[a]->GetArgs();

			if (!sArgs.empty()) {
				sArgs = " " + sArgs;
			}

			config.AddKeyValuePair("LoadModule", Mods[a]->GetModName() + sArgs);
		}
	}

	// Servers
	for (unsigned int b = 0; b < m_vServers.size(); b++) {
		config.AddKeyValuePair("Server", m_vServers[b]->GetString());
	}

	// Chans
	for (unsigned int c = 0; c < m_vChans.size(); c++) {
		CChan* pChan = m_vChans[c];
		if (pChan->InConfig()) {
			config.AddSubConfig("Chan", pChan->GetName(), pChan->ToConfig());
		}
	}

	return config;
}

void CIRCNetwork::BounceAllClients() {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->BouncedOff();
	}

	m_vClients.clear();
}

void CIRCNetwork::ClientConnected(CClient *pClient) {
	if (!m_pUser->MultiClients()) {
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
		NETWORKMODULECALL(OnPrivBufferPlayLine(*pClient, sBufLine), m_pUser, this, NULL, continue);
		pClient->PutClient(sBufLine);
	}

	// Tell them why they won't connect
	if (!m_pUser->GetIRCConnectEnabled())
		pClient->PutStatus("You are currently disconnected from IRC. "
				"Use 'connect' to reconnect.");
}

void CIRCNetwork::ClientDisconnected(CClient *pClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if (m_vClients[a] == pClient) {
			m_vClients.erase(m_vClients.begin() + a);
			break;
		}
	}
}

CUser* CIRCNetwork::GetUser() {
	return m_pUser;
}

const CString& CIRCNetwork::GetName() const {
	return m_sName;
}

void CIRCNetwork::SetUser(CUser *pUser) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->PutStatus("This network is being deleted or moved to another user.");
		m_vClients[a]->SetNetwork(NULL);
	}

	m_vClients.clear();

	if (m_pUser) {
		m_pUser->RemoveNetwork(this);
	}

	m_pUser = pUser;
	if (m_pUser) {
		m_pUser->AddNetwork(this);
	}
}

bool CIRCNetwork::SetName(const CString& sName) {
	if (IsValidNetwork(sName)) {
		m_sName = sName;
		return true;
	}

	return false;
}

bool CIRCNetwork::PutUser(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
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

bool CIRCNetwork::PutStatus(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
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

bool CIRCNetwork::PutModule(const CString& sModule, const CString& sLine, CClient* pClient, CClient* pSkipClient) {
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

// Channels

const vector<CChan*>& CIRCNetwork::GetChans() const { return m_vChans; }

CChan* CIRCNetwork::FindChan(const CString& sName) const {
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		CChan* pChan = m_vChans[a];
		if (sName.Equals(pChan->GetName())) {
			return pChan;
		}
	}

	return NULL;
}

bool CIRCNetwork::AddChan(CChan* pChan) {
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

bool CIRCNetwork::AddChan(const CString& sName, bool bInConfig) {
	if (sName.empty() || FindChan(sName)) {
		return false;
	}

	CChan* pChan = new CChan(sName, this, bInConfig);
	m_vChans.push_back(pChan);
	return true;
}

bool CIRCNetwork::DelChan(const CString& sName) {
	for (vector<CChan*>::iterator a = m_vChans.begin(); a != m_vChans.end(); ++a) {
		if (sName.Equals((*a)->GetName())) {
			delete *a;
			m_vChans.erase(a);
			return true;
		}
	}

	return false;
}

void CIRCNetwork::JoinChans() {
	// Avoid divsion by zero, it's bad!
	if (m_vChans.empty())
		return;

	// We start at a random offset into the channel list so that if your
	// first 3 channels are invite-only and you got MaxJoins == 3, ZNC will
	// still be able to join the rest of your channels.
	unsigned int start = rand() % m_vChans.size();
	unsigned int uJoins = m_pUser->MaxJoins();
	set<CChan*> sChans;
	for (unsigned int a = 0; a < m_vChans.size(); a++) {
		unsigned int idx = (start + a) % m_vChans.size();
		CChan* pChan = m_vChans[idx];
		if (!pChan->IsOn() && !pChan->IsDisabled()) {
			if (!JoinChan(pChan))
				continue;

			sChans.insert(pChan);

			// Limit the number of joins
			if (uJoins != 0 && --uJoins == 0)
				break;
		}
	}

	while (!sChans.empty())
		JoinChans(sChans);
}

void CIRCNetwork::JoinChans(set<CChan*>& sChans) {
	CString sKeys, sJoin;
	bool bHaveKey = false;
	size_t uiJoinLength = strlen("JOIN ");

	while (!sChans.empty()) {
		set<CChan*>::iterator it = sChans.begin();
		const CString& sName = (*it)->GetName();
		const CString& sKey = (*it)->GetKey();
		size_t len = sName.length() + sKey.length();
		len += 2; // two comma

		if (!sKeys.empty() && uiJoinLength + len >= 512)
			break;

		if (!sJoin.empty()) {
			sJoin += ",";
			sKeys += ",";
		}
		uiJoinLength += len;
		sJoin += sName;
		if (!sKey.empty()) {
			sKeys += sKey;
			bHaveKey = true;
		}
		sChans.erase(it);
	}

	if (bHaveKey)
		PutIRC("JOIN " + sJoin + " " + sKeys);
	else
		PutIRC("JOIN " + sJoin);
}

bool CIRCNetwork::JoinChan(CChan* pChan) {
	if (m_pUser->JoinTries() != 0 && pChan->GetJoinTries() >= m_pUser->JoinTries()) {
		PutStatus("The channel " + pChan->GetName() + " could not be joined, disabling it.");
		pChan->Disable();
	} else {
		pChan->IncJoinTries();
		NETWORKMODULECALL(OnTimerAutoJoin(*pChan), m_pUser, this, NULL, return false);
		return true;
	}
	return false;
}

bool CIRCNetwork::IsChan(const CString& sChan) const {
	if (sChan.empty())
		return false; // There is no way this is a chan
	if (GetChanPrefixes().empty())
		return true; // We can't know, so we allow everything
	// Thanks to the above if (empty), we can do sChan[0]
	return GetChanPrefixes().find(sChan[0]) != CString::npos;
}

// Server list

const vector<CServer*>& CIRCNetwork::GetServers() const { return m_vServers; }

CServer* CIRCNetwork::FindServer(const CString& sName) const {
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		CServer* pServer = m_vServers[a];
		if (sName.Equals(pServer->GetName())) {
			return pServer;
		}
	}

	return NULL;
}

bool CIRCNetwork::DelServer(const CString& sName, unsigned short uPort, const CString& sPass) {
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

bool CIRCNetwork::AddServer(const CString& sName) {
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

bool CIRCNetwork::AddServer(const CString& sName, unsigned short uPort, const CString& sPass, bool bSSL) {
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

CServer* CIRCNetwork::GetNextServer() {
	if (m_vServers.empty()) {
		return NULL;
	}

	if (m_uServerIdx >= m_vServers.size()) {
		m_uServerIdx = 0;
	}

	return m_vServers[m_uServerIdx++];
}

CServer* CIRCNetwork::GetCurrentServer() const {
	unsigned int uIdx = (m_uServerIdx) ? m_uServerIdx -1 : 0;

	if (uIdx >= m_vServers.size()) {
		return NULL;
	}

	return m_vServers[uIdx];
}

void CIRCNetwork::SetIRCServer(const CString& s) { m_sIRCServer = s; }

bool CIRCNetwork::SetNextServer(const CServer* pServer) {
	for (unsigned int a = 0; a < m_vServers.size(); a++) {
		if (m_vServers[a] == pServer) {
			m_uServerIdx = a;
			return true;
		}
	}

	return false;
}

bool CIRCNetwork::IsLastServer() const {
	return (m_uServerIdx >= m_vServers.size());
}

const CString& CIRCNetwork::GetIRCServer() const { return m_sIRCServer; }
const CNick& CIRCNetwork::GetIRCNick() const { return m_IRCNick; }

void CIRCNetwork::SetIRCNick(const CNick& n) {
	m_IRCNick = n;

	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		m_vClients[a]->SetNick(n.GetNick());
	}
}

CString CIRCNetwork::GetCurNick() const {
	const CIRCSock* pIRCSock = GetIRCSock();

	if (pIRCSock) {
		return pIRCSock->GetNick();
	}

	if (!m_vClients.empty()) {
		return m_vClients[0]->GetNick();
	}

	return "";
}

bool CIRCNetwork::Connect() {
	if (!m_pUser->GetIRCConnectEnabled() || m_pIRCSock || !HasServers())
		return false;

	CServer *pServer = GetNextServer();
	if (!pServer)
		return false;

	if (CZNC::Get().GetServerThrottle(pServer->GetName())) {
		CZNC::Get().AddNetworkToQueue(this);
		return false;
	}

	CZNC::Get().AddServerThrottle(pServer->GetName());

	bool bSSL = pServer->IsSSL();
#ifndef HAVE_LIBSSL
	if (bSSL) {
		PutStatus("Cannot connect to [" + pServer->GetString(false) + "], ZNC is not compiled with SSL.");
		CZNC::Get().AddNetworkToQueue(this);
		return false;
	}
#endif

	CIRCSock *pIRCSock = new CIRCSock(this);
	pIRCSock->SetPass(pServer->GetPass());

	DEBUG("Connecting user/network [" << m_pUser->GetUserName() << "/" << m_sName << "]");

	NETWORKMODULECALL(OnIRCConnecting(pIRCSock), m_pUser, this, NULL,
		DEBUG("Some module aborted the connection attempt");
		PutStatus("Some module aborted the connection attempt");
		delete pIRCSock;
		CZNC::Get().AddNetworkToQueue(this);
		return false;
	);

	CString sSockName = "IRC::" + m_pUser->GetUserName() + "::" + m_sName;
	if (!CZNC::Get().GetManager().Connect(pServer->GetName(), pServer->GetPort(), sSockName, 120, bSSL, m_pUser->GetBindHost(), pIRCSock)) {
		PutStatus("Unable to connect. (Bad host?)");
		CZNC::Get().AddNetworkToQueue(this);
		return false;
	}

	return true;
}

bool CIRCNetwork::IsIRCConnected() const {
	const CIRCSock* pSock = GetIRCSock();
	return (pSock && pSock->IsAuthed());
}

void CIRCNetwork::SetIRCSocket(CIRCSock* pIRCSock) {
	m_pIRCSock = pIRCSock;
}

void CIRCNetwork::IRCDisconnected() {
	m_pIRCSock = NULL;

	SetIRCServer("");
	m_bIRCAway = false;

	// Get the reconnect going
	CheckIRCConnect();
}

void CIRCNetwork::CheckIRCConnect() {
	// Do we want to connect?
	if (m_pUser->GetIRCConnectEnabled() && GetIRCSock() == NULL)
		CZNC::Get().AddNetworkToQueue(this);
}

bool CIRCNetwork::PutIRC(const CString& sLine) {
	CIRCSock* pIRCSock = GetIRCSock();

	if (!pIRCSock) {
		return false;
	}

	pIRCSock->PutIRC(sLine);
	return true;
}

const CString& CIRCNetwork::GetNick(const bool bAllowDefault) const {
	if (m_sNick.empty()) {
		return m_pUser->GetNick(bAllowDefault);
	}

	return m_sNick;
}

const CString& CIRCNetwork::GetAltNick(const bool bAllowDefault) const {
	if (m_sAltNick.empty()) {
		return m_pUser->GetAltNick(bAllowDefault);
	}

	return m_sAltNick;
}

const CString& CIRCNetwork::GetIdent(const bool bAllowDefault) const {
	if (m_sIdent.empty()) {
		return m_pUser->GetIdent(bAllowDefault);
	}

	return m_sIdent;
}

const CString& CIRCNetwork::GetRealName() const {
	if (m_sRealName.empty()) {
		return m_pUser->GetRealName();
	}

	return m_sRealName;
}

void CIRCNetwork::SetNick(const CString& s) {
	if (m_pUser->GetNick().Equals(s)) {
		m_sNick = "";
	} else {
		m_sNick = s;
	}
}

void CIRCNetwork::SetAltNick(const CString& s) {
	if (m_pUser->GetAltNick().Equals(s)) {
		m_sAltNick = "";
	} else {
		m_sAltNick = s;
	}
}

void CIRCNetwork::SetIdent(const CString& s) {
		if (m_pUser->GetIdent().Equals(s)) {
		m_sIdent = "";
	} else {
		m_sIdent = s;
	}
}

void CIRCNetwork::SetRealName(const CString& s) {
	if (m_pUser->GetRealName().Equals(s)) {
		m_sRealName = "";
	} else {
		m_sRealName = s;
	}
}

CString CIRCNetwork::ExpandString(const CString& sStr) const {
	CString sRet;
	return ExpandString(sStr, sRet);
}

CString& CIRCNetwork::ExpandString(const CString& sStr, CString& sRet) const {
	sRet = sStr;
	sRet.Replace("%defnick%", GetNick());
	sRet.Replace("%nick%", GetCurNick());
	sRet.Replace("%altnick%", GetAltNick());
	sRet.Replace("%ident%", GetIdent());
	sRet.Replace("%realname%", GetRealName());

	return m_pUser->ExpandString(sRet, sRet);
}
