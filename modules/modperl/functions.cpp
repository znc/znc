/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

/***************************************************************************
 * This file is generated automatically using codegen.pl from functions.in *
 * Don't change it manually.                                               *
 ***************************************************************************/

/*#include "module.h"
#include "swigperlrun.h"
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "pstring.h"*/

namespace {
	template<class T>
	struct SvToPtr {
		CString m_sType;
		SvToPtr(const CString& sType) {
			m_sType = sType;
		}
		T* operator()(SV* sv) {
			T* result;
			int res = SWIG_ConvertPtr(sv, (void**)&result, SWIG_TypeQuery(m_sType.c_str()), 0);
			if (SWIG_IsOK(res)) {
				return result;
			}
			return NULL;
		}
	};

	CModule::EModRet SvToEModRet(SV* sv) {
		return static_cast<CModule::EModRet>(SvUV(sv));
	}
}
/*
#define PSTART dSP; I32 ax; int ret = 0; ENTER; SAVETMPS; PUSHMARK(SP)
#define PCALL(name) PUTBACK; ret = call_pv(name, G_EVAL|G_ARRAY); SPAGAIN; SP -= ret; ax = (SP - PL_stack_base) + 1
#define PEND PUTBACK; FREETMPS; LEAVE
#define PUSH_STR(s) XPUSHs(PString(s).GetSV())
#define PUSH_PTR(type, p) XPUSHs(SWIG_NewInstanceObj(const_cast<type>(p), SWIG_TypeQuery(#type), SWIG_SHADOW))
*/
#define PSTART_IDF(Func) PSTART; PUSH_STR(GetPerlID()); PUSH_STR(#Func)
#define PCALLMOD(Error, Success) PCALL("ZNC::Core::CallModFunc"); if (SvTRUE(ERRSV)) { DEBUG("Perl hook died with: " + PString(ERRSV)); Error; } else { Success; } PEND

bool CPerlModule::OnBoot() {
	bool result = true;
	PSTART_IDF(OnBoot);
	mXPUSHi(static_cast<int>(true)); // Default value
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

bool CPerlModule::WebRequiresLogin() {
	bool result = true;
	PSTART_IDF(WebRequiresLogin);
	mXPUSHi(static_cast<int>(true)); // Default value
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

bool CPerlModule::WebRequiresAdmin() {
	bool result = false;
	PSTART_IDF(WebRequiresAdmin);
	mXPUSHi(static_cast<int>(false)); // Default value
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

CString CPerlModule::GetWebMenuTitle() {
	CString result = "";
	PSTART_IDF(GetWebMenuTitle);
	PUSH_STR(""); // Default value
	PCALLMOD(,
		result = PString(ST(0));
	);
	return result;
}

bool CPerlModule::OnWebPreRequest(CWebSock& WebSock, const CString& sPageName) {
	bool result = false;
	PSTART_IDF(OnWebPreRequest);
	mXPUSHi(static_cast<int>(false)); // Default value
	PUSH_PTR(CWebSock*, &WebSock);
	PUSH_STR(sPageName);
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

bool CPerlModule::OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
	bool result = false;
	PSTART_IDF(OnWebRequest);
	mXPUSHi(static_cast<int>(false)); // Default value
	PUSH_PTR(CWebSock*, &WebSock);
	PUSH_STR(sPageName);
	PUSH_PTR(CTemplate*, &Tmpl);
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

VWebSubPages* CPerlModule::_GetSubPages() {
	VWebSubPages* result = (VWebSubPages*)NULL;
	PSTART_IDF(_GetSubPages);
	PUSH_PTR(VWebSubPages*, (VWebSubPages*)NULL); // Default value
	PCALLMOD(,
		result = SvToPtr<VWebSubPages>("VWebSubPages*")(ST(0));
	);
	return result;
}

void CPerlModule::OnPreRehash() {
	PSTART_IDF(OnPreRehash);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

void CPerlModule::OnPostRehash() {
	PSTART_IDF(OnPostRehash);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

void CPerlModule::OnIRCDisconnected() {
	PSTART_IDF(OnIRCDisconnected);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

void CPerlModule::OnIRCConnected() {
	PSTART_IDF(OnIRCConnected);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnIRCConnecting(CIRCSock *pIRCSock) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnIRCConnecting);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CIRCSock *, pIRCSock);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

CModule::EModRet CPerlModule::OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnIRCRegistration);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sPass);
	PUSH_STR(sNick);
	PUSH_STR(sIdent);
	PUSH_STR(sRealName);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sPass = PString(ST(1));
		sNick = PString(ST(2));
		sIdent = PString(ST(3));
		sRealName = PString(ST(4));
	);
	return result;
}

CModule::EModRet CPerlModule::OnBroadcast(CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnBroadcast);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(1));
	);
	return result;
}

