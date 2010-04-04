/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "Chan.h"
#include "znc.h"

using std::map;
using std::pair;
using std::multimap;

class CLastSeenMod : public CGlobalModule {
private:
	time_t GetTime(const CUser *pUser) {
		return GetNV(pUser->GetUserName()).ToULong();
	}

	void SetTime(const CUser *pUser) {
		SetNV(pUser->GetUserName(), CString(time(NULL)));
	}

	typedef multimap<time_t, CUser*> MTimeMulti;
	typedef map<CString, CUser*> MUsers;
public:
	GLOBALMODCONSTRUCTOR(CLastSeenMod) {
	}

	virtual ~CLastSeenMod() {}

	// IRC stuff:

	virtual void OnModCommand(const CString& sLine) {
		const CString sCommand = sLine.Token(0).AsLower();

		if (!GetUser()->IsAdmin()) {
			PutModule("Access denied");
			return;
		}

		if (sCommand == "show") {
			char buf[1024];
			const MUsers& mUsers = CZNC::Get().GetUserMap();
			MUsers::const_iterator it;
			CTable Table;

			Table.AddColumn("User");
			Table.AddColumn("Last Seen");

			for (it = mUsers.begin(); it != mUsers.end(); ++it) {
				CUser *pUser = it->second;
				time_t last = GetTime(pUser);

				Table.AddRow();
				Table.SetCell("User", it->first);

				if (last == 0)
					Table.SetCell("Last Seen", "never");
				else {
					strftime(buf, sizeof(buf), "%c", localtime(&last));
					Table.SetCell("Last Seen", buf);
				}
			}

			PutModule(Table);
		} else {
			PutModule("This module only supports 'show'");
		}
	}

	// Event stuff:

	virtual void OnClientLogin() {
		SetTime(GetUser());
	}

	virtual void OnClientDisconnect() {
		SetTime(GetUser());
	}

	virtual EModRet OnDeleteUser(CUser& User) {
		DelNV(User.GetUserName());
		return CONTINUE;
	}

	// Web stuff:

	virtual bool WebRequiresLogin() { return true; }
	virtual bool WebRequiresAdmin() { return true; }
	virtual CString GetWebMenuTitle() { return "Last Seen"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName.empty() || sPageName == "index") {
			MTimeMulti mmSorted;
			const MUsers& mUsers = CZNC::Get().GetUserMap();

			for (MUsers::const_iterator uit = mUsers.begin(); uit != mUsers.end(); ++uit) {
				mmSorted.insert(pair<time_t, CUser*>(GetTime(uit->second), uit->second));
			}

			char buf[1024] = {0};

			for (MTimeMulti::const_iterator it = mmSorted.begin(); it != mmSorted.end(); ++it) {
				CUser *pUser = it->second;
				CTemplate& Row = Tmpl.AddRow("UserLoop");

				Row["Username"] = pUser->GetUserName();
				Row["IsSelf"] = CString(pUser == WebSock.GetSession()->GetUser());

				if(it->first > 0) {
					strftime(buf, sizeof(buf), "%c", localtime(&it->first));
					Row["LastSeen"] = buf;
				}

				Row["Info"] = CString(pUser->GetClients().size()) + " client(s)";
				if(!pUser->GetCurrentServer()) {
					Row["Info"] += ", not connected to IRC";
				} else {
					unsigned int uChans = 0;
					const vector<CChan*>& vChans = pUser->GetChans();
					for (unsigned int a = 0; a < vChans.size(); ++a) {
						if (vChans[a]->IsOn()) ++uChans;
					}
					Row["Info"] += ", joined to " + CString(uChans) + " channel(s)";
				}
			}

			return true;
		}

		return false;
	}
};

GLOBALMODULEDEFS(CLastSeenMod, "Collects data about when a user last logged in")
