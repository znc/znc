/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/User.h>
#include <znc/Chan.h>
#include <znc/Config.h>
#include <znc/FileUtils.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Server.h>
#include <znc/znc.h>
#include <znc/Modules.h>

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
		vector<CIRCNetwork*> vNetworks = m_pUser->GetNetworks();

		for (size_t a = 0; a < vNetworks.size(); a++) {
			CIRCNetwork* pNetwork = vNetworks[a];
			CIRCSock* pIRCSock = pNetwork->GetIRCSock();

			if (pIRCSock && pIRCSock->GetTimeSinceLastDataTransaction() >= 270) {
				pIRCSock->PutIRC("PING :ZNC");
			}

			if (pNetwork->IsIRCConnected()) {
				pNetwork->JoinChans();
			}

			vector<CClient*>& vClients = pNetwork->GetClients();
			for (size_t b = 0; b < vClients.size(); b++) {
				CClient* pClient = vClients[b];

				if (pClient->GetTimeSinceLastDataTransaction() >= 270) {
					pClient->PutClient("PING :ZNC");
				}
			}
		}
	}

	CUser* m_pUser;
};

CUser::CUser(const CString& sUserName)
		: m_sUserName(sUserName), m_sCleanUserName(MakeCleanUserName(sUserName))
{
	// set path that depends on the user name:
	m_sUserPath = CZNC::Get().GetUserPath() + "/" + m_sUserName;

	m_fTimezoneOffset = 0;
	m_sNick = m_sCleanUserName;
	m_sIdent = m_sCleanUserName;
	m_sRealName = sUserName;
	m_uBytesRead = 0;
	m_uBytesWritten = 0;
	m_pModules = new CModules;
	m_bMultiClients = true;
	m_eHashType = HASH_NONE;
	m_bDenyLoadMod = false;
	m_bAdmin= false;
	m_bDenySetBindHost= false;
	m_sStatusPrefix = "*";
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
	// Delete networks
	for (unsigned int c = 0; c < m_vIRCNetworks.size(); c++) {
		CIRCNetwork* pNetwork = m_vIRCNetworks[c];
		delete pNetwork;
	}
	m_vIRCNetworks.clear();

	// Delete clients
	for (unsigned int c = 0; c < m_vClients.size(); c++) {
		CClient* pClient = m_vClients[c];
		CZNC::Get().GetManager().DelSockByAddr(pClient);
	}
	m_vClients.clear();

	// Delete modules (unloads all modules!)
	delete m_pModules;
	m_pModules = NULL;

	CZNC::Get().GetManager().DelCronByAddr(m_pUserTimer);

	CZNC::Get().AddBytesRead(BytesRead());
	CZNC::Get().AddBytesWritten(BytesWritten());
}

template<class T>
struct TOption {
	const char *name;
	void (CUser::*pSetter)(T);
};

