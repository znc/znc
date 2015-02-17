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

#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <znc/Nick.h>
#include <sys/time.h>

class CChan;
class CClient;
class CIRCNetwork;

class CMessage {
public:
	explicit CMessage(const CString& sMessage = "");
	CMessage(const CNick& Nick, const CString& sCommand, const VCString& vsParams, const MCString& mssTags = MCString::EmptyMap);

	// ZNC <-> IRC
	CIRCNetwork* GetNetwork() const { return m_pNetwork; }
	void SetNetwork(CIRCNetwork* pNetwork) { m_pNetwork = pNetwork; }

	// ZNC <-> CLI
	CClient* GetClient() const { return m_pClient; }
	void SetClient(CClient* pClient) { m_pClient = pClient; }

	CChan* GetChan() const { return m_pChan; }
	void SetChan(CChan* pChan) { m_pChan = pChan; }

	CNick& GetNick() { return m_Nick; }
	const CNick& GetNick() const { return m_Nick; }
	void SetNick(const CNick& Nick) { m_Nick = Nick; }

	const CString& GetCommand() const { return m_sCommand; }
	void SetCommand(const CString& sCommand) { m_sCommand = sCommand; }

	const VCString& GetParams() const { return m_vsParams; }
	CString GetParams(unsigned int uIdx, unsigned int uLen = -1) const;
	void SetParams(const VCString& vsParams) { m_vsParams = vsParams; }

	CString GetParam(unsigned int uIdx) const;
	void SetParam(unsigned int uIdx, const CString& sParam);

	const timeval& GetTime() const { return m_time; }
	void SetTime(const timeval& ts) { m_time = ts; }

	const MCString& GetTags() const { return m_mssTags; }
	void SetTags(const MCString& mssTags) { m_mssTags = mssTags; }

	CString GetTag(const CString& sKey) const;
	void SetTag(const CString& sKey, const CString& sValue);

	enum FormatFlags {
		IncludeAll = 0x0,
		ExcludePrefix = 0x1,
		ExcludeTags = 0x2
	};

	CString ToString(unsigned int uFlags = IncludeAll) const;
	void Parse(CString sMessage);

private:
	void InitTime();

	CNick        m_Nick;
	CString      m_sCommand;
	VCString     m_vsParams;
	MCString     m_mssTags;
	timeval      m_time;
	CIRCNetwork* m_pNetwork = nullptr;
	CClient*     m_pClient = nullptr;
	CChan*       m_pChan = nullptr;
};

#endif // !_MESSAGE_H
