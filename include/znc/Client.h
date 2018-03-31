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

#ifndef ZNC_CLIENT_H
#define ZNC_CLIENT_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>
#include <znc/Utils.h>
#include <znc/Message.h>
#include <znc/main.h>
#include <memory>
#include <functional>

// Forward Declarations
class CZNC;
class CUser;
class CIRCNetwork;
class CIRCSock;
class CClient;
class CMessage;
class CChan;
// !Forward Declarations

class CAuthBase : private CCoreTranslationMixin {
  public:
    CAuthBase(const CString& sUsername, const CString& sPassword,
              CZNCSock* pSock)
        : m_sUsername(sUsername), m_sPassword(sPassword), m_pSock(pSock) {}

    virtual ~CAuthBase() {}

    CAuthBase(const CAuthBase&) = delete;
    CAuthBase& operator=(const CAuthBase&) = delete;

    virtual void SetLoginInfo(const CString& sUsername,
                              const CString& sPassword, CZNCSock* pSock) {
        m_sUsername = sUsername;
        m_sPassword = sPassword;
        m_pSock = pSock;
    }

    void AcceptLogin(CUser& User);
    void RefuseLogin(const CString& sReason);

    const CString& GetUsername() const { return m_sUsername; }
    const CString& GetPassword() const { return m_sPassword; }
    Csock* GetSocket() const { return m_pSock; }
    CString GetRemoteIP() const;

    // Invalidate this CAuthBase instance which means it will no longer use
    // m_pSock and AcceptLogin() or RefusedLogin() will have no effect.
    virtual void Invalidate();

  protected:
    virtual void AcceptedLogin(CUser& User) = 0;
    virtual void RefusedLogin(const CString& sReason) = 0;

  private:
    CString m_sUsername;
    CString m_sPassword;
    CZNCSock* m_pSock;
};

class CClientAuth : public CAuthBase {
  public:
    CClientAuth(CClient* pClient, const CString& sUsername,
                const CString& sPassword);
    virtual ~CClientAuth() {}

    CClientAuth(const CClientAuth&) = delete;
    CClientAuth& operator=(const CClientAuth&) = delete;

    void Invalidate() override {
        m_pClient = nullptr;
        CAuthBase::Invalidate();
    }
    void AcceptedLogin(CUser& User) override;
    void RefusedLogin(const CString& sReason) override;

  private:
  protected:
    CClient* m_pClient;
};

class CClient : public CIRCSocket {
  public:
    CClient()
        : CIRCSocket(),
          m_bGotPass(false),
          m_bGotNick(false),
          m_bGotUser(false),
          m_bInCap(false),
          m_bCapNotify(false),
          m_bAwayNotify(false),
          m_bAccountNotify(false),
          m_bExtendedJoin(false),
          m_bNamesx(false),
          m_bUHNames(false),
          m_bAway(false),
          m_bServerTime(false),
          m_bBatch(false),
          m_bEchoMessage(false),
          m_bSelfMessage(false),
          m_bPlaybackActive(false),
          m_pUser(nullptr),
          m_pNetwork(nullptr),
          m_sNick("unknown-nick"),
          m_sPass(""),
          m_sUser(""),
          m_sNetwork(""),
          m_sIdentifier(""),
          m_spAuth(),
          m_ssAcceptedCaps(),
          m_ssSupportedTags(),
          m_mCoreCaps({
              {"multi-prefix",
               {false, [this](bool bVal) { m_bNamesx = bVal; }}},
              {"userhost-in-names",
               {false, [this](bool bVal) { m_bUHNames = bVal; }}},
              {"echo-message",
               {false, [this](bool bVal) { m_bEchoMessage = bVal; }}},
              {"server-time",
               {false, [this](bool bVal) {
                m_bServerTime = bVal;
                SetTagSupport("time", bVal);
               }}},
              {"batch", {false, [this](bool bVal) {
                m_bBatch = bVal;
                SetTagSupport("batch", bVal);
              }}},
              {"cap-notify",
               {false, [this](bool bVal) { m_bCapNotify = bVal; }}},
              {"away-notify",
               {true, [this](bool bVal) { m_bAwayNotify = bVal; }}},
              {"account-notify",
               {true, [this](bool bVal) { m_bAccountNotify = bVal; }}},
              {"extended-join",
               {true, [this](bool bVal) { m_bExtendedJoin = bVal; }}},
          }) {
        EnableReadLine();
        // RFC says a line can have 512 chars max, but we are
        // a little more gentle ;)
        SetMaxBufferThreshold(1024);

        // For compatibility with older clients
        m_mCoreCaps["znc.in/server-time-iso"] = m_mCoreCaps["server-time"];
        m_mCoreCaps["znc.in/batch"] = m_mCoreCaps["batch"];
        m_mCoreCaps["znc.in/self-message"] = {
            false, [this](bool bVal) { m_bSelfMessage = bVal; }};
    }

