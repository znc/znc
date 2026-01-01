/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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

#pragma once

// This class is used from python to call functions which accept CString&
// __str__ is added to it in modpython.i
class String {
  public:
    CString s;

    String(const CString& s = "") : s(s) {}
};

class CModPython;

class ZNC_EXPORT_LIB_EXPORT CPyModule : public CModule {
    PyObject* m_pyObj;
    CModPython* m_pModPython;
    VWebSubPages* _GetSubPages();

  public:
    CPyModule(CUser* pUser, CIRCNetwork* pNetwork, const CString& sModName,
              const CString& sDataPath, CModInfo::EModuleType eType,
              PyObject* pyObj, CModPython* pModPython)
        : CModule(nullptr, pUser, pNetwork, sModName, sDataPath, eType) {
        m_pyObj = pyObj;
        Py_INCREF(pyObj);
        m_pModPython = pModPython;
    }
    PyObject* GetPyObj() {  // borrows
        return m_pyObj;
    }
    PyObject* GetNewPyObj() {
        Py_INCREF(m_pyObj);
        return m_pyObj;
    }
    void DeletePyModule() {
        Py_CLEAR(m_pyObj);
        delete this;
    }
    CString GetPyExceptionStr();
    CModPython* GetModPython() { return m_pModPython; }

