/*
 * Copyright (C) 2004-2021 ZNC, see the NOTICE file for details.
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
#include <znc/Query.h>

using ::testing::SizeIs;
using ::testing::ElementsAre;
using ::testing::MatchesRegex;

class QueryTest : public ::testing::Test {
  protected:
    void SetUp() override { CZNC::CreateInstance(); }
    void TearDown() override { CZNC::DestroyInstance(); }
};

TEST_F(QueryTest, Name) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    CQuery query("query", &network);
    EXPECT_EQ(query.GetName(), "query");
}

TEST_F(QueryTest, AddClearBuffer) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    CQuery query("query", &network);
    EXPECT_TRUE(query.GetBuffer().IsEmpty());
    query.AddBuffer("foo");
    EXPECT_EQ(query.GetBuffer().Size(), 1u);
    query.AddBuffer("bar");
    EXPECT_EQ(query.GetBuffer().Size(), 2u);
    query.ClearBuffer();
    EXPECT_TRUE(query.GetBuffer().IsEmpty());
}

TEST_F(QueryTest, BufferSize) {
    CUser user("user");
    CIRCNetwork network(&user, "network");

    CQuery query("query", &network);
    EXPECT_EQ(user.GetQueryBufferSize(), 50u);
    EXPECT_EQ(query.GetBufferCount(), 50u);

    EXPECT_EQ(CZNC::Get().GetMaxBufferSize(), 500u);
    EXPECT_FALSE(query.SetBufferCount(1000, false));
    EXPECT_EQ(query.GetBufferCount(), 50u);
    EXPECT_TRUE(query.SetBufferCount(500, false));
    EXPECT_EQ(query.GetBufferCount(), 500u);
    EXPECT_TRUE(query.SetBufferCount(1000, true));
    EXPECT_EQ(query.GetBufferCount(), 1000u);
}

TEST_F(QueryTest, SendBuffer) {
    CUser user("user");
    CIRCNetwork network(&user, "network");
    CDebug::SetDebug(false);

    TestClient client;
    client.SetNick("me");
    client.AcceptLogin(user);
    client.Reset();

    CQuery query("query", &network);
    query.AddBuffer(":sender PRIVMSG {target} :{text}", "a message");
    query.AddBuffer(":me PRIVMSG someone :{text}", "a self-message");
    query.AddBuffer(":sender NOTICE #znc :{text}", "a notice");

    client.Reset();
    query.SendBuffer(&client);
    EXPECT_THAT(
        client.vsLines,
        ElementsAre(
            MatchesRegex(R"(:sender PRIVMSG me :\[\d\d:\d\d:\d\d\] a message)"),
            MatchesRegex(
                R"(:sender NOTICE #znc :\[\d\d:\d\d:\d\d\] a notice)")));

    client.Reset();
    user.SetTimestampPrepend(false);
    query.SendBuffer(&client);
    EXPECT_THAT(client.vsLines, ElementsAre(":sender PRIVMSG me :a message",
                                            ":sender NOTICE #znc :a notice"));

    client.Reset();
    user.SetTimestampAppend(true);
    query.SendBuffer(&client);
    EXPECT_THAT(
        client.vsLines,
        ElementsAre(
            MatchesRegex(R"(:sender PRIVMSG me :a message \[\d\d:\d\d:\d\d\])"),
            MatchesRegex(
                R"(:sender NOTICE #znc :a notice \[\d\d:\d\d:\d\d\])")));

    network.ClientDisconnected(&client);
}