    virtual ~CClient();

    CClient(const CClient&) = delete;
    CClient& operator=(const CClient&) = delete;

    void SendRequiredPasswordNotice();
    void AcceptLogin(CUser& User);
    void RefuseLogin(const CString& sReason);

    CString GetNick(bool bAllowIRCNick = true) const;
    CString GetNickMask() const;
    CString GetIdentifier() const { return m_sIdentifier; }
    bool HasCapNotify() const { return m_bCapNotify; }
    bool HasAwayNotify() const { return m_bAwayNotify; }
    bool HasAccountNotify() const { return m_bAccountNotify; }
    bool HasExtendedJoin() const { return m_bExtendedJoin; }
    bool HasNamesx() const { return m_bNamesx; }
    bool HasUHNames() const { return m_bUHNames; }
    bool IsAway() const { return m_bAway; }
    bool HasServerTime() const { return m_bServerTime; }
    bool HasBatch() const { return m_bBatch; }
    bool HasEchoMessage() const { return m_bEchoMessage; }
    bool HasSelfMessage() const { return m_bSelfMessage; }

    static bool IsValidIdentifier(const CString& sIdentifier);

    void UserCommand(CString& sLine);
    void UserPortCommand(CString& sLine);
    void StatusCTCP(const CString& sCommand);
    void BouncedOff();
    bool IsAttached() const { return m_pUser != nullptr; }

    bool IsPlaybackActive() const { return m_bPlaybackActive; }
    void SetPlaybackActive(bool bActive) { m_bPlaybackActive = bActive; }

    void PutIRC(const CString& sLine);
    /** Sends a raw data line to the client.
     *  @param sLine The line to be sent.
     *
     *  The line is first passed \e unmodified to the \ref
     *  CModule::OnSendToClient() module hook. If no module halts the process,
     *  the line is then sent to the client.
     *
     *  These lines appear in the debug output in the following syntax:
     *  \code [time] (user/network) ZNC -> CLI [line] \endcode
     *
     *  Prefer \l PutClient() instead.
     */
    bool PutClientRaw(const CString& sLine);
    /** Sends a message to the client.
     *  See \l PutClient(const CMessage&) for details.
     */
    void PutClient(const CString& sLine);
    /** Sends a message to the client.
     *  @param  Message The message to be sent.
     *  @note   Only known and compatible messages and tags are sent.
     *  @return \c true if the message was sent, or \c false if it was ignored.
     *
     *  This method ensures that only messages and tags, that the client has
     *  explicitly requested, are sent. Not all IRC clients are capable of
     *  handling all messages and tags. For example, some older versions of
     *  popular clients were prepared to parse just one interesting tag,
     *  \c time, and would break if multiple tags were included. Furthermore,
     *  messages that are specific to a certain capability, should not be sent
     *  to a client that has not requested the respective capability. Thus, in
     *  order to stay compatible with a variety of IRC clients, ZNC has to
     *  filter out messages and tags that the client has not explicitly
     *  requested.
     *
     *  ### Message types
     *
     *  The following table documents which capabilities the client is required
     *  to have requested in order to receive certain types of messages.
     *
     *  Message type | Capability
     *  ------------ | ----------
     *  \c ACCOUNT   | \l CClient::HasAccountNotify() (<a href="http://ircv3.net/specs/extensions/account-notify-3.1.html">account-notify</a>)
     *  \c AWAY      | \l CClient::HasAwayNotify() (<a href="http://ircv3.net/specs/extensions/away-notify-3.1.html">away-notify</a>)
     *
     *  ### Message tags
     *
     *  The following table documents currently supported message tags, and
     *  which capabilities the client is required to have requested to receive
     *  the respective message tags.
     *
     *  Message tag | Capability
     *  ----------- | ----------
     *  \c time     | \l CClient::HasServerTime() (<a href="http://ircv3.net/specs/extensions/server-time-3.2.html">server-time</a>)
     *  \c batch    | \l CClient::HasBatch() (<a href="http://ircv3.net/specs/extensions/batch-3.2.html">batch</a>)
     *  
     *  Additional tags can be added via \l CClient::SetTagSupport().
     *
     *  @warning Bypassing the filter may cause troubles to some older IRC
     *  clients.
     *
     *  It is possible to bypass the filter by converting a message to a string
     *  using \l CMessage::ToString(), and passing the resulting raw line to the
     *  \l CClient::PutClientRaw(const CString& sLine):
     *  \code
     *  pClient->PutClientRaw(Message.ToString());
     *  \endcode
     */
    bool PutClient(const CMessage& Message);
    unsigned int PutStatus(const CTable& table);
    void PutStatus(const CString& sLine);
    void PutStatusNotice(const CString& sLine);
    void PutModule(const CString& sModule, const CString& sLine);
    void PutModNotice(const CString& sModule, const CString& sLine);

