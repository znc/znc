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

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Chan.h>

using std::map;
using std::set;

class CAutoVoiceUser {
public:
	CAutoVoiceUser() {}

	CAutoVoiceUser(const CString& sLine) {
		FromString(sLine);
	}

	CAutoVoiceUser(const CString& sUsername, const CString& sHostmask, const CString& sChannels) :
			m_sUsername(sUsername),
			m_sHostmask(sHostmask) {
		AddChans(sChannels);
	}

	virtual ~CAutoVoiceUser() {}

	const CString& GetUsername() const { return m_sUsername; }
	const CString& GetHostmask() const { return m_sHostmask; }

	bool ChannelMatches(const CString& sChan) const {
		for (const CString& s : m_ssChans) {
			if (sChan.AsLower().WildCmp(s, CString::CaseInsensitive)) {
				return true;
			}
		}

		return false;
	}

	bool HostMatches(const CString& sHostmask) {
		return sHostmask.WildCmp(m_sHostmask, CString::CaseInsensitive);
	}

	CString GetChannels() const {
		CString sRet;

		for (const CString& sChan : m_ssChans) {
			if (!sRet.empty()) {
				sRet += " ";
			}

			sRet += sChan;
		}

		return sRet;
	}

	void DelChans(const CString& sChans) {
		VCString vsChans;
		sChans.Split(" ", vsChans);

		for (const CString& sChan : vsChans) {
			m_ssChans.erase(sChan.AsLower());
		}
	}

	void AddChans(const CString& sChans) {
		VCString vsChans;
		sChans.Split(" ", vsChans);

		for (const CString& sChan : vsChans) {
			m_ssChans.insert(sChan.AsLower());
		}
	}

	CString ToString() const {
		CString sChans;

		for (const CString& sChan : m_ssChans) {
			if (!sChans.empty()) {
				sChans += " ";
			}

			sChans += sChan;
		}

		return m_sUsername + "\t" + m_sHostmask + "\t" + sChans;
	}

	bool FromString(const CString& sLine) {
		m_sUsername = sLine.Token(0, false, "\t");
		m_sHostmask = sLine.Token(1, false, "\t");
		sLine.Token(2, false, "\t").Split(" ", m_ssChans);

		return !m_sHostmask.empty();
	}
private:
protected:
	CString      m_sUsername;
	CString      m_sHostmask;
	set<CString> m_ssChans;
};

class CAutoVoiceMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoVoiceMod) {
		AddHelpCommand();
		AddCommand("ListUsers", static_cast<CModCommand::ModCmdFunc>(&CAutoVoiceMod::OnListUsersCommand), "", "List all users");
		AddCommand("AddChans", static_cast<CModCommand::ModCmdFunc>(&CAutoVoiceMod::OnAddChansCommand), "<user> <channel> [channel] ...", "Adds channels to a user");
		AddCommand("DelChans", static_cast<CModCommand::ModCmdFunc>(&CAutoVoiceMod::OnDelChansCommand), "<user> <channel> [channel] ...", "Removes channels from a user");
		AddCommand("AddUser", static_cast<CModCommand::ModCmdFunc>(&CAutoVoiceMod::OnAddUserCommand), "<user> <hostmask> [channels]", "Adds a user");
		AddCommand("DelUser", static_cast<CModCommand::ModCmdFunc>(&CAutoVoiceMod::OnDelUserCommand), "<user>", "Removes a user");
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		// Load the chans from the command line
		unsigned int a = 0;
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (const CString& sChan : vsChans) {
			CString sName = "Args";
			sName += CString(a);
			AddUser(sName, "*", sChan);
		}

		// Load the saved users
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			const CString& sLine = it->second;
			CAutoVoiceUser* pUser = new CAutoVoiceUser;

