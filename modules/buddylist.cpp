/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#include <znc/Modules.h>
#include <znc/User.h>

class CBuddyList : public CModule {
public:
	void AddBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sInfo = sLine.Token(2, true);

		if (!sNick.empty()) {
			SetNV(sNick, sInfo);

			if (sInfo.empty()) {
				PutModule(sNick + " has been added to your buddy list");
			} else {
				PutModule(sNick + " has been added to your buddy list with info as follows:");
				PutModule("\t" + sInfo);
			}
		} else {
			PutModule("syntax: Add nick [info]");
		}
	}

	void RedefineBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sInfo = sLine.Token(2, true);

		if (!sNick.empty()) {
			SetNV(sNick, sInfo);

			if (sInfo.empty()) {
				PutModule("Info on " + sNick + " has been cleared");
			} else {
				PutModule("Info on " + sNick + " has been set to");
				PutModule("\t" + sInfo);
			}
		} else {
			PutModule("syntax: Redefine nick [info]");
		}
	}

	void RenameBuddy(const CString& sLine) {
		CString sOldNick = sLine.Token(1, false);
		CString sNewNick = sLine.Token(2, false);
		CString sParams = sLine.Token(3, true);

		if (!sOldNick.empty() && !sNewNick.empty() && sParams.empty()) {
			CString sInfo = GetNV(sOldNick);

			SetNV(sNewNick, sInfo);
			DelNV(sOldNick);

			PutModule(sOldNick + " has been renamed to " + sNewNick + " in your buddy list");
		} else {
			PutModule("syntax: Rename old_nick new_nick");
		}
	}

	void RemoveBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sParams = sLine.Token(2, true);

		if (!sNick.empty() && sParams.empty()) {
			DelNV(sNick);

			PutModule(sNick + " has been removed from your buddy list");
		} else {
			PutModule("syntax: Remove nick");
		}
	}

	void RemoveAllBuddies(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			ClearNV(true);
			PutModule("All buddies have been removed from your buddy list");
		} else {
			PutModule("syntax: RemoveAll");
		}
	}

	void ListBuddies(const CString& sLine) {
		CString sSearchString = sLine.Token(1, false);
		CString sParams = sLine.Token(2, true);

		if (!sSearchString.empty() && sParams.empty()) {
			CTable table;
			
			table.AddColumn("Nick");
			table.AddColumn("Info");
			
			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				if (it->first.WildCmp(sSearchString)) {
					table.AddRow();
					table.SetCell("Nick", it->first);
					table.SetCell("Info", it->second);
				}
			}

			PutModule(table);
		} else {
			PutModule("syntax: List search-string");
		}
	}

	void ListAllBuddies(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			CTable table;

			table.AddColumn("Nick");
			table.AddColumn("Info");

			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				table.AddRow();
				table.SetCell("Nick", it->first);
				table.SetCell("Info", it->second);
			}

			PutModule(table);
		} else {
			PutModule("syntax: ListAll");
		}
	}

	MODCONSTRUCTOR(CBuddyList) {
		AddHelpCommand();
		AddCommand
		(
			"Add",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::AddBuddy),
			"nick [info]",
			"Add buddy"
		);
		AddCommand
		(
			"Rename",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::RenameBuddy),
			"old_nick new_nick",
			"Rename buddy"
		);
		AddCommand
		(
			"Redefine",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::RedefineBuddy),
			"nick [info]",
			"Redefine info on buddy"
		);
		AddCommand
		(
			"Remove",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::RemoveBuddy),
			"nick",
			"Remove buddy"
		);
		AddCommand
		(
			"RemoveAll",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::RemoveAllBuddies),
			"",
			"Remove all buddies"
		);
		AddCommand
		(
			"List",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::ListBuddies),
			"search-string",
			"List all matching buddies"
		);
		AddCommand
		(
			"ListAll",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyList::ListAllBuddies),
			"",
			"List all buddies"
		);
	}

	virtual ~CBuddyList() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		return true;
	}
};

template<> void TModInfo<CBuddyList>(CModInfo& Info) {
	Info.SetWikiPage("buddy_list");
	Info.SetHasArgs(false);
}

NETWORKMODULEDEFS(CBuddyList, "Tells you when your buddies come online or go offline")
