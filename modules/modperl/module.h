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

	virtual bool OnBoot() override;
	virtual bool WebRequiresLogin() override;
	virtual bool WebRequiresAdmin() override;
	virtual CString GetWebMenuTitle() override;
	virtual bool OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) override;
	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override;
	virtual VWebSubPages& GetSubPages() override;
	virtual void OnPreRehash() override;
	virtual void OnPostRehash() override;
	virtual void OnIRCDisconnected() override;
	virtual void OnIRCConnected() override;
	virtual EModRet OnIRCConnecting(CIRCSock *pIRCSock) override;
	virtual void OnIRCConnectionError(CIRCSock *pIRCSock) override;
	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) override;
	virtual EModRet OnBroadcast(CString& sMessage) override;
	virtual void OnChanPermission2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) override;
	virtual void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	virtual void OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	virtual void OnVoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	virtual void OnDevoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override;
	virtual void OnMode2(const CNick* pOpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) override;
	virtual void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override;
	virtual EModRet OnRaw(CString& sLine) override;
	virtual EModRet OnStatusCommand(CString& sCommand) override;
	virtual void OnModCommand(const CString& sCommand) override;
	virtual void OnModNotice(const CString& sMessage) override;
	virtual void OnModCTCP(const CString& sMessage) override;
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const std::vector<CChan*>& vChans) override;
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const std::vector<CChan*>& vChans) override;
	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) override;
	virtual EModRet OnJoining(CChan& Channel) override;
	virtual void OnJoin(const CNick& Nick, CChan& Channel) override;
	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override;
	virtual EModRet OnInvite(const CNick& Nick, const CString& sChan) override;
	virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client) override;
	virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client) override;
	virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine) override;
	virtual EModRet OnPrivBufferPlayLine(CClient& Client, CString& sLine) override;
	virtual void OnClientLogin() override;
	virtual void OnClientDisconnect() override;
	virtual EModRet OnUserRaw(CString& sLine) override;
	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override;
	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override;
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) override;
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) override;
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage) override;
	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey) override;
	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) override;
	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic) override;
	virtual EModRet OnUserTopicRequest(CString& sChannel) override;
	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override;
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override;
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override;
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage) override;
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override;
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override;
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override;
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override;
	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override;
	virtual bool OnServerCapAvailable(const CString& sCap) override;
	virtual void OnServerCapResult(const CString& sCap, bool bSuccess) override;
	virtual EModRet OnTimerAutoJoin(CChan& Channel) override;
	virtual bool OnEmbeddedWebRequest(CWebSock&, const CString&, CTemplate&) override;
	virtual EModRet OnAddNetwork(CIRCNetwork& Network, CString& sErrorRet) override;
	virtual EModRet OnDeleteNetwork(CIRCNetwork& Network) override;
	virtual EModRet OnSendToClient(CString& sLine, CClient& Client) override;
	virtual EModRet OnSendToIRC(CString& sLine) override;
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
	virtual void RunJob() override;
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
	virtual void Connected() override;
	virtual void Disconnected() override;
	virtual void Timeout() override;
	virtual void ConnectionRefused() override;
	virtual void ReadData(const char *data, size_t len) override;
	virtual void ReadLine(const CString& sLine) override;
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
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

inline bool HaveCharset() {
#ifdef HAVE_ICU
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
