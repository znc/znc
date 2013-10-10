/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Config.h>

using std::set;
using std::vector;
using std::map;

CChan::CChan(const CString& sName, CIRCNetwork* pNetwork, bool bInConfig, CConfig *pConfig) {
	m_sName = sName.Token(0);
	m_sKey = sName.Token(1);
	m_pNetwork = pNetwork;

	if (!m_pNetwork->IsChan(m_sName)) {
		m_sName = "#" + m_sName;
	}

	m_bInConfig = bInConfig;
	m_Nick.SetNetwork(m_pNetwork);
	m_bDetached = false;
	m_bDisabled = false;
	SetBufferCount(m_pNetwork->GetUser()->GetBufferCount(), true);
	SetAutoClearChanBuffer(m_pNetwork->GetUser()->AutoClearChanBuffer());
	Reset();

	if (pConfig) {
		CString sValue;
		if (pConfig->FindStringEntry("buffer", sValue))
			SetBufferCount(sValue.ToUInt(), true);
		if (pConfig->FindStringEntry("autoclearchanbuffer", sValue))
			SetAutoClearChanBuffer(sValue.ToBool());
		if (pConfig->FindStringEntry("keepbuffer", sValue))
			SetAutoClearChanBuffer(!sValue.ToBool()); // XXX Compatibility crap, added in 0.207
		if (pConfig->FindStringEntry("detached", sValue))
			SetDetached(sValue.ToBool());
		if (pConfig->FindStringEntry("autocycle", sValue))
			if (sValue.Equals("true"))
				CUtils::PrintError("WARNING: AutoCycle has been removed, instead try -> LoadModule = autocycle " + sName);
		if (pConfig->FindStringEntry("key", sValue))
			SetKey(sValue);
		if (pConfig->FindStringEntry("modes", sValue))
			SetDefaultModes(sValue);
	}
}

CChan::~CChan() {
	ClearNicks();
}

void CChan::Reset() {
	m_bIsOn = false;
	m_bModeKnown = false;
	m_musModes.clear();
	m_sTopic = "";
	m_sTopicOwner = "";
	m_ulTopicDate = 0;
	m_ulCreationDate = 0;
	m_Nick.Reset();
	ClearNicks();
	ResetJoinTries();
}

CConfig CChan::ToConfig() {
	CConfig config;
	CUser *pUser = m_pNetwork->GetUser();

	if (pUser->GetBufferCount() != GetBufferCount())
		config.AddKeyValuePair("Buffer", CString(GetBufferCount()));
	if (pUser->AutoClearChanBuffer() != AutoClearChanBuffer())
		config.AddKeyValuePair("AutoClearChanBuffer", CString(AutoClearChanBuffer()));
	if (IsDetached())
		config.AddKeyValuePair("Detached", "true");
	if (!GetKey().empty())
		config.AddKeyValuePair("Key", GetKey());
	if (!GetDefaultModes().empty())
		config.AddKeyValuePair("Modes", GetDefaultModes());

	return config;
}

void CChan::Clone(CChan& chan) {
	// We assume that m_sName and m_pNetwork are equal
	SetBufferCount(chan.GetBufferCount(), true);
	SetAutoClearChanBuffer(chan.AutoClearChanBuffer());
	SetKey(chan.GetKey());
	SetDefaultModes(chan.GetDefaultModes());

	if (IsDetached() != chan.IsDetached()) {
		// Only send something if it makes sense
		// (= Only detach if client is on the channel
		//    and only attach if we are on the channel)
		if (IsOn()) {
			if (IsDetached()) {
				JoinUser(false, "");
			} else {
				DetachUser();
			}
		}
		SetDetached(chan.IsDetached());
	}
}

void CChan::Cycle() const {
	m_pNetwork->PutIRC("PART " + GetName() + "\r\nJOIN " + GetName() + " " + GetKey());
}

