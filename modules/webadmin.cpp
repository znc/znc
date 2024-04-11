/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

#include <znc/Chan.h>
#include <znc/Server.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

using std::stringstream;
using std::make_pair;
using std::set;
using std::vector;
using std::map;

/* Stuff to be able to write this:
   // i will be name of local variable, see below
   // pUser can be nullptr if only global modules are needed
   FOR_EACH_MODULE(i, pUser) {
       // i is local variable of type CModules::iterator,
       // so *i has type CModule*
   }
*/
struct FOR_EACH_MODULE_Type {
    enum {
        AtGlobal,
        AtUser,
        AtNetwork,
    } where;
    CModules CMtemp;
    CModules& CMuser;
    CModules& CMnet;
    FOR_EACH_MODULE_Type(CUser* pUser)
        : CMuser(pUser ? pUser->GetModules() : CMtemp), CMnet(CMtemp) {
        where = AtGlobal;
    }
    FOR_EACH_MODULE_Type(CIRCNetwork* pNetwork)
        : CMuser(pNetwork ? pNetwork->GetUser()->GetModules() : CMtemp),
          CMnet(pNetwork ? pNetwork->GetModules() : CMtemp) {
        where = AtGlobal;
    }
    FOR_EACH_MODULE_Type(std::pair<CUser*, CIRCNetwork*> arg)
        : CMuser(arg.first ? arg.first->GetModules() : CMtemp),
          CMnet(arg.second ? arg.second->GetModules() : CMtemp) {
        where = AtGlobal;
    }
    operator bool() { return false; }
};

inline bool FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Type& state,
                                        CModules::iterator& i) {
    if (state.where == FOR_EACH_MODULE_Type::AtGlobal &&
        i == CZNC::Get().GetModules().end()) {
        i = state.CMuser.begin();
        state.where = FOR_EACH_MODULE_Type::AtUser;
    }
    if (state.where == FOR_EACH_MODULE_Type::AtUser &&
        i == state.CMuser.end()) {
        i = state.CMnet.begin();
        state.where = FOR_EACH_MODULE_Type::AtNetwork;
    }
    return !(state.where == FOR_EACH_MODULE_Type::AtNetwork &&
             i == state.CMnet.end());
}

#define FOR_EACH_MODULE(I, pUserOrNetwork)                            \
    if (FOR_EACH_MODULE_Type FOR_EACH_MODULE_Var = pUserOrNetwork) {  \
    } else                                                            \
        for (CModules::iterator I = CZNC::Get().GetModules().begin(); \
             FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Var, I); ++I)

class CWebAdminMod : public CModule {
  public:
    MODCONSTRUCTOR(CWebAdminMod) {
        VPair vParams;
        vParams.push_back(make_pair("user", ""));
        AddSubPage(std::make_shared<CWebSubPage>(
            "settings", t_d("Global Settings"), vParams, CWebSubPage::F_ADMIN));
        AddSubPage(std::make_shared<CWebSubPage>(
            "edituser", t_d("Your Settings"), vParams));
        AddSubPage(std::make_shared<CWebSubPage>("traffic", t_d("Traffic Info"),
                                                 vParams));
        AddSubPage(std::make_shared<CWebSubPage>(
            "listusers", t_d("Manage Users"), vParams, CWebSubPage::F_ADMIN));
    }

    ~CWebAdminMod() override {}

    bool OnLoad(const CString& sArgStr, CString& sMessage) override {
        if (sArgStr.empty() || CModInfo::GlobalModule != GetType()) return true;

        // We don't accept any arguments, but for backwards
        // compatibility we have to do some magic here.
        sMessage = "Arguments converted to new syntax";

        bool bSSL = false;
        bool bIPv6 = false;
        bool bShareIRCPorts = true;
        unsigned short uPort = 8080;
        CString sArgs(sArgStr);
        CString sPort;
        CString sListenHost;
        CString sURIPrefix;

        while (sArgs.Left(1) == "-") {
            CString sOpt = sArgs.Token(0);
            sArgs = sArgs.Token(1, true);

            if (sOpt.Equals("-IPV6")) {
                bIPv6 = true;
            } else if (sOpt.Equals("-IPV4")) {
                bIPv6 = false;
            } else if (sOpt.Equals("-noircport")) {
                bShareIRCPorts = false;
            } else {
                // Uhm... Unknown option? Let's just ignore all
                // arguments, older versions would have returned
                // an error and denied loading
                return true;
            }
        }

        // No arguments left: Only port sharing
        if (sArgs.empty() && bShareIRCPorts) return true;

        if (sArgs.find(" ") != CString::npos) {
            sListenHost = sArgs.Token(0);
            sPort = sArgs.Token(1, true);
        } else {
            sPort = sArgs;
        }

        if (sPort.Left(1) == "+") {
            sPort.TrimLeft("+");
            bSSL = true;
        }

        if (!sPort.empty()) {
            uPort = sPort.ToUShort();
        }

        if (!bShareIRCPorts) {
            // Make all existing listeners IRC-only
            const vector<CListener*>& vListeners = CZNC::Get().GetListeners();
            for (CListener* pListener : vListeners) {
                pListener->SetAcceptType(CListener::ACCEPT_IRC);
            }
        }

        // Now turn that into a listener instance
        CListener* pListener = new CListener(
            uPort, sListenHost, sURIPrefix, bSSL,
            (!bIPv6 ? ADDR_IPV4ONLY : ADDR_ALL), CListener::ACCEPT_HTTP);

        if (!pListener->Listen()) {
            sMessage = "Failed to add backwards-compatible listener";
            return false;
        }
        CZNC::Get().AddListener(pListener);

        SetArgs("");
        return true;
    }

    CUser* GetNewUser(CWebSock& WebSock, CUser* pUser) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        CString sUsername = WebSock.GetParam("newuser");

        if (sUsername.empty()) {
            sUsername = WebSock.GetParam("user");
        }

        if (sUsername.empty()) {
            WebSock.PrintErrorPage(
                t_s("Invalid Submission [Username is required]"));
            return nullptr;
        }

        if (pUser) {
            /* If we are editing a user we must not change the user name */
            sUsername = pUser->GetUsername();
        }

        CString sArg = WebSock.GetParam("password");

        if (sArg != WebSock.GetParam("password2")) {
            WebSock.PrintErrorPage(
                t_s("Invalid Submission [Passwords do not match]"));
            return nullptr;
        }

        CUser* pNewUser = new CUser(sUsername);

        if (!sArg.empty()) {
            CString sSalt = CUtils::GetSalt();
            CString sHash = CUser::SaltedHash(sArg, sSalt);
            pNewUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
        }

        sArg = WebSock.GetParam("authonlyviamodule");
        if (spSession->IsAdmin()) {
            if (!sArg.empty()) {
                pNewUser->SetAuthOnlyViaModule(sArg.ToBool());
            }
        } else if (pUser) {
            pNewUser->SetAuthOnlyViaModule(pUser->AuthOnlyViaModule());
        }

        VCString vsArgs;

        WebSock.GetRawParam("allowedips").Split("\n", vsArgs);
        if (vsArgs.size()) {
            for (const CString& sHost : vsArgs) {
                pNewUser->AddAllowedHost(sHost.Trim_n());
            }
        } else {
            pNewUser->AddAllowedHost("*");
        }

        WebSock.GetRawParam("ctcpreplies").Split("\n", vsArgs);
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetCTCPReplies()) {
            for (const CString& sReply : vsArgs) {
                pNewUser->AddCTCPReply(sReply.Token(0).Trim_n(),
                                    sReply.Token(1, true).Trim_n());
            }
        }

        sArg = WebSock.GetParam("nick");
        if (!sArg.empty()) {
            pNewUser->SetNick(sArg);
        }
        sArg = WebSock.GetParam("altnick");
        if (!sArg.empty()) {
            pNewUser->SetAltNick(sArg);
        }
        sArg = WebSock.GetParam("statusprefix");
        if (!sArg.empty()) {
            pNewUser->SetStatusPrefix(sArg);
        }
        sArg = WebSock.GetParam("ident");
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetIdent()) {
            if (!sArg.empty()) {
                pNewUser->SetIdent(sArg);
            }
        } else if (pUser) {
            pNewUser->SetIdent(pUser->GetIdent());
        }
        sArg = WebSock.GetParam("realname");
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetRealName()) {
            if (!sArg.empty()) {
                pNewUser->SetRealName(sArg);
            }
        } else if (pUser) {
            pNewUser->SetRealName(pUser->GetRealName());
        }
        sArg = WebSock.GetParam("quitmsg");
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetQuitMsg()) {
            if (!sArg.empty()) {
                pNewUser->SetQuitMsg(sArg);
            }
        } else if (pUser) {
            pNewUser->SetQuitMsg(pUser->GetQuitMsg());
        }
        sArg = WebSock.GetParam("chanmodes");
        if (!sArg.empty()) {
            pNewUser->SetDefaultChanModes(sArg);
        }
        sArg = WebSock.GetParam("timestampformat");
        if (!sArg.empty()) {
            pNewUser->SetTimestampFormat(sArg);
        }

        sArg = WebSock.GetParam("bindhost");
        // To change BindHosts be admin or don't have DenySetBindHost
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
            CString sArg2 = WebSock.GetParam("dccbindhost");
            if (!sArg.empty()) {
                pNewUser->SetBindHost(sArg);
            }
            if (!sArg2.empty()) {
                pNewUser->SetDCCBindHost(sArg2);
            }
        } else if (pUser) {
            pNewUser->SetBindHost(pUser->GetBindHost());
            pNewUser->SetDCCBindHost(pUser->GetDCCBindHost());
        }

        sArg = WebSock.GetParam("chanbufsize");
        if (!sArg.empty()) {
            // First apply the old limit in case the new one is too high
            if (pUser)
                pNewUser->SetChanBufferSize(pUser->GetChanBufferSize(), true);
            pNewUser->SetChanBufferSize(sArg.ToUInt(), spSession->IsAdmin());
        }

        sArg = WebSock.GetParam("querybufsize");
        if (!sArg.empty()) {
            // First apply the old limit in case the new one is too high
            if (pUser)
                pNewUser->SetQueryBufferSize(pUser->GetQueryBufferSize(), true);
            pNewUser->SetQueryBufferSize(sArg.ToUInt(), spSession->IsAdmin());
        }

        pNewUser->SetSkinName(WebSock.GetParam("skin"));
        pNewUser->SetAutoClearChanBuffer(
            WebSock.GetParam("autoclearchanbuffer").ToBool());
        pNewUser->SetMultiClients(WebSock.GetParam("multiclients").ToBool());
        pNewUser->SetTimestampAppend(
            WebSock.GetParam("appendtimestamp").ToBool());
        pNewUser->SetTimestampPrepend(
            WebSock.GetParam("prependtimestamp").ToBool());
        pNewUser->SetTimezone(WebSock.GetParam("timezone"));
        pNewUser->SetJoinTries(WebSock.GetParam("jointries").ToUInt());
        pNewUser->SetMaxJoins(WebSock.GetParam("maxjoins").ToUInt());
        pNewUser->SetAutoClearQueryBuffer(
            WebSock.GetParam("autoclearquerybuffer").ToBool());
        pNewUser->SetMaxQueryBuffers(
            WebSock.GetParam("maxquerybuffers").ToUInt());
        unsigned int uNoTrafficTimeout =
            WebSock.GetParam("notraffictimeout").ToUInt();
        if (uNoTrafficTimeout < 30) {
            uNoTrafficTimeout = 30;
            WebSock.GetSession()->AddError(
                t_s("Timeout can't be less than 30 seconds!"));
        }
        pNewUser->SetNoTrafficTimeout(uNoTrafficTimeout);

