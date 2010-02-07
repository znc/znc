/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "HTTPSock.h"
#include "Server.h"
#include "Template.h"
#include "User.h"
#include "znc.h"
#include <sstream>

using std::stringstream;

class CWebAdminMod;
class CWebAdminSock;

class CWebAdminAuth : public CAuthBase {
public:
	CWebAdminAuth(CWebAdminSock* pWebAdminSock, const CString& sUsername, const CString& sPassword);

	virtual ~CWebAdminAuth() {}

	void Invalidate() { m_pWebAdminSock = NULL; CAuthBase::Invalidate(); }
	void AcceptedLogin(CUser& User);
	void RefusedLogin(const CString& sReason);
private:
protected:
	CWebAdminSock*	m_pWebAdminSock;
};


class CWebAdminSock : public CHTTPSock {
public:
	CWebAdminSock(CWebAdminMod* pModule);
	CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CWebAdminSock();

	virtual bool OnPageRequest(const CString& sURI, CString& sPageRet);
	virtual bool OnLogin(const CString& sUser, const CString& sPass);

	CString GetAvailSkinsDir();
	CString GetSkinDir();
	void PrintPage(CString& sPageRet, const CString& sTmplName);

	void GetErrorPage(CString& sPageRet, const CString& sError) {
		m_Template["Action"] = "error";
		m_Template["Title"] = "Error";
		m_Template["Error"] = sError;

		PrintPage(sPageRet, "Error.tmpl");
	}

	void ListUsersPage(CString& sPageRet);
	bool SettingsPage(CString& sPageRet);
	bool ChanPage(CString& sPageRet, CChan* = NULL);
	bool DelChan(CString& sPageRet);
	bool UserPage(CString& sPageRet, CUser* pUser = NULL);
	CUser* GetNewUser(CString& sPageRet, CUser* pUser);

	CString GetModArgs(const CString& sModName, bool bGlobal = false) {
		if (!bGlobal && !m_pUser) {
			return "";
		}

		CModules& Modules = (bGlobal) ? CZNC::Get().GetModules() : m_pUser->GetModules();

		for (unsigned int a = 0; a < Modules.size(); a++) {
			CModule* pModule = Modules[a];

			if (pModule->GetModName() == sModName) {
				return pModule->GetArgs();
			}
		}

		return "";
	}

	// Setters
	void SetSessionUser(CUser* p) {
		m_pSessionUser = p;
		m_bAdmin = p->IsAdmin();

		// If m_pUser is not NULL, only that user can be edited.
		if (m_bAdmin) {
			m_pUser = NULL;
		} else {
			m_pUser = m_pSessionUser;
		}
	}
	// !Setters

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	bool IsAdmin() const { return m_bAdmin; }

	CWebAdminMod* GetModule() const { return (CWebAdminMod*) m_pModule; }

private:
protected:
	CUser*					m_pUser;
	CUser*					m_pSessionUser;
	bool					m_bAdmin;
	CTemplate				m_Template;
	CSmartPtr<CAuthBase>	m_spAuth;
};

class CWebAdminMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CWebAdminMod) {
		m_sSkinName = GetNV("SkinName");
	}

	virtual ~CWebAdminMod() {
	}

	virtual bool OnLoad(const CString& sArgStr, CString& sMessage) {
		bool bSSL = false;
		bool bIPv6 = false;
		unsigned short uPort = 8080;
		CString sArgs(sArgStr);
		CString sPort;
		CString sListenHost;

		m_bShareIRCPorts = true;

		while (sArgs.Left(1) == "-") {
			CString sOpt = sArgs.Token(0);
			sArgs = sArgs.Token(1, true);

			if (sOpt.Equals("-IPV6")) {
				bIPv6 = true;
			} else if (sOpt.Equals("-IPV4")) {
				bIPv6 = false;
			} else if (sOpt.Equals("-noircport")) {
				m_bShareIRCPorts = false;
			} else {
				sMessage = "Unknown option [" + sOpt + "] valid options are -ipv4, -ipv6 or -noircport";
				return false;
			}
		}

		// No arguments left: Only port sharing
		if (sArgs.empty() && m_bShareIRCPorts)
			return true;

		if (sArgs.find(" ") != CString::npos) {
			sListenHost = sArgs.Token(0);
			sPort = sArgs.Token(1, true);
		} else {
			sPort = sArgs;
		}

		if (sPort.Left(1) == "+") {
#ifdef HAVE_LIBSSL
			sPort.TrimLeft("+");
			bSSL = true;
#else
			return false;
#endif
		}

		if (!sPort.empty()) {
			uPort = sPort.ToUShort();
		}

		return OpenListener(sMessage, uPort, sListenHost, bSSL, bIPv6);
	}

	bool OpenListener(CString &sMessage, u_short uPort, const CString& sListenHost,
			bool bSSL = false, bool bIPv6 = false) {
		CWebAdminSock* pListenSock = new CWebAdminSock(this);
#ifdef HAVE_LIBSSL
		if (bSSL) {
			pListenSock->SetPemLocation(CZNC::Get().GetPemLocation());
		}
#endif

		errno = 0;
		bool b = m_pManager->ListenHost(uPort, "WebAdmin::Listener", sListenHost, bSSL, SOMAXCONN, pListenSock, 0, bIPv6);
		if (!b) {
			sMessage = "Could not bind to port " + CString(uPort);
			if (!sListenHost.empty())
				sMessage += " on vhost [" + sListenHost + "]";
			if (errno != 0)
				sMessage += ": " + CString(strerror(errno));
		}
		return b;
	}

	void SetSkinName(const CString& s) {
		m_sSkinName = s;
		SetNV("SkinName", m_sSkinName);
	}

	CString GetSkinName() const { return (m_sSkinName.empty()) ? CString("default") : m_sSkinName; }
	unsigned int GetSwitchCounter(const CString& sToken) {
		map<CString, unsigned int>::iterator it = m_suSwitchCounters.find(sToken);

		if (it == m_suSwitchCounters.end()) {
			m_suSwitchCounters[sToken] = 1;
			return 1;
		}

		return it->second;
	}

	unsigned int IncSwitchCounter(const CString& sToken) {
		map<CString, unsigned int>::iterator it = m_suSwitchCounters.find(sToken);

		if (it == m_suSwitchCounters.end()) {
			m_suSwitchCounters[sToken] = 2;
			return 2;
		}

		return ++it->second;
	}

	virtual EModRet OnUnknownUserRaw(CClient* pClient, CString& sLine) {
		if (!m_bShareIRCPorts)
			return CONTINUE;

		// If this is a HTTP request, we should handle it
		if (sLine.WildCmp("GET * HTTP/1.?")
				|| sLine.WildCmp("POST * HTTP/1.?")) {
			CWebAdminSock* pSock = new CWebAdminSock(this);
			CZNC::Get().GetManager().SwapSockByAddr(pSock, pClient);

			// And don't forget to give it some sane settings again
			pSock->SetSockName("WebAdmin::Client");
			pSock->SetTimeout(120);
			pSock->SetMaxBufferThreshold(10240);

			// TODO can we somehow get rid of this?
			pSock->ReadLine(sLine);
			pSock->PushBuff("", 0, true);

			return HALT;
		}
		return CONTINUE;
	}

private:
	CString				m_sSkinName;
	bool				m_bShareIRCPorts;
	map<CString, unsigned int>	m_suSwitchCounters;
};

CString CWebAdminSock::GetAvailSkinsDir() {
	return m_pModule->GetModDataDir() + "/skins/";
}

CString CWebAdminSock::GetSkinDir() {
	CString sAvailSkins = GetAvailSkinsDir();
	CString sSkinDir = GetModule()->GetSkinName() + "/";
	CString sDir = CDir::CheckPathPrefix(sAvailSkins, sSkinDir, "/");

	// Via CheckPrefix() we check if someone tries to use e.g. a skin name
	// with embed .. or such evilness.
	if (!sDir.empty() && CFile::IsDir(sDir)) {
		return sDir + "/";
	}

	return m_pModule->GetModDataDir() + "/skins/default/";
}

void CWebAdminSock::PrintPage(CString& sPageRet, const CString& sTmplName) {
	sPageRet.clear();
	CString sTmpl;

	if (IsAdmin()) {
		sTmpl = GetSkinDir();
	}

	sTmpl += sTmplName;

	if (!m_Template.SetFile(GetSkinDir() + sTmplName)) {
		sPageRet = CHTTPSock::GetErrorPage(404, "Not Found", "The template for this page [" + sTmpl + "] (or a template that it includes) was not found.");
		return;
	}

	stringstream oStr;
	if (!m_Template.Print(oStr)) {
		sPageRet = CHTTPSock::GetErrorPage(403, "Forbidden", "The template for this page [" + sTmpl + "] (or a template that it includes) can not be opened.");
		return;
	}

	sPageRet = oStr.str();
}

bool CWebAdminSock::OnLogin(const CString& sUser, const CString& sPass) {
	m_spAuth = new CWebAdminAuth(this, sUser, sPass);

	// Some authentication module could need some time, block this socket
	// until then. CWebAdminAuth will UnPauseRead().
	PauseRead();
	CZNC::Get().AuthUser(m_spAuth);

	// If CWebAdminAuth already set this, don't change it.
	return IsLoggedIn();
}

void CWebAdminSock::ListUsersPage(CString& sPageRet) {
	const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
	m_Template["Title"] = "List Users";
	m_Template["Action"] = "listusers";

	unsigned int a = 0;

	for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++, a++) {
		CServer* pServer = it->second->GetCurrentServer();
		CTemplate& l = m_Template.AddRow("UserLoop");
		CUser& User = *it->second;

		l["Username"] = User.GetUserName();
		l["Clients"] = CString(User.GetClients().size());
		l["IRCNick"] = User.GetIRCNick().GetNick();

		if (&User == m_pSessionUser) {
			l["IsSelf"] = "true";
		}

		if (pServer) {
			l["Server"] = pServer->GetName();
		}
	}

	PrintPage(sPageRet, "ListUsers.tmpl");
}

