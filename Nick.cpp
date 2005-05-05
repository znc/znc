#include "Nick.h"
#include "Chan.h"
#include "User.h"

CNick::CNick() {
	m_bIsOp = false;
	m_bIsVoice = false;
}

CNick::CNick(const CString& sNick) {
	Parse(sNick);
	m_bIsOp = false;
	m_bIsVoice = false;
}

CNick::~CNick() {}

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
			if (strcasecmp(it->first.c_str(), m_sNick.c_str()) == 0) {
				vRetChans.push_back(pChan);
				continue;
			}
		}
	}

	return vRetChans.size();
}

void CNick::SetNick(const CString& s) { m_sNick = s; }
void CNick::SetIdent(const CString& s) { m_sIdent = s; }
void CNick::SetHost(const CString& s) { m_sHost = s; }
void CNick::SetOp(bool b) { m_bIsOp = b; }
void CNick::SetVoice(bool b) { m_bIsVoice = b; }

bool CNick::IsOp() const { return m_bIsOp; }
bool CNick::IsVoice() const { return m_bIsVoice; }
const CString& CNick::GetNick() const { return m_sNick; }
const CString& CNick::GetIdent() const { return m_sIdent; }
const CString& CNick::GetHost() const { return m_sHost; }
CString CNick::GetNickMask() const { return m_sNick + "!" + m_sIdent + "@" + m_sHost; }

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
