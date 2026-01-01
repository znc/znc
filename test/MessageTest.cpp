/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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
#include <znc/ZNCString.h>
#include <znc/Message.h>

using ::testing::IsEmpty;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::SizeIs;

TEST(MessageTest, SetParam) {
    CMessage msg;

    msg.SetParam(1, "bar");
    EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"", "bar"}));

    msg.SetParam(0, "foo");
    EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"foo", "bar"}));

    msg.SetParam(3, "baz");
    EXPECT_THAT(msg.GetParams(),
                ContainerEq(VCString{"foo", "bar", "", "baz"}));
}

TEST(MessageTest, GetParams) {
    EXPECT_EQ(CMessage("CMD").GetParams(0), "");
    EXPECT_EQ(CMessage("CMD").GetParams(1), "");
    EXPECT_EQ(CMessage("CMD").GetParams(-1), "");

    EXPECT_EQ(CMessage("CMD").GetParams(0, 0), "");
    EXPECT_EQ(CMessage("CMD").GetParams(1, 0), "");
    EXPECT_EQ(CMessage("CMD").GetParams(-1, 0), "");

    EXPECT_EQ(CMessage("CMD").GetParams(0, 1), "");
    EXPECT_EQ(CMessage("CMD").GetParams(1, 1), "");
    EXPECT_EQ(CMessage("CMD").GetParams(-1, 1), "");

    EXPECT_EQ(CMessage("CMD").GetParams(0, 10), "");
    EXPECT_EQ(CMessage("CMD").GetParams(1, 10), "");
    EXPECT_EQ(CMessage("CMD").GetParams(-1, 10), "");

    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(0), "p1 :p2 p3");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(1), ":p2 p3");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(-1), "");

    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(0, 0), "");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(1, 0), "");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(-1, 0), "");

    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(0, 1), "p1");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(1, 1), ":p2 p3");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(-1, 1), "");

    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(0, 10), "p1 :p2 p3");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(1, 10), ":p2 p3");
    EXPECT_EQ(CMessage("CMD p1 :p2 p3").GetParams(-1, 10), "");
}

TEST(MessageTest, GetParamsSplit) {
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(0), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(1), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(-1), IsEmpty());

    EXPECT_THAT(CMessage("CMD").GetParamsSplit(0, 0), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(1, 0), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(-1, 0), IsEmpty());

    EXPECT_THAT(CMessage("CMD").GetParamsSplit(0, 1), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(1, 1), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(-1, 1), IsEmpty());

    EXPECT_THAT(CMessage("CMD").GetParamsSplit(0, 10), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(1, 10), IsEmpty());
    EXPECT_THAT(CMessage("CMD").GetParamsSplit(-1, 10), IsEmpty());

    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(0), ElementsAre("p1", "p2 p3"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(1), ElementsAre("p2 p3"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(-1), IsEmpty());

    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(0, 0), IsEmpty());
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(1, 0), IsEmpty());
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(-1, 0), IsEmpty());

    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(0, 1), ElementsAre("p1"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(1, 1), ElementsAre("p2 p3"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(-1, 1), IsEmpty());

    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(0, 10), ElementsAre("p1", "p2 p3"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(1, 10), ElementsAre("p2 p3"));
    EXPECT_THAT(CMessage("CMD p1 :p2 p3").GetParamsSplit(-1, 10), IsEmpty());

    EXPECT_THAT(CMessage("CMD p1 :").GetParamsSplit(0), ElementsAre("p1", ""));
}