Csock* CWebAdminSock::GetSockObj(const CString& sHost, unsigned short uPort) {
	CWebAdminSock* pSock = new CWebAdminSock(GetModule(), sHost, uPort);
	pSock->SetSockName("WebAdmin::Client");
	pSock->SetTimeout(120);

	return pSock;
}

CWebAdminSock::CWebAdminSock(CWebAdminMod* pModule) : CHTTPSock(pModule) {
	m_pModule = pModule;
	m_pUser = NULL;
	m_pSessionUser = NULL;
	m_bAdmin = false;
	SetDocRoot(GetSkinDir());
}

CWebAdminSock::CWebAdminSock(CWebAdminMod* pModule, const CString& sHostname, unsigned short uPort, int iTimeout)
		: CHTTPSock(pModule, sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_pUser = NULL;
	m_pSessionUser = NULL;
	m_bAdmin = false;
	SetDocRoot(GetSkinDir());
	m_Template.AppendPath("./");
}

CWebAdminSock::~CWebAdminSock() {
	if (!m_spAuth.IsNull()) {
		CWebAdminAuth* pAuth = (CWebAdminAuth*) &(*m_spAuth);
		pAuth->Invalidate();
	}
}

bool CWebAdminSock::OnPageRequest(const CString& sURI, CString& sPageRet) {
	if (!ForceLogin()) {
		return false;
	}

	const CString& sSkin = GetModule()->GetSkinName();
	CString sTmp = sURI;

	m_Template["SessionUser"] = GetUser();
	m_Template["SessionIP"] = GetRemoteIP();
	m_Template["Tag"] = CZNC::GetTag();
	m_Template["Skin"] = sSkin;

	if (IsAdmin()) {
		m_Template["IsAdmin"] = "true";
	}

	if (sURI == "/") {
		if (!IsAdmin()) {
			Redirect("/edituser");
			return false;
		}

		m_Template["Title"] = "Main Page";
		m_Template["Action"] = "home";
		PrintPage(sPageRet, "Main.tmpl");
	} else if (sTmp.TrimPrefix("/" + sSkin + "/")) {
		SetDocRoot(GetSkinDir() + "/data");
		PrintFile(sTmp);
		return false;
	} else if (sURI == "/favicon.ico") {
		PrintFile("/data/favicon.ico", "image/x-icon");
		return false;
	} else if (sURI == "/home") {
		m_Template["Title"] = "Main Page";
		m_Template["Action"] = "home";
		PrintPage(sPageRet, "Main.tmpl");
	} else if (sURI == "/settings") {
		if (!IsAdmin()) {
			return false;
		}

		if (!SettingsPage(sPageRet)) {
			return false;
		}
	} else if (sURI == "/switchuser") {
		unsigned int uCurCnt = GetParam("cnt").ToUInt();
		unsigned int uCounter = GetModule()->GetSwitchCounter(GetRemoteIP());

		if (!uCurCnt) {
			Redirect("/switchuser?cnt=" + CString(uCounter));
			return false;
		}

		if (uCurCnt >= uCounter) {
			m_bLoggedIn = false;
			GetModule()->IncSwitchCounter(GetRemoteIP());
			ForceLogin();
		} else {
			Redirect("/home");
		}

		return false;
	} else if (sURI == "/adduser") {
		if (!IsAdmin()) {
			return false;
		}

		if (!UserPage(sPageRet)) {
			return false;
		}
	} else if (sURI == "/edituser") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!UserPage(sPageRet, m_pUser)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/editchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (!m_pUser) {
			GetErrorPage(sPageRet, "No such username");
			return true;
		}

		CChan* pChan = m_pUser->FindChan(GetParam("name"));
		if (!pChan) {
			GetErrorPage(sPageRet, "No such channel");
			return true;
		}

		if (!ChanPage(sPageRet, pChan)) {
			return false;
		}
	} else if (sURI == "/addchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!ChanPage(sPageRet)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/delchan") {
		if (!m_pUser) {
			m_pUser = CZNC::Get().FindUser(GetParam("user"));
		}

		if (m_pUser) {
			if (!DelChan(sPageRet)) {
				return false;
			}
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else if (sURI == "/listusers") {
		if (!IsAdmin()) {
			return false;
		}

		ListUsersPage(sPageRet);
	} else if (sURI == "/deluser") {
		if (!IsAdmin()) {
			return false;
		}

		CString sUser = GetParam("user");
		CUser* pUser = CZNC::Get().FindUser(sUser);

		if (pUser && pUser == m_pSessionUser) {
			GetErrorPage(sPageRet, "You are not allowed to delete yourself");
		} else if (CZNC::Get().DeleteUser(sUser)) {
			Redirect("/listusers");
			return false;
		} else {
			GetErrorPage(sPageRet, "No such username");
		}
	} else {
		return false;
	}

	return true;
}

bool CWebAdminSock::SettingsPage(CString& sPageRet) {
	if (!GetParam("submitted").ToUInt()) {
		CString sVHosts, sMotd;
		m_Template["Action"] = "settings";
		m_Template["Title"] = "Settings";
		m_Template["StatusPrefix"] = CZNC::Get().GetStatusPrefix();
		m_Template["ISpoofFile"] = CZNC::Get().GetISpoofFile();
		m_Template["ISpoofFormat"] = CZNC::Get().GetISpoofFormat();

		const VCString& vsVHosts = CZNC::Get().GetVHosts();
		for (unsigned int a = 0; a < vsVHosts.size(); a++) {
			CTemplate& l = m_Template.AddRow("VHostLoop");
			l["VHost"] = vsVHosts[a];
		}

		const VCString& vsMotd = CZNC::Get().GetMotd();
		for (unsigned int b = 0; b < vsMotd.size(); b++) {
			CTemplate& l = m_Template.AddRow("MOTDLoop");
			l["Line"] = vsMotd[b];
		}

		const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();
		for (unsigned int c = 0; c < vpListeners.size(); c++) {
			CListener* pListener = vpListeners[c];
			CTemplate& l = m_Template.AddRow("ListenLoop");

			l["Port"] = CString(pListener->GetPort());
			l["BindHost"] = pListener->GetBindHost();

#ifdef HAVE_LIBSSL
			if (pListener->IsSSL()) {
				l["IsSSL"] = "true";
			}
#endif

#ifdef HAVE_IPV6
			if (pListener->IsIPV6()) {
				l["IsIPV6"] = "true";
			}
#endif
		}

		CString sDir(GetAvailSkinsDir());

		if (CFile::IsDir(sDir)) {
			CDir Dir(sDir);

			m_Template.AddRow("SkinLoop")["Name"] = "default";

			for (unsigned int d = 0; d < Dir.size(); d++) {
				const CFile& SubDir = *Dir[d];

				if (SubDir.IsDir() &&
						SubDir.GetShortName() != ".svn" &&
						SubDir.GetShortName() != "default") {
					CTemplate& l = m_Template.AddRow("SkinLoop");
					l["Name"] = SubDir.GetShortName();

					if (SubDir.GetShortName() == GetModule()->GetSkinName()) {
						l["Checked"] = "true";
					}
				}
			}
		}

		set<CModInfo> ssGlobalMods;
		CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, true);

		for (set<CModInfo>::iterator it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
			const CModInfo& Info = *it;
			CTemplate& l = m_Template.AddRow("ModuleLoop");

			if (CZNC::Get().GetModules().FindModule(Info.GetName())) {
				l["Checked"] = "true";
			}

			if (Info.GetName() == m_pModule->GetModName()) {
				l["Disabled"] = "true";
			}

			l["Name"] = Info.GetName();
			l["Description"] = Info.GetDescription();
			l["Args"] = GetModArgs(Info.GetName(), true);
		}

		PrintPage(sPageRet, "Settings.tmpl");
		return true;
	}

	CString sArg;
	sArg = GetParam("statusprefix"); CZNC::Get().SetStatusPrefix(sArg);
	sArg = GetParam("ispooffile"); CZNC::Get().SetISpoofFile(sArg);
	sArg = GetParam("ispoofformat"); CZNC::Get().SetISpoofFormat(sArg);
	//sArg = GetParam(""); if (!sArg.empty()) { CZNC::Get().Set(sArg); }

	VCString vsArgs;
	GetRawParam("motd").Split("\n", vsArgs);
	CZNC::Get().ClearMotd();

	unsigned int a = 0;
	for (a = 0; a < vsArgs.size(); a++) {
		CZNC::Get().AddMotd(vsArgs[a].TrimRight_n());
	}

	GetRawParam("vhosts").Split("\n", vsArgs);
	CZNC::Get().ClearVHosts();

	for (a = 0; a < vsArgs.size(); a++) {
		CZNC::Get().AddVHost(vsArgs[a].Trim_n());
	}

	GetModule()->SetSkinName(GetParam("skin"));

	set<CString> ssArgs;
	GetParamValues("loadmod", ssArgs);

	for (set<CString>::iterator it = ssArgs.begin(); it != ssArgs.end(); it++) {
		CString sModRet;
		CString sModName = (*it).TrimRight_n("\r");

		if (!sModName.empty()) {
			CString sArgs = GetParam("modargs_" + sModName);

			CModule *pMod = CZNC::Get().GetModules().FindModule(sModName);
			if (!pMod) {
				if (!CZNC::Get().GetModules().LoadModule(sModName, sArgs, NULL, sModRet)) {
					DEBUG("Unable to load module [" << sModName << "] [" << sModRet << "]");
				}
			} else if (pMod->GetArgs() != sArgs) {
				if (!CZNC::Get().GetModules().ReloadModule(sModName, sArgs, NULL, sModRet)) {
					DEBUG("Unable to reload module [" << sModName << "] [" << sModRet << "]");
				}
			} else {
				DEBUG("Unable to load module [" << sModName << "] because it is already loaded");
			}
		}
	}

	const CModules& vCurMods = CZNC::Get().GetModules();
	set<CString> ssUnloadMods;

	for (a = 0; a < vCurMods.size(); a++) {
		CModule* pCurMod = vCurMods[a];

		if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() && pCurMod->GetModName() != m_pModule->GetModName()) {
			ssUnloadMods.insert(pCurMod->GetModName());
		}
	}

	for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); it2++) {
		CZNC::Get().GetModules().UnloadModule(*it2);
	}

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Settings changed, but config was not written");
		return true;
	}

	Redirect("/settings");
	return false;
}

