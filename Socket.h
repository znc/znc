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

class CSockManager : public TSocketManager<Csock> {
public:
	CSockManager() : TSocketManager<Csock>() {}
	virtual ~CSockManager() {}

	bool ListenHost(u_short iPort, const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
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

	bool ListenAll(u_short iPort, const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return ListenHost(iPort, sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, bIsIPv6);
	}

	u_short ListenRand(const CString& sSockName, const CString& sBindHost, bool bSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
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

	u_short ListenAllRand(const CString& sSockName, bool bSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return(ListenRand(sSockName, "", bSSL, iMaxConns, pcSock, iTimeout, bIsIPv6));
	}

	bool Connect(const CString& sHostname, u_short iPort , const CString& sSockName, int iTimeout = 60, bool bSSL = false, const CString& sBindHost = "", Csock *pcSock = NULL) {
		CSConnection C(sHostname, iPort, iTimeout);

		C.SetSockName(sSockName);
		C.SetIsSSL(bSSL);
		C.SetBindHost(sBindHost);

		return TSocketManager<Csock>::Connect(C, pcSock);
	}
private:
protected:
};

#endif /* SOCKET_H */
