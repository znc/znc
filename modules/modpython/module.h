/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

class String {
public:
	CString s;
};

class CModPython;

#if HAVE_VISIBILITY
#pragma GCC visibility push(default)
#endif
class CPyModule : public CModule {
	PyObject* m_pyObj;
	CModPython* m_pModPython;
	VWebSubPages* _GetSubPages();
public:
	CPyModule(CUser* pUser, CIRCNetwork* pNetwork, const CString& sModName, const CString& sDataPath,
			PyObject* pyObj, CModPython* pModPython)
			: CModule(NULL, pUser, pNetwork, sModName, sDataPath) {
		m_pyObj = pyObj;
		Py_INCREF(pyObj);
		m_pModPython = pModPython;
	}
	PyObject* GetPyObj() { // borrows
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
	CModPython* GetModPython() {
		return m_pModPython;
	}

	bool OnBoot() override;
	bool WebRequiresLogin() override;
	bool WebRequiresAdmin() override;
	CString GetWebMenuTitle() override;
	bool OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) override;
	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override;
	VWebSubPages& GetSubPages() override;
	void OnPreRehash() override;
	void OnPostRehash() override;
	void OnIRCDisconnected() override;
	void OnIRCConnected() override;
	EModRet OnIRCConnecting(CIRCSock *pIRCSock) override;
	void OnIRCConnectionError(CIRCSock *pIRCSock) override;
	EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) override;
	EModRet OnBroadcast(CString& sMessage) override;
	void OnChanPermission2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) override;
	void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	void OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	void OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	void OnDevoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	void OnMode2(const CNick* pOpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) override;
	void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override;
	EModRet OnRaw(CString& sLine) override;
	EModRet OnStatusCommand(CString& sCommand) override;
	void OnModCommand(const CString& sCommand) override;
	void OnModNotice(const CString& sMessage) override;
	void OnModCTCP(const CString& sMessage) override;
	void OnQuit(const CNick& Nick, const CString& sMessage, const std::vector<CChan*>& vChans) override;
	void OnNick(const CNick& Nick, const CString& sNewNick, const std::vector<CChan*>& vChans) override;
	void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) override;
	EModRet OnJoining(CChan& Channel) override;
	void OnJoin(const CNick& Nick, CChan& Channel) override;
	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override;
	EModRet OnChanBufferStarting(CChan& Chan, CClient& Client) override;
	EModRet OnChanBufferEnding(CChan& Chan, CClient& Client) override;
	EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine) override;
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
	EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override;
	EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override;
	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;
	EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override;
	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override;
	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override;
	bool OnServerCapAvailable(const CString& sCap) override;
	void OnServerCapResult(const CString& sCap, bool bSuccess) override;
	EModRet OnTimerAutoJoin(CChan& Channel) override;
	bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&) override;
	EModRet OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) override;
	EModRet OnDeleteNetwork(CIRCNetwork& Network) override;
	EModRet OnSendToClient(CString& sLine, CClient& Client) override;
	EModRet OnSendToIRC(CString& sLine) override;

	// Global Modules
	EModRet OnAddUser(CUser& User, CString& sErrorRet) override;
	EModRet OnDeleteUser(CUser& User) override;
	void OnClientConnect(CZNCSock* pSock, const CString& sHost, unsigned short uPort) override;
	void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) override;
	EModRet OnUnknownUserRaw(CClient* pClient, CString& sLine) override;
	bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState) override;
	void OnClientCapRequest(CClient* pClient, const CString& sCap, bool bState) override;
	virtual EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
			CModInfo::EModuleType eType, bool& bSuccess, CString& sRetMsg) override;
	EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg) override;
	virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg) override;
	void OnGetAvailableMods(std::set<CModInfo>& ssMods, CModInfo::EModuleType eType) override;
	void OnClientCapLs(CClient* pClient, SCString& ssCaps) override;
	EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override;
};

static inline CPyModule* AsPyModule(CModule* p) {
	return dynamic_cast<CPyModule*>(p);
}

inline CPyModule* CreatePyModule(CUser* pUser, CIRCNetwork* pNetwork, const CString& sModName, const CString& sDataPath, PyObject* pyObj, CModPython* pModPython) {
	return new CPyModule(pUser, pNetwork, sModName, sDataPath, pyObj, pModPython);
}

class CPyTimer : public CTimer {
	PyObject* m_pyObj;
	CModPython* m_pModPython;
public:
	CPyTimer(CPyModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription, PyObject* pyObj)
					: CTimer (pModule, uInterval, uCycles, sLabel, sDescription), m_pyObj(pyObj) {
		Py_INCREF(pyObj);
		pModule->AddTimer(this);
		m_pModPython = pModule->GetModPython();
	}
	void RunJob() override;
	PyObject* GetPyObj() { return m_pyObj; }
	PyObject* GetNewPyObj() {
		Py_INCREF(m_pyObj);
		return m_pyObj;
	}
	~CPyTimer();
};

inline CPyTimer* CreatePyTimer(CPyModule* pModule, unsigned int uInterval, unsigned int uCycles,
		const CString& sLabel, const CString& sDescription, PyObject* pyObj) {
	return new CPyTimer(pModule, uInterval, uCycles, sLabel, sDescription, pyObj);
}

class CPySocket : public CSocket {
	PyObject* m_pyObj;
	CModPython* m_pModPython;
public:
	CPySocket(CPyModule* pModule, PyObject* pyObj) : CSocket(pModule), m_pyObj(pyObj) {
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
	void ReadData(const char *data, size_t len) override;
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

inline int GetSOMAXCONN() {
	return SOMAXCONN;
}

inline int GetVersionMajor() {
	return VERSION_MAJOR;
}

inline int GetVersionMinor() {
	return VERSION_MINOR;
}

inline double GetVersion() {
	return VERSION;
}

inline CString GetVersionExtra() {
	return ZNC_VERSION_EXTRA;
}

class MCString_iter {
public:
	MCString_iter() {}
	MCString::iterator x;
	MCString_iter(MCString::iterator z) : x(z) {}
	void plusplus() {
		++x;
	}
	CString get() {
		return x->first;
	}
	bool is_end(CModule* m) {
		return m->EndNV() == x;
	}
};

class CModulesIter {
public:
	CModulesIter(CModules *pModules) {
		m_pModules = pModules;
		m_it = pModules->begin();
	}

	void plusplus() {
		++m_it;
	}

	const CModule* get() const {
		return *m_it;
	}

	bool is_end() const {
		return m_pModules->end() == m_it;
	}

	CModules *m_pModules;
	CModules::const_iterator m_it;
};

#if HAVE_VISIBILITY
#pragma GCC visibility pop
#endif
