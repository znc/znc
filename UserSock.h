#ifndef _USERSOCK_H
#define _USERSOCK_H

#include "main.h"

// Forward Declarations
class CZNC;
class CUser;
class CIRCSock;
// !Forward Declarations


class CUserSock : public Csock {
public:
	CUserSock() : Csock() {
		Init();
	}
	CUserSock(const CString& sHostname, unsigned short uPort, int iTimeout = 60) : Csock(sHostname, uPort, iTimeout) {
		Init();
	}
	virtual ~CUserSock() {}

	void Init() {
		m_pZNC = NULL;
		m_pUser = NULL;
		m_pIRCSock = NULL;
		m_bAuthed = false;
		m_bGotPass = false;
		m_bGotNick = false;
		m_bGotUser = false;
		m_uKeepNickCounter = 0;
		EnableReadLine();
	}

	CString GetNick() const;
	CString GetNickMask() const;

	bool DecKeepNickCounter();
	void UserCommand(const CString& sCommand);
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void BouncedOff();
	bool IsAttached() const { return m_bAuthed; }

	void PutIRC(const CString& sLine);
	void PutServ(const CString& sLine);
	void PutStatus(const CString& sLine);
	void PutStatusNotice(const CString& sLine);
	void PutModule(const CString& sModule, const CString& sLine);
	void PutModNotice(const CString& sModule, const CString& sLine);

	virtual void ReadLine(const CString& sData);
	void HelpUser();
	void AuthUser();
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort);
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);

	void SetZNC(CZNC* pZNC) { m_pZNC = pZNC; }
	void SetNick(const CString& s);
private:
protected:
	bool		m_bAuthed;
	bool		m_bGotPass;
	bool		m_bGotNick;
	bool		m_bGotUser;
	CZNC*		m_pZNC;
	CUser*		m_pUser;
	CString		m_sNick;
	CString		m_sPass;
	CString		m_sUser;
	CIRCSock*	m_pIRCSock;
	unsigned int	m_uKeepNickCounter;
};

#endif // !_USERSOCK_H
