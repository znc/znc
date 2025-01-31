/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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
    void testPass(const CString& sInput, const CString& sUser,
                  const CString& sIdentifier, const CString& sNetwork,
                  const CString& sPass) const {
        CClient client;
        client.ParsePass(sInput);
        EXPECT_EQ(client.m_sUser, sUser);
        EXPECT_EQ(client.m_sIdentifier, sIdentifier);
        EXPECT_EQ(client.m_sNetwork, sNetwork);
        EXPECT_EQ(client.m_sPass, sPass);
    }

    void testUser(const CString& sInput, const CString& sUser,
                  const CString& sIdentifier, const CString& sNetwork) const {
        CClient client;
        client.ParseUser(sInput);
        EXPECT_EQ(client.m_sUser, sUser);
        EXPECT_EQ(client.m_sIdentifier, sIdentifier);
        EXPECT_EQ(client.m_sNetwork, sNetwork);
    }
};

TEST_F(ClientTest, Pass) {
    // clang-format off
    testPass("p@ss#w0rd",                                 "",            "",           "",         "p@ss#w0rd");
    testPass("user:p@ss#w0rd",                            "user",        "",           "",         "p@ss#w0rd");
    testPass("user/net-work:p@ss#w0rd",                   "user",        "",           "net-work", "p@ss#w0rd");
    testPass("user@identifier:p@ss#w0rd",                 "user",        "identifier", "",         "p@ss#w0rd");
    testPass("user@identifier/net-work:p@ss#w0rd",        "user",        "identifier", "net-work", "p@ss#w0rd");

    testPass("user@znc.in:p@ss#w0rd",                     "user@znc.in", "",           "",         "p@ss#w0rd");
    testPass("user@znc.in/net-work:p@ss#w0rd",            "user@znc.in", "",           "net-work", "p@ss#w0rd");
    testPass("user@znc.in@identifier:p@ss#w0rd",          "user@znc.in", "identifier", "",         "p@ss#w0rd");
    testPass("user@znc.in@identifier/net-work:p@ss#w0rd", "user@znc.in", "identifier", "net-work", "p@ss#w0rd");
    // clang-format on
}

TEST_F(ClientTest, User) {
    // clang-format off
    testUser("user/net-work",                   "user",        "",           "net-work");
    testUser("user@identifier",                 "user",        "identifier", "");
    testUser("user@identifier/net-work",        "user",        "identifier", "net-work");

    testUser("user@znc.in/net-work",            "user@znc.in", "",           "net-work");
    testUser("user@znc.in@identifier",          "user@znc.in", "identifier", "");
    testUser("user@znc.in@identifier/net-work", "user@znc.in", "identifier", "net-work");
    // clang-format on
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

TEST_F(ClientTest, AccountTag) {
    m_pTestSock->ReadLine(":server CAP * ACK :account-tag");
    m_pTestClient->Reset();

    CMessage msg(":nick!user@host PRIVMSG #channel :text");
    CMessage extmsg("@account=account-name :nick!user@host PRIVMSG #channel :text");
    EXPECT_FALSE(m_pTestClient->IsTagEnabled("account"));
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
    m_pTestClient->SetTagSupport("account", true);
    EXPECT_TRUE(m_pTestClient->IsTagEnabled("account"));
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines,
                ElementsAre(msg.ToString(), extmsg.ToString()));
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

TEST_F(ClientTest, MultiPrefixWho) {  // aka NAMESX
    m_pTestSock->ReadLine(
        ":server 005 guest PREFIX=(qaohv)~&@%+ NAMESX :are supported by this "
        "server");
    m_pTestClient->Reset();

    CMessage msg(
        ":kenny.chatspike.net 352 guest #test grawity broken.symlink "
        "*.chatspike.net grawity H@%+ :0 Mantas M.");
    CMessage extmsg(
        ":kenny.chatspike.net 352 guest #test grawity broken.symlink "
        "*.chatspike.net grawity H@%+ :0 Mantas M.");
    EXPECT_FALSE(m_pTestClient->HasNamesx());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
    m_pTestClient->SetNamesx(true);
    EXPECT_TRUE(m_pTestClient->HasNamesx());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines,
                ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, MultiPrefixNames) {  // aka NAMESX
    m_pTestSock->ReadLine(
        ":server 005 guest PREFIX=(qaohv)~&@%+ NAMESX :are supported by this "
        "server");
    m_pTestClient->Reset();

    CMessage msg(
        ":hades.arpa 353 guest = #tethys :~aji &Attila @alyx +KindOne Argure");
    CMessage extmsg(
        ":hades.arpa 353 guest = #tethys :~&@%+aji &@Attila @+alyx +KindOne "
        "Argure");
    EXPECT_FALSE(m_pTestClient->HasNamesx());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
    m_pTestClient->SetNamesx(true);
    EXPECT_TRUE(m_pTestClient->HasNamesx());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines,
                ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, UserhostInNames) {  // aka UHNAMES
    m_pTestSock->ReadLine(
        ":server 005 guest UHNAMES :are supported by this server");
    m_pTestClient->Reset();

    CMessage msg(":irc.bnc.im 353 guest = #atheme :Rylee somasonic");
    CMessage extmsg(
        ":irc.bnc.im 353 guest = #atheme :Rylee!rylai@localhost "
        "somasonic!andrew@somasonic.org");
    EXPECT_FALSE(m_pTestClient->HasUHNames());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines, ElementsAre(msg.ToString()));
    m_pTestClient->SetUHNames(true);
    EXPECT_TRUE(m_pTestClient->HasUHNames());
    m_pTestClient->PutClient(extmsg);
    EXPECT_THAT(m_pTestClient->vsLines,
                ElementsAre(msg.ToString(), extmsg.ToString()));
}

