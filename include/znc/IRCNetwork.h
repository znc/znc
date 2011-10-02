/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _IRCNETWORK_H
#define _IRCNETWORK_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <znc/Buffer.h>
#include <znc/Nick.h>
#include <znc/znc.h>

class CModules;
class CUser;
class CFile;
class CConfig;
class CClient;
class CConfig;
class CChan;
class CServer;
class CIRCSock;

class CIRCNetwork {
public:
	static bool IsValidNetwork(const CString& sNetwork);

	CIRCNetwork(CUser *pUser, const CString& sName);
	CIRCNetwork(CUser *pUser, const CIRCNetwork *pNetwork, bool bCloneChans = true);
	~CIRCNetwork();

	CString GetNetworkPath();

	void DelServers();

	bool ParseConfig(CConfig *pConfig, CString& sError, bool bUpgrade = false);
	CConfig ToConfig();

	void BounceAllClients();

	bool IsUserAttached() const { return !m_vClients.empty(); }
	bool IsUserOnline() const;
	void ClientConnected(CClient *pClient);
	void ClientDisconnected(CClient *pClient);

	CUser* GetUser();
	const CString& GetName() const;
	bool IsNetworkAttached() const { return !m_vClients.empty(); };
	vector<CClient*>& GetClients() { return m_vClients; }

	void SetUser(CUser *pUser);
	bool SetName(const CString& sName);

	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules

	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	const vector<CChan*>& GetChans() const;
	CChan* FindChan(const CString& sName) const;
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName, bool bInConfig);
	bool DelChan(const CString& sName);
	void JoinChans();

	const CString& GetChanPrefixes() const { return m_sChanPrefixes; };
	void SetChanPrefixes(const CString& s) { m_sChanPrefixes = s; };
	bool IsChan(const CString& sChan) const;

	const vector<CServer*>& GetServers() const;
	bool HasServers() const { return !m_vServers.empty(); }
	CServer* FindServer(const CString& sName) const;
	bool DelServer(const CString& sName, unsigned short uPort, const CString& sPass);
	bool AddServer(const CString& sName);
	bool AddServer(const CString& sName, unsigned short uPort, const CString& sPass = "", bool bSSL = false);
	CServer* GetNextServer();
	CServer* GetCurrentServer() const;
	void SetIRCServer(const CString& s);
	bool SetNextServer(const CServer* pServer);
	bool IsLastServer() const;

	CIRCSock* GetIRCSock() { return m_pIRCSock; }
	const CIRCSock* GetIRCSock() const { return m_pIRCSock; }
	const CString& GetIRCServer() const;
	const CNick& GetIRCNick() const;
	void SetIRCNick(const CNick& n);
	CString GetCurNick() const;
	bool IsIRCAway() const { return m_bIRCAway; }
	void SetIRCAway(bool b) { m_bIRCAway = b; }

	bool Connect();
	/** This method will return whether the user is connected and authenticated to an IRC server.
	 */
	bool IsIRCConnected() const;
	void SetIRCSocket(CIRCSock* pIRCSock);
	void IRCDisconnected();
	void CheckIRCConnect();

	bool PutIRC(const CString& sLine);

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

	// la
	const CString& GetNick(const bool bAllowDefault = true) const;
	const CString& GetAltNick(const bool bAllowDefault = true) const;
	const CString& GetIdent(const bool bAllowDefault = true) const;
	const CString& GetRealName() const;

	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);

	CString ExpandString(const CString& sStr) const;
	CString& ExpandString(const CString& sStr, CString& sRet) const;
private:
	bool JoinChan(CChan* pChan);
	void JoinChans(set<CChan*>& sChans);

protected:
	CString            m_sName;
	CUser*             m_pUser;

	CString            m_sNick;
	CString            m_sAltNick;
	CString            m_sIdent;
	CString            m_sRealName;

	CModules*          m_pModules;

	vector<CClient*>   m_vClients;

	CIRCSock*          m_pIRCSock;

	vector<CChan*>     m_vChans;

	CString            m_sChanPrefixes;

	CString            m_sIRCServer;
	vector<CServer*>   m_vServers;
	unsigned int       m_uServerIdx; ///< Index in m_vServers of our current server + 1

	CNick              m_IRCNick;
	bool               m_bIRCAway;

	CBuffer            m_RawBuffer;
	CBuffer            m_MotdBuffer;
	CBuffer            m_QueryBuffer;
};

#endif // !_IRCNETWORK_H