bool CWebAdminSock::ChanPage(CString& sPageRet, CChan* pChan) {
	if (!m_pUser) {
		GetErrorPage(sPageRet, "That user doesn't exist");
		return true;
	}

	if (!GetParam("submitted").ToUInt()) {
		m_Template["User"] = m_pUser->GetUserName();

		if (pChan) {
			m_Template["Action"] = "editchan";
			m_Template["Edit"] = "true";
			m_Template["Title"] = "Edit Channel" + CString(" [" + pChan->GetName() + "]");
			m_Template["ChanName"] = pChan->GetName();
			m_Template["BufferCount"] = CString(pChan->GetBufferCount());
			m_Template["DefModes"] = pChan->GetDefaultModes();

			if (pChan->InConfig()) {
				m_Template["InConfig"] = "true";
			}
		} else {
			m_Template["Action"] = "addchan";
			m_Template["Title"] = "Add Channel" + CString(" for User [" + m_pUser->GetUserName() + "]");
			m_Template["BufferCount"] = CString(m_pUser->GetBufferCount());
			m_Template["DefModes"] = CString(m_pUser->GetDefaultChanModes());
			m_Template["InConfig"] = "true";
		}

		/* o1 used to be AutoCycle which was removed */

		CTemplate& o2 = m_Template.AddRow("OptionLoop");
		o2["Name"] = "keepbuffer";
		o2["DisplayName"] = "Keep Buffer";
		if ((pChan && pChan->KeepBuffer()) || (!pChan && m_pUser->KeepBuffer())) { o2["Checked"] = "true"; }

		CTemplate& o3 = m_Template.AddRow("OptionLoop");
		o3["Name"] = "detached";
		o3["DisplayName"] = "Detached";
		if (pChan && pChan->IsDetached()) { o3["Checked"] = "true"; }

		PrintPage(sPageRet, "Channel.tmpl");
		return true;
	}

	CString sChanName = GetParam("name").Trim_n();

	if (!pChan) {
		if (sChanName.empty()) {
			GetErrorPage(sPageRet, "Channel name is a required argument");
			return true;
		}

		if (m_pUser->FindChan(sChanName.Token(0))) {
			GetErrorPage(sPageRet, "Channel [" + sChanName.Token(0) + "] already exists");
			return true;
		}

		pChan = new CChan(sChanName, m_pUser, true);
		m_pUser->AddChan(pChan);
	}

	pChan->SetDefaultModes(GetParam("defmodes"));
	pChan->SetBufferCount(GetParam("buffercount").ToUInt());
	pChan->SetInConfig(GetParam("save").ToBool());
	pChan->SetKeepBuffer(GetParam("keepbuffer").ToBool());

	bool bDetached = GetParam("detached").ToBool();

	if (pChan->IsDetached() != bDetached) {
		if (bDetached) {
			pChan->DetachUser();
		} else {
			pChan->AttachUser();
		}
	}

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Channel added/modified, but config was not written");
		return true;
	}

	Redirect("/edituser?user=" + m_pUser->GetUserName().Escape_n(CString::EURL));
	return false;
}