TEST_F(ClientTest, StatusMsg) {
    m_pTestSock->ReadLine(
        ":irc.znc.in 001 me :Welcome to the Internet Relay Network me");
    m_pTestSock->ReadLine(
        ":irc.znc.in 005 me CHANTYPES=# PREFIX=(ov)@+ STATUSMSG=@+ :are "
        "supported by this server");

    m_pTestUser->SetAutoClearChanBuffer(false);
    m_pTestClient->ReadLine("PRIVMSG @#chan :hello ops");

    EXPECT_EQ(m_pTestChan->GetBuffer().Size(), 1u);

    m_pTestUser->SetTimestampPrepend(false);
    EXPECT_EQ(m_pTestChan->GetBuffer().GetLine(0, *m_pTestClient),
              ":me PRIVMSG @#chan :hello ops");
}

TEST_F(ClientTest, TagSupport) {
    m_pTestClient->SetTagSupport("test-tag", true);
    CMessage tagmsg("@test-tag=yes;invalid-tag=no :nick!user@host PRIVMSG #chan :text");
    m_pTestClient->PutClient(tagmsg);

    EXPECT_THAT(m_pTestClient->vsLines,
        ElementsAre("@test-tag=yes :nick!user@host PRIVMSG #chan :text"));

    m_pTestClient->Reset();
    m_pTestClient->SetTagSupport("test-tag", false);
    m_pTestClient->PutClient(tagmsg);

    EXPECT_THAT(m_pTestClient->vsLines,
        ElementsAre(":nick!user@host PRIVMSG #chan :text"));
}

TEST_F(ClientTest, OnUserCTCPReplyMessage) {
    CMessage msg("NOTICE someone :\001VERSION 123\001");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    CString sReply =
        "NOTICE someone :\x01VERSION 123 via " + CZNC::GetTag(false) + "\x01";

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserCTCPReplyMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(sReply));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(sReply));
}

TEST_F(ClientTest, OnUserCTCPMessage) {
    CMessage msg("PRIVMSG someone :\001VERSION\001");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserCTCPMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserActionMessage) {
    CMessage msg("PRIVMSG #chan :\001ACTION acts\001");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserActionMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserTextMessage) {
    CMessage msg("PRIVMSG #chan :text");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserTextMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserNoticeMessage) {
    CMessage msg("NOTICE #chan :text");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserNoticeMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserJoinMessage) {
    CMessage msg("JOIN #chan key");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserJoinMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserPartMessage) {
    m_pTestChan->SetIsOn(true);
    CMessage msg("PART #chan");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserPartMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserTopicMessage) {
    CMessage msg("TOPIC #chan :topic");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserTopicMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(m_pTestChan));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
}

TEST_F(ClientTest, OnUserQuitMessage) {
    CMessage msg("QUIT :reason");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnUserQuitMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // quit is never forwarded
}

TEST_F(ClientTest, OnSendToClientMessage) {
    CMessage msg("PRIVMSG #chan :text");
    m_pTestModule->eAction = CModule::HALT;
    m_pTestModule->bSendHooks = true;
    m_pTestClient->PutClient(msg.ToString());

    EXPECT_THAT(m_pTestModule->vsHooks, ElementsAre("OnSendToClientMessage"));
    EXPECT_THAT(m_pTestModule->vsMessages, ElementsAre(msg.ToString()));
    EXPECT_THAT(m_pTestModule->vNetworks, ElementsAre(m_pTestClient->GetNetwork()));
    EXPECT_THAT(m_pTestModule->vClients, ElementsAre(m_pTestClient));
    EXPECT_THAT(m_pTestModule->vChannels, ElementsAre(nullptr));
    EXPECT_THAT(m_pTestSock->vsLines, IsEmpty());  // halt

    m_pTestModule->eAction = CModule::CONTINUE;
    m_pTestClient->ReadLine(msg.ToString());

    EXPECT_THAT(m_pTestSock->vsLines, ElementsAre(msg.ToString()));
    m_pTestModule->bSendHooks = false;
}
