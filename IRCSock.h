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
	bool OnCTCPReply(const CString& sNickMask, CString& sMessage);
	bool OnPrivCTCP(const CString& sNickMask, CString& sMessage);
	bool OnChanCTCP(const CString& sNickMask, const CString& sChan, CString& sMessage);
	bool OnPrivMsg(const CString& sNickMask, CString& sMessage);
	bool OnChanMsg(const CString& sNickMask, const CString& sChan, CString& sMessage);
	bool OnPrivNotice(const CString& sNickMask, CString& sMessage);
	bool OnChanNotice(const CString& sNickMask, const CString& sChan, CString& sMessage);
	// !Message Handlers

	virtual void ReadLine(const CString& sData);
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();

	void KeepNick();
	bool IsUserAttached() { return (m_pUserSock != NULL); }
	void UserConnected(CUserSock* pUserSock);
	void UserDisconnected();
	void PutServ(const CString& sLine);
	void PutUser(const CString& sLine);
	void PutStatus(const CString& sLine);
	void CIRCSock::ParseISupport(const CString& sLine);

	// Setters
	void SetPass(const CString& s) { m_sPass = s; }
	// !Setters

	// Getters
	CString GetNickMask() const {
		return m_Nick.GetNickMask();
	}
	const CString& GetNick() const { return m_Nick.GetNick(); }
	const CString& GetPass() const { return m_sPass; }
	// !Getters
private:
	void SetNick(const CString& sNick);
protected:
	bool					m_bISpoofReleased;
	bool					m_bAuthed;
	bool					m_bKeepNick;
	CString					m_sNickPrefixes;
	CZNC*					m_pZNC;
	CUser*					m_pUser;
	CNick					m_Nick;
	CString					m_sPass;
	CBuffer					m_RawBuffer;
	CBuffer					m_MotdBuffer;
	CUserSock*				m_pUserSock;
	map<CString, CChan*>		m_msChans;
	unsigned int			m_uQueryBufferCount;
	CBuffer					m_QueryBuffer;
};

#endif // !_IRCSOCK_H