TEST(MessageTest, ToString) {
    EXPECT_EQ(CMessage("CMD").ToString(), "CMD");
    EXPECT_EQ(CMessage("CMD p1").ToString(), "CMD p1");
    EXPECT_EQ(CMessage("CMD p1 p2").ToString(), "CMD p1 p2");
    EXPECT_EQ(CMessage("CMD :p p p").ToString(), "CMD :p p p");
    EXPECT_EQ(CMessage(":irc.znc.in").ToString(), ":irc.znc.in");
    EXPECT_EQ(CMessage(":irc.znc.in CMD").ToString(), ":irc.znc.in CMD");
    EXPECT_EQ(CMessage(":irc.znc.in CMD p1").ToString(), ":irc.znc.in CMD p1");
    EXPECT_EQ(CMessage(":irc.znc.in CMD p1 p2").ToString(),
                       ":irc.znc.in CMD p1 p2");
    EXPECT_EQ(CMessage(":irc.znc.in CMD :p p p").ToString(),
              ":irc.znc.in CMD :p p p");
    EXPECT_EQ(CMessage(":irc.znc.in CMD :").ToString(), ":irc.znc.in CMD :");

    EXPECT_EQ(CMessage(CNick(), "CMD").ToString(), "CMD");
    EXPECT_EQ(CMessage(CNick(), "CMD", {"p1"}).ToString(), "CMD p1");
    EXPECT_EQ(CMessage(CNick(), "CMD", {"p1", "p2"}).ToString(), "CMD p1 p2");
    EXPECT_EQ(CMessage(CNick(), "CMD", {"p p p"}).ToString(), "CMD :p p p");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "").ToString(), ":irc.znc.in");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "CMD").ToString(),
              ":irc.znc.in CMD");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "CMD", {"p1"}).ToString(),
              ":irc.znc.in CMD p1");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "CMD", {"p1", "p2"}).ToString(),
              ":irc.znc.in CMD p1 p2");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "CMD", {"p p p"}).ToString(),
              ":irc.znc.in CMD :p p p");
    EXPECT_EQ(CMessage(CNick(":irc.znc.in"), "CMD", {""}).ToString(),
              ":irc.znc.in CMD :");

    // #1045 - retain the colon if it was there
    EXPECT_EQ(":services. 328 user #chan http://znc.in",
              CMessage(":services. 328 user #chan http://znc.in").ToString());
    EXPECT_EQ(":services. 328 user #chan :http://znc.in",
              CMessage(":services. 328 user #chan :http://znc.in").ToString());
}

TEST(MessageTest, Tags) {
    EXPECT_THAT(CMessage("").GetTags(), IsEmpty());
    EXPECT_THAT(
        CMessage(":nick!ident@host PRIVMSG #chan :hello world").GetTags(),
        IsEmpty());

    EXPECT_THAT(CMessage("@a=b").GetTags(), ContainerEq(MCString{{"a", "b"}}));
    EXPECT_THAT(
        CMessage("@a=b :nick!ident@host PRIVMSG #chan :hello world").GetTags(),
        ContainerEq(MCString{{"a", "b"}}));
    EXPECT_THAT(CMessage("@a=b :rest").GetTags(),
                ContainerEq(MCString{{"a", "b"}}));

    EXPECT_THAT(
        CMessage("@ab=cdef;znc.in/gh-ij=klmn,op :rest").GetTags(),
        ContainerEq(MCString{{"ab", "cdef"}, {"znc.in/gh-ij", "klmn,op"}}));
    EXPECT_THAT(CMessage("@a===b== :rest").GetTags(),
                ContainerEq(MCString{{"a", "==b=="}}));

    EXPECT_THAT(CMessage("@a;b=c;d :rest").GetTags(),
                ContainerEq(MCString{{"a", ""}, {"b", "c"}, {"d", ""}}));

    EXPECT_THAT(
        CMessage(
            R"(@semi-colon=\:;space=\s;NUL=\0;backslash=\\;CR=\r;LF=\n :rest)")
            .GetTags(),
        ContainerEq(MCString{{"semi-colon", ";"},
                             {"space", " "},
                             {"NUL", {'\0'}},
                             {"backslash", "\\"},
                             {"CR", {'\r'}},
                             {"LF", {'\n'}}}));

    EXPECT_THAT(CMessage(R"(@a=\:\s\\\r\n :rest)").GetTags(),
                ContainerEq(MCString{{"a", "; \\\r\n"}}));

    CMessage msg(":rest");
    msg.SetTags({{"a", "b"}});
    EXPECT_EQ(msg.ToString(), "@a=b :rest");

    msg.SetTags({{"a", "b"}, {"c", "d"}});
    EXPECT_EQ(msg.ToString(), "@a=b;c=d :rest");

    msg.SetTags({{"a", "b"}, {"c", "d"}, {"e", ""}});
    EXPECT_EQ(msg.ToString(), "@a=b;c=d;e :rest");

    msg.SetTags({{"semi-colon", ";"},
                 {"space", " "},
                 {"NUL", {'\0'}},
                 {"backslash", "\\"},
                 {"CR", {'\r'}},
                 {"LF", {'\n'}}});
    EXPECT_EQ(
        msg.ToString(),
        R"(@CR=\r;LF=\n;NUL=\0;backslash=\\;semi-colon=\:;space=\s :rest)");

    msg.SetTags({{"a", "; \\\r\n"}});
    EXPECT_EQ(msg.ToString(), R"(@a=\:\s\\\r\n :rest)");
}

