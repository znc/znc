/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _IRCSOCK_H
#define _IRCSOCK_H

#include "zncconfig.h"
#include "Socket.h"
#include "Nick.h"

// Forward Declarations
class CChan;
class CUser;
class CClient;
// !Forward Declarations

class CIRCSock : public CZNCSock {
public:
	CIRCSock(CUser* pUser);
	virtual ~CIRCSock();

	typedef enum {
		// These values must line up with their position in the CHANMODE argument to raw 005
		ListArg    = 0,
		HasArg     = 1,
		ArgWhenSet = 2,
		NoArg      = 3
	} EChanModeArgs;

	// Message Handlers
	bool OnCTCPReply(CNick& Nick, CString& sMessage);
	bool OnPrivCTCP(CNick& Nick, CString& sMessage);
	bool OnChanCTCP(CNick& Nick, const CString& sChan, CString& sMessage);
	bool OnGeneralCTCP(CNick& Nick, CString& sMessage);
	bool OnPrivMsg(CNick& Nick, CString& sMessage);
	bool OnChanMsg(CNick& Nick, const CString& sChan, CString& sMessage);
	bool OnPrivNotice(CNick& Nick, CString& sMessage);
	bool OnChanNotice(CNick& Nick, const CString& sChan, CString& sMessage);
	bool OnServerCapAvailable(const CString& sCap);
	// !Message Handlers

	virtual void ReadLine(const CString& sData);
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();
	virtual void ReachedMaxBuffer();

	void PutIRC(const CString& sLine);
	void ResetChans();
	void Quit(const CString& sQuitMsg = "");

	/** You can call this from CModule::OnServerCapResult to suspend
	 *  sending other CAP requests and CAP END for a while. Each
	 *  call to PauseCap should be balanced with a call to ResumeCap.
	 */
	void PauseCap();
	/** If you used PauseCap, call this when CAP negotiation and logging in
	 *  should be resumed again.
	 */
	void ResumeCap();

	// Setters
	void SetPass(const CString& s) { m_sPass = s; }
	// !Setters

	// Getters
	unsigned int GetMaxNickLen() const { return m_uMaxNickLen; }
	EChanModeArgs GetModeType(unsigned char uMode) const;
	unsigned char GetPermFromMode(unsigned char uMode) const;
	const map<unsigned char, EChanModeArgs>& GetChanModes() const { return m_mueChanModes; }
	bool IsPermChar(const char c) const { return (c != '\0' && GetPerms().find(c) != CString::npos); }
	bool IsPermMode(const char c) const { return (c != '\0' && GetPermModes().find(c) != CString::npos); }
	const CString& GetPerms() const { return m_sPerms; }
	const CString& GetPermModes() const { return m_sPermModes; }
	CString GetNickMask() const { return m_Nick.GetNickMask(); }
	const CString& GetNick() const { return m_Nick.GetNick(); }
	const CString& GetPass() const { return m_sPass; }
	CUser* GetUser() const { return m_pUser; }
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }
	const set<unsigned char>& GetUserModes() const { return m_scUserModes; }
	// This is true if we are past raw 001
	bool IsAuthed() const { return m_bAuthed; }
	bool IsCapAccepted(const CString& sCap) { return 1 == m_ssAcceptedCaps.count(sCap); }
	// !Getters

	// This handles NAMESX and UHNAMES in a raw 353 reply
	void ForwardRaw353(const CString& sLine) const;
	void ForwardRaw353(const CString& sLine, CClient* pClient) const;
private:
	void SetNick(const CString& sNick);
	void ParseISupport(const CString& sLine);
	// This is called when we connect and the nick we want is already taken
	void SendAltNick(const CString& sBadNick);
	void SendNextCap();
protected:
	bool                                m_bAuthed;
	bool                                m_bNamesx;
	bool                                m_bUHNames;
	CString                             m_sPerms;
	CString                             m_sPermModes;
	set<unsigned char>                  m_scUserModes;
	map<unsigned char, EChanModeArgs>   m_mueChanModes;
	CUser*                              m_pUser;
	CNick                               m_Nick;
	CString                             m_sPass;
	map<CString, CChan*>                m_msChans;
	unsigned int                        m_uMaxNickLen;
	unsigned int                        m_uCapPaused;
	SCString                            m_ssAcceptedCaps;
	SCString                            m_ssPendingCaps;
	time_t                              m_lastCTCP;
	unsigned int                        m_uNumCTCP;
	static const time_t                 m_uCTCPFloodTime;
	static const unsigned int           m_uCTCPFloodCount;
};

#endif // !_IRCSOCK_H
