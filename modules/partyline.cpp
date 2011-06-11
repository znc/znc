/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "znc.h"

// If you change these and it breaks, you get to keep the pieces
#define CHAN_PREFIX_1   "~"
#define CHAN_PREFIX_1C  '~'
#define CHAN_PREFIX     CHAN_PREFIX_1 "#"

class CPartylineChannel {
public:
	CPartylineChannel(const CString& sName) { m_sName = sName.AsLower(); }
	~CPartylineChannel() {}

	const CString& GetTopic() const { return m_sTopic; }
	const CString& GetName() const { return m_sName; }
	const set<CString>& GetNicks() const { return m_ssNicks; }

	void SetTopic(const CString& s) { m_sTopic = s; }

	void AddNick(const CString& s) { m_ssNicks.insert(s); }
	void DelNick(const CString& s) { m_ssNicks.erase(s); }

	void AddFixedNick(const CString& s) { m_ssFixedNicks.insert(s); }
	void DelFixedNick(const CString& s) { m_ssFixedNicks.erase(s); }

	bool IsInChannel(const CString& s) { return m_ssNicks.find(s) != m_ssNicks.end(); }
	bool IsFixedChan(const CString& s) { return m_ssFixedNicks.find(s) != m_ssFixedNicks.end(); }

protected:
	CString      m_sTopic;
	CString      m_sName;
	set<CString> m_ssNicks;
	set<CString> m_ssFixedNicks;
};

class CPartylineMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CPartylineMod) {}

	virtual ~CPartylineMod() {
		while (m_ssChannels.size()) {
			delete *m_ssChannels.begin();
			m_ssChannels.erase(m_ssChannels.begin());
		}
	}

	virtual bool OnBoot() {
		// The config is now read completely, so all Users are set up
		Load();

		return true;
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			CUser* pUser = it->second;
			if (pUser->GetIRCSock()) {
				if (pUser->GetChanPrefixes().find(CHAN_PREFIX_1) == CString::npos) {
					pUser->PutUser(":" + GetIRCServer(pUser) + " 005 " + pUser->GetIRCNick().GetNick() + " CHANTYPES=" + pUser->GetChanPrefixes() + CHAN_PREFIX_1 " :are supported by this server.");
				}
			}
		}

		VCString vsChans;
		VCString::const_iterator it;
		sArgs.Split(" ", vsChans, false);

		for (it = vsChans.begin(); it != vsChans.end(); ++it) {
			if (it->Left(2) == CHAN_PREFIX) {
				m_ssDefaultChans.insert(it->Left(32));
			}
		}

		Load();

		return true;
	}

	void Load() {
		CString sAction, sKey;
		CPartylineChannel* pChannel;
		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			if (it->first.find(":") != CString::npos) {
				sAction = it->first.Token(0, false, ":");
				sKey = it->first.Token(1, true, ":");
			} else {
				// backwards compatibility for older NV data
				sAction = "fixedchan";
				sKey = it->first;
			}

			if (sAction == "fixedchan") {
				CUser* pUser = CZNC::Get().FindUser(sKey);
				if (!pUser) {
					// TODO: give some useful message?
					continue;
				}

				VCString vsChannels;
				it->second.Split(",", vsChannels, false);
				for (VCString::iterator i = vsChannels.begin(); i != vsChannels.end(); ++i) {
					if (i->Trim_n().empty())
						continue;
					pChannel = GetChannel(*i);
					JoinUser(pUser, pChannel);
					pChannel->AddFixedNick(sKey);
				}
			}

			if (sAction == "topic") {
				pChannel = FindChannel(sKey);
				if (pChannel && !(it->second).empty()) {
					PutChan(pChannel->GetNicks(), ":irc.znc.in TOPIC " + pChannel->GetName() + " :" + it->second);
					pChannel->SetTopic(it->second);
				}
			}
		}

		return;
	}

	void SaveFixedChans(CUser* pUser) {
		CString sChans;
		const CString &sUser = pUser->GetUserName();

		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin();
				it != m_ssChannels.end(); ++it) {
			if ((*it)->IsFixedChan(sUser)) {
				sChans += "," + (*it)->GetName();
			}
		}

		if (!sChans.empty())
			SetNV("fixedchan:" + sUser, sChans.substr(1)); // Strip away the first ,
		else
			DelNV("fixedchan:" + sUser);
	}

	void SaveTopic(CPartylineChannel* pChannel) {
		if (!pChannel->GetTopic().empty())
			SetNV("topic:" + pChannel->GetName(), pChannel->GetTopic());
		else
			DelNV("topic:" + pChannel->GetName());
	}

	virtual EModRet OnDeleteUser(CUser& User) {
		// Loop through each chan
		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			RemoveUser(&User, *it, "KICK", true, "User deleted", true);
		}

		return CONTINUE;
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (sLine.Token(1) == "005") {
			CString::size_type uPos = sLine.AsUpper().find("CHANTYPES=");
			if (uPos != CString::npos) {
				uPos = sLine.find(" ", uPos);

				if (uPos == CString::npos)
					sLine.append(CHAN_PREFIX_1);
				else
					sLine.insert(uPos, CHAN_PREFIX_1);
				m_spInjectedPrefixes.insert(m_pUser);
			}
		}

		return CONTINUE;
	}

	virtual void OnIRCDisconnected() {
		m_spInjectedPrefixes.erase(m_pUser);
	}

	virtual void OnClientLogin() {
		if (m_spInjectedPrefixes.find(m_pUser) == m_spInjectedPrefixes.end() && !m_pUser->GetChanPrefixes().empty()) {
			m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 005 " + m_pUser->GetIRCNick().GetNick() + " CHANTYPES=" + m_pUser->GetChanPrefixes() + CHAN_PREFIX_1 " :are supported by this server.");
		}

		// Make sure this user is in the default channels
		for (set<CString>::iterator a = m_ssDefaultChans.begin(); a != m_ssDefaultChans.end(); ++a) {
			CPartylineChannel* pChannel = GetChannel(*a);
			const CString& sNick = m_pUser->GetUserName();

			if (pChannel->IsInChannel(sNick))
				continue;

			CString sHost = m_pUser->GetBindHost();
			const set<CString>& ssNicks = pChannel->GetNicks();

			if (sHost.empty()) {
				sHost = m_pUser->GetIRCNick().GetHost();
			}
			if (sHost.empty()) {
				sHost = "znc.in";
			}
			PutChan(ssNicks, ":?" + sNick + "!" + m_pUser->GetIdent() + "@" + sHost + " JOIN " + *a, false);
			pChannel->AddNick(sNick);
		}

		CString sNickMask = m_pClient->GetNickMask();

		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			const set<CString>& ssNicks = (*it)->GetNicks();

			if ((*it)->IsInChannel(m_pUser->GetUserName())) {

				m_pClient->PutClient(":" + sNickMask + " JOIN " + (*it)->GetName());

				if (!(*it)->GetTopic().empty()) {
					m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 332 " + m_pUser->GetIRCNick().GetNickMask() + " " + (*it)->GetName() + " :" + (*it)->GetTopic());
				}

				SendNickList(m_pUser, ssNicks, (*it)->GetName());
				PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " +" + CString(m_pUser->IsAdmin() ? "o" : "v") + " ?" + m_pUser->GetUserName(), true);
			}
		}
	}

	virtual void OnClientDisconnect() {
		if (!m_pUser->IsUserAttached() && !m_pUser->IsBeingDeleted()) {
			for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
				const set<CString>& ssNicks = (*it)->GetNicks();

				if (ssNicks.find(m_pUser->GetUserName()) != ssNicks.end()) {
					PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " -ov ?" + m_pUser->GetUserName() + " ?" + m_pUser->GetUserName(), true);
				}
			}
		}
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		if (sLine.Equals("WHO " CHAN_PREFIX_1, false, 5)) {
			return HALT;
		} else if (sLine.Equals("MODE " CHAN_PREFIX_1, false, 6)) {
			return HALT;
		} else if (sLine.Equals("TOPIC " CHAN_PREFIX, false, 8)) {
			CString sChannel = sLine.Token(1);
			CString sTopic = sLine.Token(2, true);

			sTopic.TrimPrefix(":");

			CPartylineChannel* pChannel = FindChannel(sChannel);

			if (pChannel && pChannel->IsInChannel(m_pUser->GetUserName())) {
				const set<CString>& ssNicks = pChannel->GetNicks();
				if (!sTopic.empty()) {
					if (m_pUser->IsAdmin()) {
						PutChan(ssNicks, ":" + m_pUser->GetIRCNick().GetNickMask() + " TOPIC " + sChannel + " :" + sTopic);
						pChannel->SetTopic(sTopic);
						SaveTopic(pChannel);
					} else {
						m_pUser->PutUser(":irc.znc.in 482 " +  m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :You're not channel operator");
					}
				} else {
					sTopic = pChannel->GetTopic();

					if (sTopic.empty()) {
						m_pUser->PutUser(":irc.znc.in 331 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :No topic is set.");
					} else {
						m_pUser->PutUser(":irc.znc.in 332 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :" + sTopic);
					}
				}
			} else {
				m_pUser->PutUser(":irc.znc.in 442 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :You're not on that channel");
			}
			return HALT;
		}

		return CONTINUE;
	}

	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) {
		if (sChannel.Left(1) != CHAN_PREFIX_1) {
			return CONTINUE;
		}

		if (sChannel.Left(2) != CHAN_PREFIX) {
			m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :No such channel");
			return HALT;
		}

		CPartylineChannel* pChannel = FindChannel(sChannel);

		PartUser(m_pUser, pChannel);

		return HALT;
	}

	void PartUser(CUser* pUser, CPartylineChannel* pChannel, bool bForce = false,
			const CString& sMessage = "") {
		RemoveUser(pUser, pChannel, "PART", bForce, sMessage);
	}

	void RemoveUser(CUser* pUser, CPartylineChannel* pChannel, const CString& sCommand,
			bool bForce = false, const CString& sMessage = "", bool bNickAsTarget = false) {
		if (!pChannel || !pChannel->IsInChannel(pUser->GetUserName())) {
			return;
		}

		CString sCmd = " " + sCommand + " ";
		CString sMsg = sMessage;
		if (!sMsg.empty())
			sMsg = " :" + sMsg;

		if (!pChannel->IsFixedChan(pUser->GetUserName()) || bForce) {
			pChannel->DelNick(pUser->GetUserName());
			pChannel->DelFixedNick(pUser->GetUserName());

			const set<CString>& ssNicks = pChannel->GetNicks();
			CString sHost = pUser->GetBindHost();

			if (sHost.empty()) {
				sHost = pUser->GetIRCNick().GetHost();
			}

			if (bNickAsTarget) {
				pUser->PutUser(":" + pUser->GetIRCNick().GetNickMask() + sCmd
						+ pChannel->GetName() + " " + pUser->GetIRCNick().GetNick() + sMsg);
				PutChan(ssNicks, ":?" + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost
						+ sCmd + pChannel->GetName() + " ?" + pUser->GetUserName() + sMsg,
						false, true, pUser);
			} else {
				pUser->PutUser(":" + pUser->GetIRCNick().GetNickMask() + sCmd
						+ pChannel->GetName() + sMsg);
				PutChan(ssNicks, ":?" + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost
						+ sCmd + pChannel->GetName() + sMsg, false, true, pUser);
			}

			if (ssNicks.empty()) {
				delete pChannel;
				m_ssChannels.erase(pChannel);
			}
		} else {
			// some clients dont wait for the server to send an answer to a part, so we need to make them join again
			pUser->PutUser(":" + pUser->GetIRCNick().GetNickMask() + " JOIN " + pChannel->GetName());
			if (!pChannel->GetTopic().empty()) {
				pUser->PutUser(":" + GetIRCServer(pUser) + " 332 " + pUser->GetIRCNick().GetNickMask() + " " + pChannel->GetName() + " :" + pChannel->GetTopic());
			}
			const set<CString>& ssNicks = pChannel->GetNicks();
			SendNickList(pUser, ssNicks, pChannel->GetName());
		}
	}

	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey) {
		if (sChannel.Left(1) != CHAN_PREFIX_1) {
			return CONTINUE;
		}

		if (sChannel.Left(2) != CHAN_PREFIX) {
			m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :Channels look like " CHAN_PREFIX "znc");
			return HALT;
		}

		sChannel = sChannel.Left(32);
		CPartylineChannel* pChannel = GetChannel(sChannel);

		JoinUser(m_pUser, pChannel);

		return HALT;
	}

	void JoinUser(CUser* pUser, CPartylineChannel* pChannel) {
		if (pChannel && !pChannel->IsInChannel(pUser->GetUserName())) {
			const set<CString>& ssNicks = pChannel->GetNicks();
			const CString& sNick = pUser->GetUserName();
			pChannel->AddNick(sNick);

			CString sHost = pUser->GetBindHost();

			if (sHost.empty()) {
				sHost = pUser->GetIRCNick().GetHost();
			}

			pUser->PutUser(":" + pUser->GetIRCNick().GetNickMask() + " JOIN " + pChannel->GetName());
			PutChan(ssNicks, ":?" + sNick + "!" + pUser->GetIdent() + "@" + sHost + " JOIN " + pChannel->GetName(), false, true, pUser);

			if (!pChannel->GetTopic().empty()) {
				pUser->PutUser(":" + GetIRCServer(pUser) + " 332 " + pUser->GetIRCNick().GetNickMask() + " " + pChannel->GetName() + " :" + pChannel->GetTopic());
			}
			SendNickList(pUser, ssNicks, pChannel->GetName());

			if (pUser->IsAdmin()) {
				PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + pChannel->GetName() + " +o ?" + pUser->GetUserName(), false, true, pUser);
			}
		}
	}

	virtual EModRet HandleMessage(const CString& sCmd, const CString& sTarget, const CString& sMessage) {
		if (sTarget.empty()) {
			return CONTINUE;
		}

		char cPrefix = sTarget[0];

		if (cPrefix != CHAN_PREFIX_1C && cPrefix != '?') {
			return CONTINUE;
		}

		CString sHost = m_pUser->GetBindHost();

		if (sHost.empty()) {
			sHost = m_pUser->GetIRCNick().GetHost();
		}

		if (cPrefix == CHAN_PREFIX_1C) {
			if (FindChannel(sTarget) == NULL) {
				m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sTarget + " :No such channel");
				return HALT;
			}

			PutChan(sTarget, ":?" + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + sTarget + " :" + sMessage, true, false);
		} else {
			CString sNick = sTarget.LeftChomp_n(1);
			CUser* pUser = CZNC::Get().FindUser(sNick);

			if (pUser) {
				pUser->PutUser(":?" + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + pUser->GetIRCNick().GetNick() + " :" + sMessage);
			} else {
				m_pClient->PutClient(":" + GetIRCServer(m_pUser) + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sTarget + " :No such znc user: " + sNick + "");
			}
		}

		return HALT;
	}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		return HandleMessage("PRIVMSG", sTarget, sMessage);
	}

	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage) {
		return HandleMessage("NOTICE", sTarget, sMessage);
	}

	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) {
		return HandleMessage("PRIVMSG", sTarget, "\001ACTION " + sMessage + "\001");
	}

	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage) {
		return HandleMessage("PRIVMSG", sTarget, "\001" + sMessage + "\001");
	}

	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) {
		return HandleMessage("NOTICE", sTarget, "\001" + sMessage + "\001");
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);

		if (sCommand.Equals("HELP")) {
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Arguments");
			Table.AddColumn("Description");

			Table.AddRow();
			Table.SetCell("Command", "Help");
			Table.SetCell("Arguments", "");
			Table.SetCell("Description", "List all partyline commands");

			Table.AddRow();
			Table.SetCell("Command", "List");
			Table.SetCell("Arguments", "");
			Table.SetCell("Description", "List all open channels");

			Table.AddRow();
			Table.SetCell("Command", "AddFixChan");
			Table.SetCell("Arguments", "<user> <channel>");
			Table.SetCell("Description", "Force a user into a channel which he cant part");

			Table.AddRow();
			Table.SetCell("Command", "DelFixChan");
			Table.SetCell("Arguments", "<user> <channel>");
			Table.SetCell("Description", "Remove a user from such a channel");

			Table.AddRow();
			Table.SetCell("Command", "ListFixChans");
			Table.SetCell("Arguments", "<user>");
			Table.SetCell("Description", "Show which channels a user can not part");

			Table.AddRow();
			Table.SetCell("Command", "ListFixUsers");
			Table.SetCell("Arguments", "<channel>");
			Table.SetCell("Description", "Show which users can not part this channel");

			PutModule(Table);
		} else if (sCommand.Equals("LIST")) {
			if (!m_ssChannels.size()) {
				PutModule("There are no open channels.");
				return;
			}

			CTable Table;

			Table.AddColumn("Channel");
			Table.AddColumn("Users");

			for (set<CPartylineChannel*>::const_iterator a = m_ssChannels.begin(); a != m_ssChannels.end(); ++a) {
				Table.AddRow();

				Table.SetCell("Channel", (*a)->GetName());
				Table.SetCell("Users", CString((*a)->GetNicks().size()));
			}

			PutModule(Table);
		} else if (sCommand.Equals("ADDFIXCHAN")) {
			if (!m_pUser->IsAdmin()) {
				PutModule("Access denied");
				return;
			}
			CString sUser = sLine.Token(1);
			CString sChan = sLine.Token(2).Left(32);
			CUser* pUser = CZNC::Get().FindUser(sUser);
			CPartylineChannel* pChan;

			if (sChan.Left(2) != CHAN_PREFIX) {
				PutModule("Invalid channel name");
				return;
			}

			if (pUser == NULL) {
				PutModule("Unknown User '" + sUser + "'");
				return;
			}

			pChan = GetChannel(sChan);
			JoinUser(pUser, pChan);
			pChan->AddFixedNick(sUser);

			SaveFixedChans(pUser);

			PutModule("Fixed " + sUser + " to channel " + sChan);
		} else if (sCommand.Equals("DELFIXCHAN")) {
			if (!m_pUser->IsAdmin()) {
				PutModule("Access denied");
				return;
			}
			CString sUser = sLine.Token(1);
			CString sChan = sLine.Token(2).Left(32);
			CUser* pUser = CZNC::Get().FindUser(sUser);
			CPartylineChannel* pChan = FindChannel(sChan);

			if (pUser == NULL) {
				PutModule("Unknown User '" + sUser + "'");
				return;
			}

			if (!pChan || !pChan->IsFixedChan(sUser)) {
				PutModule(sUser + " is not in " + sChan + " or isnt fixed to it");
				return;
			}

			PartUser(pUser, pChan, true);

			SaveFixedChans(pUser);

			PutModule("Removed " + sUser + " from " + sChan);
		} else if (sCommand.Equals("LISTFIXCHANS")) {
			if (!m_pUser->IsAdmin()) {
				PutModule("Access denied");
				return;
			}
			CString sUser = sLine.Token(1);
			CUser* pUser = CZNC::Get().FindUser(sUser);
			if (!pUser) {
				PutModule("User not found!");
				return;
			}

			for (set<CPartylineChannel*>::const_iterator a = m_ssChannels.begin(); a != m_ssChannels.end(); ++a) {
				if ((*a)->IsFixedChan(sUser)) {
					PutModule((*a)->GetName());
				}
			}
			PutModule("--- End of list");
		} else if (sCommand.Equals("LISTFIXUSERS")) {
			if (!m_pUser->IsAdmin()) {
				PutModule("Access denied");
				return;
			}
			CString sChan = sLine.Token(1).Left(32);
			CPartylineChannel* pChan = FindChannel(sChan);

			if (!pChan) {
				PutModule("Channel does not exist!");
				return;
			}
			const set<CString>& sNicks = pChan->GetNicks();
			for (set<CString>::const_iterator it = sNicks.begin(); it != sNicks.end(); ++it) {
				if (pChan->IsFixedChan(*it)) {
					PutModule(*it);
				}
			}
			PutModule("--- End of list");
	} else {
			PutModule("Unknown command, try 'HELP'");
		}
	}

	const CString GetIRCServer(CUser *pUser) {
		const CString& sServer = pUser->GetIRCServer();

		if (!sServer.empty())
			return sServer;
		return "irc.znc.in";
	}

	bool PutChan(const CString& sChan, const CString& sLine, bool bIncludeCurUser = true, bool bIncludeClient = true, CUser* pUser = NULL, CClient* pClient = NULL) {
		CPartylineChannel* pChannel = FindChannel(sChan);

		if (pChannel != NULL) {
			PutChan(pChannel->GetNicks(), sLine, bIncludeCurUser, bIncludeClient, pUser, pClient);
			return true;
		}

		return false;
	}

	void PutChan(const set<CString>& ssNicks, const CString& sLine, bool bIncludeCurUser = true, bool bIncludeClient = true, CUser* pUser = NULL, CClient* pClient = NULL) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

		if (!pUser)
			pUser = m_pUser;
		if (!pClient)
			pClient = m_pClient;

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			if (ssNicks.find(it->first) != ssNicks.end()) {
				if (it->second == pUser) {
					if (bIncludeCurUser) {
						it->second->PutUser(sLine, NULL, (bIncludeClient ? NULL : pClient));
					}
				} else {
					it->second->PutUser(sLine);
				}
			}
		}
	}

	void PutUserIRCNick(CUser *pUser, const CString& sPre, const CString& sPost) {
		const CString& sIRCNick = pUser->GetIRCNick().GetNick();
		if (!sIRCNick.empty()) {
			pUser->PutUser(sPre + sIRCNick + sPost);
			return;
		}

		const vector<CClient*>& vClients = pUser->GetClients();
		vector<CClient*>::const_iterator it;
		for (it = vClients.begin(); it != vClients.end(); ++it) {
			(*it)->PutClient(sPre + (*it)->GetNick() + sPost);
		}
	}

	void SendNickList(CUser* pUser, const set<CString>& ssNicks, const CString& sChan) {
		CString sNickList;

		for (set<CString>::const_iterator it = ssNicks.begin(); it != ssNicks.end(); ++it) {
			CUser* pChanUser = CZNC::Get().FindUser(*it);

			if (pChanUser && pChanUser->IsUserAttached()) {
				sNickList += (pChanUser->IsAdmin()) ? "@" : "+";
			}

			sNickList += "?" + (*it) + " ";

			if (sNickList.size() >= 500) {
				PutUserIRCNick(pUser, ":" + GetIRCServer(pUser) + " 353 ", " @ " + sChan + " :" + sNickList);
				sNickList.clear();
			}
		}

		if (sNickList.size()) {
			PutUserIRCNick(pUser, ":" + GetIRCServer(pUser) + " 353 ", " @ " + sChan + " :" + sNickList);
		}

		PutUserIRCNick(pUser, ":" + GetIRCServer(pUser) + " 366 ", " " + sChan + " :End of /NAMES list.");
	}

	CPartylineChannel* FindChannel(const CString& sChan) {
		CString sChannel = sChan.AsLower();

		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			if ((*it)->GetName().AsLower() == sChannel)
				return *it;
		}

		return NULL;
	}

	CPartylineChannel* GetChannel(const CString& sChannel) {
		CPartylineChannel* pChannel = FindChannel(sChannel);

		if (pChannel == NULL) {
			pChannel = new CPartylineChannel(sChannel.AsLower());
			m_ssChannels.insert(pChannel);
		}

		return pChannel;
	}

private:
	set<CPartylineChannel*> m_ssChannels;
	set<CUser*>             m_spInjectedPrefixes;
	set<CString>            m_ssDefaultChans;
};

template<> void TModInfo<CPartylineMod>(CModInfo& Info) {
	Info.SetWikiPage("partyline");
}

GLOBALMODULEDEFS(CPartylineMod, "Internal channels and queries for users connected to znc")
