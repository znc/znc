/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _IRCSOCK_H
#define _IRCSOCK_H

#include "main.h"
#include "User.h"
#include "Chan.h"
#include "Buffer.h"

// Forward Declarations
class CZNC;
// !Forward Declarations

class CIRCSock : public Csock {
public:
	CIRCSock(CUser* pUser);
	virtual ~CIRCSock();

	typedef enum {
		ListArg		= 0,	// These values must line up with their position in the CHANMODE argument to raw 005
		HasArg		= 1,
		ArgWhenSet	= 2,
		NoArg		= 3
	} EChanModeArgs;

	// Message Handlers
	bool OnCTCPReply(CNick& Nick, CString& sMessage);
	bool OnPrivCTCP(CNick& Nick, CString& sMessage);
	bool OnChanCTCP(CNick& Nick, const CString& sChan, CString& sMessage);
	bool OnPrivMsg(CNick& Nick, CString& sMessage);
	bool OnChanMsg(CNick& Nick, const CString& sChan, CString& sMessage);
	bool OnPrivNotice(CNick& Nick, CString& sMessage);
	bool OnChanNotice(CNick& Nick, const CString& sChan, CString& sMessage);
	// !Message Handlers

	virtual void ReadLine(const CString& sData);
	virtual void Connected();
	virtual void Disconnected();
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();

	void KeepNick(bool bForce = false);
	void PutIRC(const CString& sLine);
	void ParseISupport(const CString& sLine);
	void ResetChans();
	void Quit();

	// Setters
	void SetPass(const CString& s) { m_sPass = s; }
	void SetKeepNick(bool b) { m_bKeepNick = b; }
	void SetOrigNickPending(bool b) { m_bOrigNickPending = b; }
	// !Setters

	// Getters
	bool GetKeepNick() const { return m_bKeepNick; }
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
	bool IsOrigNickPending() const { return m_bOrigNickPending; }
	CUser* GetUser() const { return m_pUser; }
	bool HasNamesx() const { return m_bNamesx; }
	bool HasUHNames() const { return m_bUHNames; }
	const set<unsigned char>& GetUserModes() const { return m_scUserModes; }
	// !Getters
private:
	void SetNick(const CString& sNick);
protected:
	bool						m_bISpoofReleased;
	bool						m_bAuthed;
	bool						m_bKeepNick;
	bool						m_bOrigNickPending;
	bool						m_bNamesx;
	bool						m_bUHNames;
	CString						m_sPerms;
	CString						m_sPermModes;
	set<unsigned char>			m_scUserModes;
	map<unsigned char, EChanModeArgs>	m_mueChanModes;
	CUser*						m_pUser;
	CNick						m_Nick;
	CString						m_sPass;
	map<CString, CChan*>		m_msChans;
	unsigned int				m_uMaxNickLen;
};

#endif // !_IRCSOCK_H
