/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _USER_H
#define _USER_H

#include "zncconfig.h"
#include "Buffer.h"
#include "Modules.h"
#include "Nick.h"
#include <set>
#include <vector>

using std::set;
using std::vector;

class CChan;
class CClient;
class CConfig;
class CFile;
class CIRCSock;
class CUserTimer;
class CServer;

class CUser {
public:
	CUser(const CString& sUserName);
	~CUser();

	bool ParseConfig(CConfig* Config, CString& sError);

	enum eHashType {
		HASH_NONE,
		HASH_MD5,
		HASH_SHA256,

		HASH_DEFAULT = HASH_SHA256
	};

	// If you change the default hash here and in HASH_DEFAULT,
	// don't forget CUtils::sDefaultHash!
	static CString SaltedHash(const CString& sPass, const CString& sSalt) {
		return CUtils::SaltedSHA256Hash(sPass, sSalt);
	}

	bool PrintLine(CFile& File, CString sName, CString sValue) const;
	bool WriteConfig(CFile& File);
	CChan* FindChan(const CString& sName) const;
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName, bool bInConfig);
	bool DelChan(const CString& sName);
	void JoinChans();
	CServer* FindServer(const CString& sName) const;
	bool DelServer(const CString& sName, unsigned short uPort, const CString& sPass);
	bool AddServer(const CString& sName);
	bool AddServer(const CString& sName, unsigned short uPort, const CString& sPass = "", bool bSSL = false);
	CServer* GetNextServer();
	CServer* GetCurrentServer() const;
	bool SetNextServer(const CServer* pServer);
	bool CheckPass(const CString& sPass) const;
	bool AddAllowedHost(const CString& sHostMask);
	bool IsHostAllowed(const CString& sHostMask) const;
	bool IsValid(CString& sErrMsg, bool bSkipPass = false) const;
	static bool IsValidUserName(const CString& sUserName);
	static CString MakeCleanUserName(const CString& sUserName);
	bool IsLastServer() const;

	void DelClients();
	void DelServers();
	void DelModules();

	// Unloads a module on all users who have it loaded and loads it again.
	static bool UpdateModule(const CString &sModule);

	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules

	// Buffers
	void AddRawBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_RawBuffer.AddLine(sPre, sPost, bIncNick); }
	void UpdateRawBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_RawBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void UpdateExactRawBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_RawBuffer.UpdateExactLine(sPre, sPost, bIncNick); }
	void ClearRawBuffer() { m_RawBuffer.Clear(); }

	void AddMotdBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_MotdBuffer.AddLine(sPre, sPost, bIncNick); }
	void UpdateMotdBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_MotdBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void ClearMotdBuffer() { m_MotdBuffer.Clear(); }

	void AddQueryBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_QueryBuffer.AddLine(sPre, sPost, bIncNick); }
	void UpdateQueryBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_QueryBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void ClearQueryBuffer() { m_QueryBuffer.Clear(); }
	// !Buffers

	bool PutIRC(const CString& sLine);
	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatusNotice(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModNotice(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	bool IsUserAttached() const { return !m_vClients.empty(); }
	void UserConnected(CClient* pClient);
	void UserDisconnected(CClient* pClient);

	CString GetLocalIP();
	CString GetLocalDCCIP();

	/** This method will return whether the user is connected and authenticated to an IRC server.
	 */
	bool IsIRCConnected() const;
	void SetIRCSocket(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void CheckIRCConnect();

	CString ExpandString(const CString& sStr) const;
	CString& ExpandString(const CString& sStr, CString& sRet) const;

	CString AddTimestamp(const CString& sStr) const;
	CString& AddTimestamp(const CString& sStr, CString& sRet) const;

	CString GetCurNick() const;
	bool Clone(const CUser& User, CString& sErrorRet, bool bCloneChans = true);
	void BounceAllClients();

	void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
	void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }

	// Setters
	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);
	void SetBindHost(const CString& s);
	void SetDCCBindHost(const CString& s);
	void SetPass(const CString& s, eHashType eHash, const CString& sSalt = "");
	void SetMultiClients(bool b);
	void SetDenyLoadMod(bool b);
	void SetAdmin(bool b);
	void SetDenySetBindHost(bool b);
	bool SetStatusPrefix(const CString& s);
	void SetDefaultChanModes(const CString& s);
	void SetIRCNick(const CNick& n);
	void SetIRCServer(const CString& s);
	void SetQuitMsg(const CString& s);
	bool AddCTCPReply(const CString& sCTCP, const CString& sReply);
	bool DelCTCPReply(const CString& sCTCP);
	bool SetBufferCount(unsigned int u, bool bForce = false);
	void SetKeepBuffer(bool b);
	void SetChanPrefixes(const CString& s) { m_sChanPrefixes = s; }
	void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }
	void SetTimestampFormat(const CString& s) { m_sTimestampFormat = s; }
	void SetTimestampAppend(bool b) { m_bAppendTimestamp = b; }
	void SetTimestampPrepend(bool b) { m_bPrependTimestamp = b; }
	void SetTimezoneOffset(float b) { m_fTimezoneOffset = b; }
	void SetJoinTries(unsigned int i) { m_uMaxJoinTries = i; }
	void SetMaxJoins(unsigned int i) { m_uMaxJoins = i; }
	void SetSkinName(const CString& s) { m_sSkinName = s; }
	void SetIRCConnectEnabled(bool b) { m_bIRCConnectEnabled = b; }
	void SetIRCAway(bool b) { m_bIRCAway = b; }
	// !Setters

	// Getters
	vector<CClient*>& GetClients() { return m_vClients; }
	CIRCSock* GetIRCSock() { return m_pIRCSock; }
	const CIRCSock* GetIRCSock() const { return m_pIRCSock; }
	const CString& GetUserName() const;
	const CString& GetCleanUserName() const;
	const CString& GetNick(bool bAllowDefault = true) const;
	const CString& GetAltNick(bool bAllowDefault = true) const;
	const CString& GetIdent(bool bAllowDefault = true) const;
	const CString& GetRealName() const;
	const CString& GetBindHost() const;
	const CString& GetDCCBindHost() const;
	const CString& GetPass() const;
	eHashType GetPassHashType() const;
	const CString& GetPassSalt() const;
	const set<CString>& GetAllowedHosts() const;
	const CString& GetTimestampFormat() const;
	bool GetTimestampAppend() const;
	bool GetTimestampPrepend() const;
	bool GetIRCConnectEnabled() const { return m_bIRCConnectEnabled; }

	const CString& GetChanPrefixes() const { return m_sChanPrefixes; }
	bool IsChan(const CString& sChan) const;

	const CString& GetUserPath() const;

	bool DenyLoadMod() const;
	bool IsAdmin() const;
	bool DenySetBindHost() const;
	bool MultiClients() const;
	const CString& GetStatusPrefix() const;
	const CString& GetDefaultChanModes() const;
	const vector<CChan*>& GetChans() const;
	const vector<CServer*>& GetServers() const;
	const CNick& GetIRCNick() const;
	const CString& GetIRCServer() const;
	CString GetQuitMsg() const;
	const MCString& GetCTCPReplies() const;
	unsigned int GetBufferCount() const;
	bool KeepBuffer() const;
	bool IsBeingDeleted() const { return m_bBeingDeleted; }
	bool HasServers() const { return !m_vServers.empty(); }
	float GetTimezoneOffset() const { return m_fTimezoneOffset; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }
	unsigned int JoinTries() const { return m_uMaxJoinTries; }
	unsigned int MaxJoins() const { return m_uMaxJoins; }
	bool IsIRCAway() const { return m_bIRCAway; }
	CString GetSkinName() const;
	// !Getters
