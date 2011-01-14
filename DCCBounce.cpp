/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "DCCBounce.h"
#include "User.h"
#include "znc.h"

// If we buffer more than this in memory, we will throttle the receiving side
const unsigned int CDCCBounce::m_uiMaxDCCBuffer = 10 * 1024;
// If less than this is in the buffer, the receiving side continues
const unsigned int CDCCBounce::m_uiMinDCCBuffer = 2 * 1024;

CDCCBounce::CDCCBounce(CUser* pUser, unsigned long uLongIP, unsigned short uPort,
		const CString& sFileName, const CString& sRemoteNick,
		const CString& sRemoteIP, bool bIsChat) : CZNCSock() {
	m_uRemotePort = uPort;
	m_sConnectIP = CUtils::GetIP(uLongIP);
	m_sRemoteIP = sRemoteIP;
	m_sFileName = sFileName;
	m_sRemoteNick = sRemoteNick;
	m_pUser = pUser;
	m_bIsChat = bIsChat;
	m_sLocalIP = pUser->GetLocalDCCIP();
	m_pPeer = NULL;
	m_bIsRemote = false;

	if (bIsChat) {
		EnableReadLine();
	}

	m_pUser->AddDCCBounce(this);
}

CDCCBounce::CDCCBounce(const CString& sHostname, unsigned short uPort, CUser* pUser,
		const CString& sRemoteNick, const CString& sRemoteIP, const CString& sFileName,
		int iTimeout, bool bIsChat) : CZNCSock(sHostname, uPort, iTimeout) {
	m_uRemotePort = 0;
	m_bIsChat = bIsChat;
	m_pUser = pUser;
	m_pPeer = NULL;
	m_sRemoteNick = sRemoteNick;
	m_sFileName = sFileName;
	m_sRemoteIP = sRemoteIP;
	m_bIsRemote = false;

	SetMaxBufferThreshold(10240);
	if (bIsChat) {
		EnableReadLine();
	}

	m_pUser->AddDCCBounce(this);
}

CDCCBounce::~CDCCBounce() {
	if (m_pPeer) {
		m_pPeer->Shutdown();
		m_pPeer = NULL;
	}

	m_pUser->AddBytesRead(GetBytesRead());
	m_pUser->AddBytesWritten(GetBytesWritten());

	m_pUser->DelDCCBounce(this);
}

void CDCCBounce::ReadLine(const CString& sData) {
	CString sLine = sData.TrimRight_n("\r\n");

	DEBUG(GetSockName() << " <- [" << sLine << "]");

	PutPeer(sLine);
}

void CDCCBounce::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");

	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Too long line received");
	Close();
}

void CDCCBounce::ReadData(const char* data, size_t len) {
	if (m_pPeer) {
		m_pPeer->Write(data, len);

		size_t BufLen = m_pPeer->GetInternalWriteBuffer().length();

		if (BufLen >= m_uiMaxDCCBuffer) {
			DEBUG(GetSockName() << " The send buffer is over the "
					"limit (" << BufLen <<"), throttling");
			PauseRead();
		}
	}
}

void CDCCBounce::ReadPaused() {
	if (!m_pPeer || m_pPeer->GetInternalWriteBuffer().length() <= m_uiMinDCCBuffer)
		UnPauseRead();
}

void CDCCBounce::Timeout() {
	DEBUG(GetSockName() << " == Timeout()");
	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		CString sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = " to [" + sHost + " " + CString(Csock::GetPort()) + "]";
		} else {
			sHost = ".";
		}

		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout while connecting" + sHost);
	} else {
		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout waiting for incoming connection [" + Csock::GetLocalIP() + ":" + CString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::ConnectionRefused() {
	DEBUG(GetSockName() << " == ConnectionRefused()");

	CString sType = (m_bIsChat) ? "Chat" : "Xfer";
	CString sHost = Csock::GetHostName();
	if (!sHost.empty()) {
		sHost = " to [" + sHost + " " + CString(Csock::GetPort()) + "]";
	} else {
		sHost = ".";
	}

	m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Connection Refused while connecting" + sHost);
}

void CDCCBounce::SockError(int iErrno) {
	DEBUG(GetSockName() << " == SockError(" << iErrno << ")");
	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		CString sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = "[" + sHost + " " + CString(Csock::GetPort()) + "]";
		}

		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + CString(strerror(iErrno)) + "]" + sHost);
	} else {
		m_pUser->PutStatus("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + CString(strerror(iErrno)) + "] [" + Csock::GetLocalIP() + ":" + CString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::Connected() {
	DEBUG(GetSockName() << " == Connected()");
	SetTimeout(0);
}

void CDCCBounce::Disconnected() {
	DEBUG(GetSockName() << " == Disconnected()");
}

void CDCCBounce::Shutdown() {
	m_pPeer = NULL;
	DEBUG(GetSockName() << " == Close(); because my peer told me to");
	Close();
}

Csock* CDCCBounce::GetSockObj(const CString& sHost, unsigned short uPort) {
	Close();

	if (m_sRemoteIP.empty()) {
		m_sRemoteIP = sHost;
	}

	CDCCBounce* pSock = new CDCCBounce(sHost, uPort, m_pUser, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	CDCCBounce* pRemoteSock = new CDCCBounce(sHost, uPort, m_pUser, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	pSock->SetPeer(pRemoteSock);
	pRemoteSock->SetPeer(pSock);
	pRemoteSock->SetRemote(true);
	pSock->SetRemote(false);

	if (!CZNC::Get().GetManager().Connect(m_sConnectIP, m_uRemotePort, "DCC::" + CString((m_bIsChat) ? "Chat" : "XFER") + "::Remote::" + m_sRemoteNick, 60, false, m_sLocalIP, pRemoteSock)) {
		pRemoteSock->Close();
	}

	pSock->SetSockName(GetSockName());
	pSock->SetTimeout(0);
	return pSock;
}

void CDCCBounce::PutServ(const CString& sLine) {
	DEBUG(GetSockName() << " -> [" << sLine << "]");
	Write(sLine + "\r\n");
}

void CDCCBounce::PutPeer(const CString& sLine) {
	if (m_pPeer) {
		m_pPeer->PutServ(sLine);
	} else {
		PutServ("*** Not connected yet ***");
	}
}

unsigned short CDCCBounce::DCCRequest(const CString& sNick, unsigned long uLongIP, unsigned short uPort, const CString& sFileName, bool bIsChat, CUser* pUser, const CString& sRemoteIP) {
	CDCCBounce* pDCCBounce = new CDCCBounce(pUser, uLongIP, uPort, sFileName, sNick, sRemoteIP, bIsChat);
	unsigned short uListenPort = CZNC::Get().GetManager().ListenRand("DCC::" + CString((bIsChat) ? "Chat" : "Xfer") + "::Local::" + sNick,
			pUser->GetLocalDCCIP(), false, SOMAXCONN, pDCCBounce, 120);

	return uListenPort;
}

