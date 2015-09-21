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

#ifndef IRCTEST_H
#define IRCTEST_H

#include <gtest/gtest.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/Client.h>
#include <znc/User.h>
#include <znc/Chan.h>
#include <znc/znc.h>
#include <znc/ZNCDebug.h>

class TestClient : public CClient {
public:
	TestClient() { SetNick("me"); }
	bool Write(const CString& sData) override {
		vsLines.push_back(sData.TrimSuffix_n("\r\n"));
		return true;
	}
	void Reset() { vsLines.clear(); }
	void SetAccountNotify(bool bEnabled) { m_bAccountNotify = bEnabled; }
	void SetAwayNotify(bool bEnabled) { m_bAwayNotify = bEnabled; }
	void SetExtendedJoin(bool bEnabled) { m_bExtendedJoin = bEnabled; }
	void SetNamesx(bool bEnabled) { m_bNamesx = bEnabled; }
	void SetUHNames(bool bEnabled) { m_bUHNames = bEnabled; }
	VCString vsLines;
};

class TestIRCSock : public CIRCSock {
public:
	TestIRCSock(CIRCNetwork* pNetwork) : CIRCSock(pNetwork) { m_Nick.SetNick("me"); }
	bool Write(const CString& sData) override {
		vsLines.push_back(sData.TrimSuffix_n("\r\n"));
		return true;
	}
	void Reset() { vsLines.clear(); }
	VCString vsLines;
};

class TestModule : public CModule {
public:
	TestModule() : CModule(nullptr, nullptr, nullptr, "testmod", "", CModInfo::NetworkModule) {}

	EModRet OnCTCPReplyMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnCTCPReplyMessage"); return OnMessage(msg); }
	EModRet OnPrivCTCPMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnPrivCTCPMessage"); return OnMessage(msg); }
	EModRet OnChanCTCPMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnChanCTCPMessage"); return OnMessage(msg); }
	EModRet OnPrivActionMessage(CActionMessage& msg) override { vsHooks.push_back("OnPrivActionMessage"); return OnMessage(msg); }
	EModRet OnChanActionMessage(CActionMessage& msg) override { vsHooks.push_back("OnChanActionMessage"); return OnMessage(msg); }
	EModRet OnPrivMessage(CTextMessage& msg) override { vsHooks.push_back("OnPrivMessage"); return OnMessage(msg); }
	EModRet OnChanMessage(CTextMessage& msg) override { vsHooks.push_back("OnChanMessage"); return OnMessage(msg); }
	EModRet OnPrivNoticeMessage(CNoticeMessage& msg) override { vsHooks.push_back("OnPrivNoticeMessage"); return OnMessage(msg); }
	EModRet OnChanNoticeMessage(CNoticeMessage& msg) override { vsHooks.push_back("OnChanNoticeMessage"); return OnMessage(msg); }
	EModRet OnTopicMessage(CTopicMessage& msg) override { vsHooks.push_back("OnTopicMessage"); return OnMessage(msg); }
	EModRet OnNumericMessage(CNumericMessage& msg) override { vsHooks.push_back("OnNumericMessage"); return OnMessage(msg); }
	void OnJoinMessage(CJoinMessage& msg) override { vsHooks.push_back("OnJoinMessage"); OnMessage(msg); }
	void OnKickMessage(CKickMessage& msg) override { vsHooks.push_back("OnKickMessage"); OnMessage(msg); }
	void OnNickMessage(CNickMessage& msg, const std::vector<CChan*>& vChans) override { vsHooks.push_back("OnNickMessage"); OnMessage(msg); }
	void OnPartMessage(CPartMessage& msg) override { vsHooks.push_back("OnPartMessage"); OnMessage(msg); }
	void OnQuitMessage(CQuitMessage& msg, const std::vector<CChan*>& vChans) override { vsHooks.push_back("OnQuitMessage"); OnMessage(msg); }

	EModRet OnUserCTCPReplyMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnUserCTCPReplyMessage"); return OnMessage(msg); }
	EModRet OnUserCTCPMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnUserCTCPMessage"); return OnMessage(msg); }
	EModRet OnUserActionMessage(CActionMessage& msg) override { vsHooks.push_back("OnUserActionMessage"); return OnMessage(msg); }
	EModRet OnUserTextMessage(CTextMessage& msg) override { vsHooks.push_back("OnUserTextMessage"); return OnMessage(msg); }
	EModRet OnUserNoticeMessage(CNoticeMessage& msg) override { vsHooks.push_back("OnUserNoticeMessage"); return OnMessage(msg); }
	EModRet OnUserJoinMessage(CJoinMessage& msg) override { vsHooks.push_back("OnUserJoinMessage"); return OnMessage(msg); }
	EModRet OnUserPartMessage(CPartMessage& msg) override { vsHooks.push_back("OnUserPartMessage"); return OnMessage(msg); }
	EModRet OnUserTopicMessage(CTopicMessage& msg) override { vsHooks.push_back("OnUserTopicMessage"); return OnMessage(msg); }
	EModRet OnUserQuitMessage(CQuitMessage& msg) override { vsHooks.push_back("OnUserQuitMessage"); return OnMessage(msg); }

	EModRet OnMessage(const CMessage& msg) {
		vsMessages.push_back(msg.ToString());
		vNetworks.push_back(msg.GetNetwork());
		vClients.push_back(msg.GetClient());
		vChannels.push_back(msg.GetChan());
		return eAction;
	}

	void Reset() {
		vsHooks.clear();
		vsMessages.clear();
		vNetworks.clear();
		vClients.clear();
		vChannels.clear();
	}

	VCString vsHooks;
	VCString vsMessages;
	std::vector<CIRCNetwork*> vNetworks;
	std::vector<CClient*> vClients;
	std::vector<CChan*> vChannels;
	EModRet eAction = CONTINUE;
};

class IRCTest : public ::testing::Test {
protected:
	IRCTest() { CDebug::SetDebug(false); CZNC::CreateInstance(); }
	~IRCTest() { CZNC::DestroyInstance(); }

	void SetUp() {
		m_pTestUser = new CUser("user");
		m_pTestNetwork = new CIRCNetwork(m_pTestUser, "network");
		m_pTestChan = new CChan("#chan", m_pTestNetwork, false);
		m_pTestSock = new TestIRCSock(m_pTestNetwork);
		m_pTestClient = new TestClient;
		m_pTestModule = new TestModule;

		m_pTestUser->AddNetwork(m_pTestNetwork);
		m_pTestNetwork->AddChan(m_pTestChan);
		m_pTestChan->AddNick("nick");
		m_pTestClient->AcceptLogin(*m_pTestUser);
		m_pTestClient->vsLines.clear(); // :irc.znc.in 001 me :- Welcome to ZNC -
		CZNC::Get().GetModules().push_back(m_pTestModule);
	}

	void TearDown() {
		m_pTestUser->RemoveNetwork(m_pTestNetwork);
		m_pTestNetwork->ClientDisconnected(m_pTestClient);
		CZNC::Get().GetModules().clear();

		delete m_pTestClient;
		delete m_pTestSock;
		delete m_pTestNetwork;
		delete m_pTestUser;
		delete m_pTestModule;
	}

	CUser* m_pTestUser;
	CIRCNetwork* m_pTestNetwork;
	CChan* m_pTestChan;
	TestIRCSock* m_pTestSock;
	TestClient* m_pTestClient;
	TestModule* m_pTestModule;
};

#endif // IRCTEST_H
