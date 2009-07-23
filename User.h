/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _USER_H
#define _USER_H

#include "Buffer.h"
#include "FileUtils.h"
#ifdef _MODULES
#include "Modules.h"
#endif
#include "Nick.h"
#include <set>
#include <vector>

using std::set;
using std::vector;

class CChan;
class CClient;
class CIRCSock;
class CJoinTimer;
class CMiscTimer;
class CServer;
class CDCCBounce;
class CDCCSock;

class CUser {
public:
	CUser(const CString& sUserName);
	~CUser();

	bool PrintLine(CFile& File, const CString& sName, const CString& sValue);
	bool WriteConfig(CFile& File);
	CChan* FindChan(const CString& sName) const;
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName, bool bInConfig);
	bool DelChan(const CString& sName);
	void JoinChans();
	CServer* FindServer(const CString& sName) const;
	bool DelServer(const CString& sName);
	bool AddServer(const CString& sName);
	bool AddServer(const CString& sName, unsigned short uPort, const CString& sPass = "", bool bSSL = false);
	CServer* GetNextServer();
	CServer* GetCurrentServer() const;
	bool CheckPass(const CString& sPass) const;
	bool AddAllowedHost(const CString& sHostMask);
	bool IsHostAllowed(const CString& sHostMask) const;
	bool IsValid(CString& sErrMsg, bool bSkipPass = false) const;
	static bool IsValidUserName(const CString& sUserName);
	static CString MakeCleanUserName(const CString& sUserName);
	bool IsLastServer() const;
	bool ConnectPaused();

	void DelClients();
	void DelServers();
#ifdef _MODULES
	void DelModules();

	// Unloads a module on all users who have it loaded and loads it again.
	static bool UpdateModule(const CString &sModule);

	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules
#endif

	// Buffers
	void AddRawBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_RawBuffer.AddLine(sPre, sPost, bIncNick); }
	void AddMotdBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_MotdBuffer.AddLine(sPre, sPost, bIncNick); }
	void AddQueryBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_QueryBuffer.AddLine(sPre, sPost, bIncNick); }
	void UpdateRawBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_RawBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void UpdateMotdBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_MotdBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void UpdateQueryBuffer(const CString& sPre, const CString& sPost, bool bIncNick = true) { m_QueryBuffer.UpdateLine(sPre, sPost, bIncNick); }
	void ClearRawBuffer() { m_RawBuffer.Clear(); }
	void ClearMotdBuffer() { m_MotdBuffer.Clear(); }
	void ClearQueryBuffer() { m_QueryBuffer.Clear(); }
	// !Buffers

	bool PutIRC(const CString& sLine);
	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatusNotice(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	bool IsUserAttached() const { return !m_vClients.empty(); }
	void UserConnected(CClient* pClient);
	void UserDisconnected(CClient* pClient);

	CString GetLocalIP();
	bool IsIRCConnected() const { return GetIRCSock() != NULL; }
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void CheckIRCConnect();

	void AddDCCBounce(CDCCBounce* p) { m_sDCCBounces.insert(p); }
	void DelDCCBounce(CDCCBounce* p) { m_sDCCBounces.erase(p); }
	void AddDCCSock(CDCCSock* p) { m_sDCCSocks.insert(p); }
	void DelDCCSock(CDCCSock* p) { m_sDCCSocks.erase(p); }

	CString ExpandString(const CString& sStr) const;
	CString& ExpandString(const CString& sStr, CString& sRet) const;

	CString AddTimestamp(const CString& sStr) const;
	CString& AddTimestamp(const CString& sStr, CString& sRet) const;

	bool SendFile(const CString& sRemoteNick, const CString& sFileName, const CString& sModuleName = "");
	bool GetFile(const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sFileName, unsigned long uFileSize, const CString& sModuleName = "");
	bool ResumeFile(unsigned short uPort, unsigned long uFileSize);
	CString GetCurNick() const;
	bool Clone(const CUser& User, CString& sErrorRet, bool bCloneChans = true);
	void BounceAllClients();

	void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
	void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }

	// Setters
	void SetUserName(const CString& s);
	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);
	void SetVHost(const CString& s);
	void SetPass(const CString& s, bool bHashed, const CString& sSalt = "");
	void SetBounceDCCs(bool b);
	void SetMultiClients(bool b);
	void SetUseClientIP(bool b);
	void SetDenyLoadMod(bool b);
	void SetAdmin(bool b);
	void SetDenySetVHost(bool b);
	bool SetStatusPrefix(const CString& s);
	void SetDefaultChanModes(const CString& s);
	void SetIRCNick(const CNick& n);
	void SetIRCServer(const CString& s);
	void SetQuitMsg(const CString& s);
	bool AddCTCPReply(const CString& sCTCP, const CString& sReply);
	void SetBufferCount(unsigned int u);
	void SetKeepBuffer(bool b);
	void SetChanPrefixes(const CString& s) { m_sChanPrefixes = s; }
	void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }
	void SetTimestampFormat(const CString& s) { m_sTimestampFormat = s; }
	void SetTimestampAppend(bool b) { m_bAppendTimestamp = b; }
	void SetTimestampPrepend(bool b) { m_bPrependTimestamp = b; }
	void SetTimezoneOffset(float b) { m_fTimezoneOffset = b; }
	void SetJoinTries(unsigned int i) { m_uMaxJoinTries = i; }
	void SetMaxJoins(unsigned int i) { m_uMaxJoins = i; }
	void SetIRCConnectEnabled(bool b) { m_bIRCConnectEnabled = b; }
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
	const CString& GetVHost() const;
	const CString& GetPass() const;
	bool IsPassHashed() const;
	const CString& GetPassSalt() const;
	const set<CString>& GetAllowedHosts() const;
	const CString& GetTimestampFormat() const;
	bool GetTimestampAppend() const;
	bool GetTimestampPrepend() const;
	bool GetIRCConnectEnabled() const { return m_bIRCConnectEnabled; }

	const CString& GetChanPrefixes() const { return m_sChanPrefixes; }
	bool IsChan(const CString& sChan) const;

	const CString& GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CDir::MakeDir(m_sUserPath); } return m_sUserPath; }
	const CString& GetDLPath() const { if (!CFile::Exists(m_sDLPath)) { CDir::MakeDir(m_sDLPath); } return m_sDLPath; }

	bool UseClientIP() const;
	bool DenyLoadMod() const;
	bool IsAdmin() const;
	bool DenySetVHost() const;
	bool BounceDCCs() const;
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
	bool HasServers() const { return m_vServers.size() > 0; }
	float GetTimezoneOffset() const { return m_fTimezoneOffset; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }
	unsigned int JoinTries() const { return m_uMaxJoinTries; }
	unsigned int MaxJoins() const { return m_uMaxJoins; }
	// !Getters
