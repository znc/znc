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
	EModRet OnUserQuit(CString& sMessage) override;
	EModRet OnUserTopicRequest(CString& sChannel) override;
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
	void RunJob() override;
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
	void Connected() override;
	void Disconnected() override;
	void Timeout() override;
	void ConnectionRefused() override;
	void ReadData(const char *data, size_t len) override;
	void ReadLine(const CString& sLine) override;
	Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
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