			if (!pUser->FromString(sLine) || FindUser(pUser->GetUsername().AsLower())) {
				delete pUser;
			} else {
				m_msUsers[pUser->GetUsername().AsLower()] = pUser;
			}
		}

		return true;
	}

	virtual ~CAutoVoiceMod() {
		for (const auto& it : m_msUsers) {
			delete it.second;
		}

		m_msUsers.clear();
	}

	void OnJoin(const CNick& Nick, CChan& Channel) override {
		// If we have ops in this chan
		if (Channel.HasPerm(CChan::Op) || Channel.HasPerm(CChan::HalfOp)) {
			for (const auto& it : m_msUsers) {
				// and the nick who joined is a valid user
				if (it.second->HostMatches(Nick.GetHostMask()) && it.second->ChannelMatches(Channel.GetName())) {
					PutIRC("MODE " + Channel.GetName() + " +v " + Nick.GetNick());
					break;
				}
			}
		}
	}

	void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		if (Nick.NickEquals(GetNetwork()->GetNick())) {
			const map<CString,CNick>& msNicks = Channel.GetNicks();

			for (const auto& it : msNicks) {
				if (!it.second.HasPerm(CChan::Voice)) {
					CheckAutoVoice(it.second, Channel);
				}
			}
		}
	}

	bool CheckAutoVoice(const CNick& Nick, CChan& Channel) {
		CAutoVoiceUser *pUser = FindUserByHost(Nick.GetHostMask(), Channel.GetName());
		if (!pUser) {
			return false;
		}

		PutIRC("MODE " + Channel.GetName() + " +v " + Nick.GetNick());
		return true;
	}

	void OnAddUserCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sHost = sLine.Token(2);

		if (sHost.empty()) {
			PutModule("Usage: AddUser <user> <hostmask> [channels]");
		} else {
			CAutoVoiceUser* pUser = AddUser(sUser, sHost, sLine.Token(3, true));

			if (pUser) {
				SetNV(sUser, pUser->ToString());
			}
		}
	}

	void OnDelUserCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);

		if (sUser.empty()) {
			PutModule("Usage: DelUser <user>");
		} else {
			DelUser(sUser);
			DelNV(sUser);
		}
	}

	void OnListUsersCommand(const CString& sLine) {
		if (m_msUsers.empty()) {
			PutModule("There are no users defined");
			return;
		}

		CTable Table;

		Table.AddColumn("User");
		Table.AddColumn("Hostmask");
		Table.AddColumn("Channels");

		for (const auto& it : m_msUsers) {
			Table.AddRow();
			Table.SetCell("User", it.second->GetUsername());
			Table.SetCell("Hostmask", it.second->GetHostmask());
			Table.SetCell("Channels", it.second->GetChannels());
		}

		PutModule(Table);
	}

	void OnAddChansCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sChans = sLine.Token(2, true);

		if (sChans.empty()) {
			PutModule("Usage: AddChans <user> <channel> [channel] ...");
			return;
		}

		CAutoVoiceUser* pUser = FindUser(sUser);

		if (!pUser) {
			PutModule("No such user");
			return;
		}

		pUser->AddChans(sChans);
		PutModule("Channel(s) added to user [" + pUser->GetUsername() + "]");

		SetNV(pUser->GetUsername(), pUser->ToString());
	}

	void OnDelChansCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sChans = sLine.Token(2, true);

		if (sChans.empty()) {
			PutModule("Usage: DelChans <user> <channel> [channel] ...");
			return;
		}

		CAutoVoiceUser* pUser = FindUser(sUser);

		if (!pUser) {
			PutModule("No such user");
			return;
		}

		pUser->DelChans(sChans);
		PutModule("Channel(s) Removed from user [" + pUser->GetUsername() + "]");

		SetNV(pUser->GetUsername(), pUser->ToString());
	}

	CAutoVoiceUser* FindUser(const CString& sUser) {
		map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.find(sUser.AsLower());

		return (it != m_msUsers.end()) ? it->second : nullptr;
	}

	CAutoVoiceUser* FindUserByHost(const CString& sHostmask, const CString& sChannel = "") {
		for (const auto& it : m_msUsers) {
			CAutoVoiceUser* pUser = it.second;

			if (pUser->HostMatches(sHostmask) && (sChannel.empty() || pUser->ChannelMatches(sChannel))) {
				return pUser;
			}
		}

		return nullptr;
	}

	void DelUser(const CString& sUser) {
		map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.find(sUser.AsLower());

		if (it == m_msUsers.end()) {
			PutModule("That user does not exist");
			return;
		}

		delete it->second;
		m_msUsers.erase(it);
		PutModule("User [" + sUser + "] removed");
	}

	CAutoVoiceUser* AddUser(const CString& sUser, const CString& sHost, const CString& sChans) {
		if (m_msUsers.find(sUser) != m_msUsers.end()) {
			PutModule("That user already exists");
			return nullptr;
		}

		CAutoVoiceUser* pUser = new CAutoVoiceUser(sUser, sHost, sChans);
		m_msUsers[sUser.AsLower()] = pUser;
		PutModule("User [" + sUser + "] added with hostmask [" + sHost + "]");
		return pUser;
	}

private:
	map<CString, CAutoVoiceUser*> m_msUsers;
};

template<> void TModInfo<CAutoVoiceMod>(CModInfo& Info) {
	Info.SetWikiPage("autovoice");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Each argument is either a channel you want autovoice for (which can include wildcards) or, if it starts with !, it is an exception for autovoice.");
}

NETWORKMODULEDEFS(CAutoVoiceMod, "Auto voice the good people")