#ifdef HAVE_I18N
        CString sLanguage = WebSock.GetParam("language");
        if (CTranslationInfo::GetTranslations().count(sLanguage) == 0) {
            sLanguage = "";
        }
        pNewUser->SetLanguage(sLanguage);
#endif
#ifdef HAVE_ICU
        CString sEncodingUtf = WebSock.GetParam("encoding_utf");
        if (sEncodingUtf == "legacy") {
            pNewUser->SetClientEncoding("");
        }
        CString sEncoding = WebSock.GetParam("encoding");
        if (sEncoding.empty()) {
            sEncoding = "UTF-8";
        }
        if (sEncodingUtf == "send") {
            pNewUser->SetClientEncoding("^" + sEncoding);
        } else if (sEncodingUtf == "receive") {
            pNewUser->SetClientEncoding("*" + sEncoding);
        } else if (sEncodingUtf == "simple") {
            pNewUser->SetClientEncoding(sEncoding);
        }
#endif

        if (spSession->IsAdmin()) {
            pNewUser->SetDenyLoadMod(WebSock.GetParam("denyloadmod").ToBool());
            pNewUser->SetDenySetBindHost(
                WebSock.GetParam("denysetbindhost").ToBool());
            pNewUser->SetDenySetIdent(
                WebSock.GetParam("denysetident").ToBool());
            pNewUser->SetDenySetNetwork(
                WebSock.GetParam("denysetnetwork").ToBool());
            pNewUser->SetDenySetRealName(
                WebSock.GetParam("denysetrealname").ToBool());
            pNewUser->SetDenySetQuitMsg(
                WebSock.GetParam("denysetquitmsg").ToBool());
            pNewUser->SetDenySetCTCPReplies(
                WebSock.GetParam("denysetctcpreplies").ToBool());
            pNewUser->SetAuthOnlyViaModule(
                WebSock.GetParam("authonlyviamodule").ToBool());
            sArg = WebSock.GetParam("maxnetworks");
            if (!sArg.empty()) pNewUser->SetMaxNetworks(sArg.ToUInt());
        } else if (pUser) {
            pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
            pNewUser->SetDenySetBindHost(pUser->DenySetBindHost());
            pNewUser->SetDenySetIdent(pUser->DenySetIdent());
            pNewUser->SetDenySetNetwork(pUser->DenySetNetwork());
            pNewUser->SetDenySetRealName(pUser->DenySetRealName());
            pNewUser->SetDenySetQuitMsg(pUser->DenySetQuitMsg());
            pNewUser->SetDenySetCTCPReplies(pUser->DenySetCTCPReplies());
            pNewUser->SetAuthOnlyViaModule(pUser->AuthOnlyViaModule());
            pNewUser->SetMaxNetworks(pUser->MaxNetworks());
        }

        // If pUser is not nullptr, we are editing an existing user.
        // Users must not be able to change their own admin flag.
        if (pUser != CZNC::Get().FindUser(WebSock.GetUser())) {
            pNewUser->SetAdmin(WebSock.GetParam("isadmin").ToBool());
        } else if (pUser) {
            pNewUser->SetAdmin(pUser->IsAdmin());
        }

        if (spSession->IsAdmin() || (pUser && !pUser->DenyLoadMod())) {
            WebSock.GetParamValues("loadmod", vsArgs);

            // disallow unload webadmin from itself
            if (CModInfo::UserModule == GetType() &&
                pUser == CZNC::Get().FindUser(WebSock.GetUser())) {
                bool bLoadedWebadmin = false;
                for (const CString& s : vsArgs) {
                    CString sModName = s.TrimRight_n("\r");
                    if (sModName == GetModName()) {
                        bLoadedWebadmin = true;
                        break;
                    }
                }
                if (!bLoadedWebadmin) {
                    vsArgs.push_back(GetModName());
                }
            }

            for (const CString& s : vsArgs) {
                CString sModRet;
                CString sModName = s.TrimRight_n("\r");
                CString sModLoadError;

                if (!sModName.empty()) {
                    CString sArgs = WebSock.GetParam("modargs_" + sModName);

                    try {
                        if (!pNewUser->GetModules().LoadModule(
                                sModName, sArgs, CModInfo::UserModule, pNewUser,
                                nullptr, sModRet)) {
                            sModLoadError =
                                t_f("Unable to load module [{1}]: {2}")(
                                    sModName, sModRet);
                        }
                    } catch (...) {
                        sModLoadError = t_f(
                            "Unable to load module [{1}] with arguments [{2}]")(
                            sModName, sArgs);
                    }

                    if (!sModLoadError.empty()) {
                        DEBUG(sModLoadError);
                        spSession->AddError(sModLoadError);
                    }
                }
            }
        } else if (pUser) {
            CModules& Modules = pUser->GetModules();

            for (const CModule* pMod : Modules) {
                CString sModName = pMod->GetModName();
                CString sArgs = pMod->GetArgs();
                CString sModRet;
                CString sModLoadError;

                try {
                    if (!pNewUser->GetModules().LoadModule(
                            sModName, sArgs, CModInfo::UserModule, pNewUser,
                            nullptr, sModRet)) {
                        sModLoadError = t_f("Unable to load module [{1}]: {2}")(
                            sModName, sModRet);
                    }
                } catch (...) {
                    sModLoadError =
                        t_f("Unable to load module [{1}] with arguments [{2}]")(
                            sModName, sArgs);
                }

                if (!sModLoadError.empty()) {
                    DEBUG(sModLoadError);
                    spSession->AddError(sModLoadError);
                }
            }
        }

        return pNewUser;
    }

    CString SafeGetUsernameParam(CWebSock& WebSock) {
        CString sUsername = WebSock.GetParam("user");  // check for POST param
        if (sUsername.empty() && !WebSock.IsPost()) {
            // if no POST param named user has been given and we are not
            // saving this form, fall back to using the GET parameter.
            sUsername = WebSock.GetParam("user", false);
        }
        return sUsername;
    }

    CString SafeGetNetworkParam(CWebSock& WebSock) {
        CString sNetwork = WebSock.GetParam("network");  // check for POST param
        if (sNetwork.empty() && !WebSock.IsPost()) {
            // if no POST param named user has been given and we are not
            // saving this form, fall back to using the GET parameter.
            sNetwork = WebSock.GetParam("network", false);
        }
        return sNetwork;
    }

    CUser* SafeGetUserFromParam(CWebSock& WebSock) {
        return CZNC::Get().FindUser(SafeGetUsernameParam(WebSock));
    }

    CIRCNetwork* SafeGetNetworkFromParam(CWebSock& WebSock) {
        CUser* pUser = CZNC::Get().FindUser(SafeGetUsernameParam(WebSock));
        CIRCNetwork* pNetwork = nullptr;

        if (pUser) {
            pNetwork = pUser->FindNetwork(SafeGetNetworkParam(WebSock));
        }

        return pNetwork;
    }

    CString GetWebMenuTitle() override { return "webadmin"; }
    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();

        if (sPageName == "settings") {
            // Admin Check
            if (!spSession->IsAdmin()) {
                return false;
            }

            return SettingsPage(WebSock, Tmpl);
        } else if (sPageName == "adduser") {
            // Admin Check
            if (!spSession->IsAdmin()) {
                return false;
            }

            return UserPage(WebSock, Tmpl);
        } else if (sPageName == "addnetwork") {
            CUser* pUser = SafeGetUserFromParam(WebSock);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || spSession->GetUser() != pUser)) {
                return false;
            }

            if (!pUser) {
                WebSock.PrintErrorPage(t_s("No such user"));
                return true;
            }

            if (spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()) {
                return NetworkPage(WebSock, Tmpl, pUser);
            }

            WebSock.PrintErrorPage(t_s("Permission denied"));
            return true;
        } else if (sPageName == "editnetwork") {
            CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || !pNetwork ||
                 spSession->GetUser() != pNetwork->GetUser())) {
                return false;
            }

            if (!pNetwork) {
                WebSock.PrintErrorPage(t_s("No such user or network"));
                return true;
            }

            return NetworkPage(WebSock, Tmpl, pNetwork->GetUser(), pNetwork);

        } else if (sPageName == "delnetwork") {
            CString sUser = WebSock.GetParam("user");
            if (sUser.empty() && !WebSock.IsPost()) {
                sUser = WebSock.GetParam("user", false);
            }

            CUser* pUser = CZNC::Get().FindUser(sUser);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || spSession->GetUser() != pUser)) {
                return false;
            }

            if (spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()) {
                return DelNetwork(WebSock, pUser, Tmpl);
            }

            WebSock.PrintErrorPage(t_s("Permission denied"));
            return true;
        } else if (sPageName == "editchan") {
            CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || !pNetwork ||
                 spSession->GetUser() != pNetwork->GetUser())) {
                return false;
            }

            if (!pNetwork) {
                WebSock.PrintErrorPage(t_s("No such user or network"));
                return true;
            }

            CString sChan = WebSock.GetParam("name");
            if (sChan.empty() && !WebSock.IsPost()) {
                sChan = WebSock.GetParam("name", false);
            }
            CChan* pChan = pNetwork->FindChan(sChan);
            if (!pChan) {
                WebSock.PrintErrorPage(t_s("No such channel"));
                return true;
            }

            return ChanPage(WebSock, Tmpl, pNetwork, pChan);
        } else if (sPageName == "addchan") {
            CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || !pNetwork ||
                 spSession->GetUser() != pNetwork->GetUser())) {
                return false;
            }

            if (pNetwork) {
                return ChanPage(WebSock, Tmpl, pNetwork);
            }

            WebSock.PrintErrorPage(t_s("No such user or network"));
            return true;
        } else if (sPageName == "delchan") {
            CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || !pNetwork ||
                 spSession->GetUser() != pNetwork->GetUser())) {
                return false;
            }

            if (pNetwork) {
                return DelChan(WebSock, pNetwork);
            }

            WebSock.PrintErrorPage(t_s("No such user or network"));
            return true;
        } else if (sPageName == "deluser") {
            if (!spSession->IsAdmin()) {
                return false;
            }

            if (!WebSock.IsPost()) {
                // Show the "Are you sure?" page:

                CString sUser = WebSock.GetParam("user", false);
                CUser* pUser = CZNC::Get().FindUser(sUser);

                if (!pUser) {
                    WebSock.PrintErrorPage(t_s("No such user"));
                    return true;
                }

                Tmpl.SetFile("del_user.tmpl");
                Tmpl["Username"] = sUser;
                return true;
            }

            // The "Are you sure?" page has been submitted with "Yes",
            // so we actually delete the user now:

            CString sUser = WebSock.GetParam("user");
            CUser* pUser = CZNC::Get().FindUser(sUser);

            if (pUser && pUser == spSession->GetUser()) {
                WebSock.PrintErrorPage(
                    t_s("Please don't delete yourself, suicide is not the "
                        "answer!"));
                return true;
            } else if (CZNC::Get().DeleteUser(sUser)) {
                WebSock.Redirect(GetWebPath() + "listusers");
                return true;
            }

            WebSock.PrintErrorPage(t_s("No such user"));
            return true;
        } else if (sPageName == "edituser") {
            CString sUsername = SafeGetUsernameParam(WebSock);
            CUser* pUser = CZNC::Get().FindUser(sUsername);

            if (!pUser) {
                if (sUsername.empty()) {
                    pUser = spSession->GetUser();
                }  // else: the "no such user" message will be printed.
            }

            // Admin||Self Check
            if (!spSession->IsAdmin() &&
                (!spSession->GetUser() || spSession->GetUser() != pUser)) {
                return false;
            }

            if (pUser) {
                return UserPage(WebSock, Tmpl, pUser);
            }

            WebSock.PrintErrorPage(t_s("No such user"));
            return true;
        } else if (sPageName == "listusers" && spSession->IsAdmin()) {
            return ListUsersPage(WebSock, Tmpl);
        } else if (sPageName == "traffic") {
            return TrafficPage(WebSock, Tmpl);
        } else if (sPageName == "index") {
            return true;
        } else if (sPageName == "add_listener") {
            // Admin Check
            if (!spSession->IsAdmin()) {
                return false;
            }

            return AddListener(WebSock, Tmpl);
        } else if (sPageName == "del_listener") {
            // Admin Check
            if (!spSession->IsAdmin()) {
                return false;
            }

            return DelListener(WebSock, Tmpl);
        }

        return false;
    }

    bool ChanPage(CWebSock& WebSock, CTemplate& Tmpl, CIRCNetwork* pNetwork,
                  CChan* pChan = nullptr) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        Tmpl.SetFile("add_edit_chan.tmpl");
        CUser* pUser = pNetwork->GetUser();

        if (!pUser) {
            WebSock.PrintErrorPage(t_s("No such user"));
            return true;
        }

        if (!WebSock.GetParam("submitted").ToUInt()) {
            Tmpl["User"] = pUser->GetUsername();
            Tmpl["Network"] = pNetwork->GetName();

            CTemplate& breadUser = Tmpl.AddRow("BreadCrumbs");
            breadUser["Text"] = t_f("Edit User [{1}]")(pUser->GetUsername());
            breadUser["URL"] =
                GetWebPath() + "edituser?user=" + pUser->GetUsername();
            CTemplate& breadNet = Tmpl.AddRow("BreadCrumbs");
            breadNet["Text"] = t_f("Edit Network [{1}]")(pNetwork->GetName());
            breadNet["URL"] = GetWebPath() + "editnetwork?user=" +
                              pUser->GetUsername() + "&network=" +
                              pNetwork->GetName();
            CTemplate& breadChan = Tmpl.AddRow("BreadCrumbs");

            if (pChan) {
                Tmpl["Action"] = "editchan";
                Tmpl["Edit"] = "true";
                Tmpl["Title"] =
                    t_f("Edit Channel [{1}] of Network [{2}] of User [{3}]")(
                        pChan->GetName(), pNetwork->GetName(),
                        pUser->GetUsername());
                Tmpl["ChanName"] = pChan->GetName();
                Tmpl["BufferSize"] = CString(pChan->GetBufferCount());
                Tmpl["DefModes"] = pChan->GetDefaultModes();
                Tmpl["Key"] = pChan->GetKey();
                breadChan["Text"] = t_f("Edit Channel [{1}]")(pChan->GetName());

                if (pChan->InConfig()) {
                    Tmpl["InConfig"] = "true";
                }
            } else {
                Tmpl["Action"] = "addchan";
                Tmpl["Title"] =
                    t_f("Add Channel to Network [{1}] of User [{2}]")(
                        pNetwork->GetName(), pUser->GetUsername());
                Tmpl["BufferSize"] = CString(pUser->GetBufferCount());
                Tmpl["DefModes"] = CString(pUser->GetDefaultChanModes());
                Tmpl["InConfig"] = "true";
                breadChan["Text"] = t_s("Add Channel");
            }

            // o1 used to be AutoCycle which was removed

            CTemplate& o2 = Tmpl.AddRow("OptionLoop");
            o2["Name"] = "autoclearchanbuffer";
            o2["DisplayName"] = t_s("Auto clear chan buffer");
            o2["Tooltip"] =
                t_s("Automatically clear channel buffer after playback");
            if ((pChan && pChan->AutoClearChanBuffer()) ||
                (!pChan && pUser->AutoClearChanBuffer())) {
                o2["Checked"] = "true";
            }

            CTemplate& o3 = Tmpl.AddRow("OptionLoop");
            o3["Name"] = "detached";
            o3["DisplayName"] = t_s("Detached");
            if (pChan && pChan->IsDetached()) {
                o3["Checked"] = "true";
            }

            CTemplate& o4 = Tmpl.AddRow("OptionLoop");
            o4["Name"] = "enabled";
            o4["DisplayName"] = t_s("Enabled");
            if (pChan && !pChan->IsDisabled()) {
                o4["Checked"] = "true";
            }

            FOR_EACH_MODULE(i, pNetwork) {
                CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
                mod.insert(Tmpl.begin(), Tmpl.end());
                mod["WebadminAction"] = "display";
                if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/channel",
                                               mod)) {
                    mod["Embed"] = WebSock.FindTmpl(*i, "WebadminChan.tmpl");
                    mod["ModName"] = (*i)->GetModName();
                }
            }

            return true;
        }

        CString sChanName = WebSock.GetParam("name").Trim_n();

        if (!pChan) {
            if (sChanName.empty()) {
                WebSock.PrintErrorPage(
                    t_s("Channel name is a required argument"));
                return true;
            }

            // This could change the channel name and e.g. add a "#" prefix
            pChan = new CChan(sChanName, pNetwork, true);

            if (pNetwork->FindChan(pChan->GetName())) {
                WebSock.PrintErrorPage(
                    t_f("Channel [{1}] already exists")(pChan->GetName()));
                delete pChan;
                return true;
            }

            if (!pNetwork->AddChan(pChan)) {
                WebSock.PrintErrorPage(
                    t_f("Could not add channel [{1}]")(sChanName));
                return true;
            }
        }

        if (WebSock.GetParam("buffersize").empty()) {
            pChan->ResetBufferCount();
        } else {
            unsigned int uBufferSize = WebSock.GetParam("buffersize").ToUInt();
            if (pChan->GetBufferCount() != uBufferSize) {
                pChan->SetBufferCount(uBufferSize, spSession->IsAdmin());
            }
        }
        pChan->SetDefaultModes(WebSock.GetParam("defmodes"));
        pChan->SetInConfig(WebSock.GetParam("save").ToBool());
        bool bAutoClearChanBuffer =
            WebSock.GetParam("autoclearchanbuffer").ToBool();
        if (pChan->AutoClearChanBuffer() != bAutoClearChanBuffer) {
            pChan->SetAutoClearChanBuffer(
                WebSock.GetParam("autoclearchanbuffer").ToBool());
        }
        pChan->SetKey(WebSock.GetParam("key"));

        bool bDetached = WebSock.GetParam("detached").ToBool();
        if (pChan->IsDetached() != bDetached) {
            if (bDetached) {
                pChan->DetachUser();
            } else {
                pChan->AttachUser();
            }
        }

        bool bEnabled = WebSock.GetParam("enabled").ToBool();
        if (bEnabled)
            pChan->Enable();
        else
            pChan->Disable();

        CTemplate TmplMod;
        TmplMod["User"] = pUser->GetUsername();
        TmplMod["ChanName"] = pChan->GetName();
        TmplMod["WebadminAction"] = "change";
        FOR_EACH_MODULE(it, pNetwork) {
            (*it)->OnEmbeddedWebRequest(WebSock, "webadmin/channel", TmplMod);
        }

        if (!CZNC::Get().WriteConfig()) {
            WebSock.PrintErrorPage(t_s(
                "Channel was added/modified, but config file was not written"));
            return true;
        }

        if (WebSock.HasParam("submit_return")) {
            WebSock.Redirect(GetWebPath() + "editnetwork?user=" +
                             pUser->GetUsername().Escape_n(CString::EURL) +
                             "&network=" +
                             pNetwork->GetName().Escape_n(CString::EURL));
        } else {
            WebSock.Redirect(
                GetWebPath() + "editchan?user=" +
                pUser->GetUsername().Escape_n(CString::EURL) + "&network=" +
                pNetwork->GetName().Escape_n(CString::EURL) + "&name=" +
                pChan->GetName().Escape_n(CString::EURL));
        }
        return true;
    }

    bool NetworkPage(CWebSock& WebSock, CTemplate& Tmpl, CUser* pUser,
                     CIRCNetwork* pNetwork = nullptr) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        Tmpl.SetFile("add_edit_network.tmpl");

        if (!pNetwork && !spSession->IsAdmin() && pUser->DenySetNetwork()) {
            WebSock.PrintErrorPage(t_s("Permission denied"));
            return true;
        }

        if (!pNetwork && !spSession->IsAdmin() &&
            !pUser->HasSpaceForNewNetwork()) {
            WebSock.PrintErrorPage(t_s(
                "Network number limit reached. Ask an admin to increase the "
                "limit for you, or delete unneeded networks from Your "
                "Settings."));
            return true;
        }

        if (!WebSock.GetParam("submitted").ToUInt()) {
            Tmpl["Username"] = pUser->GetUsername();

            CTemplate& breadUser = Tmpl.AddRow("BreadCrumbs");
            breadUser["Text"] = t_f("Edit User [{1}]")(pUser->GetUsername());
            breadUser["URL"] =
                GetWebPath() + "edituser?user=" + pUser->GetUsername();

            CTemplate& breadNet = Tmpl.AddRow("BreadCrumbs");

            CIRCNetwork EmptyNetwork(pUser, "");
            CIRCNetwork* pRealNetwork = pNetwork;
            if (pNetwork) {
                Tmpl["Action"] = "editnetwork";
                Tmpl["Edit"] = "true";
                Tmpl["Title"] = t_f("Edit Network [{1}] of User [{2}]")(
                    pNetwork->GetName(), pUser->GetUsername());
                breadNet["Text"] =
                    t_f("Edit Network [{1}]")(pNetwork->GetName());
            } else {
                Tmpl["Action"] = "addnetwork";
                Tmpl["Title"] =
                    t_f("Add Network for User [{1}]")(pUser->GetUsername());
                breadNet["Text"] = t_s("Add Network");

                pNetwork = &EmptyNetwork;
            }

            set<CModInfo> ssNetworkMods;
            CZNC::Get().GetModules().GetAvailableMods(ssNetworkMods,
                                                      CModInfo::NetworkModule);
            set<CModInfo> ssDefaultMods;
            CZNC::Get().GetModules().GetDefaultMods(ssDefaultMods,
                                                    CModInfo::NetworkModule);
            for (const CModInfo& Info : ssNetworkMods) {
                CTemplate& l = Tmpl.AddRow("ModuleLoop");

                l["Name"] = Info.GetName();
                l["Description"] = Info.GetDescription();
                l["Wiki"] = Info.GetWikiPage();
                l["HasArgs"] = CString(Info.GetHasArgs());
                l["ArgsHelpText"] = Info.GetArgsHelpText();

                if (pRealNetwork) {
                    CModule* pModule =
                        pNetwork->GetModules().FindModule(Info.GetName());
                    if (pModule) {
                        l["Checked"] = "true";
                        l["Args"] = pModule->GetArgs();
                    }
                } else {
                    for (const CModInfo& DInfo : ssDefaultMods) {
                        if (Info.GetName() == DInfo.GetName()) {
                            l["Checked"] = "true";
                        }
                    }
                }

                // Check if module is loaded globally
                l["CanBeLoadedGlobally"] =
                    CString(Info.SupportsType(CModInfo::GlobalModule));
                l["LoadedGlobally"] =
                    CString(CZNC::Get().GetModules().FindModule(
                                Info.GetName()) != nullptr);

                // Check if module is loaded by user
                l["CanBeLoadedByUser"] =
                    CString(Info.SupportsType(CModInfo::UserModule));
                l["LoadedByUser"] = CString(
                    pUser->GetModules().FindModule(Info.GetName()) != nullptr);

                if (!spSession->IsAdmin() && pUser->DenyLoadMod()) {
                    l["Disabled"] = "true";
                }
            }

            // To change BindHosts be admin or don't have DenySetBindHost
            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetBindHost()) {
                Tmpl["BindHostEdit"] = "true";
                Tmpl["BindHost"] = pNetwork->GetBindHost();
            }

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetNetwork()) {
                Tmpl["NameEdit"] = "true";
            }

            Tmpl["Name"] = pNetwork->GetName();
            Tmpl["Nick"] = pNetwork->GetNick();
            Tmpl["AltNick"] = pNetwork->GetAltNick();

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetIdent()) {
                Tmpl["IdentEdit"] = "true";
                Tmpl["Ident"] = pNetwork->GetIdent();
            }

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetRealName()) {
                Tmpl["RealNameEdit"] = "true";
                Tmpl["RealName"] = pNetwork->GetRealName();
            }

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetQuitMsg()) {
                Tmpl["QuitMsgEdit"] = "true";
                Tmpl["QuitMsg"] = pNetwork->GetQuitMsg();
            }

            Tmpl["NetworkEdit"] =
                spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()
                ? "true" : "false";

            Tmpl["FloodProtection"] =
                CString(CIRCSock::IsFloodProtected(pNetwork->GetFloodRate()));
            Tmpl["FloodRate"] = CString(pNetwork->GetFloodRate());
            Tmpl["FloodBurst"] = CString(pNetwork->GetFloodBurst());

            Tmpl["JoinDelay"] = CString(pNetwork->GetJoinDelay());

            Tmpl["IRCConnectEnabled"] =
                CString(pNetwork->GetIRCConnectEnabled());
            Tmpl["TrustAllCerts"] = CString(pNetwork->GetTrustAllCerts());
            Tmpl["TrustPKI"] = CString(pNetwork->GetTrustPKI());

            const vector<CServer*>& vServers = pNetwork->GetServers();
            for (const CServer* pServer : vServers) {
                CTemplate& l = Tmpl.AddRow("ServerLoop");
                l["Server"] = pServer->GetString();
            }

            const vector<CChan*>& Channels = pNetwork->GetChans();
            unsigned int uIndex = 1;
            for (const CChan* pChan : Channels) {
                CTemplate& l = Tmpl.AddRow("ChannelLoop");

                l["Network"] = pNetwork->GetName();
                l["Username"] = pUser->GetUsername();
                l["Name"] = pChan->GetName();
                l["Perms"] = pChan->GetPermStr();
                l["CurModes"] = pChan->GetModeString();
                l["DefModes"] = pChan->GetDefaultModes();
                if (pChan->HasBufferCountSet()) {
                    l["BufferSize"] = CString(pChan->GetBufferCount());
                } else {
                    l["BufferSize"] =
                        CString(pChan->GetBufferCount()) + " (default)";
                }
                l["Options"] = pChan->GetOptions();

                if (pChan->InConfig()) {
                    l["InConfig"] = "true";
                }

                l["MaxIndex"] = CString(Channels.size());
                l["Index"] = CString(uIndex++);
            }
            for (const CString& sFP : pNetwork->GetTrustedFingerprints()) {
                CTemplate& l = Tmpl.AddRow("TrustedFingerprints");
                l["FP"] = sFP;
            }

            FOR_EACH_MODULE(i, make_pair(pUser, pNetwork)) {
                CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
                mod.insert(Tmpl.begin(), Tmpl.end());
                mod["WebadminAction"] = "display";
                if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/network",
                                               mod)) {
                    mod["Embed"] = WebSock.FindTmpl(*i, "WebadminNetwork.tmpl");
                    mod["ModName"] = (*i)->GetModName();
                }
            }

