/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/znc.h>

using std::vector;
using std::map;

class CSendRaw_Mod: public CModule {
	void SendClient(const CString& sLine) {
		CUser *pUser = CZNC::Get().FindUser(sLine.Token(1));

		if (pUser) {
			CIRCNetwork *pNetwork = pUser->FindNetwork(sLine.Token(2));

			if (pNetwork) {
				pNetwork->PutUser(sLine.Token(3, true));
				PutModule("Sent [" + sLine.Token(3, true) + "] to " + pUser->GetUserName() + "/" + pNetwork->GetName());
			} else {
				PutModule("Network [" + sLine.Token(2) + "] not found for user [" + sLine.Token(1) + "]");
			}
		} else {
			PutModule("User [" + sLine.Token(1) + "] not found");
		}
	}

	void SendServer(const CString& sLine) {
		CUser *pUser = CZNC::Get().FindUser(sLine.Token(1));

		if (pUser) {
			CIRCNetwork *pNetwork = pUser->FindNetwork(sLine.Token(2));

			if (pNetwork) {
				pNetwork->PutIRC(sLine.Token(3, true));
				PutModule("Sent [" + sLine.Token(3, true) + "] to IRC Server of " + pUser->GetUserName() + "/" + pNetwork->GetName());
			} else {
				PutModule("Network [" + sLine.Token(2) + "] not found for user [" + sLine.Token(1) + "]");
			}
		} else {
			PutModule("User [" + sLine.Token(1) + "] not found");
		}
	}

	void CurrentClient(const CString& sLine) {
		CString sData = sLine.Token(1, true);
		m_pClient->PutClient(sData);
	}

public:
	virtual ~CSendRaw_Mod() {}

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
				CUser *pUser = CZNC::Get().FindUser(WebSock.GetParam("network").Token(0, false, "/"));
				if (!pUser) {
					WebSock.GetSession()->AddError("User not found");
					return true;
				}

				CIRCNetwork *pNetwork = pUser->FindNetwork(WebSock.GetParam("network").Token(1, false, "/"));
				if (!pNetwork) {
					WebSock.GetSession()->AddError("Network not found");
					return true;
				}

				bool bToServer = WebSock.GetParam("send_to") == "server";
				const CString sLine = WebSock.GetParam("line");

				Tmpl["user"] = pUser->GetUserName();
				Tmpl[bToServer ? "to_server" : "to_client"] = "true";
				Tmpl["line"] = sLine;

				if (bToServer) {
					pNetwork->PutIRC(sLine);
				} else {
					pNetwork->PutUser(sLine);
				}

				WebSock.GetSession()->AddSuccess("Line sent");
			}

			const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
			for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
				CTemplate& l = Tmpl.AddRow("UserLoop");
				l["Username"] = (*it->second).GetUserName();

				vector<CIRCNetwork*> vNetworks = (*it->second).GetNetworks();
				for (vector<CIRCNetwork*>::const_iterator it2 = vNetworks.begin(); it2 != vNetworks.end(); ++it2) {
					CTemplate& NetworkLoop = l.AddRow("NetworkLoop");
					NetworkLoop["Username"] = (*it->second).GetUserName();
					NetworkLoop["Network"] = (*it2)->GetName();
				}
			}

			return true;
		}

		return false;
	}

	MODCONSTRUCTOR(CSendRaw_Mod) {
		AddHelpCommand();
		AddCommand("Client",          static_cast<CModCommand::ModCmdFunc>(&CSendRaw_Mod::SendClient),
			"[user] [network] [data to send]",  "The data will be sent to the user's IRC client(s)");
		AddCommand("Server",          static_cast<CModCommand::ModCmdFunc>(&CSendRaw_Mod::SendServer),
			"[user] [network] [data to send]",  "The data will be sent to the IRC server the user is connected to");
		AddCommand("Current",         static_cast<CModCommand::ModCmdFunc>(&CSendRaw_Mod::CurrentClient),
			"[data to send]",                   "The data will be sent to your current client");
	}
};

template<> void TModInfo<CSendRaw_Mod>(CModInfo& Info) {
	Info.SetWikiPage("send_raw");
}

USERMODULEDEFS(CSendRaw_Mod, "Lets you send some raw IRC lines as/to someone else")
