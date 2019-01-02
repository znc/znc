/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_IRCNETWORK_H
#define ZNC_IRCNETWORK_H

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
class CQuery;
class CServer;
class CIRCSock;
class CIRCNetworkPingTimer;
class CIRCNetworkJoinTimer;
class CMessage;

class CIRCNetwork : private CCoreTranslationMixin {
  public:
    static bool IsValidNetwork(const CString& sNetwork);

    CIRCNetwork(CUser* pUser, const CString& sName);
    CIRCNetwork(CUser* pUser, const CIRCNetwork& Network);
    ~CIRCNetwork();

    CIRCNetwork(const CIRCNetwork&) = delete;
    CIRCNetwork& operator=(const CIRCNetwork&) = delete;

    void Clone(const CIRCNetwork& Network, bool bCloneName = true);

    CString GetNetworkPath() const;

    void DelServers();

    bool ModuleInConfig(CConfig* pConfig, const CString& sModule) const;

    bool ParseConfig(CConfig* pConfig, CString& sError, bool bUpgrade = false);
    CConfig ToConfig() const;

    void BounceAllClients();

    bool IsUserAttached() const { return !m_vClients.empty(); }
    bool IsUserOnline() const;
    void ClientConnected(CClient* pClient);
    void ClientDisconnected(CClient* pClient);

    CUser* GetUser() const;
    const CString& GetName() const;
    bool IsNetworkAttached() const { return !m_vClients.empty(); }
    const std::vector<CClient*>& GetClients() const { return m_vClients; }
    std::vector<CClient*> FindClients(const CString& sIdentifier) const;

    void SetUser(CUser* pUser);
    bool SetName(const CString& sName);

    // Modules
    CModules& GetModules() { return *m_pModules; }
    const CModules& GetModules() const { return *m_pModules; }
    // !Modules

    bool PutUser(const CString& sLine, CClient* pClient = nullptr,
                 CClient* pSkipClient = nullptr);
    bool PutUser(const CMessage& Message, CClient* pClient = nullptr,
                 CClient* pSkipClient = nullptr);
    bool PutStatus(const CString& sLine, CClient* pClient = nullptr,
                   CClient* pSkipClient = nullptr);
    bool PutModule(const CString& sModule, const CString& sLine,
                   CClient* pClient = nullptr, CClient* pSkipClient = nullptr);

    const std::vector<CChan*>& GetChans() const;
    CChan* FindChan(CString sName) const;
    std::vector<CChan*> FindChans(const CString& sWild) const;
    bool AddChan(CChan* pChan);
    bool AddChan(const CString& sName, bool bInConfig);
    bool DelChan(const CString& sName);
    void JoinChans();
    void JoinChans(std::set<CChan*>& sChans);

    const std::vector<CQuery*>& GetQueries() const;
    CQuery* FindQuery(const CString& sName) const;
    std::vector<CQuery*> FindQueries(const CString& sWild) const;
    CQuery* AddQuery(const CString& sName);
    bool DelQuery(const CString& sName);

    const CString& GetChanPrefixes() const { return m_sChanPrefixes; }
    void SetChanPrefixes(const CString& s) { m_sChanPrefixes = s; }
    bool IsChan(const CString& sChan) const;

    const std::vector<CServer*>& GetServers() const;
    bool HasServers() const { return !m_vServers.empty(); }
    CServer* FindServer(const CString& sName) const;
    bool DelServer(const CString& sName, unsigned short uPort,
                   const CString& sPass);
    bool AddServer(const CString& sName);
    bool AddServer(const CString& sName, unsigned short uPort,
                   const CString& sPass = "", bool bSSL = false);
    CServer* GetNextServer(bool bAdvance = true);
    CServer* GetCurrentServer() const;
    void SetIRCServer(const CString& s);
    bool SetNextServer(const CServer* pServer);
    bool IsLastServer() const;

    const SCString& GetTrustedFingerprints() const {
        return m_ssTrustedFingerprints;
    }
    void AddTrustedFingerprint(const CString& sFP) {
        m_ssTrustedFingerprints.insert(
            sFP.Escape_n(CString::EHEXCOLON, CString::EHEXCOLON));
    }
    void DelTrustedFingerprint(const CString& sFP) {
        m_ssTrustedFingerprints.erase(sFP);
    }
    void ClearTrustedFingerprints() { m_ssTrustedFingerprints.clear(); }

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
    /** This method will return whether the user is connected and authenticated
     * to an IRC server.
     */
    bool IsIRCConnected() const;
    void SetIRCSocket(CIRCSock* pIRCSock);
    void IRCConnected();
    void IRCDisconnected();
    void CheckIRCConnect();

    bool PutIRC(const CString& sLine);
    bool PutIRC(const CMessage& Message);

    // Buffers
    void AddRawBuffer(const CMessage& Format, const CString& sText = "") {
        m_RawBuffer.AddLine(Format, sText);
    }
    void UpdateRawBuffer(const CString& sCommand, const CMessage& Format,
                         const CString& sText = "") {
        m_RawBuffer.UpdateLine(sCommand, Format, sText);
    }
    void UpdateExactRawBuffer(const CMessage& Format,
                              const CString& sText = "") {
        m_RawBuffer.UpdateExactLine(Format, sText);
    }
    void ClearRawBuffer() { m_RawBuffer.Clear(); }

