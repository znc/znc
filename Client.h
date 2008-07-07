/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _CLIENT_H
#define _CLIENT_H

#include "Csocket.h"
#include "Utils.h"
#include "main.h"

// Forward Declarations
class CZNC;
class CUser;
class CIRCSock;
class CClient;
class CClientTimeout;
// !Forward Declarations

class CAuthBase {
public:
	CAuthBase(const CString& sUsername, const CString& sPassword, const CString& sRemoteIP) {
		SetLoginInfo(sUsername, sPassword, sRemoteIP);
	}

	virtual ~CAuthBase() {}

	virtual void SetLoginInfo(const CString& sUsername, const CString& sPassword,
			const CString& sRemoteIP) {
		m_sUsername = sUsername;
		m_sPassword = sPassword;
		m_sRemoteIP = sRemoteIP;
	}

	virtual void AcceptLogin(CUser& User) = 0;
	virtual void RefuseLogin(const CString& sReason) = 0;

	virtual const CString& GetUsername() const { return m_sUsername; }
	virtual const CString& GetPassword() const { return m_sPassword; }
	virtual const CString& GetRemoteIP() const { return m_sRemoteIP; }

	static void AuthUser(CSmartPtr<CAuthBase> AuthClass);

private:
	CString		m_sUsername;
	CString		m_sPassword;
	CString		m_sRemoteIP;
};


class CClientAuth : public CAuthBase {
public:
	CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword);
	virtual ~CClientAuth() {}

	void SetClient(CClient* pClient) { m_pClient = pClient; }
	void AcceptLogin(CUser& User);
	void RefuseLogin(const CString& sReason);
private:
protected:
	CClient*	m_pClient;
};

class CClient : public Csock {
public:
	CClient() : Csock() {
		InitClient();
	}

	CClient(const CString& sHostname, unsigned short uPort, int iTimeout = 60) : Csock(sHostname, uPort, iTimeout) {
		InitClient();
	}

	virtual ~CClient();

	void InitClient() {
		m_pUser = NULL;
		m_pTimeout = NULL;
		m_pIRCSock = NULL;
		m_bGotPass = false;
		m_bGotNick = false;
		m_bGotUser = false;
		m_bNamesx = false;
		m_bUHNames = false;
		m_uKeepNickCounter = 0;
		EnableReadLine();
	}

	void AcceptLogin(CUser& User);
	void RefuseLogin(const CString& sReason);
	void StartLoginTimeout();
	void LoginTimeout();

	CString GetNick(bool bAllowIRCNick = true) const;
	CString GetNickMask() const;
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }

	bool DecKeepNickCounter();
	void UserCommand(const CString& sCommand);
	void StatusCTCP(const CString& sCommand);
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void BouncedOff();
	bool IsAttached() const { return m_pUser != NULL; }

	void PutIRC(const CString& sLine);
	void PutClient(const CString& sLine);
	void PutStatus(const CString& sLine);
	void PutStatusNotice(const CString& sLine);
	void PutModule(const CString& sModule, const CString& sLine);
	void PutModNotice(const CString& sModule, const CString& sLine);

	virtual void ReadLine(const CString& sData);
	bool SendMotd();
	void HelpUser();
	void AuthUser();
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort);
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);

	void SetNick(const CString& s);
	CUser* GetUser() const { return m_pUser; }
private:

protected:
	bool		m_bGotPass;
	bool		m_bGotNick;
	bool		m_bGotUser;
	bool		m_bNamesx;
	bool		m_bUHNames;
	CUser*		m_pUser;
	CString		m_sNick;
	CString		m_sPass;
	CString		m_sUser;
	CIRCSock*	m_pIRCSock;
	unsigned int	m_uKeepNickCounter;
	CSmartPtr<CAuthBase>	m_spAuth;
	CClientTimeout*	m_pTimeout;
};

#endif // !_CLIENT_H