#ifdef HAVE_ICU
            for (const CString& sEncoding : CUtils::GetEncodings()) {
                CTemplate& l = Tmpl.AddRow("EncodingLoop");
                l["Encoding"] = sEncoding;
            }
            const CString sEncoding =
                pRealNetwork ? pNetwork->GetEncoding() : "^UTF-8";
            if (sEncoding.empty()) {
                Tmpl["EncodingUtf"] = "legacy";
            } else if (sEncoding[0] == '*') {
                Tmpl["EncodingUtf"] = "receive";
                Tmpl["Encoding"] = sEncoding.substr(1);
            } else if (sEncoding[0] == '^') {
                Tmpl["EncodingUtf"] = "send";
                Tmpl["Encoding"] = sEncoding.substr(1);
            } else {
                Tmpl["EncodingUtf"] = "simple";
                Tmpl["Encoding"] = sEncoding;
            }
            Tmpl["LegacyEncodingDisabled"] =
                CString(CZNC::Get().IsForcingEncoding());
#else
            Tmpl["LegacyEncodingDisabled"] = "true";
            Tmpl["EncodingDisabled"] = "true";
            Tmpl["EncodingUtf"] = "legacy";
#endif

            return true;
        }

        CString sName = WebSock.GetParam("name").Trim_n();
        if (sName.empty()) {
            WebSock.PrintErrorPage(t_s("Network name is a required argument"));
            return true;
        }
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()) {
            if (!pNetwork || pNetwork->GetName() != sName) {
                CString sNetworkAddError;
                CIRCNetwork* pOldNetwork = pNetwork;
                pNetwork = pUser->AddNetwork(sName, sNetworkAddError);
                if (!pNetwork) {
                    WebSock.PrintErrorPage(sNetworkAddError);
                    return true;
                }
                if (pOldNetwork) {
                    for (CModule* pModule : pOldNetwork->GetModules()) {
                        CString sPath = pUser->GetUserPath() + "/networks/" +
                                        sName + "/moddata/" + pModule->GetModName();
                        pModule->MoveRegistry(sPath);
                    }
                    pNetwork->Clone(*pOldNetwork, false);
                    pUser->DeleteNetwork(pOldNetwork->GetName());
                }
            }
        }

        CString sArg;

        pNetwork->SetNick(WebSock.GetParam("nick"));
        pNetwork->SetAltNick(WebSock.GetParam("altnick"));

        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetIdent()) {
            pNetwork->SetIdent(WebSock.GetParam("ident"));
        }

        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetRealName()) {
            pNetwork->SetRealName(WebSock.GetParam("realname"));
        }

        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetQuitMsg()) {
            pNetwork->SetQuitMsg(WebSock.GetParam("quitmsg"));
        }

        pNetwork->SetIRCConnectEnabled(WebSock.GetParam("doconnect").ToBool());

        pNetwork->SetTrustAllCerts(WebSock.GetParam("trustallcerts").ToBool());
        pNetwork->SetTrustPKI(WebSock.GetParam("trustpki").ToBool());

        sArg = WebSock.GetParam("bindhost");
        // To change BindHosts be admin or don't have DenySetBindHost
        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
            pNetwork->SetBindHost(WebSock.GetParam("bindhost"));
        }

        if (WebSock.GetParam("floodprotection").ToBool()) {
            pNetwork->SetFloodRate(WebSock.GetParam("floodrate").ToDouble());
            pNetwork->SetFloodBurst(WebSock.GetParam("floodburst").ToUShort());
        } else {
            pNetwork->SetFloodRate(-1);
        }

        pNetwork->SetJoinDelay(WebSock.GetParam("joindelay").ToUShort());

