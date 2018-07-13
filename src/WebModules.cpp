/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#include <znc/WebModules.h>
#include <znc/FileUtils.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/znc.h>
#include <time.h>
#include <algorithm>
#include <sstream>

using std::pair;
using std::vector;

/// @todo Do we want to make this a configure option?
#define _SKINDIR_ _DATADIR_ "/webskins"

const unsigned int CWebSock::m_uiMaxSessions = 5;

// We need this class to make sure the contained maps and their content is
// destroyed in the order that we want.
struct CSessionManager {
    // Sessions are valid for a day, (24h, ...)
    CSessionManager() : m_mspSessions(24 * 60 * 60 * 1000), m_mIPSessions() {}
    ~CSessionManager() {
        // Make sure all sessions are destroyed before any of our maps
        // are destroyed
        m_mspSessions.Clear();
    }

    CWebSessionMap m_mspSessions;
    std::multimap<CString, CWebSession*> m_mIPSessions;
};
typedef std::multimap<CString, CWebSession*>::iterator mIPSessionsIterator;

static CSessionManager Sessions;

class CWebAuth : public CAuthBase {
  public:
    CWebAuth(CWebSock* pWebSock, const CString& sUsername,
             const CString& sPassword, bool bBasic);
    ~CWebAuth() override {}

    CWebAuth(const CWebAuth&) = delete;
    CWebAuth& operator=(const CWebAuth&) = delete;

    void SetWebSock(CWebSock* pWebSock) { m_pWebSock = pWebSock; }
    void AcceptedLogin(CUser& User) override;
    void RefusedLogin(const CString& sReason) override;
    void Invalidate() override;

  private:
  protected:
    CWebSock* m_pWebSock;
    bool m_bBasic;
};

void CWebSock::FinishUserSessions(const CUser& User) {
    Sessions.m_mspSessions.FinishUserSessions(User);
}

CWebSession::~CWebSession() {
    // Find our entry in mIPSessions
    pair<mIPSessionsIterator, mIPSessionsIterator> p =
        Sessions.m_mIPSessions.equal_range(m_sIP);
    mIPSessionsIterator it = p.first;
    mIPSessionsIterator end = p.second;

    while (it != end) {
        if (it->second == this) {
            Sessions.m_mIPSessions.erase(it++);
        } else {
            ++it;
        }
    }
}

CZNCTagHandler::CZNCTagHandler(CWebSock& WebSock)
    : CTemplateTagHandler(), m_WebSock(WebSock) {}

bool CZNCTagHandler::HandleTag(CTemplate& Tmpl, const CString& sName,
                               const CString& sArgs, CString& sOutput) {
    if (sName.Equals("URLPARAM")) {
        // sOutput = CZNC::Get()
        sOutput = m_WebSock.GetParam(sArgs.Token(0), false);
        return true;
    }

    return false;
}

CWebSession::CWebSession(const CString& sId, const CString& sIP)
    : m_sId(sId),
      m_sIP(sIP),
      m_pUser(nullptr),
      m_vsErrorMsgs(),
      m_vsSuccessMsgs(),
      m_tmLastActive() {
    Sessions.m_mIPSessions.insert(make_pair(sIP, this));
    UpdateLastActive();
}

void CWebSession::UpdateLastActive() { time(&m_tmLastActive); }

bool CWebSession::IsAdmin() const { return IsLoggedIn() && m_pUser->IsAdmin(); }

CWebAuth::CWebAuth(CWebSock* pWebSock, const CString& sUsername,
                   const CString& sPassword, bool bBasic)
    : CAuthBase(sUsername, sPassword, pWebSock),
      m_pWebSock(pWebSock),
      m_bBasic(bBasic) {}

void CWebSession::ClearMessageLoops() {
    m_vsErrorMsgs.clear();
    m_vsSuccessMsgs.clear();
}

