/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "WebModules.h"
#include "User.h"
#include "znc.h"
#include <sstream>

// Sessions are valid for a day, (24h, ...)
CWebSessionMap CWebSock::m_mspSessions(24 * 60 * 60 * 1000);

CZNCTagHandler::CZNCTagHandler(CWebSock& WebSock) : CTemplateTagHandler(), m_WebSock(WebSock) {
}

bool CZNCTagHandler::HandleTag(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) {
	if (sName.Equals("URLPARAM")) {
		//sOutput = CZNC::Get()
		sOutput = m_WebSock.GetParam(sArgs.Token(0));
		return true;
	}

	return false;
}

CWebSession::CWebSession(const CString& sId) : m_sId(sId) {
	m_bLoggedIn = false;
	m_pUser = NULL;
}

bool CWebSession::IsAdmin() const { return IsLoggedIn() && m_pUser->IsAdmin(); }

CWebAuth::CWebAuth(CWebSock* pWebSock, const CString& sUsername, const CString& sPassword)
	: CAuthBase(sUsername, sPassword, pWebSock) {
	m_pWebSock = pWebSock;
}

void CWebSession::ClearMessageLoops() {
	m_vsErrorMsgs.clear();
	m_vsSuccessMsgs.clear();
}

void CWebSession::FillMessageLoops(CTemplate& Tmpl) {
	for (unsigned int a = 0; a < m_vsErrorMsgs.size(); a++) {
		CTemplate& Row = Tmpl.AddRow("ErrorLoop");
		Row["Message"] = m_vsErrorMsgs[a];
	}

	for (unsigned int b = 0; b < m_vsSuccessMsgs.size(); b++) {
		CTemplate& Row = Tmpl.AddRow("SuccessLoop");
		Row["Message"] = m_vsSuccessMsgs[b];
	}
}

size_t CWebSession::AddError(const CString& sMessage) {
	m_vsErrorMsgs.push_back(sMessage);
	return m_vsErrorMsgs.size();
}

size_t CWebSession::AddSuccess(const CString& sMessage) {
	m_vsSuccessMsgs.push_back(sMessage);
	return m_vsSuccessMsgs.size();
}

void CWebSessionMap::FinishUserSessions(const CUser& User) {
	iterator it = m_mItems.begin();

	while (it != m_mItems.end()) {
		if (it->second.second->GetUser() == &User) {
			m_mItems.erase(it++);
		} else {
			++it;
		}
	}
}

void CWebAuth::AcceptedLogin(CUser& User) {
	if (m_pWebSock) {
		CSmartPtr<CWebSession> spSession = m_pWebSock->GetSession();

		spSession->SetUser(&User);
		spSession->SetLoggedIn(true);

		m_pWebSock->SetLoggedIn(true);
		m_pWebSock->UnPauseRead();

		DEBUG("Successful login attempt ==> USER [" + User.GetUserName() + "] ==> SESSION [" + spSession->GetId() + "]");
	}
}

void CWebAuth::RefusedLogin(const CString& sReason) {
	if (m_pWebSock) {
		CSmartPtr<CWebSession> spSession = m_pWebSock->GetSession();

		spSession->AddError("Invalid login!");
		spSession->SetUser(NULL);
		spSession->SetLoggedIn(false);

		m_pWebSock->SetLoggedIn(false);
		m_pWebSock->UnPauseRead();

		DEBUG("UNSUCCESSFUL login attempt ==> REASON [" + sReason + "] ==> SESSION [" + spSession->GetId() + "]");
	}
}

void CWebAuth::Invalidate() {
	CAuthBase::Invalidate();
	m_pWebSock = NULL;
}

CWebSock::CWebSock(CModule* pModule) : CHTTPSock(pModule) {
	m_pModule = pModule;
	m_bPathsSet = false;

	m_Template.AddTagHandler(new CZNCTagHandler(*this));
}

