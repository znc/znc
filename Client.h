/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _CLIENT_H
#define _CLIENT_H

#include "zncconfig.h"
#include "Socket.h"
#include "Utils.h"
#include "main.h"

// Forward Declarations
class CZNC;
class CUser;
class CIRCSock;
class CClient;
// !Forward Declarations

class CAuthBase {
public:
	CAuthBase(const CString& sUsername, const CString& sPassword, Csock *pSock) {
		SetLoginInfo(sUsername, sPassword, pSock);
	}

	virtual ~CAuthBase() {}

	virtual void SetLoginInfo(const CString& sUsername, const CString& sPassword,
			Csock *pSock) {
		m_sUsername = sUsername;
		m_sPassword = sPassword;
		m_pSock = pSock;
	}

	void AcceptLogin(CUser& User);
	void RefuseLogin(const CString& sReason);

	const CString& GetUsername() const { return m_sUsername; }
	const CString& GetPassword() const { return m_sPassword; }
	Csock *GetSocket() const { return m_pSock; }
	CString GetRemoteIP() const;

	// Invalidate this CAuthBase instance which means it will no longer use
	// m_pSock and AcceptLogin() or RefusedLogin() will have no effect.
	virtual void Invalidate();

protected:
	virtual void AcceptedLogin(CUser& User) = 0;
	virtual void RefusedLogin(const CString& sReason) = 0;

private:
	CString  m_sUsername;
	CString  m_sPassword;
	Csock*   m_pSock;
};


class CClientAuth : public CAuthBase {
public:
	CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword);
	virtual ~CClientAuth() {}

	void Invalidate() { m_pClient = NULL; CAuthBase::Invalidate(); }
	void AcceptedLogin(CUser& User);
	void RefusedLogin(const CString& sReason);
private:
protected:
	CClient* m_pClient;
};

class CClient : public CZNCSock {
public:
	CClient() : CZNCSock() {
		m_pUser = NULL;
		m_bGotPass = false;
		m_bGotNick = false;
		m_bGotUser = false;
		m_bInCap = false;
		m_bNamesx = false;
		m_bUHNames = false;
		EnableReadLine();
		// RFC says a line can have 512 chars max, but we are
		// a little more gentle ;)
		SetMaxBufferThreshold(1024);

		SetNick("unknown-nick");
	}

	virtual ~CClient();

	void AcceptLogin(CUser& User);
	void RefuseLogin(const CString& sReason);

	CString GetNick(bool bAllowIRCNick = true) const;
	CString GetNickMask() const;
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }

	void UserCommand(CString& sLine);
	void UserPortCommand(CString& sLine);
	void StatusCTCP(const CString& sCommand);
	void BouncedOff();
	bool IsAttached() const { return m_pUser != NULL; }

	void PutIRC(const CString& sLine);
	void PutClient(const CString& sLine);
	unsigned int PutStatus(const CTable& table);
	void PutStatus(const CString& sLine);
	void PutStatusNotice(const CString& sLine);
	void PutModule(const CString& sModule, const CString& sLine);
	void PutModNotice(const CString& sModule, const CString& sLine);

	bool IsCapEnabled(const CString& sCap) { return 1 == m_ssAcceptedCaps.count(sCap); }

	virtual void ReadLine(const CString& sData);
	bool SendMotd();
	void HelpUser();
	void AuthUser();
	virtual void Connected();
	virtual void Timeout();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual void ReachedMaxBuffer();

	void SetNick(const CString& s);
	CUser* GetUser() const { return m_pUser; }
	const CIRCSock* GetIRCSock() const;
	CIRCSock* GetIRCSock();
private:
	void HandleCap(const CString& sLine);
	void RespondCap(const CString& sResponse);

protected:
	bool                 m_bGotPass;
	bool                 m_bGotNick;
	bool                 m_bGotUser;
	bool                 m_bInCap;
	bool                 m_bNamesx;
	bool                 m_bUHNames;
	CUser*               m_pUser;
	CString              m_sNick;
	CString              m_sPass;
	CString              m_sUser;
	CSmartPtr<CAuthBase> m_spAuth;
	SCString             m_ssAcceptedCaps;
};

#endif // !_CLIENT_H
