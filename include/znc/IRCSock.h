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

#ifndef ZNC_IRCSOCK_H
#define ZNC_IRCSOCK_H

#include <znc/zncconfig.h>
#include <znc/Socket.h>
#include <znc/Nick.h>
#include <znc/Message.h>

#include <deque>

// Forward Declarations
class CChan;
class CUser;
class CIRCNetwork;
class CClient;
// !Forward Declarations

// TODO: This class needs new name
class CIRCSock : public CIRCSocket {
  public:
    CIRCSock(CIRCNetwork* pNetwork);
    virtual ~CIRCSock();

    CIRCSock(const CIRCSock&) = delete;
    CIRCSock& operator=(const CIRCSock&) = delete;

    typedef enum {
        // These values must line up with their position in the CHANMODE
        // argument to raw 005
        ListArg = 0,
        HasArg = 1,
        ArgWhenSet = 2,
        NoArg = 3
    } EChanModeArgs;

    void ReadLine(const CString& sData) override;
    void Connected() override;
    void Disconnected() override;
    void ConnectionRefused() override;
    void SockError(int iErrno, const CString& sDescription) override;
    void Timeout() override;
    void ReachedMaxBuffer() override;
    /** Sends a raw data line to the server.
     *  @param sLine The line to be sent.
     *
     *  The line is first passed \e unmodified to the \ref
     *  CModule::OnSendToIRC() module hook. If no module halts the process,
     *  the line is then sent to the server.
     *
     *  Prefer \l PutIRC() instead.
     */
    void PutIRCRaw(const CString& sLine);
    /** Sends a message to the server.
     *  See \l PutIRC(const CMessage&) for details.
     */
    void PutIRC(const CString& sLine);
    /** Sends a message to the server.
     *  @param  Message The message to be sent.
     *  @note   Only known and compatible messages and tags are sent.
     *
     *  This method can delay the delivery of the message to honor protection
     *  from flood.
     *
     *  This method ensures that only tags, that were negotiated with CAP REQ
     *  and CAP ACK, are sent. Not all IRC server are capable of handling all
     *  messages and tags. Thus, in order to stay compatible with a variety of
     *  IRC servers, ZNC has to filter out messages and tags that the server
     *  has not explicitly acknowleged.
     *
     *  Additional tags can be added via \l CIRCSock::SetTagSupport().
     *
     *  @warning Bypassing the filter may cause troubles to some older IRC
     *  servers.
     *
     *  It is possible to bypass the filter by converting a message to a string
     *  using \l CMessage::ToString(), and passing the resulting raw line to the
     *  \l CIRCSock::PutIRCRaw(const CString& sLine):
     *  \code
     *  pServer->PutIRCRaw(Message.ToString());
     *  \endcode
     */
    void PutIRC(const CMessage& Message);
    void PutIRCQuick(const CString& sLine);  //!< Should be used for PONG only
    void ResetChans();
    void Quit(const CString& sQuitMsg = "");

    /** You can call this from CModule::OnServerCapResult to suspend
     *  sending other CAP requests and CAP END for a while. Each
     *  call to PauseCap should be balanced with a call to ResumeCap.
     */
    void PauseCap();
    /** If you used PauseCap, call this when CAP negotiation and logging in
     *  should be resumed again.
     */
    void ResumeCap();
    
    bool IsTagEnabled(const CString& sTag) const {
        return 1 == m_ssSupportedTags.count(sTag);
    }
    /** Registers a tag as being supported or unsupported by the server.
     *  This doesn't affect tags which the server sends.
     *  @param sTag The tag to register.
     *  @param bState Whether the client supports the tag.
     */
    void SetTagSupport(const CString& sTag, bool bState);

    // Setters
    void SetPass(const CString& s) { m_sPass = s; }
    // !Setters