private:
	bool JoinChan(CChan* pChan);
	void JoinChans(set<CChan*>& sChans);

protected:
	const CString         m_sUserName;
	const CString         m_sCleanUserName;
	CString               m_sNick;
	CString               m_sAltNick;
	CString               m_sIdent;
	CString               m_sRealName;
	CString               m_sBindHost;
	CString               m_sDCCBindHost;
	CString               m_sPass;
	CString               m_sPassSalt;
	CString               m_sStatusPrefix;
	CString               m_sDefaultChanModes;
	CString               m_sChanPrefixes;
	CNick                 m_IRCNick;
	bool                  m_bIRCAway;
	CString               m_sIRCServer;
	CString               m_sQuitMsg;
	MCString              m_mssCTCPReplies;
	CString               m_sTimestampFormat;
	float                 m_fTimezoneOffset;
	eHashType             m_eHashType;

	// Paths
	CString               m_sUserPath;
	// !Paths

	CBuffer               m_RawBuffer;
	CBuffer               m_MotdBuffer;
	CBuffer               m_QueryBuffer;
	bool                  m_bMultiClients;
	bool                  m_bDenyLoadMod;
	bool                  m_bAdmin;
	bool                  m_bDenySetBindHost;
	bool                  m_bKeepBuffer;
	bool                  m_bBeingDeleted;
	bool                  m_bAppendTimestamp;
	bool                  m_bPrependTimestamp;
	bool                  m_bIRCConnectEnabled;
	CIRCSock*             m_pIRCSock;

	CUserTimer*           m_pUserTimer;

	vector<CServer*>      m_vServers;
	vector<CChan*>        m_vChans;
	vector<CClient*>      m_vClients;
	set<CString>          m_ssAllowedHosts;
	unsigned int          m_uServerIdx; ///< Index in m_vServers of our current server + 1
	unsigned int          m_uBufferCount;
	unsigned long long    m_uBytesRead;
	unsigned long long    m_uBytesWritten;
	unsigned int          m_uMaxJoinTries;
	unsigned int          m_uMaxJoins;
	CString               m_sSkinName;

	CModules*             m_pModules;
};

#endif // !_USER_H
