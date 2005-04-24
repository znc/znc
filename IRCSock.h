#ifndef _IRCSOCK_H
#define _IRCSOCK_H

#include "main.h"
#include "User.h"
#include "Chan.h"
#include "Buffer.h"

// Forward Declarations
class CZNC;
class CUserSock;
// !Forward Declarations

class CIRCSock : public Csock {
public:
	CIRCSock(CZNC* pZNC, CUser* pUser);
	virtual ~CIRCSock();

	// Message Handlers
	bool OnCTCPReply(const string& sNickMask, string& sMessage);
	bool OnPrivCTCP(const string& sNickMask, string& sMessage);
	bool OnChanCTCP(const string& sNickMask, const string& sChan, string& sMessage);
	bool OnPrivMsg(const string& sNickMask, string& sMessage);
	bool OnChanMsg(const string& sNickMask, const string& sChan, string& sMessage);
	bool OnPrivNotice(const string& sNickMask, string& sMessage);
	bool OnChanNotice(const string& sNickMask, const string& sChan, string& sMessage);
	// !Message Handlers

	virtual void ReadLine(const string& sData);
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();

	void KeepNick();
	bool IsUserAttached() { return (m_pUserSock != NULL); }
	void UserConnected(CUserSock* pUserSock);
	void UserDisconnected();
	void PutServ(const string& sLine);
	void PutUser(const string& sLine);
	void PutStatus(const string& sLine);

	// Setters
	void SetPass(const string& s) { m_sPass = s; }
	// !Setters

	// Getters
	string GetNickMask() const {
		return m_Nick.GetNickMask();
	}
	const string& GetNick() const { return m_Nick.GetNick(); }
	const string& GetPass() const { return m_sPass; }
	// !Getters
private:
	void SetNick(const string& sNick);
protected:
	bool					m_bISpoofReleased;
	bool					m_bAuthed;
	bool					m_bKeepNick;
	CZNC*					m_pZNC;
	CUser*					m_pUser;
	CNick					m_Nick;
	string					m_sPass;
	CBuffer					m_RawBuffer;
	CBuffer					m_MotdBuffer;
	CUserSock*				m_pUserSock;
	map<string, CChan*>		m_msChans;
	unsigned int			m_uQueryBufferCount;
	CBuffer					m_QueryBuffer;
};

#endif // !_IRCSOCK_H
