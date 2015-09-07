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
#include <znc/Modules.h>
#include <znc/znc.h>

class ModulesTest : public ::testing::Test {
protected:
	void SetUp() { CZNC::CreateInstance(); }
	void TearDown() { CZNC::DestroyInstance(); }
};

class CLegacyModule : public CModule {
public:
	CLegacyModule() : CModule(nullptr, nullptr, nullptr, "legacy", "", CModInfo::NetworkModule) {}

	EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override { sTarget = "#legacy"; sMessage = "CLegacyModule::OnUserCTCPReply"; return eAction; }
	EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override { sTarget = "#legacy"; sMessage = "CLegacyModule::OnUserCTCP"; return eAction; }
	EModRet OnUserAction(CString& sTarget, CString& sMessage) override { sTarget = "#legacy"; sMessage = "CLegacyModule::OnUserAction"; return eAction; }
	EModRet OnUserMsg(CString& sTarget, CString& sMessage) override { sTarget = "#legacy"; sMessage = "CLegacyModule::OnUserMsg"; return eAction; }
	EModRet OnUserNotice(CString& sTarget, CString& sMessage) override { sTarget = "#legacy"; sMessage = "CLegacyModule::OnUserNotice"; return eAction; }
	EModRet OnUserJoin(CString& sChannel, CString& sKey) override { sChannel = "#legacy"; sKey = "CLegacyModule::OnUserJoin"; return eAction; }
	EModRet OnUserPart(CString& sChannel, CString& sMessage) override { sChannel = "#legacy"; sMessage = "CLegacyModule::OnUserPart"; return eAction; }
	EModRet OnUserTopic(CString& sChannel, CString& sTopic) override { sChannel = "#legacy"; sTopic = "CLegacyModule::OnUserTopic"; return eAction; }
	EModRet OnUserQuit(CString& sMessage) override { sMessage = "CLegacyModule::OnUserQuit"; return eAction; }

	EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnCTCPReply"; return eAction; }
	EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnPrivCTCP"; return eAction; }
	EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnChanCTCP"; return eAction; }
	EModRet OnPrivAction(CNick& Nick, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnPrivAction"; return eAction; }
	EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnChanAction"; return eAction; }
	EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnPrivMsg"; return eAction; }
	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnChanMsg"; return eAction; }
	EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnPrivNotice"; return eAction; }
	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override { Nick.Parse("legacy!znc@znc.in"); sMessage = "CLegacyModule::OnChanNotice"; return eAction; }
	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override { Nick.Parse("legacy!znc@znc.in"); sTopic = "CLegacyModule::OnTopic"; return eAction; }

	EModRet eAction = CONTINUE;
};

class CMessageModule : public CModule {
public:
	CMessageModule() : CModule(nullptr, nullptr, nullptr, "CMessage", "", CModInfo::NetworkModule) {}

	EModRet OnUserCTCPReplyMessage(CCTCPMessage& Message) override { Message.SetTarget("#target"); Message.SetText("CMessageModule::OnUserCTCPReplyMessage"); return eAction; }
	EModRet OnUserCTCPMessage(CCTCPMessage& Message) override { Message.SetTarget("#target"); Message.SetText("CMessageModule::OnUserCTCPMessage"); return eAction; }
	EModRet OnUserActionMessage(CActionMessage& Message) override { Message.SetTarget("#target"); Message.SetText("CMessageModule::OnUserActionMessage"); return eAction; }
	EModRet OnUserTextMessage(CTextMessage& Message) override { Message.SetTarget("#target"); Message.SetText("CMessageModule::OnUserTextMessage"); return eAction; }
	EModRet OnUserNoticeMessage(CNoticeMessage& Message) override { Message.SetTarget("#target"); Message.SetText("CMessageModule::OnUserNoticeMessage"); return eAction; }
	EModRet OnUserJoinMessage(CJoinMessage& Message) override { Message.SetTarget("#target"); Message.SetKey("CMessageModule::OnUserJoinMessage"); return eAction; }
	EModRet OnUserPartMessage(CPartMessage& Message) override { Message.SetTarget("#target"); Message.SetReason("CMessageModule::OnUserPartMessage"); return eAction; }
	EModRet OnUserTopicMessage(CTopicMessage& Message) override { Message.SetTarget("#target"); Message.SetTopic("CMessageModule::OnUserTopicMessage"); return eAction; }
	EModRet OnUserQuitMessage(CQuitMessage& Message) override { Message.SetReason("CMessageModule::OnUserQuitMessage"); return eAction; }