    bool IsCapEnabled(const CString& sCap) const {
        return 1 == m_ssAcceptedCaps.count(sCap);
    }

    bool IsTagEnabled(const CString& sTag) const {
        return 1 == m_ssSupportedTags.count(sTag);
    }
    /** Registers a tag as being supported or unsupported by the client.
     *  This doesn't affect tags which the client sends.
     *  @param sTag The tag to register.
     *  @param bState Whether the client supports the tag.
     */
    void SetTagSupport(const CString& sTag, bool bState);

    void NotifyServerDependentCaps(const SCString& ssCaps);
    void ClearServerDependentCaps();

    void ReadLine(const CString& sData) override;
    bool SendMotd();
    void HelpUser(const CString& sFilter = "");
    void AuthUser();
    void Connected() override;
    void Timeout() override;
    void Disconnected() override;
    void ConnectionRefused() override;
    void ReachedMaxBuffer() override;

    void SetNick(const CString& s);
    void SetAway(bool bAway) { m_bAway = bAway; }
    CUser* GetUser() const { return m_pUser; }
    void SetNetwork(CIRCNetwork* pNetwork, bool bDisconnect = true,
                    bool bReconnect = true);
    CIRCNetwork* GetNetwork() const { return m_pNetwork; }
    const std::vector<CClient*>& GetClients() const;
    const CIRCSock* GetIRCSock() const;
    CIRCSock* GetIRCSock();
    CString GetFullName() const;

  private:
    void HandleCap(const CMessage& Message);
    void RespondCap(const CString& sResponse);
    void ParsePass(const CString& sAuthLine);
    void ParseUser(const CString& sAuthLine);
    void ParseIdentifier(const CString& sAuthLine);

    template <typename T>
    void AddBuffer(const T& Message);
    void EchoMessage(const CMessage& Message);

    std::set<CChan*> MatchChans(const CString& sPatterns) const;
    unsigned int AttachChans(const std::set<CChan*>& sChans);
    unsigned int DetachChans(const std::set<CChan*>& sChans);

    bool OnActionMessage(CActionMessage& Message);
    bool OnCTCPMessage(CCTCPMessage& Message);
    bool OnJoinMessage(CJoinMessage& Message);
    bool OnModeMessage(CModeMessage& Message);
    bool OnNoticeMessage(CNoticeMessage& Message);
    bool OnPartMessage(CPartMessage& Message);
    bool OnPingMessage(CMessage& Message);
    bool OnPongMessage(CMessage& Message);
    bool OnQuitMessage(CQuitMessage& Message);
    bool OnTextMessage(CTextMessage& Message);
    bool OnTopicMessage(CTopicMessage& Message);
    bool OnOtherMessage(CMessage& Message);

  protected:
    bool m_bGotPass;
    bool m_bGotNick;
    bool m_bGotUser;
    bool m_bInCap;
    bool m_bCapNotify;
    bool m_bAwayNotify;
    bool m_bAccountNotify;
    bool m_bExtendedJoin;
    bool m_bNamesx;
    bool m_bUHNames;
    bool m_bAway;
    bool m_bServerTime;
    bool m_bBatch;
    bool m_bEchoMessage;
    bool m_bSelfMessage;
    bool m_bPlaybackActive;
    CUser* m_pUser;
    CIRCNetwork* m_pNetwork;
    CString m_sNick;
    CString m_sPass;
    CString m_sUser;
    CString m_sNetwork;
    CString m_sIdentifier;
    std::shared_ptr<CAuthBase> m_spAuth;
    SCString m_ssAcceptedCaps;
    SCString m_ssSupportedTags;
    // The capabilities supported by the ZNC core - capability names mapped
    // to a pair which contains a bool describing whether the capability is
    // server-dependent, and a capability value change handler.
    std::map<CString, std::pair<bool, std::function<void(bool bVal)>>>
        m_mCoreCaps;
    // A subset of CIRCSock::GetAcceptedCaps(), the caps that can be listed
    // in CAP LS and may be notified to the client with CAP NEW (cap-notify).
    SCString m_ssServerDependentCaps;

    friend class ClientTest;
};

#endif  // !ZNC_CLIENT_H