private:
protected:
	time_t			m_uConnectTime;
	CString			m_sUserName;
	CString			m_sCleanUserName;
	CString			m_sNick;
	CString			m_sAltNick;
	CString			m_sIdent;
	CString			m_sRealName;
	CString			m_sVHost;
	CString			m_sPass;
	CString			m_sPassSalt;
	CString			m_sStatusPrefix;
	CString			m_sDefaultChanModes;
	CString			m_sChanPrefixes;
	CNick			m_IRCNick;
	CString			m_sIRCServer;
	CString			m_sQuitMsg;
	MCString		m_mssCTCPReplies;
	CString			m_sTimestampFormat;
	float			m_fTimezoneOffset;

	// Paths
	CString			m_sUserPath;
	CString			m_sDLPath;
	// !Paths

	CBuffer				m_RawBuffer;
	CBuffer				m_MotdBuffer;
	CBuffer				m_QueryBuffer;
	bool				m_bMultiClients;
	bool				m_bBounceDCCs;
	bool				m_bPassHashed;
	bool				m_bUseClientIP;
	bool				m_bDenyLoadMod;
	bool				m_bAdmin;
	bool				m_bDenySetVHost;
	bool				m_bKeepBuffer;
	bool				m_bBeingDeleted;
	bool				m_bAppendTimestamp;
	bool				m_bPrependTimestamp;
	bool				m_bIRCConnectEnabled;
	CIRCSock*			m_pIRCSock;

	CJoinTimer*			m_pJoinTimer;
	CMiscTimer*			m_pMiscTimer;

	vector<CServer*>	m_vServers;
	vector<CChan*>		m_vChans;
	vector<CClient*>	m_vClients;
	set<CDCCBounce*>	m_sDCCBounces;
	set<CDCCSock*>		m_sDCCSocks;
	set<CString>		m_ssAllowedHosts;
	unsigned int		m_uServerIdx;
	unsigned int		m_uBufferCount;
	unsigned long long      m_uBytesRead;
	unsigned long long      m_uBytesWritten;
	unsigned int		m_uMaxJoinTries;
	unsigned int		m_uMaxJoins;

#ifdef _MODULES
	CModules*		m_pModules;
#endif
};

#endif // !_USER_H