void CChan::JoinUser(bool bForce, const CString& sKey, CClient* pClient) {
	if (!bForce && (!IsOn() || !IsDetached())) {
		m_pNetwork->PutIRC("JOIN " + GetName() + " " + ((sKey.empty()) ? GetKey() : sKey));
		SetDetached(false);
		return;
	}

	m_pNetwork->PutUser(":" + m_pNetwork->GetIRCNick().GetNickMask() + " JOIN :" + GetName(), pClient);

	if (!GetTopic().empty()) {
		m_pNetwork->PutUser(":" + m_pNetwork->GetIRCServer() + " 332 " + m_pNetwork->GetIRCNick().GetNick() + " " + GetName() + " :" + GetTopic(), pClient);
		m_pNetwork->PutUser(":" + m_pNetwork->GetIRCServer() + " 333 " + m_pNetwork->GetIRCNick().GetNick() + " " + GetName() + " " + GetTopicOwner() + " " + CString(GetTopicDate()), pClient);
	}

	CString sPre = ":" + m_pNetwork->GetIRCServer() + " 353 " + m_pNetwork->GetIRCNick().GetNick() + " " + GetModeForNames() + " " + GetName() + " :";
	CString sLine = sPre;
	CString sPerm, sNick;

	vector<CClient*>& vpClients = m_pNetwork->GetClients();
	for (vector<CClient*>::iterator it = vpClients.begin(); it != vpClients.end(); ++it) {
		CClient* pThisClient;
		if (!pClient)
			pThisClient = *it;
		else
			pThisClient = pClient;

		for (map<CString,CNick>::iterator a = m_msNicks.begin(); a != m_msNicks.end(); ++a) {
			if (pThisClient->HasNamesx()) {
				sPerm = a->second.GetPermStr();
			} else {
				char c = a->second.GetPermChar();
				sPerm = "";
				if (c != '\0') {
					sPerm += c;
				}
			}
			if (pThisClient->HasUHNames() && !a->second.GetIdent().empty() && !a->second.GetHost().empty()) {
				sNick = a->first + "!" + a->second.GetIdent() + "@" + a->second.GetHost();
			} else {
				sNick = a->first;
			}

			sLine += sPerm + sNick;

			if (sLine.size() >= 490 || a == (--m_msNicks.end())) {
				m_pNetwork->PutUser(sLine, pThisClient);
				sLine = sPre;
			} else {
				sLine += " ";
			}
		}

		if (pClient) // We only want to do this for one client
			break;
	}

	m_pNetwork->PutUser(":" + m_pNetwork->GetIRCServer() + " 366 " + m_pNetwork->GetIRCNick().GetNick() + " " + GetName() + " :End of /NAMES list.", pClient);
	m_bDetached = false;

	// Send Buffer
	SendBuffer(pClient);
}

void CChan::DetachUser() {
	if (!m_bDetached) {
		m_pNetwork->PutUser(":" + m_pNetwork->GetIRCNick().GetNickMask() + " PART " + GetName());
		m_bDetached = true;
	}
}

void CChan::AttachUser() {
	if (m_bDetached) {
		m_pNetwork->PutUser(":" + m_pNetwork->GetIRCNick().GetNickMask() + " JOIN " + GetName());
		m_bDetached = false;
	}
}

CString CChan::GetModeString() const {
	CString sModes, sArgs;

	for (map<unsigned char, CString>::const_iterator it = m_musModes.begin(); it != m_musModes.end(); ++it) {
		sModes += it->first;
		if (it->second.size()) {
			sArgs += " " + it->second;
		}
	}

	return sModes.empty() ? sModes : CString("+" + sModes + sArgs);
}

