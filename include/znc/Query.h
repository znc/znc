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

#ifndef _QUERY_H
#define _QUERY_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <znc/Buffer.h>

// Forward Declarations
class CClient;
class CIRCNetwork;
// !Forward Declarations

class CQuery {
public:
	CQuery(const CString& sName, CIRCNetwork* pNetwork);
	~CQuery();

	// Buffer
	const CBuffer& GetBuffer() const { return m_Buffer; }
	unsigned int GetBufferCount() const { return m_Buffer.GetLineCount(); }
	bool SetBufferCount(unsigned int u, bool bForce = false) { return m_Buffer.SetLineCount(u, bForce); }
	size_t AddBuffer(const CString& sFormat, const CString& sText = "", const timeval* ts = nullptr) { return m_Buffer.AddLine(sFormat, sText, ts); }
	void ClearBuffer() { m_Buffer.Clear(); }
	void SendBuffer(CClient* pClient);
	void SendBuffer(CClient* pClient, const CBuffer& Buffer);
	// !Buffer

	// Getters
	const CString& GetName() const { return m_sName; }
	// !Getters

private:
	CString                      m_sName;
	CIRCNetwork*                 m_pNetwork;
	CBuffer                      m_Buffer;
};

#endif // !_QUERY_H
