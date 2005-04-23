#include "Chan.h"
#include "znc.h"
#include "User.h"
#include "Utils.h"

CChan::CChan(const string& sName, CUser* pUser) {
	m_sName = sName;
	m_pUser = pUser;
	m_bAutoCycle = true;
	m_bDetached = false;
	m_uBufferCount = m_pUser->GetBufferCount();
	m_bKeepBuffer = m_pUser->KeepBuffer();
	Reset();
}
CChan::~CChan() {
	ClearNicks();
}

void CChan::Reset() {
	m_bWhoDone = false;
	m_bIsOn = false;
	m_bIsOp = false;
	m_bIsVoice = false;
	m_uOpCount = 0;
	m_uVoiceCount =	0;
	m_uModes = 0;
	m_uLimit = 0;
	m_uClientRequests = 0;
	ClearNicks();
}

void CChan::Joined() {
}

void CChan::Cycle() const {
	m_pUser->PutIRC("PART " + GetName() + "\r\nJOIN " + GetName() + " " + GetKey());
}

void CChan::JoinUser() {
	if (!IsOn()) {
		IncClientRequests();
		m_pUser->PutIRC("JOIN " + GetName());
		return;
	}

	m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " JOIN :" + GetName());

	if (!GetTopic().empty()) {
		m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 332 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " :" + GetTopic());
		m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 333 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " " + GetTopicOwner() + " " + CUtils::ToString(GetTopicDate()));
	}

	string sPre = ":" + m_pUser->GetIRCServer() + " 353 " + m_pUser->GetIRCNick().GetNick() + " = " + GetName() + " :";
	string sLine = sPre;

	for (map<string,CNick*>::iterator a = m_msNicks.begin(); a != m_msNicks.end(); a++) {
		if (a->second->IsOp()) {
			sLine += "@";
		} else if (a->second->IsVoice()) {
			sLine += "+";
		}

		sLine += a->first;

		if (sLine.size() >= 490 || a == (--m_msNicks.end())) {
			m_pUser->PutUser(sLine);
			sLine = sPre;
		} else {
			sLine += " ";
		}
	}

	m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 366 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " :End of /NAMES list.");

	//m_pUser->PutIRC("NAMES " + GetName());
	//m_pUser->PutIRC("TOPIC " + GetName());

	SendBuffer();

	m_bDetached = false;
}

void CChan::SendBuffer() {
	if (m_pUser->IsUserAttached()) {
		const vector<string>& vsBuffer = GetBuffer();

		if (vsBuffer.size()) {
			m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + GetName() + " :Buffer Playback...");

			for (unsigned int a = 0; a < vsBuffer.size(); a++) {
				m_pUser->PutUser(vsBuffer[a]);
			}

			if (!KeepBuffer()) {
				ClearBuffer();
			}

			m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + GetName() + " :Playback Complete.");
		}
	}
}

void CChan::DetachUser() {
	m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " PART " + GetName());
	m_bDetached = true;
}

string CChan::GetModeString() const {
	string sRet;

	if (m_uModes & Secret) { sRet += "s"; }
	if (m_uModes & Private) { sRet += "p"; }
	if (m_uModes & OpTopic) { sRet += "t"; }
	if (m_uModes & InviteOnly) { sRet += "i"; }
	if (m_uModes & NoMessages) { sRet += "n"; }
	if (m_uModes & Moderated) { sRet += "m"; }
	if (m_uLimit) { sRet += "l"; }
	if (m_uModes & Key) { sRet += "k"; }

	return (sRet.empty()) ? sRet : ("+" + sRet);
}

void CChan::SetModes(const string& sModes) {
	m_uModes = 0;
	m_uLimit = 0;
	m_sKey = "";
	ModeChange(sModes);
}

void CChan::IncClientRequests() {
	m_uClientRequests++;
}

bool CChan::DecClientRequests() {
	if (!m_uClientRequests) {
		return false;
	}

	m_uClientRequests--;
	return true;
}

bool CChan::Who() {
	if (m_bWhoDone) {
		return false;
	}

	m_pUser->PutIRC("WHO " + GetName());
	return true;
}

