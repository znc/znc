/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef SOCKET_H
#define SOCKET_H

#include "Csocket.h"

#ifdef HAVE_ARES
struct DNSLookup;
#endif

class CZNCSock : public Csock {
public:
	CZNCSock(int timeout = 60) : Csock(timeout) {
#ifdef HAVE_ARES
		m_dns_lookup = NULL;
#endif
	}
	CZNCSock(const CString& sHost, u_short port, int timeout = 60) : Csock(sHost, port, timeout) {
#ifdef HAVE_ARES
		m_dns_lookup = NULL;
#endif
	}

	~CZNCSock();

#ifdef HAVE_ARES
	static void ares_callback(void *lookup, int status, int timeout, struct hostent *h);
	virtual int GetAddrInfo(const CS_STRING & sHostname, CSSockAddr & csSockAddr);

private:
	struct DNSLookup	*m_dns_lookup;
#endif
};

class CSockManager : public TSocketManager<CZNCSock> {
public:
	CSockManager();
	virtual ~CSockManager();

	bool ListenHost(u_short iPort, const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		CSListener L(iPort, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(bSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		if (bIsIPv6) {
			L.SetAFRequire(CSSockAddr::RAF_INET6);
		}
#endif

		return Listen(L, pcSock);
	}

	bool ListenAll(u_short iPort, const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return ListenHost(iPort, sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, bIsIPv6);
	}

	u_short ListenRand(const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		unsigned short uPort = 0;
		CSListener L(0, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(bSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		if (bIsIPv6) {
			L.SetAFRequire(CSSockAddr::RAF_INET6);
		}
#endif

		Listen(L, pcSock, &uPort);

		return uPort;
	}

	u_short ListenAllRand(const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, CZNCSock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return(ListenRand(sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, bIsIPv6));
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
#ifdef HAVE_ARES
	int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

private:
	using TSocketManager<CZNCSock>::Select;
#endif /* HAVE_ARES */
};

#endif /* SOCKET_H */
