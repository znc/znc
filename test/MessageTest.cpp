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
#include <znc/ZNCString.h>
#include <znc/Message.h>

using ::testing::IsEmpty;
using ::testing::ContainerEq;

TEST(MessageTest, SetParam) {
	CMessage msg;

	msg.SetParam(1, "bar");
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"", "bar"}));

	msg.SetParam(0, "foo");
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"foo", "bar"}));

	msg.SetParam(3, "baz");
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"foo", "bar", "", "baz"}));
}

TEST(MessageTest, GetParams) {
	EXPECT_EQ("", CMessage("CMD").GetParams(0));
	EXPECT_EQ("", CMessage("CMD").GetParams(1));
	EXPECT_EQ("", CMessage("CMD").GetParams(-1));

	EXPECT_EQ("", CMessage("CMD").GetParams(0, 0));
	EXPECT_EQ("", CMessage("CMD").GetParams(1, 0));
	EXPECT_EQ("", CMessage("CMD").GetParams(-1, 0));

	EXPECT_EQ("", CMessage("CMD").GetParams(0, 1));
	EXPECT_EQ("", CMessage("CMD").GetParams(1, 1));
	EXPECT_EQ("", CMessage("CMD").GetParams(-1, 1));

	EXPECT_EQ("", CMessage("CMD").GetParams(0, 10));
	EXPECT_EQ("", CMessage("CMD").GetParams(1, 10));
	EXPECT_EQ("", CMessage("CMD").GetParams(-1, 10));

	EXPECT_EQ("p1 :p2 p3", CMessage("CMD p1 :p2 p3").GetParams(0));
	EXPECT_EQ(":p2 p3", CMessage("CMD p1 :p2 p3").GetParams(1));
	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(-1));

	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(0, 0));
	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(1, 0));
	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(-1, 0));

	EXPECT_EQ("p1", CMessage("CMD p1 :p2 p3").GetParams(0, 1));
	EXPECT_EQ(":p2 p3", CMessage("CMD p1 :p2 p3").GetParams(1, 1));
	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(-1, 1));

	EXPECT_EQ("p1 :p2 p3", CMessage("CMD p1 :p2 p3").GetParams(0, 10));
	EXPECT_EQ(":p2 p3", CMessage("CMD p1 :p2 p3").GetParams(1, 10));
	EXPECT_EQ("", CMessage("CMD p1 :p2 p3").GetParams(-1, 10));
}

TEST(MessageTest, ToString) {
	EXPECT_EQ("CMD", CMessage("CMD").ToString());
	EXPECT_EQ("CMD p1", CMessage("CMD p1").ToString());
	EXPECT_EQ("CMD p1 p2", CMessage("CMD p1 p2").ToString());
	EXPECT_EQ("CMD :p p p", CMessage("CMD :p p p").ToString());
	EXPECT_EQ(":irc.znc.in", CMessage(":irc.znc.in").ToString());
	EXPECT_EQ(":irc.znc.in CMD", CMessage(":irc.znc.in CMD").ToString());
	EXPECT_EQ(":irc.znc.in CMD p1", CMessage(":irc.znc.in CMD p1").ToString());
	EXPECT_EQ(":irc.znc.in CMD p1 p2", CMessage(":irc.znc.in CMD p1 p2").ToString());
	EXPECT_EQ(":irc.znc.in CMD :p p p", CMessage(":irc.znc.in CMD :p p p").ToString());
	EXPECT_EQ(":irc.znc.in CMD :", CMessage(":irc.znc.in CMD :").ToString());

	EXPECT_EQ("CMD", CMessage(CNick(), "CMD").ToString());
	EXPECT_EQ("CMD p1", CMessage(CNick(), "CMD", {"p1"}).ToString());
	EXPECT_EQ("CMD p1 p2", CMessage(CNick(), "CMD", {"p1", "p2"}).ToString());
	EXPECT_EQ("CMD :p p p", CMessage(CNick(), "CMD", {"p p p"}).ToString());
	EXPECT_EQ(":irc.znc.in", CMessage(CNick(":irc.znc.in"), "").ToString());
	EXPECT_EQ(":irc.znc.in CMD", CMessage(CNick(":irc.znc.in"), "CMD").ToString());
	EXPECT_EQ(":irc.znc.in CMD p1", CMessage(CNick(":irc.znc.in"), "CMD", {"p1"}).ToString());
	EXPECT_EQ(":irc.znc.in CMD p1 p2", CMessage(CNick(":irc.znc.in"), "CMD", {"p1", "p2"}).ToString());
	EXPECT_EQ(":irc.znc.in CMD :p p p", CMessage(CNick(":irc.znc.in"), "CMD", {"p p p"}).ToString());
	EXPECT_EQ(":irc.znc.in CMD :", CMessage(CNick(":irc.znc.in"), "CMD", {""}).ToString());

	// #1045 - retain the colon if it was there
	EXPECT_EQ(":services. 328 user #chan http://znc.in", CMessage(":services. 328 user #chan http://znc.in").ToString());
	EXPECT_EQ(":services. 328 user #chan :http://znc.in", CMessage(":services. 328 user #chan :http://znc.in").ToString());
}

