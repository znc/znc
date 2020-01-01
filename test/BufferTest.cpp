/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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
    void SetUp() override { CZNC::CreateInstance(); }
    void TearDown() override { CZNC::DestroyInstance(); }
};

TEST_F(BufferTest, BufLine) {
    CBuffer buffer(1);
    buffer.AddLine(CMessage("@key=value :nick PRIVMSG {target} {text}"),
                   "hello there");
    const CBufLine& line = buffer.GetBufLine(0);
    EXPECT_THAT(line.GetTags(), ContainerEq(MCString{{"key", "value"}}));
    EXPECT_EQ(line.GetFormat(), ":nick PRIVMSG {target} {text}");
    EXPECT_EQ(line.GetText(), "hello there");
    EXPECT_EQ(line.GetCommand(), "PRIVMSG");
}

TEST_F(BufferTest, LineCount) {
    CBuffer buffer(123);
    EXPECT_EQ(buffer.GetLineCount(), 123u);

    EXPECT_EQ(CZNC::Get().GetMaxBufferSize(), 500u);
    EXPECT_FALSE(buffer.SetLineCount(1000, false));
    EXPECT_EQ(buffer.GetLineCount(), 123u);
    EXPECT_TRUE(buffer.SetLineCount(500, false));
    EXPECT_EQ(buffer.GetLineCount(), 500u);
    EXPECT_TRUE(buffer.SetLineCount(1000, true));
    EXPECT_EQ(buffer.GetLineCount(), 1000u);
}

TEST_F(BufferTest, AddLine) {
    CBuffer buffer(2);
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 0u);

    EXPECT_EQ(buffer.AddLine(CMessage("PRIVMSG nick :msg1")), 1u);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 1u);
    EXPECT_EQ(buffer.GetBufLine(0).GetFormat(), "PRIVMSG nick :msg1");

    EXPECT_EQ(buffer.AddLine(CMessage("PRIVMSG nick :msg2")), 2u);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 2u);
    EXPECT_EQ(buffer.GetBufLine(0).GetFormat(), "PRIVMSG nick :msg1");
    EXPECT_EQ(buffer.GetBufLine(1).GetFormat(), "PRIVMSG nick :msg2");

    EXPECT_EQ(buffer.AddLine(CMessage("PRIVMSG nick :msg3")), 2u);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 2u);
    EXPECT_EQ(buffer.GetBufLine(0).GetFormat(), "PRIVMSG nick :msg2");
    EXPECT_EQ(buffer.GetBufLine(1).GetFormat(), "PRIVMSG nick :msg3");

    buffer.SetLineCount(1);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 1u);
    EXPECT_EQ(buffer.GetBufLine(0).GetFormat(), "PRIVMSG nick :msg3");

    buffer.Clear();
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 0u);

    buffer.SetLineCount(0);
    buffer.AddLine(CMessage("TEST"));
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST_F(BufferTest, UpdateLine) {
    // clang-format off
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
    EXPECT_EQ(buffer.Size(), 15u);

    EXPECT_EQ(buffer.UpdateLine("002", CMessage(":irc.server.net 002 nick :Your host is irc.server.net[11.22.33.44/6697], running version ircd-fake-3.2.1")), 15u);
    EXPECT_EQ(buffer.GetBufLine(1).GetFormat(), ":irc.server.net 002 nick :Your host is irc.server.net[11.22.33.44/6697], running version ircd-fake-3.2.1");

    EXPECT_EQ(buffer.UpdateLine("252", CMessage(":irc.server.com 252 nick 100 :IRC Operators online")), 15u);
    EXPECT_EQ(buffer.GetBufLine(8).GetFormat(), ":irc.server.com 252 nick 100 :IRC Operators online");

    EXPECT_EQ(buffer.UpdateLine("123", CMessage(":irc.server.com 123 nick foo bar")), 16u);
    EXPECT_EQ(buffer.GetBufLine(15).GetFormat(), ":irc.server.com 123 nick foo bar");

    EXPECT_EQ(buffer.UpdateExactLine(CMessage(":irc.server.com 005 nick EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server")), 16u);
    EXPECT_EQ(buffer.GetBufLine(6).GetFormat(), ":irc.server.com 005 nick EXTBAN=$,ajrxz WHOX CLIENTVER=3.0 SAFELIST ELIST=CTU :are supported by this server");

    EXPECT_EQ(buffer.UpdateExactLine(CMessage(":irc.server.com 005 nick FOO=bar :are supported by this server")), 17u);
    EXPECT_EQ(buffer.GetBufLine(16).GetFormat(), ":irc.server.com 005 nick FOO=bar :are supported by this server");
    // clang-format on
}
