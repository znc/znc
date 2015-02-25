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
#include <znc/znc.h>

using std::map;
using std::pair;
using std::multimap;

class CLastSeenMod : public CModule {
private:
	time_t GetTime(const CUser *pUser) {
		return GetNV(pUser->GetUserName()).ToULong();
	}

	void SetTime(const CUser *pUser) {
		SetNV(pUser->GetUserName(), CString(time(NULL)));
	}

	const CString FormatLastSeen(const CUser *pUser, const char* sDefault = "") {
		time_t last = GetTime(pUser);
		if (last < 1) {
			return sDefault;
		} else {
			char buf[1024];
			strftime(buf, sizeof(buf) - 1, "%c", localtime(&last));
			return buf;
		}
	}

	typedef multimap<time_t, CUser*> MTimeMulti;
	typedef map<CString, CUser*> MUsers;

	void ShowCommand(const CString &sLine) {
		if (!GetUser()->IsAdmin()) {
			PutModule("Access denied");
			return;
		}

		const MUsers& mUsers = CZNC::Get().GetUserMap();
		MUsers::const_iterator it;
		CTable Table;

		Table.AddColumn("User");
		Table.AddColumn("Last Seen");

		for (it = mUsers.begin(); it != mUsers.end(); ++it) {
			Table.AddRow();
			Table.SetCell("User", it->first);
			Table.SetCell("Last Seen", FormatLastSeen(it->second, "never"));
		}

		PutModule(Table);
	}

public:
	MODCONSTRUCTOR(CLastSeenMod) {
		AddHelpCommand();
		AddCommand("Show", static_cast<CModCommand::ModCmdFunc>(&CLastSeenMod::ShowCommand));
	}

	virtual ~CLastSeenMod() {}

	// Event stuff:

	void OnClientLogin() override {
		SetTime(GetUser());
	}

	void OnClientDisconnect() override {
		SetTime(GetUser());
	}

	EModRet OnDeleteUser(CUser& User) override {
		DelNV(User.GetUserName());
		return CONTINUE;
	}

	// Web stuff:

	bool WebRequiresAdmin() override { return true; }
	CString GetWebMenuTitle() override { return "Last Seen"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "index") {
			CModules& GModules = CZNC::Get().GetModules();
			Tmpl["WebAdminLoaded"] = CString(GModules.FindModule("webadmin") != NULL);

			MTimeMulti mmSorted;
			const MUsers& mUsers = CZNC::Get().GetUserMap();

			for (MUsers::const_iterator uit = mUsers.begin(); uit != mUsers.end(); ++uit) {
				mmSorted.insert(pair<time_t, CUser*>(GetTime(uit->second), uit->second));
			}

			for (MTimeMulti::const_iterator it = mmSorted.begin(); it != mmSorted.end(); ++it) {
				CUser *pUser = it->second;
				CTemplate& Row = Tmpl.AddRow("UserLoop");

				Row["Username"] = pUser->GetUserName();
				Row["IsSelf"] = CString(pUser == WebSock.GetSession()->GetUser());
				Row["LastSeen"] = FormatLastSeen(pUser, "never");
			}

			return true;
		}

		return false;
	}

	bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "webadmin/user" && WebSock.GetSession()->IsAdmin()) {
			CUser* pUser = CZNC::Get().FindUser(Tmpl["Username"]);
			if (pUser) {
				Tmpl["LastSeen"] = FormatLastSeen(pUser);
			}
			return true;
		}

		return false;
	}

};

template<> void TModInfo<CLastSeenMod>(CModInfo& Info) {
	Info.SetWikiPage("lastseen");
}

GLOBALMODULEDEFS(CLastSeenMod, "Collects data about when a user last logged in.")
