/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

#include <znc/Nick.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/IRCNetwork.h>

using std::vector;
using std::map;

CNick::CNick()
    : m_sChanPerms(""),
      m_pNetwork(nullptr),
      m_sNick(""),
      m_sIdent(""),
      m_sHost("") {}

CNick::CNick(const CString& sNick) : CNick() { Parse(sNick); }

CNick::~CNick() {}

void CNick::Reset() {
    m_sChanPerms.clear();
    m_pNetwork = nullptr;
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

    m_sNick =
        sNickMask.substr((sNickMask[0] == ':'), uPos - (sNickMask[0] == ':'));
    m_sHost = sNickMask.substr(uPos + 1);

    if ((uPos = m_sHost.find('@')) != CString::npos) {
        m_sIdent = m_sHost.substr(0, uPos);
        m_sHost = m_sHost.substr(uPos + 1);
    }
}

size_t CNick::GetCommonChans(vector<CChan*>& vRetChans,
                             CIRCNetwork* pNetwork) const {
    vRetChans.clear();

    const vector<CChan*>& vChans = pNetwork->GetChans();

    for (CChan* pChan : vChans) {
        const map<CString, CNick>& msNicks = pChan->GetNicks();

        for (const auto& it : msNicks) {
            if (it.first.Equals(m_sNick)) {
                vRetChans.push_back(pChan);
                continue;
            }
        }
    }

    return vRetChans.size();
}

bool CNick::NickEquals(const CString& nickname) const {
    // TODO add proper IRC case mapping here
    // https://tools.ietf.org/html/draft-brocklesby-irc-isupport-03#section-3.1
    return m_sNick.Equals(nickname);
}

void CNick::SetNetwork(CIRCNetwork* pNetwork) { m_pNetwork = pNetwork; }
void CNick::SetNick(const CString& s) { m_sNick = s; }
void CNick::SetIdent(const CString& s) { m_sIdent = s; }
void CNick::SetHost(const CString& s) { m_sHost = s; }

bool CNick::HasPerm(char cPerm) const {
    return (cPerm && m_sChanPerms.find(cPerm) != CString::npos);
}

bool CNick::AddPerm(char cPerm) {
    if (!cPerm || HasPerm(cPerm)) {
        return false;
    }

    m_sChanPerms.append(1, cPerm);

    return true;
}

bool CNick::RemPerm(char cPerm) {
    CString::size_type uPos = m_sChanPerms.find(cPerm);
    if (uPos == CString::npos) {
        return false;
    }

    m_sChanPerms.erase(uPos, 1);

    return true;
}

char CNick::GetPermChar() const {
    CIRCSock* pIRCSock = (!m_pNetwork) ? nullptr : m_pNetwork->GetIRCSock();
    const CString sChanPerms = (!pIRCSock) ? "@+" : pIRCSock->GetPerms();

    for (unsigned int a = 0; a < sChanPerms.size(); a++) {
        const char& c = sChanPerms[a];
        if (HasPerm(c)) {
            return c;
        }
    }

    return '\0';
}

CString CNick::GetPermStr() const {
    CIRCSock* pIRCSock = (!m_pNetwork) ? nullptr : m_pNetwork->GetIRCSock();
    const CString sChanPerms = (!pIRCSock) ? "@+" : pIRCSock->GetPerms();
    CString sRet;

    for (unsigned int a = 0; a < sChanPerms.size(); a++) {
        const char& c = sChanPerms[a];

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
    CString sRet = m_sNick;

    if (!m_sHost.empty()) {
        if (!m_sIdent.empty()) sRet += "!" + m_sIdent;
        sRet += "@" + m_sHost;
    }

    return sRet;
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

void CNick::Clone(const CNick& SourceNick) {
    SetNick(SourceNick.GetNick());
    SetIdent(SourceNick.GetIdent());
    SetHost(SourceNick.GetHost());

    m_sChanPerms = SourceNick.m_sChanPerms;
    m_pNetwork = SourceNick.m_pNetwork;
}
