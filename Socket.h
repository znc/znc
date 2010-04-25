/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef SOCKET_H
#define SOCKET_H

#include "Csocket.h"

class CModule;

class CZNCSock : public Csock {
public:
	CZNCSock(int timeout = 60) : Csock(timeout) {}
	CZNCSock(const CString& sHost, u_short port, int timeout = 60) : Csock(sHost, port, timeout) {}
	~CZNCSock() {}

	virtual CS_STRING ConvertAddress(void *addr, bool ipv6 = false);
};

enum EAddrType {
	ADDR_IPV4ONLY,
	ADDR_IPV6ONLY,
	ADDR_ALL
};

class CSockManager : public TSocketManager<CZNCSock> {
public:
	CSockManager() {}
	virtual ~CSockManager() {}

	bool ListenHost(u_short iPort, const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, EAddrType eAddr = ADDR_ALL) {
		CSListener L(iPort, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(bSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		switch (eAddr) {
			case ADDR_IPV4ONLY:
				L.SetAFRequire(CSSockAddr::RAF_INET);
				break;
			case ADDR_IPV6ONLY:
				L.SetAFRequire(CSSockAddr::RAF_INET6);
				break;
			case ADDR_ALL:
				L.SetAFRequire(CSSockAddr::RAF_ANY);
				break;
		}
#endif

		return Listen(L, pcSock);
	}

	bool ListenAll(u_short iPort, const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, EAddrType eAddr = ADDR_ALL) {
		return ListenHost(iPort, sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, eAddr);
	}

	u_short ListenRand(const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, EAddrType eAddr = ADDR_ALL) {
		unsigned short uPort = 0;
		CSListener L(0, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(bSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		switch (eAddr) {
			case ADDR_IPV4ONLY:
				L.SetAFRequire(CSSockAddr::RAF_INET);
				break;
			case ADDR_IPV6ONLY:
				L.SetAFRequire(CSSockAddr::RAF_INET6);
				break;
			case ADDR_ALL:
				L.SetAFRequire(CSSockAddr::RAF_ANY);
				break;
		}
#endif

		Listen(L, pcSock, &uPort);

		return uPort;
	}

	u_short ListenAllRand(const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, EAddrType eAddr = ADDR_ALL) {
		return(ListenRand(sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, eAddr));
	}

	bool Connect(const CString& sHostname, u_short iPort , const CString& sSockName, int iTimeout = 60, bool bSSL = false, const CString& sBindHost = "", CZNCSock *pcSock = NULL) {
		CSConnection C(sHostname, iPort, iTimeout);

		C.SetSockName(sSockName);
		C.SetIsSSL(bSSL);
		C.SetBindHost(sBindHost);

		return TSocketManager<CZNCSock>::Connect(C, pcSock);
	}

	unsigned int GetAnonConnectionCount(const CString &sIP) const;
private:
protected:
};

class CSocket : public CZNCSock {
public:
	CSocket(CModule* pModule);
	CSocket(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CSocket();

	using Csock::Connect;
	using Csock::Listen;

	// This defaults to closing the socket, feel free to override
	virtual void ReachedMaxBuffer();
	virtual void SockError(int iErrno);
	// This limits the global connections from this IP to defeat DoS
	// attacks, feel free to override
	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort);

	bool Connect(const CString& sHostname, unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 60);
	bool Listen(unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 0);
	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");

	// Setters
	void SetModule(CModule* p);
	// !Setters

	// Getters
	CModule* GetModule() const;
	// !Getters
private:
protected:
	CModule*	m_pModule;
};

#endif /* SOCKET_H */
