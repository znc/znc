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

/// @todo Do we want to make this a configure option?
#define _SKINDIR_ _DATADIR_ "/webskins"

// Sessions are valid for a day, (24h, ...)
CWebSessionMap CWebSock::m_mspSessions(24 * 60 * 60 * 1000);

CZNCTagHandler::CZNCTagHandler(CWebSock& WebSock) : CTemplateTagHandler(), m_WebSock(WebSock) {
}

bool CZNCTagHandler::HandleTag(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) {
	if (sName.Equals("URLPARAM")) {
		//sOutput = CZNC::Get()
		sOutput = m_WebSock.GetParam(sArgs.Token(0), false);
		return true;
	}

	return false;
}

CWebSession::CWebSession(const CString& sId) : m_sId(sId) {
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

		m_pWebSock->SetLoggedIn(true);
		m_pWebSock->UnPauseRead();
		m_pWebSock->Redirect("/?cookie_check=true");

		DEBUG("Successful login attempt ==> USER [" + User.GetUserName() + "] ==> SESSION [" + spSession->GetId() + "]");
	}
}

void CWebAuth::RefusedLogin(const CString& sReason) {
	if (m_pWebSock) {
		CSmartPtr<CWebSession> spSession = m_pWebSock->GetSession();

		spSession->AddError("Invalid login!");
		spSession->SetUser(NULL);

		m_pWebSock->SetLoggedIn(false);
		m_pWebSock->UnPauseRead();
		m_pWebSock->Redirect("/?cookie_check=true");

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

	DEBUG("Path [" + m_sPath + "], User [" + m_sForceUser + "], Module [" + m_sModName + "], Page [" + m_sPage + "]");
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
			DEBUG("User not found while trying to handle web request for [" + m_sPage + "]");
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

			pModRet = GetSession()->GetUser()->GetModules().FindModule(m_sModName);
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

		// 3. ./modules/<mod_name>/tmpl/
		//
		m_Template.AppendPath(pModule->GetModDataDir() + "/tmpl/");

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
	m_Template["Tag"] = CZNC::GetTag(GetSession()->GetUser() != NULL);
	m_Template["SkinName"] = GetSkinName();
	m_Template["_CSRF_Check"] = GetCSRFCheck();

	if (GetSession()->IsAdmin()) {
		m_Template["IsAdmin"] = "true";
	}

	GetSession()->FillMessageLoops(m_Template);
	GetSession()->ClearMessageLoops();

	// Global Mods
	CGlobalModules& vgMods = CZNC::Get().GetModules();
	for (unsigned int a = 0; a < vgMods.size(); a++) {
		AddModLoop("GlobalModLoop", *vgMods[a]);
	}

	// User Mods
	if (IsLoggedIn()) {
		CModules& vMods = GetSession()->GetUser()->GetModules();

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

	if (!sTitle.empty() && (IsLoggedIn() || (!Module.WebRequiresLogin() && !Module.WebRequiresAdmin())) && (GetSession()->IsAdmin() || !Module.WebRequiresAdmin())) {
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

			if (SubPage->RequiresAdmin() && !GetSession()->IsAdmin()) {
				continue;  // Don't add admin-only subpages to requests from non-admin users
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

					if (bActive && GetParam(ssNV.first, false) != ssNV.second) {
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

CWebSock::EPageReqResult CWebSock::PrintStaticFile(const CString& sPath, CString& sPageRet, CModule* pModule) {
	SetPaths(pModule);
	CString sFile = m_Template.ExpandFile(sPath.TrimLeft_n("/"));
	DEBUG("About to print [" + sFile+ "]");
	// Either PrintFile() fails and sends an error page or it suceeds and
	// sends a result. In both cases we don't have anything more to do.
	PrintFile(sFile);
	return PAGE_DONE;
}

CWebSock::EPageReqResult CWebSock::PrintTemplate(const CString& sPageName, CString& sPageRet, CModule* pModule) {
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
		return PAGE_NOTFOUND;
	}

	if (m_Template.PrintString(sPageRet)) {
		return PAGE_PRINT;
	} else {
		return PAGE_NOTFOUND;
	}
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
	if (GetSession()->IsLoggedIn()) {
		return true;
	}

	GetSession()->AddError("You must login to view that page");
	Redirect("/");
	return false;
}

CString CWebSock::GetRequestCookie(const CString& sKey) const {
	CString sRet;

	if (!m_sModName.empty()) {
		sRet = CHTTPSock::GetRequestCookie("Mod::" + m_sModName + "::" + sKey);
	}

	if (sRet.empty()) {
		return CHTTPSock::GetRequestCookie(sKey);
	}
	return sRet;
}

bool CWebSock::SendCookie(const CString& sKey, const CString& sValue) {
	if (!m_sModName.empty()) {
		return CHTTPSock::SendCookie("Mod::" + m_sModName + "::" + sKey, sValue);
	}

	return CHTTPSock::SendCookie(sKey, sValue);
}

void CWebSock::OnPageRequest(const CString& sURI) {
	CString sPageRet;
	EPageReqResult eRet = OnPageRequestInternal(sURI, sPageRet);
	switch (eRet) {
	case PAGE_PRINT:
		PrintPage(sPageRet);
		break;
	case PAGE_DEFERRED:
		// Something else will later call Close()
		break;
	case PAGE_DONE:
		// Redirect or something like that, it's done, just make sure
		// the connection will be closed
		Close(CLT_AFTERWRITE);
		break;
	default:
		PrintNotFound();
		break;
	}
}

CWebSock::EPageReqResult CWebSock::OnPageRequestInternal(const CString& sURI, CString& sPageRet) {
	// Check that they really POSTed from one our forms by checking if they
	// know the "secret" CSRF check value. Don't do this for login since
	// CSRF against the login form makes no sense and the login form does a
	// cookies-enabled check which would break otherwise.
	if (IsPost() && GetParam("_CSRF_Check") != GetCSRFCheck() && sURI != "/login") {
		sPageRet = GetErrorPage(403, "Access denied", "POST requests need to send "
				"a secret token to prevent cross-site request forgery attacks.");
		return PAGE_PRINT;
	}

	SendCookie("SessionId", GetSession()->GetId());

	if (GetSession()->IsLoggedIn()) {
		m_sUser = GetSession()->GetUser()->GetUserName();
		m_bLoggedIn = true;
	}

	// Handle the static pages that don't require a login
	if (sURI == "/") {
		if(!m_bLoggedIn && GetParam("cookie_check", false).ToBool() && GetRequestCookie("SessionId").empty()) {
			GetSession()->AddError("Your browser does not have cookies enabled for this site!");
		}
		return PrintTemplate("index", sPageRet);
	} else if (sURI == "/favicon.ico") {
		return PrintStaticFile("/pub/favicon.ico", sPageRet);
	} else if (sURI == "/robots.txt") {
		return PrintStaticFile("/pub/robots.txt", sPageRet);
	} else if (sURI == "/logout") {
		GetSession()->SetUser(NULL);
		SetLoggedIn(false);
		Redirect("/");

		// We already sent a reply
		return PAGE_DONE;
	} else if (sURI == "/login") {
		if (GetParam("submitted").ToBool()) {
			m_sUser = GetParam("user");
			m_sPass = GetParam("pass");
			m_bLoggedIn = OnLogin(m_sUser, m_sPass);

			// AcceptedLogin()/RefusedLogin() will call Redirect()
			return PAGE_DEFERRED;
		}

		Redirect("/"); // the login form is here
		return PAGE_DONE;
	} else if (sURI.Left(5) == "/pub/") {
		return PrintStaticFile(sURI, sPageRet);
	} else if (sURI.Left(11) == "/skinfiles/") {
		CString sSkinName = sURI.substr(11);
		CString::size_type uPathStart = sSkinName.find("/");
		if (uPathStart != CString::npos) {
			CString sFilePath = sSkinName.substr(uPathStart + 1);
			sSkinName.erase(uPathStart);

			m_Template.ClearPaths();
			m_Template.AppendPath(GetSkinPath(sSkinName) + "pub");

			if (PrintFile(m_Template.ExpandFile(sFilePath))) {
				return PAGE_DONE;
			} else {
				return PAGE_NOTFOUND;
			}
		}
		return PAGE_NOTFOUND;
	} else if (sURI.Left(6) == "/mods/" || sURI.Left(10) == "/modfiles/") {
		ParsePath();
		// Make sure modules are treated as directories
		if (sURI.Right(1) != "/" && sURI.find(".") == CString::npos && sURI.TrimLeft_n("/mods/").TrimLeft_n("/").find("/") == CString::npos) {
			Redirect(sURI + "/");
			return PAGE_DONE;
		}

		if (m_sModName.empty()) {
			return PrintTemplate("modlist", sPageRet);
		}

		DEBUG("FindModule(" + m_sModName + ", " + m_sForceUser + ")");
		CModule* pModule = CZNC::Get().FindModule(m_sModName, m_sForceUser);

		if (!pModule && m_sForceUser.empty()) {
			if (!ForceLogin()) {
				return PAGE_DONE;
			}

			pModule = CZNC::Get().FindModule(m_sModName, GetSession()->GetUser());
		}

		if (!pModule) {
			return PAGE_NOTFOUND;
		} else if (pModule->WebRequiresLogin() && !ForceLogin()) {
			return PAGE_PRINT;
		} else if (pModule->WebRequiresAdmin() && !GetSession()->IsAdmin()) {
			sPageRet = GetErrorPage(403, "Forbidden", "You need to be an admin to access this module");
			return PAGE_PRINT;
		} else if (!pModule->IsGlobal() && pModule->GetUser() != GetSession()->GetUser()) {
			sPageRet = GetErrorPage(403, "Forbidden", "You must login as " + pModule->GetUser()->GetUserName() + " in order to view this page");
			return PAGE_PRINT;
		} else if (pModule->OnWebPreRequest(*this, m_sPage)) {
			return PAGE_DEFERRED;
		}

		VWebSubPages& vSubPages = pModule->GetSubPages();

		for (unsigned int a = 0; a < vSubPages.size(); a++) {
			TWebSubPage& SubPage = vSubPages[a];

			bool bActive = (m_sModName == pModule->GetModName() && m_sPage == SubPage->GetName());

			if (bActive && SubPage->RequiresAdmin() && !GetSession()->IsAdmin()) {
				sPageRet = GetErrorPage(403, "Forbidden", "You need to be an admin to access this page");
				return PAGE_PRINT;
			}
		}

		if (pModule && !pModule->IsGlobal() && (!IsLoggedIn() || pModule->GetUser() != GetSession()->GetUser())) {
			AddModLoop("UserModLoop", *pModule);
		}

		if (sURI.Left(10) == "/modfiles/") {
			m_Template.AppendPath(GetSkinPath(GetSkinName()) + "/mods/" + m_sModName + "/files/");
			m_Template.AppendPath(pModule->GetModDataDir() + "/files/");

			if (PrintFile(m_Template.ExpandFile(m_sPage.TrimLeft_n("/")))) {
				return PAGE_PRINT;
			} else {
				return PAGE_NOTFOUND;
			}
		} else {
			SetPaths(pModule, true);

			/* if a module returns false from OnWebRequest, it does not
			   want the template to be printed, usually because it did a redirect. */
			if (pModule->OnWebRequest(*this, m_sPage, m_Template)) {
				// If they already sent a reply, let's assume
				// they did what they wanted to do.
				if (SentHeader()) {
					return PAGE_DONE;
				}
				return PrintTemplate(m_sPage, sPageRet, pModule);
			}

			if (!SentHeader()) {
				sPageRet = GetErrorPage(404, "Not Implemented", "The requested module does not acknowledge web requests");
				return PAGE_PRINT;
			} else {
				return PAGE_DONE;
			}
		}
	} else {
		CString sPage(sURI.Trim_n("/"));
		if (sPage.length() < 32) {
			for (unsigned int a = 0; a < sPage.length(); a++) {
				unsigned char c = sPage[a];

				if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
					return PAGE_NOTFOUND;
				}
			}

			return PrintTemplate(sPage, sPageRet);
		}
	}

	return PAGE_NOTFOUND;
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

	const CString sCookieSessionId = GetRequestCookie("SessionId");
	CSmartPtr<CWebSession> *pSession = m_mspSessions.GetItem(sCookieSessionId);

	if (pSession != NULL) {
		// Refresh the timeout
		m_mspSessions.AddItem((*pSession)->GetId(), *pSession);
		m_spSession = *pSession;
		DEBUG("Found existing session from cookie: [" + sCookieSessionId + "] IsLoggedIn(" + CString((*pSession)->IsLoggedIn() ? "true" : "false") + ")");
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

	m_spSession = spSession;

	return spSession;
}

CString CWebSock::GetCSRFCheck() {
	CSmartPtr<CWebSession> pSession = GetSession();
	return pSession->GetId().MD5();
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

