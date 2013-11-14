/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#ifndef _ZNC_H
#define _ZNC_H

#include <znc/zncconfig.h>
#include <znc/Client.h>
#include <znc/Modules.h>
#include <znc/Socket.h>
#include <znc/Listener.h>
#include <map>
#include <list>

class CListener;
class CUser;
class CIRCNetwork;
class CConnectQueueTimer;
class CConfig;
class CFile;

class CZNC {
public:
	CZNC();
	~CZNC();

	enum ConfigState {
		ECONFIG_NOTHING,
		ECONFIG_NEED_REHASH,
		ECONFIG_NEED_WRITE
	};

	void DeleteUsers();
	void Loop();
	bool WritePidFile(int iPid);
	bool DeletePidFile();
	bool WaitForChildLock();
	bool IsHostAllowed(const CString& sHostMask) const;
	// This returns false if there are too many anonymous connections from this ip
	bool AllowConnectionFrom(const CString& sIP) const;
	void InitDirs(const CString& sArgvPath, const CString& sDataDir);
	bool OnBoot();
	CString ExpandConfigPath(const CString& sConfigFile, bool bAllowMkDir = true);
	bool WriteNewConfig(const CString& sConfigFile);
	bool WriteConfig();
	bool ParseConfig(const CString& sConfig);
	bool RehashConfig(CString& sError);
	void BackupConfigOnce(const CString& sSuffix);
	static CString GetVersion();
	static CString GetTag(bool bIncludeVersion = true, bool bHTML = false);
	static CString GetCompileOptionsString();
	CString GetUptime() const;
	void ClearBindHosts();
	bool AddBindHost(const CString& sHost);
	bool RemBindHost(const CString& sHost);
	void ClearTrustedProxies();
	bool AddTrustedProxy(const CString& sHost);
	bool RemTrustedProxy(const CString& sHost);
	void Broadcast(const CString& sMessage, bool bAdminOnly = false,
			CUser* pSkipUser = NULL, CClient* pSkipClient = NULL);
	void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
	void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }

	// Traffic fun
	typedef std::pair<unsigned long long, unsigned long long> TrafficStatsPair;
	typedef std::map<CString, TrafficStatsPair> TrafficStatsMap;
	// Returns a map which maps user names to <traffic in, traffic out>
	// while also providing the traffic of all users together, traffic which
	// couldn't be accounted to any particular user and the total traffic
	// generated through ZNC.
	TrafficStatsMap GetTrafficStats(TrafficStatsPair &Users,
			TrafficStatsPair &ZNC, TrafficStatsPair &Total);

	// Authenticate a user.
	// The result is passed back via callbacks to CAuthBase.
	// CSmartPtr handles freeing this pointer!
	void AuthUser(CSmartPtr<CAuthBase> AuthClass);

	// Setters
	void SetConfigState(enum ConfigState e) { m_eConfigState = e; }
	void SetSkinName(const CString& s) { m_sSkinName = s; }
	void SetStatusPrefix(const CString& s) { m_sStatusPrefix = (s.empty()) ? "*" : s; }
	void SetMaxBufferSize(unsigned int i) { m_uiMaxBufferSize = i; }
	void SetAnonIPLimit(unsigned int i) { m_uiAnonIPLimit = i; }
	void SetServerThrottle(unsigned int i) { m_sConnectThrottle.SetTTL(i*1000); }
	void SetProtectWebSessions(bool b) { m_bProtectWebSessions = b; }
	void SetConnectDelay(unsigned int i);
	// !Setters

	// Getters
	enum ConfigState GetConfigState() const { return m_eConfigState; }
	CSockManager& GetManager() { return m_Manager; }
	const CSockManager& GetManager() const { return m_Manager; }
	CModules& GetModules() { return *m_pModules; }
	size_t FilterUncommonModules(std::set<CModInfo>& ssModules);
	CString GetSkinName() const { return m_sSkinName; }
	const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
	const CString& GetCurPath() const;
	const CString& GetHomePath() const;
	const CString& GetZNCPath() const;
	CString GetConfPath(bool bAllowMkDir = true) const;
	CString GetUserPath() const;
	CString GetModPath() const;
	CString GetPemLocation() const;
	const CString& GetConfigFile() const { return m_sConfigFile; }
	bool WritePemFile();
	const VCString& GetBindHosts() const { return m_vsBindHosts; }
	const VCString& GetTrustedProxies() const { return m_vsTrustedProxies; }
	const std::vector<CListener*>& GetListeners() const { return m_vpListeners; }
	time_t TimeStarted() const { return m_TimeStarted; }
	unsigned int GetMaxBufferSize() const { return m_uiMaxBufferSize; }
	unsigned int GetAnonIPLimit() const { return m_uiAnonIPLimit; }
	unsigned int GetServerThrottle() const { return m_sConnectThrottle.GetTTL() / 1000; }
	unsigned int GetConnectDelay() const { return m_uiConnectDelay; }
	bool GetProtectWebSessions() const { return m_bProtectWebSessions; }
	// !Getters

	// Static allocator
	static CZNC& Get();
	CUser* FindUser(const CString& sUsername);
	CModule* FindModule(const CString& sModName, const CString& sUsername);
	CModule* FindModule(const CString& sModName, CUser* pUser);

	/** Reload a module everywhere
	 *
	 * This method will unload a module globally, for a user and for each
	 * network. It will then reload them all again.
	 *
	 * @param sModule The name of the module to reload
	 */
	bool UpdateModule(const CString &sModule);

	bool DeleteUser(const CString& sUsername);
	bool AddUser(CUser* pUser, CString& sErrorRet);
	const std::map<CString,CUser*> & GetUserMap() const { return(m_msUsers); }

	// Listener yummy
	CListener* FindListener(u_short uPort, const CString& BindHost, EAddrType eAddr);
	bool AddListener(CListener*);
	bool AddListener(unsigned short uPort, const CString& sBindHost, bool bSSL,
			EAddrType eAddr, CListener::EAcceptType eAccept, CString& sError);
	bool DelListener(CListener*);

	// Message of the Day
	void SetMotd(const CString& sMessage) { ClearMotd(); AddMotd(sMessage); }
	void AddMotd(const CString& sMessage) { if (!sMessage.empty()) { m_vsMotd.push_back(sMessage); } }
	void ClearMotd() { m_vsMotd.clear(); }
	const VCString& GetMotd() const { return m_vsMotd; }
	// !MOTD

	void AddServerThrottle(CString sName) { m_sConnectThrottle.AddItem(sName); }
	bool GetServerThrottle(CString sName) { return m_sConnectThrottle.GetItem(sName); }

	void AddNetworkToQueue(CIRCNetwork *pNetwork);
	std::list<CIRCNetwork*>& GetConnectionQueue() { return m_lpConnectQueue; }

	// This creates a CConnectQueueTimer if we haven't got one yet
	void EnableConnectQueue();
	void DisableConnectQueue();

	void PauseConnectQueue();
	void ResumeConnectQueue();

	// Never call this unless you are CConnectQueueTimer::~CConnectQueueTimer()
	void LeakConnectQueueTimer(CConnectQueueTimer *pTimer);

	static void DumpConfig(const CConfig* Config);

