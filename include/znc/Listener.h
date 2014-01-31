/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LISTENER_H
#define _LISTENER_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>

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
	virtual void SockError(int iErrno, const CString& sDescription);

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
