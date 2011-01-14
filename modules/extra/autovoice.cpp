/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"

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
	MODCONSTRUCTOR(CAutoVoiceMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		// Load the chans from the command line
		unsigned int a = 0;
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (VCString::const_iterator it = vsChans.begin(); it != vsChans.end(); ++it) {
			CString sName = "Args";
			sName += CString(a);
			AddUser(sName, "*", *it);
		}

		// Load the saved users
		for (MCString::iterator it = BeginNV(); it != EndNV(); it++) {
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
		for (map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			delete it->second;
		}

		m_msUsers.clear();
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		// If we have ops in this chan
		if (Channel.HasPerm(CChan::Op) || Channel.HasPerm(CChan::HalfOp)) {
			for (map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
				// and the nick who joined is a valid user
				if (it->second->HostMatches(Nick.GetHostMask()) && it->second->ChannelMatches(Channel.GetName())) {
					PutIRC("MODE " + Channel.GetName() + " +v " + Nick.GetNick());
					break;
				}
			}
		}
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0).AsUpper();

		if (sCommand.Equals("HELP")) {
			PutModule("Commands are: ListUsers, AddChans, DelChans, AddUser, DelUser");
		} else if (sCommand.Equals("ADDUSER") || sCommand.Equals("DELUSER")) {
			CString sUser = sLine.Token(1);
			CString sHost = sLine.Token(2);

			if (sCommand.Equals("ADDUSER")) {
				if (sHost.empty()) {
					PutModule("Usage: " + sCommand + " <user> <hostmask> [channels]");
				} else {
					CAutoVoiceUser* pUser = AddUser(sUser, sHost, sLine.Token(3, true));

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
			Table.AddColumn("Channels");

			for (map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
				Table.AddRow();
				Table.SetCell("User", it->second->GetUsername());
				Table.SetCell("Hostmask", it->second->GetHostmask());
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

			CAutoVoiceUser* pUser = FindUser(sUser);

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

	CAutoVoiceUser* FindUser(const CString& sUser) {
		map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.find(sUser.AsLower());

		return (it != m_msUsers.end()) ? it->second : NULL;
	}

	CAutoVoiceUser* FindUserByHost(const CString& sHostmask, const CString& sChannel = "") {
		for (map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			CAutoVoiceUser* pUser = it->second;

			if (pUser->HostMatches(sHostmask) && (sChannel.empty() || pUser->ChannelMatches(sChannel))) {
				return pUser;
			}
		}

		return NULL;
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
			return NULL;
		}

		CAutoVoiceUser* pUser = new CAutoVoiceUser(sUser, sHost, sChans);
		m_msUsers[sUser.AsLower()] = pUser;
		PutModule("User [" + sUser + "] added with hostmask [" + sHost + "]");
		return pUser;
	}

private:
	map<CString, CAutoVoiceUser*> m_msUsers;
};

MODULEDEFS(CAutoVoiceMod, "Auto voice the good guys")