CWebSock::CWebSock(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout)
		: CHTTPSock(pModule, sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	m_bPathsSet = false;

	m_Template.AddTagHandler(new CZNCTagHandler(*this));
}

CWebSock::~CWebSock() {
	if (!m_spAuth.IsNull()) {
		m_spAuth->Invalidate();
	}

	CUser *pUser = GetSession()->GetUser();
	if (pUser) {
		pUser->AddBytesWritten(GetBytesWritten());
		pUser->AddBytesRead(GetBytesRead());
	} else {
		CZNC::Get().AddBytesWritten(GetBytesWritten());
		CZNC::Get().AddBytesRead(GetBytesRead());
	}

	// If the module IsFake() then it was created as a dummy and needs to be deleted
	if (m_pModule && m_pModule->IsFake()) {
		m_pModule->UnlinkSocket(this);
		delete m_pModule;
		m_pModule = NULL;
	}
}

void CWebSock::ParsePath() {
	// The URI looks like:
	//         /[user:][module][/page][?arg1=val1&arg2=val2...]

	m_sForceUser.clear();

	m_sPath = GetPath().TrimLeft_n("/");

	m_sPath.TrimPrefix("mods/");
	m_sPath.TrimPrefix("modfiles/");

	m_sModName = m_sPath.Token(0, false, "/");
	m_sPage = m_sPath.Token(1, true, "/");

	if (m_sModName.find(":") != CString::npos) {
		m_sForceUser = m_sModName.Token(0, false, ":");
		m_sModName = m_sModName.Token(1, false, ":");
	}

	if (m_sPage.empty()) {
		m_sPage = "index";
	}

	DEBUG("Path   [" + m_sPath + "]");
	DEBUG("User   [" + m_sForceUser + "]");
	DEBUG("Module [" + m_sModName + "]");
	DEBUG("Page   [" + m_sPage + "]");
}

CModule* CWebSock::ResolveModule() {
	ParsePath();
	CModule* pModRet = NULL;

	// Dot means static file, not module
	if (m_sModName.find(".") != CString::npos) {
		return NULL;
	}

	// First look for forced user-mods
	if (!m_sForceUser.empty()) {
		CUser* pUser = CZNC::Get().FindUser(m_sForceUser);

		if (pUser) {
			pModRet = pUser->GetModules().FindModule(m_sModName);
		} else {
			DEBUG("User not found while trying to handle web requrest for [" + m_sPage + "]");
		}
	} else {
		// This could be user level or global level, check both
		pModRet = CZNC::Get().GetModules().FindModule(m_sModName);

		if (!pModRet) {
			// It's not a loaded global module and it has no forced username so we
			// have to force a login to try a module loaded by the current user
			if (!ForceLogin()) {
				return NULL;
			}

			pModRet = m_spSession->GetUser()->GetModules().FindModule(m_sModName);
		}
	}

	if (!pModRet) {
		DEBUG("Module not found");
	} else if (pModRet->IsFake()) {
		DEBUG("Fake module found, ignoring");
		pModRet = NULL;
	}

	return pModRet;
}

size_t CWebSock::GetAvailSkins(vector<CFile>& vRet) {
	vRet.clear();

	CString sRoot(GetSkinPath("_default_"));

	sRoot.TrimRight("/");
	sRoot.TrimRight("_default_");
	sRoot.TrimRight("/");

	if (!sRoot.empty()) {
		sRoot += "/";
	}

	if (!sRoot.empty() && CFile::IsDir(sRoot)) {
		CDir Dir(sRoot);

		for (unsigned int d = 0; d < Dir.size(); d++) {
			const CFile& SubDir = *Dir[d];

			if (SubDir.IsDir() && SubDir.GetShortName() == "_default_") {
				vRet.push_back(SubDir);
			}
		}

		for (unsigned int e = 0; e < Dir.size(); e++) {
			const CFile& SubDir = *Dir[e];

			if (SubDir.IsDir() && SubDir.GetShortName() != "_default_" && SubDir.GetShortName() != ".svn") {
				vRet.push_back(SubDir);
			}
		}
	}

	return vRet.size();
}

