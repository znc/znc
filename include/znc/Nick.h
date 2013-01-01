/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _NICK_H
#define _NICK_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <vector>

// Forward Decl
class CIRCNetwork;
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
	size_t GetCommonChans(std::vector<CChan*>& vChans, CIRCNetwork* pNetwork) const;

	// Setters
	void SetNetwork(CIRCNetwork* pNetwork);
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
	CString      m_sChanPerms;
	CIRCNetwork* m_pNetwork;
	CString      m_sNick;
	CString      m_sIdent;
	CString      m_sHost;
};

#endif // !_NICK_H