#ifdef HAVE_ICU
        CString sEncodingUtf = WebSock.GetParam("encoding_utf");
        if (sEncodingUtf == "legacy") {
            pNetwork->SetEncoding("");
        }
        CString sEncoding = WebSock.GetParam("encoding");
        if (sEncoding.empty()) {
            sEncoding = "UTF-8";
        }
        if (sEncodingUtf == "send") {
            pNetwork->SetEncoding("^" + sEncoding);
        } else if (sEncodingUtf == "receive") {
            pNetwork->SetEncoding("*" + sEncoding);
        } else if (sEncodingUtf == "simple") {
            pNetwork->SetEncoding(sEncoding);
        }
#endif

        VCString vsArgs;

        if (spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()) {
            pNetwork->DelServers();
            WebSock.GetRawParam("servers").Split("\n", vsArgs);
            for (const CString& sServer : vsArgs) {
                pNetwork->AddServer(sServer.Trim_n());
            }
        }

        WebSock.GetRawParam("fingerprints").Split("\n", vsArgs);
        pNetwork->ClearTrustedFingerprints();
        for (const CString& sFP : vsArgs) {
            pNetwork->AddTrustedFingerprint(sFP);
        }

        WebSock.GetParamValues("channel", vsArgs);
        for (const CString& sChan : vsArgs) {
            CChan* pChan = pNetwork->FindChan(sChan.TrimRight_n("\r"));
            if (pChan) {
                CString sError;
                if (!pNetwork->MoveChan(
                        sChan, WebSock.GetParam("index_" + sChan).ToUInt() - 1,
                        sError)) {
                    WebSock.PrintErrorPage(sError);
                    return true;
                }
                pChan->SetInConfig(WebSock.GetParam("save_" + sChan).ToBool());
            }
        }

        set<CString> ssArgs;
        WebSock.GetParamValues("loadmod", ssArgs);
        if (spSession->IsAdmin() || !pUser->DenyLoadMod()) {
            for (const CString& s : ssArgs) {
                CString sModRet;
                CString sModName = s.TrimRight_n("\r");
                CString sModLoadError;

                if (!sModName.empty()) {
                    CString sArgs = WebSock.GetParam("modargs_" + sModName);

                    CModule* pMod = pNetwork->GetModules().FindModule(sModName);

                    if (!pMod) {
                        if (!pNetwork->GetModules().LoadModule(
                                sModName, sArgs, CModInfo::NetworkModule, pUser,
                                pNetwork, sModRet)) {
                            sModLoadError =
                                t_f("Unable to load module [{1}]: {2}")(
                                    sModName, sModRet);
                        }
                    } else if (pMod->GetArgs() != sArgs) {
                        if (!pNetwork->GetModules().ReloadModule(
                                sModName, sArgs, pUser, pNetwork, sModRet)) {
                            sModLoadError =
                                t_f("Unable to reload module [{1}]: {2}")(
                                    sModName, sModRet);
                        }
                    }

                    if (!sModLoadError.empty()) {
                        DEBUG(sModLoadError);
                        WebSock.GetSession()->AddError(sModLoadError);
                    }
                }
            }
        }

        const CModules& vCurMods = pNetwork->GetModules();
        set<CString> ssUnloadMods;

        for (const CModule* pCurMod : vCurMods) {
            if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() &&
                pCurMod->GetModName() != GetModName()) {
                ssUnloadMods.insert(pCurMod->GetModName());
            }
        }

        for (const CString& sMod : ssUnloadMods) {
            pNetwork->GetModules().UnloadModule(sMod);
        }

        CTemplate TmplMod;
        TmplMod["Username"] = pUser->GetUsername();
        TmplMod["Name"] = pNetwork->GetName();
        TmplMod["WebadminAction"] = "change";
        FOR_EACH_MODULE(it, make_pair(pUser, pNetwork)) {
            (*it)->OnEmbeddedWebRequest(WebSock, "webadmin/network", TmplMod);
        }

        if (!CZNC::Get().WriteConfig()) {
            WebSock.PrintErrorPage(t_s(
                "Network was added/modified, but config file was not written"));
            return true;
        }

        if (WebSock.HasParam("submit_return")) {
            WebSock.Redirect(GetWebPath() + "edituser?user=" +
                             pUser->GetUsername().Escape_n(CString::EURL));
        } else {
            WebSock.Redirect(GetWebPath() + "editnetwork?user=" +
                             pUser->GetUsername().Escape_n(CString::EURL) +
                             "&network=" +
                             pNetwork->GetName().Escape_n(CString::EURL));
        }
        return true;
    }

    bool DelNetwork(CWebSock& WebSock, CUser* pUser, CTemplate& Tmpl) {
        CString sNetwork = WebSock.GetParam("name");
        if (sNetwork.empty() && !WebSock.IsPost()) {
            sNetwork = WebSock.GetParam("name", false);
        }

        if (!pUser) {
            WebSock.PrintErrorPage(t_s("No such user"));
            return true;
        }

        if (sNetwork.empty()) {
            WebSock.PrintErrorPage(
                t_s("That network doesn't exist for this user"));
            return true;
        }

        if (!WebSock.IsPost()) {
            // Show the "Are you sure?" page:

            Tmpl.SetFile("del_network.tmpl");
            Tmpl["Username"] = pUser->GetUsername();
            Tmpl["Network"] = sNetwork;
            return true;
        }

        pUser->DeleteNetwork(sNetwork);

        if (!CZNC::Get().WriteConfig()) {
            WebSock.PrintErrorPage(
                t_s("Network was deleted, but config file was not written"));
            return true;
        }

        WebSock.Redirect(GetWebPath() + "edituser?user=" +
                         pUser->GetUsername().Escape_n(CString::EURL));
        return false;
    }

    bool DelChan(CWebSock& WebSock, CIRCNetwork* pNetwork) {
        CString sChan = WebSock.GetParam("name", false);

        if (sChan.empty()) {
            WebSock.PrintErrorPage(
                t_s("That channel doesn't exist for this network"));
            return true;
        }

        pNetwork->DelChan(sChan);
        pNetwork->PutIRC("PART " + sChan);

        if (!CZNC::Get().WriteConfig()) {
            WebSock.PrintErrorPage(
                t_s("Channel was deleted, but config file was not written"));
            return true;
        }

        WebSock.Redirect(
            GetWebPath() + "editnetwork?user=" +
            pNetwork->GetUser()->GetUsername().Escape_n(CString::EURL) +
            "&network=" + pNetwork->GetName().Escape_n(CString::EURL));
        return false;
    }

    bool UserPage(CWebSock& WebSock, CTemplate& Tmpl, CUser* pUser = nullptr) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        Tmpl.SetFile("add_edit_user.tmpl");

        if (!WebSock.GetParam("submitted").ToUInt()) {
            CUser EmptyUser("");
            CUser* pRealUser = pUser;
            if (pUser) {
                Tmpl["Title"] = t_f("Edit User [{1}]")(pUser->GetUsername());
                Tmpl["Edit"] = "true";
            } else {
                CString sUsername = WebSock.GetParam("clone", false);
                pUser = CZNC::Get().FindUser(sUsername);
                pRealUser = pUser;

                if (pUser) {
                    Tmpl["Title"] =
                        t_f("Clone User [{1}]")(pUser->GetUsername());
                    Tmpl["Clone"] = "true";
                    Tmpl["CloneUsername"] = pUser->GetUsername();
                } else {
                    pUser = &EmptyUser;
                    Tmpl["Title"] = "Add User";
                }
            }

            Tmpl["ImAdmin"] = CString(spSession->IsAdmin());

            Tmpl["Username"] = pUser->GetUsername();
            Tmpl["AuthOnlyViaModule"] = CString(pUser->AuthOnlyViaModule());
            Tmpl["Nick"] = pUser->GetNick();
            Tmpl["AltNick"] = pUser->GetAltNick();
            Tmpl["StatusPrefix"] = pUser->GetStatusPrefix();

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetIdent()) {
                Tmpl["IdentEdit"] = "true";
                Tmpl["Ident"] = pUser->GetIdent();
            }

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetRealName()) {
                Tmpl["RealNameEdit"] = "true";
                Tmpl["RealName"] = pUser->GetRealName();
            }

            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetQuitMsg()) {
                Tmpl["QuitMsgEdit"] = "true";
                Tmpl["QuitMsg"] = pUser->GetQuitMsg();
            }

            Tmpl["NetworkEdit"] =
                spSession->IsAdmin() || !spSession->GetUser()->DenySetNetwork()
                ? "true" : "false";

            Tmpl["CTCPEdit"] =
                spSession->IsAdmin() || !spSession->GetUser()->DenySetCTCPReplies()
                ? "true" : "false";

            Tmpl["DefaultChanModes"] = pUser->GetDefaultChanModes();
            Tmpl["ChanBufferSize"] = CString(pUser->GetChanBufferSize());
            Tmpl["QueryBufferSize"] = CString(pUser->GetQueryBufferSize());
            Tmpl["TimestampFormat"] = pUser->GetTimestampFormat();
            Tmpl["Timezone"] = pUser->GetTimezone();
            Tmpl["JoinTries"] = CString(pUser->JoinTries());
            Tmpl["NoTrafficTimeout"] = CString(pUser->GetNoTrafficTimeout());
            Tmpl["MaxNetworks"] = CString(pUser->MaxNetworks());
            Tmpl["MaxJoins"] = CString(pUser->MaxJoins());
            Tmpl["MaxQueryBuffers"] = CString(pUser->MaxQueryBuffers());
            Tmpl["Language"] = pUser->GetLanguage();

            const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
            for (const CString& sHost : ssAllowedHosts) {
                CTemplate& l = Tmpl.AddRow("AllowedHostLoop");
                l["Host"] = sHost;
            }

            const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();
            for (const CIRCNetwork* pNetwork : vNetworks) {
                CTemplate& l = Tmpl.AddRow("NetworkLoop");
                l["Name"] = pNetwork->GetName();
                l["Username"] = pUser->GetUsername();
                l["Clients"] = CString(pNetwork->GetClients().size());
                l["IRCNick"] = pNetwork->GetIRCNick().GetNick();
                CServer* pServer = pNetwork->GetCurrentServer();
                if (pServer) {
                    l["Server"] = pServer->GetName() + ":" +
                                  (pServer->IsSSL() ? "+" : "") +
                                  CString(pServer->GetPort());
                }
            }

            const MCString& msCTCPReplies = pUser->GetCTCPReplies();
            for (const auto& it : msCTCPReplies) {
                CTemplate& l = Tmpl.AddRow("CTCPLoop");
                l["CTCP"] = it.first + " " + it.second;
            }

            SCString ssTimezones = CUtils::GetTimezones();
            for (const CString& sTZ : ssTimezones) {
                CTemplate& l = Tmpl.AddRow("TZLoop");
                l["TZ"] = sTZ;
            }

