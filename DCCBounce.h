/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _DCCBOUNCE_H
#define _DCCBOUNCE_H

#include "zncconfig.h"
#include "Socket.h"

class CUser;

class CDCCBounce : public CZNCSock {
public:
	CDCCBounce(CUser* pUser, unsigned long uLongIP, unsigned short uPort,
			const CString& sFileName, const CString& sRemoteNick,
			const CString& sRemoteIP, bool bIsChat = false);
	CDCCBounce(const CString& sHostname, unsigned short uPort, CUser* pUser,
			const CString& sRemoteNick, const CString& sRemoteIP,
			const CString& sFileName, int iTimeout = 60, bool bIsChat = false);
	virtual ~CDCCBounce();

	static unsigned short DCCRequest(const CString& sNick, unsigned long uLongIP, unsigned short uPort, const CString& sFileName, bool bIsChat, CUser* pUser, const CString& sRemoteIP);

	void ReadLine(const CString& sData);
	virtual void ReadData(const char* data, size_t len);
	virtual void ReadPaused();
	virtual void Timeout();
	virtual void ConnectionRefused();
	virtual void ReachedMaxBuffer();
	virtual void SockError(int iErrno);
	virtual void Connected();
	virtual void Disconnected();
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	void Shutdown();
	void PutServ(const CString& sLine);
	void PutPeer(const CString& sLine);
	bool IsPeerConnected() { return (m_pPeer) ? m_pPeer->IsConnected() : false; }

	// Setters
	void SetPeer(CDCCBounce* p) { m_pPeer = p; }
	void SetRemoteIP(const CString& s) { m_sRemoteIP = s; }
	void SetRemoteNick(const CString& s) { m_sRemoteNick = s; }
	void SetRemote(bool b) { m_bIsRemote = b; }
	// !Setters

	// Getters
	unsigned short GetUserPort() const { return m_uRemotePort; }
	const CString& GetRemoteIP() const { return m_sRemoteIP; }
	const CString& GetRemoteNick() const { return m_sRemoteNick; }
	const CString& GetFileName() const { return m_sFileName; }
	CDCCBounce* GetPeer() const { return m_pPeer; }
	bool IsRemote() { return m_bIsRemote; }
	// !Getters
private:
protected:
	CString                      m_sRemoteNick;
	CString                      m_sRemoteIP;
	CString                      m_sConnectIP;
	CString                      m_sLocalIP;
	CString                      m_sFileName;
	CUser*                       m_pUser;
	CDCCBounce*                  m_pPeer;
	unsigned short               m_uRemotePort;
	bool                         m_bIsChat;
	bool                         m_bIsRemote;

	static const unsigned int    m_uiMaxDCCBuffer;
	static const unsigned int    m_uiMinDCCBuffer;
};

#endif // !_DCCBOUNCE_H

