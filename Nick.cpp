#include "Nick.h"

CNick::CNick() {
	m_bIsOp = false;
	m_bIsVoice = false;
}

CNick::CNick(const string& sNick) {
	Parse(sNick);
	m_bIsOp = false;
	m_bIsVoice = false;
}

CNick::~CNick() {}

void CNick::Parse(const string& sNickMask) {
	if (sNickMask.empty()) {
		return;
	}

	unsigned int uPos = sNickMask.find('!');

	if (uPos == string::npos) {
		m_sNick = sNickMask.substr((sNickMask[0] == ':'));
		return;
	}

	m_sNick = sNickMask.substr((sNickMask[0] == ':'), uPos);
	m_sHost = sNickMask.substr(uPos +1);

	if ((uPos = m_sHost.find('@')) != string::npos) {
		m_sIdent = m_sHost.substr(0, uPos);
		m_sHost = m_sHost.substr(uPos +1);
	}
}

void CNick::SetNick(const string& s) { m_sNick = s; }
void CNick::SetIdent(const string& s) { m_sIdent = s; }
void CNick::SetHost(const string& s) { m_sHost = s; }
void CNick::SetOp(bool b) { m_bIsOp = b; }
void CNick::SetVoice(bool b) { m_bIsVoice = b; }

bool CNick::IsOp() const { return m_bIsOp; }
bool CNick::IsVoice() const { return m_bIsVoice; }
const string& CNick::GetNick() const { return m_sNick; }
const string& CNick::GetIdent() const { return m_sIdent; }
const string& CNick::GetHost() const { return m_sHost; }
string CNick::GetNickMask() const { return m_sNick + "!" + m_sIdent + "@" + m_sHost; }

string CNick::GetHostMask() const {
	string sRet = m_sNick;

	if (!m_sIdent.empty()) {
		sRet += "!" + m_sIdent;
	}

	if (!m_sHost.empty()) {
		sRet += "@" + m_sHost;
	}

	return (sRet);
}
