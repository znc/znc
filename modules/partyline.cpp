#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"
#include "HTTPSock.h"
#include "Server.h"

class CPartylineMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CPartylineMod) {}

	virtual ~CPartylineMod() {}

	virtual bool OnBoot() {
		return true;
	}

	virtual bool OnLoad(const CString& sArgs) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
			CUser* pUser = it->second;
			if (pUser->GetIRCSock()) {
				if (pUser->GetChanPrefixes().find("~") == CString::npos) {
					pUser->PutUser(":" + pUser->GetIRCServer() + " 005 " + pUser->GetIRCNick().GetNick() + " CHANTYPES=" + pUser->GetChanPrefixes() + "~ :are supported by this server.");
				}
			}
		}

		return true;
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (sLine.Token(1) == "005") {
			CString::size_type uPos = sLine.AsUpper().find("CHANTYPES=");
			if (uPos != CString::npos) {
				uPos = sLine.find(" ", uPos);

				sLine.insert(uPos, "~");
				m_spInjectedPrefixes.insert(m_pUser);
			}
		}

		return CONTINUE;
	}

	virtual void OnIRCDisconnected() {
		m_spInjectedPrefixes.erase(m_pUser);
	}

	virtual void OnUserAttached() {
		if (m_spInjectedPrefixes.find(m_pUser) == m_spInjectedPrefixes.end()) {
			m_pClient->PutClient(":" + m_pUser->GetIRCServer() + " 005 " + m_pUser->GetIRCNick().GetNick() + " CHANTYPES=" + m_pUser->GetChanPrefixes() + "~ :are supported by this server.");
		}

		for (map<CString, set<CString> >::iterator it = m_msChans.begin(); it != m_msChans.end(); it++) {
			set<CString>& ssNicks = it->second;

			if (ssNicks.find(m_pUser->GetUserName()) != ssNicks.end()) {
				m_pClient->PutClient(":" + m_pUser->GetIRCNick().GetNickMask() + " JOIN " + it->first);
				SendNickList(ssNicks, it->first);
				PutChan(ssNicks, ":*" + GetModName() + "!znc@rottenboy.com MODE " + it->first + " +" + CString(m_pUser->IsAdmin() ? "o" : "v") + " ?" + m_pUser->GetUserName(), true);
			}
		}
	}

	virtual void OnUserDetached() {
		if (!m_pUser->IsUserAttached()) {
			for (map<CString, set<CString> >::iterator it = m_msChans.begin(); it != m_msChans.end(); it++) {
				set<CString>& ssNicks = it->second;

				if (ssNicks.find(m_pUser->GetUserName()) != ssNicks.end()) {
					PutChan(ssNicks, ":*" + GetModName() + "!znc@rottenboy.com MODE " + it->first + " -ov ?" + m_pUser->GetUserName() + " ?" + m_pUser->GetUserName(), true);
				}
			}
		}
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		if (sLine.Left(5).CaseCmp("WHO ~") == 0) {
			return HALT;
		} else if (sLine.Left(6).CaseCmp("MODE ~") == 0) {
			return HALT;
		}

		return CONTINUE;
	}

	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) {
		if (sChannel.Left(1) != "~") {
			return CONTINUE;
		}

		if (sChannel.Left(2) != "~#") {
			m_pClient->PutClient(":" + m_pUser->GetIRCServer() + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :No such channel");
			return HALT;
		}

		set<CString>& ssNicks = m_msChans[sChannel.AsLower()];
		const CString& sNick = m_pUser->GetUserName();

		if (ssNicks.find(sNick) != ssNicks.end()) {
			ssNicks.erase(sNick);
			CString sHost = m_pUser->GetVHost();

			if (sHost.empty()) {
				sHost = m_pUser->GetIRCNick().GetHost();
			}

			m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " PART " + sChannel);
			PutChan(ssNicks, ":?" + sNick + "!" + m_pUser->GetIdent() + "@" + sHost + " PART " + sChannel, false);

			if (ssNicks.empty()) {
				m_msChans.erase(sChannel.AsLower());
			}
		}

		return HALT;
	}

	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey) {
		if (sChannel.Left(1) != "~") {
			return CONTINUE;
		}

		if (sChannel.Left(2) != "~#") {
			m_pClient->PutClient(":" + m_pUser->GetIRCServer() + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sChannel + " :Channels look like ~#znc");
			return HALT;
		}

		sChannel = sChannel.Left(32);
		set<CString>& ssNicks = m_msChans[sChannel.AsLower()];
		const CString& sNick = m_pUser->GetUserName();

		if (ssNicks.find(sNick) == ssNicks.end()) {
			ssNicks.insert(sNick);

			CString sHost = m_pUser->GetVHost();

			if (sHost.empty()) {
				sHost = m_pUser->GetIRCNick().GetHost();
			}

			m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " JOIN " + sChannel);
			PutChan(ssNicks, ":?" + sNick + "!" + m_pUser->GetIdent() + "@" + sHost + " JOIN " + sChannel, false);
			SendNickList(ssNicks, sChannel);

			if (m_pUser->IsAdmin()) {
				PutChan(ssNicks, ":*" + GetModName() + "!znc@rottenboy.com MODE " + sChannel + " +o ?" + sNick, false);
			}
		}

		return HALT;
	}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		if (sTarget.empty()) {
			return CONTINUE;
		}

		char cPrefix = sTarget[0];

		if (cPrefix != '~' && cPrefix != '?') {
			return CONTINUE;
		}

		CString sHost = m_pUser->GetVHost();

		if (sHost.empty()) {
			sHost = m_pUser->GetIRCNick().GetHost();
		}

		if (cPrefix == '~') {
			if (m_msChans.find(sTarget.AsLower()) == m_msChans.end()) {
				m_pClient->PutClient(":" + m_pUser->GetIRCServer() + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sTarget + " :No such channel");
				return HALT;
			}

			PutChan(sTarget, ":?" + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " PRIVMSG " + sTarget + " :" + sMessage, true, false);
		} else {
			CString sNick = sTarget.LeftChomp_n(1);
			CUser* pUser = CZNC::Get().FindUser(sNick);

			if (pUser) {
				pUser->PutUser(":?" + m_pUser->GetUserName() + "!" + m_pUser->GetIdent() + "@" + sHost + " PRIVMSG " + pUser->GetIRCNick().GetNick() + " :" + sMessage);
			} else {
				m_pClient->PutClient(":" + m_pUser->GetIRCServer() + " 403 " + m_pUser->GetIRCNick().GetNick() + " " + sTarget + " :No such znc user: " + sNick + "");
			}
		}

		return HALT;
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);

		if (sCommand.CaseCmp("HELP") == 0) {
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Arguments");
			Table.AddColumn("Description");

			Table.AddRow(); Table.SetCell("Command", "Help"); Table.SetCell("Arguments", ""); Table.SetCell("Description", "List all partyline commands");
			Table.AddRow(); Table.SetCell("Command", "List"); Table.SetCell("Arguments", ""); Table.SetCell("Description", "List all open channels");

			unsigned int uTableIdx = 0;
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutModule(sLine);
			}
		} else if (sCommand.CaseCmp("LIST") == 0) {
			if (!m_msChans.size()) {
				PutModule("There are no open channels.");
				return;
			}

			CTable Table;

			Table.AddColumn("Channel");
			Table.AddColumn("Users");

			for (map<CString, set<CString> >::const_iterator a = m_msChans.begin(); a != m_msChans.end(); a++) {
				Table.AddRow();

				Table.SetCell("Channel", a->first);
				Table.SetCell("Users", CString::ToString(a->second.size()));
			}

			unsigned int uTableIdx = 0;
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutModule(sLine);
			}
		} else {
			PutModule("Unkown command, try 'HELP'");
		}
	}

	bool PutChan(const CString& sChan, const CString& sLine, bool bIncludeCurUser = true, bool bIncludeClient = true) {
		map<CString, set<CString> >::iterator it  = m_msChans.find(sChan.AsLower());

		if (it != m_msChans.end()) {
			PutChan(it->second, sLine, bIncludeCurUser, bIncludeClient);
			return true;
		}

		return false;
	}

	void PutChan(const set<CString>& ssNicks, const CString& sLine, bool bIncludeCurUser = true, bool bIncludeClient = true) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
			if (ssNicks.find(it->first) != ssNicks.end()) {
				if (it->second == m_pUser) {
					if (bIncludeCurUser) {
						it->second->PutUser(sLine, NULL, (bIncludeClient ? NULL : m_pClient));
					}
				} else {
					it->second->PutUser(sLine);
				}
			}
		}
	}

	void SendNickList(set<CString>& ssNicks, const CString& sChan) {
		CString sNickList;
		for (set<CString>::iterator it = ssNicks.begin(); it != ssNicks.end(); it++) {
			CUser* pUser = CZNC::Get().FindUser(*it);

			if (pUser && pUser->IsUserAttached()) {
				sNickList += (pUser->IsAdmin()) ? "@" : "+";
			}

			sNickList += "?" + (*it) + " ";

			if (sNickList.size() >= 500) {
				m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 353 " + m_pUser->GetIRCNick().GetNick() + " @ " + sChan + " :" + sNickList);
				sNickList.clear();
			}
		}

		if (sNickList.size()) {
			m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 353 " + m_pUser->GetIRCNick().GetNick() + " @ " + sChan + " :" + sNickList);
		}

		m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 366 " + m_pUser->GetIRCNick().GetNick() + " " + sChan + " :End of /NAMES list.");
	}

private:
	map<CString, set<CString> >	m_msChans;
	set<CUser*>					m_spInjectedPrefixes;
};

GLOBALMODULEDEFS(CPartylineMod, "Internal channels and queries for users connected to znc");
