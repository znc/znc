/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
	CPyModule(CUser* pUser, const CString& sModName, const CString& sDataPath,
			PyObject* pyObj, CGlobalModule* pModPython)
			: CModule(NULL, pUser, sModName, sDataPath) {
		m_pyObj = pyObj;
		Py_INCREF(pyObj);
		m_pModPython = reinterpret_cast<CModPython*>(pModPython);
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
	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);
	virtual EModRet OnRaw(CString& sLine);
	virtual EModRet OnStatusCommand(CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage);
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
	bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&);
};

static inline CPyModule* AsPyModule(CModule* p) {
	return dynamic_cast<CPyModule*>(p);
}

inline CPyModule* CreatePyModule(CUser* pUser, const CString& sModName, const CString& sDataPath, PyObject* pyObj, CGlobalModule* pModPython) {
	return new CPyModule(pUser, sModName, sDataPath, pyObj, pModPython);
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
	PyObject* WriteBytes(PyObject* data);
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

inline bool HaveCAres_() {
#ifdef HAVE_C_ARES
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
	return VERSION_EXTRA;
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

#if HAVE_VISIBILITY
#pragma GCC visibility pop
#endif