#ifdef HAVE_I18N
            Tmpl["HaveI18N"] = "true";
            // TODO maybe stop hardcoding English here
            CTemplate& l_en = Tmpl.AddRow("LanguageLoop");
            l_en["Code"] = "";
            l_en["Name"] = "English";
            for (const auto& it : CTranslationInfo::GetTranslations()) {
                CTemplate& lang = Tmpl.AddRow("LanguageLoop");
                lang["Code"] = it.first;
                lang["Name"] = it.second.sSelfName;
            }
#else
            Tmpl["HaveI18N"] = "false";
#endif
#ifdef HAVE_ICU
            for (const CString& sEncoding : CUtils::GetEncodings()) {
                CTemplate& l = Tmpl.AddRow("EncodingLoop");
                l["Encoding"] = sEncoding;
            }
            const CString sEncoding =
                pRealUser ? pUser->GetClientEncoding() : "^UTF-8";
            if (sEncoding.empty()) {
                Tmpl["EncodingUtf"] = "legacy";
            } else if (sEncoding[0] == '*') {
                Tmpl["EncodingUtf"] = "receive";
                Tmpl["Encoding"] = sEncoding.substr(1);
            } else if (sEncoding[0] == '^') {
                Tmpl["EncodingUtf"] = "send";
                Tmpl["Encoding"] = sEncoding.substr(1);
            } else {
                Tmpl["EncodingUtf"] = "simple";
                Tmpl["Encoding"] = sEncoding;
            }
            Tmpl["LegacyEncodingDisabled"] =
                CString(CZNC::Get().IsForcingEncoding());