CModule::EModRet CPerlModule::OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnConfigLine);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sName);
	PUSH_STR(sValue);
	PUSH_PTR(CUser*, pUser);
	PUSH_PTR(CChan*, pChan);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

void CPerlModule::OnWriteUserConfig(CFile& Config) {
	PSTART_IDF(OnWriteUserConfig);
	mXPUSHi(0); // Default value
	PUSH_PTR(CFile*, &Config);
	PCALLMOD(,
	);
}

void CPerlModule::OnWriteChanConfig(CFile& Config, CChan& Chan) {
	PSTART_IDF(OnWriteChanConfig);
	mXPUSHi(0); // Default value
	PUSH_PTR(CFile*, &Config);
	PUSH_PTR(CChan*, &Chan);
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnDCCUserSend);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR( CNick*, &RemoteNick);
	mXPUSHu(uLongIP);
	mXPUSHu(uPort);
	PUSH_STR(sFile);
	mXPUSHu(uFileSize);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

void CPerlModule::OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {
	PSTART_IDF(OnChanPermission);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHu(uMode);
	mXPUSHi(bAdded);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	PSTART_IDF(OnOp);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	PSTART_IDF(OnDeop);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	PSTART_IDF(OnVoice);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
	PSTART_IDF(OnDevoice);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) {
	PSTART_IDF(OnMode);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR(CChan*, &Channel);
	mXPUSHi(uMode);
	PUSH_STR(sArg);
	mXPUSHi(bAdded);
	mXPUSHi(bNoChange);
	PCALLMOD(,
	);
}

void CPerlModule::OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
	PSTART_IDF(OnRawMode);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sModes);
	PUSH_STR(sArgs);
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnRaw(CString& sLine) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnRaw);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sLine);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sLine = PString(ST(1));
	);
	return result;
}

CModule::EModRet CPerlModule::OnStatusCommand(CString& sCommand) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnStatusCommand);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sCommand);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sCommand = PString(ST(1));
	);
	return result;
}

void CPerlModule::OnModCommand(const CString& sCommand) {
	PSTART_IDF(OnModCommand);
	mXPUSHi(0); // Default value
	PUSH_STR(sCommand);
	PCALLMOD(,
	);
}

void CPerlModule::OnModNotice(const CString& sMessage) {
	PSTART_IDF(OnModNotice);
	mXPUSHi(0); // Default value
	PUSH_STR(sMessage);
	PCALLMOD(,
	);
}

void CPerlModule::OnModCTCP(const CString& sMessage) {
	PSTART_IDF(OnModCTCP);
	mXPUSHi(0); // Default value
	PUSH_STR(sMessage);
	PCALLMOD(,
	);
}

void CPerlModule::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
	PSTART_IDF(OnQuit);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &Nick);
	PUSH_STR(sMessage);
	for (vector<CChan*>::const_iterator i = vChans.begin(); i != vChans.end(); ++i) {
		PUSH_PTR(CChan*, *i);
	}
	PCALLMOD(,
	);
}

void CPerlModule::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {
	PSTART_IDF(OnNick);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &Nick);
	PUSH_STR(sNewNick);
	for (vector<CChan*>::const_iterator i = vChans.begin(); i != vChans.end(); ++i) {
		PUSH_PTR(CChan*, *i);
	}
	PCALLMOD(,
	);
}

void CPerlModule::OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
	PSTART_IDF(OnKick);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &OpNick);
	PUSH_STR(sKickedNick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sMessage);
	PCALLMOD(,
	);
}

void CPerlModule::OnJoin(const CNick& Nick, CChan& Channel) {
	PSTART_IDF(OnJoin);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PCALLMOD(,
	);
}

