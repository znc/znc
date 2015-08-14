/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Server.h>

CServer::CServer(const CString& sName, unsigned short uPort, const CString& sPass, bool bSSL)
		: m_sName(sName),
		  m_uPort((uPort) ? uPort : (unsigned short)6667),
		  m_sPass(sPass),
		  m_bSSL(bSSL)
{
}

CServer::~CServer() {}

bool CServer::IsValidHostName(const CString& sHostName) {
	return (!sHostName.empty() && !sHostName.Contains(" "));
}

const CString& CServer::GetName() const { return m_sName; }
unsigned short CServer::GetPort() const { return m_uPort; }
const CString& CServer::GetPass() const { return m_sPass; }
bool CServer::IsSSL() const { return m_bSSL; }

CString CServer::GetString(bool bIncludePassword) const {
	return m_sName + " " + CString(m_bSSL ? "+" : "") + CString(m_uPort) +
		CString(bIncludePassword ? (m_sPass.empty() ? "" : " " + m_sPass) : "");
}
