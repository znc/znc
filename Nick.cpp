/*
 * Copyright (C) 2004-2007  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "Nick.h"
#include "User.h"
#include "IRCSock.h"

CNick::CNick() {
	Reset();
}

CNick::CNick(const CString& sNick) {
	Reset();
	Parse(sNick);
}

CNick::~CNick() {}

void CNick::Reset() {
	m_cPerm = '\0';
}

CString CNick::Concat(const CString& sNick, const CString& sSuffix, unsigned int uMaxNickLen) {
	if (sSuffix.length() >= uMaxNickLen) {
		return sSuffix.Left(uMaxNickLen);
	}

	return sNick.Left(uMaxNickLen - sSuffix.length()) + sSuffix;
}

void CNick::Parse(const CString& sNickMask) {
	if (sNickMask.empty()) {
		return;
	}

	CString::size_type uPos = sNickMask.find('!');

	if (uPos == CString::npos) {
		m_sNick = sNickMask.substr((sNickMask[0] == ':'));
		return;
	}

	m_sNick = sNickMask.substr((sNickMask[0] == ':'), uPos);
	m_sHost = sNickMask.substr(uPos +1);

	if ((uPos = m_sHost.find('@')) != CString::npos) {
		m_sIdent = m_sHost.substr(0, uPos);
		m_sHost = m_sHost.substr(uPos +1);
	}
}

unsigned int CNick::GetCommonChans(vector<CChan*>& vRetChans, CUser* pUser) const {
	vRetChans.clear();

	const vector<CChan*>& vChans = pUser->GetChans();

	for (unsigned int a = 0; a < vChans.size(); a++) {
		CChan* pChan = vChans[a];
		const map<CString,CNick*>& msNicks = pChan->GetNicks();

		for (map<CString,CNick*>::const_iterator it = msNicks.begin(); it != msNicks.end(); it++) {
			if (it->first.CaseCmp(m_sNick) == 0) {
				vRetChans.push_back(pChan);
				continue;
			}
		}
	}

	return vRetChans.size();
}

void CNick::SetUser(CUser* pUser) { m_pUser = pUser; }
void CNick::SetPermChar(char c) { m_cPerm = c; }
void CNick::SetNick(const CString& s) { m_sNick = s; }
void CNick::SetIdent(const CString& s) { m_sIdent = s; }
void CNick::SetHost(const CString& s) { m_sHost = s; }

bool CNick::HasPerm(unsigned char uPerm) const {
	return (uPerm && m_suChanPerms.find(uPerm) != m_suChanPerms.end());
}

bool CNick::AddPerm(unsigned char uPerm) {
	if (!uPerm || HasPerm(uPerm)) {
		return false;
	}

	m_suChanPerms.insert(uPerm);
	UpdatePermChar();

	return true;
}

bool CNick::RemPerm(unsigned char uPerm) {
	if (!HasPerm(uPerm)) {
		return false;
	}

	m_suChanPerms.erase(uPerm);
	UpdatePermChar();

	return true;
}

void CNick::UpdatePermChar() {
	CIRCSock* pIRCSock = (!m_pUser) ? NULL : m_pUser->GetIRCSock();
	const CString& sChanPerms = (!pIRCSock) ? "@+" : pIRCSock->GetPerms();
	m_cPerm = 0;

	for (unsigned int a = 0; a < sChanPerms.size(); a++) {
		const unsigned char& c = sChanPerms[a];
		if (HasPerm(c)) {
			m_cPerm = c;
			break;
		}
	}
}

const set<unsigned char>& CNick::GetChanPerms() const { return m_suChanPerms; }
const unsigned char CNick::GetPermChar() const { return m_cPerm; }
CString CNick::GetPermStr() const {
	CIRCSock* pIRCSock = (!m_pUser) ? NULL : m_pUser->GetIRCSock();
	const CString& sChanPerms = (!pIRCSock) ? "@+" : pIRCSock->GetPerms();
	CString sRet;

   	for (unsigned int a = 0; a < sChanPerms.size(); a++) {
		const unsigned char& c = sChanPerms[a];

		if (HasPerm(c)) {
			sRet += c;
		}
	}

	return sRet;
}
const CString& CNick::GetNick() const { return m_sNick; }
const CString& CNick::GetIdent() const { return m_sIdent; }
const CString& CNick::GetHost() const { return m_sHost; }
CString CNick::GetNickMask() const {
	if (m_sNick.find('.') != CString::npos) {
		return m_sNick;
	}

	return (m_sNick + "!" + m_sIdent + "@" + m_sHost);
}

CString CNick::GetHostMask() const {
	CString sRet = m_sNick;

	if (!m_sIdent.empty()) {
		sRet += "!" + m_sIdent;
	}

	if (!m_sHost.empty()) {
		sRet += "@" + m_sHost;
	}

	return (sRet);
}