	EModRet OnCTCPReplyMessage(CCTCPMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnCTCPReplyMessage"); return eAction; }
	EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnPrivCTCPMessage"); return eAction; }
	EModRet OnChanCTCPMessage(CCTCPMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnChanCTCPMessage"); return eAction; }
	EModRet OnPrivActionMessage(CActionMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnPrivActionMessage"); return eAction; }
	EModRet OnChanActionMessage(CActionMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnChanActionMessage"); return eAction; }
	EModRet OnPrivMessage(CTextMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnPrivMessage"); return eAction; }
	EModRet OnChanMessage(CTextMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnChanMessage"); return eAction; }
	EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnPrivNoticeMessage"); return eAction; }
	EModRet OnChanNoticeMessage(CNoticeMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetText("CMessageModule::OnChanNoticeMessage"); return eAction; }
	EModRet OnTopicMessage(CTopicMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetTopic("CMessageModule::OnTopicMessage"); return eAction; }
	EModRet OnNumericMessage(CNumericMessage& Message) override { Message.GetNick().SetNick("nick"); Message.SetCommand("123"); return eAction; }

	EModRet eAction = CONTINUE;
};

TEST_F(ModulesTest, Hooks) {
	CModules& Modules = CZNC::Get().GetModules();

	CLegacyModule LegacyMod;
	Modules.push_back(&LegacyMod);

	CMessageModule MessageMod;
	Modules.push_back(&MessageMod);

	CCTCPMessage UserCTCPReply;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserCTCPReplyMessage(UserCTCPReply);
	EXPECT_EQ("#legacy", UserCTCPReply.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserCTCPReply", UserCTCPReply.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserCTCPReplyMessage(UserCTCPReply);
	EXPECT_EQ("#target", UserCTCPReply.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserCTCPReplyMessage", UserCTCPReply.GetText());

	CCTCPMessage UserCTCPMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserCTCPMessage(UserCTCPMsg);
	EXPECT_EQ("#legacy", UserCTCPMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserCTCP", UserCTCPMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserCTCPMessage(UserCTCPMsg);
	EXPECT_EQ("#target", UserCTCPMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserCTCPMessage", UserCTCPMsg.GetText());

	CActionMessage UserActionMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserActionMessage(UserActionMsg);
	EXPECT_EQ("#legacy", UserActionMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserAction", UserActionMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserActionMessage(UserActionMsg);
	EXPECT_EQ("#target", UserActionMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserActionMessage", UserActionMsg.GetText());

	CTextMessage UserTextMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserTextMessage(UserTextMsg);
	EXPECT_EQ("#legacy", UserTextMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserMsg", UserTextMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserTextMessage(UserTextMsg);
	EXPECT_EQ("#target", UserTextMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserTextMessage", UserTextMsg.GetText());

	CNoticeMessage UserNoticeMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserNoticeMessage(UserNoticeMsg);
	EXPECT_EQ("#legacy", UserNoticeMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserNotice", UserNoticeMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserNoticeMessage(UserNoticeMsg);
	EXPECT_EQ("#target", UserNoticeMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserNoticeMessage", UserNoticeMsg.GetText());

	CJoinMessage UserJoinMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserJoinMessage(UserJoinMsg);
	EXPECT_EQ("#legacy", UserJoinMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserJoin", UserJoinMsg.GetKey());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserJoinMessage(UserJoinMsg);
	EXPECT_EQ("#target", UserJoinMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserJoinMessage", UserJoinMsg.GetKey());

	CPartMessage UserPartMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserPartMessage(UserPartMsg);
	EXPECT_EQ("#legacy", UserPartMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserPart", UserPartMsg.GetReason());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserPartMessage(UserPartMsg);
	EXPECT_EQ("#target", UserPartMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserPartMessage", UserPartMsg.GetReason());

	CTopicMessage UserTopicMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserTopicMessage(UserTopicMsg);
	EXPECT_EQ("#legacy", UserTopicMsg.GetTarget());
	EXPECT_EQ("CLegacyModule::OnUserTopic", UserTopicMsg.GetTopic());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserTopicMessage(UserTopicMsg);
	EXPECT_EQ("#target", UserTopicMsg.GetTarget());
	EXPECT_EQ("CMessageModule::OnUserTopicMessage", UserTopicMsg.GetTopic());

	CQuitMessage UserQuitMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnUserQuitMessage(UserQuitMsg);
	EXPECT_EQ("CLegacyModule::OnUserQuit", UserQuitMsg.GetReason());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnUserQuitMessage(UserQuitMsg);
	EXPECT_EQ("CMessageModule::OnUserQuitMessage", UserQuitMsg.GetReason());

	CCTCPMessage CTCPReply;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnCTCPReplyMessage(CTCPReply);
	EXPECT_EQ("legacy", CTCPReply.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnCTCPReply", CTCPReply.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnCTCPReplyMessage(CTCPReply);
	EXPECT_EQ("nick", CTCPReply.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnCTCPReplyMessage", CTCPReply.GetText());

	CCTCPMessage PrivCTCP;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnPrivCTCPMessage(PrivCTCP);
	EXPECT_EQ("legacy", PrivCTCP.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnPrivCTCP", PrivCTCP.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnPrivCTCPMessage(PrivCTCP);
	EXPECT_EQ("nick", PrivCTCP.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnPrivCTCPMessage", PrivCTCP.GetText());

	CCTCPMessage ChanCTCP;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnChanCTCPMessage(ChanCTCP);
	EXPECT_EQ("legacy", ChanCTCP.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnChanCTCP", ChanCTCP.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnChanCTCPMessage(ChanCTCP);
	EXPECT_EQ("nick", ChanCTCP.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnChanCTCPMessage", ChanCTCP.GetText());

	CActionMessage PrivAction;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnPrivActionMessage(PrivAction);
	EXPECT_EQ("legacy", PrivAction.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnPrivAction", PrivAction.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnPrivActionMessage(PrivAction);
	EXPECT_EQ("nick", PrivAction.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnPrivActionMessage", PrivAction.GetText());

	CActionMessage ChanAction;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnChanActionMessage(ChanAction);
	EXPECT_EQ("legacy", ChanAction.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnChanAction", ChanAction.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnChanActionMessage(ChanAction);
	EXPECT_EQ("nick", ChanAction.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnChanActionMessage", ChanAction.GetText());

	CTextMessage PrivMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnPrivMessage(PrivMsg);
	EXPECT_EQ("legacy", PrivMsg.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnPrivMsg", PrivMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnPrivMessage(PrivMsg);
	EXPECT_EQ("nick", PrivMsg.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnPrivMessage", PrivMsg.GetText());

	CTextMessage ChanMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnChanMessage(ChanMsg);
	EXPECT_EQ("legacy", ChanMsg.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnChanMsg", ChanMsg.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnChanMessage(ChanMsg);
	EXPECT_EQ("nick", ChanMsg.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnChanMessage", ChanMsg.GetText());

	CNoticeMessage PrivNotice;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnPrivNoticeMessage(PrivNotice);
	EXPECT_EQ("legacy", PrivNotice.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnPrivNotice", PrivNotice.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnPrivNoticeMessage(PrivNotice);
	EXPECT_EQ("nick", PrivNotice.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnPrivNoticeMessage", PrivNotice.GetText());

	CNoticeMessage ChanNotice;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnChanNoticeMessage(ChanNotice);
	EXPECT_EQ("legacy", ChanNotice.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnChanNotice", ChanNotice.GetText());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnChanNoticeMessage(ChanNotice);
	EXPECT_EQ("nick", ChanNotice.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnChanNoticeMessage", ChanNotice.GetText());

	CTopicMessage TopicMsg;
	LegacyMod.eAction = CModule::HALT;
	Modules.OnTopicMessage(TopicMsg);
	EXPECT_EQ("legacy", TopicMsg.GetNick().GetNick());
	EXPECT_EQ("CLegacyModule::OnTopic", TopicMsg.GetTopic());
	LegacyMod.eAction = CModule::CONTINUE;
	Modules.OnTopicMessage(TopicMsg);
	EXPECT_EQ("nick", TopicMsg.GetNick().GetNick());
	EXPECT_EQ("CMessageModule::OnTopicMessage", TopicMsg.GetTopic());

	CNumericMessage NumericMsg;
	Modules.OnNumericMessage(NumericMsg);
	EXPECT_EQ("nick", TopicMsg.GetNick().GetNick());
	EXPECT_EQ(123u, NumericMsg.GetCode());

	Modules.clear();
}
