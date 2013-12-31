/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#ifndef _CHAN_H
#define _CHAN_H

#include <znc/zncconfig.h>
#include <znc/Nick.h>
#include <znc/ZNCString.h>
#include <znc/Buffer.h>
#include <map>

// Forward Declarations
class CUser;
class CIRCNetwork;
class CClient;
class CConfig;
class CFile;
// !Forward Declarations

class CChan {
public:
	typedef enum {
		Voice   = '+',
		HalfOp  = '%',
		Op      = '@',
		Admin   = '!',
		Owner   = '*'
	} EUserPerms;

	typedef enum {
		M_Private    = 'p',
		M_Secret     = 's',
		M_Moderated  = 'm',
		M_InviteOnly = 'i',
		M_NoMessages = 'n',
		M_OpTopic    = 't',
		M_Limit      = 'l',
		M_Key        = 'k',
		M_Op         = 'o',
		M_Voice      = 'v',
		M_Ban        = 'b',
		M_Except     = 'e'
	} EModes;

	CChan(const CString& sName, CIRCNetwork* pNetwork, bool bInConfig, CConfig *pConfig = NULL);
	~CChan();

	void Reset();
	CConfig ToConfig();
	void Clone(CChan& chan);
	void Cycle() const;
	void JoinUser(bool bForce = false, const CString& sKey = "", CClient* pClient = NULL);
	void DetachUser();
	void AttachUser();

	void OnWho(const CString& sNick, const CString& sIdent, const CString& sHost);

	// Modes
	void SetModes(const CString& s);
	void ModeChange(const CString& sModes, const CNick* OpNick = NULL);
	bool AddMode(unsigned char uMode, const CString& sArg);
	bool RemMode(unsigned char uMode);
	CString GetModeString() const;
	CString GetModeArg(CString& sArgs) const;
	CString GetModeForNames() const;
	// !Modes

	// Nicks
	void ClearNicks();
	const CNick* FindNick(const CString& sNick) const;
	CNick* FindNick(const CString& sNick);
	int AddNicks(const CString& sNicks);
	bool AddNick(const CString& sNick);
	bool RemNick(const CString& sNick);
	bool ChangeNick(const CString& sOldNick, const CString& sNewNick);
	// !Nicks

	// Buffer
	const CBuffer& GetBuffer() const { return m_Buffer; }
	unsigned int GetBufferCount() const { return m_Buffer.GetLineCount(); }
	bool SetBufferCount(unsigned int u, bool bForce = false) { return m_Buffer.SetLineCount(u, bForce); };
	size_t AddBuffer(const CString& sFormat, const CString& sText = "", const timeval* ts = NULL) { return m_Buffer.AddLine(sFormat, sText, ts); }
	void ClearBuffer() { m_Buffer.Clear(); }
	void SendBuffer(CClient* pClient);
	// !Buffer

	// m_Nick wrappers
	CString GetPermStr() const { return m_Nick.GetPermStr(); }
	bool HasPerm(unsigned char uPerm) const { return m_Nick.HasPerm(uPerm); }
	bool AddPerm(unsigned char uPerm) { return m_Nick.AddPerm(uPerm); }
	bool RemPerm(unsigned char uPerm) { return m_Nick.RemPerm(uPerm); }
	// !wrappers

	// Setters
	void SetModeKnown(bool b) { m_bModeKnown = b; }
	void SetIsOn(bool b) { m_bIsOn = b; if (!b) { Reset(); } }
	void SetKey(const CString& s) { m_sKey = s; }
	void SetTopic(const CString& s) { m_sTopic = s; }
	void SetTopicOwner(const CString& s) { m_sTopicOwner = s; }
	void SetTopicDate(unsigned long u) { m_ulTopicDate = u; }
	void SetDefaultModes(const CString& s) { m_sDefaultModes = s; }
	void SetAutoClearChanBuffer(bool b);
	void SetDetached(bool b = true) { m_bDetached = b; }
	void SetInConfig(bool b) { m_bInConfig = b; }
	void SetCreationDate(unsigned long u) { m_ulCreationDate = u; }
	void Disable() { m_bDisabled = true; }
	void Enable();
	void IncJoinTries() { m_uJoinTries++; }
	void ResetJoinTries() { m_uJoinTries = 0; }
	// !Setters

	// Getters
	bool IsModeKnown() const { return m_bModeKnown; }
	bool HasMode(unsigned char uMode) const;
	CString GetOptions() const;
	CString GetModeArg(unsigned char uMode) const;
	std::map<char, unsigned int> GetPermCounts() const;
	bool IsOn() const { return m_bIsOn; }
	const CString& GetName() const { return m_sName; }
	const std::map<unsigned char, CString>& GetModes() const { return m_musModes; }
	const CString& GetKey() const { return m_sKey; }
	const CString& GetTopic() const { return m_sTopic; }
	const CString& GetTopicOwner() const { return m_sTopicOwner; }
	unsigned long GetTopicDate() const { return m_ulTopicDate; }
	const CString& GetDefaultModes() const { return m_sDefaultModes; }
	const std::map<CString,CNick>& GetNicks() const { return m_msNicks; }
	size_t GetNickCount() const { return m_msNicks.size(); }
	bool AutoClearChanBuffer() const { return m_bAutoClearChanBuffer; }
	bool IsDetached() const { return m_bDetached; }
	bool InConfig() const { return m_bInConfig; }
	unsigned long GetCreationDate() const { return m_ulCreationDate; }
	bool IsDisabled() const { return m_bDisabled; }
	unsigned int GetJoinTries() const { return m_uJoinTries; }
	// !Getters
private:
protected:
	bool                         m_bDetached;
	bool                         m_bIsOn;
	bool                         m_bAutoClearChanBuffer;
	bool                         m_bInConfig;
	bool                         m_bDisabled;
	CString                      m_sName;
	CString                      m_sKey;
	CString                      m_sTopic;
	CString                      m_sTopicOwner;
	unsigned long                m_ulTopicDate;
	unsigned long                m_ulCreationDate;
	CIRCNetwork*                 m_pNetwork;
	CNick                        m_Nick;
	unsigned int                 m_uJoinTries;
	CString                      m_sDefaultModes;
	std::map<CString,CNick>      m_msNicks;       // Todo: make this caseless (irc style)
	CBuffer                      m_Buffer;

	bool                         m_bModeKnown;
	std::map<unsigned char, CString> m_musModes;
};

#endif // !_CHAN_H
