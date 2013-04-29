/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
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

class CRegistartionMod : public CModule {
public:
	MODCONSTRUCTOR(CRegistartionMod) {
	}

	virtual ~CRegistartionMod() {
	}

	virtual bool OnLoad(const CString& sArgStr, CString& sMessage) {
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

		CUser* pNewUser = new CUser(sUsername);

		if (!sArg.empty()) {
			CString sSalt = CUtils::GetSalt();
			CString sHash = CUser::SaltedHash(sArg, sSalt);
			pNewUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
		}

		return pNewUser;
	}

	virtual CString GetWebMenuTitle() { return "Register"; }
	virtual bool WebRequiresLogin() { return false; }
	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();

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
			WebSock.PrintErrorPage("Invalid user settings");
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

};

GLOBALMODULEDEFS(CRegistartionMod, "Web based administration module")