TEST(MessageTest, FormatFlags) {
    const CString line = "@foo=bar :irc.example.com COMMAND param";

    CMessage msg(line);
    EXPECT_EQ(msg.ToString(), line);
    EXPECT_EQ(msg.ToString(CMessage::ExcludeTags),
              ":irc.example.com COMMAND param");
    EXPECT_EQ(msg.ToString(CMessage::ExcludePrefix), "@foo=bar COMMAND param");
    EXPECT_EQ(msg.ToString(CMessage::ExcludePrefix | CMessage::ExcludeTags),
              "COMMAND param");
}

TEST(MessageTest, Equals) {
    EXPECT_TRUE(CMessage("JOIN #chan").Equals(CMessage("JOIN #chan")));
    EXPECT_FALSE(CMessage("JOIN #chan").Equals(CMessage("JOIN #znc")));

    EXPECT_TRUE(
        CMessage(":nick JOIN #chan").Equals(CMessage(":nick JOIN #chan")));
    EXPECT_FALSE(
        CMessage(":nick JOIN #chan").Equals(CMessage(":nick JOIN #znc")));
    EXPECT_FALSE(
        CMessage(":nick JOIN #chan").Equals(CMessage(":someone JOIN #chan")));

    EXPECT_TRUE(
        CMessage("PRIVMSG nick :hi").Equals(CMessage("PRIVMSG nick :hi")));
    EXPECT_TRUE(
        CMessage("PRIVMSG nick hi").Equals(CMessage("PRIVMSG nick :hi")));
    EXPECT_TRUE(
        CMessage("PRIVMSG nick :hi").Equals(CMessage("PRIVMSG nick hi")));
    EXPECT_TRUE(
        CMessage("PRIVMSG nick hi").Equals(CMessage("PRIVMSG nick hi")));

    EXPECT_TRUE(CMessage("CMD nick p1 p2").Equals(CMessage("CMD nick p1 p2")));
    EXPECT_TRUE(
        CMessage("CMD nick :p1 p2").Equals(CMessage("CMD nick :p1 p2")));
    EXPECT_TRUE(CMessage("CMD nick p1 :p2").Equals(CMessage("CMD nick p1 p2")));
    EXPECT_FALSE(
        CMessage("CMD nick :p1 p2").Equals(CMessage("CMD nick p1 p2")));

    EXPECT_TRUE(CMessage("@t=now :sender CMD p")
                    .Equals(CMessage("@t=then :sender CMD p")));
}

