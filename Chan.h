/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _CHAN_H
#define _CHAN_H

#include "zncconfig.h"
#include "Nick.h"
#include "ZNCString.h"
#include <map>
#include <set>
#include <vector>

using std::vector;
using std::map;
using std::set;

// Forward Declarations
class CUser;
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

	CChan(const CString& sName, CUser* pUser, bool bInConfig, CConfig *pConfig = NULL);
	~CChan();

	void Reset();
	bool WriteConfig(CFile& File);
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
	int AddBuffer(const CString& sLine);
	void ClearBuffer();
	void TrimBuffer(const unsigned int uMax);
	void SendBuffer(CClient* pClient);
	// !Buffer

	// m_Nick wrappers
	CString GetPermStr() const { return m_Nick.GetPermStr(); }
	bool HasPerm(unsigned char uPerm) const { return m_Nick.HasPerm(uPerm); }
	bool AddPerm(unsigned char uPerm) { return m_Nick.AddPerm(uPerm); }
	bool RemPerm(unsigned char uPerm) { return m_Nick.RemPerm(uPerm); }
	// !wrappers

	// Setters
	void SetIsOn(bool b) { m_bIsOn = b; if (!b) { Reset(); } }
	void SetKey(const CString& s) { m_sKey = s; }
	void SetTopic(const CString& s) { m_sTopic = s; }
	void SetTopicOwner(const CString& s) { m_sTopicOwner = s; }
	void SetTopicDate(unsigned long u) { m_ulTopicDate = u; }
	void SetDefaultModes(const CString& s) { m_sDefaultModes = s; }
	bool SetBufferCount(unsigned int u, bool bForce = false);
	void SetKeepBuffer(bool b) { m_bKeepBuffer = b; }
	void SetDetached(bool b = true) { m_bDetached = b; }
	void SetInConfig(bool b) { m_bInConfig = b; }
	void SetCreationDate(unsigned long u) { m_ulCreationDate = u; }
	void Disable() { m_bDisabled = true; }
	void Enable() { m_bDisabled = false; }
	void IncJoinTries() { m_uJoinTries++; }
	void ResetJoinTries() { m_uJoinTries = 0; }
	// !Setters

	// Getters
	bool HasMode(unsigned char uMode) const;
	CString GetOptions() const;
	CString GetModeArg(unsigned char uMode) const;
	map<char, unsigned int> GetPermCounts() const;
	bool IsOn() const { return m_bIsOn; }
	const CString& GetName() const { return m_sName; }
	const map<unsigned char, CString>& GetModes() const { return m_musModes; }
	const CString& GetKey() const { return m_sKey; }
	const CString& GetTopic() const { return m_sTopic; }
	const CString& GetTopicOwner() const { return m_sTopicOwner; }
	unsigned int GetTopicDate() const { return m_ulTopicDate; }
	const CString& GetDefaultModes() const { return m_sDefaultModes; }
	const vector<CString>& GetBuffer() const { return m_vsBuffer; }
	const map<CString,CNick>& GetNicks() const { return m_msNicks; }
	unsigned int GetNickCount() const { return m_msNicks.size(); }
	unsigned int GetBufferCount() const { return m_uBufferCount; }
	bool KeepBuffer() const { return m_bKeepBuffer; }
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
	bool                         m_bKeepBuffer;
	bool                         m_bInConfig;
	bool                         m_bDisabled;
	CString                      m_sName;
	CString                      m_sKey;
	CString                      m_sTopic;
	CString                      m_sTopicOwner;
	unsigned long                m_ulTopicDate;
	unsigned long                m_ulCreationDate;
	CUser*                       m_pUser;
	CNick                        m_Nick;
	unsigned int                 m_uJoinTries;
	CString                      m_sDefaultModes;
	map<CString,CNick>           m_msNicks;       // Todo: make this caseless (irc style)
	unsigned int                 m_uBufferCount;
	vector<CString>              m_vsBuffer;

	map<unsigned char, CString>  m_musModes;
};

#endif // !_CHAN_H