    bool OnBoot() override;
    bool WebRequiresLogin() override;
    bool WebRequiresAdmin() override;
    CString GetWebMenuTitle() override;
    bool OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) override;
    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override;
    bool ValidateWebRequestCSRFCheck(CWebSock& WebSock,
                                     const CString& sPageName) override;
    VWebSubPages& GetSubPages() override;
    void OnPreRehash() override;
    void OnPostRehash() override;
    void OnIRCDisconnected() override;
    void OnIRCConnected() override;
    EModRet OnIRCConnecting(CIRCSock* pIRCSock) override;
    void OnIRCConnectionError(CIRCSock* pIRCSock) override;
    EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent,
                              CString& sRealName) override;
    EModRet OnBroadcast(CString& sMessage) override;
    void OnChanPermission2(const CNick* pOpNick, const CNick& Nick,
                           CChan& Channel, unsigned char uMode, bool bAdded,
                           bool bNoChange) override;
    void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
               bool bNoChange) override;
    void OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                 bool bNoChange) override;
    void OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                  bool bNoChange) override;
    void OnDevoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
                    bool bNoChange) override;
    void OnMode2(const CNick* pOpNick, CChan& Channel, char uMode,
                 const CString& sArg, bool bAdded, bool bNoChange) override;
    void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes,
                    const CString& sArgs) override;
    EModRet OnRaw(CString& sLine) override;
    EModRet OnStatusCommand(CString& sCommand) override;
    void OnModCommand(const CString& sCommand) override;
    void OnModNotice(const CString& sMessage) override;
    void OnModCTCP(const CString& sMessage) override;
    void OnQuit(const CNick& Nick, const CString& sMessage,
                const std::vector<CChan*>& vChans) override;
    void OnNick(const CNick& Nick, const CString& sNewNick,
                const std::vector<CChan*>& vChans) override;
    void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel,
                const CString& sMessage) override;
    EModRet OnJoining(CChan& Channel) override;
    void OnJoin(const CNick& Nick, CChan& Channel) override;
    void OnPart(const CNick& Nick, CChan& Channel,
                const CString& sMessage) override;
    EModRet OnInvite(const CNick& Nick, const CString& sChan) override;
    EModRet OnChanBufferStarting(CChan& Chan, CClient& Client) override;
    EModRet OnChanBufferEnding(CChan& Chan, CClient& Client) override;
    EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client,
                                 CString& sLine) override;
    EModRet OnPrivBufferPlayLine(CClient& Client, CString& sLine) override;
    void OnClientLogin() override;
    void OnClientDisconnect() override;
    EModRet OnUserRaw(CString& sLine) override;
    EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override;
    EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override;
    EModRet OnUserAction(CString& sTarget, CString& sMessage) override;
    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override;
    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override;
    EModRet OnUserJoin(CString& sChannel, CString& sKey) override;
    EModRet OnUserPart(CString& sChannel, CString& sMessage) override;
    EModRet OnUserTopic(CString& sChannel, CString& sTopic) override;
    EModRet OnUserTopicRequest(CString& sChannel) override;
    EModRet OnUserQuit(CString& sMessage) override;
    EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override;
    EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override;
    EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override;
    EModRet OnPrivAction(CNick& Nick, CString& sMessage) override;
    EModRet OnChanAction(CNick& Nick, CChan& Channel,
                         CString& sMessage) override;
    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override;
    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;
    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override;
    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override;
    EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override;
    bool OnServerCapAvailable(const CString& sCap) override;
    bool OnServerCap302Available(const CString& sCap, const CString& sValue) override;
    void OnServerCapResult(const CString& sCap, bool bSuccess) override;
    void OnClientAttached() override;
    void OnClientDetached() override;
    EModRet OnTimerAutoJoin(CChan& Channel) override;
    bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&) override;
    EModRet OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) override;
    EModRet OnDeleteNetwork(CIRCNetwork& Network) override;
    EModRet OnSendToClient(CString& sLine, CClient& Client) override;
    EModRet OnSendToIRC(CString& sLine) override;

    EModRet OnRawMessage(CMessage& Message) override;
    EModRet OnNumericMessage(CNumericMessage& Message) override;
    void OnQuitMessage(CQuitMessage& Message,
                       const std::vector<CChan*>& vChans) override;
    void OnNickMessage(CNickMessage& Message,
                       const std::vector<CChan*>& vChans) override;
    void OnKickMessage(CKickMessage& Message) override;
    void OnJoinMessage(CJoinMessage& Message) override;
    void OnPartMessage(CPartMessage& Message) override;
    EModRet OnChanBufferPlayMessage(CMessage& Message) override;
    EModRet OnPrivBufferPlayMessage(CMessage& Message) override;
    EModRet OnUserRawMessage(CMessage& Message) override;
    EModRet OnUserCTCPReplyMessage(CCTCPMessage& Message) override;
    EModRet OnUserCTCPMessage(CCTCPMessage& Message) override;
    EModRet OnUserActionMessage(CActionMessage& Message) override;
    EModRet OnUserTextMessage(CTextMessage& Message) override;
    EModRet OnUserNoticeMessage(CNoticeMessage& Message) override;
    EModRet OnUserJoinMessage(CJoinMessage& Message) override;
    EModRet OnUserPartMessage(CPartMessage& Message) override;
    EModRet OnUserTopicMessage(CTopicMessage& Message) override;
    EModRet OnUserQuitMessage(CQuitMessage& Message) override;
    EModRet OnCTCPReplyMessage(CCTCPMessage& Message) override;
    EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override;
    EModRet OnChanCTCPMessage(CCTCPMessage& Message) override;
    EModRet OnPrivActionMessage(CActionMessage& Message) override;
    EModRet OnChanActionMessage(CActionMessage& Message) override;
    EModRet OnPrivTextMessage(CTextMessage& Message) override;
    EModRet OnChanTextMessage(CTextMessage& Message) override;
    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override;
    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override;
    EModRet OnTopicMessage(CTopicMessage& Message) override;
    EModRet OnSendToClientMessage(CMessage& Message) override;
    EModRet OnSendToIRCMessage(CMessage& Message) override;
    EModRet OnUserTagMessage(CTargetMessage& Message) override;
    EModRet OnChanTagMessage(CTargetMessage& Message) override;
    EModRet OnPrivTagMessage(CTargetMessage& Message) override;
    EModRet OnInviteMessage(CInviteMessage& Message) override;

    // Global Modules
    EModRet OnAddUser(CUser& User, CString& sErrorRet) override;
    EModRet OnDeleteUser(CUser& User) override;
    void OnClientConnect(CZNCSock* pSock, const CString& sHost,
                         unsigned short uPort) override;
    void OnFailedLogin(const CString& sUsername,
                       const CString& sRemoteIP) override;
    EModRet OnUnknownUserRaw(CClient* pClient, CString& sLine) override;
    EModRet OnUnknownUserRawMessage(CMessage& Message) override;
    bool IsClientCapSupported(CClient* pClient, const CString& sCap,
                              bool bState) override;
    void OnClientCapRequest(CClient* pClient, const CString& sCap,
                            bool bState) override;
    void OnClientGetSASLMechanisms(SCString& ssMechanisms) override;
    EModRet OnClientSASLServerInitialChallenge(const CString& sMechanism,
                                               CString& sResponse) override;
    EModRet OnClientSASLAuthenticate(const CString& sMechanism,
                                     const CString& sMessage) override;
    void OnClientSASLAborted() override;
    virtual EModRet OnModuleLoading(const CString& sModName,
                                    const CString& sArgs,
                                    CModInfo::EModuleType eType, bool& bSuccess,
                                    CString& sRetMsg) override;
    EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess,
                              CString& sRetMsg) override;
    virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
                                 bool& bSuccess, CString& sRetMsg) override;
    void OnGetAvailableMods(std::set<CModInfo>& ssMods,
                            CModInfo::EModuleType eType) override;
    void OnClientCapLs(CClient* pClient, SCString& ssCaps) override;
    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override;
};

static inline CPyModule* AsPyModule(CModule* p) {
    return dynamic_cast<CPyModule*>(p);
}

inline CPyModule* CreatePyModule(CUser* pUser, CIRCNetwork* pNetwork,
                                 const CString& sModName,
                                 const CString& sDataPath,
                                 CModInfo::EModuleType eType, PyObject* pyObj,
                                 CModPython* pModPython) {
    return new CPyModule(pUser, pNetwork, sModName, sDataPath, eType, pyObj,
                         pModPython);
}

class ZNC_EXPORT_LIB_EXPORT CPyTimer : public CTimer {
    PyObject* m_pyObj;
    CModPython* m_pModPython;