void CChan::OnWho(const string& sNick, const string& sIdent, const string& sHost) {
	CNick* pNick = FindNick(sNick);

	if (pNick) {
		pNick->SetIdent(sIdent);
		pNick->SetHost(sHost);
	}
}

void CChan::ModeChange(const string& sModes, const string& sOpNick) {
	string sModeArg = CUtils::Token(sModes, 0);
	string sArgs = CUtils::Token(sModes, 1, true);
	bool bAdd = true;

#ifdef _MODULES
	CNick* pNick = FindNick(sOpNick);
	if (pNick) {
		m_pUser->GetModules().OnRawMode(*pNick, *this, sModeArg, sArgs);
	}
#endif

	for (unsigned int a = 0; a < sModeArg.size(); a++) {
		switch (sModeArg[a]) {
			case '+': bAdd = true; break;
			case '-': bAdd = false; break;
			case 's': if (bAdd) { m_uModes |= Secret; } else { m_uModes &= ~Secret; } break;
			case 'p': if (bAdd) { m_uModes |= Private; } else { m_uModes &= ~Private; }  break;
			case 'm': if (bAdd) { m_uModes |= Moderated; } else { m_uModes &= ~Moderated; }  break;
			case 'i': if (bAdd) { m_uModes |= InviteOnly; } else { m_uModes &= ~InviteOnly; }  break;
			case 'n': if (bAdd) { m_uModes |= NoMessages; } else { m_uModes &= ~NoMessages; }  break;
			case 't': if (bAdd) { m_uModes |= OpTopic; } else { m_uModes &= ~OpTopic; }  break;
			case 'l': if (bAdd) { m_uLimit = strtoul(GetModeArg(sArgs).c_str(), NULL, 10); } else { m_uLimit = 0; }  break;
			case 'k': if (bAdd) { m_uModes |= Key; SetKey(GetModeArg(sArgs)); } else { m_uModes &= ~Key; GetModeArg(sArgs); }  break;
			case 'o': OnOp(sOpNick, GetModeArg(sArgs), bAdd); break;
			case 'v': OnVoice(sOpNick, GetModeArg(sArgs), bAdd); break;
			case 'b': // Don't do anything with bans yet
			case 'e': // Don't do anything with excepts yet
			default: GetModeArg(sArgs); // Pop off an arg, assume new modes will have an argument
		}
	}
}

string CChan::GetModeArg(string& sArgs) const {
	string sRet = sArgs.substr(0, sArgs.find(' '));
	sArgs = (sRet.size() < sArgs.size()) ? sArgs.substr(sRet.size() +1) : "";
	return sRet;
}

void CChan::ClearNicks() {
	for (map<string,CNick*>::iterator a = m_msNicks.begin(); a != m_msNicks.end(); a++) {
		delete a->second;
	}

	m_msNicks.clear();
}

int CChan::AddNicks(const string& sNicks) {
	if (IsOn()) {
		return 0;
	}

	int iRet = 0;
	string sCurNick;

	for (unsigned int a = 0; a < sNicks.size(); a++) {
		switch (sNicks[a]) {
			case ' ':
				if (AddNick(sCurNick)) {
					iRet++;
				}

				sCurNick = "";

				break;
			default:
				sCurNick += sNicks[a];
				break;
		}
	}

	if (!sCurNick.empty()) {
		if (AddNick(sCurNick)) {
			iRet++;
		}
	}

	return iRet;
}

bool CChan::AddNick(const string& sNick) {
	const char* p = sNick.c_str();
	bool bIsOp = false;
	bool bIsVoice = false;

	switch (*p) {
		case '\0':
			return false;
		case '@':
			bIsOp = true;
		case '+':
			if (!*++p) {
				return false;
			}

			bIsVoice = !bIsOp;
	}

	CNick* pNick = FindNick(p);
	if (!pNick) {
		pNick = new CNick(p);
	}

	if ((bIsOp) && (!pNick->IsOp())) {
		IncOpCount();
		pNick->SetOp(true);

		if (strcasecmp(pNick->GetNick().c_str(), m_pUser->GetCurNick().c_str()) == 0) {
			SetOpped(true);
		}
	} else if ((bIsVoice) && (!pNick->IsVoice())) {
		IncVoiceCount();
		pNick->SetVoice(true);

		if (strcasecmp(pNick->GetNick().c_str(), m_pUser->GetCurNick().c_str()) == 0) {
			SetVoiced(true);
		}
	}

	m_msNicks[pNick->GetNick()] = pNick;

	return true;
}

