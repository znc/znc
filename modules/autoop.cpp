/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"

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
	virtual void RunJob();

	CAutoOpMod* m_pParent;
};

class CAutoOpUser {
public:
	CAutoOpUser() {}

	CAutoOpUser(const CString& sLine) {
		FromString(sLine);
	}

	CAutoOpUser(const CString& sUsername, const CString& sUserKey, const CString& sHostmask, const CString& sChannels) :
			m_sUsername(sUsername),
			m_sUserKey(sUserKey),
			m_sHostmask(sHostmask) {
		AddChans(sChannels);
	}

	virtual ~CAutoOpUser() {}

	const CString& GetUsername() const { return m_sUsername; }
	const CString& GetUserKey() const { return m_sUserKey; }
	const CString& GetHostmask() const { return m_sHostmask; }

	bool ChannelMatches(const CString& sChan) const {
		for (set<CString>::const_iterator it = m_ssChans.begin(); it != m_ssChans.end(); ++it) {
			if (sChan.AsLower().WildCmp(*it)) {
				return true;
			}
		}

		return false;
	}

	bool HostMatches(const CString& sHostmask) {
		return sHostmask.WildCmp(m_sHostmask);
	}

	CString GetChannels() const {
		CString sRet;

		for (set<CString>::const_iterator it = m_ssChans.begin(); it != m_ssChans.end(); ++it) {
			if (!sRet.empty()) {
				sRet += " ";
			}

			sRet += *it;
		}

		return sRet;
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
		CString sChans;

		for (set<CString>::const_iterator it = m_ssChans.begin(); it != m_ssChans.end(); ++it) {
			if (!sChans.empty()) {
				sChans += " ";
			}

			sChans += *it;
		}

		return m_sUsername + "\t" + m_sHostmask + "\t" + m_sUserKey + "\t" + sChans;
	}

	bool FromString(const CString& sLine) {
		m_sUsername = sLine.Token(0, false, "\t");
		m_sHostmask = sLine.Token(1, false, "\t");
		m_sUserKey = sLine.Token(2, false, "\t");
		sLine.Token(3, false, "\t").Split(" ", m_ssChans);

		return !m_sUserKey.empty();
	}
private:
protected:
	CString      m_sUsername;
	CString      m_sUserKey;
	CString      m_sHostmask;
	set<CString> m_ssChans;
};

class CAutoOpMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoOpMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
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

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		// If we have ops in this chan
		if (Channel.HasPerm(CChan::Op)) {
			for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
				// and the nick who joined is a valid user
				if (it->second->HostMatches(Nick.GetHostMask()) && it->second->ChannelMatches(Channel.GetName())) {
					if (it->second->GetUserKey().Equals("__NOKEY__")) {
						PutIRC("MODE " + Channel.GetName() + " +o " + Nick.GetNick());
					} else {
						// then insert this nick into the queue, the timer does the rest
						CString sNick = Nick.GetNick().AsLower();
						if (m_msQueue.find(sNick) == m_msQueue.end()) {
							m_msQueue[sNick] = "";
						}
					}

					break;
				}
			}
		}
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		MCString::iterator it = m_msQueue.find(Nick.GetNick().AsLower());

		if (it != m_msQueue.end()) {
			m_msQueue.erase(it);
		}
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		// Update the queue with nick changes
		MCString::iterator it = m_msQueue.find(OldNick.GetNick().AsLower());

		if (it != m_msQueue.end()) {
			m_msQueue[sNewNick.AsLower()] = it->second;
			m_msQueue.erase(it);
		}
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
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

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0).AsUpper();

		if (sCommand.Equals("HELP")) {
			PutModule("Commands are: ListUsers, AddChans, DelChans, AddUser, DelUser");
		} else if (sCommand.Equals("TIMERS")) {
			ListTimers();
		} else if (sCommand.Equals("ADDUSER") || sCommand.Equals("DELUSER")) {
			CString sUser = sLine.Token(1);
			CString sHost = sLine.Token(2);
			CString sKey = sLine.Token(3);

			if (sCommand.Equals("ADDUSER")) {
				if (sHost.empty()) {
					PutModule("Usage: " + sCommand + " <user> <hostmask> <key> [channels]");
				} else {
					CAutoOpUser* pUser = AddUser(sUser, sKey, sHost, sLine.Token(4, true));

					if (pUser) {
						SetNV(sUser, pUser->ToString());
					}
				}
			} else {
				DelUser(sUser);
				DelNV(sUser);
			}
		} else if (sCommand.Equals("LISTUSERS")) {
			if (m_msUsers.empty()) {
				PutModule("There are no users defined");
				return;
			}

			CTable Table;

			Table.AddColumn("User");
			Table.AddColumn("Hostmask");
			Table.AddColumn("Key");
			Table.AddColumn("Channels");

			for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
				Table.AddRow();
				Table.SetCell("User", it->second->GetUsername());
				Table.SetCell("Hostmask", it->second->GetHostmask());
				Table.SetCell("Key", it->second->GetUserKey());
				Table.SetCell("Channels", it->second->GetChannels());
			}

			PutModule(Table);
		} else if (sCommand.Equals("ADDCHANS") || sCommand.Equals("DELCHANS")) {
			CString sUser = sLine.Token(1);
			CString sChans = sLine.Token(2, true);

			if (sChans.empty()) {
				PutModule("Usage: " + sCommand + " <user> <channel> [channel] ...");
				return;
			}

			CAutoOpUser* pUser = FindUser(sUser);

			if (!pUser) {
				PutModule("No such user");
				return;
			}

			if (sCommand.Equals("ADDCHANS")) {
				pUser->AddChans(sChans);
				PutModule("Channel(s) added to user [" + pUser->GetUsername() + "]");
			} else {
				pUser->DelChans(sChans);
				PutModule("Channel(s) Removed from user [" + pUser->GetUsername() + "]");
			}

			SetNV(pUser->GetUsername(), pUser->ToString());
		} else {
			PutModule("Unknown command, try HELP");
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

	CAutoOpUser* AddUser(const CString& sUser, const CString& sKey, const CString& sHost, const CString& sChans) {
		if (m_msUsers.find(sUser) != m_msUsers.end()) {
			PutModule("That user already exists");
			return NULL;
		}

		CAutoOpUser* pUser = new CAutoOpUser(sUser, sKey, sHost, sChans);
		m_msUsers[sUser.AsLower()] = pUser;
		PutModule("User [" + sUser + "] added with hostmask [" + sHost + "]");
		return pUser;
	}

	bool ChallengeRespond(const CNick& Nick, const CString& sChallenge) {
		// Validate before responding - don't blindly trust everyone
		bool bValid = false;
		bool bMatchedHost = false;
		CAutoOpUser* pUser = NULL;

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			pUser = it->second;

			// First verify that the guy who challenged us matches a user's host
			if (pUser->HostMatches(Nick.GetHostMask())) {
				const vector<CChan*>& Chans = m_pUser->GetChans();
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
		const vector<CChan*>& Chans = m_pUser->GetChans();

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

MODULEDEFS(CAutoOpMod, "Auto op the good guys")
