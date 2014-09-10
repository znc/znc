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

class CBuddyListModule;

class CBuddyListTimer : CTimer {
public:
	CBuddyListTimer(CBuddyListModule *pModule);
	~CBuddyListTimer() {}

	void RunJob();

private:
	CBuddyListModule* m_pModule;
};

class CBuddyListModule : public CModule {
public:
	void Enable(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			EnableTimer();
			SetNV("State", "Enabled");
		} else {
			PutModule("syntax: Enable");
		}
	}

	void Disable(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			DisableTimer();
			SetNV("State", "Disabled");
		} else {
			PutModule("syntax: Disable");
		}
	}

	void AddBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sInfo = sLine.Token(2, true);

		if (!sNick.empty()) {
			SetNV("Buddy:" + sNick, sInfo);

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
			SetNV("Buddy:" + sNick, sInfo);

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
			CString sInfo = GetNV("Buddy:" + sOldNick);

			SetNV("Buddy:" + sNewNick, sInfo);
			DelNV("Buddy:" + sOldNick);

			PutModule(sOldNick + " has been renamed to " + sNewNick + " in your buddy list");
		} else {
			PutModule("syntax: Rename old_nick new_nick");
		}
	}

	void RemoveBuddy(const CString& sLine) {
		CString sNick = sLine.Token(1, false);
		CString sParams = sLine.Token(2, true);

		if (!sNick.empty() && sParams.empty()) {
			DelNV("Buddy:" + sNick);

			PutModule(sNick + " has been removed from your buddy list");
		} else {
			PutModule("syntax: Remove nick");
		}
	}

	void RemoveAllBuddies(const CString& sLine) {
		CString sParams = sLine.Token(1, true);

		if (sParams.empty()) {
			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				if (it->first.WildCmp("Buddy:*")) {
					DelNV(it->first);
				}
			}

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
				if (it->first.WildCmp("Buddy:" + sSearchString)) {
					table.AddRow();
					table.SetCell("Nick", it->first.Token(1, true, ":"));
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
				if (it->first.WildCmp("Buddy:*")) {
					table.AddRow();
					table.SetCell("Nick", it->first.Token(1, true, ":"));
					table.SetCell("Info", it->second);
				}
			}

			PutModule(table);
		} else {
			PutModule("syntax: ListAll");
		}
	}

	void PollServer() {
		if (!m_pTimer) {
			return;
		}

		CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();

		if (!pIRCSock) {
			return;
		}

		PutModule("TODO : Poll server");
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
			"Enable",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::Enable),
			"",
			"Enable the buddy list"
		);
		AddCommand
		(
			"Disable",
			static_cast<CModCommand::ModCmdFunc>(&CBuddyListModule::Disable),
			"",
			"Disable the buddy list"
		);
	}

	virtual ~CBuddyListModule() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (GetNV("State").empty()) {
			SetNV("State", "Enabled");
		}

		m_bIsEnabled = GetNV("Status").Equals("Enabled");
		m_pTimer = NULL;

		EnableTimer();

		return true;
	}

	void OnIRCDisconnected() {
		DisableTimer();
	}

	void OnIRCConnected() {
		EnableTimer();
	}

private:
	bool m_bIsEnabled;
	CBuddyListTimer* m_pTimer;

	void EnableTimer() {
		if (m_bIsEnabled && m_pNetwork->IsIRCConnected()) {
			if (m_pTimer) {
				return;
			}

			m_pTimer = new CBuddyListTimer(this);
			AddTimer(m_pTimer);
		}
	}

	void DisableTimer() {
		if (!m_pTimer) {
			return;
		}

		m_pTimer->Stop();
		RemTimer(m_pTimer);
		m_pTimer = NULL;
	}
};

template<> void TModInfo<CBuddyListModule>(CModInfo& Info) {
	Info.SetWikiPage("buddylist");
	Info.SetHasArgs(false);
}

CBuddyListTimer::CBuddyListTimer(CBuddyListModule *pModule) : CTimer(pModule, 30, 0,
		"BuddyListTimer", "Polls the server to find the status of buddies in the list") {
	m_pModule = pModule;
}

void CBuddyListTimer::RunJob() {
	m_pModule->PollServer();
}

NETWORKMODULEDEFS(CBuddyListModule, "Tells you when your buddies come online or go offline")