TEST(MessageTest, Type) {
    EXPECT_EQ(CMessage("FOO").GetType(), CMessage::Type::Unknown);
    EXPECT_EQ(CMessage("ACCOUNT").GetType(), CMessage::Type::Account);
    EXPECT_EQ(CMessage("AWAY").GetType(), CMessage::Type::Away);
    EXPECT_EQ(CMessage("CAP").GetType(), CMessage::Type::Capability);
    EXPECT_EQ(CMessage("ERROR").GetType(), CMessage::Type::Error);
    EXPECT_EQ(CMessage("INVITE").GetType(), CMessage::Type::Invite);
    EXPECT_EQ(CMessage("JOIN").GetType(), CMessage::Type::Join);
    EXPECT_EQ(CMessage("KICK").GetType(), CMessage::Type::Kick);
    EXPECT_EQ(CMessage("MODE").GetType(), CMessage::Type::Mode);
    EXPECT_EQ(CMessage("NICK").GetType(), CMessage::Type::Nick);
    EXPECT_EQ(CMessage("NOTICE").GetType(), CMessage::Type::Notice);
    EXPECT_EQ(CMessage("123").GetType(), CMessage::Type::Numeric);
    EXPECT_EQ(CMessage("PART").GetType(), CMessage::Type::Part);
    EXPECT_EQ(CMessage("PING").GetType(), CMessage::Type::Ping);
    EXPECT_EQ(CMessage("PONG").GetType(), CMessage::Type::Pong);
    EXPECT_EQ(CMessage("QUIT").GetType(), CMessage::Type::Quit);
    EXPECT_EQ(CMessage("PRIVMSG").GetType(), CMessage::Type::Text);
    EXPECT_EQ(CMessage("TOPIC").GetType(), CMessage::Type::Topic);
    EXPECT_EQ(CMessage("WALLOPS").GetType(), CMessage::Type::Wallops);

    CMessage msg;
    EXPECT_EQ(msg.GetType(), CMessage::Type::Unknown);

    msg.SetCommand("PRIVMSG");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Text);

    msg.SetParams({"target", "\001ACTION foo\001"});
    EXPECT_EQ(msg.GetType(), CMessage::Type::Action);

    msg.SetParam(1, "\001foo\001");
    EXPECT_EQ(msg.GetType(), CMessage::Type::CTCP);

    msg.SetCommand("NOTICE");
    EXPECT_EQ(msg.GetType(), CMessage::Type::CTCP);

    msg.SetParam(1, "foo");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Notice);
}

TEST(MessageTest, Target) {
    CTargetMessage msg;
    msg.Parse(":sender PRIVMSG #chan :foo bar");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    msg.SetTarget("#znc");
    EXPECT_EQ(msg.GetTarget(), "#znc");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG #znc :foo bar");
}

TEST(MessageTest, ChanAction) {
    CActionMessage msg;
    msg.Parse(":sender PRIVMSG #chan :\001ACTION ACTS\001");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetText(), "ACTS");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Action);

    msg.SetText("foo bar");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG #chan :\001ACTION foo bar\001");
}

TEST(MessageTest, ChanCTCP) {
    CCTCPMessage msg;
    msg.Parse(":sender PRIVMSG #chan :\001text\001");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetText(), "text");
    EXPECT_FALSE(msg.IsReply());
    EXPECT_EQ(msg.GetType(), CMessage::Type::CTCP);

    msg.SetText("foo bar");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG #chan :\001foo bar\001");
}

TEST(MessageTest, ChanMsg) {
    CTextMessage msg;
    msg.Parse(":sender PRIVMSG #chan :text");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetText(), "text");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Text);

    msg.SetText("foo bar");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG #chan :foo bar");
}

TEST(MessageTest, CTCPReply) {
    CCTCPMessage msg;
    msg.Parse(":sender NOTICE nick :\001FOO bar\001");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "NOTICE");
    EXPECT_EQ(msg.GetTarget(), "nick");
    EXPECT_EQ(msg.GetText(), "FOO bar");
    EXPECT_TRUE(msg.IsReply());
    EXPECT_EQ(msg.GetType(), CMessage::Type::CTCP);

    msg.SetText("BAR foo");
    EXPECT_EQ(msg.GetText(), "BAR foo");
    EXPECT_EQ(msg.ToString(), ":sender NOTICE nick :\001BAR foo\001");
}

TEST(MessageTest, Kick) {
    CKickMessage msg;
    msg.Parse(":nick KICK #chan person :reason");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "KICK");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetKickedNick(), "person");
    EXPECT_EQ(msg.GetReason(), "reason");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Kick);

    msg.SetKickedNick("noone");
    EXPECT_EQ(msg.GetKickedNick(), "noone");
    msg.SetReason("test");
    EXPECT_EQ(msg.GetReason(), "test");
    EXPECT_EQ(msg.ToString(), ":nick KICK #chan noone :test");
}

