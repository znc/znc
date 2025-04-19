/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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
    CClient();
    virtual ~CClient();

    CClient(const CClient&) = delete;
    CClient& operator=(const CClient&) = delete;

    void SendRequiredPasswordNotice();
    void AcceptLogin(CUser& User);
    void RefuseLogin(const CString& sReason);

    CString GetNick(bool bAllowIRCNick = true) const;
    CString GetNickMask() const;
    CString GetIdentifier() const { return m_sIdentifier; }
    unsigned short int CapVersion() const { return m_uCapVersion; }
    bool HasCap302() const { return CapVersion() >= 302; }
    bool HasCapNotify() const { return m_bCapNotify; }
    bool HasAwayNotify() const { return m_bAwayNotify; }
    bool HasAccountNotify() const { return m_bAccountNotify; }
    bool HasInviteNotify() const { return m_bInviteNotify; }
    bool HasExtendedJoin() const { return m_bExtendedJoin; }
    bool HasNamesx() const { return m_bNamesx; }
    bool HasUHNames() const { return m_bUHNames; }
    bool HasChgHost() const { return m_bChgHost; }
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
    // Strips prefix and potentially tags before sending to server.
    void PutIRCStripping(CMessage Message);
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
     *  \c ACCOUNT   | \l CClient::HasAccountNotify() (<a href="http://ircv3.net/specs/extensions/account-notify">account-notify</a>)
     *  \c AWAY      | \l CClient::HasAwayNotify() (<a href="http://ircv3.net/specs/extensions/away-notify">away-notify</a>)
     *  \c INVITE    | \l CClient::HasInviteNotify() (<a href="http://ircv3.net/specs/extensions/invite-notify">invite-notify</a>) if someone else is invited; invites sent to this user are not filtered out regardless of any capability.
     *  \c TAGMSG    | \l CClient::HasMessageTagCap() (<a href="http://ircv3.net/specs/extensions/message-tags">message-tags</a>)
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
     *  any tag     | \l CClient::HasMessageTagCap() (<a href="http://ircv3.net/specs/extensions/message-tags">message-tags</a>)
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

    /** Notifies client about one specific cap which server has just notified us about.
     */
    void NotifyServerDependentCap(const CString& sCap, bool bValue, const CString& sValue);

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

    /** Sends AUTHENTIATE message to client.
     * It encodes it to Base64 and splits to multiple IRC messages if necessary.
     */
    void SendSASLChallenge(CString sMessage);
    void RefuseSASLLogin(const CString& sReason);
    void AcceptSASLLogin(CUser& User);
    /** Start potentially asynchronous process of checking the credentials.
     * When finished, will send the success/failure SASL numerics to the
     * client. This is mostly useful for SASL PLAIN.
     * sAuthorizationId is internally passed through ParseUser() to extract
     * network and client id.
     * Currently sUser should match the username from
     * sAuthorizationId: either in full, or just the username part; but in a
     * future version we may add an ability to actually login as a different
     * user, but with your password.
     */
    void StartSASLPasswordCheck(const CString& sUser, const CString& sPassword,
                                const CString& sAuthorizationId);
    /** Gathers username, client id, network name, if present. Returns username
     * cleaned from client id and network name.
     */
    CString ParseUser(const CString& sAuthLine);

  private:
    void HandleCap(const CMessage& Message);
    void RespondCap(const CString& sResponse);
    void ParsePass(const CString& sAuthLine);
    void ParseIdentifier(const CString& sAuthLine);

    template <typename T>
    void AddBuffer(const T& Message);
    void EchoMessage(const CMessage& Message);

    std::set<CChan*> MatchChans(const CString& sPatterns) const;
    unsigned int AttachChans(const std::set<CChan*>& sChans);
    unsigned int DetachChans(const std::set<CChan*>& sChans);

    bool OnActionMessage(CActionMessage& Message);
    void OnAuthenticateMessage(const CAuthenticateMessage& Message);
    void AbortSASL(const CString& sFullIRCLine);
    bool IsDuringSASL() const { return !m_sSASLMechanism.empty(); }

    /**
     * Returns set of all available SASL mechanisms.
     */
    SCString EnumerateSASLMechanisms() const;

    bool OnCTCPMessage(CCTCPMessage& Message);
    bool OnJoinMessage(CJoinMessage& Message);
    bool OnModeMessage(CModeMessage& Message);
    bool OnNoticeMessage(CNoticeMessage& Message);
    bool OnPartMessage(CPartMessage& Message);
    bool OnPingMessage(CMessage& Message);
    bool OnPongMessage(CMessage& Message);
    bool OnQuitMessage(CQuitMessage& Message);
    bool OnTagMessage(CTargetMessage& Message);
    bool OnTextMessage(CTextMessage& Message);
    bool OnTopicMessage(CTopicMessage& Message);
    bool OnOtherMessage(CMessage& Message);

  protected:
    bool m_bGotPass;
    bool m_bGotNick;
    bool m_bGotUser;
    unsigned short int m_uCapVersion;
    bool m_bInCap;
    bool m_bCapNotify;
    bool m_bAwayNotify;
    bool m_bAccountNotify;
    bool m_bInviteNotify;
    bool m_bExtendedJoin;
    bool m_bNamesx;
    bool m_bUHNames;
    bool m_bChgHost;
    bool m_bAway;
    bool m_bServerTime;
    bool m_bBatch;
    bool m_bEchoMessage;
    bool m_bSelfMessage;
    bool m_bMessageTagCap;
    bool m_bSASLCap;
    bool m_bPlaybackActive;
    CUser* m_pUser;
    CIRCNetwork* m_pNetwork;
    CString m_sNick;
    CString m_sPass;
    // User who didn't necessarily login yet, or might not even exist.
    CString m_sUser;
    CString m_sNetwork;
    CString m_sIdentifier;
    CString m_sSASLBuffer;
    // Set while the exchange is in progress
    CString m_sSASLMechanism;
    // Username who successfully logged in using SASL. This is not a CUser*
    // because between the 903 and CAP END the user could have been deleted.
    CString m_sSASLUser;
    std::shared_ptr<CAuthBase> m_spAuth;
    SCString m_ssAcceptedCaps;
    SCString m_ssSupportedTags;
    SCString m_ssPreviouslyFailedSASLMechanisms;
    // The capabilities supported by the ZNC core - capability names mapped to
    // change handler. Note: this lists caps which don't require support on IRC
    // server.
    static const std::map<CString, std::function<void(CClient*, bool bVal)>>&
    CoreCaps();

    friend class ClientTest;
    friend class CCoreCaps;
};

#endif  // !ZNC_CLIENT_H
