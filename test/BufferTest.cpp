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
#include <znc/Buffer.h>
#include <znc/znc.h>

using ::testing::SizeIs;
using ::testing::ContainerEq;

class BufferTest : public ::testing::Test {
protected:
	void SetUp() { CZNC::CreateInstance(); }
	void TearDown() { CZNC::DestroyInstance(); }
};

TEST_F(BufferTest, BufLine) {
	CBuffer buffer(1);
	buffer.AddLine(CMessage("@key=value :nick PRIVMSG {target} {text}"), "hello there");
	const CBufLine& line = buffer.GetBufLine(0);
	EXPECT_THAT(line.GetTags(), ContainerEq(MCString{{"key","value"}}));
	EXPECT_EQ(":nick PRIVMSG {target} {text}", line.GetFormat());
	EXPECT_EQ("hello there", line.GetText());
	EXPECT_EQ("PRIVMSG", line.GetCommand());
}

TEST_F(BufferTest, LineCount) {
	CBuffer buffer(123);
	EXPECT_EQ(123u, buffer.GetLineCount());

	EXPECT_EQ(500u, CZNC::Get().GetMaxBufferSize());
	EXPECT_FALSE(buffer.SetLineCount(1000, false));
	EXPECT_EQ(123u, buffer.GetLineCount());
	EXPECT_TRUE(buffer.SetLineCount(500, false));
	EXPECT_EQ(500u, buffer.GetLineCount());
	EXPECT_TRUE(buffer.SetLineCount(1000, true));
	EXPECT_EQ(1000u, buffer.GetLineCount());
}

TEST_F(BufferTest, AddLine) {
	CBuffer buffer(2);
	EXPECT_TRUE(buffer.IsEmpty());
	EXPECT_EQ(0u, buffer.Size());

	EXPECT_EQ(1u, buffer.AddLine(CMessage("PRIVMSG nick :msg1")));
	EXPECT_FALSE(buffer.IsEmpty());
	EXPECT_EQ(1u, buffer.Size());
	EXPECT_EQ("PRIVMSG nick :msg1", buffer.GetBufLine(0).GetFormat());

	EXPECT_EQ(2u, buffer.AddLine(CMessage("PRIVMSG nick :msg2")));
	EXPECT_FALSE(buffer.IsEmpty());
	EXPECT_EQ(2u, buffer.Size());
	EXPECT_EQ("PRIVMSG nick :msg1", buffer.GetBufLine(0).GetFormat());
	EXPECT_EQ("PRIVMSG nick :msg2", buffer.GetBufLine(1).GetFormat());

	EXPECT_EQ(2u, buffer.AddLine(CMessage("PRIVMSG nick :msg3")));
	EXPECT_FALSE(buffer.IsEmpty());
	EXPECT_EQ(2u, buffer.Size());
	EXPECT_EQ("PRIVMSG nick :msg2", buffer.GetBufLine(0).GetFormat());
	EXPECT_EQ("PRIVMSG nick :msg3", buffer.GetBufLine(1).GetFormat());

	buffer.SetLineCount(1);
	EXPECT_FALSE(buffer.IsEmpty());
	EXPECT_EQ(1u, buffer.Size());
	EXPECT_EQ("PRIVMSG nick :msg3", buffer.GetBufLine(0).GetFormat());

	buffer.Clear();
	EXPECT_TRUE(buffer.IsEmpty());
	EXPECT_EQ(0u, buffer.Size());

	buffer.SetLineCount(0);
	buffer.AddLine(CMessage("TEST"));
	EXPECT_TRUE(buffer.IsEmpty());
	EXPECT_EQ(0u, buffer.Size());
}

TEST_F(BufferTest, UpdateLine) {
	CBuffer buffer(50);

	VCString lines = {
		":irc.server.com 001 nick :Welcome to the fake Internet Relay Chat Network nick",
		":irc.server.com 002 nick :Your host is irc.server.com[12.34.56.78/6697], running version ircd-fake-1.2.3",
		":irc.server.com 003 nick :This server was created Mon Jul 6 2015 at 16:53:49 UTC",
		":irc.server.com 004 nick irc.server.com ircd-fake-1.2.3 DOQRSZaghilopswz CFILMPQSbcefgijklmnopqrstvz bkloveqjfI",
		":irc.server.com 005 nick CHANTYPES=# EXCEPTS INVEX CHANMODES=eIbq,k,flj,CFLMPQScgimnprstz CHANLIMIT=#:120 PREFIX=(ov)@+ MAXLIST=bqeI:100 MODES=4 NETWORK=fake KNOCK STATUSMSG=@+ CALLERID=g :are supported by this server",
		":irc.server.com 005 nick CASEMAPPING=rfc1459 CHARSET=ascii NICKLEN=16 CHANNELLEN=50 TOPICLEN=390 ETRACE CPRIVMSG CNOTICE DEAF=D MONITOR=100 FNC TARGMAX=NAMES:1,LIST:1,KICK:1,WHOIS:1,PRIVMSG:4,NOTICE:4,ACCEPT:,MONITOR: :are supported by this server",
		":irc.server.com 005 nick EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server",
		":irc.server.com 251 nick :There are 160 users and 92457 invisible on 26 servers",
		":irc.server.com 252 nick 25 :IRC Operators online",
		":irc.server.com 253 nick 10 :unknown connection(s)",
		":irc.server.com 254 nick 63434 :channels formed",
		":irc.server.com 255 nick :I have 9455 clients and 1 servers",
		":irc.server.com 265 nick 9455 9682 :Current local users 9455, max 9682",
		":irc.server.com 266 nick 92617 96692 :Current global users 92617, max 96692",
		":irc.server.com 250 nick :Highest connection count: 9683 (9682 clients) (854623 connections received)"
	};
	EXPECT_THAT(lines, SizeIs(15));

	for (const CString& line : lines) {
		buffer.AddLine(line);
	}
	EXPECT_EQ(15u, buffer.Size());

	EXPECT_EQ(15u, buffer.UpdateLine("002", CMessage(":irc.server.net 002 nick :Your host is irc.server.net[11.22.33.44/6697], running version ircd-fake-3.2.1")));
	EXPECT_EQ(":irc.server.net 002 nick :Your host is irc.server.net[11.22.33.44/6697], running version ircd-fake-3.2.1", buffer.GetBufLine(1).GetFormat());

	EXPECT_EQ(15u, buffer.UpdateLine("252", CMessage(":irc.server.com 252 nick 100 :IRC Operators online")));
	EXPECT_EQ(":irc.server.com 252 nick 100 :IRC Operators online", buffer.GetBufLine(8).GetFormat());

	EXPECT_EQ(16u, buffer.UpdateLine("123", CMessage(":irc.server.com 123 nick foo bar")));
	EXPECT_EQ(":irc.server.com 123 nick foo bar", buffer.GetBufLine(15).GetFormat());

	EXPECT_EQ(16u, buffer.UpdateExactLine(CMessage(":irc.server.com 005 nick EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server")));
	EXPECT_EQ(":irc.server.com 005 nick EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server", buffer.GetBufLine(6).GetFormat());

	EXPECT_EQ(17u, buffer.UpdateExactLine(CMessage(":irc.server.com 005 nick FOO=bar :are supported by this server")));
	EXPECT_EQ(":irc.server.com 005 nick FOO=bar :are supported by this server", buffer.GetBufLine(16).GetFormat());
}
