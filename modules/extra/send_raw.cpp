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

	virtual CString GetWebMenuTitle() { return "Send Raw"; }
	virtual bool WebRequiresAdmin() { return true; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			if (WebSock.IsPost()) {
				CUser *pUser = CZNC::Get().FindUser(WebSock.GetParam("user"));
				bool bOutgoing = WebSock.GetParam("direction") == "out";
				const CString sLine = WebSock.GetParam("line");
 
				if (!pUser) {
					Tmpl["user"] = WebSock.GetParam("user");
					Tmpl[bOutgoing ? "direction_out" : "direction_in"] = "true";
					Tmpl["line"] = sLine;
					WebSock.GetSession()->AddError("User not found");
					return true;
				}

				if (bOutgoing) {
					pUser->PutIRC(sLine);
				} else {
					pUser->PutUser(sLine);
				}

				WebSock.GetSession()->AddSuccess("Line sent");
			}

			return true;
		}

		return false;
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
