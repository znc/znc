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

#define MESSAGE "Your account has been disabled. Contact your administrator."

class CBlockUser : public CModule {
public:
	MODCONSTRUCTOR(CBlockUser) {
		AddHelpCommand();
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CBlockUser::OnListCommand), "", "List blocked users");
		AddCommand("Block", static_cast<CModCommand::ModCmdFunc>(&CBlockUser::OnBlockCommand), "<user>", "Block a user");
		AddCommand("Unblock", static_cast<CModCommand::ModCmdFunc>(&CBlockUser::OnUnblockCommand), "<user>", "Unblock a user");
	}

	virtual ~CBlockUser() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
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

	EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
		if (IsBlocked(Auth->GetUsername())) {
			Auth->RefuseLogin(MESSAGE);
			return HALT;
		}

		return CONTINUE;
	}

	void OnModCommand(const CString& sCommand) override {
		if (!GetUser()->IsAdmin()) {
			PutModule("Access denied");
		} else {
			HandleCommand(sCommand);
		}
	}

	void OnListCommand(const CString& sCommand) {
		CTable Table;
		MCString::iterator it;

		Table.AddColumn("Blocked user");

		for (it = BeginNV(); it != EndNV(); ++it) {
			Table.AddRow();
			Table.SetCell("Blocked user", it->first);
		}

		if (PutModule(Table) == 0)
			PutModule("No users blocked");
	}

	void OnBlockCommand(const CString& sCommand) {
		CString sUser = sCommand.Token(1, true);

		if (sUser.empty()) {
			PutModule("Usage: Block <user>");
			return;
		}

		if (GetUser()->GetUserName().Equals(sUser)) {
			PutModule("You can't block yourself");
			return;
		}

		if (Block(sUser))
			PutModule("Blocked [" + sUser + "]");
		else
			PutModule("Could not block [" + sUser + "] (misspelled?)");
	}

	void OnUnblockCommand(const CString& sCommand) {
		CString sUser = sCommand.Token(1, true);

		if (sUser.empty()) {
			PutModule("Usage: Unblock <user>");
			return;
		}

		if (DelNV(sUser))
			PutModule("Unblocked [" + sUser + "]");
		else
			PutModule("This user is not blocked");
	}

	bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
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
		vector<CClient*> vpClients = pUser->GetAllClients();
		vector<CClient*>::iterator it;
		for (it = vpClients.begin(); it != vpClients.end(); ++it) {
			(*it)->PutStatusNotice(MESSAGE);
			(*it)->Close(Csock::CLT_AFTERWRITE);
		}

		// Disconnect all networks from irc
		vector<CIRCNetwork*> vNetworks = pUser->GetNetworks();
		for (vector<CIRCNetwork*>::iterator it2 = vNetworks.begin(); it2 != vNetworks.end(); ++it2) {
			(*it2)->SetIRCConnectEnabled(false);
		}

		SetNV(pUser->GetUserName(), "");
		return true;
	}
};

template<> void TModInfo<CBlockUser>(CModInfo& Info) {
	Info.SetWikiPage("blockuser");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Enter one or more user names. Separate them by spaces.");
}

GLOBALMODULEDEFS(CBlockUser, "Block certain users from logging in.")
