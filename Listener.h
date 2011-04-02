/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _LISTENER_H
#define _LISTENER_H

#include "zncconfig.h"
#include "Socket.h"

// Forward Declarations
class CRealListener;
// !Forward Declarations

class CListener {
public:
	typedef enum {
		ACCEPT_IRC,
		ACCEPT_HTTP,
		ACCEPT_ALL
	} EAcceptType;

	CListener(unsigned short uPort, const CString& sBindHost, bool bSSL, EAddrType eAddr, EAcceptType eAccept) {
		m_uPort = uPort;
		m_sBindHost = sBindHost;
		m_bSSL = bSSL;
		m_eAddr = eAddr;
		m_pListener = NULL;
		m_eAcceptType = eAccept;
	}

	~CListener();

	// Getters
	bool IsSSL() const { return m_bSSL; }
	EAddrType GetAddrType() const { return m_eAddr; }
	unsigned short GetPort() const { return m_uPort; }
	const CString& GetBindHost() const { return m_sBindHost; }
	CRealListener* GetRealListener() const { return m_pListener; }
	EAcceptType GetAcceptType() const { return m_eAcceptType; }
	// !Getters

	// It doesn't make sense to change any of the settings after Listen()
	// except this one, so don't add other setters!
	void SetAcceptType(EAcceptType eType) { m_eAcceptType = eType; }

	bool Listen();
	void ResetRealListener();

private:
protected:
	bool            m_bSSL;
	EAddrType       m_eAddr;
	unsigned short  m_uPort;
	CString         m_sBindHost;
	CRealListener*  m_pListener;
	EAcceptType     m_eAcceptType;
};

class CRealListener : public CZNCSock {
public:
	CRealListener(CListener *pParent) : CZNCSock(), m_pParent(pParent) {}
	virtual ~CRealListener();

	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort);
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	virtual void SockError(int iErrno);

private:
	CListener* m_pParent;
};

class CIncomingConnection : public CZNCSock {
public:
	CIncomingConnection(const CString& sHostname, unsigned short uPort, CListener::EAcceptType eAcceptType);
	virtual ~CIncomingConnection() {}
	virtual void ReadLine(const CString& sData);
	virtual void ReachedMaxBuffer();

private:
	CListener::EAcceptType m_eAcceptType;
};

#endif // !_LISTENER_H