void CWebSock::SetPaths(CModule* pModule, bool bIsTemplate) {
	m_Template.ClearPaths();

	CString sHomeSkinsDir(CZNC::Get().GetZNCPath() + "/webskins/");
	CString sSkinName(GetSkinName());

	DEBUG("---- sHomeSkinsDir=[" + sHomeSkinsDir + "] sSkinName=[" + sSkinName + "]");

	// Module specific paths

	if (pModule) {
		const CString& sModName(pModule->GetModName());

		// 1. ~/.znc/webskins/<user_skin_setting>/mods/<mod_name>/
		//
		if (!sSkinName.empty()) {
			m_Template.AppendPath(GetSkinPath(sSkinName) + "/mods/" + sModName + "/");
		}

		// 2. ~/.znc/webskins/_default_/mods/<mod_name>/
		//
		m_Template.AppendPath(GetSkinPath("_default_") + "/mods/" + sModName + "/");

		// 3. ./modules/<mod_name>/
		//
		m_Template.AppendPath(GetModWebPath(sModName));

		// 4. ~/.znc/webskins/<user_skin_setting>/mods/<mod_name>/
		//
		if (!sSkinName.empty()) {
			m_Template.AppendPath(GetSkinPath(sSkinName) + "/mods/" + sModName + "/");
		}

		// 5. ~/.znc/webskins/_default_/mods/<mod_name>/
		//
		m_Template.AppendPath(GetSkinPath("_default_") + "/mods/" + sModName + "/");
	}

	// 6. ~/.znc/webskins/<user_skin_setting>/
	//
	if (!sSkinName.empty()) {
		m_Template.AppendPath(GetSkinPath(sSkinName) + CString(bIsTemplate ? "/tmpl/" : "/"), (0 && pModule != NULL));
	}

	// 7. ~/.znc/webskins/_default_/
	//
	m_Template.AppendPath(GetSkinPath("_default_") + CString(bIsTemplate ? "/tmpl/" : "/"), (0 && pModule != NULL));

	m_bPathsSet = true;
}

void CWebSock::SetVars() {
	m_Template["SessionUser"] = GetUser();
	m_Template["SessionIP"] = GetRemoteIP();
	m_Template["Tag"] = CZNC::GetTag();
	m_Template["SkinName"] = GetSkinName();

	if (m_spSession->IsAdmin()) {
		m_Template["IsAdmin"] = "true";
	}

	m_spSession->FillMessageLoops(m_Template);
	m_spSession->ClearMessageLoops();

	// Global Mods
	CGlobalModules& vgMods = CZNC::Get().GetModules();
	for (unsigned int a = 0; a < vgMods.size(); a++) {
		AddModLoop("GlobalModLoop", *vgMods[a]);
	}

	// User Mods
	if (IsLoggedIn()) {
		CModules& vMods = m_spSession->GetUser()->GetModules();

		for (unsigned int a = 0; a < vMods.size(); a++) {
			AddModLoop("UserModLoop", *vMods[a]);
		}
	}

	if (IsLoggedIn()) {
		m_Template["LoggedIn"] = "true";
	}
}

