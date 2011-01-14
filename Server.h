/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _SERVER_H
#define _SERVER_H

#include "zncconfig.h"
#include "ZNCString.h"

class CServer {
public:
	CServer(const CString& sName, unsigned short uPort = 6667, const CString& sPass = "", bool bSSL = false);
	~CServer();

	const CString& GetName() const;
	unsigned short GetPort() const;
	const CString& GetPass() const;
	bool IsSSL() const;
	CString GetString(bool bIncludePassword = true) const;
	static bool IsValidHostName(const CString& sHostName);
private:
protected:
	CString         m_sName;
	unsigned short  m_uPort;
	CString         m_sPass;
	bool            m_bSSL;
};

#endif // !_SERVER_H
