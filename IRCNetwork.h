/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _IRCNETWORK_H
#define _IRCNETWORK_H

#include "zncconfig.h"
#include "ZNCString.h"

class CUser;
class CFile;
class CConfig;

class CIRCNetwork {
public:
	CIRCNetwork(CUser *pUser);
	CIRCNetwork(CUser *pUser, const CString& sName);
	CIRCNetwork(CUser *pUser, const CIRCNetwork *pNetwork);
	~CIRCNetwork();

	bool ParseConfig(CConfig *pConfig, CString& sError);
	bool WriteConfig(CFile& File); 

	CUser* GetUser();
	const CString& GetName() const;

	void SetUser(CUser *pUser);
	void SetName(const CString& sName);
protected:
	CString  m_sName;
	CUser*   m_pUser;
};

#endif // !_IRCNETWORK_H