bool CUser::ParseConfig(CConfig* pConfig, CString& sError) {
	TOption<const CString&> StringOptions[] = {
		{ "nick", &CUser::SetNick },
		{ "quitmsg", &CUser::SetQuitMsg },
		{ "altnick", &CUser::SetAltNick },
		{ "ident", &CUser::SetIdent },
		{ "realname", &CUser::SetRealName },
		{ "chanmodes", &CUser::SetDefaultChanModes },
		{ "bindhost", &CUser::SetBindHost },
		{ "vhost", &CUser::SetBindHost },
		{ "dccbindhost", &CUser::SetDCCBindHost },
		{ "dccvhost", &CUser::SetDCCBindHost },
		{ "timestampformat", &CUser::SetTimestampFormat },
		{ "skin", &CUser::SetSkinName },
	};
	size_t numStringOptions = sizeof(StringOptions) / sizeof(StringOptions[0]);
	TOption<unsigned int> UIntOptions[] = {
		{ "jointries", &CUser::SetJoinTries },
		{ "maxjoins", &CUser::SetMaxJoins },
	};
	size_t numUIntOptions = sizeof(UIntOptions) / sizeof(UIntOptions[0]);
	TOption<bool> BoolOptions[] = {
		{ "keepbuffer", &CUser::SetKeepBuffer },
		{ "multiclients", &CUser::SetMultiClients },
		{ "denyloadmod", &CUser::SetDenyLoadMod },
		{ "admin", &CUser::SetAdmin },
		{ "denysetbindhost", &CUser::SetDenySetBindHost },
		{ "denysetvhost", &CUser::SetDenySetBindHost },
		{ "appendtimestamp", &CUser::SetTimestampAppend },
		{ "prependtimestamp", &CUser::SetTimestampPrepend },
		{ "ircconnectenabled", &CUser::SetIRCConnectEnabled },
	};
	size_t numBoolOptions = sizeof(BoolOptions) / sizeof(BoolOptions[0]);

	for (size_t i = 0; i < numStringOptions; i++) {
		CString sValue;
		if (pConfig->FindStringEntry(StringOptions[i].name, sValue))
			(this->*StringOptions[i].pSetter)(sValue);
	}
	for (size_t i = 0; i < numUIntOptions; i++) {
		CString sValue;
		if (pConfig->FindStringEntry(UIntOptions[i].name, sValue))
			(this->*UIntOptions[i].pSetter)(sValue.ToUInt());
	}
	for (size_t i = 0; i < numBoolOptions; i++) {
		CString sValue;
		if (pConfig->FindStringEntry(BoolOptions[i].name, sValue))
			(this->*BoolOptions[i].pSetter)(sValue.ToBool());
	}

	VCString vsList;
	VCString::const_iterator vit;
	pConfig->FindStringVector("allow", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		AddAllowedHost(*vit);
	}
	pConfig->FindStringVector("ctcpreply", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		const CString& sValue = *vit;
		AddCTCPReply(sValue.Token(0), sValue.Token(1, true));
	}

	CString sValue;

	CString sDCCLookupValue;
	pConfig->FindStringEntry("dcclookupmethod", sDCCLookupValue);
	if (pConfig->FindStringEntry("bouncedccs", sValue))  {
		if (sValue.ToBool()) {
			CUtils::PrintAction("Loading Module [bouncedcc]");
			CString sModRet;
			bool bModRet = GetModules().LoadModule("bouncedcc", "", CModInfo::UserModule, this, NULL, sModRet);

			CUtils::PrintStatus(bModRet, sModRet);
			if (!bModRet) {
				sError = sModRet;
				return false;
			}

			if (sDCCLookupValue.Equals("Client")) {
				GetModules().FindModule("bouncedcc")->SetNV("UseClientIP", "1");
			}
		}
	}
	if (pConfig->FindStringEntry("buffer", sValue))
		SetBufferCount(sValue.ToUInt(), true);
	if (pConfig->FindStringEntry("awaysuffix", sValue)) {
		CUtils::PrintMessage("WARNING: AwaySuffix has been depricated, instead try -> LoadModule = awaynick %nick%_" + sValue);
	}
	if (pConfig->FindStringEntry("autocycle", sValue)) {
		if (sValue.Equals("true"))
			CUtils::PrintError("WARNING: AutoCycle has been removed, instead try -> LoadModule = autocycle");
	}
	if (pConfig->FindStringEntry("keepnick", sValue)) {
		if (sValue.Equals("true"))
			CUtils::PrintError("WARNING: KeepNick has been deprecated, instead try -> LoadModule = keepnick");
	}
	if (pConfig->FindStringEntry("statusprefix", sValue)) {
		if (!SetStatusPrefix(sValue)) {
			sError = "Invalid StatusPrefix [" + sValue + "] Must be 1-5 chars, no spaces.";
			CUtils::PrintError(sError);
			return false;
		}
	}
	if (pConfig->FindStringEntry("timezoneoffset", sValue)) {
		SetTimezoneOffset(sValue.ToDouble());
	}
	if (pConfig->FindStringEntry("timestamp", sValue)) {
		if (!sValue.Trim_n().Equals("true")) {
			if (sValue.Trim_n().Equals("append")) {
				SetTimestampAppend(true);
				SetTimestampPrepend(false);
			} else if (sValue.Trim_n().Equals("prepend")) {
				SetTimestampAppend(false);
				SetTimestampPrepend(true);
			} else if (sValue.Trim_n().Equals("false")) {
				SetTimestampAppend(false);
				SetTimestampPrepend(false);
			} else {
				SetTimestampFormat(sValue);
			}
		}
	}
	pConfig->FindStringEntry("pass", sValue);
	// There are different formats for this available:
	// Pass = <plain text>
	// Pass = <md5 hash> -
	// Pass = plain#<plain text>
	// Pass = <hash name>#<hash>
	// Pass = <hash name>#<salted hash>#<salt>#
	// 'Salted hash' means hash of 'password' + 'salt'
	// Possible hashes are md5 and sha256
	if (sValue.Right(1) == "-") {
		sValue.RightChomp();
		sValue.Trim();
		SetPass(sValue, CUser::HASH_MD5);
	} else {
		CString sMethod = sValue.Token(0, false, "#");
		CString sPass = sValue.Token(1, true, "#");
		if (sMethod == "md5" || sMethod == "sha256") {
			CUser::eHashType type = CUser::HASH_MD5;
			if (sMethod == "sha256")
				type = CUser::HASH_SHA256;

			CString sSalt = sPass.Token(1, false, "#");
			sPass = sPass.Token(0, false, "#");
			SetPass(sPass, type, sSalt);
		} else if (sMethod == "plain") {
			SetPass(sPass, CUser::HASH_NONE);
		} else {
			SetPass(sValue, CUser::HASH_NONE);
		}
	}
	CConfig::SubConfig subConf;
	CConfig::SubConfig::const_iterator subIt;
	pConfig->FindSubConfig("pass", subConf);
	if (!sValue.empty() && !subConf.empty()) {
		sError = "Password defined more than once";
		CUtils::PrintError(sError);
		return false;
	}
	subIt = subConf.begin();
	if (subIt != subConf.end()) {
		CConfig* pSubConf = subIt->second.m_pSubConfig;
		CString sHash;
		CString sMethod;
		CString sSalt;
		CUser::eHashType method;
		pSubConf->FindStringEntry("hash", sHash);
		pSubConf->FindStringEntry("method", sMethod);
		pSubConf->FindStringEntry("salt", sSalt);
		if (sMethod.empty() || sMethod.Equals("plain"))
			method = CUser::HASH_NONE;
		else if (sMethod.Equals("md5"))
			method = CUser::HASH_MD5;
		else if (sMethod.Equals("sha256"))
			method = CUser::HASH_SHA256;
		else {
			sError = "Invalid hash method";
			CUtils::PrintError(sError);
			return false;
		}

		SetPass(sHash, method, sSalt);
		if (!pSubConf->empty()) {
			sError = "Unhandled lines in config!";
			CUtils::PrintError(sError);

			CZNC::DumpConfig(pSubConf);
			return false;
		}
		subIt++;
	}
	if (subIt != subConf.end()) {
		sError = "Password defined more than once";
		CUtils::PrintError(sError);
		return false;
	}

	pConfig->FindSubConfig("network", subConf);
	for (subIt = subConf.begin(); subIt != subConf.end(); ++subIt) {
		const CString& sNetworkName = subIt->first;

		CIRCNetwork *pNetwork = FindNetwork(sNetworkName);

		if (!pNetwork) {
			pNetwork = new CIRCNetwork(this, sNetworkName);
		}

		if (!pNetwork->ParseConfig(subIt->second.m_pSubConfig, sError)) {
			return false;
		}
	}

	if (pConfig->FindStringVector("server", vsList, false) || pConfig->FindStringVector("chan", vsList, false) || pConfig->FindSubConfig("chan", subConf, false)) {
		CIRCNetwork *pNetwork = FindNetwork("user");
		if (!pNetwork) {
			pNetwork = AddNetwork("user");
		}

		if (pNetwork) {
			CUtils::PrintMessage("NOTICE: Found deprecated config, upgrading to a network");

			if (!pNetwork->ParseConfig(pConfig, sError, true)) {
				return false;
			}
		}
	}

	pConfig->FindStringVector("loadmodule", vsList);
	for (vit = vsList.begin(); vit != vsList.end(); ++vit) {
		sValue = *vit;
		CString sModName = sValue.Token(0);

		// XXX Legacy crap, added in ZNC 0.089
		if (sModName == "discon_kick") {
			CUtils::PrintMessage("NOTICE: [discon_kick] was renamed, loading [disconkick] instead");
			sModName = "disconkick";
		}

		// XXX Legacy crap, added in ZNC 0.099
		if (sModName == "fixfreenode") {
			CUtils::PrintMessage("NOTICE: [fixfreenode] doesn't do anything useful anymore, ignoring it");
			continue;
		}

		CUtils::PrintAction("Loading Module [" + sModName + "]");
		CString sModRet;
		CString sArgs = sValue.Token(1, true);
		bool bModRet;

		CModInfo ModInfo;
		if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sModName, sModRet)) {
			sError = "Unable to find modinfo [" + sModName + "] [" + sModRet + "]";
			return false;
		}

		if (!ModInfo.SupportsType(CModInfo::UserModule) && ModInfo.SupportsType(CModInfo::NetworkModule)) {
			CUtils::PrintMessage("NOTICE: Module [" + sModName + "] is a network module, loading module for all networks in user.");

			// Do they have old NV?
			CFile fNVFile = CFile(GetUserPath() + "/moddata/" + sModName + "/.registry");

			for (vector<CIRCNetwork*>::iterator it = m_vIRCNetworks.begin(); it != m_vIRCNetworks.end(); ++it) {
				if (fNVFile.Exists()) {
					CString sNetworkModPath = (*it)->GetNetworkPath() + "/moddata/" + sModName;
					if (!CFile::Exists(sNetworkModPath)) {
						CDir::MakeDir(sNetworkModPath);
					}	

					fNVFile.Copy(sNetworkModPath + "/.registry");
				}

				bModRet = (*it)->GetModules().LoadModule(sModName, sArgs, CModInfo::NetworkModule, this, *it, sModRet);
				if (!bModRet) {
					break;
				}
			}
		} else {
			bModRet = GetModules().LoadModule(sModName, sArgs, CModInfo::UserModule, this, NULL, sModRet);
		}

		CUtils::PrintStatus(bModRet, sModRet);
		if (!bModRet) {
			sError = sModRet;
			return false;
		}
		continue;
	}

	return true;
}