void CWebSession::FillMessageLoops(CTemplate& Tmpl) {
    for (const CString& sMessage : m_vsErrorMsgs) {
        CTemplate& Row = Tmpl.AddRow("ErrorLoop");
        Row["Message"] = sMessage;
    }

    for (const CString& sMessage : m_vsSuccessMsgs) {
        CTemplate& Row = Tmpl.AddRow("SuccessLoop");
        Row["Message"] = sMessage;
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
        std::shared_ptr<CWebSession> spSession = m_pWebSock->GetSession();

        spSession->SetUser(&User);

        m_pWebSock->SetLoggedIn(true);
        m_pWebSock->UnPauseRead();
        if (m_bBasic) {
            m_pWebSock->ReadLine("");
        } else {
            m_pWebSock->Redirect("/?cookie_check=true");
        }

        DEBUG("Successful login attempt ==> USER [" + User.GetUserName() +
              "] ==> SESSION [" + spSession->GetId() + "]");
    }
}

void CWebAuth::RefusedLogin(const CString& sReason) {
    if (m_pWebSock) {
        std::shared_ptr<CWebSession> spSession = m_pWebSock->GetSession();

        spSession->AddError("Invalid login!");
        spSession->SetUser(nullptr);

        m_pWebSock->SetLoggedIn(false);
        m_pWebSock->UnPauseRead();
        if (m_bBasic) {
            m_pWebSock->AddHeader("WWW-Authenticate", "Basic realm=\"ZNC\"");
            m_pWebSock->CHTTPSock::PrintErrorPage(
                401, "Unauthorized",
                "HTTP Basic authentication attempted with invalid credentials");
            // Why CWebSock makes this function protected?..
        } else {
            m_pWebSock->Redirect("/?cookie_check=true");
        }

        DEBUG("UNSUCCESSFUL login attempt ==> REASON [" + sReason +
              "] ==> SESSION [" + spSession->GetId() + "]");
    }
}

void CWebAuth::Invalidate() {
    CAuthBase::Invalidate();
    m_pWebSock = nullptr;
}

CWebSock::CWebSock(const CString& sURIPrefix)
    : CHTTPSock(nullptr, sURIPrefix),
      m_bPathsSet(false),
      m_Template(),
      m_spAuth(),
      m_sModName(""),
      m_sPath(""),
      m_sPage(""),
      m_spSession() {
    m_Template.AddTagHandler(std::make_shared<CZNCTagHandler>(*this));
}

CWebSock::~CWebSock() {
    if (m_spAuth) {
        m_spAuth->Invalidate();
    }

    // we have to account for traffic here because CSocket does
    // not have a valid CModule* pointer.
    CUser* pUser = GetSession()->GetUser();
    if (pUser) {
        pUser->AddBytesWritten(GetBytesWritten());
        pUser->AddBytesRead(GetBytesRead());
    } else {
        CZNC::Get().AddBytesWritten(GetBytesWritten());
        CZNC::Get().AddBytesRead(GetBytesRead());
    }

    // bytes have been accounted for, so make sure they don't get again:
    ResetBytesWritten();
    ResetBytesRead();
}

void CWebSock::GetAvailSkins(VCString& vRet) const {
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

        for (const CFile* pSubDir : Dir) {
            if (pSubDir->IsDir() && pSubDir->GetShortName() == "_default_") {
                vRet.push_back(pSubDir->GetShortName());
                break;
            }
        }

        for (const CFile* pSubDir : Dir) {
            if (pSubDir->IsDir() && pSubDir->GetShortName() != "_default_" &&
                pSubDir->GetShortName() != ".svn") {
                vRet.push_back(pSubDir->GetShortName());
            }
        }
    }
}

