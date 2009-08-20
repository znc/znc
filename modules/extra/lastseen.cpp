/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "znc.h"

using std::map;

class CLastSeenMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CLastSeenMod)
	{
	}

	virtual ~CLastSeenMod() {}

	time_t GetTime(CUser *pUser)
	{
		return GetNV(pUser->GetUserName()).ToULong();
	}

	void SetTime(CUser *pUser)
	{
		SetNV(pUser->GetUserName(), CString(time(NULL)));
	}

	virtual void OnModCommand(const CString& sLine)
	{
		const CString sCommand = sLine.Token(0).AsLower();

		if (!GetUser()->IsAdmin()) {
			PutModule("Access denied");
			return;
		}

		if (sCommand == "show") {
			char buf[1024];
			const map<CString, CUser*>& mUsers = CZNC::Get().GetUserMap();
			map<CString, CUser*>::const_iterator it;
			CTable Table;

			Table.AddColumn("User");
			Table.AddColumn("Last Seen");

			for (it = mUsers.begin(); it != mUsers.end(); it++) {
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

	virtual void OnClientLogin()
	{
		SetTime(GetUser());
	}

	virtual void OnClientDisconnect()
	{
		SetTime(GetUser());
	}

private:
};

GLOBALMODULEDEFS(CLastSeenMod, "Collects data about when a user last logged in")