TEST(MessageTest, Join) {
    CJoinMessage msg;
    msg.Parse(":nick JOIN #chan");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "JOIN");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Join);

    EXPECT_EQ(msg.ToString(), ":nick JOIN #chan");
}

TEST(MessageTest, Mode) {
    CModeMessage msg;
    msg.Parse(":nick MODE #chan +k foo");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "MODE");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetModes(), "+k foo");

    EXPECT_EQ(msg.ToString(), ":nick MODE #chan +k foo");

    msg.Parse(":nick MODE nick :+i");
    EXPECT_EQ(msg.GetModes(), "+i");

    EXPECT_EQ(msg.GetModeList(), "+i");

    EXPECT_EQ(msg.ToString(), ":nick MODE nick :+i");

    msg.Parse(":nick MODE nick +ov Person :Other");

    EXPECT_EQ(msg.GetModeList(), "+ov");
    EXPECT_THAT(msg.GetModeParams(), ElementsAre("Person", "Other"));
}

TEST(MessageTest, Nick) {
    CNickMessage msg;
    msg.Parse(":nick NICK person");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "NICK");
    EXPECT_EQ(msg.GetOldNick(), "nick");
    EXPECT_EQ(msg.GetNewNick(), "person");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Nick);

    msg.SetNewNick("test");
    EXPECT_EQ(msg.GetNewNick(), "test");
    EXPECT_EQ(msg.ToString(), ":nick NICK test");
}

TEST(MessageTest, Numeric) {
    CNumericMessage msg;
    msg.Parse(":server 123 user :foo bar");
    EXPECT_EQ(msg.GetNick().GetNick(), "server");
    EXPECT_EQ(msg.GetCommand(), "123");
    EXPECT_EQ(msg.GetCode(), 123u);
    EXPECT_EQ(msg.GetType(), CMessage::Type::Numeric);

    EXPECT_EQ(msg.ToString(), ":server 123 user :foo bar");
}

TEST(MessageTest, Part) {
    CPartMessage msg;
    msg.Parse(":nick PART #chan :reason");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "PART");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetReason(), "reason");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Part);

    msg.SetReason("test");
    EXPECT_EQ(msg.GetReason(), "test");
    EXPECT_EQ(msg.ToString(), ":nick PART #chan :test");
}

TEST(MessageTest, PrivAction) {
    CActionMessage msg;
    msg.Parse(":sender PRIVMSG receiver :\001ACTION ACTS\001");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "receiver");
    EXPECT_EQ(msg.GetText(), "ACTS");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Action);

    msg.SetText("foo bar");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.ToString(),
              ":sender PRIVMSG receiver :\001ACTION foo bar\001");
}

TEST(MessageTest, PrivCTCP) {
    CCTCPMessage msg;
    msg.Parse(":sender PRIVMSG receiver :\001text\001");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "receiver");
    EXPECT_EQ(msg.GetText(), "text");
    EXPECT_FALSE(msg.IsReply());
    EXPECT_EQ(msg.GetType(), CMessage::Type::CTCP);

    msg.SetText("foo bar");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG receiver :\001foo bar\001");
}

TEST(MessageTest, PrivMsg) {
    CTextMessage msg;
    msg.Parse(":sender PRIVMSG receiver :foo bar");
    EXPECT_EQ(msg.GetNick().GetNick(), "sender");
    EXPECT_EQ(msg.GetCommand(), "PRIVMSG");
    EXPECT_EQ(msg.GetTarget(), "receiver");
    EXPECT_EQ(msg.GetText(), "foo bar");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Text);

    msg.SetText(":)");
    EXPECT_EQ(msg.GetText(), ":)");
    EXPECT_EQ(msg.ToString(), ":sender PRIVMSG receiver ::)");
}

TEST(MessageTest, Quit) {
    CQuitMessage msg;
    msg.Parse(":nick QUIT :reason");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "QUIT");
    EXPECT_EQ(msg.GetReason(), "reason");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Quit);

    msg.SetReason("test");
    EXPECT_EQ(msg.GetReason(), "test");
    EXPECT_EQ(msg.ToString(), ":nick QUIT :test");
}