CString CChan::GetModeForNames() const {
	CString sMode;

	for (map<unsigned char, CString>::const_iterator it = m_musModes.begin(); it != m_musModes.end(); ++it) {
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
	ModeChange(sModes);
}

void CChan::SetAutoClearChanBuffer(bool b) {
	m_bAutoClearChanBuffer = b;

	if (m_bAutoClearChanBuffer && !IsDetached() && m_pNetwork->IsUserOnline()) {
		ClearBuffer();
	}
}

void CChan::OnWho(const CString& sNick, const CString& sIdent, const CString& sHost) {
	CNick* pNick = FindNick(sNick);

	if (pNick) {
		pNick->SetIdent(sIdent);
		pNick->SetHost(sHost);
	}
}

void CChan::ModeChange(const CString& sModes, const CNick* pOpNick) {
	CString sModeArg = sModes.Token(0);
	CString sArgs = sModes.Token(1, true);
	bool bAdd = true;

	/* Try to find a CNick* from this channel so that pOpNick->HasPerm()
	 * works as expected. */
	if (pOpNick) {
		CNick* OpNick = FindNick(pOpNick->GetNick());
		/* If nothing was found, use the original pOpNick, else use the
		 * CNick* from FindNick() */
		if (OpNick)
			pOpNick = OpNick;
	}

	if (pOpNick) {
		NETWORKMODULECALL(OnRawMode(*pOpNick, *this, sModeArg, sArgs), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
	}

	for (unsigned int a = 0; a < sModeArg.size(); a++) {
		const unsigned char& uMode = sModeArg[a];

		if (uMode == '+') {
			bAdd = true;
		} else if (uMode == '-') {
			bAdd = false;
		} else if (m_pNetwork->GetIRCSock()->IsPermMode(uMode)) {
			CString sArg = GetModeArg(sArgs);
			CNick* pNick = FindNick(sArg);
			if (pNick) {
				unsigned char uPerm = m_pNetwork->GetIRCSock()->GetPermFromMode(uMode);

				if (uPerm) {
					bool bNoChange = (pNick->HasPerm(uPerm) == bAdd);

					if (bAdd) {
						pNick->AddPerm(uPerm);

						if (pNick->NickEquals(m_pNetwork->GetCurNick())) {
							AddPerm(uPerm);
						}
					} else {
						pNick->RemPerm(uPerm);

						if (pNick->NickEquals(m_pNetwork->GetCurNick())) {
							RemPerm(uPerm);
						}
					}

					if (uMode && pOpNick) {
						NETWORKMODULECALL(OnChanPermission(*pOpNick, *pNick, *this, uMode, bAdd, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);

						if (uMode == CChan::M_Op) {
							if (bAdd) {
								NETWORKMODULECALL(OnOp(*pOpNick, *pNick, *this, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
							} else {
								NETWORKMODULECALL(OnDeop(*pOpNick, *pNick, *this, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
							}
						} else if (uMode == CChan::M_Voice) {
							if (bAdd) {
								NETWORKMODULECALL(OnVoice(*pOpNick, *pNick, *this, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
							} else {
								NETWORKMODULECALL(OnDevoice(*pOpNick, *pNick, *this, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
							}
						}
					}
				}
			}
		} else {
			bool bList = false;
			CString sArg;

			switch (m_pNetwork->GetIRCSock()->GetModeType(uMode)) {
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

			if (pOpNick) {
				bool bNoChange;
				if (bList) {
					bNoChange = false;
				} else if (bAdd) {
					bNoChange = HasMode(uMode) && GetModeArg(uMode) == sArg;
				} else {
					bNoChange = !HasMode(uMode);
				}
				NETWORKMODULECALL(OnMode(*pOpNick, *this, uMode, sArg, bAdd, bNoChange), m_pNetwork->GetUser(), m_pNetwork, NULL, NOTHING);
			}

			if (!bList) {
				(bAdd) ? AddMode(uMode, sArg) : RemMode(uMode);
			}
		}
	}
}

CString CChan::GetOptions() const {
	CString sRet;

	if (IsDetached()) {
		sRet += (sRet.empty()) ? "Detached" : ", Detached";
	}

	if (AutoClearChanBuffer()) {
		sRet += (sRet.empty()) ? "AutoClearChanBuffer" : ", AutoClearChanBuffer";
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
	m_musModes[uMode] = sArg;
	return true;
}

bool CChan::RemMode(unsigned char uMode) {
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
	m_msNicks.clear();
}

int CChan::AddNicks(const CString& sNicks) {
	int iRet = 0;
	VCString vsNicks;
	VCString::iterator it;

	sNicks.Split(" ", vsNicks, false);

	for (it = vsNicks.begin(); it != vsNicks.end(); ++it) {
		if (AddNick(*it)) {
			iRet++;
		}
	}

	return iRet;
}

bool CChan::AddNick(const CString& sNick) {
	const char* p = sNick.c_str();
	CString sPrefix, sTmp, sIdent, sHost;

	while (m_pNetwork->GetIRCSock()->IsPermChar(*p)) {
		sPrefix += *p;

		if (!*++p) {
			return false;
		}
	}

	sTmp = p;

	// The UHNames extension gets us nick!ident@host instead of just plain nick
	sIdent = sTmp.Token(1, true, "!");
	sHost  = sIdent.Token(1, true, "@");
	sIdent = sIdent.Token(0, false, "@");
	// Get the nick
	sTmp   = sTmp.Token(0, false, "!");

	CNick tmpNick(sTmp);
	CNick* pNick = FindNick(sTmp);
	if (!pNick) {
		pNick = &tmpNick;
		pNick->SetNetwork(m_pNetwork);
	}

	if (!sIdent.empty())
		pNick->SetIdent(sIdent);
	if (!sHost.empty())
		pNick->SetHost(sHost);

	for (CString::size_type i = 0; i < sPrefix.length(); i++) {
		pNick->AddPerm(sPrefix[i]);
	}

	if (pNick->NickEquals(m_pNetwork->GetCurNick())) {
		for (CString::size_type i = 0; i < sPrefix.length(); i++) {
			AddPerm(sPrefix[i]);
		}
	}

	m_msNicks[pNick->GetNick()] = *pNick;

	return true;
}

map<char, unsigned int> CChan::GetPermCounts() const {
	map<char, unsigned int> mRet;

	map<CString,CNick>::const_iterator it;
	for (it = m_msNicks.begin(); it != m_msNicks.end(); ++it) {
		CString sPerms = it->second.GetPermStr();

		for (unsigned int p = 0; p < sPerms.size(); p++) {
			mRet[sPerms[p]]++;
		}
	}

	return mRet;
}

bool CChan::RemNick(const CString& sNick) {
	map<CString,CNick>::iterator it;
	set<unsigned char>::iterator it2;

	it = m_msNicks.find(sNick);
	if (it == m_msNicks.end()) {
		return false;
	}

	m_msNicks.erase(it);

	return true;
}

bool CChan::ChangeNick(const CString& sOldNick, const CString& sNewNick) {
	map<CString,CNick>::iterator it = m_msNicks.find(sOldNick);

	if (it == m_msNicks.end()) {
		return false;
	}

	// Rename this nick
	it->second.SetNick(sNewNick);

	// Insert a new element into the map then erase the old one, do this to change the key to the new nick
	m_msNicks[sNewNick] = it->second;
	m_msNicks.erase(it);

	return true;
}

const CNick* CChan::FindNick(const CString& sNick) const {
	map<CString,CNick>::const_iterator it = m_msNicks.find(sNick);
	return (it != m_msNicks.end()) ? &it->second : NULL;
}

CNick* CChan::FindNick(const CString& sNick) {
	map<CString,CNick>::iterator it = m_msNicks.find(sNick);
	return (it != m_msNicks.end()) ? &it->second : NULL;
}

void CChan::SendBuffer(CClient* pClient) {
	if (m_pNetwork && m_pNetwork->IsUserAttached()) {
		// in the event that pClient is NULL, need to send this to all clients for the user
		// I'm presuming here that pClient is listed inside vClients thus vClients at this
		// point can't be empty.
		//
		// This loop has to be cycled twice to maintain the existing behavior which is
		// 1. OnChanBufferStarting
		// 2. OnChanBufferPlayLine
		// 3. ClearBuffer() if not keeping the buffer
		// 4. OnChanBufferEnding
		//
		// With the exception of ClearBuffer(), this needs to happen per client, and
		// if pClient is not NULL, the loops break after the first iteration.
		//
		// Rework this if you like ...
		if (!m_Buffer.IsEmpty()) {
			const vector<CClient*> & vClients = m_pNetwork->GetClients();
			for (size_t uClient = 0; uClient < vClients.size(); ++uClient) {
				CClient * pUseClient = (pClient ? pClient : vClients[uClient]);

				bool bSkipStatusMsg = pUseClient->HasServerTime();
				NETWORKMODULECALL(OnChanBufferStarting(*this, *pUseClient), m_pNetwork->GetUser(), m_pNetwork, NULL, &bSkipStatusMsg);

				if (!bSkipStatusMsg) {
					m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + GetName() + " :Buffer Playback...", pUseClient);
				}

				size_t uSize = m_Buffer.Size();
				for (size_t uIdx = 0; uIdx < uSize; uIdx++) {
					CString sLine = m_Buffer.GetLine(uIdx, *pUseClient);
					bool bNotShowThisLine = false;
					NETWORKMODULECALL(OnChanBufferPlayLine(*this, *pUseClient, sLine), m_pNetwork->GetUser(), m_pNetwork, NULL, &bNotShowThisLine);
					if (bNotShowThisLine) continue;
					m_pNetwork->PutUser(sLine, pUseClient);
				}

				bSkipStatusMsg = pUseClient->HasServerTime();
				NETWORKMODULECALL(OnChanBufferEnding(*this, *pUseClient), m_pNetwork->GetUser(), m_pNetwork, NULL, &bSkipStatusMsg);
				if (!bSkipStatusMsg) {
					m_pNetwork->PutUser(":***!znc@znc.in PRIVMSG " + GetName() + " :Playback Complete.", pUseClient);
				}

				if (pClient)
					break;
			}

			if (AutoClearChanBuffer()) {
 				ClearBuffer();
			}
		}
	}
}

void CChan::Enable() {
	ResetJoinTries();
	m_bDisabled = false;
}