TEST(MessageTest, Tags) {
	EXPECT_THAT(CMessage("").GetTags(), IsEmpty());
	EXPECT_THAT(CMessage(":nick!ident@host PRIVMSG #chan :hello world").GetTags(), IsEmpty());

	EXPECT_THAT(CMessage("@a=b").GetTags(), ContainerEq(MCString{{"a","b"}}));
	EXPECT_THAT(CMessage("@a=b :nick!ident@host PRIVMSG #chan :hello world").GetTags(), ContainerEq(MCString{{"a","b"}}));
	EXPECT_THAT(CMessage("@a=b :rest").GetTags(), ContainerEq(MCString{{"a","b"}}));

	EXPECT_THAT(CMessage("@ab=cdef;znc.in/gh-ij=klmn,op :rest").GetTags(), ContainerEq(MCString{{"ab","cdef"},{"znc.in/gh-ij","klmn,op"}}));
	EXPECT_THAT(CMessage("@a===b== :rest").GetTags(), ContainerEq(MCString{{"a","==b=="}}));

	EXPECT_THAT(CMessage("@a;b=c;d :rest").GetTags(), ContainerEq(MCString{{"a",""},{"b","c"},{"d",""}}));

	EXPECT_THAT(CMessage(R"(@semi-colon=\:;space=\s;NUL=\0;backslash=\\;CR=\r;LF=\n :rest)").GetTags(),
	            ContainerEq(MCString{{"semi-colon",";"},{"space"," "},{"NUL",{'\0'}},{"backslash","\\"},{"CR",{'\r'}},{"LF",{'\n'}}}));

	EXPECT_THAT(CMessage(R"(@a=\:\s\\\r\n :rest)").GetTags(), ContainerEq(MCString{{"a","; \\\r\n"}}));

	CMessage msg(":rest");
	msg.SetTags({{"a","b"}});
	EXPECT_EQ("@a=b :rest", msg.ToString());

	msg.SetTags({{"a","b"}, {"c","d"}});
	EXPECT_EQ("@a=b;c=d :rest", msg.ToString());

	msg.SetTags({{"a","b"},{"c","d"},{"e",""}});
	EXPECT_EQ("@a=b;c=d;e :rest", msg.ToString());

	msg.SetTags({{"semi-colon",";"},{"space"," "},{"NUL",{'\0'}},{"backslash","\\"},{"CR",{'\r'}},{"LF",{'\n'}}});
	EXPECT_EQ(R"(@CR=\r;LF=\n;NUL=\0;backslash=\\;semi-colon=\:;space=\s :rest)", msg.ToString());

	msg.SetTags({{"a","; \\\r\n"}});
	EXPECT_EQ(R"(@a=\:\s\\\r\n :rest)", msg.ToString());
}

TEST(MessageTest, FormatFlags) {
	const CString line = "@foo=bar :irc.example.com COMMAND param";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(":irc.example.com COMMAND param", msg.ToString(CMessage::ExcludeTags));
	EXPECT_EQ("@foo=bar COMMAND param", msg.ToString(CMessage::ExcludePrefix));
	EXPECT_EQ("COMMAND param", msg.ToString(CMessage::ExcludePrefix|CMessage::ExcludeTags));
}

