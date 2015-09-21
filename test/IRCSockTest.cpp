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
#include "IRCTest.h"
#include <algorithm>

using ::testing::IsEmpty;
using ::testing::ElementsAre;
using ::testing::ContainerEq;

class IRCSockTest : public IRCTest {
protected:
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

TEST_F(IRCSockTest, ISupport) {
	MCString m1 = {
		{"CHANTYPES", "#"},
		{"EXCEPTS", ""},
		{"INVEX", ""},
		{"CHANMODES", "eIbq,k,flj,CFLMPQScgimnprstz"},
		{"CHANLIMIT", "#:120"},
		{"PREFIX", "(ov)@+"},
		{"MAXLIST", "bqeI:100"},
		{"MODES", "4"},
		{"NETWORK", "znc"},
		{"KNOCK", ""},
		{"STATUSMSG", "@+"},
		{"CALLERID", "g"},
	};

	m_pTestSock->ReadLine(":irc.znc.in 005 user CHANTYPES=# EXCEPTS INVEX CHANMODES=eIbq,k,flj,CFLMPQScgimnprstz CHANLIMIT=#:120 PREFIX=(ov)@+ MAXLIST=bqeI:100 MODES=4 NETWORK=znc KNOCK STATUSMSG=@+ CALLERID=g :are supported by this server");
	EXPECT_THAT(m_pTestSock->GetISupport(), ContainerEq(m1));
	for (const auto& it : m1) {
		EXPECT_EQ(it.second, m_pTestSock->GetISupport(it.first));
	}

	MCString m2 = {
		{"CASEMAPPING", "rfc1459"},
		{"CHARSET", "ascii"},
		{"NICKLEN", "16"},
		{"CHANNELLEN", "50"},
		{"TOPICLEN", "390"},
		{"ETRACE", ""},
		{"CPRIVMSG", ""},
		{"CNOTICE", ""},
		{"DEAF", "D"},
		{"MONITOR", "100"},
		{"FNC", ""},
		{"TARGMAX", "NAMES:1,LIST:1,KICK:1,WHOIS:1,PRIVMSG:4,NOTICE:4,ACCEPT:,MONITOR:"},
	};

	MCString m12;
	std::merge(m1.begin(), m1.end(), m2.begin(), m2.end(), std::inserter(m12, m12.begin()));

	m_pTestSock->ReadLine(":server 005 user CASEMAPPING=rfc1459 CHARSET=ascii NICKLEN=16 CHANNELLEN=50 TOPICLEN=390 ETRACE CPRIVMSG CNOTICE DEAF=D MONITOR=100 FNC TARGMAX=NAMES:1,LIST:1,KICK:1,WHOIS:1,PRIVMSG:4,NOTICE:4,ACCEPT:,MONITOR: :are supported by this server");
	EXPECT_THAT(m_pTestSock->GetISupport(), ContainerEq(m12));
	for (const auto& it : m2) {
		EXPECT_EQ(it.second, m_pTestSock->GetISupport(it.first));
	}

	MCString m3 = {
		{"EXTBAN", "$,ajrxz"},
		{"WHOX", ""},
		{"CLIENTVER", "3.0"},
		{"SAFELIST", ""},
		{"ELIST", "CTU"},
	};

	MCString m123;
	std::merge(m12.begin(), m12.end(), m3.begin(), m3.end(), std::inserter(m123, m123.begin()));

	m_pTestSock->ReadLine(":server 005 zzzzzz EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server");
	EXPECT_THAT(m_pTestSock->GetISupport(), ContainerEq(m123));
	for (const auto& it : m3) {
		EXPECT_EQ(it.second, m_pTestSock->GetISupport(it.first));
	}

	EXPECT_EQ("default", m_pTestSock->GetISupport("FOOBAR", "default"));
	EXPECT_EQ("3.0", m_pTestSock->GetISupport("CLIENTVER", "default"));
	EXPECT_EQ("", m_pTestSock->GetISupport("SAFELIST", "default"));
}

TEST_F(IRCSockTest, StatusMsg) {
	m_pTestSock->ReadLine(":irc.znc.in 001 me :Welcome to the Internet Relay Network me");
	m_pTestSock->ReadLine(":irc.znc.in 005 me CHANTYPES=# PREFIX=(ov)@+ STATUSMSG=@+ :are supported by this server");

	m_pTestUser->SetAutoClearChanBuffer(false);
	m_pTestSock->ReadLine(":someone PRIVMSG @#chan :hello ops");

	EXPECT_EQ(1u, m_pTestChan->GetBuffer().Size());

	m_pTestUser->SetTimestampPrepend(false);
	EXPECT_EQ(":someone PRIVMSG @#chan :hello ops", m_pTestChan->GetBuffer().GetLine(0, *m_pTestClient));
}
