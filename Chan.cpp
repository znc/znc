/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "IRCSock.h"
#include "User.h"
#include "znc.h"

CChan::CChan(const CString& sName, CUser* pUser, bool bInConfig) {
	m_sName = sName.Token(0);
	m_sKey = sName.Token(1);
	m_pUser = pUser;

	if (!m_pUser->IsChan(m_sName)) {
		m_sName = "#" + m_sName;
	}

	m_bInConfig = bInConfig;
	m_Nick.SetUser(pUser);
	m_bAutoCycle = m_pUser->AutoCycle();
	m_bDetached = false;
	m_uBufferCount = m_pUser->GetBufferCount();
	m_bKeepBuffer = m_pUser->KeepBuffer();
	m_bDisabled = false;
	Reset();
}

CChan::~CChan() {
	ClearNicks();
}

void CChan::Reset() {
	m_bWhoDone = false;
	m_bIsOn = false;
	m_suUserPerms.clear();
	m_musModes.clear();
	m_muuPermCount.clear();
	m_uLimit = 0;
	m_uClientRequests = 0;
	m_sTopic = "";
	m_sTopicOwner = "";
	m_ulTopicDate = 0;
	m_ulCreationDate = 0;
	ClearNicks();
	ResetJoinTries();
}

bool CChan::WriteConfig(CFile& File) {
	if (!InConfig()) {
		return false;
	}

	File.Write("\t<Chan " + GetName() + ">\n");

	if (m_pUser->GetBufferCount() != GetBufferCount())
		File.Write("\t\tBuffer     = " + CString(GetBufferCount()) + "\n");
	if (m_pUser->KeepBuffer() != KeepBuffer())
		File.Write("\t\tKeepBuffer = " + CString((KeepBuffer()) ? "true" : "false") + "\n");
	if (IsDetached())
		File.Write("\t\tDetached   = true\n");
	if (m_pUser->AutoCycle() != AutoCycle())
		File.Write("\t\tAutoCycle  = " + CString((AutoCycle()) ? "true" : "false") + "\n");
	if (!GetKey().empty()) { File.Write("\t\tKey        = " + GetKey() + "\n"); }
	if (!GetDefaultModes().empty()) { File.Write("\t\tModes      = " + GetDefaultModes() + "\n"); }

	File.Write("\t</Chan>\n");
	return true;
}

void CChan::Joined() {
}

void CChan::Cycle() const {
	m_pUser->PutIRC("PART " + GetName() + "\r\nJOIN " + GetName() + " " + GetKey());
}

void CChan::JoinUser(bool bForce, const CString& sKey, CClient* pClient) {
	if (!bForce && (!IsOn() || !IsDetached())) {
		IncClientRequests();

		m_pUser->PutIRC("JOIN " + GetName() + " " + ((sKey.empty()) ? GetKey() : sKey));
		return;
	}

	m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " JOIN :" + GetName(), pClient);

	if (!GetTopic().empty()) {
		m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 332 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " :" + GetTopic(), pClient);
		m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 333 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " " + GetTopicOwner() + " " + CString(GetTopicDate()), pClient);
	}

	CString sPre = ":" + m_pUser->GetIRCServer() + " 353 " + m_pUser->GetIRCNick().GetNick() + " " + GetModeForNames() + " " + GetName() + " :";
	CString sLine = sPre;
	CString sPerm, sNick;

	vector<CClient*>& vpClients = m_pUser->GetClients();
	for (vector<CClient*>::iterator it = vpClients.begin(); it != vpClients.end(); it++) {
		CClient* pThisClient;
		if (!pClient)
			pThisClient = *it;
		else
			pThisClient = pClient;

		for (map<CString,CNick*>::iterator a = m_msNicks.begin(); a != m_msNicks.end(); a++) {
			if (pThisClient->HasNamesx()) {
				sPerm = a->second->GetPermStr();
			} else {
				char c = a->second->GetPermChar();
				sPerm = "";
				if (c != '\0') {
					sPerm += c;
				}
			}
			if (pThisClient->HasUHNames() && !a->second->GetIdent().empty() && !a->second->GetHost().empty()) {
				sNick = a->first + "!" + a->second->GetIdent() + "@" + a->second->GetHost();
			} else {
				sNick = a->first;
			}

			sLine += sPerm + sNick;

			if (sLine.size() >= 490 || a == (--m_msNicks.end())) {
				m_pUser->PutUser(sLine, pThisClient);
				sLine = sPre;
			} else {
				sLine += " ";
			}
		}

		if (pClient) // We only want to do this for one client
			break;
	}

	m_pUser->PutUser(":" + m_pUser->GetIRCServer() + " 366 " + m_pUser->GetIRCNick().GetNick() + " " + GetName() + " :End of /NAMES list.", pClient);
	m_bDetached = false;

	// Send Buffer
	SendBuffer(pClient);
}