TEST(MessageTest, Equals) {
	EXPECT_TRUE(CMessage("JOIN #chan").Equals(CMessage("JOIN #chan")));
	EXPECT_FALSE(CMessage("JOIN #chan").Equals(CMessage("JOIN #znc")));

	EXPECT_TRUE(CMessage(":nick JOIN #chan").Equals(CMessage(":nick JOIN #chan")));
	EXPECT_FALSE(CMessage(":nick JOIN #chan").Equals(CMessage(":nick JOIN #znc")));
	EXPECT_FALSE(CMessage(":nick JOIN #chan").Equals(CMessage(":someone JOIN #chan")));

	EXPECT_TRUE(CMessage("PRIVMSG nick :hi").Equals(CMessage("PRIVMSG nick :hi")));
	EXPECT_TRUE(CMessage("PRIVMSG nick hi").Equals(CMessage("PRIVMSG nick :hi")));
	EXPECT_TRUE(CMessage("PRIVMSG nick :hi").Equals(CMessage("PRIVMSG nick hi")));
	EXPECT_TRUE(CMessage("PRIVMSG nick hi").Equals(CMessage("PRIVMSG nick hi")));

	EXPECT_TRUE(CMessage("CMD nick p1 p2").Equals(CMessage("CMD nick p1 p2")));
	EXPECT_TRUE(CMessage("CMD nick :p1 p2").Equals(CMessage("CMD nick :p1 p2")));
	EXPECT_TRUE(CMessage("CMD nick p1 :p2").Equals(CMessage("CMD nick p1 p2")));
	EXPECT_FALSE(CMessage("CMD nick :p1 p2").Equals(CMessage("CMD nick p1 p2")));

	EXPECT_TRUE(CMessage("@t=now :sender CMD p").Equals(CMessage("@t=then :sender CMD p")));
}

TEST(MessageTest, Type) {
	EXPECT_EQ(CMessage::Type::Unknown, CMessage("FOO").GetType());
	EXPECT_EQ(CMessage::Type::Account, CMessage("ACCOUNT").GetType());
	EXPECT_EQ(CMessage::Type::Away, CMessage("AWAY").GetType());
	EXPECT_EQ(CMessage::Type::Capability, CMessage("CAP").GetType());
	EXPECT_EQ(CMessage::Type::Error, CMessage("ERROR").GetType());
	EXPECT_EQ(CMessage::Type::Invite, CMessage("INVITE").GetType());
	EXPECT_EQ(CMessage::Type::Join, CMessage("JOIN").GetType());
	EXPECT_EQ(CMessage::Type::Kick, CMessage("KICK").GetType());
	EXPECT_EQ(CMessage::Type::Mode, CMessage("MODE").GetType());
	EXPECT_EQ(CMessage::Type::Nick, CMessage("NICK").GetType());
	EXPECT_EQ(CMessage::Type::Notice, CMessage("NOTICE").GetType());
	EXPECT_EQ(CMessage::Type::Numeric, CMessage("123").GetType());
	EXPECT_EQ(CMessage::Type::Part, CMessage("PART").GetType());
	EXPECT_EQ(CMessage::Type::Ping, CMessage("PING").GetType());
	EXPECT_EQ(CMessage::Type::Pong, CMessage("PONG").GetType());
	EXPECT_EQ(CMessage::Type::Quit, CMessage("QUIT").GetType());
	EXPECT_EQ(CMessage::Type::Text, CMessage("PRIVMSG").GetType());
	EXPECT_EQ(CMessage::Type::Topic, CMessage("TOPIC").GetType());
	EXPECT_EQ(CMessage::Type::Wallops, CMessage("WALLOPS").GetType());

	CMessage msg;
	EXPECT_EQ(CMessage::Type::Unknown, msg.GetType());

	msg.SetCommand("PRIVMSG");
	EXPECT_EQ(CMessage::Type::Text, msg.GetType());

	msg.SetParams({"target", "\001ACTION foo\001"});
	EXPECT_EQ(CMessage::Type::Action, msg.GetType());

	msg.SetParam(1, "\001foo\001");
	EXPECT_EQ(CMessage::Type::CTCP, msg.GetType());

	msg.SetCommand("NOTICE");
	EXPECT_EQ(CMessage::Type::CTCP, msg.GetType());

	msg.SetParam(1, "foo");
	EXPECT_EQ(CMessage::Type::Notice, msg.GetType());
}