bool CWebSock::AddModLoop(const CString& sLoopName, CModule& Module) {
	CString sTitle(Module.GetWebMenuTitle());

	DEBUG("===   ===   ===   ===   === [" + Module.GetModName() + "] [" + CString(IsLoggedIn()) + "]");

	if (!sTitle.empty() && (IsLoggedIn() || (!Module.WebRequiresLogin() && !Module.WebRequiresAdmin())) && (m_spSession->IsAdmin() || !Module.WebRequiresAdmin())) {
		CTemplate& Row = m_Template.AddRow(sLoopName);

		Row["ModName"] = Module.GetModName();
		Row["Title"] = sTitle;

		if (m_sModName == Module.GetModName()) {
			Row["Active"] = "true";
		}

		if (Module.GetUser()) {
			Row["Username"] = Module.GetUser()->GetUserName();
		}

		VWebSubPages& vSubPages = Module.GetSubPages();

		for (unsigned int a = 0; a < vSubPages.size(); a++) {
			TWebSubPage& SubPage = vSubPages[a];

			// bActive is whether or not the current url matches this subpage (params will be checked below)
			bool bActive = (m_sModName == Module.GetModName() && m_sPage == SubPage->GetName());

			if (SubPage->RequiresAdmin() && !m_spSession->IsAdmin()) {
				continue;	// Don't add admin-only subpages to requests from non-admin users
			}

			CTemplate& SubRow = Row.AddRow("SubPageLoop");

			SubRow["ModName"] = Module.GetModName();
			SubRow["PageName"] = SubPage->GetName();
			SubRow["Title"] = SubPage->GetTitle().empty() ? SubPage->GetName() : SubPage->GetTitle();

			CString& sParams = SubRow["Params"];

			const VPair& vParams = SubPage->GetParams();
			for (size_t b = 0; b < vParams.size(); b++) {
				pair<CString, CString> ssNV = vParams[b];

				if (!sParams.empty()) {
					sParams += "&";
				}

				if (!ssNV.first.empty()) {
					if (!ssNV.second.empty()) {
						sParams += ssNV.first.Escape_n(CString::EURL);
						sParams += "=";
						sParams += ssNV.second.Escape_n(CString::EURL);
					}

					if (bActive && GetParam(ssNV.first) != ssNV.second) {
						bActive = false;
					}
				}
			}

			if (bActive) {
				SubRow["Active"] = "true";
			}
		}

		return true;
	}

	return false;
}

bool CWebSock::PrintStaticFile(const CString& sPath, CString& sPageRet, CModule* pModule) {
	SetPaths(pModule);
	DEBUG("About to print [" + m_Template.ExpandFile(sPath) + "]");
	return PrintFile(m_Template.ExpandFile(sPath.TrimLeft_n("/")));
}

bool CWebSock::PrintTemplate(const CString& sPageName, CString& sPageRet, CModule* pModule) {
	SetVars();
	m_Template["PageName"] = sPageName;

	if (pModule) {
		CUser* pUser = pModule->GetUser();
		m_Template["ModUser"] = pUser ? pUser->GetUserName() : "";
		m_Template["ModName"] = pModule->GetModName();

		if (m_Template.find("Title") == m_Template.end()) {
			m_Template["Title"] = pModule->GetWebMenuTitle();
		}
	}

	if (!m_bPathsSet) {
		SetPaths(pModule, true);
	}

	if (m_Template.GetFileName().empty() && !m_Template.SetFile(sPageName + ".tmpl")) {
		return false;
	}

	return m_Template.PrintString(sPageRet);
}

CString CWebSock::GetModWebPath(const CString& sModName) const {
	CString sRet = CZNC::Get().GetCurPath() + "/modules/www/" + sModName;

	if (!CFile::IsDir(sRet)) {
		sRet = CString(_MODDIR_) + "/www/" + sModName;
	}

	return sRet + "/";
}

CString CWebSock::GetSkinPath(const CString& sSkinName) const {
	CString sRet = CZNC::Get().GetZNCPath() + "/webskins/" + sSkinName;

	if (!CFile::IsDir(sRet)) {
		sRet = CZNC::Get().GetCurPath() + "/webskins/" + sSkinName;

		if (!CFile::IsDir(sRet)) {
			sRet = CString(_SKINDIR_) + "/" + sSkinName;
		}
	}

	return sRet + "/";
}

bool CWebSock::ForceLogin() {
	if (m_spSession->IsLoggedIn()) {
		return true;
	}

	m_spSession->AddError("You must login to view that page");
	Redirect("/");
	return false;
}