CIRCNetwork* CUser::AddNetwork(const CString &sNetwork) {
	if (!CIRCNetwork::IsValidNetwork(sNetwork) || FindNetwork(sNetwork)) {
		return NULL;
	}

	return new CIRCNetwork(this, sNetwork);
}

bool CUser::AddNetwork(CIRCNetwork *pNetwork) {
	if (FindNetwork(pNetwork->GetName())) {
		return false;
	}

	m_vIRCNetworks.push_back(pNetwork);

	return true;
}

void CUser::RemoveNetwork(CIRCNetwork *pNetwork) {
	for (vector<CIRCNetwork*>::iterator it = m_vIRCNetworks.begin(); it != m_vIRCNetworks.end(); ++it) {
		if (pNetwork == *it) {
			m_vIRCNetworks.erase(it);
			return;
		}
	}
}

bool CUser::DeleteNetwork(const CString& sNetwork) {
	CIRCNetwork *pNetwork = FindNetwork(sNetwork);

	if (pNetwork) {
		delete pNetwork;
		return true;
	}

	return false;
}

CIRCNetwork* CUser::FindNetwork(const CString& sNetwork) {
	for (vector<CIRCNetwork*>::iterator it = m_vIRCNetworks.begin(); it != m_vIRCNetworks.end(); ++it) {
		CIRCNetwork *pNetwork = *it;
		if (pNetwork->GetName().Equals(sNetwork)) {
			return pNetwork;
		}
	}

	return NULL;
}

