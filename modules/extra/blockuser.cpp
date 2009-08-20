/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
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
		for (it2 = BeginNV(); it2 != EndNV(); it2++) {
			// Ignore errors
			Block(it2->first);
		}

		// Parse arguments, each argument is a user name to block
		sArgs.Split(" ", vArgs, false);

		for (it = vArgs.begin(); it != vArgs.end(); it++) {
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

		if (sCmd.Equals("list")) {
			CTable Table;
			MCString::iterator it;

			Table.AddColumn("Blocked user");

			for (it = BeginNV(); it != EndNV(); it++) {
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
		} else if (sCmd.Equals("help")) {
			PutModule("Commands: list, block [user], unblock [user]");
		}
	}

private:
	bool IsBlocked(const CString& sUser) {
		MCString::iterator it;
		for (it = BeginNV(); it != EndNV(); it++) {
			if (sUser.Equals(it->first)) {
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
		for (it = vpClients.begin(); it != vpClients.end(); it++) {
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

GLOBALMODULEDEFS(CBlockUser, "Block certain users from logging in")
