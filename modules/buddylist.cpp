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
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

using std::vector;

class CBuddyListModule : public CModule {
public:
	void AddBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sInfo = sLine.Token(2, true);

		if (!sNick.empty()) {
			if (FindNV("Buddy:" + sNick) == EndNV()) {
				SetNV("Buddy:" + sNick, sInfo);

				if (sInfo.empty()) {
					PutModule(sNick + " has been added to your buddy list");
				} else {
					PutModule(sNick + " has been added to your buddy list with info as follows:");
					PutModule("\t" + sInfo);
				}
			} else {
				PutModule(sNick + " is already in the list; please use Rename or Redefine");
			}
		} else {
			PutModule("syntax: Add nick [info]");
		}
	}

	void RedefineBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sInfo = sLine.Token(2, true);

		if (!sNick.empty()) {
			if (FindNV("Buddy:" + sNick) != EndNV()) {
				SetNV("Buddy:" + sNick, sInfo);

				if (sInfo.empty()) {
					PutModule("Info on " + sNick + " has been cleared");
				} else {
					PutModule("Info on " + sNick + " has been set to");
					PutModule("\t" + sInfo);
				}
			} else {
				PutModule(sNick + " is not in your buddy list");
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
			if (FindNV("Buddy:" + sOldNick) != EndNV()) {
				if (FindNV("Buddy:" + sNewNick) == EndNV()) {
					CString sInfo = GetNV("Buddy:" + sOldNick);

					SetNV("Buddy:" + sNewNick, sInfo);
					DelNV("Buddy:" + sOldNick);

					PutModule(sOldNick + " has been renamed to " + sNewNick + " in your buddy list");
				} else {
					PutModule(sNewNick + " is already in your buddy list");
				}
			} else {
				PutModule(sOldNick + " is not in your buddy list");
			}
		} else {
			PutModule("syntax: Rename old_nick new_nick");
		}
	}

	void RemoveBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sParams = sLine.Token(2, true);

		if (!sNick.empty() && sParams.empty()) {
			if (FindNV("Buddy:" + sNick) != EndNV()) {
				DelNV("Buddy:" + sNick);

				PutModule(sNick + " has been removed from your buddy list");
			} else {
				PutModule(sNick + " is not in your buddy list");
			}
		} else {
			PutModule("syntax: Remove nick");
		}
	}

	void RemoveAllBuddies(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			RemoveBuddiesImpl("Buddy:*", "All buddies have been removed from your buddy list");
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
				if (it->first.WildCmp("Buddy:" + sSearchString)) {
					table.AddRow();
					table.SetCell("Nick", it->first.Token(1, true, ":"));
					table.SetCell("Info", it->second);
				}
			}

			if (table.size() > 0) {
				PutModule(table);
			} else {
				PutModule("Buddy list is empty or no matching buddies were found");
			}
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
				if (it->first.WildCmp("Buddy:*")) {
					table.AddRow();
					table.SetCell("Nick", it->first.Token(1, true, ":"));
					table.SetCell("Info", it->second);
				}
			}

			if (table.size() > 0) {
				PutModule(table);
			} else {
				PutModule("Buddy list is empty");
			}
		} else {
			PutModule("syntax: ListAll");
		}
	}

	void CheckBuddies(const CString& sLine) {
		CString sSearchString = sLine.Token(1, false);
		CString sParams = sLine.Token(2, true);

		if (!sSearchString.empty() && sParams.empty()) {
			m_sMessage = "No matching buddies are online";
			CheckBuddiesImpl("Buddy:" + sSearchString, "Buddy list is empty or no matching buddies were found");
		} else {
			PutModule("syntax: Check search-string");
		}
	}

	void CheckAllBuddies(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			m_sMessage = "No buddies are online";
			CheckBuddiesImpl("Buddy:*", "Buddy list is empty");
		} else {
			PutModule("syntax: CheckAll");
		}
	}

	MODCONSTRUCTOR(CBuddyListModule) {
		AddHelpCommand();
		AddCommand
		(
			"Add",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::AddBuddy),
			"nick [info]",
			"Add buddy"
		);
		AddCommand
		(
			"Rename",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::RenameBuddy),
			"old_nick new_nick",
			"Rename buddy"
		);
		AddCommand
		(
			"Redefine",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::RedefineBuddy),
			"nick [info]",
			"Redefine info on buddy"
		);
		AddCommand
		(
			"Remove",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::RemoveBuddy),
			"nick",
			"Remove buddy"
		);
		AddCommand
		(
			"RemoveAll",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::RemoveAllBuddies),
			"",
			"Remove all buddies"
		);
		AddCommand
		(
			"List",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::ListBuddies),
			"search-string",
			"List all matching buddies"
		);
		AddCommand
		(
			"ListAll",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::ListAllBuddies),
			"",
			"List all buddies"
		);
		AddCommand
		(
			"Check",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::CheckBuddies),
			"search-string",
			"Check all matching buddies if they are online"
		);
		AddCommand
		(
			"CheckAll",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::CheckAllBuddies),
			"",
			"Check all buddies if they are online"
		);
	}

	virtual ~CBuddyListModule() {
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		return true;
	}

	void OnIRCDisconnected() {
	}

	void OnIRCConnected() {
	}

	virtual EModRet OnRaw(CString &sLine) {
		if (sLine.Token(1, false) == "303") {
			VCString vsOnlineBuddies;
			CString sOnlineBuddies = sLine.Token(3, true);

			sOnlineBuddies.TrimPrefix();

			std::size_t nCount = sOnlineBuddies.Split(" ", vsOnlineBuddies, false);

			if (nCount > 0) {
				sOnlineBuddies = "";

				for (std::size_t nIndex = 0; nIndex < nCount; nIndex++) {
					sOnlineBuddies += vsOnlineBuddies[nIndex];

					if (nIndex < nCount - 1) {
						if (nIndex < nCount - 2) {
							sOnlineBuddies += ", ";
						} else {
							sOnlineBuddies += ", and ";
						}
					}
				}

				PutModule(sOnlineBuddies + (nCount == 1 ? " is online" : " are online"));
			} else {
				PutModule(m_sMessage);
				m_sMessage = "";
			}
		}

		return CONTINUE;
	}

private:
	CString m_sMessage;

	void RemoveBuddiesImpl(const CString& sMask, const CString& sError) {
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			if (it->first.WildCmp(sMask)) {
				DelNV(it->first);
			}
		}

		PutModule(sError);
	}

	void CheckBuddiesImpl(const CString& sMask, const CString& sError) {
		SCString ssBuddies;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			if (it->first.WildCmp(sMask)) {
				ssBuddies.insert(it->first.Token(1, true, ":"));
			}
		}

		if (ssBuddies.size() > 0) {
			CString sBuddies = "";
			CString sIsOnCommand = "ISON";

			for (SCString::iterator it = ssBuddies.begin(); it != ssBuddies.end(); ++it) {
				sBuddies += " " + *it;
			}

			PutModule("Checking" + sBuddies);
			PutIRC(sIsOnCommand + sBuddies);
		} else {
			PutModule(sError);
		}
	}
};

template<> void TModInfo<CBuddyListModule>(CModInfo& Info) {
	Info.SetWikiPage("buddylist");
	Info.SetHasArgs(false);
}

NETWORKMODULEDEFS(CBuddyListModule, "Tells you when your buddies come online or go offline")
