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

using ::testing::IsEmpty;
using ::testing::ElementsAre;

class ClientTest : public IRCTest {
protected:
	void testPass(const CString& sInput, const CString& sUser, const CString& sIdentifier, const CString& sNetwork, const CString& sPass) const {
		CClient client;
		client.ParsePass(sInput);
		EXPECT_EQ(sUser, client.m_sUser);
		EXPECT_EQ(sIdentifier, client.m_sIdentifier);
		EXPECT_EQ(sNetwork, client.m_sNetwork);
		EXPECT_EQ(sPass, client.m_sPass);
	}

	void testUser(const CString& sInput, const CString& sUser, const CString& sIdentifier, const CString& sNetwork) const {
		CClient client;
		client.ParseUser(sInput);
		EXPECT_EQ(sUser, client.m_sUser);
		EXPECT_EQ(sIdentifier, client.m_sIdentifier);
		EXPECT_EQ(sNetwork, client.m_sNetwork);
	}
};

TEST_F(ClientTest, Pass) {
	testPass("p@ss#w0rd",                                 "",            "",           "",         "p@ss#w0rd");
	testPass("user:p@ss#w0rd",                            "user",        "",           "",         "p@ss#w0rd");
	testPass("user/net-work:p@ss#w0rd",                   "user",        "",           "net-work", "p@ss#w0rd");
	testPass("user@identifier:p@ss#w0rd",                 "user",        "identifier", "",         "p@ss#w0rd");
	testPass("user@identifier/net-work:p@ss#w0rd",        "user",        "identifier", "net-work", "p@ss#w0rd");

	testPass("user@znc.in:p@ss#w0rd",                     "user@znc.in", "",           "",         "p@ss#w0rd");
	testPass("user@znc.in/net-work:p@ss#w0rd",            "user@znc.in", "",           "net-work", "p@ss#w0rd");
	testPass("user@znc.in@identifier:p@ss#w0rd",          "user@znc.in", "identifier", "",         "p@ss#w0rd");
	testPass("user@znc.in@identifier/net-work:p@ss#w0rd", "user@znc.in", "identifier", "net-work", "p@ss#w0rd");
}

TEST_F(ClientTest, User) {
	testUser("user/net-work",                   "user",        "",           "net-work");
	testUser("user@identifier",                 "user",        "identifier", "");
	testUser("user@identifier/net-work",        "user",        "identifier", "net-work");

	testUser("user@znc.in/net-work",            "user@znc.in", "",           "net-work");
	testUser("user@znc.in@identifier",          "user@znc.in", "identifier", "");
	testUser("user@znc.in@identifier/net-work", "user@znc.in", "identifier", "net-work");
}

TEST_F(ClientTest, AccountNotify) {
	CMessage msg(":nick!user@host ACCOUNT accountname");
	EXPECT_FALSE(m_pTestClient->HasAccountNotify());
	m_pTestClient->PutClient(msg);
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());
	m_pTestClient->SetAccountNotify(true);
	EXPECT_TRUE(m_pTestClient->HasAccountNotify());
	m_pTestClient->PutClient(msg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, AwayNotify) {
	CMessage msg(":nick!user@host AWAY :message");
	EXPECT_FALSE(m_pTestClient->HasAwayNotify());
	m_pTestClient->PutClient(msg);
	EXPECT_THAT(m_pTestClient->vsLines, IsEmpty());
	m_pTestClient->SetAwayNotify(true);
	EXPECT_TRUE(m_pTestClient->HasAwayNotify());
	m_pTestClient->PutClient(msg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, MultiPrefixWho) { // aka NAMESX
	m_pTestSock->ReadLine(":server 005 guest PREFIX=(qaohv)~&@%+ NAMESX :are supported by this server");
	m_pTestClient->Reset();

	CMessage msg(":kenny.chatspike.net 352 guest #test grawity broken.symlink *.chatspike.net grawity H@%+ :0 Mantas M.");
	CMessage extmsg(":kenny.chatspike.net 352 guest #test grawity broken.symlink *.chatspike.net grawity H@%+ :0 Mantas M.");
	EXPECT_FALSE(m_pTestClient->HasNamesx());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
	m_pTestClient->SetNamesx(true);
	EXPECT_TRUE(m_pTestClient->HasNamesx());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, MultiPrefixNames) { // aka NAMESX
	m_pTestSock->ReadLine(":server 005 guest PREFIX=(qaohv)~&@%+ NAMESX :are supported by this server");
	m_pTestClient->Reset();

	CMessage msg(":hades.arpa 353 guest = #tethys :~aji &Attila @alyx +KindOne Argure");
	CMessage extmsg(":hades.arpa 353 guest = #tethys :~&@%+aji &@Attila @+alyx +KindOne Argure");
	EXPECT_FALSE(m_pTestClient->HasNamesx());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
	m_pTestClient->SetNamesx(true);
	EXPECT_TRUE(m_pTestClient->HasNamesx());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, UserhostInNames) { // aka UHNAMES
	m_pTestSock->ReadLine(":server 005 guest UHNAMES :are supported by this server");
	m_pTestClient->Reset();

	CMessage msg(":irc.bnc.im 353 guest = #atheme :Rylee somasonic");
	CMessage extmsg(":irc.bnc.im 353 guest = #atheme :Rylee!rylai@localhost somasonic!andrew@somasonic.org");
	EXPECT_FALSE(m_pTestClient->HasUHNames());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
	m_pTestClient->SetUHNames(true);
	EXPECT_TRUE(m_pTestClient->HasUHNames());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, ExtendedJoin) {
	m_pTestSock->ReadLine(":server CAP * ACK :extended-join");
	m_pTestClient->Reset();

	CMessage msg(":nick!user@host JOIN #channel");
	CMessage extmsg(":nick!user@host JOIN #channel account :Real Name");
	EXPECT_FALSE(m_pTestClient->HasExtendedJoin());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
	m_pTestClient->SetExtendedJoin(true);
	EXPECT_TRUE(m_pTestClient->HasExtendedJoin());
	m_pTestClient->PutClient(extmsg);
	EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString(), extmsg.ToString()));
}
