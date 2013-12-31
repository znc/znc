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
	CIRCNetwork(CUser *pUser, const CIRCNetwork& Network);
	~CIRCNetwork();

	void Clone(const CIRCNetwork& Network, bool bCloneName = true);

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
	std::vector<CClient*>& GetClients() { return m_vClients; }

	void SetUser(CUser *pUser);
	bool SetName(const CString& sName);

	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules

	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	const std::vector<CChan*>& GetChans() const;
	CChan* FindChan(CString sName) const;
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName, bool bInConfig);
	bool DelChan(const CString& sName);
	void JoinChans();
	void JoinChans(std::set<CChan*>& sChans);

	const CString& GetChanPrefixes() const { return m_sChanPrefixes; };
	void SetChanPrefixes(const CString& s) { m_sChanPrefixes = s; };
	bool IsChan(const CString& sChan) const;

	const std::vector<CServer*>& GetServers() const;
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

	void SetIRCConnectEnabled(bool b);
	bool GetIRCConnectEnabled() const { return m_bIRCConnectEnabled; }

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
	void AddRawBuffer(const CString& sFormat, const CString& sText = "") { m_RawBuffer.AddLine(sFormat, sText); }
	void UpdateRawBuffer(const CString& sMatch, const CString& sFormat, const CString& sText = "") { m_RawBuffer.UpdateLine(sMatch, sFormat, sText); }
	void UpdateExactRawBuffer(const CString& sFormat, const CString& sText = "") { m_RawBuffer.UpdateExactLine(sFormat, sText); }
	void ClearRawBuffer() { m_RawBuffer.Clear(); }

	void AddMotdBuffer(const CString& sFormat, const CString& sText = "") { m_MotdBuffer.AddLine(sFormat, sText); }
	void UpdateMotdBuffer(const CString& sMatch, const CString& sFormat, const CString& sText = "") { m_MotdBuffer.UpdateLine(sMatch, sFormat, sText); }
	void ClearMotdBuffer() { m_MotdBuffer.Clear(); }

	void AddQueryBuffer(const CString& sFormat, const CString& sText = "") { m_QueryBuffer.AddLine(sFormat, sText); }
	void UpdateQueryBuffer(const CString& sMatch, const CString& sFormat, const CString& sText = "") { m_QueryBuffer.UpdateLine(sMatch, sFormat, sText); }
	void ClearQueryBuffer() { m_QueryBuffer.Clear(); }
	// !Buffers

	// la
	const CString& GetNick(const bool bAllowDefault = true) const;
	const CString& GetAltNick(const bool bAllowDefault = true) const;
	const CString& GetIdent(const bool bAllowDefault = true) const;
	const CString& GetRealName() const;
	const CString& GetBindHost() const;

	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);
	void SetBindHost(const CString& s);

	double GetFloodRate() const { return m_fFloodRate; }
	unsigned short int GetFloodBurst() const { return m_uFloodBurst; }
	void SetFloodRate(double fFloodRate) { m_fFloodRate = fFloodRate; }
	void SetFloodBurst(unsigned short int uFloodBurst) { m_uFloodBurst = uFloodBurst; }

	CString ExpandString(const CString& sStr) const;
	CString& ExpandString(const CString& sStr, CString& sRet) const;
private:
	bool JoinChan(CChan* pChan);

protected:
	CString            m_sName;
	CUser*             m_pUser;

	CString            m_sNick;
	CString            m_sAltNick;
	CString            m_sIdent;
	CString            m_sRealName;
	CString            m_sBindHost;

	CModules*          m_pModules;

	std::vector<CClient*>   m_vClients;

	CIRCSock*          m_pIRCSock;

	std::vector<CChan*>     m_vChans;

	CString            m_sChanPrefixes;

	bool               m_bIRCConnectEnabled;
	CString            m_sIRCServer;
	std::vector<CServer*>   m_vServers;
	size_t             m_uServerIdx; ///< Index in m_vServers of our current server + 1

	CNick              m_IRCNick;
	bool               m_bIRCAway;

	double             m_fFloodRate; ///< Set to -1 to disable protection.
	unsigned short int m_uFloodBurst;

	CBuffer            m_RawBuffer;
	CBuffer            m_MotdBuffer;
	CBuffer            m_QueryBuffer;
};

#endif // !_IRCNETWORK_H
