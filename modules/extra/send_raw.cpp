/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "znc.h"

class CSendRaw_Mod: public CModule {
public:
	MODCONSTRUCTOR(CSendRaw_Mod) {}

	virtual ~CSendRaw_Mod() {
	}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
		if (!m_pUser->IsAdmin()) {
			sErrorMsg = "You must have admin privileges to load this module";
			return false;
		}

		return true;
	}

	virtual void OnModCommand(const CString& sLine) {
		const CString sUser = sLine.Token(0);
		const CString sDirection = sLine.Token(1);
		CUser *pUser = CZNC::Get().FindUser(sUser);

		if (!pUser) {
			PutModule("User not found");
			PutModule("The expected format is: <user> [<in|out>] <line to send>");
			PutModule("Out (default): The line will be sent to the user's IRC server");
			PutModule("In: The line will be sent to the user's IRC client");
			return;
		}

		if (!sDirection.CaseCmp("in")) {
			pUser->PutUser(sLine.Token(2, true));
		} else if (!sDirection.CaseCmp("out")) {
			pUser->PutIRC(sLine.Token(2, true));
		} else {
			/* The user did not supply a direction, let's send the line out.
			We do this to preserve backwards compatibility. */
			pUser->PutIRC(sLine.Token(1, true));
		}

		PutModule("done");
	}
};

MODULEDEFS(CSendRaw_Mod, "Lets you send some raw IRC lines as/to someone else")
