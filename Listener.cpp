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
	CClient *pClient = new CClient(sHost, uPort);
	if (CZNC::Get().AllowConnectionFrom(sHost)) {
		CZNC::Get().GetModules().OnClientConnect(pClient, sHost, uPort);
	} else {
		pClient->RefuseLogin("Too many anonymous connections from your IP");
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