  public:
    CPyTimer(CPyModule* pModule, unsigned int uInterval, unsigned int uCycles,
             const CString& sLabel, const CString& sDescription,
             PyObject* pyObj)
        : CTimer(pModule, uInterval, uCycles, sLabel, sDescription),
          m_pyObj(pyObj) {
        Py_INCREF(pyObj);
        m_pModPython = pModule->GetModPython();
        pModule->AddTimer(this);
    }
    void RunJob() override;
    PyObject* GetPyObj() { return m_pyObj; }
    PyObject* GetNewPyObj() {
        Py_INCREF(m_pyObj);
        return m_pyObj;
    }
    ~CPyTimer();
};

inline CPyTimer* CreatePyTimer(CPyModule* pModule, unsigned int uInterval,
                               unsigned int uCycles, const CString& sLabel,
                               const CString& sDescription, PyObject* pyObj) {
    return new CPyTimer(pModule, uInterval, uCycles, sLabel, sDescription,
                        pyObj);
}

class ZNC_EXPORT_LIB_EXPORT CPySocket : public CSocket {
    PyObject* m_pyObj;
    CModPython* m_pModPython;

  public:
    CPySocket(CPyModule* pModule, PyObject* pyObj)
        : CSocket(pModule), m_pyObj(pyObj) {
        Py_INCREF(pyObj);
        m_pModPython = pModule->GetModPython();
    }
    PyObject* GetPyObj() { return m_pyObj; }
    PyObject* GetNewPyObj() {
        Py_INCREF(m_pyObj);
        return m_pyObj;
    }
    ~CPySocket();
    void Connected() override;
    void Disconnected() override;
    void Timeout() override;
    void ConnectionRefused() override;
    void ReadData(const char* data, size_t len) override;
    void ReadLine(const CString& sLine) override;
    Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
};

inline CPySocket* CreatePySocket(CPyModule* pModule, PyObject* pyObj) {
    return new CPySocket(pModule, pyObj);
}

inline bool HaveIPv6_() {
#ifdef HAVE_IPV6
    return true;
#endif
    return false;
}

inline bool HaveSSL_() {
#ifdef HAVE_LIBSSL
    return true;
#endif
    return false;
}

inline bool HaveCharset_() {
#ifdef HAVE_ICU
    return true;
#endif
    return false;
}

inline int GetSOMAXCONN() { return SOMAXCONN; }

inline int GetVersionMajor() { return VERSION_MAJOR; }

inline int GetVersionMinor() { return VERSION_MINOR; }

inline double GetVersion() { return VERSION; }

inline CString GetVersionExtra() { return ZNC_VERSION_EXTRA; }

class MCString_iter {
  public:
    MCString_iter() {}
    MCString::iterator x;
    MCString_iter(MCString::iterator z) : x(z) {}
    void plusplus() { ++x; }
    CString get() { return x->first; }
    bool is_end(CModule* m) { return m->EndNV() == x; }
};

class CModulesIter {
  public:
    CModulesIter(CModules* pModules) {
        m_pModules = pModules;
        m_it = pModules->begin();
    }

    void plusplus() { ++m_it; }

    const CModule* get() const { return *m_it; }

    bool is_end() const { return m_pModules->end() == m_it; }

    CModules* m_pModules;
    CModules::const_iterator m_it;
};

class ZNC_EXPORT_LIB_EXPORT CPyModCommand : public CModCommand {
  CPyModule* m_pModule;
  CModPython* m_pModPython;
  PyObject* m_pyObj;

  void operator()(const CString& sLine);

  public:
    CPyModCommand(CPyModule* pModule,
                  const CString& sCmd, const COptionalTranslation& sArgs,
                  const COptionalTranslation& sDesc, PyObject *pyObj)
      : CModCommand(sCmd, [=](const CString& sLine) { (*this)(sLine); }, sArgs,
                    sDesc),
        m_pModule(pModule),
        m_pModPython(pModule->GetModPython()),
        m_pyObj(pyObj) {
      Py_INCREF(pyObj);
      pModule->AddCommand(*this);
    }
    virtual ~CPyModCommand();

    CPyModule* GetModule();
};

inline CPyModCommand* CreatePyModCommand(CPyModule* pModule,
                                         const CString& sCmd,
                                         const COptionalTranslation& sArgs,
                                         const COptionalTranslation& sDesc,
                                         PyObject* pyObj) {
    return new CPyModCommand(pModule, sCmd, sArgs, sDesc, pyObj);
}

class ZNC_EXPORT_LIB_EXPORT CPyCapability : public CCapability {
  public:
    CPyCapability(PyObject* serverCb, PyObject* clientCb);
    ~CPyCapability();

    void OnServerChangedSupport(CIRCNetwork* pNetwork, bool bState) override;
    void OnClientChangedSupport(CClient* pClient, bool bState) override;

  private:
    PyObject* m_serverCb;
    PyObject* m_clientCb;
};