    /// @deprecated
    void AddRawBuffer(const CString& sFormat, const CString& sText = "") {
        m_RawBuffer.AddLine(sFormat, sText);
    }
    /// @deprecated
    void UpdateRawBuffer(const CString& sMatch, const CString& sFormat,
                         const CString& sText = "") {
        m_RawBuffer.UpdateLine(sMatch, sFormat, sText);
    }
    /// @deprecated
    void UpdateExactRawBuffer(const CString& sFormat,
                              const CString& sText = "") {
        m_RawBuffer.UpdateExactLine(sFormat, sText);
    }

    void AddMotdBuffer(const CMessage& Format, const CString& sText = "") {
        m_MotdBuffer.AddLine(Format, sText);
    }
    void UpdateMotdBuffer(const CString& sCommand, const CMessage& Format,
                          const CString& sText = "") {
        m_MotdBuffer.UpdateLine(sCommand, Format, sText);
    }
    void ClearMotdBuffer() { m_MotdBuffer.Clear(); }

    /// @deprecated
    void AddMotdBuffer(const CString& sFormat, const CString& sText = "") {
        m_MotdBuffer.AddLine(sFormat, sText);
    }
    /// @deprecated
    void UpdateMotdBuffer(const CString& sMatch, const CString& sFormat,
                          const CString& sText = "") {
        m_MotdBuffer.UpdateLine(sMatch, sFormat, sText);
    }

    void AddNoticeBuffer(const CMessage& Format, const CString& sText = "") {
        m_NoticeBuffer.AddLine(Format, sText);
    }
    void UpdateNoticeBuffer(const CString& sCommand, const CMessage& Format,
                            const CString& sText = "") {
        m_NoticeBuffer.UpdateLine(sCommand, Format, sText);
    }
    void ClearNoticeBuffer() { m_NoticeBuffer.Clear(); }

    /// @deprecated
    void AddNoticeBuffer(const CString& sFormat, const CString& sText = "") {
        m_NoticeBuffer.AddLine(sFormat, sText);
    }
    /// @deprecated
    void UpdateNoticeBuffer(const CString& sMatch, const CString& sFormat,
                            const CString& sText = "") {
        m_NoticeBuffer.UpdateLine(sMatch, sFormat, sText);
    }

    void ClearQueryBuffer();
    // !Buffers

    // la
    const CString& GetNick(const bool bAllowDefault = true) const;
    const CString& GetAltNick(const bool bAllowDefault = true) const;
    const CString& GetIdent(const bool bAllowDefault = true) const;
    CString GetRealName() const;
    const CString& GetBindHost() const;
    const CString& GetEncoding() const;
    CString GetQuitMsg() const;

    void SetNick(const CString& s);
    void SetAltNick(const CString& s);
    void SetIdent(const CString& s);
    void SetRealName(const CString& s);
    void SetBindHost(const CString& s);
    void SetEncoding(const CString& s);
    void SetQuitMsg(const CString& s);

    double GetFloodRate() const { return m_fFloodRate; }
    unsigned short int GetFloodBurst() const { return m_uFloodBurst; }
    void SetFloodRate(double fFloodRate) { m_fFloodRate = fFloodRate; }
    void SetFloodBurst(unsigned short int uFloodBurst) {
        m_uFloodBurst = uFloodBurst;
    }

    unsigned short int GetJoinDelay() const { return m_uJoinDelay; }
    void SetJoinDelay(unsigned short int uJoinDelay) {
        m_uJoinDelay = uJoinDelay;
    }

    void SetTrustAllCerts(const bool bTrustAll = false) {
        m_bTrustAllCerts = bTrustAll;
    }
    bool GetTrustAllCerts() const { return m_bTrustAllCerts; }

    void SetTrustPKI(const bool bTrustPKI = true) { m_bTrustPKI = bTrustPKI; }
    bool GetTrustPKI() const { return m_bTrustPKI; }

    unsigned long long BytesRead() const { return m_uBytesRead; }
    unsigned long long BytesWritten() const { return m_uBytesWritten; }

    void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
    void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }

    CString ExpandString(const CString& sStr) const;
    CString& ExpandString(const CString& sStr, CString& sRet) const;

  private:
    bool JoinChan(CChan* pChan);
    bool LoadModule(const CString& sModName, const CString& sArgs,
                    const CString& sNotice, CString& sError);

  protected:
    CString m_sName;
    CUser* m_pUser;

    CString m_sNick;
    CString m_sAltNick;
    CString m_sIdent;
    CString m_sRealName;
    CString m_sBindHost;
    CString m_sEncoding;
    CString m_sQuitMsg;
    SCString m_ssTrustedFingerprints;

    CModules* m_pModules;

    std::vector<CClient*> m_vClients;

    CIRCSock* m_pIRCSock;

    std::vector<CChan*> m_vChans;
    std::vector<CQuery*> m_vQueries;

    CString m_sChanPrefixes;

    bool m_bIRCConnectEnabled;
    bool m_bTrustAllCerts;
    bool m_bTrustPKI;
    CString m_sIRCServer;
    std::vector<CServer*> m_vServers;
    size_t m_uServerIdx;  ///< Index in m_vServers of our current server + 1

    CNick m_IRCNick;
    bool m_bIRCAway;

    double m_fFloodRate;  ///< Set to -1 to disable protection.
    unsigned short int m_uFloodBurst;

    CBuffer m_RawBuffer;
    CBuffer m_MotdBuffer;
    CBuffer m_NoticeBuffer;

    CIRCNetworkPingTimer* m_pPingTimer;
    CIRCNetworkJoinTimer* m_pJoinTimer;

    unsigned short int m_uJoinDelay;
    unsigned long long m_uBytesRead;
    unsigned long long m_uBytesWritten;
};

#endif  // !ZNC_IRCNETWORK_H
