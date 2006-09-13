//! @author prozac@rottenboy.com

#include "main.h"
#include "Server.h"

CServer::CServer(const CString& sName, unsigned short uPort, const CString& sPass, bool bSSL, bool bIPV6) {
	m_sName = sName;
	m_uPort = (uPort) ? uPort : 6667;
	m_sPass = sPass;
	m_bSSL = bSSL;
	m_bIPV6 = bIPV6;
}

CServer::~CServer() {}

bool CServer::IsValidHostName(const CString& sHostName) {
	const char* p = sHostName.c_str();

	if (sHostName.empty()) {
		return false;
	}

	while (*p) {
		if (*p++ == ' ') {
			return false;
		}
	}

	return true;
}

const CString& CServer::GetName() const { return m_sName; }
unsigned short CServer::GetPort() const { return m_uPort; }
const CString& CServer::GetPass() const { return m_sPass; }
bool CServer::IsSSL() const { return m_bSSL; }
bool CServer::IsIPV6() const { return m_bIPV6; }

CString CServer::GetString() const {
	return m_sName + " " + CString(m_bSSL ? "+" : "") + CString(m_uPort) + CString(m_sPass.empty() ? CString("") : " " + m_sPass);
}