    // Getters
    unsigned int GetMaxNickLen() const { return m_uMaxNickLen; }
    EChanModeArgs GetModeType(char cMode) const;
    char GetPermFromMode(char cMode) const;
    const std::map<char, EChanModeArgs>& GetChanModes() const {
        return m_mceChanModes;
    }
    bool IsPermChar(const char c) const {
        return (c != '\0' && GetPerms().find(c) != CString::npos);
    }
    bool IsPermMode(const char c) const {
        return (c != '\0' && GetPermModes().find(c) != CString::npos);
    }
    const CString& GetPerms() const { return m_sPerms; }
    const CString& GetPermModes() const { return m_sPermModes; }
    CString GetNickMask() const { return m_Nick.GetNickMask(); }
    const CString& GetNick() const { return m_Nick.GetNick(); }
    const CString& GetPass() const { return m_sPass; }
    CIRCNetwork* GetNetwork() const { return m_pNetwork; }
    bool HasNamesx() const { return m_bNamesx; }
    bool HasUHNames() const { return m_bUHNames; }
    bool HasAwayNotify() const { return m_bAwayNotify; }
    bool HasAccountNotify() const { return m_bAccountNotify; }
    bool HasExtendedJoin() const { return m_bExtendedJoin; }
    bool HasServerTime() const { return m_bServerTime; }
    const std::set<char>& GetUserModes() const {
        return m_scUserModes;
    }
    // This is true if we are past raw 001
    bool IsAuthed() const { return m_bAuthed; }
    const SCString& GetAcceptedCaps() const { return m_ssAcceptedCaps; }
    bool IsCapAccepted(const CString& sCap) {
        return 1 == m_ssAcceptedCaps.count(sCap);
    }
    const MCString& GetISupport() const { return m_mISupport; }
    CString GetISupport(const CString& sKey,
                        const CString& sDefault = "") const;
    // !Getters

    // TODO move this function to CIRCNetwork and make it non-static?
    static bool IsFloodProtected(double fRate);

  private:
    // Message Handlers
    bool OnAccountMessage(CMessage& Message);
    bool OnActionMessage(CActionMessage& Message);
    bool OnAwayMessage(CMessage& Message);
    bool OnCapabilityMessage(CMessage& Message);
    bool OnCTCPMessage(CCTCPMessage& Message);
    bool OnErrorMessage(CMessage& Message);
    bool OnInviteMessage(CMessage& Message);
    bool OnJoinMessage(CJoinMessage& Message);
    bool OnKickMessage(CKickMessage& Message);
    bool OnModeMessage(CModeMessage& Message);
    bool OnNickMessage(CNickMessage& Message);
    bool OnNoticeMessage(CNoticeMessage& Message);
    bool OnNumericMessage(CNumericMessage& Message);
    bool OnPartMessage(CPartMessage& Message);
    bool OnPingMessage(CMessage& Message);
    bool OnPongMessage(CMessage& Message);
    bool OnQuitMessage(CQuitMessage& Message);
    bool OnTextMessage(CTextMessage& Message);
    bool OnTopicMessage(CTopicMessage& Message);
    bool OnWallopsMessage(CMessage& Message);
    bool OnServerCapAvailable(const CString& sCap);
    // !Message Handlers

    void SetNick(const CString& sNick);
    void ParseISupport(const CMessage& Message);
    // This is called when we connect and the nick we want is already taken
    void SendAltNick(const CString& sBadNick);
    void SendNextCap();
    void TrySend();

  protected:
    bool m_bAuthed;
    bool m_bNamesx;
    bool m_bUHNames;
    bool m_bAwayNotify;
    bool m_bAccountNotify;
    bool m_bExtendedJoin;
    bool m_bServerTime;
    CString m_sPerms;
    CString m_sPermModes;
    std::set<char> m_scUserModes;
    std::map<char, EChanModeArgs> m_mceChanModes;
    CIRCNetwork* m_pNetwork;
    CNick m_Nick;
    CString m_sPass;
    std::map<CString, CChan*> m_msChans;
    unsigned int m_uMaxNickLen;
    unsigned int m_uCapPaused;
    SCString m_ssAcceptedCaps;
    SCString m_ssPendingCaps;
    time_t m_lastCTCP;
    unsigned int m_uNumCTCP;
    static const time_t m_uCTCPFloodTime;
    static const unsigned int m_uCTCPFloodCount;
    MCString m_mISupport;
    std::deque<CMessage> m_vSendQueue;
    short int m_iSendsAllowed;
    unsigned short int m_uFloodBurst;
    double m_fFloodRate;
    bool m_bFloodProtection;
    SCString m_ssSupportedTags;

    friend class CIRCFloodTimer;
};

#endif  // !ZNC_IRCSOCK_H
