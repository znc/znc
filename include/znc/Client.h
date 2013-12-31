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

#ifndef _CLIENT_H
#define _CLIENT_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>
#include <znc/Utils.h>
#include <znc/main.h>

// Forward Declarations
class CZNC;
class CUser;
class CIRCNetwork;
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
		m_pNetwork = NULL;
		m_bGotPass = false;
		m_bGotNick = false;
		m_bGotUser = false;
		m_bInCap = false;
		m_bNamesx = false;
		m_bUHNames = false;
		m_bAway = false;
		m_bServerTime = false;
		EnableReadLine();
		// RFC says a line can have 512 chars max, but we are
		// a little more gentle ;)
		SetMaxBufferThreshold(1024);

		SetNick("unknown-nick");
	}

	virtual ~CClient();

	void SendRequiredPasswordNotice();
	void AcceptLogin(CUser& User);
	void RefuseLogin(const CString& sReason);

	CString GetNick(bool bAllowIRCNick = true) const;
	CString GetNickMask() const;
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }
	bool IsAway() const { return m_bAway; }
	bool HasServerTime() const { return m_bServerTime; }

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
	void SetAway(bool bAway) { m_bAway = bAway; }
	CUser* GetUser() const { return m_pUser; }
	void SetNetwork(CIRCNetwork* pNetwork, bool bDisconnect=true, bool bReconnect=true);
	CIRCNetwork* GetNetwork() const { return m_pNetwork; }
	std::vector<CClient*>& GetClients();
	const CIRCSock* GetIRCSock() const;
	CIRCSock* GetIRCSock();
	CString GetFullName();
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
	bool                 m_bAway;
	bool                 m_bServerTime;
	CUser*               m_pUser;
	CIRCNetwork*         m_pNetwork;
	CString              m_sNick;
	CString              m_sPass;
	CString              m_sUser;
	CString              m_sNetwork;
	CSmartPtr<CAuthBase> m_spAuth;
	SCString             m_ssAcceptedCaps;
};

#endif // !_CLIENT_H