bool CChan::RemNick(const string& sNick) {
	map<string,CNick*>::iterator it = m_msNicks.find(sNick);
	if (it == m_msNicks.end()) {
		return false;
	}

	if (it->second->IsOp()) {
		DecOpCount();
	}

	if (it->second->IsVoice()) {
		DecVoiceCount();
	}

	delete it->second;
	m_msNicks.erase(it);
	CNick* pNick = m_msNicks.begin()->second;

	if ((m_msNicks.size() == 1) && (!pNick->IsOp()) && (strcasecmp(pNick->GetNick().c_str(), m_pUser->GetCurNick().c_str()) == 0)) {
		if (AutoCycle()) {
			Cycle();
		}
	}

	return true;
}

bool CChan::ChangeNick(const string& sOldNick, const string& sNewNick) {
	map<string,CNick*>::iterator it = m_msNicks.find(sOldNick);

	if (it == m_msNicks.end()) {
		return false;
	}

	// Rename this nick
	it->second->SetNick(sNewNick);

	// Insert a new element into the map then erase the old one, do this to change the key
	m_msNicks[sNewNick] = it->second;
	m_msNicks.erase(it);

	return true;
}

void CChan::OnOp(const string& sOpNick, const string& sNick, bool bOpped) {
	CNick* pNick = FindNick(sNick);

	if (pNick) {
		bool bNoChange = (pNick->IsOp() == bOpped);
#ifdef _MODULES
		CNick* pOpNick = FindNick(sOpNick);

		if (pOpNick) {
			if (bOpped) {
				m_pUser->GetModules().OnOp(*pOpNick, *pNick, *this, bNoChange);
			} else {
				m_pUser->GetModules().OnDeop(*pOpNick, *pNick, *this, bNoChange);
			}
		}
#endif

		if (strcasecmp(sNick.c_str(), m_pUser->GetCurNick().c_str()) == 0) {
			SetOpped(bOpped);
		}

		if (bNoChange) {
			// If no change, return
			return;
		}

		pNick->SetOp(bOpped);
		(bOpped) ? IncOpCount() : DecOpCount();
	}
}

void CChan::OnVoice(const string& sOpNick, const string& sNick, bool bVoiced) {
	CNick* pNick = FindNick(sNick);

	if (pNick) {
		bool bNoChange = (pNick->IsVoice() == bVoiced);

#ifdef _MODULES
		CNick* pOpNick = FindNick(sOpNick);

		if (pOpNick) {
			if (bVoiced) {
				m_pUser->GetModules().OnVoice(*pOpNick, *pNick, *this, bNoChange);
			} else {
				m_pUser->GetModules().OnDevoice(*pOpNick, *pNick, *this, bNoChange);
			}
		}
#endif

		if (strcasecmp(sNick.c_str(), m_pUser->GetCurNick().c_str()) == 0) {
			SetVoiced(bVoiced);
		}

		if (bNoChange) {
			// If no change, return
			return;
		}

		pNick->SetVoice(bVoiced);
		(bVoiced) ? IncVoiceCount() : DecVoiceCount();
	}
}

CNick* CChan::FindNick(const string& sNick) const {
	map<string,CNick*>::const_iterator it = m_msNicks.find(sNick);
	return (it != m_msNicks.end()) ? it->second : NULL;
}

int CChan::AddBuffer(const string& sLine) {
	// Todo: revisit the buffering
	if (!m_uBufferCount) {
		return 0;
	}

	if (m_vsBuffer.size() >= m_uBufferCount) {
		m_vsBuffer.erase(m_vsBuffer.begin());
	}

	m_vsBuffer.push_back(sLine);
	return m_vsBuffer.size();
}

void CChan::ClearBuffer() {
	m_vsBuffer.clear();
}

