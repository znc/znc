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

	void SendClient(const CString& sLine) {
		CUser *pUser = CZNC::Get().FindUser(sLine.Token(1));

		if (pUser) {
			pUser->PutUser(sLine.Token(2, true));
			PutModule("Sent [" + sLine.Token(2, true) + "] to " + pUser->GetUserName());
		} else {
			PutModule("User [" + sLine.Token(1) + "] not found");
		}
	}

	void SendServer(const CString& sLine) {
		CUser *pUser = CZNC::Get().FindUser(sLine.Token(1));

		if (pUser) {
			pUser->PutIRC(sLine.Token(2, true));
			PutModule("Sent [" + sLine.Token(2, true) + "] to IRC Server of " + pUser->GetUserName());
		} else {
			PutModule("User [" + sLine.Token(1) + "] not found");
		}
	}

public:
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
				bool bToServer = WebSock.GetParam("send_to") == "server";
				const CString sLine = WebSock.GetParam("line");

				if (!pUser) {
					WebSock.GetSession()->AddError("User not found");
					return true;
				}

				Tmpl["user"] = pUser->GetUserName();
				Tmpl[bToServer ? "to_server" : "to_client"] = "true";
				Tmpl["line"] = sLine;

				if (bToServer) {
					pUser->PutIRC(sLine);
				} else {
					pUser->PutUser(sLine);
				}

				WebSock.GetSession()->AddSuccess("Line sent");
			}

			const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
			for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
				CTemplate& l = Tmpl.AddRow("UserLoop");
				l["Username"] = (*it->second).GetUserName();
			}

			return true;
		}

		return false;
	}

	/* This is here for backwards compatibility. We used to accept commands in this format:
	   <user> [<in|out>] <line> */
	virtual void OnUnknownModCommand(const CString& sLine) {
		const CString sUser = sLine.Token(0);
		const CString sDirection = sLine.Token(1);
		CUser *pUser = CZNC::Get().FindUser(sUser);

		if (!pUser) {
			/* Since the user doesn't exist we'll adopt the default action of this method */
			PutModule("Unknown command!");
			return;
		}

		if (sDirection.Equals("IN")) {
			SendClient("Client " + sUser + " " + sLine.Token(2, true));
		} else if (sDirection.Equals("OUT")) {
			SendServer("Server " + sUser + " " + sLine.Token(2, true));
		} else {
			/* No direction given -- assume it's out for compatibility */
			SendServer("Server " + sUser + " " + sLine.Token(1, true));
		}
	}

	MODCONSTRUCTOR(CSendRaw_Mod) {
		AddHelpCommand();
		AddCommand("Client",          static_cast<CModCommand::ModCmdFunc>(&CSendRaw_Mod::SendClient),
			"[user] [data to send]",  "The data will be sent to the user's IRC client(s)");
		AddCommand("Server",            static_cast<CModCommand::ModCmdFunc>(&CSendRaw_Mod::SendServer),
			"[user] [data to send]",  "The data will be sent to the IRC server the user is connected to");
	}
};

MODULEDEFS(CSendRaw_Mod, "Lets you send some raw IRC lines as/to someone else")