#else
            Tmpl["LegacyEncodingDisabled"] = "true";
            Tmpl["EncodingDisabled"] = "true";
            Tmpl["EncodingUtf"] = "legacy";
#endif

            // To change BindHosts be admin or don't have DenySetBindHost
            if (spSession->IsAdmin() ||
                !spSession->GetUser()->DenySetBindHost()) {
                Tmpl["BindHostEdit"] = "true";
                Tmpl["BindHost"] = pUser->GetBindHost();
                Tmpl["DCCBindHost"] = pUser->GetDCCBindHost();
            }

            vector<CString> vDirs;
            WebSock.GetAvailSkins(vDirs);

            for (const CString& SubDir : vDirs) {
                CTemplate& l = Tmpl.AddRow("SkinLoop");
                l["Name"] = SubDir;

                if (SubDir == pUser->GetSkinName()) {
                    l["Checked"] = "true";
                }
            }

            set<CModInfo> ssUserMods;
            CZNC::Get().GetModules().GetAvailableMods(ssUserMods);
            set<CModInfo> ssDefaultMods;
            CZNC::Get().GetModules().GetDefaultMods(ssDefaultMods,
                                                    CModInfo::UserModule);

            for (const CModInfo& Info : ssUserMods) {
                CTemplate& l = Tmpl.AddRow("ModuleLoop");

                l["Name"] = Info.GetName();
                l["Description"] = Info.GetDescription();
                l["Wiki"] = Info.GetWikiPage();
                l["HasArgs"] = CString(Info.GetHasArgs());
                l["ArgsHelpText"] = Info.GetArgsHelpText();

                CModule* pModule = nullptr;
                pModule = pUser->GetModules().FindModule(Info.GetName());
                // Check if module is loaded by all or some networks
                const vector<CIRCNetwork*>& userNetworks = pUser->GetNetworks();
                unsigned int networksWithRenderedModuleCount = 0;
                for (const CIRCNetwork* pCurrentNetwork : userNetworks) {
                    const CModules& networkModules =
                        pCurrentNetwork->GetModules();
                    if (networkModules.FindModule(Info.GetName())) {
                        networksWithRenderedModuleCount++;
                    }
                }
                l["CanBeLoadedByNetwork"] =
                    CString(Info.SupportsType(CModInfo::NetworkModule));
                l["LoadedByAllNetworks"] = CString(
                    networksWithRenderedModuleCount == userNetworks.size());
                l["LoadedBySomeNetworks"] =
                    CString(networksWithRenderedModuleCount != 0);
                if (pModule) {
                    l["Checked"] = "true";
                    l["Args"] = pModule->GetArgs();
                    if (CModInfo::UserModule == GetType() &&
                        Info.GetName() == GetModName()) {
                        l["Disabled"] = "true";
                    }
                }
                if (!pRealUser) {
                    for (const CModInfo& DInfo : ssDefaultMods) {
                        if (Info.GetName() == DInfo.GetName()) {
                            l["Checked"] = "true";
                        }
                    }
                }
                l["CanBeLoadedGlobally"] =
                    CString(Info.SupportsType(CModInfo::GlobalModule));
                // Check if module is loaded globally
                l["LoadedGlobally"] =
                    CString(CZNC::Get().GetModules().FindModule(
                                Info.GetName()) != nullptr);

                if (!spSession->IsAdmin() && pUser->DenyLoadMod()) {
                    l["Disabled"] = "true";
                }
            }

            CTemplate& o1 = Tmpl.AddRow("OptionLoop");
            o1["Name"] = "autoclearchanbuffer";
            o1["DisplayName"] = t_s("Auto clear chan buffer");
            o1["Tooltip"] =
                t_s("Automatically clear channel buffer after playback (the "
                    "default value for new channels)");
            if (pUser->AutoClearChanBuffer()) {
                o1["Checked"] = "true";
            }

            /* o2 used to be auto cycle which was removed */

            CTemplate& o4 = Tmpl.AddRow("OptionLoop");
            o4["Name"] = "multiclients";
            o4["DisplayName"] = t_s("Allow multiple clients");
            if (pUser->MultiClients()) {
                o4["Checked"] = "true";
            }

            CTemplate& o7 = Tmpl.AddRow("OptionLoop");
            o7["Name"] = "appendtimestamp";
            o7["DisplayName"] = t_s("Append timestamps");
            if (pUser->GetTimestampAppend()) {
                o7["Checked"] = "true";
            }

            CTemplate& o8 = Tmpl.AddRow("OptionLoop");
            o8["Name"] = "prependtimestamp";
            o8["DisplayName"] = t_s("Prepend timestamps");
            if (pUser->GetTimestampPrepend()) {
                o8["Checked"] = "true";
            }

            if (spSession->IsAdmin()) {
                CTemplate& o9 = Tmpl.AddRow("OptionLoop");
                o9["Name"] = "denyloadmod";
                o9["DisplayName"] = t_s("Deny LoadMod");
                if (pUser->DenyLoadMod()) {
                    o9["Checked"] = "true";
                }

                CTemplate& o10 = Tmpl.AddRow("OptionLoop");
                o10["Name"] = "isadmin";
                o10["DisplayName"] = t_s("Admin (dangerous! may gain shell access)");
                if (pUser->IsAdmin()) {
                    o10["Checked"] = "true";
                }
                if (pUser == CZNC::Get().FindUser(WebSock.GetUser())) {
                    o10["Disabled"] = "true";
                }

                CTemplate& o11 = Tmpl.AddRow("OptionLoop");
                o11["Name"] = "denysetbindhost";
                o11["DisplayName"] = t_s("Deny setting BindHost");
                if (pUser->DenySetBindHost()) {
                    o11["Checked"] = "true";
                }

                CTemplate& o12 = Tmpl.AddRow("OptionLoop");
                o12["Name"] = "denysetident";
                o12["DisplayName"] = t_s("Deny setting Ident");
                if (pUser->DenySetIdent()) {
                    o12["Checked"] = "true";
                }

                CTemplate& o13 = Tmpl.AddRow("OptionLoop");
                o13["Name"] = "denysetnetwork";
                o13["DisplayName"] = t_s("Deny editing networks/servers");
                o13["Tooltip"] =
                    t_s("Deny adding/deleting networks, setting network name and editing the server list");
                if (pUser->DenySetNetwork()) {
                    o13["Checked"] = "true";
                }

                CTemplate& o14 = Tmpl.AddRow("OptionLoop");
                o14["Name"] = "denysetrealname";
                o14["DisplayName"] = t_s("Deny setting RealName");
                if (pUser->DenySetRealName()) {
                    o14["Checked"] = "true";
                }

                CTemplate& o15 = Tmpl.AddRow("OptionLoop");
                o15["Name"] = "denysetquitmsg";
                o15["DisplayName"] = t_s("Deny setting quit message");
                if (pUser->DenySetQuitMsg()) {
                    o15["Checked"] = "true";
                }

                CTemplate& o16 = Tmpl.AddRow("OptionLoop");
                o16["Name"] = "denysetctcpreplies";
                o16["DisplayName"] = t_s("Deny setting CTCP replies");
                o16["Tooltip"] =
                    t_s("Block customizing CTCP replies for non-admin users");
                if (pUser->DenySetCTCPReplies()) {
                    o16["Checked"] = "true";
                }
            }

            CTemplate& o17 = Tmpl.AddRow("OptionLoop");
            o17["Name"] = "autoclearquerybuffer";
            o17["DisplayName"] = t_s("Auto clear query buffer");
            o17["Tooltip"] =
                t_s("Automatically clear query buffer after playback");
            if (pUser->AutoClearQueryBuffer()) {
                o17["Checked"] = "true";
            }

            FOR_EACH_MODULE(i, pUser) {
                CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
                mod.insert(Tmpl.begin(), Tmpl.end());
                mod["WebadminAction"] = "display";
                if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/user", mod)) {
                    mod["Embed"] = WebSock.FindTmpl(*i, "WebadminUser.tmpl");
                    mod["ModName"] = (*i)->GetModName();
                }
            }

            return true;
        }

        /* If pUser is nullptr, we are adding a user, else we are editing this
         * one */

        CString sUsername = WebSock.GetParam("user");
        if (!pUser && CZNC::Get().FindUser(sUsername)) {
            WebSock.PrintErrorPage(
                t_f("Invalid Submission: User {1} already exists")(sUsername));
            return true;
        }

        CUser* pNewUser = GetNewUser(WebSock, pUser);
        if (!pNewUser) {
            // GetNewUser already called WebSock.PrintErrorPage()
            return true;
        }

        CString sErr;
        CString sConfigErrorMsg;

        if (!pUser) {
            CString sClone = WebSock.GetParam("clone");
            if (CUser* pCloneUser = CZNC::Get().FindUser(sClone)) {
                pNewUser->CloneNetworks(*pCloneUser);
            }

            // Add User Submission
            if (!CZNC::Get().AddUser(pNewUser, sErr)) {
                delete pNewUser;
                WebSock.PrintErrorPage(t_f("Invalid submission: {1}")(sErr));
                return true;
            }

            pUser = pNewUser;
            sConfigErrorMsg =
                t_s("User was added, but config file was not written");
        } else {
            // Edit User Submission
            if (!pUser->Clone(*pNewUser, sErr, false)) {
                delete pNewUser;
                WebSock.PrintErrorPage(t_f("Invalid submission: {1}")(sErr));
                return true;
            }

            delete pNewUser;
            sConfigErrorMsg =
                t_s("User was edited, but config file was not written");
        }

        CTemplate TmplMod;
        TmplMod["Username"] = sUsername;
        TmplMod["WebadminAction"] = "change";
        FOR_EACH_MODULE(it, pUser) {
            (*it)->OnEmbeddedWebRequest(WebSock, "webadmin/user", TmplMod);
        }

        if (!CZNC::Get().WriteConfig()) {
            WebSock.PrintErrorPage(sConfigErrorMsg);
            return true;
        }

        if (spSession->IsAdmin() && WebSock.HasParam("submit_return")) {
            WebSock.Redirect(GetWebPath() + "listusers");
        } else {
            WebSock.Redirect(GetWebPath() + "edituser?user=" +
                             pUser->GetUsername());
        }

        /* we don't want the template to be printed while we redirect */
        return false;
    }

    bool ListUsersPage(CWebSock& WebSock, CTemplate& Tmpl) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
        Tmpl["Title"] = t_s("Manage Users");
        Tmpl["Action"] = "listusers";

        for (const auto& it : msUsers) {
            CTemplate& l = Tmpl.AddRow("UserLoop");
            CUser* pUser = it.second;

            l["Username"] = pUser->GetUsername();
            l["Clients"] = CString(pUser->GetAllClients().size());
            l["Networks"] = CString(pUser->GetNetworks().size());

            if (pUser == spSession->GetUser()) {
                l["IsSelf"] = "true";
            }
        }

        return true;
    }

    bool TrafficPage(CWebSock& WebSock, CTemplate& Tmpl) {
        std::shared_ptr<CWebSession> spSession = WebSock.GetSession();
        Tmpl["Title"] = t_s("Traffic Info");
        Tmpl["Uptime"] = CZNC::Get().GetUptime();

        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
        Tmpl["TotalUsers"] = CString(msUsers.size());

        size_t uiNetworks = 0, uiAttached = 0, uiClients = 0, uiServers = 0;

        for (const auto& it : msUsers) {
            CUser* pUser = it.second;

            if (!spSession->IsAdmin() && spSession->GetUser() != it.second) {
                continue;
            }

            vector<CIRCNetwork*> vNetworks = pUser->GetNetworks();

            for (const CIRCNetwork* pNetwork : vNetworks) {
                uiNetworks++;

                if (pNetwork->IsIRCConnected()) {
                    uiServers++;
                }

                if (pNetwork->IsNetworkAttached()) {
                    uiAttached++;
                }

                uiClients += pNetwork->GetClients().size();
            }

            uiClients += pUser->GetUserClients().size();
        }

        Tmpl["TotalNetworks"] = CString(uiNetworks);
        Tmpl["AttachedNetworks"] = CString(uiAttached);
        Tmpl["TotalCConnections"] = CString(uiClients);
        Tmpl["TotalIRCConnections"] = CString(uiServers);

        CZNC::TrafficStatsPair Users, ZNC, Total;
        CZNC::TrafficStatsMap traffic =
            CZNC::Get().GetTrafficStats(Users, ZNC, Total);

        for (const auto& it : traffic) {
            if (!spSession->IsAdmin() &&
                !spSession->GetUser()->GetUsername().Equals(it.first)) {
                continue;
            }

            CTemplate& l = Tmpl.AddRow("TrafficLoop");

            l["Username"] = it.first;
            l["In"] = CString::ToByteStr(it.second.first);
            l["Out"] = CString::ToByteStr(it.second.second);
            l["Total"] = CString::ToByteStr(it.second.first + it.second.second);

            CZNC::TrafficStatsPair NetworkTotal;
            CZNC::TrafficStatsMap NetworkTraffic =
                CZNC::Get().GetNetworkTrafficStats(it.first, NetworkTotal);
            for (const auto& it2 : NetworkTraffic) {
                CTemplate& l2 = Tmpl.AddRow("TrafficLoop");

                l2["Network"] = it2.first;
                l2["In"] = CString::ToByteStr(it2.second.first);
                l2["Out"] = CString::ToByteStr(it2.second.second);
                l2["Total"] =
                    CString::ToByteStr(it2.second.first + it2.second.second);
            }
        }

        Tmpl["UserIn"] = CString::ToByteStr(Users.first);
        Tmpl["UserOut"] = CString::ToByteStr(Users.second);
        Tmpl["UserTotal"] = CString::ToByteStr(Users.first + Users.second);

        Tmpl["ZNCIn"] = CString::ToByteStr(ZNC.first);
        Tmpl["ZNCOut"] = CString::ToByteStr(ZNC.second);
        Tmpl["ZNCTotal"] = CString::ToByteStr(ZNC.first + ZNC.second);

        Tmpl["AllIn"] = CString::ToByteStr(Total.first);
        Tmpl["AllOut"] = CString::ToByteStr(Total.second);
        Tmpl["AllTotal"] = CString::ToByteStr(Total.first + Total.second);

        return true;
    }

    bool AddListener(CWebSock& WebSock, CTemplate& Tmpl) {
        unsigned short uPort = WebSock.GetParam("port").ToUShort();
        CString sHost = WebSock.GetParam("host");
        CString sURIPrefix = WebSock.GetParam("uriprefix");
        if (sHost == "*") sHost = "";
        bool bSSL = WebSock.GetParam("ssl").ToBool();
        bool bIPv4 = WebSock.GetParam("ipv4").ToBool();
        bool bIPv6 = WebSock.GetParam("ipv6").ToBool();
        bool bIRC = WebSock.GetParam("irc").ToBool();
        bool bWeb = WebSock.GetParam("web").ToBool();

        EAddrType eAddr = ADDR_ALL;
        if (bIPv4) {
            if (bIPv6) {
                eAddr = ADDR_ALL;
            } else {
                eAddr = ADDR_IPV4ONLY;
            }
        } else {
            if (bIPv6) {
                eAddr = ADDR_IPV6ONLY;
            } else {
                WebSock.GetSession()->AddError(
                    t_s("Choose either IPv4 or IPv6 or both."));
                return SettingsPage(WebSock, Tmpl);
            }
        }

        CListener::EAcceptType eAccept;
        if (bIRC) {
            if (bWeb) {
                eAccept = CListener::ACCEPT_ALL;
            } else {
                eAccept = CListener::ACCEPT_IRC;
            }
        } else {
            if (bWeb) {
                eAccept = CListener::ACCEPT_HTTP;
            } else {
                WebSock.GetSession()->AddError(
                    t_s("Choose either IRC or HTTP or both."));
                return SettingsPage(WebSock, Tmpl);
            }
        }

        CString sMessage;
        if (CZNC::Get().AddListener(uPort, sHost, sURIPrefix, bSSL, eAddr,
                                    eAccept, sMessage)) {
            if (!sMessage.empty()) {
                WebSock.GetSession()->AddSuccess(sMessage);
            }
            if (!CZNC::Get().WriteConfig()) {
                WebSock.GetSession()->AddError(
                    t_s("Port was changed, but config file was not written"));
            }
        } else {
            WebSock.GetSession()->AddError(sMessage);
        }

        return SettingsPage(WebSock, Tmpl);
    }

    bool DelListener(CWebSock& WebSock, CTemplate& Tmpl) {
        unsigned short uPort = WebSock.GetParam("port").ToUShort();
        CString sHost = WebSock.GetParam("host");
        bool bIPv4 = WebSock.GetParam("ipv4").ToBool();
        bool bIPv6 = WebSock.GetParam("ipv6").ToBool();

        EAddrType eAddr = ADDR_ALL;
        if (bIPv4) {
            if (bIPv6) {
                eAddr = ADDR_ALL;
            } else {
                eAddr = ADDR_IPV4ONLY;
            }
        } else {
            if (bIPv6) {
                eAddr = ADDR_IPV6ONLY;
            } else {
                WebSock.GetSession()->AddError(t_s("Invalid request."));
                return SettingsPage(WebSock, Tmpl);
            }
        }

        CListener* pListener = CZNC::Get().FindListener(uPort, sHost, eAddr);
        if (pListener) {
            CZNC::Get().DelListener(pListener);
            if (!CZNC::Get().WriteConfig()) {
                WebSock.GetSession()->AddError(
                    t_s("Port was changed, but config file was not written"));
            }
        } else {
            WebSock.GetSession()->AddError(
                t_s("The specified listener was not found."));
        }

        return SettingsPage(WebSock, Tmpl);
    }

    bool SettingsPage(CWebSock& WebSock, CTemplate& Tmpl) {
        Tmpl.SetFile("settings.tmpl");
        if (!WebSock.GetParam("submitted").ToUInt()) {
            Tmpl["Action"] = "settings";
            Tmpl["Title"] = t_s("Global Settings");
            Tmpl["StatusPrefix"] = CZNC::Get().GetStatusPrefix();
            Tmpl["MaxBufferSize"] = CString(CZNC::Get().GetMaxBufferSize());
            Tmpl["ConnectDelay"] = CString(CZNC::Get().GetConnectDelay());
            Tmpl["ServerThrottle"] = CString(CZNC::Get().GetServerThrottle());
            Tmpl["AnonIPLimit"] = CString(CZNC::Get().GetAnonIPLimit());
            Tmpl["ProtectWebSessions"] =
                CString(CZNC::Get().GetProtectWebSessions());
            Tmpl["HideVersion"] = CString(CZNC::Get().GetHideVersion());
            Tmpl["AuthOnlyViaModule"] = CString(CZNC::Get().GetAuthOnlyViaModule());

            const VCString& vsMotd = CZNC::Get().GetMotd();
            for (const CString& sMotd : vsMotd) {
                CTemplate& l = Tmpl.AddRow("MOTDLoop");
                l["Line"] = sMotd;
            }

            const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();
            for (const CListener* pListener : vpListeners) {
                CTemplate& l = Tmpl.AddRow("ListenLoop");

                l["Port"] = CString(pListener->GetPort());
                l["BindHost"] = pListener->GetBindHost();

                l["IsHTTP"] = CString(pListener->GetAcceptType() !=
                                     CListener::ACCEPT_IRC);
                l["IsIRC"] = CString(pListener->GetAcceptType() !=
                                     CListener::ACCEPT_HTTP);

                l["URIPrefix"] = pListener->GetURIPrefix() + "/";

                // simple protection for user from shooting his own foot
                // TODO check also for hosts/families
                // such check is only here, user still can forge HTTP request to
                // delete web port
                l["SuggestDeletion"] =
                    CString(pListener->GetPort() != WebSock.GetLocalPort());

#ifdef HAVE_LIBSSL
                if (pListener->IsSSL()) {
                    l["IsSSL"] = "true";
                }
#endif

#ifdef HAVE_IPV6
                switch (pListener->GetAddrType()) {
                    case ADDR_IPV4ONLY:
                        l["IsIPV4"] = "true";
                        break;
                    case ADDR_IPV6ONLY:
                        l["IsIPV6"] = "true";
                        break;
                    case ADDR_ALL:
                        l["IsIPV4"] = "true";
                        l["IsIPV6"] = "true";
                        break;
                }
#else
                l["IsIPV4"] = "true";
#endif
            }

            vector<CString> vDirs;
            WebSock.GetAvailSkins(vDirs);

            for (const CString& SubDir : vDirs) {
                CTemplate& l = Tmpl.AddRow("SkinLoop");
                l["Name"] = SubDir;

                if (SubDir == CZNC::Get().GetSkinName()) {
                    l["Checked"] = "true";
                }
            }

            set<CModInfo> ssGlobalMods;
            CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods,
                                                      CModInfo::GlobalModule);

            for (const CModInfo& Info : ssGlobalMods) {
                CTemplate& l = Tmpl.AddRow("ModuleLoop");

                CModule* pModule =
                    CZNC::Get().GetModules().FindModule(Info.GetName());
                if (pModule) {
                    l["Checked"] = "true";
                    l["Args"] = pModule->GetArgs();
                    if (CModInfo::GlobalModule == GetType() &&
                        Info.GetName() == GetModName()) {
                        l["Disabled"] = "true";
                    }
                }

                l["Name"] = Info.GetName();
                l["Description"] = Info.GetDescription();
                l["Wiki"] = Info.GetWikiPage();
                l["HasArgs"] = CString(Info.GetHasArgs());
                l["ArgsHelpText"] = Info.GetArgsHelpText();

                // Check if the module is loaded by all or some users, and/or by
                // all or some networks
                unsigned int usersWithRenderedModuleCount = 0;
                unsigned int networksWithRenderedModuleCount = 0;
                unsigned int networksCount = 0;
                const map<CString, CUser*>& allUsers = CZNC::Get().GetUserMap();
                for (const auto& it : allUsers) {
                    const CUser* pUser = it.second;

                    // Count users which has loaded a render module
                    const CModules& userModules = pUser->GetModules();
                    if (userModules.FindModule(Info.GetName())) {
                        usersWithRenderedModuleCount++;
                    }
                    // Count networks which has loaded a render module
                    const vector<CIRCNetwork*>& userNetworks =
                        pUser->GetNetworks();
                    networksCount += userNetworks.size();
                    for (const CIRCNetwork* pCurrentNetwork : userNetworks) {
                        if (pCurrentNetwork->GetModules().FindModule(
                                Info.GetName())) {
                            networksWithRenderedModuleCount++;
                        }
                    }
                }
                l["CanBeLoadedByNetwork"] =
                    CString(Info.SupportsType(CModInfo::NetworkModule));
                l["LoadedByAllNetworks"] =
                    CString(networksWithRenderedModuleCount == networksCount);
                l["LoadedBySomeNetworks"] =
                    CString(networksWithRenderedModuleCount != 0);

                l["CanBeLoadedByUser"] =
                    CString(Info.SupportsType(CModInfo::UserModule));
                l["LoadedByAllUsers"] =
                    CString(usersWithRenderedModuleCount == allUsers.size());
                l["LoadedBySomeUsers"] =
                    CString(usersWithRenderedModuleCount != 0);
            }

            return true;
        }

        CString sArg;
        sArg = WebSock.GetParam("statusprefix");
        CZNC::Get().SetStatusPrefix(sArg);
        sArg = WebSock.GetParam("maxbufsize");
        CZNC::Get().SetMaxBufferSize(sArg.ToUInt());
        sArg = WebSock.GetParam("connectdelay");
        CZNC::Get().SetConnectDelay(sArg.ToUInt());
        sArg = WebSock.GetParam("serverthrottle");
        CZNC::Get().SetServerThrottle(sArg.ToUInt());
        sArg = WebSock.GetParam("anoniplimit");
        CZNC::Get().SetAnonIPLimit(sArg.ToUInt());
        sArg = WebSock.GetParam("protectwebsessions");
        CZNC::Get().SetProtectWebSessions(sArg.ToBool());
        sArg = WebSock.GetParam("hideversion");
        CZNC::Get().SetHideVersion(sArg.ToBool());
        sArg = WebSock.GetParam("authonlyviamodule");
        CZNC::Get().SetAuthOnlyViaModule(sArg.ToBool());

        VCString vsArgs;
        WebSock.GetRawParam("motd").Split("\n", vsArgs);
        CZNC::Get().ClearMotd();

        for (const CString& sMotd : vsArgs) {
            CZNC::Get().AddMotd(sMotd.TrimRight_n());
        }

        CZNC::Get().SetSkinName(WebSock.GetParam("skin"));

        set<CString> ssArgs;
        WebSock.GetParamValues("loadmod", ssArgs);

        for (const CString& s : ssArgs) {
            CString sModRet;
            CString sModName = s.TrimRight_n("\r");
            CString sModLoadError;

            if (!sModName.empty()) {
                CString sArgs = WebSock.GetParam("modargs_" + sModName);

                CModule* pMod = CZNC::Get().GetModules().FindModule(sModName);
                if (!pMod) {
                    if (!CZNC::Get().GetModules().LoadModule(
                            sModName, sArgs, CModInfo::GlobalModule, nullptr,
                            nullptr, sModRet)) {
                        sModLoadError = t_f("Unable to load module [{1}]: {2}")(
                            sModName, sModRet);
                    }
                } else if (pMod->GetArgs() != sArgs) {
                    if (!CZNC::Get().GetModules().ReloadModule(
                            sModName, sArgs, nullptr, nullptr, sModRet)) {
                        sModLoadError =
                            t_f("Unable to reload module [{1}]: {2}")(sModName,
                                                                      sModRet);
                    }
                }

                if (!sModLoadError.empty()) {
                    DEBUG(sModLoadError);
                    WebSock.GetSession()->AddError(sModLoadError);
                }
            }
        }

        const CModules& vCurMods = CZNC::Get().GetModules();
        set<CString> ssUnloadMods;

        for (const CModule* pCurMod : vCurMods) {
            if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() &&
                (CModInfo::GlobalModule != GetType() ||
                 pCurMod->GetModName() != GetModName())) {
                ssUnloadMods.insert(pCurMod->GetModName());
            }
        }

        for (const CString& sMod : ssUnloadMods) {
            CZNC::Get().GetModules().UnloadModule(sMod);
        }

        if (!CZNC::Get().WriteConfig()) {
            WebSock.GetSession()->AddError(
                t_s("Settings were changed, but config file was not written"));
        }

        WebSock.Redirect(GetWebPath() + "settings");
        /* we don't want the template to be printed while we redirect */
        return false;
    }
};

template <>
void TModInfo<CWebAdminMod>(CModInfo& Info) {
    Info.AddType(CModInfo::UserModule);
    Info.SetWikiPage("webadmin");
}

GLOBALMODULEDEFS(CWebAdminMod, "Web based administration module.")
