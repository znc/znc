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

#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/FileUtils.h>
#include <znc/Config.h>
#include <znc/IRCSock.h>
#include <znc/Server.h>
#include <znc/Chan.h>

using std::vector;
using std::set;

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

	m_fFloodRate = 1;
	m_uFloodBurst = 4;

	m_RawBuffer.SetLineCount(100, true);   // This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200, true);  // This should be more than enough motd lines
	m_QueryBuffer.SetLineCount(250, true);

	SetIRCConnectEnabled(true);
}

CIRCNetwork::CIRCNetwork(CUser *pUser, const CIRCNetwork &Network) {
	m_pUser = NULL;
	SetUser(pUser);

	m_pModules = new CModules;

	m_pIRCSock = NULL;
	m_uServerIdx = 0;

	m_sChanPrefixes = "";
	m_bIRCAway = false;

	m_RawBuffer.SetLineCount(100, true);   // This should be more than enough raws, especially since we are buffering the MOTD separately
	m_MotdBuffer.SetLineCount(200, true);  // This should be more than enough motd lines
	m_QueryBuffer.SetLineCount(250, true);

	Clone(Network);
}

void CIRCNetwork::Clone(const CIRCNetwork& Network, bool bCloneName) {
	if (bCloneName) {
		m_sName = Network.GetName();
	}

	m_fFloodRate = Network.GetFloodRate();
	m_uFloodBurst = Network.GetFloodBurst();

	SetNick(Network.GetNick());
	SetAltNick(Network.GetAltNick());
	SetIdent(Network.GetIdent());
	SetRealName(Network.GetRealName());
	SetBindHost(Network.GetBindHost());

	// Servers
	const vector<CServer*>& vServers = Network.GetServers();
	CString sServer;
	CServer* pCurServ = GetCurrentServer();

	if (pCurServ) {
		sServer = pCurServ->GetName();
	}

	DelServers();

	size_t a;
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
	const vector<CChan*>& vChans = Network.GetChans();
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
		CChan* pNewChan = Network.FindChan(pChan->GetName());

		if (!pNewChan) {
			pChan->SetInConfig(false);
		} else {
			pChan->Clone(*pNewChan);
		}
	}
	// !Chans

	// Modules
	set<CString> ssUnloadMods;
	CModules& vCurMods = GetModules();
	const CModules& vNewMods = Network.GetModules();

	for (a = 0; a < vNewMods.size(); a++) {
		CString sModRet;
		CModule* pNewMod = vNewMods[a];
		CModule* pCurMod = vCurMods.FindModule(pNewMod->GetModName());

		if (!pCurMod) {
			vCurMods.LoadModule(pNewMod->GetModName(), pNewMod->GetArgs(), CModInfo::NetworkModule, m_pUser, this, sModRet);
		} else if (pNewMod->GetArgs() != pCurMod->GetArgs()) {
			vCurMods.ReloadModule(pNewMod->GetModName(), pNewMod->GetArgs(), m_pUser, this, sModRet);
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

	SetIRCConnectEnabled(Network.GetIRCConnectEnabled());
}

CIRCNetwork::~CIRCNetwork() {
	if (m_pIRCSock) {
		CZNC::Get().GetManager().DelSockByAddr(m_pIRCSock);
		m_pIRCSock = NULL;
	}

	// Delete clients
	while (!m_vClients.empty()) {
		CZNC::Get().GetManager().DelSockByAddr(m_vClients[0]);
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
			{ "realname", &CIRCNetwork::SetRealName },
			{ "bindhost", &CIRCNetwork::SetBindHost },
		};
		size_t numStringOptions = sizeof(StringOptions) / sizeof(StringOptions[0]);
		TOption<bool> BoolOptions[] = {
			{ "ircconnectenabled", &CIRCNetwork::SetIRCConnectEnabled },
		};
		size_t numBoolOptions = sizeof(BoolOptions) / sizeof(BoolOptions[0]);
		TOption<double> DoubleOptions[] = {
			{ "floodrate", &CIRCNetwork::SetFloodRate },
		};
		size_t numDoubleOptions = sizeof(DoubleOptions) / sizeof(DoubleOptions[0]);
		TOption<short unsigned int> SUIntOptions[] = {
			{ "floodburst", &CIRCNetwork::SetFloodBurst },
		};
		size_t numSUIntOptions = sizeof(SUIntOptions) / sizeof(SUIntOptions[0]);

		for (size_t i = 0; i < numStringOptions; i++) {
			CString sValue;
			if (pConfig->FindStringEntry(StringOptions[i].name, sValue))
				(this->*StringOptions[i].pSetter)(sValue);
		}

		for (size_t i = 0; i < numBoolOptions; i++) {
			CString sValue;
			if (pConfig->FindStringEntry(BoolOptions[i].name, sValue))
				(this->*BoolOptions[i].pSetter)(sValue.ToBool());
		}

		for (size_t i = 0; i < numDoubleOptions; ++i) {
			double fValue;
			if (pConfig->FindDoubleEntry(DoubleOptions[i].name, fValue))
				(this->*DoubleOptions[i].pSetter)(fValue);
		}

		for (size_t i = 0; i < numSUIntOptions; ++i) {
			unsigned short value;
			if (pConfig->FindUShortEntry(SUIntOptions[i].name, value))
				(this->*SUIntOptions[i].pSetter)(value);
		}

		pConfig->FindStringVector("loadmodule", vsList);
		for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
			CString sValue = *vit;
			CString sModName = sValue.Token(0);

			// XXX Legacy crap, added in ZNC 0.203, modified in 0.207
			// Note that 0.203 == 0.207
			if (sModName == "away") {
				CUtils::PrintMessage("NOTICE: [away] was renamed, "
						"loading [awaystore] instead");
				sModName = "awaystore";
			}

			// XXX Legacy crap, added in ZNC 0.207
			if (sModName == "autoaway") {
				CUtils::PrintMessage("NOTICE: [autoaway] was renamed, "
						"loading [awaystore] instead");
				sModName = "awaystore";
			}
		
			// XXX Legacy crap, added in 1.1; fakeonline module was dropped in 1.0 and returned in 1.1
			if (sModName == "fakeonline") {
				CUtils::PrintMessage("NOTICE: [fakeonline] was renamed, loading [modules_online] instead");
				sModName = "modules_online";
			}

			CUtils::PrintAction("Loading network module [" + sModName + "]");
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
		CUtils::PrintAction("Adding server [" + *vit + "]");
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
	if (!m_sBindHost.empty()) {
		config.AddKeyValuePair("BindHost", m_sBindHost);
	}

	config.AddKeyValuePair("IRCConnectEnabled", CString(GetIRCConnectEnabled()));
	config.AddKeyValuePair("FloodRate", CString(GetFloodRate()));
	config.AddKeyValuePair("FloodBurst", CString(GetFloodBurst()));

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

bool CIRCNetwork::IsUserOnline() const {
	vector<CClient*>::const_iterator it;
	for (it = m_vClients.begin(); it != m_vClients.end(); ++it) {
		CClient *pClient = *it;
		if (!pClient->IsAway()) {
			return true;
		}
	}

	return false;
}

void CIRCNetwork::ClientConnected(CClient *pClient) {
	if (!m_pUser->MultiClients()) {
		BounceAllClients();
	}

	m_vClients.push_back(pClient);

	size_t uIdx, uSize;

	if (m_RawBuffer.IsEmpty()) {
		pClient->PutClient(":irc.znc.in 001 " + pClient->GetNick() + " :- Welcome to ZNC -");
	} else {
		const CString& sClientNick = pClient->GetNick(false);
		MCString msParams;
		msParams["target"] = sClientNick;

		uSize = m_RawBuffer.Size();
		for (uIdx = 0; uIdx < uSize; uIdx++) {
			pClient->PutClient(m_RawBuffer.GetLine(uIdx, *pClient, msParams));
		}

		const CNick& Nick = GetIRCNick();
		if (sClientNick != Nick.GetNick()) { // case-sensitive match
			pClient->PutClient(":" + sClientNick + "!" + Nick.GetIdent() +
					"@" + Nick.GetHost() + " NICK :" + Nick.GetNick());
			pClient->SetNick(Nick.GetNick());
		}
	}

	MCString msParams;
	msParams["target"] = GetIRCNick().GetNick();

	// Send the cached MOTD
	uSize = m_MotdBuffer.Size();
	if (uSize > 0) {
		for (uIdx = 0; uIdx < uSize; uIdx++) {
			pClient->PutClient(m_MotdBuffer.GetLine(uIdx, *pClient, msParams));
		}
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
	for (size_t a = 0; a < vChans.size(); a++) {
		if ((vChans[a]->IsOn()) && (!vChans[a]->IsDetached())) {
			vChans[a]->JoinUser(true, "", pClient);
		}
	}

	uSize = m_QueryBuffer.Size();
	for (uIdx = 0; uIdx < uSize; uIdx++) {
		CString sLine = m_QueryBuffer.GetLine(uIdx, *pClient, msParams);
		bool bContinue = false;
		NETWORKMODULECALL(OnPrivBufferPlayLine(*pClient, sLine), m_pUser, this, NULL, &bContinue);
		if (bContinue) continue;
		pClient->PutClient(sLine);
	}
	m_QueryBuffer.Clear();

	// Tell them why they won't connect
	if (!GetIRCConnectEnabled())
		pClient->PutStatus("You are currently disconnected from IRC. "
				"Use 'connect' to reconnect.");
}

void CIRCNetwork::ClientDisconnected(CClient *pClient) {
	for (size_t a = 0; a < m_vClients.size(); a++) {
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

CChan* CIRCNetwork::FindChan(CString sName) const {
	if (GetIRCSock()) {
		// See https://tools.ietf.org/html/draft-brocklesby-irc-isupport-03#section-3.16
		sName.TrimLeft(GetIRCSock()->GetISupport("STATUSMSG", ""));
	}

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
		bool bFailed = false;
		NETWORKMODULECALL(OnTimerAutoJoin(*pChan), m_pUser, this, NULL, &bFailed);
		if (bFailed) return false;
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

	for (vector<CServer*>::iterator it = m_vServers.begin(); it != m_vServers.end(); ++it, a++) {
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
	size_t uIdx = (m_uServerIdx) ? m_uServerIdx -1 : 0;

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
	if (!GetIRCConnectEnabled() || m_pIRCSock || !HasServers())
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

	bool bAbort = false;
	NETWORKMODULECALL(OnIRCConnecting(pIRCSock), m_pUser, this, NULL, &bAbort);
	if (bAbort) {
		DEBUG("Some module aborted the connection attempt");
		PutStatus("Some module aborted the connection attempt");
		delete pIRCSock;
		CZNC::Get().AddNetworkToQueue(this);
		return false;
	}

	CString sSockName = "IRC::" + m_pUser->GetUserName() + "::" + m_sName;
	CZNC::Get().GetManager().Connect(pServer->GetName(), pServer->GetPort(), sSockName, 120, bSSL, GetBindHost(), pIRCSock);

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

void CIRCNetwork::SetIRCConnectEnabled(bool b) {
	m_bIRCConnectEnabled = b;

	if (m_bIRCConnectEnabled) {
		CheckIRCConnect();
	} else if (GetIRCSock()) {
		if (GetIRCSock()->IsConnected()) {
			GetIRCSock()->Quit();
		} else {
			GetIRCSock()->Close();
		}
	}
}

void CIRCNetwork::CheckIRCConnect() {
	// Do we want to connect?
	if (GetIRCConnectEnabled() && GetIRCSock() == NULL)
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

const CString& CIRCNetwork::GetBindHost() const {
	if (m_sBindHost.empty()) {
		return m_pUser->GetBindHost();
	}

	return m_sBindHost;
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

void CIRCNetwork::SetBindHost(const CString& s) {
	if (m_pUser->GetBindHost().Equals(s)) {
		m_sBindHost = "";
	} else {
		m_sBindHost = s;
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
	sRet.Replace("%bindhost%", GetBindHost());

	return m_pUser->ExpandString(sRet, sRet);
}
