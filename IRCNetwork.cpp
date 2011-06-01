/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "IRCNetwork.h"
#include "User.h"
#include "FileUtils.h"

CIRCNetwork::CIRCNetwork(CUser *pUser, const CString& sName) {
	m_pUser = NULL;
	SetUser(pUser);
	SetName(sName);
}

CIRCNetwork::CIRCNetwork(CUser *pUser, const CIRCNetwork *pNetwork) {
	m_pUser = NULL;
	SetUser(pUser);
	SetName(pNetwork->GetName());
}

CIRCNetwork::~CIRCNetwork() {
	SetUser(NULL);
}

bool CIRCNetwork::ParseConfig(CConfig *pConfig, CString& sError) {
	return true;
}

bool CIRCNetwork::WriteConfig(CFile& File) {
	File.Write("\t<Network " + m_sName + ">\n");
	File.Write("\t</Network>\n");
	return true;
}

CUser* CIRCNetwork::GetUser() {
	return m_pUser;
}

const CString& CIRCNetwork::GetName() const {
	return m_sName;
}

void CIRCNetwork::SetUser(CUser *pUser) {
	if (m_pUser) {
		m_pUser->RemoveNetwork(this);
	}

	m_pUser = pUser;
	if (m_pUser) {
		m_pUser->AddNetwork(this);
	}
}

void CIRCNetwork::SetName(const CString& sName) {
	m_sName = sName;
}
