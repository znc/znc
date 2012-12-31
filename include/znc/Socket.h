/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef SOCKET_H
#define SOCKET_H

#include <znc/zncconfig.h>
#include <znc/Csocket.h>

class CModule;

class CZNCSock : public Csock {
public:
	CZNCSock(int timeout = 60) : Csock(timeout) {}
	CZNCSock(const CString& sHost, u_short port, int timeout = 60) : Csock(sHost, port, timeout) {}
	~CZNCSock() {}

	virtual int ConvertAddress(const struct sockaddr_storage * pAddr, socklen_t iAddrLen, CS_STRING & sIP, u_short * piPort);
};

enum EAddrType {
	ADDR_IPV4ONLY,
	ADDR_IPV6ONLY,
	ADDR_ALL
};

class CSockManager : public TSocketManager<CZNCSock> {
public:
	CSockManager();
	virtual ~CSockManager();

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

	void Connect(const CString& sHostname, u_short iPort, const CString& sSockName, int iTimeout = 60, bool bSSL = false, const CString& sBindHost = "", CZNCSock *pcSock = NULL);

	unsigned int GetAnonConnectionCount(const CString &sIP) const;
private:
	void FinishConnect(const CString& sHostname, u_short iPort, const CString& sSockName, int iTimeout, bool bSSL, const CString& sBindHost, CZNCSock *pcSock);

#ifdef HAVE_THREADED_DNS
	int              m_iTDNSpipe[2];

	class CTDNSMonitorFD;
	friend class CTDNSMonitorFD;
	struct TDNSTask {
		CString   sHostname;
		u_short   iPort;
		CString   sSockName;
		int       iTimeout;
		bool      bSSL;
		CString   sBindhost;
		CZNCSock* pcSock;

		bool      bDoneTarget;
		bool      bDoneBind;
		addrinfo* aiTarget;
		addrinfo* aiBind;
	};
	struct TDNSArg {
		CString          sHostname;
		TDNSTask*        task;
		int              fd;
		bool             bBind;

		int              iRes;
		addrinfo*        aiResult;
	};
	struct TDNSStatus {
		/* mutex which protects this whole struct */
		pthread_mutex_t mutex;
		/* condition variable for idle threads */
		pthread_cond_t cond;
		/* When this is true, all threads should exit */
		bool done;
		/* Total number of running DNS threads */
		size_t num_threads;
		/* Number of DNS threads which don't have any work */
		size_t num_idle;
		/* List of pending DNS jobs */
		std::list<TDNSArg *> jobs;
	};
	void StartTDNSThread(TDNSTask* task, bool bBind);
	void SetTDNSThreadFinished(TDNSTask* task, bool bBind, addrinfo* aiResult);
	void RetrieveTDNSResult();
	static void* TDNSThread(void* argument);
	static void DoDNS(TDNSArg *arg);

	/** Must be called with threadStatus->mutex held.
	 * @returns false when the calling DNS thread should exit.
	 */
	static bool ThreadNeeded(struct TDNSStatus* status);

	TDNSStatus m_threadStatus;
#endif
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
	virtual void SockError(int iErrno, const CString& sDescription);

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
