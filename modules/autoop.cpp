#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CAutoOpMod;

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

	CAutoOpMod*		m_pParent;
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
		for (set<CString>::iterator it = m_ssChans.begin(); it != m_ssChans.end(); it++) {
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

		for (set<CString>::iterator it = m_ssChans.begin(); it != m_ssChans.end(); it++) {
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

		for (set<CString>::iterator it = m_ssChans.begin(); it != m_ssChans.end(); it++) {
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
	CString			m_sUsername;
	CString			m_sUserKey;
	CString			m_sHostmask;
	set<CString>	m_ssChans;
};

class CAutoOpMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoOpMod) {}

	virtual bool OnLoad(const CString& sArgs) {
		AddTimer(new CAutoOpTimer(this));

		// Load the users
		for (MCString::iterator it = BeginNV(); it != EndNV(); it++) {
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
	}

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {
	}

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	}

	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	}

	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	}

	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		if (Channel.HasPerm(CChan::Op)) {																						// If we have ops in this chan
			for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
				if (it->second->HostMatches(Nick.GetHostMask()) && it->second->ChannelMatches(Channel.GetName())) {				// and the nick who joined is a valid user
					if (it->second->GetUserKey().CaseCmp("__NOKEY__") == 0) {
						PutIRC("MODE " + Channel.GetName() + " +o " + Nick.GetNick());
					} else {
						m_msQueue[Nick.GetNick().AsLower()] = "";		// then insert this nick into the queue
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
		if (sMessage.Token(0).CaseCmp("!ZNCAO") != 0) {
			return CONTINUE;
		}

		CString sCommand = sMessage.Token(1);

		if (sCommand.CaseCmp("CHALLENGE") == 0) {
			ChallengeRespond(Nick, sMessage.Token(2));
		} else if (sCommand.CaseCmp("RESPONSE") == 0) {
			VerifyResponse(Nick, sMessage.Token(2));
		}

		return HALTCORE;
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0).AsUpper();

		if (sCommand.CaseCmp("HELP") == 0) {
			PutModule("Commands are: ListUsers, AddChans, DelChans, AddUser, DelUser");
		} else if (sCommand.CaseCmp("TIMERS") == 0) {
			ListTimers();
		} else if (sCommand.CaseCmp("ADDUSER") == 0 || sCommand.CaseCmp("DELUSER") == 0) {
			CString sUser = sLine.Token(1);
			CString sHost = sLine.Token(2);
			CString sKey = sLine.Token(3);

			if (sCommand.CaseCmp("ADDUSER") == 0) {
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
		} else if (sCommand.CaseCmp("LISTUSERS") == 0) {
			if (m_msUsers.empty()) {
				PutModule("There are no users defined");
				return;
			}

			CTable Table;

			Table.AddColumn("User");
			Table.AddColumn("Hostmask");
			Table.AddColumn("Key");
			Table.AddColumn("Channels");

			for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
				Table.AddRow();
				Table.SetCell("User", it->second->GetUsername());
				Table.SetCell("Hostmask", it->second->GetHostmask());
				Table.SetCell("Key", it->second->GetUserKey());
				Table.SetCell("Channels", it->second->GetChannels());
			}

			CString sLine;
			unsigned int uTableIdx = 0;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutModule(sLine);
			}
		} else if (sCommand.CaseCmp("ADDCHANS") == 0 || sCommand.CaseCmp("DELCHANS") == 0) {
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

			if (sCommand.CaseCmp("ADDCHANS") == 0) {
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
		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
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

	/* This isn't being used yet
	bool RequestOps(const CString& sChannel) {
		CChan* pChan = m_pUser->FindChan(sChannel);
		return (pChan) ? RequestOps(*pChan) : false;
	}

	bool RequestOps(const CChan& Channel) {
		if (Channel.HasPerm(CChan::Op)) {
			return false;					// If we already have ops then don't bother
		}

		const map<CString,CNick*>& msNicks = Channel.GetNicks();
		VCString vsNicks;

		for (map<CString,CNick*>::const_iterator it = msNicks.begin(); it != msNicks.end(); it++) {
			const CNick& Nick = *it->second;

			if (Nick.HasPerm(CChan::Op)) {								// Ok, this guy is an op, now lets make sure he matches one of our defined users
				for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
					cerr << "??? [" << Nick.GetHostMask() << "] [" << Channel.GetName() << "]" << endl;
					if (it->second->HostMatches(Nick.GetHostMask()) && it->second->ChannelMatches(Channel.GetName())) {
						vsNicks.push_back(Nick.GetNick());				// Add the nick into a vector so we chan choose one at random later
						break;											// Ok, we found a match, on to the next op in the channel
					}
				}
			}
		}

		if (!vsNicks.size()) {
			return false;
		}

		// Need to make this random in the future, just ask the first one for now
		PutIRC("NOTICE " + vsNicks[0] + " :!ZNCAO REQUEST " + Channel.GetName());

		return true;
	}*/

	bool ChallengeRespond(const CNick& Nick, const CString& sChallenge) {
		// Validate before responding - don't blindly trust everyone
		bool bValid = false;
		bool bMatchedHost = false;
		CAutoOpUser* pUser = NULL;

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
			pUser = it->second;

			// First verify that the guy who challenged us matches a user's host
			if (pUser->HostMatches(Nick.GetHostMask())) {
				const vector<CChan*>& Chans = m_pUser->GetChans();
				bMatchedHost = true;

				// Also verify that they are opped in at least one of the user's chans
				for (size_t a = 0; a < Chans.size(); a++) {
					const CChan& Chan = *Chans[a];

					CNick* pNick = Chan.FindNick(Nick.GetNick());

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

		for (map<CString, CAutoOpUser*>::iterator it = m_msUsers.begin(); it != m_msUsers.end(); it++) {
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

			for (MCString::iterator it = m_msQueue.begin(); it != m_msQueue.end(); it++) {
				if (!it->second.empty()) {
					m_msQueue.erase(it);
					bRemoved = true;
					break;
				}
			}
		}

		// Now issue challenges for the new users in the queue
		for (MCString::iterator it = m_msQueue.begin(); it != m_msQueue.end(); it++) {
			it->second = CString::RandomString(32);
			PutIRC("NOTICE " + it->first + " :!ZNCAO CHALLENGE " + it->second);
		}
	}

	void OpUser(const CNick& Nick, const CAutoOpUser& User) {
		const vector<CChan*>& Chans = m_pUser->GetChans();

		for (size_t a = 0; a < Chans.size(); a++) {
			const CChan& Chan = *Chans[a];

			if (Chan.HasPerm(CChan::Op) && User.ChannelMatches(Chan.GetName())) {
				CNick* pNick = Chan.FindNick(Nick.GetNick());

				if (pNick && !pNick->HasPerm(CChan::Op)) {
					PutIRC("MODE " + Chan.GetName() + " +o " + Nick.GetNick());
				}
			}
		}
	}
private:
	map<CString, CAutoOpUser*>		m_msUsers;
	MCString						m_msQueue;
};

void CAutoOpTimer::RunJob() {
	m_pParent->ProcessQueue();
}

MODULEDEFS(CAutoOpMod, "Auto op the good guys")