bool CWebAdminSock::DelChan(CString& sPageRet) {
	CString sChan = GetParam("name");

	if (!m_pUser) {
		GetErrorPage(sPageRet, "That user doesn't exist");
		return true;
	}

	if (sChan.empty()) {
		GetErrorPage(sPageRet, "That channel doesn't exist for this user");
		return true;
	}

	m_pUser->DelChan(sChan);
	m_pUser->PutIRC("PART " + sChan);

	if (!CZNC::Get().WriteConfig()) {
		GetErrorPage(sPageRet, "Channel deleted, but config was not written");
		return true;
	}

	Redirect("/edituser?user=" + m_pUser->GetUserName().Escape_n(CString::EURL));
	return false;
}

bool CWebAdminSock::UserPage(CString& sPageRet, CUser* pUser) {
	if (!GetParam("submitted").ToUInt()) {
		CString sAllowedHosts, sServers, sChans, sCTCPReplies;

		if (pUser) {
			m_Template["Action"] = "edituser";
			m_Template["Title"] = "Edit User [" + pUser->GetUserName() + "]";
			m_Template["Edit"] = "true";
			m_Template["Username"] = pUser->GetUserName();
			m_Template["Nick"] = pUser->GetNick();
			m_Template["AltNick"] = pUser->GetAltNick();
			m_Template["StatusPrefix"] = pUser->GetStatusPrefix();
			m_Template["Ident"] = pUser->GetIdent();
			m_Template["RealName"] = pUser->GetRealName();
			m_Template["QuitMsg"] = pUser->GetQuitMsg();
			m_Template["DefaultChanModes"] = pUser->GetDefaultChanModes();
			m_Template["BufferCount"] = CString(pUser->GetBufferCount());
			m_Template["TimestampFormat"] = pUser->GetTimestampFormat();
			m_Template["TimezoneOffset"] = CString(pUser->GetTimezoneOffset());
			m_Template["JoinTries"] = CString(pUser->JoinTries());
			m_Template["MaxJoins"] = CString(pUser->MaxJoins());

			const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
			for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); it++) {
				CTemplate& l = m_Template.AddRow("AllowedHostLoop");
				l["Host"] = *it;
			}

			const vector<CServer*>& vServers = pUser->GetServers();
			for (unsigned int a = 0; a < vServers.size(); a++) {
				CTemplate& l = m_Template.AddRow("ServerLoop");
				l["Server"] = vServers[a]->GetString();
			}

			const MCString& msCTCPReplies = pUser->GetCTCPReplies();
			for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); it2++) {
				CTemplate& l = m_Template.AddRow("CTCPLoop");
				l["CTCP"] = it2->first + " " + it2->second;
			}

			const vector<CChan*>& Channels = pUser->GetChans();
			for (unsigned int c = 0; c < Channels.size(); c++) {
				CChan* pChan = Channels[c];
				CTemplate& l = m_Template.AddRow("ChannelLoop");

				l["Username"] = pUser->GetUserName();
				l["Name"] = pChan->GetName();
				l["Perms"] = pChan->GetPermStr();
				l["CurModes"] = pChan->GetModeString();
				l["DefModes"] = pChan->GetDefaultModes();
				l["BufferCount"] = CString(pChan->GetBufferCount());
				l["Options"] = pChan->GetOptions();

				if (pChan->InConfig()) {
					l["InConfig"] = "true";
				}
			}
		} else {
			m_Template["Action"] = "adduser";
			m_Template["Title"] = "Add User";
			m_Template["StatusPrefix"] = "*";
		}

		// To change VHosts be admin or don't have DenySetVHost
		const VCString& vsVHosts = CZNC::Get().GetVHosts();
		bool bFoundVHost = false;
		if (IsAdmin() || !m_pSessionUser->DenySetVHost()) {
			for (unsigned int b = 0; b < vsVHosts.size(); b++) {
				const CString& sVHost = vsVHosts[b];
				CTemplate& l = m_Template.AddRow("VHostLoop");

				l["VHost"] = sVHost;

				if (pUser && pUser->GetVHost() == sVHost) {
					l["Checked"] = "true";
					bFoundVHost = true;
				}
			}

			// If our current vhost is not in the global list...
			if (pUser && !bFoundVHost && !pUser->GetVHost().empty()) {
				CTemplate& l = m_Template.AddRow("VHostLoop");

				l["VHost"] = pUser->GetVHost();
				l["Checked"] = "true";
			}
		}

		set<CModInfo> ssUserMods;
		CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

		for (set<CModInfo>::iterator it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
			const CModInfo& Info = *it;
			CTemplate& l = m_Template.AddRow("ModuleLoop");

			l["Name"] = Info.GetName();
			l["Description"] = Info.GetDescription();
			l["Args"] = GetModArgs(Info.GetName());

			if (pUser && pUser->GetModules().FindModule(Info.GetName())) {
				l["Checked"] = "true";
			}

			if (!IsAdmin() && pUser && pUser->DenyLoadMod()) {
				l["Disabled"] = "true";
			}
		}

		CTemplate& o1 = m_Template.AddRow("OptionLoop");
		o1["Name"] = "keepbuffer";
		o1["DisplayName"] = "Keep Buffer";
		if (!pUser || pUser->KeepBuffer()) { o1["Checked"] = "true"; }

		/* o2 used to be auto cycle which was removed */

		CTemplate& o4 = m_Template.AddRow("OptionLoop");
		o4["Name"] = "multiclients";
		o4["DisplayName"] = "Multi Clients";
		if (!pUser || pUser->MultiClients()) { o4["Checked"] = "true"; }

		CTemplate& o5 = m_Template.AddRow("OptionLoop");
		o5["Name"] = "bouncedccs";
		o5["DisplayName"] = "Bounce DCCs";
		if (!pUser || pUser->BounceDCCs()) { o5["Checked"] = "true"; }

		CTemplate& o6 = m_Template.AddRow("OptionLoop");
		o6["Name"] = "useclientip";
		o6["DisplayName"] = "Use Client IP";
		if (pUser && pUser->UseClientIP()) { o6["Checked"] = "true"; }

		CTemplate& o7 = m_Template.AddRow("OptionLoop");
		o7["Name"] = "appendtimestamp";
		o7["DisplayName"] = "Append Timestamps";
		if (pUser && pUser->GetTimestampAppend()) { o7["Checked"] = "true"; }

		CTemplate& o8 = m_Template.AddRow("OptionLoop");
		o8["Name"] = "prependtimestamp";
		o8["DisplayName"] = "Prepend Timestamps";
		if (pUser && pUser->GetTimestampPrepend()) { o8["Checked"] = "true"; }

		if (IsAdmin()) {
			CTemplate& o9 = m_Template.AddRow("OptionLoop");
			o9["Name"] = "denyloadmod";
			o9["DisplayName"] = "Deny LoadMod";
			if (pUser && pUser->DenyLoadMod()) { o9["Checked"] = "true"; }

			CTemplate& o10 = m_Template.AddRow("OptionLoop");
			o10["Name"] = "isadmin";
			o10["DisplayName"] = "Admin";
			if (pUser && pUser->IsAdmin()) { o10["Checked"] = "true"; }
			if (pUser && pUser == CZNC::Get().FindUser(GetUser())) { o10["Disabled"] = "true"; }

			CTemplate& o11 = m_Template.AddRow("OptionLoop");
			o11["Name"] = "denysetvhost";
			o11["DisplayName"] = "Deny SetVHost";
			if (pUser && pUser->DenySetVHost()) { o11["Checked"] = "true"; }
		}

		PrintPage(sPageRet, "UserPage.tmpl");
		return true;
	}

	/* If pUser is NULL, we are adding a user, else we are editing this one */

	CString sUsername = GetParam("user");
	if (!pUser && CZNC::Get().FindUser(sUsername)) {
		GetErrorPage(sPageRet, "Invalid Submission [User " + sUsername + " already exists]");
		return true;
	}

	CUser* pNewUser = GetNewUser(sPageRet, pUser);
	if (!pNewUser) {
		return true;
	}

	CString sErr;

	if (!pUser) {
		// Add User Submission
		if (!CZNC::Get().AddUser(pNewUser, sErr)) {
			delete pNewUser;
			GetErrorPage(sPageRet, "Invalid submission [" + sErr + "]");
			return true;
		}

		if (!CZNC::Get().WriteConfig()) {
			GetErrorPage(sPageRet, "User added, but config was not written");
			return true;
		}
	} else {
		// Edit User Submission
		if (!pUser->Clone(*pNewUser, sErr, false)) {
			delete pNewUser;
			GetErrorPage(sPageRet, "Invalid Submission [" + sErr + "]");
			return true;
		}

		delete pNewUser;
		if (!CZNC::Get().WriteConfig()) {
			GetErrorPage(sPageRet, "User edited, but config was not written");
			return true;
		}
	}

	if (!IsAdmin()) {
		Redirect("/edituser");
	} else {
		Redirect("/listusers");
	}

	return false;
}