private:
	CFile* InitPidFile();
	bool DoRehash(CString& sError);
	// Returns true if something was done
	bool HandleUserDeletion();
	CString MakeConfigHeader();
	bool AddListener(const CString& sLine, CString& sError);
	bool AddListener(CConfig* pConfig, CString& sError);

protected:
	time_t                 m_TimeStarted;

	enum ConfigState       m_eConfigState;
	std::vector<CListener*>     m_vpListeners;
	std::map<CString,CUser*> m_msUsers;
	std::map<CString,CUser*> m_msDelUsers;
	CSockManager           m_Manager;

	CString                m_sCurPath;
	CString                m_sZNCPath;

	CString                m_sConfigFile;
	CString                m_sSkinName;
	CString                m_sStatusPrefix;
	CString                m_sPidFile;
	CString                m_sSSLCertFile;
	VCString               m_vsBindHosts;
	VCString               m_vsTrustedProxies;
	VCString               m_vsMotd;
	CFile*                 m_pLockFile;
	unsigned int           m_uiConnectDelay;
	unsigned int           m_uiAnonIPLimit;
	unsigned int           m_uiMaxBufferSize;
	CModules*              m_pModules;
	unsigned long long     m_uBytesRead;
	unsigned long long     m_uBytesWritten;
	std::list<CIRCNetwork*>     m_lpConnectQueue;
	CConnectQueueTimer    *m_pConnectQueueTimer;
	unsigned int           m_uiConnectPaused;
	TCacheMap<CString>     m_sConnectThrottle;
	bool                   m_bProtectWebSessions;
};

#endif // !_ZNC_H
