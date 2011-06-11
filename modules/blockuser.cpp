/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "IRCSock.h"
#include "znc.h"

#define MESSAGE "Your account has been disabled. Contact your administrator."

class CBlockUser : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CBlockUser) {}

	virtual ~CBlockUser() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		VCString vArgs;
		VCString::iterator it;
		MCString::iterator it2;

		// Load saved settings
		for (it2 = BeginNV(); it2 != EndNV(); ++it2) {
			// Ignore errors
			Block(it2->first);
		}

		// Parse arguments, each argument is a user name to block
		sArgs.Split(" ", vArgs, false);

		for (it = vArgs.begin(); it != vArgs.end(); ++it) {
			if (!Block(*it)) {
				sMessage = "Could not block [" + *it + "]";
				return false;
			}
		}

		return true;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		if (IsBlocked(Auth->GetUsername())) {
			Auth->RefuseLogin(MESSAGE);
			return HALT;
		}

		return CONTINUE;
	}

	void OnModCommand(const CString& sCommand) {
		CString sCmd = sCommand.Token(0);

		if (!m_pUser->IsAdmin()) {
			PutModule("Access denied");
			return;
		}

		if (sCmd.Equals("list")) {
			CTable Table;
			MCString::iterator it;

			Table.AddColumn("Blocked user");

			for (it = BeginNV(); it != EndNV(); ++it) {
				Table.AddRow();
				Table.SetCell("Blocked user", it->first);
			}

			if (PutModule(Table) == 0)
				PutModule("No users blocked");
		} else if (sCmd.Equals("block")) {
			CString sUser = sCommand.Token(1, true);

			if (m_pUser->GetUserName().Equals(sUser)) {
				PutModule("You can't block yourself");
				return;
			}

			if (Block(sUser))
				PutModule("Blocked [" + sUser + "]");
			else
				PutModule("Could not block [" + sUser + "] (misspelled?)");
		} else if (sCmd.Equals("unblock")) {
			CString sUser = sCommand.Token(1, true);

			if (DelNV(sUser))
				PutModule("Unblocked [" + sUser + "]");
			else
				PutModule("This user is not blocked");
		} else {
			PutModule("Commands: list, block [user], unblock [user]");
		}
	}

	bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "webadmin/user" && WebSock.GetSession()->IsAdmin()) {
			CString sAction = Tmpl["WebadminAction"];
			if (sAction == "display") {
				Tmpl["Blocked"] = CString(IsBlocked(Tmpl["Username"]));
				Tmpl["Self"] = CString(Tmpl["Username"].Equals(WebSock.GetSession()->GetUser()->GetUserName()));
				return true;
			}
			if (sAction == "change" && WebSock.GetParam("embed_blockuser_presented").ToBool()) {
				if (Tmpl["Username"].Equals(WebSock.GetSession()->GetUser()->GetUserName()) &&
						WebSock.GetParam("embed_blockuser_block").ToBool()) {
					WebSock.GetSession()->AddError("You can't block yourself");
				} else if (WebSock.GetParam("embed_blockuser_block").ToBool()) {
					if (!WebSock.GetParam("embed_blockuser_old").ToBool()) {
						if (Block(Tmpl["Username"])) {
							WebSock.GetSession()->AddSuccess("Blocked [" + Tmpl["Username"] + "]");
						} else {
							WebSock.GetSession()->AddError("Couldn't block [" + Tmpl["Username"] + "]");
						}
					}
				} else  if (WebSock.GetParam("embed_blockuser_old").ToBool()){
					if (DelNV(Tmpl["Username"])) {
						WebSock.GetSession()->AddSuccess("Unblocked [" + Tmpl["Username"] + "]");
					} else {
						WebSock.GetSession()->AddError("User [" + Tmpl["Username"] + "is not blocked");
					}
				}
				return true;
			}
		}
		return false;
	}

private:
	bool IsBlocked(const CString& sUser) {
		MCString::iterator it;
		for (it = BeginNV(); it != EndNV(); ++it) {
			if (sUser == it->first) {
				return true;
			}
		}
		return false;
	}

	bool Block(const CString& sUser) {
		CUser *pUser = CZNC::Get().FindUser(sUser);

		if (!pUser)
			return false;

		// Disconnect all clients
		vector<CClient*>& vpClients = pUser->GetClients();
		vector<CClient*>::iterator it;
		for (it = vpClients.begin(); it != vpClients.end(); ++it) {
			(*it)->PutStatusNotice(MESSAGE);
			(*it)->Close(Csock::CLT_AFTERWRITE);
		}

		// Disconnect from IRC...
		CIRCSock *pIRCSock = pUser->GetIRCSock();
		if (pIRCSock) {
			pIRCSock->Quit();
		}

		// ...and don't reconnect
		pUser->SetIRCConnectEnabled(false);

		SetNV(pUser->GetUserName(), "");
		return true;
	}
};

template<> void TModInfo<CBlockUser>(CModInfo& Info) {
	Info.SetWikiPage("blockuser");
}

GLOBALMODULEDEFS(CBlockUser, "Block certain users from logging in")