CUser* CWebAdminSock::GetNewUser(CString& sPageRet, CUser* pUser) {
	CString sUsername = GetParam("newuser");

	if (sUsername.empty()) {
		sUsername = GetParam("user");
	}

	if (sUsername.empty()) {
		GetErrorPage(sPageRet, "Invalid Submission [Username is required]");
		return NULL;
	}

	if (pUser) {
		/* If we are editing a user we must not change the user name */
		sUsername = pUser->GetUserName();
	}

	CUser* pNewUser = new CUser(sUsername);

	CString sArg = GetParam("password");

	if (sArg != GetParam("password2")) {
		GetErrorPage(sPageRet, "Invalid Submission [Passwords do not match]");
		return NULL;
	}

	if (!sArg.empty()) {
		CString sSalt = CUtils::GetSalt();
		CString sHash = CUser::SaltedHash(sArg, sSalt);
		pNewUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
	}

	VCString vsArgs;
	GetRawParam("servers").Split("\n", vsArgs);
	unsigned int a = 0;

	for (a = 0; a < vsArgs.size(); a++) {
		pNewUser->AddServer(vsArgs[a].Trim_n());
	}

	GetRawParam("allowedips").Split("\n", vsArgs);
	if (vsArgs.size()) {
		for (a = 0; a < vsArgs.size(); a++) {
			pNewUser->AddAllowedHost(vsArgs[a].Trim_n());
		}
	} else {
		pNewUser->AddAllowedHost("*");
	}

	GetRawParam("ctcpreplies").Split("\n", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		CString sReply = vsArgs[a].TrimRight_n("\r");
		pNewUser->AddCTCPReply(sReply.Token(0).Trim_n(), sReply.Token(1, true).Trim_n());
	}

	sArg = GetParam("nick"); if (!sArg.empty()) { pNewUser->SetNick(sArg); }
	sArg = GetParam("altnick"); if (!sArg.empty()) { pNewUser->SetAltNick(sArg); }
	sArg = GetParam("statusprefix"); if (!sArg.empty()) { pNewUser->SetStatusPrefix(sArg); }
	sArg = GetParam("ident"); if (!sArg.empty()) { pNewUser->SetIdent(sArg); }
	sArg = GetParam("realname"); if (!sArg.empty()) { pNewUser->SetRealName(sArg); }
	sArg = GetParam("quitmsg"); if (!sArg.empty()) { pNewUser->SetQuitMsg(sArg); }
	sArg = GetParam("chanmodes"); if (!sArg.empty()) { pNewUser->SetDefaultChanModes(sArg); }
	sArg = GetParam("timestampformat"); if (!sArg.empty()) { pNewUser->SetTimestampFormat(sArg); }

	sArg = GetParam("vhost");
	// To change VHosts be admin or don't have DenySetVHost
	if (IsAdmin() || !m_pSessionUser->DenySetVHost()) {
		if (!sArg.empty())
			pNewUser->SetVHost(sArg);
	} else if (pUser){
		pNewUser->SetVHost(pUser->GetVHost());
	}

	pNewUser->SetBufferCount(GetParam("bufsize").ToUInt());
	pNewUser->SetKeepBuffer(GetParam("keepbuffer").ToBool());
	pNewUser->SetMultiClients(GetParam("multiclients").ToBool());
	pNewUser->SetBounceDCCs(GetParam("bouncedccs").ToBool());
	pNewUser->SetUseClientIP(GetParam("useclientip").ToBool());
	pNewUser->SetTimestampAppend(GetParam("appendtimestamp").ToBool());
	pNewUser->SetTimestampPrepend(GetParam("prependtimestamp").ToBool());
	pNewUser->SetTimezoneOffset(GetParam("timezoneoffset").ToDouble());
	pNewUser->SetJoinTries(GetParam("jointries").ToUInt());
	pNewUser->SetMaxJoins(GetParam("maxjoins").ToUInt());

	if (IsAdmin()) {
		pNewUser->SetDenyLoadMod(GetParam("denyloadmod").ToBool());
		pNewUser->SetDenySetVHost(GetParam("denysetvhost").ToBool());
	} else if (pUser) {
		pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
		pNewUser->SetDenySetVHost(pUser->DenySetVHost());
	}

	// If pUser is not NULL, we are editing an existing user.
	// Users must not be able to change their own admin flag.
	if (pUser != CZNC::Get().FindUser(GetUser())) {
		pNewUser->SetAdmin(GetParam("isadmin").ToBool());
	} else if (pUser) {
		pNewUser->SetAdmin(pUser->IsAdmin());
	}

	GetParamValues("channel", vsArgs);
	for (a = 0; a < vsArgs.size(); a++) {
		const CString& sChan = vsArgs[a];
		pNewUser->AddChan(sChan.TrimRight_n("\r"), GetParam("save_" + sChan).ToBool());
	}

	if (IsAdmin() || (pUser && !pUser->DenyLoadMod())) {
		GetParamValues("loadmod", vsArgs);

		for (a = 0; a < vsArgs.size(); a++) {
			CString sModRet;
			CString sModName = vsArgs[a].TrimRight_n("\r");

			if (!sModName.empty()) {
				CString sArgs = GetParam("modargs_" + sModName);

				try {
					if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet, (pUser != NULL))) {
						DEBUG("Unable to load module [" << sModName << "] [" << sModRet << "]");
					}
				} catch (...) {
					DEBUG("Unable to load module [" << sModName << "] [" << sArgs << "]");
				}
			}
		}
	} else if (pUser) {
		CModules& Modules = pUser->GetModules();

		for (a = 0; a < Modules.size(); a++) {
			CString sModName = Modules[a]->GetModName();
			CString sArgs = Modules[a]->GetArgs();
			CString sModRet;

			try {
				if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet, (pUser != NULL))) {
					DEBUG("Unable to load module [" << sModName << "] [" << sModRet << "]");
				}
			} catch (...) {
				DEBUG("Unable to load module [" << sModName << "]");
			}
		}
	}

	return pNewUser;
}

CWebAdminAuth::CWebAdminAuth(CWebAdminSock* pWebAdminSock, const CString& sUsername,
		const CString& sPassword)
		: CAuthBase(sUsername, sPassword, pWebAdminSock) {
	m_pWebAdminSock = pWebAdminSock;
}

void CWebAdminAuth::AcceptedLogin(CUser& User) {
	if (m_pWebAdminSock) {
		m_pWebAdminSock->SetSessionUser(&User);
		m_pWebAdminSock->SetLoggedIn(true);
		m_pWebAdminSock->UnPauseRead();
	}
}

void CWebAdminAuth::RefusedLogin(const CString& sReason) {
	if (m_pWebAdminSock) {
		m_pWebAdminSock->SetLoggedIn(false);
		m_pWebAdminSock->UnPauseRead();
	}
}

GLOBALMODULEDEFS(CWebAdminMod, "Dynamic configuration of users/settings through a web browser")