void CPerlModule::OnPart(const CNick& Nick, CChan& Channel) {
	PSTART_IDF(OnPart);
	mXPUSHi(0); // Default value
	PUSH_PTR( CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnChanBufferStarting(CChan& Chan, CClient& Client) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanBufferStarting);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CChan*, &Chan);
	PUSH_PTR(CClient*, &Client);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanBufferEnding(CChan& Chan, CClient& Client) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanBufferEnding);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CChan*, &Chan);
	PUSH_PTR(CClient*, &Client);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanBufferPlayLine);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CChan*, &Chan);
	PUSH_PTR(CClient*, &Client);
	PUSH_STR(sLine);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sLine = PString(ST(3));
	);
	return result;
}

CModule::EModRet CPerlModule::OnPrivBufferPlayLine(CClient& Client, CString& sLine) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnPrivBufferPlayLine);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CClient*, &Client);
	PUSH_STR(sLine);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sLine = PString(ST(2));
	);
	return result;
}

void CPerlModule::OnClientLogin() {
	PSTART_IDF(OnClientLogin);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

void CPerlModule::OnClientDisconnect() {
	PSTART_IDF(OnClientDisconnect);
	mXPUSHi(0); // Default value
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnUserRaw(CString& sLine) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserRaw);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sLine);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sLine = PString(ST(1));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserCTCPReply(CString& sTarget, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserCTCPReply);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sTarget);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTarget = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserCTCP(CString& sTarget, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserCTCP);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sTarget);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTarget = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserAction(CString& sTarget, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserAction);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sTarget);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTarget = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserMsg(CString& sTarget, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserMsg);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sTarget);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTarget = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserNotice(CString& sTarget, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserNotice);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sTarget);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTarget = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserJoin(CString& sChannel, CString& sKey) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserJoin);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sChannel);
	PUSH_STR(sKey);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sChannel = PString(ST(1));
		sKey = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserPart(CString& sChannel, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserPart);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sChannel);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sChannel = PString(ST(1));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserTopic(CString& sChannel, CString& sTopic) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserTopic);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sChannel);
	PUSH_STR(sTopic);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sChannel = PString(ST(1));
		sTopic = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnUserTopicRequest(CString& sChannel) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnUserTopicRequest);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_STR(sChannel);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sChannel = PString(ST(1));
	);
	return result;
}

CModule::EModRet CPerlModule::OnCTCPReply(CNick& Nick, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnCTCPReply);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnPrivCTCP(CNick& Nick, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnPrivCTCP);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanCTCP);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(3));
	);
	return result;
}

CModule::EModRet CPerlModule::OnPrivAction(CNick& Nick, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnPrivAction);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanAction);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(3));
	);
	return result;
}

CModule::EModRet CPerlModule::OnPrivMsg(CNick& Nick, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnPrivMsg);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanMsg);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(3));
	);
	return result;
}

CModule::EModRet CPerlModule::OnPrivNotice(CNick& Nick, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnPrivNotice);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(2));
	);
	return result;
}

CModule::EModRet CPerlModule::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnChanNotice);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sMessage);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sMessage = PString(ST(3));
	);
	return result;
}

CModule::EModRet CPerlModule::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnTopic);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CNick*, &Nick);
	PUSH_PTR(CChan*, &Channel);
	PUSH_STR(sTopic);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
		sTopic = PString(ST(3));
	);
	return result;
}

bool CPerlModule::OnServerCapAvailable(const CString& sCap) {
	bool result = false;
	PSTART_IDF(OnServerCapAvailable);
	mXPUSHi(static_cast<int>(false)); // Default value
	PUSH_STR(sCap);
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

void CPerlModule::OnServerCapResult(const CString& sCap, bool bSuccess) {
	PSTART_IDF(OnServerCapResult);
	mXPUSHi(0); // Default value
	PUSH_STR(sCap);
	mXPUSHi(bSuccess);
	PCALLMOD(,
	);
}

CModule::EModRet CPerlModule::OnTimerAutoJoin(CChan& Channel) {
	CModule::EModRet result = CONTINUE;
	PSTART_IDF(OnTimerAutoJoin);
	mXPUSHi(static_cast<int>(CONTINUE)); // Default value
	PUSH_PTR(CChan*, &Channel);
	PCALLMOD(,
		result = SvToEModRet(ST(0));
	);
	return result;
}

bool CPerlModule::OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
	bool result = false;
	PSTART_IDF(OnEmbeddedWebRequest);
	mXPUSHi(static_cast<int>(false)); // Default value
	PUSH_PTR(CWebSock*, &WebSock);
	PUSH_STR(sPageName);
	PUSH_PTR(CTemplate*, &Tmpl);
	PCALLMOD(,
		result = SvIV(ST(0));
	);
	return result;
}

