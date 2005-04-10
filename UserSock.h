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
	CUserSock(const string& sHostname, int iport, int itimeout = 60) : Csock(sHostname, iport, itimeout) {
		Init();
	}
	virtual ~CUserSock() {}

	void Init() {
		m_pZNC = NULL;
		m_pUser = NULL;
		m_pIRCSock = NULL;
		m_bAuthed = false;
		m_bGotNick = false;
		m_bGotUser = false;
		m_uKeepNickCounter = 0;
		EnableReadLine();
	}

	string GetNick() const;
	string GetNickMask() const;

	bool DecKeepNickCounter();
	void UserCommand(const string& sCommand);
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void BouncedOff();
	bool IsAttached() const { return m_bAuthed; }

	void PutIRC(const string& sLine);
	void PutServ(const string& sLine);
	void PutStatus(const string& sLine);
	void PutStatusNotice(const string& sLine);
	void PutModule(const string& sModule, const string& sLine);
	void PutModNotice(const string& sModule, const string& sLine);

	virtual void ReadLine(const string& sData);
	void HelpUser();
	void AuthUser();
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual bool ConnectionFrom(const string& sHost, int iPort);
	virtual Csock* GetSockObj(const string& sHost, int iPort);

	void SetZNC(CZNC* pZNC) { m_pZNC = pZNC; }
	void SetNick(const string& s);
private:
protected:
	bool		m_bAuthed;
	bool		m_bGotNick;
	bool		m_bGotUser;
	CZNC*		m_pZNC;
	CUser*		m_pUser;
	string		m_sNick;
	string		m_sPass;
	string		m_sUser;
	CIRCSock*	m_pIRCSock;
	unsigned int	m_uKeepNickCounter;
};

#endif // !_USERSOCK_H