CString CWebSock::GetCookie(const CString& sKey) const {
	if (!m_sModName.empty()) {
		return CHTTPSock::GetCookie("Mod::" + m_sModName + "::" + sKey);
	}

	return CHTTPSock::GetCookie(sKey);
}

bool CWebSock::SetCookie(const CString& sKey, const CString& sValue) {
	if (!m_sModName.empty()) {
		return CHTTPSock::SetCookie("Mod::" + m_sModName + "::" + sKey, sValue);
	}

	return CHTTPSock::SetCookie(sKey, sValue);
}

bool CWebSock::OnPageRequest(const CString& sURI, CString& sPageRet) {
	DEBUG("CWebSock::OnPageRequest(" + sURI + ")");
	m_spSession = GetSession();
	SetCookie("SessionId", m_spSession->GetId());

	if (m_spSession->IsLoggedIn()) {
		m_sUser = m_spSession->GetUser()->GetUserName();
		m_bLoggedIn = true;
	}

	// Handle the static pages that don't require a login
	if (sURI == "/") {
		return PrintTemplate("index", sPageRet);
	} else if (sURI == "/favicon.ico") {
		return PrintStaticFile("/pub/favicon.ico", sPageRet);
	} else if (sURI == "/logout") {
		m_spSession->SetLoggedIn(false);
		SetLoggedIn(false);
		Redirect("/");

		return true;
	} else if (sURI == "/login" || sURI.Left(7) == "/login/") {
		if (GetParam("submitted").ToBool()) {
			m_sUser = GetParam("user");
			m_sPass = GetParam("pass");
			m_bLoggedIn = OnLogin(m_sUser, m_sPass);

			Redirect("/");
			return true;
		}

		return PrintTemplate("login", sPageRet);
	} else if (sURI.Left(5) == "/pub/") {
		return PrintStaticFile(sURI, sPageRet);
	} else if (sURI.Left(6) == "/mods/" || sURI.Left(10) == "/modfiles/") {
		ParsePath();
		// Make sure modules are treated as directories
		if (sURI.Right(1) != "/" && sURI.find(".") == CString::npos && sURI.TrimLeft_n("/mods/").TrimLeft_n("/").find("/") == CString::npos) {
			Redirect(sURI + "/");
			return true;
		}

		if (m_sModName.empty()) {
			return PrintTemplate("modlist", sPageRet);
		}

		DEBUG("FindModule(" + m_sModName + ", " + m_sForceUser + ")");
		CModule* pModule = CZNC::Get().FindModule(m_sModName, m_sForceUser);

		if (!pModule && m_sForceUser.empty()) {
			if (!ForceLogin()) {
				return true;
			}

			pModule = CZNC::Get().FindModule(m_sModName, m_spSession->GetUser());
		}

		if (!pModule) {
			return false;
		} else if (pModule->WebRequiresLogin() && !ForceLogin()) {
			return true;
		} else if (pModule->WebRequiresAdmin() && !m_spSession->IsAdmin()) {
			sPageRet = GetErrorPage(403, "Forbidden", "You need to be an admin to access this module");
			return true;
		} else if (pModule && !pModule->IsGlobal() && pModule->GetUser() != m_spSession->GetUser()) {
			sPageRet = GetErrorPage(403, "Forbidden", "You must login as " + pModule->GetUser()->GetUserName() + " in order to view this page");
			return true;
		}

		VWebSubPages& vSubPages = pModule->GetSubPages();

		for (unsigned int a = 0; a < vSubPages.size(); a++) {
			TWebSubPage& SubPage = vSubPages[a];

			bool bActive = (m_sModName == pModule->GetModName() && m_sPage == SubPage->GetName());

			if (bActive && SubPage->RequiresAdmin() && !m_spSession->IsAdmin()) {
				sPageRet = GetErrorPage(403, "Forbidden", "You need to be an admin to access this page");
				return true;
			}
		}

		if (pModule && !pModule->IsGlobal() && (!IsLoggedIn() || pModule->GetUser() != m_spSession->GetUser())) {
			AddModLoop("UserModLoop", *pModule);
		}

		if (sURI.Left(10) == "/modfiles/") {
			m_Template.AppendPath(GetSkinPath(GetSkinName()) + "/mods/" + m_sModName + "/files/");
			m_Template.AppendPath(GetModWebPath(m_sModName) + "/files/");

			std::cerr << "===================== fffffffffffffffffffff   [" << m_sModName << "]  [" << m_sPage << "]" << std::endl;
			return PrintFile(m_Template.ExpandFile(m_sPage.TrimLeft_n("/")));
		} else {
			SetPaths(pModule, true);

			if (pModule->OnWebRequest(*this, m_sPage, m_Template)) {
				return PrintTemplate(m_sPage, sPageRet, pModule);
			}

			sPageRet = GetErrorPage(404, "Not Implemented", "The requested module does not acknowledge web requests");
			return false;
		}
	} else {
		CString sPage(sURI.Trim_n("/"));
		if (sPage.length() < 32) {
			for (unsigned int a = 0; a < sPage.length(); a++) {
				unsigned char c = sPage[a];

				if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
					return false;
				}
			}

			return PrintTemplate(sPage, sPageRet);
		}
	}

	return false;
}

