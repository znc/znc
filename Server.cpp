#include "main.h"
#include "Server.h"

CServer::CServer(const string& sName, unsigned short uPort, const string& sPass, bool bSSL) {
	m_sName = sName;
	m_uPort = (uPort) ? uPort : 6667;
	m_sPass = sPass;
	m_bSSL = bSSL;
}

CServer::~CServer() {}

const string& CServer::GetName() const { return m_sName; }
unsigned short CServer::GetPort() const { return m_uPort; }
const string& CServer::GetPass() const { return m_sPass; }
bool CServer::IsSSL() const { return m_bSSL; }

