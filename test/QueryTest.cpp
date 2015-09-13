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
#include <znc/Query.h>
#include <znc/User.h>
#include <znc/Client.h>
#include <znc/IRCNetwork.h>
#include <znc/ZNCString.h>
#include <znc/ZNCDebug.h>
#include <znc/znc.h>

using ::testing::SizeIs;
using ::testing::ElementsAre;
using ::testing::MatchesRegex;

class QueryTest : public ::testing::Test {
protected:
	void SetUp() { CZNC::CreateInstance(); }
	void TearDown() { CZNC::DestroyInstance(); }
};

TEST_F(QueryTest, Name) {
	CUser user("user");
	CIRCNetwork network(&user, "network");

	CQuery query("query", &network);
	EXPECT_EQ("query", query.GetName());
}

TEST_F(QueryTest, AddClearBuffer) {
	CUser user("user");
	CIRCNetwork network(&user, "network");

	CQuery query("query", &network);
	EXPECT_TRUE(query.GetBuffer().IsEmpty());
	query.AddBuffer("foo");
	EXPECT_EQ(1u, query.GetBuffer().Size());
	query.AddBuffer("bar");
	EXPECT_EQ(2u, query.GetBuffer().Size());
	query.ClearBuffer();
	EXPECT_TRUE(query.GetBuffer().IsEmpty());
}

TEST_F(QueryTest, BufferSize) {
	CUser user("user");
	CIRCNetwork network(&user, "network");

	CQuery query("query", &network);
	EXPECT_EQ(50u, user.GetQueryBufferSize());
	EXPECT_EQ(50u, query.GetBufferCount());

	EXPECT_EQ(500u, CZNC::Get().GetMaxBufferSize());
	EXPECT_FALSE(query.SetBufferCount(1000, false));
	EXPECT_EQ(50u, query.GetBufferCount());
	EXPECT_TRUE(query.SetBufferCount(500, false));
	EXPECT_EQ(500u, query.GetBufferCount());
	EXPECT_TRUE(query.SetBufferCount(1000, true));
	EXPECT_EQ(1000u, query.GetBufferCount());
}

class TestClient : public CClient {
public:
	bool Write(const CString& sData) override {
		lines.push_back(sData);
		return true;
	}
	VCString lines;
};

TEST_F(QueryTest, SendBuffer) {
	CUser user("user");
	CIRCNetwork network(&user, "network");
	CDebug::SetDebug(false);

	TestClient client;
	client.SetNick("me");
	client.AcceptLogin(user);
	client.lines.clear();

	CQuery query("query", &network);
	query.AddBuffer(":sender PRIVMSG {target} :{text}", "a message");
	query.AddBuffer(":me PRIVMSG someone :{text}", "a self-message");
	query.AddBuffer(":sender NOTICE #znc :{text}", "a notice");

	client.lines.clear();
	query.SendBuffer(&client);
	EXPECT_THAT(client.lines, ElementsAre(MatchesRegex(R"(:sender PRIVMSG me :\[\d\d:\d\d:\d\d\] a message)"),
	                                      MatchesRegex(R"(:sender NOTICE #znc :\[\d\d:\d\d:\d\d\] a notice)")));

	client.lines.clear();
	user.SetTimestampPrepend(false);
	query.SendBuffer(&client);
	EXPECT_THAT(client.lines, ElementsAre(":sender PRIVMSG me :a message", ":sender NOTICE #znc :a notice"));

	client.lines.clear();
	user.SetTimestampAppend(true);
	query.SendBuffer(&client);
	EXPECT_THAT(client.lines, ElementsAre(MatchesRegex(R"(:sender PRIVMSG me :a message \[\d\d:\d\d:\d\d\])"),
	                                      MatchesRegex(R"(:sender NOTICE #znc :a notice \[\d\d:\d\d:\d\d\])")));

	network.ClientDisconnected(&client);
}