void CWebSock::PrintErrorPage(const CString& sMessage) {
	m_Template.SetFile("Error.tmpl");

	m_Template["Action"] = "error";
	m_Template["Title"] = "Error";
	m_Template["Error"] = sMessage;
}

CSmartPtr<CWebSession> CWebSock::GetSession() {
	if (!m_spSession.IsNull()) {
		return m_spSession;
	}

	CSmartPtr<CWebSession> *pSession = m_mspSessions.GetItem(GetCookie("SessionId"));

	if (pSession != NULL) {
		// Refresh the timeout
		m_mspSessions.AddItem((*pSession)->GetId(), *pSession);
		DEBUG("Found existing session from cookie: [" + GetCookie("SessionId") + "] IsLoggedIn(" + CString((*pSession)->IsLoggedIn() ? "true" : "false") + ")");
		return *pSession;
	}

	CString sSessionID;
	do {
		sSessionID = CString::RandomString(32);
		sSessionID += ":" + GetRemoteIP() + ":" + CString(GetRemotePort());
		sSessionID += ":" + GetLocalIP()  + ":" + CString(GetLocalPort());
		sSessionID += ":" + CString(time(NULL));
		sSessionID = sSessionID.SHA256();

		DEBUG("Auto generated session: [" + sSessionID + "]");
	} while (m_mspSessions.HasItem(sSessionID));

	CSmartPtr<CWebSession> spSession(new CWebSession(sSessionID));
	m_mspSessions.AddItem(spSession->GetId(), spSession);

	return spSession;
}

bool CWebSock::OnLogin(const CString& sUser, const CString& sPass) {
	DEBUG("=================== CWebSock::OnLogin()");
	m_spAuth = new CWebAuth(this, sUser, sPass);

	// Some authentication module could need some time, block this socket
	// until then. CWebAuth will UnPauseRead().
	PauseRead();
	CZNC::Get().AuthUser(m_spAuth);

	// If CWebAuth already set this, don't change it.
	return IsLoggedIn();
}

Csock* CWebSock::GetSockObj(const CString& sHost, unsigned short uPort) {
	CWebSock* pSock = new CWebSock(GetModule(), sHost, uPort);
	pSock->SetSockName("Web::Client");
	pSock->SetTimeout(120);

	return pSock;
}

CString CWebSock::GetSkinName() {
	CSmartPtr<CWebSession> spSession = GetSession();

	if (spSession->IsLoggedIn() && !spSession->GetUser()->GetSkinName().empty()) {
		return spSession->GetUser()->GetSkinName();
	}

	return CZNC::Get().GetSkinName();
}