const vector<CIRCNetwork*>& CUser::GetNetworks() const {
	return m_vIRCNetworks;
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
	sRet.Replace("%nick%", GetNick());
	sRet.Replace("%altnick%", GetAltNick());
	sRet.Replace("%ident%", GetIdent());
	sRet.Replace("%realname%", GetRealName());
	sRet.Replace("%vhost%", GetBindHost());
	sRet.Replace("%bindhost%", GetBindHost());
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
	time_t tm;
	return AddTimestamp(time(&tm), sStr);
}

CString CUser::AddTimestamp(time_t tm, const CString& sStr) const {
	char szTimestamp[1024];
	CString sRet = sStr;

	if (!GetTimestampFormat().empty() && (m_bAppendTimestamp || m_bPrependTimestamp)) {
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
			// From http://www.mirc.com/colors.html
			// The Control+O key combination in mIRC inserts ascii character 15,
			// which turns off all previous attributes, including color, bold, underline, and italics.
			sRet += "\x0F ";
			
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

	pClient->PutClient(":irc.znc.in 001 " + pClient->GetNick() + " :- Welcome to ZNC -");

	m_vClients.push_back(pClient);
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

	// user names can only specified for the constructor, changing it later
	// on breaks too much stuff (e.g. lots of paths depend on the user name)
	if (GetUserName() != User.GetUserName()) {
		DEBUG("Ignoring username in CUser::Clone(), old username [" << GetUserName()
				<< "]; New username [" << User.GetUserName() << "]");
	}

	if (!User.GetPass().empty()) {
		SetPass(User.GetPass(), User.GetPassHashType(), User.GetPassSalt());
	}

	SetNick(User.GetNick(false));
	SetAltNick(User.GetAltNick(false));
	SetIdent(User.GetIdent(false));
	SetRealName(User.GetRealName());
	SetStatusPrefix(User.GetStatusPrefix());
	SetBindHost(User.GetBindHost());
	SetDCCBindHost(User.GetDCCBindHost());
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

	// Networks
	const vector<CIRCNetwork*>& vNetworks = User.GetNetworks();
	for (a = 0; a < vNetworks.size(); a++) {
		new CIRCNetwork(this, vNetworks[a], bCloneChans);
	}
	// !Networks

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
	SetDenyLoadMod(User.DenyLoadMod());
	SetAdmin(User.IsAdmin());
	SetDenySetBindHost(User.DenySetBindHost());
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
			vCurMods.LoadModule(pNewMod->GetModName(), pNewMod->GetArgs(), CModInfo::UserModule, this, NULL, sModRet);
		} else if (pNewMod->GetArgs() != pCurMod->GetArgs()) {
			vCurMods.ReloadModule(pNewMod->GetModName(), pNewMod->GetArgs(), this, NULL, sModRet);
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
	// /^[a-zA-Z][a-zA-Z@._\-]*$/
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

CConfig CUser::ToConfig() {
	CConfig config;
	CConfig passConfig;

	CString sHash;
	switch (m_eHashType) {
	case HASH_NONE:
		sHash = "Plain";
		break;
	case HASH_MD5:
		sHash = "MD5";
		break;
	case HASH_SHA256:
		sHash = "SHA256";
		break;
	}
	passConfig.AddKeyValuePair("Salt", m_sPassSalt);
	passConfig.AddKeyValuePair("Method", sHash);
	passConfig.AddKeyValuePair("Hash", GetPass());
	config.AddSubConfig("Pass", "password", passConfig);

	config.AddKeyValuePair("Nick", GetNick());
	config.AddKeyValuePair("AltNick", GetAltNick());
	config.AddKeyValuePair("Ident", GetIdent());
	config.AddKeyValuePair("RealName", GetRealName());
	config.AddKeyValuePair("BindHost", GetBindHost());
	config.AddKeyValuePair("DCCBindHost", GetDCCBindHost());
	config.AddKeyValuePair("QuitMsg", GetQuitMsg());
	if (CZNC::Get().GetStatusPrefix() != GetStatusPrefix())
		config.AddKeyValuePair("StatusPrefix", GetStatusPrefix());
	config.AddKeyValuePair("Skin", GetSkinName());
	config.AddKeyValuePair("ChanModes", GetDefaultChanModes());
	config.AddKeyValuePair("Buffer", CString(GetBufferCount()));
	config.AddKeyValuePair("KeepBuffer", CString(KeepBuffer()));
	config.AddKeyValuePair("MultiClients", CString(MultiClients()));
	config.AddKeyValuePair("DenyLoadMod", CString(DenyLoadMod()));
	config.AddKeyValuePair("Admin", CString(IsAdmin()));
	config.AddKeyValuePair("DenySetBindHost", CString(DenySetBindHost()));
	config.AddKeyValuePair("TimestampFormat", GetTimestampFormat());
	config.AddKeyValuePair("AppendTimestamp", CString(GetTimestampAppend()));
	config.AddKeyValuePair("PrependTimestamp", CString(GetTimestampPrepend()));
	config.AddKeyValuePair("TimezoneOffset", CString(m_fTimezoneOffset));
	config.AddKeyValuePair("JoinTries", CString(m_uMaxJoinTries));
	config.AddKeyValuePair("MaxJoins", CString(m_uMaxJoins));
	config.AddKeyValuePair("IRCConnectEnabled", CString(GetIRCConnectEnabled()));

	// Allow Hosts
	if (!m_ssAllowedHosts.empty()) {
		for (set<CString>::iterator it = m_ssAllowedHosts.begin(); it != m_ssAllowedHosts.end(); ++it) {
			config.AddKeyValuePair("Allow", *it);
		}
	}

	// CTCP Replies
	if (!m_mssCTCPReplies.empty()) {
		for (MCString::const_iterator itb = m_mssCTCPReplies.begin(); itb != m_mssCTCPReplies.end(); ++itb) {
			config.AddKeyValuePair("CTCPReply", itb->first.AsUpper() + " " + itb->second);
		}
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

	// Networks
	for (unsigned int d = 0; d < m_vIRCNetworks.size(); d++) {
		CIRCNetwork *pNetwork = m_vIRCNetworks[d];
		config.AddSubConfig("Network", pNetwork->GetName(), pNetwork->ToConfig());
	}

	return config;
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

CString CUser::GetLocalDCCIP() {
	if (!GetDCCBindHost().empty())
		return GetDCCBindHost();

	for (vector<CIRCNetwork*>::iterator it = m_vIRCNetworks.begin(); it != m_vIRCNetworks.end(); ++it) {
		CIRCNetwork *pNetwork = *it;
		CIRCSock* pIRCSock = pNetwork->GetIRCSock();
		if (pIRCSock) {
			return pIRCSock->GetLocalIP();
		}
	}

	if (!GetAllClients().empty()) {
		return GetAllClients()[0]->GetLocalIP();
	}

	return "";
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

bool CUser::PutAllUser(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	PutUser(sLine, pClient, pSkipClient);

	for (vector<CIRCNetwork*>::iterator it = m_vIRCNetworks.begin(); it != m_vIRCNetworks.end(); ++it) {
		CIRCNetwork* pNetwork = *it;
		if (pNetwork->PutUser(sLine, pClient, pSkipClient)) {
			return true;
		}
	}

	return (pClient == NULL);
}

bool CUser::PutStatus(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	vector<CClient*> vClients = GetAllClients();
	for (unsigned int a = 0; a < vClients.size(); a++) {
		if ((!pClient || pClient == vClients[a]) && pSkipClient != vClients[a]) {
			vClients[a]->PutStatus(sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

bool CUser::PutStatusNotice(const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	vector<CClient*> vClients = GetAllClients();
	for (unsigned int a = 0; a < vClients.size(); a++) {
		if ((!pClient || pClient == vClients[a]) && pSkipClient != vClients[a]) {
			vClients[a]->PutStatusNotice(sLine);

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

bool CUser::PutModNotice(const CString& sModule, const CString& sLine, CClient* pClient, CClient* pSkipClient) {
	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		if ((!pClient || pClient == m_vClients[a]) && pSkipClient != m_vClients[a]) {
			m_vClients[a]->PutModNotice(sModule, sLine);

			if (pClient) {
				return true;
			}
		}
	}

	return (pClient == NULL);
}

CString CUser::MakeCleanUserName(const CString& sUserName) {
	return sUserName.Token(0, false, "@").Replace_n(".", "");
}

bool CUser::IsUserAttached() const {
	if (!m_vClients.empty()) {
		return true;
	}

	for (vector<CIRCNetwork*>::const_iterator i = m_vIRCNetworks.begin(); i != m_vIRCNetworks.end(); ++i) {
		if ((*i)->IsUserAttached()) {
			return true;
		}
	}

	return false;
}

// Setters
void CUser::SetNick(const CString& s) { m_sNick = s; }
void CUser::SetAltNick(const CString& s) { m_sAltNick = s; }
void CUser::SetIdent(const CString& s) { m_sIdent = s; }
void CUser::SetRealName(const CString& s) { m_sRealName = s; }
void CUser::SetBindHost(const CString& s) { m_sBindHost = s; }
void CUser::SetDCCBindHost(const CString& s) { m_sDCCBindHost = s; }
void CUser::SetPass(const CString& s, eHashType eHash, const CString& sSalt) {
	m_sPass = s;
	m_eHashType = eHash;
	m_sPassSalt = sSalt;
}
void CUser::SetMultiClients(bool b) { m_bMultiClients = b; }
void CUser::SetDenyLoadMod(bool b) { m_bDenyLoadMod = b; }
void CUser::SetAdmin(bool b) { m_bAdmin = b; }
void CUser::SetDenySetBindHost(bool b) { m_bDenySetBindHost = b; }
void CUser::SetDefaultChanModes(const CString& s) { m_sDefaultChanModes = s; }
void CUser::SetQuitMsg(const CString& s) { m_sQuitMsg = s; }
void CUser::SetKeepBuffer(bool b) { m_bKeepBuffer = b; }

bool CUser::SetBufferCount(unsigned int u, bool bForce) {
	if (!bForce && u > CZNC::Get().GetMaxBufferSize())
		return false;
	m_uBufferCount = u;
	return true;
}

bool CUser::AddCTCPReply(const CString& sCTCP, const CString& sReply) {
	// Reject CTCP requests containing spaces
	if (sCTCP.find_first_of(' ') != CString::npos) {
		return false;
	}
	// Reject empty CTCP requests
	if (sCTCP.empty()) {
		return false;
	}
	m_mssCTCPReplies[sCTCP.AsUpper()] = sReply;
	return true;
}

bool CUser::DelCTCPReply(const CString& sCTCP) {
	return m_mssCTCPReplies.erase(sCTCP) > 0;
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
vector<CClient*> CUser::GetAllClients() {
	vector<CClient*> vClients;

	for (unsigned int a = 0; a < m_vIRCNetworks.size(); a++) {
		for (unsigned int b = 0; b < m_vIRCNetworks[a]->GetClients().size(); b++) {
			vClients.push_back(m_vIRCNetworks[a]->GetClients()[b]);
		}
	}

	for (unsigned int a = 0; a < m_vClients.size(); a++) {
		vClients.push_back(m_vClients[a]);
	}

	return vClients;
}

const CString& CUser::GetUserName() const { return m_sUserName; }
const CString& CUser::GetCleanUserName() const { return m_sCleanUserName; }
const CString& CUser::GetNick(bool bAllowDefault) const { return (bAllowDefault && m_sNick.empty()) ? GetCleanUserName() : m_sNick; }
const CString& CUser::GetAltNick(bool bAllowDefault) const { return (bAllowDefault && m_sAltNick.empty()) ? GetCleanUserName() : m_sAltNick; }
const CString& CUser::GetIdent(bool bAllowDefault) const { return (bAllowDefault && m_sIdent.empty()) ? GetCleanUserName() : m_sIdent; }
const CString& CUser::GetRealName() const { return m_sRealName.empty() ? m_sUserName : m_sRealName; }
const CString& CUser::GetBindHost() const { return m_sBindHost; }
const CString& CUser::GetDCCBindHost() const { return m_sDCCBindHost; }
const CString& CUser::GetPass() const { return m_sPass; }
CUser::eHashType CUser::GetPassHashType() const { return m_eHashType; }
const CString& CUser::GetPassSalt() const { return m_sPassSalt; }
bool CUser::DenyLoadMod() const { return m_bDenyLoadMod; }
bool CUser::IsAdmin() const { return m_bAdmin; }
bool CUser::DenySetBindHost() const { return m_bDenySetBindHost; }
bool CUser::MultiClients() const { return m_bMultiClients; }
const CString& CUser::GetStatusPrefix() const { return m_sStatusPrefix; }
const CString& CUser::GetDefaultChanModes() const { return m_sDefaultChanModes; }


CString CUser::GetQuitMsg() const { return (!m_sQuitMsg.Trim_n().empty()) ? m_sQuitMsg : CZNC::GetTag(false); }
const MCString& CUser::GetCTCPReplies() const { return m_mssCTCPReplies; }
unsigned int CUser::GetBufferCount() const { return m_uBufferCount; }
bool CUser::KeepBuffer() const { return m_bKeepBuffer; }
//CString CUser::GetSkinName() const { return (!m_sSkinName.empty()) ? m_sSkinName : CZNC::Get().GetSkinName(); }
CString CUser::GetSkinName() const { return m_sSkinName; }
const CString& CUser::GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CDir::MakeDir(m_sUserPath); } return m_sUserPath; }
// !Getters