TEST(MessageTest, Topic) {
    CTopicMessage msg;
    msg.Parse(":nick TOPIC #chan :topic");
    EXPECT_EQ(msg.GetNick().GetNick(), "nick");
    EXPECT_EQ(msg.GetCommand(), "TOPIC");
    EXPECT_EQ(msg.GetTarget(), "#chan");
    EXPECT_EQ(msg.GetTopic(), "topic");
    EXPECT_EQ(msg.GetType(), CMessage::Type::Topic);

    msg.SetTopic("test");
    EXPECT_EQ(msg.GetTopic(), "test");
    EXPECT_EQ(msg.ToString(), ":nick TOPIC #chan :test");
}

TEST(MessageTest, Parse) {
    CMessage msg;

    // #1037
    msg.Parse(":irc.znc.in PRIVMSG ::)");
    EXPECT_EQ(msg.GetParam(0), ":)");
}

// The test data for MessageTest.Parse originates from
// https://github.com/SaberUK/ircparser
//
// IRCParser - Internet Relay Chat Message Parser
//
//   Copyright (C) 2015 Peter "SaberUK" Powell <petpow@saberuk.com>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without
// fee is hereby granted, provided that the above copyright notice and this
// permission notice appear
// in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS
// SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
// NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
// OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
// USE OR PERFORMANCE
// OF THIS SOFTWARE.

// when checking a valid message with tags and a source
TEST(MessageTest, ParseWithTags) {
    const CString line =
        "@tag1=value1;tag2;vendor1/tag3=value2;vendor2/tag4 :irc.example.com "
        "COMMAND param1 param2 :param3 param3";

    CMessage msg(line);
    EXPECT_EQ(msg.ToString(), line);
    EXPECT_THAT(msg.GetTags(), ContainerEq(MCString{{"tag1", "value1"},
                                                    {"tag2", ""},
                                                    {"vendor1/tag3", "value2"},
                                                    {"vendor2/tag4", ""}}));
    EXPECT_EQ(msg.GetNick().GetNick(), "irc.example.com");
    EXPECT_EQ(msg.GetCommand(), "COMMAND");
    EXPECT_THAT(msg.GetParams(),
                ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with a source but no tags
TEST(MessageTest, ParseWithoutTags) {
    const CString line =
        ":irc.example.com COMMAND param1 param2 :param3 param3";

    CMessage msg(line);
    EXPECT_EQ(msg.ToString(), line);
    EXPECT_EQ(msg.GetTags(), MCString());
    EXPECT_EQ(msg.GetNick().GetNick(), "irc.example.com");
    EXPECT_EQ(msg.GetCommand(), "COMMAND");
    EXPECT_THAT(msg.GetParams(),
                ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with tags but no source
TEST(MessageTest, ParseWithoutSource) {
    const CString line =
        "@tag1=value1;tag2;vendor1/tag3=value2;vendor2/tag4 COMMAND param1 "
        "param2 :param3 param3";

    CMessage msg(line);
    EXPECT_EQ(msg.ToString(), line);
    EXPECT_THAT(msg.GetTags(), ContainerEq(MCString{{"tag1", "value1"},
                                                    {"tag2", ""},
                                                    {"vendor1/tag3", "value2"},
                                                    {"vendor2/tag4", ""}}));
    EXPECT_EQ(msg.GetNick().GetNick(), "");
    EXPECT_EQ(msg.GetCommand(), "COMMAND");
    EXPECT_THAT(msg.GetParams(),
                ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with no tags, source or parameters
TEST(MessageTest, ParseWithoutSourceAndTags) {
    const CString line = "COMMAND";

    CMessage msg(line);
    EXPECT_EQ(msg.ToString(), line);
    EXPECT_EQ(msg.GetTags(), MCString());
    EXPECT_EQ(msg.GetNick().GetNick(), "");
    EXPECT_EQ(msg.GetCommand(), "COMMAND");
    EXPECT_EQ(msg.GetParams(), VCString());
}

TEST(MessageTest, HugeParse) {
    CString line;
    for (int i = 0; i < 1000000; ++i) {
        line += "a ";
    }
    CMessage msg(line);
    EXPECT_THAT(msg.GetParams(), SizeIs(999999));
}
