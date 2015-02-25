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
#include <znc/Chan.h>

using std::map;
using std::set;
using std::vector;

class CAutoOpMod;

#define AUTOOP_CHALLENGE_LENGTH 32

class CAutoOpTimer : public CTimer {
public:

	CAutoOpTimer(CAutoOpMod* pModule)
			: CTimer((CModule*) pModule, 20, 0, "AutoOpChecker", "Check channels for auto op candidates") {
		m_pParent = pModule;
	}

	virtual ~CAutoOpTimer() {}

private:
protected:
	void RunJob() override;

	CAutoOpMod* m_pParent;
};

class CAutoOpUser {
public:
	CAutoOpUser() {}

	CAutoOpUser(const CString& sLine) {
		FromString(sLine);
	}

	CAutoOpUser(const CString& sUsername, const CString& sUserKey, const CString& sHostmasks, const CString& sChannels) :
			m_sUsername(sUsername),
			m_sUserKey(sUserKey) {
		AddHostmasks(sHostmasks);
		AddChans(sChannels);
	}

	virtual ~CAutoOpUser() {}

	const CString& GetUsername() const { return m_sUsername; }
	const CString& GetUserKey() const { return m_sUserKey; }

	bool ChannelMatches(const CString& sChan) const {
		for (set<CString>::const_iterator it = m_ssChans.begin(); it != m_ssChans.end(); ++it) {
			if (sChan.AsLower().WildCmp(*it, CString::CaseInsensitive)) {
				return true;
			}
		}

		return false;
	}

