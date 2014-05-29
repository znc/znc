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

	virtual bool OnBoot();
	virtual bool WebRequiresLogin();
	virtual bool WebRequiresAdmin();
	virtual CString GetWebMenuTitle();
	virtual bool OnWebPreRequest(CWebSock& WebSock, const CString& sPageName);
	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl);
	virtual VWebSubPages& GetSubPages();
	virtual void OnPreRehash();
	virtual void OnPostRehash();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();
	virtual EModRet OnIRCConnecting(CIRCSock *pIRCSock);
	virtual void OnIRCConnectionError(CIRCSock *pIRCSock);
	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName);
	virtual EModRet OnBroadcast(CString& sMessage);
	virtual void OnChanPermission2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDevoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnMode2(const CNick* pOpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange);
	virtual void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs);
	virtual EModRet OnRaw(CString& sLine);
	virtual EModRet OnStatusCommand(CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const std::vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const std::vector<CChan*>& vChans);
	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage);
	virtual EModRet OnJoining(CChan& Channel);
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage);
	virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client);
	virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client);
	virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine);
	virtual EModRet OnPrivBufferPlayLine(CClient& Client, CString& sLine);
	virtual void OnClientLogin();
	virtual void OnClientDisconnect();
	virtual EModRet OnUserRaw(CString& sLine);
	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey);
	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage);
	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic);
	virtual EModRet OnUserTopicRequest(CString& sChannel);
	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage);
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic);
	virtual bool OnServerCapAvailable(const CString& sCap);
	virtual void OnServerCapResult(const CString& sCap, bool bSuccess);
	virtual EModRet OnTimerAutoJoin(CChan& Channel);
	virtual bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&);
	virtual EModRet OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet);
	virtual EModRet OnDeleteNetwork(CIRCNetwork& Network);
	virtual EModRet OnSendToClient(CString& sLine, CClient& Client);
	virtual EModRet OnSendToIRC(CString& sLine);

	// Global Modules
	virtual EModRet OnAddUser(CUser& User, CString& sErrorRet);
	virtual EModRet OnDeleteUser(CUser& User);
	virtual void OnClientConnect(CZNCSock* pSock, const CString& sHost, unsigned short uPort);
	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP);
	virtual EModRet OnUnknownUserRaw(CClient* pClient, CString& sLine);
	virtual bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState);
	virtual void OnClientCapRequest(CClient* pClient, const CString& sCap, bool bState);
	virtual EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
			CModInfo::EModuleType eType, bool& bSuccess, CString& sRetMsg);
	virtual EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg);
	virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg);
	virtual void OnGetAvailableMods(std::set<CModInfo>& ssMods, CModInfo::EModuleType eType);
	virtual void OnClientCapLs(CClient* pClient, SCString& ssCaps);
	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth);
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
	virtual void RunJob();
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
	virtual void Connected();
	virtual void Disconnected();
	virtual void Timeout();
	virtual void ConnectionRefused();
	virtual void ReadData(const char *data, size_t len);
	virtual void ReadLine(const CString& sLine);
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
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
