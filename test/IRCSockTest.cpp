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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>
#include <znc/Message.h>
#include <znc/IRCNetwork.h>
#include <znc/Client.h>
#include <znc/User.h>
#include <znc/Chan.h>
#include <znc/znc.h>

using ::testing::IsEmpty;
using ::testing::ElementsAre;

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

class IRCSockTest : public ::testing::Test {
protected:
	IRCSockTest() { CDebug::SetDebug(false); CZNC::CreateInstance(); }
	~IRCSockTest() { CZNC::DestroyInstance(); }

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

TEST_F(IRCSockTest, OnAccountMessage) {
	CMessage msg(":nick!user@host ACCOUNT accountname");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnAccountMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // account-notify disabled
}

TEST_F(IRCSockTest, OnActionMessage) {
	CMessage msg(":nick PRIVMSG #chan :\001ACTION hello\001");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnChanActionMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));

	m_pTestClient->Reset();
	m_pTestModule->Reset();

	msg.Parse(":nick PRIVMSG me :\001ACTION hello\001");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnPrivActionMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnAwayMessage) {
	CMessage msg(":nick!user@host AWAY :reason");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnAwayMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // away-notify disabled
}

TEST_F(IRCSockTest, OnCapabilityMessage) {
	CMessage msg(":server CAP * LS :multi-prefix sasl");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnCapabilityMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());
}

TEST_F(IRCSockTest, OnCTCPMessage) {
	CMessage msg(":nick PRIVMSG #chan :\001VERSION\001");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnChanCTCPMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));

	m_pTestClient->Reset();
	m_pTestModule->Reset();

	msg.Parse(":nick PRIVMSG me :\001VERSION\001");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnPrivCTCPMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));

	m_pTestClient->Reset();
	m_pTestModule->Reset();

	msg.Parse(":nick NOTICE me :\001VERSION ZNC 0.0\001");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnCTCPReplyMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnErrorMessage) {
	CMessage msg(":server ERROR :foo bar");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnErrorMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(":*status!znc@znc.in PRIVMSG me :Error from Server [foo bar]"));
}

TEST_F(IRCSockTest, OnInviteMessage) {
	CMessage msg(":nick INVITE me #znc");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnInviteMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnJoinMessage) {
	CMessage msg(":somebody JOIN #chan");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnJoinMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
}

TEST_F(IRCSockTest, OnKickMessage) {
	CMessage msg(":nick KICK #chan person :reason");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnKickMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
}

TEST_F(IRCSockTest, OnModeMessage) {
	CMessage msg(":nick MODE #chan +k key");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnModeMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnNickMessage) {
	CMessage msg(":nobody NICK nick");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnNickMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());

	msg.Parse(":nick NICK somebody");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnNoticeMessage) {
	CMessage msg(":nick NOTICE #chan :hello");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnChanNoticeMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));

	m_pTestClient->Reset();
	m_pTestModule->Reset();

	msg.Parse(":nick NOTICE me :hello");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnPrivNoticeMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnNumericMessage) {
	CMessage msg(":irc.server.com 372 nick :motd");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnNumericMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnPartMessage) {
	CMessage msg(":nick PART #chan :reason");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnPartMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
}

TEST_F(IRCSockTest, OnPingMessage) {
	CMessage msg(":server PING :arg");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty());
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());
}

TEST_F(IRCSockTest, OnPongMessage) {
	CMessage msg(":server PONG :arg");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty());
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());
}

TEST_F(IRCSockTest, OnQuitMessage) {
	CMessage msg(":nobody QUIT :bye");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnQuitMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));

	msg.Parse(":nick QUIT :bye");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnTextMessage) {
	CMessage msg(":nick PRIVMSG #chan :hello");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnChanMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));

	m_pTestClient->Reset();
	m_pTestModule->Reset();

	msg.Parse(":nick PRIVMSG me :hello");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnPrivMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnTopicMessage) {
	CMessage msg(":nick TOPIC #chan :topic");
	m_pTestModule->eAction = CModule::HALT;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnTopicMessage"));
	EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
	EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestNetwork));
	EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty()); // halt

	m_pTestModule->eAction = CModule::CONTINUE;
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(IRCSockTest, OnWallopsMessage) {
	CMessage msg(":blub!dummy@rox-8DBEFE92 WALLOPS :this is a test");
	m_pTestSock->ReadLine(msg.ToString());

	EXPECT_THAT(m_pTestModule->vsHooks, IsEmpty()); // no OnWallopsMessage() hook (yet?)
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}