VCString CWebSock::GetDirs(CModule* pModule, bool bIsTemplate) {
    CString sHomeSkinsDir(CZNC::Get().GetZNCPath() + "/webskins/");
    CString sSkinName(GetSkinName());
    VCString vsResult;

    // Module specific paths

    if (pModule) {
        const CString& sModName(pModule->GetModName());

        // 1. ~/.znc/webskins/<user_skin_setting>/mods/<mod_name>/
        //
        if (!sSkinName.empty()) {
            vsResult.push_back(GetSkinPath(sSkinName) + "/mods/" + sModName +
                               "/");
        }

        // 2. ~/.znc/webskins/_default_/mods/<mod_name>/
        //
        vsResult.push_back(GetSkinPath("_default_") + "/mods/" + sModName +
                           "/");

        // 3. ./modules/<mod_name>/tmpl/
        //
        vsResult.push_back(pModule->GetModDataDir() + "/tmpl/");

        // 4. ~/.znc/webskins/<user_skin_setting>/mods/<mod_name>/
        //
        if (!sSkinName.empty()) {
            vsResult.push_back(GetSkinPath(sSkinName) + "/mods/" + sModName +
                               "/");
        }

        // 5. ~/.znc/webskins/_default_/mods/<mod_name>/
        //
        vsResult.push_back(GetSkinPath("_default_") + "/mods/" + sModName +
                           "/");
    }

    // 6. ~/.znc/webskins/<user_skin_setting>/
    //
    if (!sSkinName.empty()) {
        vsResult.push_back(GetSkinPath(sSkinName) +
                           CString(bIsTemplate ? "/tmpl/" : "/"));
    }

    // 7. ~/.znc/webskins/_default_/
    //
    vsResult.push_back(GetSkinPath("_default_") +
                       CString(bIsTemplate ? "/tmpl/" : "/"));

    return vsResult;
}

CString CWebSock::FindTmpl(CModule* pModule, const CString& sName) {
    VCString vsDirs = GetDirs(pModule, true);
    CString sFile = pModule->GetModName() + "_" + sName;
    for (const CString& sDir : vsDirs) {
        if (CFile::Exists(CDir::ChangeDir(sDir, sFile))) {
            m_Template.AppendPath(sDir);
            return sFile;
        }
    }
    return sName;
}

void CWebSock::SetPaths(CModule* pModule, bool bIsTemplate) {
    m_Template.ClearPaths();

    VCString vsDirs = GetDirs(pModule, bIsTemplate);
    for (const CString& sDir : vsDirs) {
        m_Template.AppendPath(sDir);
    }

    m_bPathsSet = true;
}

void CWebSock::SetVars() {
    m_Template["SessionUser"] = GetUser();
    m_Template["SessionIP"] = GetRemoteIP();
    m_Template["Tag"] = CZNC::GetTag(GetSession()->GetUser() != nullptr, true);
    m_Template["Version"] = CZNC::GetVersion();
    m_Template["SkinName"] = GetSkinName();
    m_Template["_CSRF_Check"] = GetCSRFCheck();
    m_Template["URIPrefix"] = GetURIPrefix();

    if (GetSession()->IsAdmin()) {
        m_Template["IsAdmin"] = "true";
    }

    GetSession()->FillMessageLoops(m_Template);
    GetSession()->ClearMessageLoops();

    // Global Mods
    CModules& vgMods = CZNC::Get().GetModules();
    for (CModule* pgMod : vgMods) {
        AddModLoop("GlobalModLoop", *pgMod);
    }

    // User Mods
    if (IsLoggedIn()) {
        CModules& vMods = GetSession()->GetUser()->GetModules();

        for (CModule* pMod : vMods) {
            AddModLoop("UserModLoop", *pMod);
        }

        vector<CIRCNetwork*> vNetworks = GetSession()->GetUser()->GetNetworks();
        for (CIRCNetwork* pNetwork : vNetworks) {
            CModules& vnMods = pNetwork->GetModules();

            CTemplate& Row = m_Template.AddRow("NetworkModLoop");
            Row["NetworkName"] = pNetwork->GetName();

            for (CModule* pnMod : vnMods) {
                AddModLoop("ModLoop", *pnMod, &Row);
            }
        }
    }

    if (IsLoggedIn()) {
        m_Template["LoggedIn"] = "true";
    }
}

