/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef SOCKET_H
#define SOCKET_H

#include "zncconfig.h"
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

/**
 * @class CSocket
 * @brief Base Csock implementation to be used by modules
 *
 * By all means, this class should be used as a base for sockets originating from modules. It handles removing instances of itself
 * from the module as it unloads, and simplifies use in general.
 * - EnableReadLine is default to true in this class
 * - MaxBuffer for readline is set to 10240, in the event this is reached the socket is closed (@see ReachedMaxBuffer)
 */
class CSocket : public CZNCSock {
public:
	/**
	 * @brief ctor
	 * @param pModule the module this sock instance is associated to
	 */
	CSocket(CModule* pModule);
	/**
	 * @brief ctor
	 * @param pModule the module this sock instance is associated to
	 * @param sHostname the hostname being connected to
	 * @param uPort the port being connected to
	 * @param iTimeout the timeout period for this specific sock
	 */
	CSocket(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CSocket();

	using Csock::Connect;
	using Csock::Listen;

	//! This defaults to closing the socket, feel free to override
	virtual void ReachedMaxBuffer();
	virtual void SockError(int iErrno);

	//! This limits the global connections from this IP to defeat DoS attacks, feel free to override. The ACL used is provided by the main interface @see CZNC::AllowConnectionFrom
	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort);

	//! Ease of use Connect, assigns to the manager and is subsequently tracked
	bool Connect(const CString& sHostname, unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 60);
	//! Ease of use Listen, assigned to the manager and is subsequently tracked
	bool Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout = 0);

	// Getters
	CModule* GetModule() const;
	// !Getters
private:
protected:
	CModule*  m_pModule; //!< pointer to the module that this sock instance belongs to
};

#endif /* SOCKET_H */