void CChan::DetachUser() {
	if (!m_bDetached) {
		m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " PART " + GetName());
		m_bDetached = true;
	}
}

void CChan::AttachUser() {
	if (m_bDetached) {
		m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " JOIN " + GetName());
		m_bDetached = false;
	}
}

CString CChan::GetModeString() const {
	CString sModes, sArgs;

	for (map<unsigned char, CString>::const_iterator it = m_musModes.begin(); it != m_musModes.end(); it++) {
		sModes += it->first;
		if (it->second.size()) {
			sArgs += " " + it->second;
		}
	}

	return sModes.empty() ? sModes : CString("+" + sModes + sArgs);
}

CString CChan::GetModeForNames() const {
	CString sMode;

	for (map<unsigned char, CString>::const_iterator it = m_musModes.begin(); it != m_musModes.end(); it++) {
		if (it->first == 's') {
		    sMode = "@";
		} else if ((it->first == 'p') && sMode.empty()){
		    sMode = "*";
		}
	}

	return (sMode.empty() ? "=" : sMode);
}

void CChan::SetModes(const CString& sModes) {
	m_musModes.clear();
	m_uLimit = 0;
	m_sCurKey = "";
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

void CChan::OnWho(const CString& sNick, const CString& sIdent, const CString& sHost) {
	CNick* pNick = FindNick(sNick);

	if (pNick) {
		pNick->SetIdent(sIdent);
		pNick->SetHost(sHost);
	}
}

void CChan::ModeChange(const CString& sModes, const CString& sOpNick) {
	CString sModeArg = sModes.Token(0);
	CString sArgs = sModes.Token(1, true);
	bool bAdd = true;

#ifdef _MODULES
	CNick* pOpNick = FindNick(sOpNick);

	if (pOpNick) {
		MODULECALL(OnRawMode(*pOpNick, *this, sModeArg, sArgs), m_pUser, NULL, );
	}
#endif

	for (unsigned int a = 0; a < sModeArg.size(); a++) {
		const unsigned char& uMode = sModeArg[a];

		if (uMode == '+') {
			bAdd = true;
		} else if (uMode == '-') {
			bAdd = false;
		} else if (m_pUser->GetIRCSock()->IsPermMode(uMode)) {
			CString sArg = GetModeArg(sArgs);
			CNick* pNick = FindNick(sArg);
			if (pNick) {
				unsigned char uPerm = m_pUser->GetIRCSock()->GetPermFromMode(uMode);

				if (uPerm) {
					if (bAdd) {
						if (pNick->AddPerm(uPerm)) {
							IncPermCount(uPerm);
						}

						if (pNick->GetNick().CaseCmp(m_pUser->GetCurNick()) == 0) {
							AddPerm(uPerm);
						}
					} else {
						if (pNick->RemPerm(uPerm)) {
							DecPermCount(uPerm);
						}

						if (pNick->GetNick().CaseCmp(m_pUser->GetCurNick()) == 0) {
							RemPerm(uPerm);
						}
					}
#ifdef _MODULES
					bool bNoChange = (pNick->HasPerm(uPerm) == bAdd);

					if (uMode && pOpNick) {
						MODULECALL(OnChanPermission(*pOpNick, *pNick, *this, uMode, bAdd, bNoChange), m_pUser, NULL, );

						if (uMode == CChan::M_Op) {
							if (bAdd) {
								MODULECALL(OnOp(*pOpNick, *pNick, *this, bNoChange), m_pUser, NULL, );
							} else {
								MODULECALL(OnDeop(*pOpNick, *pNick, *this, bNoChange), m_pUser, NULL, );
							}
						} else if (uMode == CChan::M_Voice) {
							if (bAdd) {
								MODULECALL(OnVoice(*pOpNick, *pNick, *this, bNoChange), m_pUser, NULL, );
							} else {
								MODULECALL(OnDevoice(*pOpNick, *pNick, *this, bNoChange), m_pUser, NULL, );
							}
						}
					}
#endif
				}
			}
		} else {
			bool bList = false;
			CString sArg;

			switch (m_pUser->GetIRCSock()->GetModeType(uMode)) {
				case CIRCSock::ListArg:
					bList = true;
					sArg = GetModeArg(sArgs);
					break;
				case CIRCSock::HasArg:
					sArg = GetModeArg(sArgs);
					break;
				case CIRCSock::NoArg:
					break;
				case CIRCSock::ArgWhenSet:
					if (bAdd) {
						sArg = GetModeArg(sArgs);
					}

					break;
			}

			if (uMode == M_Key) {
				m_sCurKey = (bAdd) ? sArg : "";
			}

			if (!bList) {
				(bAdd) ? AddMode(uMode, sArg) : RemMode(uMode, sArg);
			}
		}
	}
}

CString CChan::GetOptions() const {
	CString sRet;

	if (IsDetached()) {
		sRet += (sRet.empty()) ? "Detached" : ", Detached";
	}

	if (AutoCycle()) {
		sRet += (sRet.empty()) ? "AutoCycle" : ", AutoCycle";
	}

	if (KeepBuffer()) {
		sRet += (sRet.empty()) ? "KeepBuffer" : ", KeepBuffer";
	}

	return sRet;
}

CString CChan::GetModeArg(unsigned char uMode) const {
	if (uMode) {
		map<unsigned char, CString>::const_iterator it = m_musModes.find(uMode);

		if (it != m_musModes.end()) {
			return it->second;
		}
	}

	return "";
}

bool CChan::HasMode(unsigned char uMode) const {
	return (uMode && m_musModes.find(uMode) != m_musModes.end());
}

bool CChan::AddMode(unsigned char uMode, const CString& sArg) {
	if (HasMode(uMode)) {
		return false;
	}

	m_musModes[uMode] = sArg;
	return true;
}

bool CChan::RemMode(unsigned char uMode, const CString& sArg) {
	if (!HasMode(uMode)) {
		return false;
	}

	m_musModes.erase(uMode);
	return true;
}

CString CChan::GetModeArg(CString& sArgs) const {
	CString sRet = sArgs.substr(0, sArgs.find(' '));
	sArgs = (sRet.size() < sArgs.size()) ? sArgs.substr(sRet.size() +1) : "";
	return sRet;
}

void CChan::ClearNicks() {
	for (map<CString,CNick*>::iterator a = m_msNicks.begin(); a != m_msNicks.end(); a++) {
		delete a->second;
	}

	m_msNicks.clear();
}

int CChan::AddNicks(const CString& sNicks) {
	if (IsOn()) {
		return 0;
	}

	int iRet = 0;
	CString sCurNick;

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

bool CChan::AddNick(const CString& sNick) {
	const char* p = sNick.c_str();
	CString sPrefix, sTmp, sIdent, sHost;

	while (m_pUser->GetIRCSock()->IsPermChar(*p)) {
		sPrefix += *p;

		if (!*++p) {
			return false;
		}
	}

	sTmp = p;
	sIdent = sTmp.Token(1, true, "!").Token(0, false, "@");
	sHost = sTmp.Token(1, true, "@");
	sTmp = sTmp.Token(0, false, "!");

	CNick* pNick = FindNick(sTmp);
	if (!pNick) {
		pNick = new CNick(sTmp);
		pNick->SetUser(m_pUser);
	}

	if (!sIdent.empty())
		pNick->SetIdent(sIdent);
	if (!sHost.empty())
		pNick->SetHost(sHost);

	for (CString::size_type i = 0; i < sPrefix.length(); i++) {
		if (pNick->AddPerm(sPrefix[i])) {
			IncPermCount(sPrefix[i]);
		}
	}

	if (pNick->GetNick().CaseCmp(m_pUser->GetCurNick()) == 0) {
		for (CString::size_type i = 0; i < sPrefix.length(); i++) {
			AddPerm(sPrefix[i]);
		}
	}

	m_msNicks[pNick->GetNick()] = pNick;

	return true;
}

unsigned int CChan::GetPermCount(unsigned char uPerm) {
	map<unsigned char, unsigned int>::iterator it = m_muuPermCount.find(uPerm);
	return (it == m_muuPermCount.end()) ? 0 : it->second;
}

void CChan::DecPermCount(unsigned char uPerm) {
	map<unsigned char, unsigned int>::iterator it = m_muuPermCount.find(uPerm);

	if (it == m_muuPermCount.end()) {
		m_muuPermCount[uPerm] = 0;
	} else {
		if (it->second) {
			m_muuPermCount[uPerm]--;
		}
	}
}

void CChan::IncPermCount(unsigned char uPerm) {
	map<unsigned char, unsigned int>::iterator it = m_muuPermCount.find(uPerm);

	if (it == m_muuPermCount.end()) {
		m_muuPermCount[uPerm] = 1;
	} else {
		m_muuPermCount[uPerm]++;
	}
}

bool CChan::RemNick(const CString& sNick) {
	map<CString,CNick*>::iterator it;
	set<unsigned char>::iterator it2;

	it = m_msNicks.find(sNick);
	if (it == m_msNicks.end()) {
		return false;
	}

	const set<unsigned char>& suPerms = it->second->GetChanPerms();

	for (it2 = suPerms.begin(); it2 != suPerms.end(); it2++) {
		DecPermCount(*it2);
	}

	delete it->second;
	m_msNicks.erase(it);
	CNick* pNick = m_msNicks.begin()->second;

	if ((m_msNicks.size() == 1) && (!pNick->HasPerm(Op)) && (pNick->GetNick().CaseCmp(m_pUser->GetCurNick()) == 0)) {
		if (AutoCycle()) {
			Cycle();
		}
	}

	return true;
}

bool CChan::ChangeNick(const CString& sOldNick, const CString& sNewNick) {
	map<CString,CNick*>::iterator it = m_msNicks.find(sOldNick);

	if (it == m_msNicks.end()) {
		return false;
	}

	// Rename this nick
	it->second->SetNick(sNewNick);

	// Insert a new element into the map then erase the old one, do this to change the key to the new nick
	m_msNicks[sNewNick] = it->second;
	m_msNicks.erase(it);

	return true;
}

CNick* CChan::FindNick(const CString& sNick) const {
	map<CString,CNick*>::const_iterator it = m_msNicks.find(sNick);
	return (it != m_msNicks.end()) ? it->second : NULL;
}

int CChan::AddBuffer(const CString& sLine) {
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

void CChan::SendBuffer(CClient* pClient) {
	if (m_pUser && m_pUser->IsUserAttached()) {
		const vector<CString>& vsBuffer = GetBuffer();

		if (vsBuffer.size()) {
			m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + GetName() + " :Buffer Playback...", pClient);

			for (unsigned int a = 0; a < vsBuffer.size(); a++) {
				m_pUser->PutUser(vsBuffer[a], pClient);
			}

			if (!KeepBuffer()) {
				ClearBuffer();
			}

			m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + GetName() + " :Playback Complete.", pClient);
		}
	}
}
