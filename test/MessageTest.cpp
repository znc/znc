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
#include <znc/ZNCString.h>
#include <znc/Message.h>

TEST(MessageTest, SetParam) {
	CMessage msg;

	msg.SetParam(1, "bar");
	msg.SetParam(0, "foo");

	VCString params = {"foo", "bar"};
	EXPECT_EQ(params, msg.GetParams());

	msg.SetParam(3, "baz");

	params = {"foo", "bar", "", "baz"};
	EXPECT_EQ(params, msg.GetParams());
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
}

TEST(MessageTest, FormatFlags) {
	const CString line = "@foo=bar :irc.example.com COMMAND param";

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(":irc.example.com COMMAND param", msg.ToString(CMessage::ExcludeTags));
	EXPECT_EQ("@foo=bar COMMAND param", msg.ToString(CMessage::ExcludePrefix));
	EXPECT_EQ("COMMAND param", msg.ToString(CMessage::ExcludePrefix|CMessage::ExcludeTags));
}

TEST(MessageTest, ChanAction) {
	CActionMessage msg;
	msg.Parse(":sender PRIVMSG #chan :\001ACTION ACTS\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("ACTS", msg.GetText());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #znc :\001ACTION foo bar\001", msg.ToString());
}

TEST(MessageTest, ChanCTCP) {
	CCTCPMessage msg;
	msg.Parse(":sender PRIVMSG #chan :\001text\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #znc :\001foo bar\001", msg.ToString());
}

TEST(MessageTest, ChanMsg) {
	CTextMessage msg;
	msg.Parse(":sender PRIVMSG #chan :text");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG #znc :foo bar", msg.ToString());
}

TEST(MessageTest, Kick) {
	CKickMessage msg;
	msg.Parse(":nick KICK #chan person :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("KICK", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("person", msg.GetKickedNick());
	EXPECT_EQ("reason", msg.GetReason());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetKickedNick("noone");
	EXPECT_EQ("noone", msg.GetKickedNick());
	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick KICK #znc noone test", msg.ToString());
}

TEST(MessageTest, Join) {
	CJoinMessage msg;
	msg.Parse(":nick JOIN #chan");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("JOIN", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	EXPECT_EQ(":nick JOIN #znc", msg.ToString());
}

TEST(MessageTest, Nick) {
	CNickMessage msg;
	msg.Parse(":nick NICK person");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("NICK", msg.GetCommand());
	EXPECT_EQ("nick", msg.GetOldNick());
	EXPECT_EQ("person", msg.GetNewNick());

	msg.SetNewNick("test");
	EXPECT_EQ("test", msg.GetNewNick());
	EXPECT_EQ(":nick NICK test", msg.ToString());
}

TEST(MessageTest, Part) {
	CPartMessage msg;
	msg.Parse(":nick PART #chan :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("PART", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("reason", msg.GetReason());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick PART #znc test", msg.ToString());
}

TEST(MessageTest, PrivAction) {
	CActionMessage msg;
	msg.Parse(":sender PRIVMSG receiver :\001ACTION ACTS\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("ACTS", msg.GetText());

	msg.SetTarget("noone");
	EXPECT_EQ("noone", msg.GetTarget());
	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG noone :\001ACTION foo bar\001", msg.ToString());
}

TEST(MessageTest, PrivCTCP) {
	CCTCPMessage msg;
	msg.Parse(":sender PRIVMSG receiver :\001text\001");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("text", msg.GetText());

	msg.SetTarget("noone");
	EXPECT_EQ("noone", msg.GetTarget());
	msg.SetText("foo bar");
	EXPECT_EQ("foo bar", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG noone :\001foo bar\001", msg.ToString());
}

TEST(MessageTest, PrivMsg) {
	CTextMessage msg;
	msg.Parse(":sender PRIVMSG receiver :foo bar");
	EXPECT_EQ("sender", msg.GetNick().GetNick());
	EXPECT_EQ("PRIVMSG", msg.GetCommand());
	EXPECT_EQ("receiver", msg.GetTarget());
	EXPECT_EQ("foo bar", msg.GetText());

	msg.SetTarget("noone");
	EXPECT_EQ("noone", msg.GetTarget());
	msg.SetText(":)");
	EXPECT_EQ(":)", msg.GetText());
	EXPECT_EQ(":sender PRIVMSG noone ::)", msg.ToString());
}

TEST(MessageTest, Quit) {
	CQuitMessage msg;
	msg.Parse(":nick QUIT :reason");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("QUIT", msg.GetCommand());
	EXPECT_EQ("reason", msg.GetReason());

	msg.SetReason("test");
	EXPECT_EQ("test", msg.GetReason());
	EXPECT_EQ(":nick QUIT test", msg.ToString());
}

TEST(MessageTest, Topic) {
	CTopicMessage msg;
	msg.Parse(":nick TOPIC #chan :topic");
	EXPECT_EQ("nick", msg.GetNick().GetNick());
	EXPECT_EQ("TOPIC", msg.GetCommand());
	EXPECT_EQ("#chan", msg.GetTarget());
	EXPECT_EQ("topic", msg.GetTopic());

	msg.SetTarget("#znc");
	EXPECT_EQ("#znc", msg.GetTarget());
	msg.SetTopic("test");
	EXPECT_EQ("test", msg.GetTopic());
	EXPECT_EQ(":nick TOPIC #znc test", msg.ToString());
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

	MCString tags;
	tags["tag1"] = "value1";
	tags["tag2"] = "";
	tags["vendor1/tag3"] = "value2";
	tags["vendor2/tag4"] = "";

	VCString params = {"param1", "param2", "param3 param3"};

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(tags, msg.GetTags());
	EXPECT_EQ("irc.example.com", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_EQ(params, msg.GetParams());
}

// when checking a valid message with a source but no tags
TEST(MessageTest, ParseWithoutTags) {
	const CString line = ":irc.example.com COMMAND param1 param2 :param3 param3";

	VCString params = {"param1", "param2", "param3 param3"};

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(MCString(), msg.GetTags());
	EXPECT_EQ("irc.example.com", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_EQ(params, msg.GetParams());
}

// when checking a valid message with tags but no source
TEST(MessageTest, ParseWithoutSource) {
	const CString line = "@tag1=value1;tag2;vendor1/tag3=value2;vendor2/tag4 COMMAND param1 param2 :param3 param3";

	MCString tags;
	tags["tag1"] = "value1";
	tags["tag2"] = "";
	tags["vendor1/tag3"] = "value2";
	tags["vendor2/tag4"] = "";

	VCString params = {"param1", "param2", "param3 param3"};

	CMessage msg(line);
	EXPECT_EQ(line, msg.ToString());
	EXPECT_EQ(tags, msg.GetTags());
	EXPECT_EQ("", msg.GetNick().GetNick());
	EXPECT_EQ("COMMAND", msg.GetCommand());
	EXPECT_EQ(params, msg.GetParams());
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
