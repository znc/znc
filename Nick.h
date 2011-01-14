/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _NICK_H
#define _NICK_H

#include "zncconfig.h"
#include "ZNCString.h"
#include <set>
#include <vector>

using std::vector;
using std::set;

// Forward Decl
class CUser;
class CChan;
// !Forward Decl

class CNick
{
public:
	CNick();
	CNick(const CString& sNick);
	~CNick();

	void Reset();
	void Parse(const CString& sNickMask);
	CString GetHostMask() const;
	unsigned int GetCommonChans(vector<CChan*>& vChans, CUser* pUser) const;

	// Setters
	void SetUser(CUser* pUser);
	void SetNick(const CString& s);
	void SetIdent(const CString& s);
	void SetHost(const CString& s);
	bool AddPerm(unsigned char uPerm);
	bool RemPerm(unsigned char uPerm);
	// !Setters

	// Getters
	CString GetPermStr() const;
	unsigned char GetPermChar() const;
	bool HasPerm(unsigned char uPerm) const;
	const CString& GetNick() const;
	const CString& GetIdent() const;
	const CString& GetHost() const;
	CString GetNickMask() const;
	// !Getters

	void Clone(const CNick& SourceNick);
private:
protected:
	CString    m_sChanPerms;
	CUser*     m_pUser;
	CString    m_sNick;
	CString    m_sIdent;
	CString    m_sHost;
};

#endif // !_NICK_H
