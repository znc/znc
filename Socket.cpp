/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Socket.h"
#include "Modules.h"
#include "User.h"
#include "znc.h"

unsigned int CSockManager::GetAnonConnectionCount(const CString &sIP) const {
	const_iterator it;
	unsigned int ret = 0;

	for (it = begin(); it != end(); ++it) {
		CZNCSock *pSock = *it;
		// Logged in CClients have "USR::<username>" as their sockname
		if (pSock->GetType() == Csock::INBOUND && pSock->GetRemoteIP() == sIP
				&& pSock->GetSockName().Left(5) != "USR::") {
			ret++;
		}
	}

	DEBUG("There are [" << ret << "] clients from [" << sIP << "]");

	return ret;
}

CS_STRING CZNCSock::ConvertAddress(void *addr, bool ipv6) {
	CString sRet = Csock::ConvertAddress(addr, ipv6);
	sRet.TrimPrefix("::ffff:");
	return sRet;
}

/////////////////// CSocket ///////////////////
CSocket::CSocket(CModule* pModule) : CZNCSock() {
	m_pModule = pModule;
	if (m_pModule) m_pModule->AddSocket(this);
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CSocket::CSocket(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout) : CZNCSock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	if (m_pModule) m_pModule->AddSocket(this);
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CSocket::~CSocket() {
	CUser *pUser = NULL;

	// CWebSock could cause us to have a NULL pointer here
	if (m_pModule) {
		pUser = m_pModule->GetUser();
		m_pModule->UnlinkSocket(this);
	}

	if (pUser && m_pModule && !m_pModule->IsGlobal()) {
		pUser->AddBytesWritten(GetBytesWritten());
		pUser->AddBytesRead(GetBytesRead());
	} else {
		CZNC::Get().AddBytesWritten(GetBytesWritten());
		CZNC::Get().AddBytesRead(GetBytesRead());
	}
}

void CSocket::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	if (m_pModule) m_pModule->PutModule("Some socket reached its max buffer limit and was closed!");
	Close();
}

void CSocket::SockError(int iErrno) {
	DEBUG(GetSockName() << " == SockError(" << strerror(iErrno) << ")");
	if (iErrno == EMFILE) {
		// We have too many open fds, this can cause a busy loop.
		Close();
	}
}

bool CSocket::ConnectionFrom(const CString& sHost, unsigned short uPort) {
	return CZNC::Get().AllowConnectionFrom(sHost);
}

bool CSocket::Connect(const CString& sHostname, unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	if (!m_pModule) {
		DEBUG("ERROR: CSocket::Connect called on instance without m_pModule handle!");
		return false;
	}

	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::C::" + m_pModule->GetModName();
	CString sBindHost;

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
		sBindHost = m_pModule->GetUser()->GetBindHost();
	}

	// Don't overwrite the socket name if one is already set
	if (!GetSockName().empty()) {
		sSockName = GetSockName();
	}

	return m_pModule->GetManager()->Connect(sHostname, uPort, sSockName, uTimeout, bSSL, sBindHost, this);
}

bool CSocket::Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	if (!m_pModule) {
		DEBUG("ERROR: CSocket::Listen called on instance without m_pModule handle!");
		return false;
	}

	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::L::" + m_pModule->GetModName();

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
	}
	// Don't overwrite the socket name if one is already set
	if (!GetSockName().empty()) {
		sSockName = GetSockName();
	}

	return m_pModule->GetManager()->ListenAll(uPort, sSockName, bSSL, SOMAXCONN, this);
}

CModule* CSocket::GetModule() const { return m_pModule; }
/////////////////// !CSocket ///////////////////
