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

#include <znc/User.h>
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
		if (m_ssChannels.empty()) {
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

		while (!m_ssChannels.empty()) {
			delete *m_ssChannels.begin();
			m_ssChannels.erase(m_ssChannels.begin());
		}
	}

	bool OnBoot() override {
		// The config is now read completely, so all Users are set up
		Load();

		return true;
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
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

	EModRet OnDeleteUser(CUser& User) override {
		// Loop through each chan
		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end();) {
			CPartylineChannel *pChan = *it;
			// RemoveUser() might delete channels, so make sure our
			// iterator doesn't break.
			++it;
			RemoveUser(&User, pChan, "KICK", "User deleted", true);
		}

		return CONTINUE;
	}

	EModRet OnRaw(CString& sLine) override {
		if (sLine.Token(1) == "005") {
			CString::size_type uPos = sLine.AsUpper().find("CHANTYPES=");
			if (uPos != CString::npos) {
				uPos = sLine.find(" ", uPos);

				if (uPos == CString::npos)
					sLine.append(CHAN_PREFIX_1);
				else
					sLine.insert(uPos, CHAN_PREFIX_1);
				m_spInjectedPrefixes.insert(GetNetwork());
			}
		}

		return CONTINUE;
	}

	void OnIRCDisconnected() override {
		m_spInjectedPrefixes.erase(GetNetwork());
	}

	void OnClientLogin() override {
		CUser* pUser = GetUser();
		CClient* pClient = GetClient();
		CIRCNetwork* pNetwork = GetNetwork();
		if (m_spInjectedPrefixes.find(pNetwork) == m_spInjectedPrefixes.end() && pNetwork && !pNetwork->GetChanPrefixes().empty()) {
			pClient->PutClient(":" + GetIRCServer(pNetwork) + " 005 " + pClient->GetNick() + " CHANTYPES=" + pNetwork->GetChanPrefixes() + CHAN_PREFIX_1 " :are supported by this server.");
		}

		// Make sure this user is in the default channels
		for (set<CString>::iterator a = m_ssDefaultChans.begin(); a != m_ssDefaultChans.end(); ++a) {
			CPartylineChannel* pChannel = GetChannel(*a);
			const CString& sNick = pUser->GetUserName();

			if (pChannel->IsInChannel(sNick))
				continue;

			CString sHost = pUser->GetBindHost();
			const set<CString>& ssNicks = pChannel->GetNicks();

			if (sHost.empty()) {
				sHost = "znc.in";
			}
			PutChan(ssNicks, ":" + NICK_PREFIX + sNick + "!" + pUser->GetIdent() + "@" + sHost + " JOIN " + *a, false);
			pChannel->AddNick(sNick);
		}

		CString sNickMask = pClient->GetNickMask();

		for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
			const set<CString>& ssNicks = (*it)->GetNicks();

			if ((*it)->IsInChannel(pUser->GetUserName())) {

				pClient->PutClient(":" + sNickMask + " JOIN " + (*it)->GetName());

				if (!(*it)->GetTopic().empty()) {
					pClient->PutClient(":" + GetIRCServer(pNetwork) + " 332 " + pClient->GetNickMask() + " " + (*it)->GetName() + " :" + (*it)->GetTopic());
				}

				SendNickList(pUser, pNetwork, ssNicks, (*it)->GetName());
				PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " +" + CString(pUser->IsAdmin() ? "o" : "v") + " " + NICK_PREFIX + pUser->GetUserName(), false);
			}
		}
	}

	void OnClientDisconnect() override {
		CUser* pUser = GetUser();
		if (!pUser->IsUserAttached() && !pUser->IsBeingDeleted()) {
			for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin(); it != m_ssChannels.end(); ++it) {
				const set<CString>& ssNicks = (*it)->GetNicks();

				if (ssNicks.find(pUser->GetUserName()) != ssNicks.end()) {
					PutChan(ssNicks, ":*" + GetModName() + "!znc@znc.in MODE " + (*it)->GetName() + " -ov " + NICK_PREFIX + pUser->GetUserName() + " " + NICK_PREFIX + pUser->GetUserName(), false);
				}
			}
		}
	}

	EModRet OnUserRaw(CString& sLine) override {
		if (sLine.StartsWith("WHO " CHAN_PREFIX_1)) {
			return HALT;
		} else if (sLine.StartsWith("MODE " CHAN_PREFIX_1)) {
			return HALT;
		} else if (sLine.StartsWith("TOPIC " CHAN_PREFIX)) {
			CString sChannel = sLine.Token(1);
			CString sTopic = sLine.Token(2, true);

			sTopic.TrimPrefix(":");

			CUser* pUser = GetUser();
			CClient* pClient = GetClient();
			CPartylineChannel* pChannel = FindChannel(sChannel);

			if (pChannel && pChannel->IsInChannel(pUser->GetUserName())) {
				const set<CString>& ssNicks = pChannel->GetNicks();
				if (!sTopic.empty()) {
					if (pUser->IsAdmin()) {
						PutChan(ssNicks, ":" + pClient->GetNickMask() + " TOPIC " + sChannel + " :" + sTopic);
						pChannel->SetTopic(sTopic);
						SaveTopic(pChannel);
					} else {
						pUser->PutUser(":irc.znc.in 482 " +  pClient->GetNick() + " " + sChannel + " :You're not channel operator");
					}
				} else {
					sTopic = pChannel->GetTopic();

					if (sTopic.empty()) {
						pUser->PutUser(":irc.znc.in 331 " + pClient->GetNick() + " " + sChannel + " :No topic is set.");
					} else {
						pUser->PutUser(":irc.znc.in 332 " + pClient->GetNick() + " " + sChannel + " :" + sTopic);
					}
				}
			} else {
				pUser->PutUser(":irc.znc.in 442 " + pClient->GetNick() + " " + sChannel + " :You're not on that channel");
			}
			return HALT;
		}

		return CONTINUE;
	}

	EModRet OnUserPart(CString& sChannel, CString& sMessage) override {
		if (sChannel.Left(1) != CHAN_PREFIX_1) {
			return CONTINUE;
		}

		if (sChannel.Left(2) != CHAN_PREFIX) {
			GetClient()->PutClient(":" + GetIRCServer(GetNetwork()) + " 401 " + GetClient()->GetNick() + " " + sChannel + " :No such channel");
			return HALT;
		}

		CPartylineChannel* pChannel = FindChannel(sChannel);

		PartUser(GetUser(), pChannel);

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

		if (!pUser->IsBeingDeleted() && m_ssDefaultChans.find(pChannel->GetName()) != m_ssDefaultChans.end()) {
			JoinUser(pUser, pChannel);
		}

		if (ssNicks.empty()) {
			delete pChannel;
			m_ssChannels.erase(pChannel);
		}
	}

	EModRet OnUserJoin(CString& sChannel, CString& sKey) override {
		if (sChannel.Left(1) != CHAN_PREFIX_1) {
			return CONTINUE;
		}

		if (sChannel.Left(2) != CHAN_PREFIX) {
			GetClient()->PutClient(":" + GetIRCServer(GetNetwork()) + " 403 " + GetClient()->GetNick() + " " + sChannel + " :Channels look like " CHAN_PREFIX "znc");
			return HALT;
		}

		sChannel = sChannel.Left(32);
		CPartylineChannel* pChannel = GetChannel(sChannel);

		JoinUser(GetUser(), pChannel);

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

	EModRet HandleMessage(const CString& sCmd, const CString& sTarget, const CString& sMessage) {
		if (sTarget.empty()) {
			return CONTINUE;
		}

		char cPrefix = sTarget[0];

		if (cPrefix != CHAN_PREFIX_1C && cPrefix != NICK_PREFIX_C) {
			return CONTINUE;
		}

		CUser* pUser = GetUser();
		CClient* pClient = GetClient();
		CIRCNetwork* pNetwork = GetNetwork();
		CString sHost = pUser->GetBindHost();

		if (sHost.empty()) {
			sHost = "znc.in";
		}

		if (cPrefix == CHAN_PREFIX_1C) {
			if (FindChannel(sTarget) == NULL) {
				pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " + pClient->GetNick() + " " + sTarget + " :No such channel");
				return HALT;
			}

			PutChan(sTarget, ":" + NICK_PREFIX + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + sTarget + " :" + sMessage, true, false);
		} else {
			CString sNick = sTarget.LeftChomp_n(1);
			CUser* pTargetUser = CZNC::Get().FindUser(sNick);

			if (pTargetUser) {
				vector<CClient*> vClients = pTargetUser->GetAllClients();

				if (vClients.empty()) {
					pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " + pClient->GetNick() + " " + sTarget + " :User is not attached: " + sNick + "");
					return HALT;
				}

				for (vector<CClient*>::const_iterator it = vClients.begin(); it != vClients.end(); ++it) {
					CClient* pTarget = *it;

					pTarget->PutClient(":" + NICK_PREFIX + pUser->GetUserName() + "!" + pUser->GetIdent() + "@" + sHost + " " + sCmd + " " + pTarget->GetNick() + " :" + sMessage);
				}
			} else {
				pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " + pClient->GetNick() + " " + sTarget + " :No such znc user: " + sNick + "");
			}
		}

		return HALT;
	}

	EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
		return HandleMessage("PRIVMSG", sTarget, sMessage);
	}

	EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
		return HandleMessage("NOTICE", sTarget, sMessage);
	}

	EModRet OnUserAction(CString& sTarget, CString& sMessage) override {
		return HandleMessage("PRIVMSG", sTarget, "\001ACTION " + sMessage + "\001");
	}

	EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override {
		return HandleMessage("PRIVMSG", sTarget, "\001" + sMessage + "\001");
	}

	EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override {
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
			pUser = GetUser();
		if (!pClient)
			pClient = GetClient();

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
