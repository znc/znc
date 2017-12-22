/*
 * Copyright (C) 2004-2017 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_H
#define ZNC_H

#include <znc/zncconfig.h>
#include <znc/Client.h>
#include <znc/Modules.h>
#include <znc/Socket.h>
#include <znc/Listener.h>
#include <znc/Translation.h>
#include <mutex>
#include <map>
#include <list>

class CListener;
class CUser;
class CIRCNetwork;
class CConnectQueueTimer;
class CConfigWriteTimer;
class CConfig;
class CFile;

class CZNC {
  public:
    CZNC();
    ~CZNC();

    CZNC(const CZNC&) = delete;
    CZNC& operator=(const CZNC&) = delete;

    enum ConfigState {
        ECONFIG_NOTHING,
        ECONFIG_NEED_REHASH,
        ECONFIG_NEED_WRITE,
        ECONFIG_NEED_VERBOSE_WRITE,
        ECONFIG_DELAYED_WRITE,
        ECONFIG_NEED_QUIT,  // Not really config...
    };

    void DeleteUsers();
    void Loop();
    bool WritePidFile(int iPid);
    bool DeletePidFile();
    bool WaitForChildLock();
    bool IsHostAllowed(const CString& sHostMask) const;
    // This returns false if there are too many anonymous connections from this
    // ip
    bool AllowConnectionFrom(const CString& sIP) const;
    void InitDirs(const CString& sArgvPath, const CString& sDataDir);
    bool OnBoot();
    CString ExpandConfigPath(const CString& sConfigFile,
                             bool bAllowMkDir = true);
    bool WriteNewConfig(const CString& sConfigFile);
    bool WriteConfig();
    bool ParseConfig(const CString& sConfig, CString& sError);
    bool RehashConfig(CString& sError);
    void BackupConfigOnce(const CString& sSuffix);
    static CString GetVersion();
    static CString GetTag(bool bIncludeVersion = true, bool bHTML = false);
    static CString GetCompileOptionsString();
    CString GetUptime() const;
    /** @deprecated Since 1.7.0. List of allowed bind hosts was a flawed design. */
    void ClearBindHosts() {}
    /** @deprecated Since 1.7.0. List of allowed bind hosts was a flawed design. */
    bool AddBindHost(const CString& sHost) { return false; }
    /** @deprecated Since 1.7.0. List of allowed bind hosts was a flawed design. */
    bool RemBindHost(const CString& sHost) { return false; }
    void ClearTrustedProxies();
    bool AddTrustedProxy(const CString& sHost);
    bool RemTrustedProxy(const CString& sHost);
    void Broadcast(const CString& sMessage, bool bAdminOnly = false,
                   CUser* pSkipUser = nullptr, CClient* pSkipClient = nullptr);
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
    TrafficStatsMap GetTrafficStats(TrafficStatsPair& Users,
                                    TrafficStatsPair& ZNC,
                                    TrafficStatsPair& Total);
    TrafficStatsMap GetNetworkTrafficStats(const CString& sUsername,
                                           TrafficStatsPair& Total);

    // Authenticate a user.
    // The result is passed back via callbacks to CAuthBase.
    void AuthUser(std::shared_ptr<CAuthBase> AuthClass);

    // Setters
    void SetConfigState(enum ConfigState e) {
        std::lock_guard<std::mutex> guard(m_mutexConfigState);
        m_eConfigState = e;
    }
    void SetSkinName(const CString& s) { m_sSkinName = s; }
    void SetStatusPrefix(const CString& s) {
        m_sStatusPrefix = (s.empty()) ? "*" : s;
    }
    void SetMaxBufferSize(unsigned int i) { m_uiMaxBufferSize = i; }
    void SetAnonIPLimit(unsigned int i) { m_uiAnonIPLimit = i; }
    void SetServerThrottle(unsigned int i) {
        m_sConnectThrottle.SetTTL(i * 1000);
    }
    void SetProtectWebSessions(bool b) { m_bProtectWebSessions = b; }
    void SetHideVersion(bool b) { m_bHideVersion = b; }
    void SetAuthOnlyViaModule(bool b) { m_bAuthOnlyViaModule = b; }
    void SetConnectDelay(unsigned int i);
    void SetSSLCiphers(const CString& sCiphers) { m_sSSLCiphers = sCiphers; }
    bool SetSSLProtocols(const CString& sProtocols);
    void SetSSLCertFile(const CString& sFile) { m_sSSLCertFile = sFile; }
    void SetConfigWriteDelay(unsigned int i) { m_uiConfigWriteDelay = i; }
    // !Setters

    // Getters
    enum ConfigState GetConfigState() {
        std::lock_guard<std::mutex> guard(m_mutexConfigState);
        return m_eConfigState;
    }
    CSockManager& GetManager() { return m_Manager; }
    const CSockManager& GetManager() const { return m_Manager; }
    CModules& GetModules() { return *m_pModules; }
    CString GetSkinName() const { return m_sSkinName; }
    const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
    const CString& GetCurPath() const;
    const CString& GetHomePath() const;
    const CString& GetZNCPath() const;
    CString GetConfPath(bool bAllowMkDir = true) const;
    CString GetUserPath() const;
    CString GetModPath() const;
    CString GetPemLocation() const;
    CString GetKeyLocation() const;
    CString GetDHParamLocation() const;
    const CString& GetConfigFile() const { return m_sConfigFile; }
    bool WritePemFile();
    /** @deprecated Since 1.7.0. List of allowed bind hosts was a flawed design. */
    const VCString& GetBindHosts() const { return m_vsBindHosts; }
    const VCString& GetTrustedProxies() const { return m_vsTrustedProxies; }
    const std::vector<CListener*>& GetListeners() const {
        return m_vpListeners;
    }
    time_t TimeStarted() const { return m_TimeStarted; }
    unsigned int GetMaxBufferSize() const { return m_uiMaxBufferSize; }
    unsigned int GetAnonIPLimit() const { return m_uiAnonIPLimit; }
    unsigned int GetServerThrottle() const {
        return m_sConnectThrottle.GetTTL() / 1000;
    }
    unsigned int GetConnectDelay() const { return m_uiConnectDelay; }
    bool GetProtectWebSessions() const { return m_bProtectWebSessions; }
    bool GetHideVersion() const { return m_bHideVersion; }
    bool GetAuthOnlyViaModule() const { return m_bAuthOnlyViaModule; }
    CString GetSSLCiphers() const { return m_sSSLCiphers; }
    CString GetSSLProtocols() const { return m_sSSLProtocols; }
    Csock::EDisableProtocol GetDisabledSSLProtocols() const {
        return static_cast<Csock::EDisableProtocol>(m_uDisabledSSLProtocols);
    }
    CString GetSSLCertFile() const { return m_sSSLCertFile; }
    static VCString GetAvailableSSLProtocols();
    unsigned int GetConfigWriteDelay() const { return m_uiConfigWriteDelay; }
    // !Getters

    // Static allocator
    static void CreateInstance();
    static CZNC& Get();
    static void DestroyInstance();
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
    bool UpdateModule(const CString& sModule);

    bool DeleteUser(const CString& sUsername);
    bool AddUser(CUser* pUser, CString& sErrorRet, bool bStartup = false);
    const std::map<CString, CUser*>& GetUserMap() const { return (m_msUsers); }

    // Listener yummy
    CListener* FindListener(u_short uPort, const CString& BindHost,
                            EAddrType eAddr);
    bool AddListener(CListener*);
    bool AddListener(unsigned short uPort, const CString& sBindHost,
                     const CString& sURIPrefix, bool bSSL, EAddrType eAddr,
                     CListener::EAcceptType eAccept, CString& sError);
    bool DelListener(CListener*);

    // Message of the Day
    void SetMotd(const CString& sMessage) {
        ClearMotd();
        AddMotd(sMessage);
    }
    void AddMotd(const CString& sMessage) {
        if (!sMessage.empty()) {
            m_vsMotd.push_back(sMessage);
        }
    }
    void ClearMotd() { m_vsMotd.clear(); }
    const VCString& GetMotd() const { return m_vsMotd; }
    // !MOTD

    void AddServerThrottle(CString sName) {
        m_sConnectThrottle.AddItem(sName, true);
    }
    bool GetServerThrottle(CString sName) {
        bool* b = m_sConnectThrottle.GetItem(sName);
        return (b && *b);
    }

    void AddNetworkToQueue(CIRCNetwork* pNetwork);
    std::list<CIRCNetwork*>& GetConnectionQueue() { return m_lpConnectQueue; }

    // This creates a CConnectQueueTimer if we haven't got one yet
    void EnableConnectQueue();
    void DisableConnectQueue();

    void PauseConnectQueue();
    void ResumeConnectQueue();

    void ForceEncoding();
    void UnforceEncoding();
    bool IsForcingEncoding() const;
    CString FixupEncoding(const CString& sEncoding) const;

    // Never call this unless you are CConnectQueueTimer::~CConnectQueueTimer()
    void LeakConnectQueueTimer(CConnectQueueTimer* pTimer);

    void DisableConfigTimer();

    static void DumpConfig(const CConfig* Config);

  private:
    CFile* InitPidFile();

    bool ReadConfig(CConfig& config, CString& sError);
    bool LoadGlobal(CConfig& config, CString& sError);
    bool LoadUsers(CConfig& config, CString& sError);
    bool LoadListeners(CConfig& config, CString& sError);
    void UnloadRemovedModules(const MCString& msModules);

    bool HandleUserDeletion();
    CString MakeConfigHeader();
    bool AddListener(const CString& sLine, CString& sError);
    bool AddListener(CConfig* pConfig, CString& sError);

  protected:
    time_t m_TimeStarted;

    enum ConfigState m_eConfigState;
    std::mutex m_mutexConfigState;

    std::vector<CListener*> m_vpListeners;
    std::map<CString, CUser*> m_msUsers;
    std::map<CString, CUser*> m_msDelUsers;
    CSockManager m_Manager;

    CString m_sCurPath;
    CString m_sZNCPath;

    CString m_sConfigFile;
    CString m_sSkinName;
    CString m_sStatusPrefix;
    CString m_sPidFile;
    CString m_sSSLCertFile;
    CString m_sSSLKeyFile;
    CString m_sSSLDHParamFile;
    CString m_sSSLCiphers;
    CString m_sSSLProtocols;
    VCString m_vsBindHosts;  // TODO: remove (deprecated in 1.7.0)
    VCString m_vsTrustedProxies;
    VCString m_vsMotd;
    CFile* m_pLockFile;
    unsigned int m_uiConnectDelay;
    unsigned int m_uiAnonIPLimit;
    unsigned int m_uiMaxBufferSize;
    unsigned int m_uDisabledSSLProtocols;
    CModules* m_pModules;
    unsigned long long m_uBytesRead;
    unsigned long long m_uBytesWritten;
    std::list<CIRCNetwork*> m_lpConnectQueue;
    CConnectQueueTimer* m_pConnectQueueTimer;
    unsigned int m_uiConnectPaused;
    unsigned int m_uiForceEncoding;
    TCacheMap<CString> m_sConnectThrottle;
    bool m_bProtectWebSessions;
    bool m_bHideVersion;
    bool m_bAuthOnlyViaModule;
    CTranslationDomainRefHolder m_Translation;
    unsigned int m_uiConfigWriteDelay;
    CConfigWriteTimer* m_pConfigTimer;
};

#endif  // !ZNC_H