TEST(MessageTest, Target) {
	CTargetMessage msg;
	msg.Parse(":sender PRIVMSG #chan :foo bar");
	EXPECT_EQ("#chan", msg.GetTarget());
	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	EXPECT_EQ(":sender PRIVMSG #znc :foo bar", msg.ToString());
}

TEST(MessageTest, ChanAction) {
	CActionMessage msg;
	msg.Parse(":sender PRIVMSG #chan :\001ACTION ACTS\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("ACTS", msg.GetText());
	EXPECT_EQ(CMessage::Type::Action, msg.GetType());

	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #chan :\001ACTION foo bar\001", msg.ToString());
}

TEST(MessageTest, ChanCTCP) {
	CCTCPMessage msg;
	msg.Parse(":sender PRIVMSG #chan :\001text\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());
	EXPECT_FALSE(msg.IsReply());
	EXPECT_EQ(CMessage::Type::CTCP, msg.GetType());

	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #chan :\001foo bar\001", msg.ToString());
}

TEST(MessageTest, ChanMsg) {
	CTextMessage msg;
	msg.Parse(":sender PRIVMSG #chan :text");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());
	EXPECT_EQ(CMessage::Type::Text, msg.GetType());

	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #chan :foo bar", msg.ToString());
}

TEST(MessageTest, CTCPReply) {
	CCTCPMessage msg;
	msg.Parse(":sender NOTICE nick :\001FOO bar\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("NOTICE", msg.GetCommand());
	EXPECT_EQ("nick", msg.GetTarget());
	EXPECT_EQ("FOO bar", msg.GetText());
	EXPECT_TRUE(msg.IsReply());
	EXPECT_EQ(CMessage::Type::CTCP, msg.GetType());

	msg.SetText("BAR foo");
	EXPECT_EQ("BAR foo", msg.GetText());
	EXPECT_EQ(":sender NOTICE nick :\001BAR foo\001", msg.ToString());
}

TEST(MessageTest, Kick) {
	CKickMessage msg;
	msg.Parse(":nick KICK #chan person :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("KICK", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("person", msg.GetKickedNick());
	EXPECT_EQ("reason", msg.GetReason());
	EXPECT_EQ(CMessage::Type::Kick, msg.GetType());

	msg.SetKickedNick("noone");
	EXPECT_EQ("noone", msg.GetKickedNick());
	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick KICK #chan noone :test", msg.ToString());
}

TEST(MessageTest, Join) {
	CJoinMessage msg;
	msg.Parse(":nick JOIN #chan");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("JOIN", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ(CMessage::Type::Join, msg.GetType());

	EXPECT_EQ(":nick JOIN #chan", msg.ToString());
}

TEST(MessageTest, Mode) {
	CModeMessage msg;
	msg.Parse(":nick MODE #chan +k foo");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("MODE", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("+k foo", msg.GetModes());

	EXPECT_EQ(":nick MODE #chan +k foo", msg.ToString());

	msg.Parse(":nick MODE nick :+i");
	EXPECT_EQ("+i", msg.GetModes());

	EXPECT_EQ(":nick MODE nick :+i", msg.ToString());
}

TEST(MessageTest, Nick) {
	CNickMessage msg;
	msg.Parse(":nick NICK person");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("NICK", msg.GetCommand());
	EXPECT_EQ("nick", msg.GetOldNick());
	EXPECT_EQ("person", msg.GetNewNick());
	EXPECT_EQ(CMessage::Type::Nick, msg.GetType());

	msg.SetNewNick("test");
	EXPECT_EQ("test", msg.GetNewNick());
	EXPECT_EQ(":nick NICK test", msg.ToString());
}

TEST(MessageTest, Numeric) {
	CNumericMessage msg;
	msg.Parse(":server 123 user :foo bar");
	EXPECT_EQ("server", msg.GetNick().GetNick());
	EXPECT_EQ("123", msg.GetCommand());
	EXPECT_EQ(123u, msg.GetCode());
	EXPECT_EQ(CMessage::Type::Numeric, msg.GetType());

	EXPECT_EQ(":server 123 user :foo bar", msg.ToString());
}

TEST(MessageTest, Part) {
	CPartMessage msg;
	msg.Parse(":nick PART #chan :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("PART", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("reason", msg.GetReason());
	EXPECT_EQ(CMessage::Type::Part, msg.GetType());

	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick PART #chan :test", msg.ToString());
}

TEST(MessageTest, PrivAction) {
	CActionMessage msg;
	msg.Parse(":sender PRIVMSG receiver :\001ACTION ACTS\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("ACTS", msg.GetText());
	EXPECT_EQ(CMessage::Type::Action, msg.GetType());

	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG receiver :\001ACTION foo bar\001", msg.ToString());
}

TEST(MessageTest, PrivCTCP) {
	CCTCPMessage msg;
	msg.Parse(":sender PRIVMSG receiver :\001text\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());
	EXPECT_FALSE(msg.IsReply());
	EXPECT_EQ(CMessage::Type::CTCP, msg.GetType());

	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG receiver :\001foo bar\001", msg.ToString());
}

TEST(MessageTest, PrivMsg) {
	CTextMessage msg;
	msg.Parse(":sender PRIVMSG receiver :foo bar");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(CMessage::Type::Text, msg.GetType());

	msg.SetText(":)");
	EXPECT_EQ(":)", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG receiver ::)", msg.ToString());
}

TEST(MessageTest, Quit) {
	CQuitMessage msg;
	msg.Parse(":nick QUIT :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("QUIT", msg.GetCommand());
	EXPECT_EQ("reason", msg.GetReason());
	EXPECT_EQ(CMessage::Type::Quit, msg.GetType());

	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick QUIT :test", msg.ToString());
}

TEST(MessageTest, Topic) {
	CTopicMessage msg;
	msg.Parse(":nick TOPIC #chan :topic");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("TOPIC", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("topic", msg.GetTopic());
	EXPECT_EQ(CMessage::Type::Topic, msg.GetType());

	msg.SetTopic("test");
	EXPECT_EQ("test", msg.GetTopic());
	EXPECT_EQ(":nick TOPIC #chan :test", msg.ToString());
}

TEST(MessageTest, Parse) {
	CMessage msg;

	// #1037
	msg.Parse(":irc.znc.in PRIVMSG ::)");
	EXPECT_EQ(":)", msg.GetParam(0));
}

// The test data for MessageTest.Parse originates from https://github.com/SaberUK/ircparser
//
// IRCParser - Internet Relay Chat Message Parser
//
//   Copyright (C) 2015 Peter "SaberUK" Powell <petpow@saberuk.com>
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without
// fee is hereby granted, provided that the above copyright notice and this permission notice appear
// in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
// SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.

// when checking a valid message with tags and a source
TEST(MessageTest, ParseWithTags) {
	const CString line = "@tag1=value1;tag2;vendor1/tag3=value2;vendor2/tag4 :irc.example.com COMMAND param1 param2 :param3 param3";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_THAT(msg.GetTags(), ContainerEq(MCString{{"tag1","value1"},{"tag2",""},{"vendor1/tag3","value2"},{"vendor2/tag4",""}}));
	EXPECT_EQ("irc.example.com", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with a source but no tags
TEST(MessageTest, ParseWithoutTags) {
	const CString line = ":irc.example.com COMMAND param1 param2 :param3 param3";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(MCString(), msg.GetTags());
	EXPECT_EQ("irc.example.com", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with tags but no source
TEST(MessageTest, ParseWithoutSource) {
	const CString line = "@tag1=value1;tag2;vendor1/tag3=value2;vendor2/tag4 COMMAND param1 param2 :param3 param3";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_THAT(msg.GetTags(), ContainerEq(MCString{{"tag1","value1"},{"tag2",""},{"vendor1/tag3","value2"},{"vendor2/tag4",""}}));
	EXPECT_EQ("", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_THAT(msg.GetParams(), ContainerEq(VCString{"param1", "param2", "param3 param3"}));
}

// when checking a valid message with no tags, source or parameters
TEST(MessageTest, ParseWithoutSourceAndTags) {
	const CString line = "COMMAND";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(MCString(), msg.GetTags());
	EXPECT_EQ("", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_EQ(VCString(), msg.GetParams());
}
