/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

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
		GetClient()->PutClient(sData);
	}

public:
	virtual ~CSendRaw_Mod() {}

	bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
		if (!GetUser()->IsAdmin()) {
			sErrorMsg = "You must have admin privileges to load this module";
			return false;
		}

		return true;
	}

	CString GetWebMenuTitle() override { return "Send Raw"; }
	bool WebRequiresAdmin() override { return true; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
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
