/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "znc.h"

class CSendRaw_Mod: public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CSendRaw_Mod) {}

	virtual ~CSendRaw_Mod() {
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sUser = sLine.Token(0);
		CString sSend = sLine.Token(1, true);
		CUser *pUser;

		if (!m_pUser->IsAdmin()) {
			PutModule("You must have admin privileges to use this");
			return;
		}

		pUser = CZNC::Get().FindUser(sUser);

		if (!pUser) {
			PutModule("User not found");
			PutModule("The expected format is: <user> <line to send>");
			return;
		}

		pUser->PutIRC(sSend);
		PutModule("done");
	}
};

GLOBALMODULEDEFS(CSendRaw_Mod, "Let's you send some raw IRC lines as someone else");
