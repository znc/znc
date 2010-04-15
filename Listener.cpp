/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Listener.h"

bool CListener::Listen() {
	if (!m_uPort || m_pListener) {
		return false;
	}

	m_pListener = new CRealListener(this);

	bool bSSL = false;
#ifdef HAVE_LIBSSL
	if (IsSSL()) {
		bSSL = true;
		m_pListener->SetPemLocation(CZNC::Get().GetPemLocation());
	}
#endif

	return CZNC::Get().GetManager().ListenHost(m_uPort, "_LISTENER", m_sBindHost, bSSL, SOMAXCONN,
			m_pListener, 0, m_eAddr);
}

CRealListener::~CRealListener() {
	m_pParent->SetRealListener(NULL);
}

bool CRealListener::ConnectionFrom(const CString& sHost, unsigned short uPort) {
	bool bHostAllowed = CZNC::Get().IsHostAllowed(sHost);
	DEBUG(GetSockName() << " == ConnectionFrom(" << sHost << ", " << uPort << ") [" << (bHostAllowed ? "Allowed" : "Not allowed") << "]");
	return bHostAllowed;
}

Csock* CRealListener::GetSockObj(const CString& sHost, unsigned short uPort) {
	CIncomingConnection *pClient = new CIncomingConnection(sHost, uPort);
	if (CZNC::Get().AllowConnectionFrom(sHost)) {
		CZNC::Get().GetModules().OnClientConnect(pClient, sHost, uPort);
	} else {
		pClient->Write(":irc.znc.in 464 unknown-nick :Too many anonymous connections from your IP\r\n");
		pClient->Close(Csock::CLT_AFTERWRITE);
		CZNC::Get().GetModules().OnFailedLogin("", sHost);
	}
	return pClient;
}

void CRealListener::SockError(int iErrno) {
	DEBUG(GetSockName() << " == SockError(" << strerror(iErrno) << ")");
	if (iErrno == EMFILE) {
		// We have too many open fds, let's close this listening port to be able to continue
		// to work, next rehash will (try to) reopen it.
		Close();
	}
}

CIncomingConnection::CIncomingConnection(const CString& sHostname, unsigned short uPort) : CZNCSock(sHostname, uPort) {
	// The socket will time out in 120 secs, no matter what.
	// This has to be fixed up later, if desired.
	SetTimeout(120, 0);

	EnableReadLine();
}

void CIncomingConnection::ReadLine(const CString& sLine) {
	bool bIsHTTP = (sLine.WildCmp("GET * HTTP/1.?\r\n") || sLine.WildCmp("POST * HTTP/1.?\r\n"));
	Csock *pSock = NULL;

	if (!bIsHTTP) {
		// Let's assume it's an IRC connection

		pSock = new CClient();
		CZNC::Get().GetManager().SwapSockByAddr(pSock, this);

		// And don't forget to give it some sane name / timeout
		pSock->SetSockName("USR::???");
	} else {
		// This is a HTTP request, let the webmods handle it

		CModule* pMod = new CModule(NULL, "<webmod>", "");
		pMod->SetFake(true);

		pSock = new CWebSock(pMod);
		CZNC::Get().GetManager().SwapSockByAddr(pSock, this);

		// And don't forget to give it some sane name / timeout
		pSock->SetSockName("WebMod::Client");
	}

	if (pSock) {
		// TODO can we somehow get rid of this?
		pSock->ReadLine(sLine);
		pSock->PushBuff("", 0, true);
	}
}
