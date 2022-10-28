/*
 * Copyright (C) 2004-2022 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_USER_H
#define ZNC_USER_H

#include <znc/zncconfig.h>
#include <znc/Utils.h>
#include <znc/Buffer.h>
#include <znc/Nick.h>
#include <znc/Translation.h>
#include <set>
#include <vector>

class CModules;
class CChan;
class CClient;
class CConfig;
class CFile;
class CIRCNetwork;
class CIRCSock;
class CUserTimer;
class CServer;

class CUser : private CCoreTranslationMixin {
  public:
    CUser(const CString& sUsername);
    ~CUser();

    CUser(const CUser&) = delete;
    CUser& operator=(const CUser&) = delete;

    bool ParseConfig(CConfig* Config, CString& sError);

    // TODO refactor this
    enum eHashType {
        HASH_NONE,
        HASH_MD5,
        HASH_SHA256,

        HASH_DEFAULT = HASH_SHA256
    };

    // If you change the default hash here and in HASH_DEFAULT,
    // don't forget CUtils::sDefaultHash!
    // TODO refactor this
    static CString SaltedHash(const CString& sPass, const CString& sSalt) {
        return CUtils::SaltedSHA256Hash(sPass, sSalt);
    }

    CConfig ToConfig() const;
    bool CheckPass(const CString& sPass) const;
    bool AddAllowedHost(const CString& sHostMask);
    bool RemAllowedHost(const CString& sHostMask);
    void ClearAllowedHosts();
    bool IsHostAllowed(const CString& sHost) const;
    bool IsValid(CString& sErrMsg, bool bSkipPass = false) const;
    static bool IsValidUsername(const CString& sUsername);
    /** @deprecated Use IsValidUsername() instead. */
    static bool IsValidUserName(const CString& sUsername);
    static CString MakeCleanUsername(const CString& sUsername);
    /** @deprecated Use MakeCleanUsername() instead. */
    static CString MakeCleanUserName(const CString& sUsername);

    // Modules
    CModules& GetModules() { return *m_pModules; }
    const CModules& GetModules() const { return *m_pModules; }
    // !Modules

    // Networks
    CIRCNetwork* AddNetwork(const CString& sNetwork, CString& sErrorRet);
    bool DeleteNetwork(const CString& sNetwork);
    bool AddNetwork(CIRCNetwork* pNetwork);
    void RemoveNetwork(CIRCNetwork* pNetwork);
    CIRCNetwork* FindNetwork(const CString& sNetwork) const;
    const std::vector<CIRCNetwork*>& GetNetworks() const;
    bool HasSpaceForNewNetwork() const;
    // !Networks

    bool PutUser(const CString& sLine, CClient* pClient = nullptr,
                 CClient* pSkipClient = nullptr);
    bool PutAllUser(const CString& sLine, CClient* pClient = nullptr,
                    CClient* pSkipClient = nullptr)
        ZNC_MSG_DEPRECATED("Use PutUser() instead") {
        return PutUser(sLine, pClient, pSkipClient);
    }
    bool PutStatus(const CString& sLine, CClient* pClient = nullptr,
                   CClient* pSkipClient = nullptr);
    bool PutStatusNotice(const CString& sLine, CClient* pClient = nullptr,
                         CClient* pSkipClient = nullptr);
    bool PutModule(const CString& sModule, const CString& sLine,
                   CClient* pClient = nullptr, CClient* pSkipClient = nullptr);
    bool PutModNotice(const CString& sModule, const CString& sLine,
                      CClient* pClient = nullptr,
                      CClient* pSkipClient = nullptr);

    bool IsUserAttached() const;
    void UserConnected(CClient* pClient);
    void UserDisconnected(CClient* pClient);

    CString GetLocalDCCIP() const;

    CString ExpandString(const CString& sStr) const;
    CString& ExpandString(const CString& sStr, CString& sRet) const;

    CString AddTimestamp(const CString& sStr) const;
    CString AddTimestamp(time_t tm, const CString& sStr) const;
    CString AddTimestamp(timeval tv, const CString& sStr) const;

    void CloneNetworks(const CUser& User);
    bool Clone(const CUser& User, CString& sErrorRet,
               bool bCloneNetworks = true);
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
    void SetDenySetIdent(bool b);
    void SetDenySetNetwork(bool b);
    void SetDenySetRealName(bool b);
    void SetDenySetQuitMsg(bool b);
    void SetDenySetCTCPReplies(bool b);
    bool SetStatusPrefix(const CString& s);
    void SetDefaultChanModes(const CString& s);
    void SetClientEncoding(const CString& s);
    void SetQuitMsg(const CString& s);
    bool AddCTCPReply(const CString& sCTCP, const CString& sReply);
    bool DelCTCPReply(const CString& sCTCP);
    /** @deprecated Use SetChanBufferSize() or SetQueryBufferSize() instead. */
    bool SetBufferCount(unsigned int u, bool bForce = false);
    bool SetChanBufferSize(unsigned int u, bool bForce = false);
    bool SetQueryBufferSize(unsigned int u, bool bForce = false);
    void SetAutoClearChanBuffer(bool b);
    void SetAutoClearQueryBuffer(bool b);
    bool SetLanguage(const CString& s);

    void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }
    void SetTimestampFormat(const CString& s) { m_sTimestampFormat = s; }
    void SetTimestampAppend(bool b) { m_bAppendTimestamp = b; }
    void SetTimestampPrepend(bool b) { m_bPrependTimestamp = b; }
    void SetAuthOnlyViaModule(bool b) { m_bAuthOnlyViaModule = b; }
    void SetTimezone(const CString& s) { m_sTimezone = s; }
    void SetJoinTries(unsigned int i) { m_uMaxJoinTries = i; }
    void SetMaxJoins(unsigned int i) { m_uMaxJoins = i; }
    void SetSkinName(const CString& s) { m_sSkinName = s; }
    void SetMaxNetworks(unsigned int i) { m_uMaxNetworks = i; }
    void SetMaxQueryBuffers(unsigned int i) { m_uMaxQueryBuffers = i; }
    void SetNoTrafficTimeout(unsigned int i) { m_uNoTrafficTimeout = i; }
    // !Setters

    // Getters
    const std::vector<CClient*>& GetUserClients() const { return m_vClients; }
    std::vector<CClient*> GetAllClients() const;
    /** @deprecated Use GetUsername() instead. */
    const CString& GetUserName() const;
    const CString& GetUsername() const;
    const CString& GetCleanUserName() const;
    const CString& GetNick(bool bAllowDefault = true) const;
    const CString& GetAltNick(bool bAllowDefault = true) const;
    const CString& GetIdent(bool bAllowDefault = true) const;
    CString GetRealName() const;
    const CString& GetBindHost() const;
    const CString& GetDCCBindHost() const;
    const CString& GetPass() const;
    eHashType GetPassHashType() const;
    const CString& GetPassSalt() const;
    const std::set<CString>& GetAllowedHosts() const;
    const CString& GetTimestampFormat() const;
    const CString& GetClientEncoding() const;
    bool GetTimestampAppend() const;
    bool GetTimestampPrepend() const;

    const CString& GetUserPath() const;

    bool DenyLoadMod() const;
    bool IsAdmin() const;
    bool DenySetBindHost() const;
    bool DenySetIdent() const;
    bool DenySetNetwork() const;
    bool DenySetRealName() const;
    bool DenySetQuitMsg() const;
    bool DenySetCTCPReplies() const;
    bool MultiClients() const;
    bool AuthOnlyViaModule() const;
    const CString& GetStatusPrefix() const;
    const CString& GetDefaultChanModes() const;
    /** How long must an IRC connection be idle before ZNC sends a ping */
    unsigned int GetPingFrequency() const { return m_uNoTrafficTimeout / 2; }
    /** Time between checks if PINGs need to be sent */
    unsigned int GetPingSlack() const { return m_uNoTrafficTimeout / 6; }
    /** Timeout after which IRC connections are closed. Must
     *  obviously be greater than GetPingFrequency() + GetPingSlack().
     */
    unsigned int GetNoTrafficTimeout() const { return m_uNoTrafficTimeout; }

    CString GetQuitMsg() const;
    const MCString& GetCTCPReplies() const;
    /** @deprecated Use GetChanBufferSize() or GetQueryBufferSize() instead. */
    unsigned int GetBufferCount() const;
    unsigned int GetChanBufferSize() const;
    unsigned int GetQueryBufferSize() const;
    bool AutoClearChanBuffer() const;
    bool AutoClearQueryBuffer() const;
    bool IsBeingDeleted() const { return m_bBeingDeleted; }
    CString GetTimezone() const { return m_sTimezone; }
    unsigned long long BytesRead() const;
    unsigned long long BytesWritten() const;
    unsigned int JoinTries() const { return m_uMaxJoinTries; }
    unsigned int MaxJoins() const { return m_uMaxJoins; }
    CString GetSkinName() const;
    CString GetLanguage() const;
    unsigned int MaxNetworks() const { return m_uMaxNetworks; }
    unsigned int MaxQueryBuffers() const { return m_uMaxQueryBuffers; }
    // !Getters

  protected:
    const CString m_sUsername;
    const CString m_sCleanUsername;
    CString m_sNick;
    CString m_sAltNick;
    CString m_sIdent;
    CString m_sRealName;
    CString m_sBindHost;
    CString m_sDCCBindHost;
    CString m_sPass;
    CString m_sPassSalt;
    CString m_sStatusPrefix;
    CString m_sDefaultChanModes;
    CString m_sClientEncoding;

    CString m_sQuitMsg;
    MCString m_mssCTCPReplies;
    CString m_sTimestampFormat;
    CString m_sTimezone;
    eHashType m_eHashType;

    // Paths
    CString m_sUserPath;
    // !Paths

    bool m_bMultiClients;
    bool m_bDenyLoadMod;
    bool m_bAdmin;
    bool m_bDenySetBindHost;
    bool m_bDenySetIdent;
    bool m_bDenySetNetwork;
    bool m_bDenySetRealName;
    bool m_bDenySetQuitMsg;
    bool m_bDenySetCTCPReplies;
    bool m_bAutoClearChanBuffer;
    bool m_bAutoClearQueryBuffer;
    bool m_bBeingDeleted;
    bool m_bAppendTimestamp;
    bool m_bPrependTimestamp;
    bool m_bAuthOnlyViaModule;

    CUserTimer* m_pUserTimer;

    std::vector<CIRCNetwork*> m_vIRCNetworks;
    std::vector<CClient*> m_vClients;
    std::set<CString> m_ssAllowedHosts;
    unsigned int m_uChanBufferSize;
    unsigned int m_uQueryBufferSize;
    unsigned long long m_uBytesRead;
    unsigned long long m_uBytesWritten;
    unsigned int m_uMaxJoinTries;
    unsigned int m_uMaxNetworks;
    unsigned int m_uMaxQueryBuffers;
    unsigned int m_uMaxJoins;
    unsigned int m_uNoTrafficTimeout;
    CString m_sSkinName;
    CString m_sLanguage;

    CModules* m_pModules;

  private:
    void SetKeepBuffer(bool b) {
        SetAutoClearChanBuffer(!b);
    }  // XXX compatibility crap, added in 0.207
    bool LoadModule(const CString& sModName, const CString& sArgs,
                    const CString& sNotice, CString& sError);
};

#endif  // !ZNC_USER_H
