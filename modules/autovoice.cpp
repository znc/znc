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
		for (map<CString, CAutoVoiceUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); ++it) {
			delete it->second;
		}

		m_msUsers.clear();
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		// If we have ops in this chan
		if (Channel.HasPerm(CChan::Op) || Channel.HasPerm(CChan::HalfOp)) {
			CheckAutoVoice(Nick, Channel);
		}
	}

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick()) {
			VoiceNicks(Channel);
		}
	}

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {
		if ((Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick()) && (bAdded) && (uMode = 'h')){
			VoiceNicks(Channel);
		}
	}
	
	void VoiceNicks(CChan& Channel) {
		const map<CString,CNick>& msNicks = Channel.GetNicks();
	
		for (map<CString,CNick>::const_iterator it = msNicks.begin(); it != msNicks.end(); ++it) {
			if (!it->second.HasPerm(CChan::Voice)) {
				CheckAutoVoice(it->second, Channel);
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

	virtual EModRet OnRaw(CString& sLine) {
		CString sCmd = sLine.Token(1);
		unsigned int uRaw = sCmd.ToUInt();;
		CString sRest = sLine.Token(3, true);
		sRest.Trim();
		
		switch(uRaw) {
			case 352: {	// WHO reply
				CString sIdent = sLine.Token(4);
				CString sHost = sLine.Token(5);					
				CString sNick = sLine.Token(7);			
				const vector<CChan*>& vChans = m_pNetwork->GetChans();
	
				for (unsigned int a = 0; a < vChans.size(); a++) {
					CChan *pChan = vChans[a];
					CNick *pNick = pChan->FindNick(sNick);
					if (pNick) {
						// Let the channel know about the full who details or FindUserByHost
						// will fail in CheckAutoOp
						vChans[a]->OnWho(sNick, sIdent, sHost);
						CheckAutoVoice(*pNick, *pChan);
					}	
				}
				break;
			}
				
			case 353:  { // NAMES reply
				CString sNicks = sRest.Token(2, true).TrimPrefix_n();
				VCString vsNicks;
				VCString::iterator it;
	
				sNicks.Split(" ", vsNicks, false);
	
				for (it = vsNicks.begin(); it != vsNicks.end(); ++it) {
					CString sNick = *it;
					if (CString::npos == sNick.find('!')) {
						// We weren't given any hostmask info for the nick so let's ask for it.
						PutIRC("WHO " + sNick);	
					}
				}
				break;
			}
		}
		return CONTINUE;
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
	
	bool CheckAutoVoice(const CNick& Nick, CChan& Channel) {
		CAutoVoiceUser *pUser = FindUserByHost(Nick.GetHostMask(), Channel.GetName());

		if (pUser) {
			PutIRC("MODE " + Channel.GetName() + " +v " + Nick.GetNick());
		}
		return pUser;
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

template<> void TModInfo<CAutoVoiceMod>(CModInfo& Info) {
	Info.SetWikiPage("autovoice");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Each argument is either a channel you want autovoice for (which can include wildcards) or, if it starts with !, it is an exception for autovoice.");
}

NETWORKMODULEDEFS(CAutoVoiceMod, "Auto voice the good people")
