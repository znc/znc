#include "DCCBounce.h"

void CDCCBounce::ReadLine(const string& sData) {
	string sLine = sData;

	while ((CUtils::Right(sLine, 1) == "\r") || (CUtils::Right(sLine, 1) == "\n")) {
		CUtils::RightChomp(sLine);
	}

	DEBUG_ONLY(cout << GetSockName() << " <- [" << sLine << "]" << endl);

	PutPeer(sLine);
}

void CDCCBounce::ReadData(const char* data, int len) {
	if (m_pPeer) {
		m_pPeer->Write(data, len);
	}
}

void CDCCBounce::Timeout() {
	DEBUG_ONLY(cout << GetSockName() << " == Timeout()" << endl);
	string sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		string sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = " to [" + sHost + ":" + CUtils::ToString(Csock::GetPort()) + "]";
		} else {
			sHost = ".";
		}

		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout while connecting" + sHost);
	} else {
		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout waiting for incoming connection [" + Csock::GetLocalIP() + ":" + CUtils::ToString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);

	string sType = (m_bIsChat) ? "Chat" : "Xfer";
	string sHost = Csock::GetHostName();
	if (!sHost.empty()) {
		sHost = " to [" + sHost + ":" + CUtils::ToString(Csock::GetPort()) + "]";
	} else {
		sHost = ".";
	}

	m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Connection Refused while connecting" + sHost);
}

void CDCCBounce::SockError(int iErrno) {
	DEBUG_ONLY(cout << GetSockName() << " == SockError(" << iErrno << ")" << endl);
	string sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		string sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = "[" + sHost + ":" + CUtils::ToString(Csock::GetPort()) + "]";
		}

		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + string(strerror(iErrno)) + "]" + sHost);
	} else {
		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + string(strerror(iErrno)) + "] [" + Csock::GetLocalIP() + ":" + CUtils::ToString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected()" << endl);
	SetTimeout(0);
}

void CDCCBounce::Disconnected() {
	DEBUG_ONLY(cout << GetSockName() << " == Disconnected()" << endl);
}

void CDCCBounce::Shutdown() {
	m_pPeer = NULL;
	DEBUG_ONLY(cout << GetSockName() << " == Close(); because my peer told me to" << endl);
	Close();
}

Csock* CDCCBounce::GetSockObj(const string& sHost, int iPort) {
	Close();

	if (!m_pManager) {
		return NULL;
	}

	if (m_sRemoteIP.empty()) {
		m_sRemoteIP = sHost;
	}

	CDCCBounce* pSock = new CDCCBounce(sHost, iPort, m_pUser, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	CDCCBounce* pRemoteSock = new CDCCBounce(sHost, iPort, m_pUser, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	pSock->SetPeer(pRemoteSock);
	pRemoteSock->SetPeer(pSock);
	pRemoteSock->SetRemote(true);
	pSock->SetRemote(false);

	if (!m_pManager->Connect(m_sConnectIP, m_uRemotePort, "DCC::" + string((m_bIsChat) ? "Chat" : "XFER") + "::Remote::" + m_sRemoteNick, 60, false, m_sLocalIP, pRemoteSock)) {
		pRemoteSock->Close();
	}

	pSock->SetSockName(GetSockName());
	pSock->SetTimeout(0);
	return pSock;
}

void CDCCBounce::PutServ(const string& sLine) {
	DEBUG_ONLY(cout << GetSockName() << " -> [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CDCCBounce::PutPeer(const string& sLine) {
	if (m_pPeer) {
		m_pPeer->PutServ(sLine);
	} else {
		PutServ("*** Not connected yet ***");
	}
}

unsigned short CDCCBounce::DCCRequest(const string& sNick, unsigned long uLongIP, unsigned short uPort, const string& sFileName, bool bIsChat, CUser* pUser, const string& sLocalIP, const string& sRemoteIP) {
	CDCCBounce* pDCCBounce = new CDCCBounce(pUser, uLongIP, uPort, sFileName, sNick, sRemoteIP, sLocalIP, bIsChat);
	unsigned short uListenPort = pUser->GetManager()->ListenAllRand("DCC::" + string((bIsChat) ? "Chat" : "Xfer") + "::Local::" + sNick, false, SOMAXCONN, pDCCBounce, 120);

	return uListenPort;
}