	bool HostMatches(const CString& sHostmask) {
		for (set<CString>::const_iterator it = m_ssHostmasks.begin(); it != m_ssHostmasks.end(); ++it) {
			if (sHostmask.WildCmp(*it, CString::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}

	CString GetHostmasks() const {
		return CString(",").Join(m_ssHostmasks.begin(), m_ssHostmasks.end());
	}

	CString GetChannels() const {
		return CString(" ").Join(m_ssChans.begin(), m_ssChans.end());
	}

	bool DelHostmasks(const CString& sHostmasks) {
		VCString vsHostmasks;
		sHostmasks.Split(",", vsHostmasks);

		for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
			m_ssHostmasks.erase(vsHostmasks[a]);
		}

		return m_ssHostmasks.empty();
	}

	void AddHostmasks(const CString& sHostmasks) {
		VCString vsHostmasks;
		sHostmasks.Split(",", vsHostmasks);

		for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
			m_ssHostmasks.insert(vsHostmasks[a]);
		}
	}

	void DelChans(const CString& sChans) {
		VCString vsChans;
		sChans.Split(" ", vsChans);

		for (unsigned int a = 0; a < vsChans.size(); a++) {
			m_ssChans.erase(vsChans[a].AsLower());
		}
	}

	void AddChans(const CString& sChans) {
		VCString vsChans;
		sChans.Split(" ", vsChans);

		for (unsigned int a = 0; a < vsChans.size(); a++) {
			m_ssChans.insert(vsChans[a].AsLower());
		}
	}

	CString ToString() const {
		return m_sUsername + "\t" + GetHostmasks() + "\t" + m_sUserKey + "\t" + GetChannels();
	}

	bool FromString(const CString& sLine) {
		m_sUsername = sLine.Token(0, false, "\t");
		sLine.Token(1, false, "\t").Split(",", m_ssHostmasks);
		m_sUserKey = sLine.Token(2, false, "\t");
		sLine.Token(3, false, "\t").Split(" ", m_ssChans);

		return !m_sUserKey.empty();
	}
private:
protected:
	CString      m_sUsername;
	CString      m_sUserKey;
	set<CString> m_ssHostmasks;
	set<CString> m_ssChans;
};

class CAutoOpMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoOpMod) {
		AddHelpCommand();
		AddCommand("ListUsers", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnListUsersCommand), "", "List all users");
		AddCommand("AddChans", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnAddChansCommand), "<user> <channel> [channel] ...", "Adds channels to a user");
		AddCommand("DelChans", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnDelChansCommand), "<user> <channel> [channel] ...", "Removes channels from a user");
		AddCommand("AddMasks", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnAddMasksCommand), "<user> <mask>,[mask] ...", "Adds masks to a user");
		AddCommand("DelMasks", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnDelMasksCommand), "<user> <mask>,[mask] ...", "Removes masks from a user");
		AddCommand("AddUser", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnAddUserCommand), "<user> <hostmask>[,<hostmasks>...] <key> [channels]", "Adds a user");
		AddCommand("DelUser", static_cast<CModCommand::ModCmdFunc>(&CAutoOpMod::OnDelUserCommand), "<user>", "Removes a user");
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		AddTimer(new CAutoOpTimer(this));

		// Load the users
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			const CString& sLine = it->second;
			CAutoOpUser* pUser = new CAutoOpUser;

			if (!pUser->FromString(sLine) || FindUser(pUser->GetUsername().AsLower())) {
				delete pUser;
			} else {
				m_msUsers[pUser->GetUsername().AsLower()] = pUser;
			}
		}

		return true;
	}

	virtual ~CAutoOpMod() {
		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			delete it->second;
		}
		m_msUsers.clear();
	}

	void OnJoin(const CNick& Nick, CChan& Channel) override {
		// If we have ops in this chan
		if (Channel.HasPerm(CChan::Op)) {
			CheckAutoOp(Nick, Channel);
		}
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		MCString::iterator it = m_msQueue.find(Nick.GetNick().AsLower());

		if (it != m_msQueue.end()) {
			m_msQueue.erase(it);
		}
	}

	void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) override {
		// Update the queue with nick changes
		MCString::iterator it = m_msQueue.find(OldNick.GetNick().AsLower());

		if (it != m_msQueue.end()) {
			m_msQueue[sNewNick.AsLower()] = it->second;
			m_msQueue.erase(it);
		}
	}

	EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
		if (!sMessage.Token(0).Equals("!ZNCAO")) {
			return CONTINUE;
		}

		CString sCommand = sMessage.Token(1);

		if (sCommand.Equals("CHALLENGE")) {
			ChallengeRespond(Nick, sMessage.Token(2));
		} else if (sCommand.Equals("RESPONSE")) {
			VerifyResponse(Nick, sMessage.Token(2));
		}

		return HALTCORE;
	}

	void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		if (Nick.GetNick() == GetNetwork()->GetIRCNick().GetNick()) {
			const map<CString,CNick>& msNicks = Channel.GetNicks();

			for (map<CString,CNick>::const_iterator it = msNicks.begin(); it != msNicks.end(); ++it) {
				if (!it->second.HasPerm(CChan::Op)) {
					CheckAutoOp(it->second, Channel);
				}
			}
		}
	}

	void OnModCommand(const CString& sLine) override {
		CString sCommand = sLine.Token(0).AsUpper();
		if (sCommand.Equals("TIMERS")) {
			// for testing purposes - hidden from help
			ListTimers();
		} else {
			HandleCommand(sLine);
		}
	}

	void OnAddUserCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sHost = sLine.Token(2);
		CString sKey = sLine.Token(3);

		if (sHost.empty()) {
			PutModule("Usage: AddUser <user> <hostmask>[,<hostmasks>...] <key> [channels]");
		} else {
			CAutoOpUser* pUser = AddUser(sUser, sKey, sHost, sLine.Token(4, true));

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
		Table.AddColumn("Hostmasks");
		Table.AddColumn("Key");
		Table.AddColumn("Channels");

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			VCString vsHostmasks;
			it->second->GetHostmasks().Split(",", vsHostmasks);
			for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
				Table.AddRow();
				if (a == 0) {
					Table.SetCell("User", it->second->GetUsername());
					Table.SetCell("Key", it->second->GetUserKey());
					Table.SetCell("Channels", it->second->GetChannels());
				} else if (a == vsHostmasks.size()-1) {
					Table.SetCell("User", "`-");
				} else {
					Table.SetCell("User", "|-");
				}
				Table.SetCell("Hostmasks", vsHostmasks[a]);
			}
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

		CAutoOpUser* pUser = FindUser(sUser);

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

		CAutoOpUser* pUser = FindUser(sUser);

		if (!pUser) {
			PutModule("No such user");
			return;
		}

		pUser->DelChans(sChans);
		PutModule("Channel(s) Removed from user [" + pUser->GetUsername() + "]");
		SetNV(pUser->GetUsername(), pUser->ToString());
	}

	void OnAddMasksCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sHostmasks = sLine.Token(2, true);

		if (sHostmasks.empty()) {
			PutModule("Usage: AddMasks <user> <mask>,[mask] ...");
			return;
		}

		CAutoOpUser* pUser = FindUser(sUser);

		if (!pUser) {
			PutModule("No such user");
			return;
		}

		pUser->AddHostmasks(sHostmasks);
		PutModule("Hostmasks(s) added to user [" + pUser->GetUsername() + "]");
		SetNV(pUser->GetUsername(), pUser->ToString());
	}

	void OnDelMasksCommand(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sHostmasks = sLine.Token(2, true);

		if (sHostmasks.empty()) {
			PutModule("Usage: DelMasks <user> <mask>,[mask] ...");
			return;
		}

		CAutoOpUser* pUser = FindUser(sUser);

		if (!pUser) {
			PutModule("No such user");
			return;
		}

		if (pUser->DelHostmasks(sHostmasks)) {
			PutModule("Removed user [" + pUser->GetUsername() + "] with key [" + pUser->GetUserKey() + "] and channels [" + pUser->GetChannels() + "]");
			DelUser(sUser);
			DelNV(sUser);
		} else {
			PutModule("Hostmasks(s) Removed from user [" + pUser->GetUsername() + "]");
			SetNV(pUser->GetUsername(), pUser->ToString());
		}
	}

	CAutoOpUser* FindUser(const CString& sUser) {
		map<CString, CAutoOpUser*>::iterator it = m_msUsers.find(sUser.AsLower());

		return (it != m_msUsers.end()) ? it->second : NULL;
	}

	CAutoOpUser* FindUserByHost(const CString& sHostmask, const CString& sChannel = "") {
		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			CAutoOpUser* pUser = it->second;

			if (pUser->HostMatches(sHostmask) && (sChannel.empty() || pUser->ChannelMatches(sChannel))) {
				return pUser;
			}
		}

		return NULL;
	}

	bool CheckAutoOp(const CNick& Nick, CChan& Channel) {
		CAutoOpUser *pUser = FindUserByHost(Nick.GetHostMask(), Channel.GetName());

		if (!pUser) {
			return false;
		}

		if (pUser->GetUserKey().Equals("__NOKEY__")) {
			PutIRC("MODE " + Channel.GetName() + " +o " + Nick.GetNick());
		} else {
			// then insert this nick into the queue, the timer does the rest
			CString sNick = Nick.GetNick().AsLower();
			if (m_msQueue.find(sNick) == m_msQueue.end()) {
				m_msQueue[sNick] = "";
			}
		}

		return true;
	}

	void DelUser(const CString& sUser) {
		map<CString, CAutoOpUser*>::iterator it = m_msUsers.find(sUser.AsLower());

		if (it == m_msUsers.end()) {
			PutModule("That user does not exist");
			return;
		}

		delete it->second;
		m_msUsers.erase(it);
		PutModule("User [" + sUser + "] removed");
	}

	CAutoOpUser* AddUser(const CString& sUser, const CString& sKey, const CString& sHosts, const CString& sChans) {
		if (m_msUsers.find(sUser) != m_msUsers.end()) {
			PutModule("That user already exists");
			return NULL;
		}

		CAutoOpUser* pUser = new CAutoOpUser(sUser, sKey, sHosts, sChans);
		m_msUsers[sUser.AsLower()] = pUser;
		PutModule("User [" + sUser + "] added with hostmask(s) [" + sHosts + "]");
		return pUser;
	}

	bool ChallengeRespond(const CNick& Nick, const CString& sChallenge) {
		// Validate before responding - don't blindly trust everyone
		bool bValid = false;
		bool bMatchedHost = false;
		CAutoOpUser* pUser = NULL;

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			pUser = it->second;

			// First verify that the person who challenged us matches a user's host
			if (pUser->HostMatches(Nick.GetHostMask())) {
				const vector<CChan*>& Chans = GetNetwork()->GetChans();
				bMatchedHost = true;

				// Also verify that they are opped in at least one of the user's chans
				for (size_t a = 0; a < Chans.size(); a++) {
					const CChan& Chan = *Chans[a];

					const CNick* pNick = Chan.FindNick(Nick.GetNick());

					if (pNick) {
						if (pNick->HasPerm(CChan::Op) && pUser->ChannelMatches(Chan.GetName())) {
							bValid = true;
							break;
						}
					}
				}

				if (bValid) {
					break;
				}
			}
		}

		if (!bValid) {
			if (bMatchedHost) {
				PutModule("[" + Nick.GetHostMask() + "] sent us a challenge but they are not opped in any defined channels.");
			} else {
				PutModule("[" + Nick.GetHostMask() + "] sent us a challenge but they do not match a defined user.");
			}

			return false;
		}

		if (sChallenge.length() != AUTOOP_CHALLENGE_LENGTH) {
			PutModule("WARNING! [" + Nick.GetHostMask() + "] sent an invalid challenge.");
			return false;
		}

		CString sResponse = pUser->GetUserKey() + "::" + sChallenge;
		PutIRC("NOTICE " + Nick.GetNick() + " :!ZNCAO RESPONSE " + sResponse.MD5());
		return false;
	}

	bool VerifyResponse(const CNick& Nick, const CString& sResponse) {
		MCString::iterator itQueue = m_msQueue.find(Nick.GetNick().AsLower());

		if (itQueue == m_msQueue.end()) {
			PutModule("[" + Nick.GetHostMask() + "] sent an unchallenged response.  This could be due to lag.");
			return false;
		}

		CString sChallenge = itQueue->second;
		m_msQueue.erase(itQueue);

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			if (it->second->HostMatches(Nick.GetHostMask())) {
				if (sResponse == CString(it->second->GetUserKey() + "::" + sChallenge).MD5()) {
					OpUser(Nick, *it->second);
					return true;
				} else {
					PutModule("WARNING! [" + Nick.GetHostMask() + "] sent a bad response.  Please verify that you have their correct password.");
					return false;
				}
			}
		}

		PutModule("WARNING! [" + Nick.GetHostMask() + "] sent a response but did not match any defined users.");
		return false;
	}

	void ProcessQueue() {
		bool bRemoved = true;

		// First remove any stale challenges

		while (bRemoved) {
			bRemoved = false;

			for (MCString::iterator it = m_msQueue.begin(); it != m_msQueue.end(); ++it) {
				if (!it->second.empty()) {
					m_msQueue.erase(it);
					bRemoved = true;
					break;
				}
			}
		}

		// Now issue challenges for the new users in the queue
		for (MCString::iterator it = m_msQueue.begin(); it != m_msQueue.end(); ++it) {
			it->second = CString::RandomString(AUTOOP_CHALLENGE_LENGTH);
			PutIRC("NOTICE " + it->first + " :!ZNCAO CHALLENGE " + it->second);
		}
	}

	void OpUser(const CNick& Nick, const CAutoOpUser& User) {
		const vector<CChan*>& Chans = GetNetwork()->GetChans();

		for (size_t a = 0; a < Chans.size(); a++) {
			const CChan& Chan = *Chans[a];

			if (Chan.HasPerm(CChan::Op) && User.ChannelMatches(Chan.GetName())) {
				const CNick* pNick = Chan.FindNick(Nick.GetNick());

				if (pNick && !pNick->HasPerm(CChan::Op)) {
					PutIRC("MODE " + Chan.GetName() + " +o " + Nick.GetNick());
				}
			}
		}
	}
private:
	map<CString, CAutoOpUser*> m_msUsers;
	MCString                   m_msQueue;
};

void CAutoOpTimer::RunJob() {
	m_pParent->ProcessQueue();
}

template<> void TModInfo<CAutoOpMod>(CModInfo& Info) {
	Info.SetWikiPage("autoop");
}

NETWORKMODULEDEFS(CAutoOpMod, "Auto op the good people")
