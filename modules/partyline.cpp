/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/User.h>
#include <znc/znc.h>
#include <znc/IRCNetwork.h>

using std::set;
using std::vector;
using std::map;

// If you change these and it breaks, you get to keep the pieces
#define CHAN_PREFIX_1   "~"
#define CHAN_PREFIX_1C  '~'
#define CHAN_PREFIX     CHAN_PREFIX_1 "#"

#define NICK_PREFIX    CString("?")
#define NICK_PREFIX_C  '?'

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

	bool IsInChannel(const CString& s) { return m_ssNicks.find(s) != m_ssNicks.end(); }

protected:
	CString      m_sTopic;
	CString      m_sName;
	set<CString> m_ssNicks;
};

class CPartylineMod : public CModule {
public:
	void ListChannelsCommand(const CString& sLine) {
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
	}

	MODCONSTRUCTOR(CPartylineMod) {
		AddHelpCommand();
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CPartylineMod::ListChannelsCommand),
			"", "List all open channels");
	}

	virtual ~CPartylineMod() {
		// Kick all clients who are in partyline channels
		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			set<CString> ssNicks = (*it)->GetNicks();

			for (set<CString>::const_iterator it2 = ssNicks.begin(); it2 != ssNicks.end(); ++it2) {
				CUser* pUser = CZNC::Get().FindUser(*it2);
				vector<CClient*> vClients = pUser->GetAllClients();

				for (vector<CClient*>::const_iterator it3 = vClients.begin(); it3 != vClients.end(); ++it3) {
					CClient* pClient = *it3;
					pClient->PutClient( ":*" + GetModName() + "!znc@znc.in KICK " + (*it)->GetName() + " " + pClient->GetNick() + " :" + GetModName() + " unloaded");
				}
			}
		}

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
			for (vector<CIRCNetwork*>::const_iterator i = pUser->GetNetworks().begin(); i != pUser->GetNetworks().end(); ++i) {
				CIRCNetwork* pNetwork = *i;
				if (pNetwork->GetIRCSock()) {
					if (pNetwork->GetChanPrefixes().find(CHAN_PREFIX_1) == CString::npos) {
						pNetwork->PutUser(":" + GetIRCServer(pNetwork) + " 005 " + pNetwork->GetIRCNick().GetNick() + " CHANTYPES=" + pNetwork->GetChanPrefixes() + CHAN_PREFIX_1 " :are supported by this server.");
					}
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
				// Sorry, this was removed
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

	void SaveTopic(CPartylineChannel* pChannel) {
		if (!pChannel->GetTopic().empty())
			SetNV("topic:" + pChannel->GetName(), pChannel->GetTopic());
		else
			DelNV("topic:" + pChannel->GetName());
	}

	virtual EModRet OnDeleteUser(CUser& User) {
		// Loop through each chan
		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end();) {
			CPartylineChannel *pChan = *it;
			// RemoveUser() might delete channels, so make sure our
			// iterator doesn't break.
			it++;
			RemoveUser(&User, pChan, "KICK", "User deleted", true);
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
				m_spInjectedPrefixes.insert(m_pNetwork);
			}
		}

		return CONTINUE;
	}

	virtual void OnIRCDisconnected() {
		m_spInjectedPrefixes.erase(m_pNetwork);
	}

	virtual void OnClientLogin() {
		if (m_spInjectedPrefixes.find(m_pNetwork) == m_spInjectedPrefixes.end() && m_pNetwork && !m_pNetwork->GetChanPrefixes().empty()) {
			m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 005 " + m_pClient->GetNick() + " CHANTYPES=" + m_pNetwork->GetChanPrefixes() + CHAN_PREFIX_1 " :are supported by this server.");
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
				sHost = "znc.in";
			}
			PutChan(ssNicks, ":" + NICK_PREFIX + sNick + "!" + m_pUser->GetIdent() + "@" + sHost + " JOIN " + *a, false);
			pChannel->AddNick(sNick);
		}

		CString sNickMask = m_pClient->GetNickMask();

		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			const set<CString>& ssNicks = (*it)->GetNicks();

			if ((*it)->IsInChannel(m_pUser->GetUserName())) {

				m_pClient->PutClient(":" + sNickMask + " JOIN " + (*it)->GetName());

				if (!(*it)->GetTopic().empty()) {
					m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 332 " + m_pClient->GetNickMask() + " " + (*it)->GetName() + " :" + (*it)->GetTopic());
				}

				SendNickList(m_pUser, m_pNetwork, ssNicks, (*it)->GetName());
				PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " +" + CString(m_pUser->IsAdmin() ? "o" : "v") + " " + NICK_PREFIX + m_pUser->GetUserName(), false);
			}
		}
	}

	virtual void OnClientDisconnect() {
		if (!m_pUser->IsUserAttached() && !m_pUser->IsBeingDeleted()) {
			for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
				const set<CString>& ssNicks = (*it)->GetNicks();

				if (ssNicks.find(m_pUser->GetUserName()) != ssNicks.end()) {
					PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " -ov " + NICK_PREFIX + m_pUser->GetUserName() + " " + NICK_PREFIX + m_pUser->GetUserName(), false);
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
						PutChan(ssNicks, ":" + m_pClient->GetNickMask() + " TOPIC " + sChannel + " :" + sTopic);
						pChannel->SetTopic(sTopic);
						SaveTopic(pChannel);
					} else {
						m_pUser->PutUser(":irc.znc.in 482 " +  m_pClient->GetNick() + " " + sChannel + " :You're not channel operator");
					}
				} else {
					sTopic = pChannel->GetTopic();

					if (sTopic.empty()) {
						m_pUser->PutUser(":irc.znc.in 331 " + m_pClient->GetNick() + " " + sChannel + " :No topic is set.");
					} else {
						m_pUser->PutUser(":irc.znc.in 332 " + m_pClient->GetNick() + " " + sChannel + " :" + sTopic);
					}
				}
			} else {
				m_pUser->PutUser(":irc.znc.in 442 " + m_pClient->GetNick() + " " + sChannel + " :You're not on that channel");
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
			m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 401 " + m_pClient->GetNick() + " " + sChannel + " :No such channel");
			return HALT;
		}

		CPartylineChannel* pChannel = FindChannel(sChannel);

		PartUser(m_pUser, pChannel);

		return HALT;
	}

	void PartUser(CUser* pUser, CPartylineChannel* pChannel,
			const CString& sMessage = "") {
		RemoveUser(pUser, pChannel, "PART", sMessage);
	}

	void RemoveUser(CUser* pUser, CPartylineChannel* pChannel, const CString& sCommand,
			const CString& sMessage = "", bool bNickAsTarget = false) {
		if (!pChannel || !pChannel->IsInChannel(pUser->GetUserName())) {
			return;
		}

		vector<CClient*> vClients = pUser->GetAllClients();

		CString sCmd = " " + sCommand + " ";
		CString sMsg = sMessage;
		if (!sMsg.empty())
			sMsg = " :" + sMsg;

		pChannel->DelNick(pUser->GetUserName());

		const set<CString>& ssNicks = pChannel->GetNicks();
		CString sHost = pUser->GetBindHost();

		if (sHost.empty()) {
			sHost = "znc.in";
		}

		if (bNickAsTarget) {
			for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
				CClient* pClient = *it;

				pClient->PutClient(":" + pClient->GetNickMask() + sCmd + pChannel->GetName() + " " + pClient->GetNick() + sMsg);
			}

			PutChan(ssNicks, ":" + NICK_PREFIX + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost
					+ sCmd + pChannel->GetName() + " " + NICK_PREFIX + pUser->GetUserName() + sMsg,
					false, true, pUser);
		} else {
			for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
				CClient* pClient = *it;

				pClient->PutClient(":" + pClient->GetNickMask() + sCmd + pChannel->GetName() + sMsg);
			}

			PutChan(ssNicks, ":" + NICK_PREFIX + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost
					+ sCmd + pChannel->GetName() + sMsg, false, true, pUser);
		}

		if (ssNicks.empty()) {
			delete pChannel;
			m_ssChannels.erase(pChannel);
		}
	}

	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey) {
		if (sChannel.Left(1) != CHAN_PREFIX_1) {
			return CONTINUE;
		}

		if (sChannel.Left(2) != CHAN_PREFIX) {
			m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 403 " + m_pClient->GetNick() + " " + sChannel + " :Channels look like " CHAN_PREFIX "znc");
			return HALT;
		}

		sChannel = sChannel.Left(32);
		CPartylineChannel* pChannel = GetChannel(sChannel);

		JoinUser(m_pUser, pChannel);

		return HALT;
	}

	void JoinUser(CUser* pUser, CPartylineChannel* pChannel) {
		if (pChannel && !pChannel->IsInChannel(pUser->GetUserName())) {
			vector<CClient*> vClients = pUser->GetAllClients();

			const set<CString>& ssNicks = pChannel->GetNicks();
			const CString& sNick = pUser->GetUserName();
			pChannel->AddNick(sNick);

			CString sHost = pUser->GetBindHost();

			if (sHost.empty()) {
				sHost = "znc.in";
			}

			for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
				CClient* pClient = *it;
				pClient->PutClient(":" + pClient->GetNickMask() + " JOIN " + pChannel->GetName());
			}

			PutChan(ssNicks, ":" + NICK_PREFIX + sNick + "!" + pUser->GetIdent() + "@" + sHost + " JOIN " + pChannel->GetName(), false, true, pUser);

			if (!pChannel->GetTopic().empty()) {
				for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
					CClient* pClient = *it;
					pClient->PutClient(":" + GetIRCServer(pClient->GetNetwork()) + " 332 " + pClient->GetNickMask() + " " + pChannel->GetName() + " :" + pChannel->GetTopic());
				}
			}

			SendNickList(pUser, NULL, ssNicks, pChannel->GetName());

			/* Tell the other clients we have op or voice, the current user's clients already know from NAMES list */

			if (pUser->IsAdmin()) {
				PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + pChannel->GetName() + " +o " + NICK_PREFIX + pUser->GetUserName(), false, false, pUser);
			}

			PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + pChannel->GetName() + " +v " + NICK_PREFIX + pUser->GetUserName(), false, false, pUser);
		}
	}

	virtual EModRet HandleMessage(const CString& sCmd, const CString& sTarget, const CString& sMessage) {
		if (sTarget.empty()) {
			return CONTINUE;
		}

		char cPrefix = sTarget[0];

		if (cPrefix != CHAN_PREFIX_1C && cPrefix != NICK_PREFIX_C) {
			return CONTINUE;
		}

		CString sHost = m_pUser->GetBindHost();

		if (sHost.empty()) {
			sHost = "znc.in";
		}

		if (cPrefix == CHAN_PREFIX_1C) {
			if (FindChannel(sTarget) == NULL) {
				m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 401 " + m_pClient->GetNick() + " " + sTarget + " :No such channel");
				return HALT;
			}

			PutChan(sTarget, ":" + NICK_PREFIX + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + sTarget + " :" + sMessage, true, false);
		} else {
			CString sNick = sTarget.LeftChomp_n(1);
			CUser* pUser = CZNC::Get().FindUser(sNick);

			if (pUser) {
				vector<CClient*> vClients = pUser->GetAllClients();

				if (vClients.empty()) {
					m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 401 " + m_pClient->GetNick() + " " + sTarget + " :User is not attached: " + sNick + "");
					return HALT;
				}

				for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
					CClient* pClient = *it;

					pClient->PutClient(":" + NICK_PREFIX + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + pClient->GetNick() + " :" + sMessage);
				}
			} else {
				m_pClient->PutClient(":" + GetIRCServer(m_pNetwork) + " 401 " + m_pClient->GetNick() + " " + sTarget + " :No such znc user: " + sNick + "");
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

	const CString GetIRCServer(CIRCNetwork *pNetwork) {
		if (!pNetwork) {
			return "irc.znc.in";
		}

		const CString& sServer = pNetwork->GetIRCServer();
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
						it->second->PutAllUser(sLine, NULL, (bIncludeClient ? NULL : pClient));
					}
				} else {
					it->second->PutAllUser(sLine);
				}
			}
		}
	}

	void PutUserIRCNick(CUser* pUser, const CString& sPre, const CString& sPost) {
		const vector<CClient*>& vClients = pUser->GetAllClients();
		vector<CClient*>::const_iterator it;
		for (it = vClients.begin(); it != vClients.end(); ++it) {
			(*it)->PutClient(sPre + (*it)->GetNick() + sPost);
		}
	}

	void SendNickList(CUser* pUser, CIRCNetwork* pNetwork, const set<CString>& ssNicks, const CString& sChan) {
		CString sNickList;

		for (set<CString>::const_iterator it = ssNicks.begin(); it != ssNicks.end(); ++it) {
			CUser* pChanUser = CZNC::Get().FindUser(*it);

			if (pChanUser == pUser) {
				continue;
			}

			if (pChanUser && pChanUser->IsUserAttached()) {
				sNickList += (pChanUser->IsAdmin()) ? "@" : "+";
			}

			sNickList += NICK_PREFIX + (*it) + " ";

			if (sNickList.size() >= 500) {
				PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 353 ", " @ " + sChan + " :" + sNickList);
				sNickList.clear();
			}
		}

		if (sNickList.size()) {
			PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 353 ", " @ " + sChan + " :" + sNickList);
		}

		vector<CClient*> vClients = pUser->GetAllClients();
		for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
			CClient* pClient = *it;
			pClient->PutClient(":" + GetIRCServer(pNetwork) + " 353 " +  pClient->GetNick() + " @ " + sChan + " :" + ((pUser->IsAdmin()) ? "@" : "+") + pClient->GetNick());
		}

		PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 366 ", " " + sChan + " :End of /NAMES list.");
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

		if (!pChannel) {
			pChannel = new CPartylineChannel(sChannel.AsLower());
			m_ssChannels.insert(pChannel);
		}

		return pChannel;
	}

private:
	set<CPartylineChannel*> m_ssChannels;
	set<CIRCNetwork*>       m_spInjectedPrefixes;
	set<CString>            m_ssDefaultChans;
};

template<> void TModInfo<CPartylineMod>(CModInfo& Info) {
	Info.SetWikiPage("partyline");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("You may enter a list of channels the user joins, when entering the internal partyline.");
}

GLOBALMODULEDEFS(CPartylineMod, "Internal channels and queries for users connected to znc")
