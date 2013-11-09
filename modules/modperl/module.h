/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include <znc/Modules.h>

#if HAVE_VISIBILITY
#pragma GCC visibility push(default)
#endif
class CPerlModule : public CModule {
	SV* m_perlObj;
	VWebSubPages* _GetSubPages();
public:
	CPerlModule(CUser* pUser, CIRCNetwork* pNetwork, const CString& sModName, const CString& sDataPath,
			SV* perlObj)
			: CModule(NULL, pUser, pNetwork, sModName, sDataPath) {
		m_perlObj = newSVsv(perlObj);
	}
	SV* GetPerlObj() {
		return sv_2mortal(newSVsv(m_perlObj));
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
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const std::vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const std::vector<CChan*>& vChans);
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
	virtual bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&);
	virtual EModRet OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet);
	virtual EModRet OnDeleteNetwork(CIRCNetwork& Network);
};

static inline CPerlModule* AsPerlModule(CModule* p) {
	return dynamic_cast<CPerlModule*>(p);
}

enum ELoadPerlMod {
	Perl_NotFound,
	Perl_Loaded,
	Perl_LoadError,
};

class CPerlTimer : public CTimer {
	SV* m_perlObj;
public:
	CPerlTimer(CPerlModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription, SV* perlObj)
					: CTimer (pModule, uInterval, uCycles, sLabel, sDescription), m_perlObj(newSVsv(perlObj)) {
		pModule->AddTimer(this);
	}
	virtual void RunJob();
	SV* GetPerlObj() {
		return sv_2mortal(newSVsv(m_perlObj));
	}
	~CPerlTimer();
};

inline CPerlTimer* CreatePerlTimer(CPerlModule* pModule, unsigned int uInterval, unsigned int uCycles,
		const CString& sLabel, const CString& sDescription, SV* perlObj) {
	return new CPerlTimer(pModule, uInterval, uCycles, sLabel, sDescription, perlObj);
}

class CPerlSocket : public CSocket {
	SV* m_perlObj;
public:
	CPerlSocket(CPerlModule* pModule, SV* perlObj) : CSocket(pModule), m_perlObj(newSVsv(perlObj)) {}
	SV* GetPerlObj() {
		return sv_2mortal(newSVsv(m_perlObj));
	}
	~CPerlSocket();
	virtual void Connected();
	virtual void Disconnected();
	virtual void Timeout();
	virtual void ConnectionRefused();
	virtual void ReadData(const char *data, size_t len);
	virtual void ReadLine(const CString& sLine);
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
};

inline CPerlSocket* CreatePerlSocket(CPerlModule* pModule, SV* perlObj) {
	return new CPerlSocket(pModule, perlObj);
}

inline bool HaveIPv6() {
#ifdef HAVE_IPV6
	return true;
#endif
	return false;
}

inline bool HaveSSL() {
#ifdef HAVE_LIBSSL
	return true;
#endif
	return false;
}

inline int _GetSOMAXCONN() {
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
#if HAVE_VISIBILITY
#pragma GCC visibility pop
#endif
