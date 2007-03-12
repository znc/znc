//! @author prozac@rottenboy.com

#ifndef _CLIENT_H
#define _CLIENT_H

#include "main.h"

// Forward Declarations
class CZNC;
class CUser;
class CIRCSock;
class CClient;
// !Forward Declarations


class CAuthBase {
public:
	CAuthBase() {}
	CAuthBase(const CString& sUsername, const CString& sPassword) {
		SetLoginInfo(sUsername, sPassword);
	}

	virtual ~CAuthBase() {}

	virtual void SetLoginInfo(const CString& sUsername, const CString& sPassword) {
		m_sUsername = sUsername;
		m_sPassword = sPassword;
	}

	virtual void AcceptLogin(CUser& User) = 0;
	virtual void RefuseLogin(const CString& sReason) = 0;

	virtual const CString& GetUsername() const { return m_sUsername; }
	virtual const CString& GetPassword() const { return m_sPassword; }

	static void AuthUser(CSmartPtr<CAuthBase> AuthClass);

private:
	CString		m_sUsername;
	CString		m_sPassword;
};


class CClientAuth : public CAuthBase {
public:
	CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword) : CAuthBase(sUsername, sPassword) {
		m_pClient = pClient;
	}

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
		Init();
	}

	CClient(const CString& sHostname, unsigned short uPort, int iTimeout = 60) : Csock(sHostname, uPort, iTimeout) {
		Init();
	}

	virtual ~CClient() {
		if (!m_spAuth.IsNull()) {
			CClientAuth* pAuth = (CClientAuth*) &(*m_spAuth);
			pAuth->SetClient(NULL);
		}
	}

	void Init() {
		m_pUser = NULL;
		m_pIRCSock = NULL;
		m_bAuthed = false;
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

	CString GetNick() const;
	CString GetNickMask() const;
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }

	bool DecKeepNickCounter();
	void UserCommand(const CString& sCommand);
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void BouncedOff();
	bool IsAttached() const { return m_bAuthed; }

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
private:
protected:
	bool		m_bAuthed;
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
};

#endif // !_CLIENT_H
