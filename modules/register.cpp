/*
 * Copyright (C) 2004-2013	See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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

class CRegistrationMod : public CModule {
public:
	MODCONSTRUCTOR(CRegistrationMod) {
	}

	virtual ~CRegistrationMod() {
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		m_sSalt = sArgs;
		return true;
	}

	CUser* GetNewUser(CWebSock& WebSock) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();

		CString sUsername = WebSock.GetParam("user");
		if (sUsername.empty()) {
			WebSock.PrintErrorPage("Invalid Submission [Username is required]");
			return NULL;
		}

		CString sArg = WebSock.GetParam("password");

		if (sArg != WebSock.GetParam("password2")) {
			WebSock.PrintErrorPage("Invalid Submission [Passwords do not match]");
			return NULL;
		}

		if (!m_sSalt.empty()) {
			CString sCode = WebSock.GetParam("code");
			CString sHash = CString(m_sSalt + sUsername).MD5();

			if (sCode != sHash) {
				WebSock.PrintErrorPage("Invalid Submission [Invalid confirmation code]");
				return NULL;
			}
		}

		CUser* pNewUser = new CUser(sUsername);

		if (!sArg.empty()) {
			CString sSalt = CUtils::GetSalt();
			CString sHash = CUser::SaltedHash(sArg, sSalt);
			pNewUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
		}

		#ifdef REGISTER_HOST
		#define STR(x)   #x
		#define XSTR(x)  STR(x)
		#define STRING_REGISTER_HOST XSTR(REGISTER_HOST)
		CIRCNetwork *pNet = new CIRCNetwork(pNewUser, "default");
		pNet->AddServer(STRING_REGISTER_HOST);
		#endif

		return pNewUser;
	}

	virtual CString GetWebMenuTitle() { return "Register"; }
	virtual bool WebRequiresLogin() { return false; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();

		if (!m_sSalt.empty()) {
			Tmpl["Verify"] = "yes";
			Tmpl["Code"] = WebSock.GetParam("code", false);
			Tmpl["Username"] = WebSock.GetParam("user", false);
		}

		if (!WebSock.GetParam("submitted").ToUInt()) {
			return true;
		}

		CString sUsername = WebSock.GetParam("user");
		if (CZNC::Get().FindUser(sUsername)) {
			WebSock.PrintErrorPage("Invalid Submission [User " + sUsername + " already exists]");
			return true;
		}

		CUser* pNewUser = GetNewUser(WebSock);
		if (!pNewUser) {
			return true;
		}

		CString sErr;
		CString sAction;

		// Add User Submission
		if (!CZNC::Get().AddUser(pNewUser, sErr)) {
			delete pNewUser;
			WebSock.PrintErrorPage("Invalid submission [" + sErr + "]");
			return true;
		}

		sAction = "added";

		CTemplate TmplMod;
		TmplMod["Username"] = sUsername;
		TmplMod["WebadminAction"] = "change";

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("User " + sAction + ", but config was not written");
			return true;
		}

		spSession->SetUser(pNewUser);
		WebSock.SetLoggedIn(true);
		WebSock.UnPauseRead();
		WebSock.Redirect("/?cookie_check=true");
		return false;
	}
	private:
		CString m_sSalt;
};

template<> void TModInfo<CRegistrationMod>(CModInfo& Info) {
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("If a salt is provided, a verification code will be asked.");
}

GLOBALMODULEDEFS(CRegistrationMod, "User registration plugin")
