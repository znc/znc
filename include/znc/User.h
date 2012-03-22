/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _USER_H
#define _USER_H

#include <znc/zncconfig.h>
#include <znc/Utils.h>
#include <znc/Buffer.h>
#include <znc/Nick.h>
#include <set>
#include <vector>

using std::set;
using std::vector;

class CModules;
class CChan;
class CClient;
class CConfig;
class CFile;
class CIRCNetwork;
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

	CConfig ToConfig();
	bool CheckPass(const CString& sPass) const;
	bool AddAllowedHost(const CString& sHostMask);
	bool IsHostAllowed(const CString& sHostMask) const;
	bool IsValid(CString& sErrMsg, bool bSkipPass = false) const;
	static bool IsValidUserName(const CString& sUserName);
	static CString MakeCleanUserName(const CString& sUserName);

	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules

	// Networks
	CIRCNetwork* AddNetwork(const CString &sNetwork);
	bool DeleteNetwork(const CString& sNetwork);
	bool AddNetwork(CIRCNetwork *pNetwork);
	void RemoveNetwork(CIRCNetwork *pNetwork);
	CIRCNetwork* FindNetwork(const CString& sNetwork) const;
	const vector<CIRCNetwork*>& GetNetworks() const;
	// !Networks

	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutAllUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatusNotice(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModNotice(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	bool IsUserAttached() const;
	void UserConnected(CClient* pClient);
	void UserDisconnected(CClient* pClient);

	CString GetLocalDCCIP();

	CString ExpandString(const CString& sStr) const;
	CString& ExpandString(const CString& sStr, CString& sRet) const;

	CString AddTimestamp(const CString& sStr) const;
	CString AddTimestamp(time_t tm, const CString& sStr) const;

	void CloneNetworks(const CUser& User);
	bool Clone(const CUser& User, CString& sErrorRet, bool bCloneNetworks = true);
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
	void SetQuitMsg(const CString& s);
	bool AddCTCPReply(const CString& sCTCP, const CString& sReply);
	bool DelCTCPReply(const CString& sCTCP);
	bool SetBufferCount(unsigned int u, bool bForce = false);
	void SetKeepBuffer(bool b);

	void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }
	void SetTimestampFormat(const CString& s) { m_sTimestampFormat = s; }
	void SetTimestampAppend(bool b) { m_bAppendTimestamp = b; }
	void SetTimestampPrepend(bool b) { m_bPrependTimestamp = b; }
	void SetTimezone(const CString& s) { m_sTimezone = s; }
	void SetJoinTries(unsigned int i) { m_uMaxJoinTries = i; }
	void SetSkinName(const CString& s) { m_sSkinName = s; }
	// !Setters

	// Getters
	vector<CClient*>& GetUserClients() { return m_vClients; }
	vector<CClient*> GetAllClients();
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

	const CString& GetUserPath() const;

	bool DenyLoadMod() const;
	bool IsAdmin() const;
	bool DenySetBindHost() const;
	bool MultiClients() const;
	const CString& GetStatusPrefix() const;
	const CString& GetDefaultChanModes() const;

	CString GetQuitMsg() const;
	const MCString& GetCTCPReplies() const;
	unsigned int GetBufferCount() const;
	bool KeepBuffer() const;
	bool IsBeingDeleted() const { return m_bBeingDeleted; }
	CString GetTimezone() const { return m_sTimezone; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }
	unsigned int JoinTries() const { return m_uMaxJoinTries; }
	CString GetSkinName() const;
	// !Getters

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

	CString               m_sQuitMsg;
	MCString              m_mssCTCPReplies;
	CString               m_sTimestampFormat;
	CString               m_sTimezone;
	eHashType             m_eHashType;

	// Paths
	CString               m_sUserPath;
	// !Paths

	bool                  m_bMultiClients;
	bool                  m_bDenyLoadMod;
	bool                  m_bAdmin;
	bool                  m_bDenySetBindHost;
	bool                  m_bKeepBuffer;
	bool                  m_bBeingDeleted;
	bool                  m_bAppendTimestamp;
	bool                  m_bPrependTimestamp;

	CUserTimer*           m_pUserTimer;

	vector<CIRCNetwork*>  m_vIRCNetworks;
	vector<CClient*>      m_vClients;
	set<CString>          m_ssAllowedHosts;
	unsigned int          m_uBufferCount;
	unsigned long long    m_uBytesRead;
	unsigned long long    m_uBytesWritten;
	unsigned int          m_uMaxJoinTries;
	CString               m_sSkinName;

	CModules*             m_pModules;
};

#endif // !_USER_H
