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
	VCString vsLines;
};

class TestIRCSock : public CIRCSock {
public:
	TestIRCSock(CIRCNetwork* pNetwork) : CIRCSock(pNetwork) { m_Nick.SetNick("me"); }
};

class TestModule : public CModule {
public:
	TestModule() : CModule(nullptr, nullptr, nullptr, "testmod", "", CModInfo::NetworkModule) {}

	EModRet OnCTCPReplyMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnCTCPReplyMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnPrivCTCPMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnPrivCTCPMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnChanCTCPMessage(CCTCPMessage& msg) override { vsHooks.push_back("OnChanCTCPMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnPrivActionMessage(CActionMessage& msg) override { vsHooks.push_back("OnPrivActionMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnChanActionMessage(CActionMessage& msg) override { vsHooks.push_back("OnChanActionMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnPrivMessage(CTextMessage& msg) override { vsHooks.push_back("OnPrivMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnChanMessage(CTextMessage& msg) override { vsHooks.push_back("OnChanMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnPrivNoticeMessage(CNoticeMessage& msg) override { vsHooks.push_back("OnPrivNoticeMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnChanNoticeMessage(CNoticeMessage& msg) override { vsHooks.push_back("OnChanNoticeMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnTopicMessage(CTopicMessage& msg) override { vsHooks.push_back("OnTopicMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	EModRet OnNumericMessage(CNumericMessage& msg) override { vsHooks.push_back("OnNumericMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); return eAction; }
	void OnJoinMessage(CJoinMessage& msg) override { vsHooks.push_back("OnJoinMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); }
	void OnKickMessage(CKickMessage& msg) override { vsHooks.push_back("OnKickMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); }
	void OnNickMessage(CNickMessage& msg, const std::vector<CChan*>& vChans) override { vsHooks.push_back("OnNickMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); }
	void OnPartMessage(CPartMessage& msg) override { vsHooks.push_back("OnPartMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); }
	void OnQuitMessage(CQuitMessage& msg, const std::vector<CChan*>& vChans) override { vsHooks.push_back("OnQuitMessage"); vsMessages.push_back(msg.ToString()); vNetworks.push_back(msg.GetNetwork()); vChannels.push_back(msg.GetChan()); }

	void Reset() {
		vsHooks.clear();
		vsMessages.clear();
		vNetworks.clear();
		vChannels.clear();
	}

	VCString vsHooks;
	VCString vsMessages;
	std::vector<CIRCNetwork*> vNetworks;
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
	CIRCSock* m_pTestSock;
	TestClient* m_pTestClient;
	TestModule* m_pTestModule;
};

#endif // IRCTEST_H