bool CWebSock::AddModLoop(const CString& sLoopName, CModule& Module,
                          CTemplate* pTemplate) {
    if (!pTemplate) {
        pTemplate = &m_Template;
    }

    CString sTitle(Module.GetWebMenuTitle());

    if (!sTitle.empty() && (IsLoggedIn() || (!Module.WebRequiresLogin() &&
                                             !Module.WebRequiresAdmin())) &&
        (GetSession()->IsAdmin() || !Module.WebRequiresAdmin())) {
        CTemplate& Row = pTemplate->AddRow(sLoopName);
        bool bActiveModule = false;

        Row["ModName"] = Module.GetModName();
        Row["ModPath"] = Module.GetWebPath();
        Row["Title"] = sTitle;

        if (m_sModName == Module.GetModName()) {
            CString sModuleType = GetPath().Token(1, false, "/");
            if (sModuleType == "global" &&
                Module.GetType() == CModInfo::GlobalModule) {
                bActiveModule = true;
            } else if (sModuleType == "user" &&
                       Module.GetType() == CModInfo::UserModule) {
                bActiveModule = true;
            } else if (sModuleType == "network" &&
                       Module.GetType() == CModInfo::NetworkModule) {
                CIRCNetwork* Network = Module.GetNetwork();
                if (Network) {
                    CString sNetworkName = GetPath().Token(2, false, "/");
                    if (sNetworkName == Network->GetName()) {
                        bActiveModule = true;
                    }
                } else {
                    bActiveModule = true;
                }
            }
        }

        if (bActiveModule) {
            Row["Active"] = "true";
        }

        if (Module.GetUser()) {
            Row["Username"] = Module.GetUser()->GetUserName();
        }

        VWebSubPages& vSubPages = Module.GetSubPages();

        for (TWebSubPage& SubPage : vSubPages) {
            // bActive is whether or not the current url matches this subpage
            // (params will be checked below)
            bool bActive = (m_sModName == Module.GetModName() &&
                            m_sPage == SubPage->GetName() && bActiveModule);

            if (SubPage->RequiresAdmin() && !GetSession()->IsAdmin()) {
                // Don't add admin-only subpages to requests from non-admin
                // users
                continue;
            }

            CTemplate& SubRow = Row.AddRow("SubPageLoop");

            SubRow["ModName"] = Module.GetModName();
            SubRow["ModPath"] = Module.GetWebPath();
            SubRow["PageName"] = SubPage->GetName();
            SubRow["Title"] = SubPage->GetTitle().empty() ? SubPage->GetName()
                                                          : SubPage->GetTitle();

            CString& sParams = SubRow["Params"];

            const VPair& vParams = SubPage->GetParams();
            for (const pair<CString, CString>& ssNV : vParams) {
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

CWebSock::EPageReqResult CWebSock::PrintStaticFile(const CString& sPath,
                                                   CString& sPageRet,
                                                   CModule* pModule) {
    SetPaths(pModule);
    CString sFile = m_Template.ExpandFile(sPath.TrimLeft_n("/"));
    DEBUG("About to print [" + sFile + "]");
    // Either PrintFile() fails and sends an error page or it succeeds and
    // sends a result. In both cases we don't have anything more to do.
    PrintFile(sFile);
    return PAGE_DONE;
}

CWebSock::EPageReqResult CWebSock::PrintTemplate(const CString& sPageName,
                                                 CString& sPageRet,
                                                 CModule* pModule) {
    SetVars();

    m_Template["PageName"] = sPageName;

    if (pModule) {
        m_Template["ModName"] = pModule->GetModName();

        if (m_Template.find("Title") == m_Template.end()) {
            m_Template["Title"] = pModule->GetWebMenuTitle();
        }

        std::vector<CTemplate*>* breadcrumbs =
            m_Template.GetLoop("BreadCrumbs");
        if (breadcrumbs->size() == 1 &&
            m_Template["Title"] != pModule->GetModName()) {
            // Module didn't add its own breadcrumbs, so add a generic one...
            // But it'll be useless if it's the same as module name
            CTemplate& bread = m_Template.AddRow("BreadCrumbs");
            bread["Text"] = m_Template["Title"];
        }
    }

    if (!m_bPathsSet) {
        SetPaths(pModule, true);
    }

    if (m_Template.GetFileName().empty() &&
        !m_Template.SetFile(sPageName + ".tmpl")) {
        return PAGE_NOTFOUND;
    }

    if (m_Template.PrintString(sPageRet)) {
        return PAGE_PRINT;
    } else {
        return PAGE_NOTFOUND;
    }
}

CString CWebSock::GetSkinPath(const CString& sSkinName) {
    const CString sSkin = sSkinName.Replace_n("/", "_").Replace_n(".", "_");

    CString sRet = CZNC::Get().GetZNCPath() + "/webskins/" + sSkin;

    if (!CFile::IsDir(sRet)) {
        sRet = CZNC::Get().GetCurPath() + "/webskins/" + sSkin;

        if (!CFile::IsDir(sRet)) {
            sRet = CString(_SKINDIR_) + "/" + sSkin;
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

CString CWebSock::GetRequestCookie(const CString& sKey) {
    const CString sPrefixedKey = CString(GetLocalPort()) + "-" + sKey;
    CString sRet;

    if (!m_sModName.empty()) {
        sRet = CHTTPSock::GetRequestCookie("Mod-" + m_sModName + "-" +
                                           sPrefixedKey);
    }

    if (sRet.empty()) {
        return CHTTPSock::GetRequestCookie(sPrefixedKey);
    }

    return sRet;
}

bool CWebSock::SendCookie(const CString& sKey, const CString& sValue) {
    const CString sPrefixedKey = CString(GetLocalPort()) + "-" + sKey;

    if (!m_sModName.empty()) {
        return CHTTPSock::SendCookie("Mod-" + m_sModName + "-" + sPrefixedKey,
                                     sValue);
    }

    return CHTTPSock::SendCookie(sPrefixedKey, sValue);
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
        case PAGE_NOTFOUND:
        default:
            PrintNotFound();
            break;
    }
}

CWebSock::EPageReqResult CWebSock::OnPageRequestInternal(const CString& sURI,
                                                         CString& sPageRet) {
    // Check that their session really belongs to their IP address. IP-based
    // authentication is bad, but here it's just an extra layer that makes
    // stealing cookies harder to pull off.
    //
    // When their IP is wrong, we give them an invalid cookie. This makes
    // sure that they will get a new cookie on their next request.
    if (CZNC::Get().GetProtectWebSessions() &&
        GetSession()->GetIP() != GetRemoteIP()) {
        DEBUG("Expected IP: " << GetSession()->GetIP());
        DEBUG("Remote IP:   " << GetRemoteIP());
        SendCookie("SessionId", "WRONG_IP_FOR_SESSION");
        PrintErrorPage(403, "Access denied",
                       "This session does not belong to your IP.");
        return PAGE_DONE;
    }

    // For pages *not provided* by modules, a CSRF check is performed which involves:
    // Ensure that they really POSTed from one our forms by checking if they
    // know the "secret" CSRF check value. Don't do this for login since
    // CSRF against the login form makes no sense and the login form does a
    // cookies-enabled check which would break otherwise.
    // Don't do this, if user authenticated using http-basic auth, because:
    // 1. they obviously know the password,
    // 2. it's easier to automate some tasks e.g. user creation, without need to
    //    care about cookies and CSRF
    if (IsPost() && !m_bBasicAuth && !sURI.StartsWith("/mods/") &&
        !ValidateCSRFCheck(sURI)) {
        DEBUG("Expected _CSRF_Check: " << GetCSRFCheck());
        DEBUG("Actual _CSRF_Check:   " << GetParam("_CSRF_Check"));
        PrintErrorPage(
            403, "Access denied",
            "POST requests need to send "
            "a secret token to prevent cross-site request forgery attacks.");
        return PAGE_DONE;
    }

    SendCookie("SessionId", GetSession()->GetId());

    if (GetSession()->IsLoggedIn()) {
        m_sUser = GetSession()->GetUser()->GetUserName();
        m_bLoggedIn = true;
    }
    CLanguageScope user_language(
        m_bLoggedIn ? GetSession()->GetUser()->GetLanguage() : "");

    // Handle the static pages that don't require a login
    if (sURI == "/") {
        if (!m_bLoggedIn && GetParam("cookie_check", false).ToBool() &&
            GetRequestCookie("SessionId").empty()) {
            GetSession()->AddError(
                "Your browser does not have cookies enabled for this site!");
        }
        return PrintTemplate("index", sPageRet);
    } else if (sURI == "/favicon.ico") {
        return PrintStaticFile("/pub/favicon.ico", sPageRet);
    } else if (sURI == "/robots.txt") {
        return PrintStaticFile("/pub/robots.txt", sPageRet);
    } else if (sURI == "/logout") {
        GetSession()->SetUser(nullptr);
        SetLoggedIn(false);
        Redirect("/");

        // We already sent a reply
        return PAGE_DONE;
    } else if (sURI == "/login") {
        if (GetParam("submitted").ToBool()) {
            m_sUser = GetParam("user");
            m_sPass = GetParam("pass");
            m_bLoggedIn = OnLogin(m_sUser, m_sPass, false);

            // AcceptedLogin()/RefusedLogin() will call Redirect()
            return PAGE_DEFERRED;
        }

        Redirect("/");  // the login form is here
        return PAGE_DONE;
    } else if (sURI.StartsWith("/pub/")) {
        return PrintStaticFile(sURI, sPageRet);
    } else if (sURI.StartsWith("/skinfiles/")) {
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
    } else if (sURI.StartsWith("/mods/") || sURI.StartsWith("/modfiles/")) {
        // Make sure modules are treated as directories
        if (!sURI.EndsWith("/") && !sURI.Contains(".") &&
            !sURI.TrimLeft_n("/mods/").TrimLeft_n("/").Contains("/")) {
            Redirect(sURI + "/");
            return PAGE_DONE;
        }

        // The URI looks like:
        // /mods/[type]/([network]/)?[module][/page][?arg1=val1&arg2=val2...]

        m_sPath = GetPath().TrimLeft_n("/");

        m_sPath.TrimPrefix("mods/");
        m_sPath.TrimPrefix("modfiles/");

        CString sType = m_sPath.Token(0, false, "/");
        m_sPath = m_sPath.Token(1, true, "/");

        CModInfo::EModuleType eModType;
        if (sType.Equals("global")) {
            eModType = CModInfo::GlobalModule;
        } else if (sType.Equals("user")) {
            eModType = CModInfo::UserModule;
        } else if (sType.Equals("network")) {
            eModType = CModInfo::NetworkModule;
        } else {
            PrintErrorPage(403, "Forbidden",
                           "Unknown module type [" + sType + "]");
            return PAGE_DONE;
        }

        if ((eModType != CModInfo::GlobalModule) && !ForceLogin()) {
            // Make sure we have a valid user
            return PAGE_DONE;
        }

        CIRCNetwork* pNetwork = nullptr;
        if (eModType == CModInfo::NetworkModule) {
            CString sNetwork = m_sPath.Token(0, false, "/");
            m_sPath = m_sPath.Token(1, true, "/");

            pNetwork = GetSession()->GetUser()->FindNetwork(sNetwork);

            if (!pNetwork) {
                PrintErrorPage(404, "Not Found",
                               "Network [" + sNetwork + "] not found.");
                return PAGE_DONE;
            }
        }

        m_sModName = m_sPath.Token(0, false, "/");
        m_sPage = m_sPath.Token(1, true, "/");

        if (m_sPage.empty()) {
            m_sPage = "index";
        }

        DEBUG("Path [" + m_sPath + "], Module [" + m_sModName + "], Page [" +
              m_sPage + "]");

        CModule* pModule = nullptr;

        switch (eModType) {
            case CModInfo::GlobalModule:
                pModule = CZNC::Get().GetModules().FindModule(m_sModName);
                break;
            case CModInfo::UserModule:
                pModule = GetSession()->GetUser()->GetModules().FindModule(
                    m_sModName);
                break;
            case CModInfo::NetworkModule:
                pModule = pNetwork->GetModules().FindModule(m_sModName);
                break;
        }

        if (!pModule) return PAGE_NOTFOUND;

        // Pass CSRF check to module.
        // Note that the normal CSRF checks are not applied to /mods/ URLs.
        if (IsPost() && !m_bBasicAuth &&
            !pModule->ValidateWebRequestCSRFCheck(*this, m_sPage)) {
            DEBUG("Expected _CSRF_Check: " << GetCSRFCheck());
            DEBUG("Actual _CSRF_Check:   " << GetParam("_CSRF_Check"));
            PrintErrorPage(
                403, "Access denied",
                "POST requests need to send "
                "a secret token to prevent cross-site request forgery attacks.");
            return PAGE_DONE;
        }

        m_Template["ModPath"] = pModule->GetWebPath();
        m_Template["ModFilesPath"] = pModule->GetWebFilesPath();

        if (pModule->WebRequiresLogin() && !ForceLogin()) {
            return PAGE_PRINT;
        } else if (pModule->WebRequiresAdmin() && !GetSession()->IsAdmin()) {
            PrintErrorPage(403, "Forbidden",
                           "You need to be an admin to access this module");
            return PAGE_DONE;
        } else if (pModule->GetType() != CModInfo::GlobalModule &&
                   pModule->GetUser() != GetSession()->GetUser()) {
            PrintErrorPage(403, "Forbidden",
                           "You must login as " +
                               pModule->GetUser()->GetUserName() +
                               " in order to view this page");
            return PAGE_DONE;
        } else if (pModule->OnWebPreRequest(*this, m_sPage)) {
            return PAGE_DEFERRED;
        }

        VWebSubPages& vSubPages = pModule->GetSubPages();

        for (TWebSubPage& SubPage : vSubPages) {
            bool bActive = (m_sModName == pModule->GetModName() &&
                            m_sPage == SubPage->GetName());

            if (bActive && SubPage->RequiresAdmin() &&
                !GetSession()->IsAdmin()) {
                PrintErrorPage(403, "Forbidden",
                               "You need to be an admin to access this page");
                return PAGE_DONE;
            }
        }

        if (pModule && pModule->GetType() != CModInfo::GlobalModule &&
            (!IsLoggedIn() || pModule->GetUser() != GetSession()->GetUser())) {
            AddModLoop("UserModLoop", *pModule);
        }

        if (sURI.StartsWith("/modfiles/")) {
            m_Template.AppendPath(GetSkinPath(GetSkinName()) + "/mods/" +
                                  m_sModName + "/files/");
            m_Template.AppendPath(pModule->GetModDataDir() + "/files/");

            if (PrintFile(m_Template.ExpandFile(m_sPage.TrimLeft_n("/")))) {
                return PAGE_PRINT;
            } else {
                return PAGE_NOTFOUND;
            }
        } else {
            SetPaths(pModule, true);

            CTemplate& breadModule = m_Template.AddRow("BreadCrumbs");
            breadModule["Text"] = pModule->GetModName();
            breadModule["URL"] = pModule->GetWebPath();

            /* if a module returns false from OnWebRequest, it does not
               want the template to be printed, usually because it did a
               redirect. */
            if (pModule->OnWebRequest(*this, m_sPage, m_Template)) {
                // If they already sent a reply, let's assume
                // they did what they wanted to do.
                if (SentHeader()) {
                    return PAGE_DONE;
                }
                return PrintTemplate(m_sPage, sPageRet, pModule);
            }

            if (!SentHeader()) {
                PrintErrorPage(
                    404, "Not Implemented",
                    "The requested module does not acknowledge web requests");
            }
            return PAGE_DONE;
        }
    } else {
        CString sPage(sURI.Trim_n("/"));
        if (sPage.length() < 32) {
            for (unsigned int a = 0; a < sPage.length(); a++) {
                unsigned char c = sPage[a];

                if ((c < '0' || c > '9') && (c < 'a' || c > 'z') &&
                    (c < 'A' || c > 'Z') && c != '_') {
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

static inline bool compareLastActive(
    const std::pair<const CString, CWebSession*>& first,
    const std::pair<const CString, CWebSession*>& second) {
    return first.second->GetLastActive() < second.second->GetLastActive();
}

std::shared_ptr<CWebSession> CWebSock::GetSession() {
    if (m_spSession) {
        return m_spSession;
    }

    const CString sCookieSessionId = GetRequestCookie("SessionId");
    std::shared_ptr<CWebSession>* pSession =
        Sessions.m_mspSessions.GetItem(sCookieSessionId);

    if (pSession != nullptr) {
        // Refresh the timeout
        Sessions.m_mspSessions.AddItem((*pSession)->GetId(), *pSession);
        (*pSession)->UpdateLastActive();
        m_spSession = *pSession;
        DEBUG("Found existing session from cookie: [" + sCookieSessionId +
              "] IsLoggedIn(" +
              CString((*pSession)->IsLoggedIn()
                          ? "true, " + ((*pSession)->GetUser()->GetUserName())
                          : "false") +
              ")");
        return *pSession;
    }

    if (Sessions.m_mIPSessions.count(GetRemoteIP()) > m_uiMaxSessions) {
        pair<mIPSessionsIterator, mIPSessionsIterator> p =
            Sessions.m_mIPSessions.equal_range(GetRemoteIP());
        mIPSessionsIterator it =
            std::min_element(p.first, p.second, compareLastActive);
        DEBUG("Remote IP:   " << GetRemoteIP() << "; discarding session ["
                              << it->second->GetId() << "]");
        Sessions.m_mspSessions.RemItem(it->second->GetId());
    }

    CString sSessionID;
    do {
        sSessionID = CString::RandomString(32);
        sSessionID += ":" + GetRemoteIP() + ":" + CString(GetRemotePort());
        sSessionID += ":" + GetLocalIP() + ":" + CString(GetLocalPort());
        sSessionID += ":" + CString(time(nullptr));
        sSessionID = sSessionID.SHA256();

        DEBUG("Auto generated session: [" + sSessionID + "]");
    } while (Sessions.m_mspSessions.HasItem(sSessionID));

    std::shared_ptr<CWebSession> spSession(
        new CWebSession(sSessionID, GetRemoteIP()));
    Sessions.m_mspSessions.AddItem(spSession->GetId(), spSession);

    m_spSession = spSession;

    return spSession;
}

CString CWebSock::GetCSRFCheck() {
    std::shared_ptr<CWebSession> pSession = GetSession();
    return pSession->GetId().MD5();
}

bool CWebSock::ValidateCSRFCheck(const CString& sURI) {
    return sURI == "/login" || GetParam("_CSRF_Check") == GetCSRFCheck();
}

bool CWebSock::OnLogin(const CString& sUser, const CString& sPass,
                       bool bBasic) {
    DEBUG("=================== CWebSock::OnLogin(), basic auth? "
          << std::boolalpha << bBasic);
    m_spAuth = std::make_shared<CWebAuth>(this, sUser, sPass, bBasic);

    // Some authentication module could need some time, block this socket
    // until then. CWebAuth will UnPauseRead().
    PauseRead();
    CZNC::Get().AuthUser(m_spAuth);

    // If CWebAuth already set this, don't change it.
    return IsLoggedIn();
}

Csock* CWebSock::GetSockObj(const CString& sHost, unsigned short uPort) {
    // All listening is done by CListener, thus CWebSock should never have
    // to listen, but since GetSockObj() is pure virtual...
    DEBUG("CWebSock::GetSockObj() called - this should never happen!");
    return nullptr;
}

CString CWebSock::GetSkinName() {
    std::shared_ptr<CWebSession> spSession = GetSession();

    if (spSession->IsLoggedIn() &&
        !spSession->GetUser()->GetSkinName().empty()) {
        return spSession->GetUser()->GetSkinName();
    }

    return CZNC::Get().GetSkinName();
}
