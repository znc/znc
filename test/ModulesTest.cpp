/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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
    void SetUp() override { CZNC::CreateInstance(); }
    void TearDown() override { CZNC::DestroyInstance(); }
};

class CLegacyModule : public CModule {
  public:
    CLegacyModule()
        : CModule(nullptr, nullptr, nullptr, "legacy", "",
                  CModInfo::NetworkModule) {}

    EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override {
        sTarget = "#legacy";
        sMessage = "CLegacyModule::OnUserCTCPReply";
        return eAction;
    }
    EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override {
        sTarget = "#legacy";
        sMessage = "CLegacyModule::OnUserCTCP";
        return eAction;
    }
    EModRet OnUserAction(CString& sTarget, CString& sMessage) override {
        sTarget = "#legacy";
        sMessage = "CLegacyModule::OnUserAction";
        return eAction;
    }
    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        sTarget = "#legacy";
        sMessage = "CLegacyModule::OnUserMsg";
        return eAction;
    }
    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        sTarget = "#legacy";
        sMessage = "CLegacyModule::OnUserNotice";
        return eAction;
    }
    EModRet OnUserJoin(CString& sChannel, CString& sKey) override {
        sChannel = "#legacy";
        sKey = "CLegacyModule::OnUserJoin";
        return eAction;
    }
    EModRet OnUserPart(CString& sChannel, CString& sMessage) override {
        sChannel = "#legacy";
        sMessage = "CLegacyModule::OnUserPart";
        return eAction;
    }
    EModRet OnUserTopic(CString& sChannel, CString& sTopic) override {
        sChannel = "#legacy";
        sTopic = "CLegacyModule::OnUserTopic";
        return eAction;
    }
    EModRet OnUserQuit(CString& sMessage) override {
        sMessage = "CLegacyModule::OnUserQuit";
        return eAction;
    }

    EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnCTCPReply";
        return eAction;
    }
    EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnPrivCTCP";
        return eAction;
    }
    EModRet OnChanCTCP(CNick& Nick, CChan& Channel,
                       CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnChanCTCP";
        return eAction;
    }
    EModRet OnPrivAction(CNick& Nick, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnPrivAction";
        return eAction;
    }
    EModRet OnChanAction(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnChanAction";
        return eAction;
    }
    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnPrivMsg";
        return eAction;
    }
    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnChanMsg";
        return eAction;
    }
    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnPrivNotice";
        return eAction;
    }
    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        Nick.Parse("legacy!znc@znc.in");
        sMessage = "CLegacyModule::OnChanNotice";
        return eAction;
    }
    EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override {
        Nick.Parse("legacy!znc@znc.in");
        sTopic = "CLegacyModule::OnTopic";
        return eAction;
    }

    EModRet eAction = CONTINUE;
};

class CMessageModule : public CModule {
  public:
    CMessageModule()
        : CModule(nullptr, nullptr, nullptr, "CMessage", "",
                  CModInfo::NetworkModule) {}

    EModRet OnUserCTCPReplyMessage(CCTCPMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetText("CMessageModule::OnUserCTCPReplyMessage");
        return eAction;
    }
    EModRet OnUserCTCPMessage(CCTCPMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetText("CMessageModule::OnUserCTCPMessage");
        return eAction;
    }
    EModRet OnUserActionMessage(CActionMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetText("CMessageModule::OnUserActionMessage");
        return eAction;
    }
    EModRet OnUserTextMessage(CTextMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetText("CMessageModule::OnUserTextMessage");
        return eAction;
    }
    EModRet OnUserNoticeMessage(CNoticeMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetText("CMessageModule::OnUserNoticeMessage");
        return eAction;
    }
    EModRet OnUserJoinMessage(CJoinMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetKey("CMessageModule::OnUserJoinMessage");
        return eAction;
    }
    EModRet OnUserPartMessage(CPartMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetReason("CMessageModule::OnUserPartMessage");
        return eAction;
    }
    EModRet OnUserTopicMessage(CTopicMessage& Message) override {
        Message.SetTarget("#target");
        Message.SetTopic("CMessageModule::OnUserTopicMessage");
        return eAction;
    }
    EModRet OnUserQuitMessage(CQuitMessage& Message) override {
        Message.SetReason("CMessageModule::OnUserQuitMessage");
        return eAction;
    }

    EModRet OnCTCPReplyMessage(CCTCPMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnCTCPReplyMessage");
        return eAction;
    }
    EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnPrivCTCPMessage");
        return eAction;
    }
    EModRet OnChanCTCPMessage(CCTCPMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnChanCTCPMessage");
        return eAction;
    }
    EModRet OnPrivActionMessage(CActionMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnPrivActionMessage");
        return eAction;
    }
    EModRet OnChanActionMessage(CActionMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnChanActionMessage");
        return eAction;
    }
    EModRet OnPrivTextMessage(CTextMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnPrivTextMessage");
        return eAction;
    }
    EModRet OnChanTextMessage(CTextMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnChanTextMessage");
        return eAction;
    }
    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnPrivNoticeMessage");
        return eAction;
    }
    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetText("CMessageModule::OnChanNoticeMessage");
        return eAction;
    }
    EModRet OnTopicMessage(CTopicMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetTopic("CMessageModule::OnTopicMessage");
        return eAction;
    }
    EModRet OnNumericMessage(CNumericMessage& Message) override {
        Message.GetNick().SetNick("nick");
        Message.SetCommand("123");
        return eAction;
    }

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
    EXPECT_EQ(UserCTCPReply.GetTarget(), "#legacy");
    EXPECT_EQ(UserCTCPReply.GetText(), "CLegacyModule::OnUserCTCPReply");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserCTCPReplyMessage(UserCTCPReply);
    EXPECT_EQ(UserCTCPReply.GetTarget(), "#target");
    EXPECT_EQ(UserCTCPReply.GetText(),
              "CMessageModule::OnUserCTCPReplyMessage");

    CCTCPMessage UserCTCPMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserCTCPMessage(UserCTCPMsg);
    EXPECT_EQ(UserCTCPMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserCTCPMsg.GetText(), "CLegacyModule::OnUserCTCP");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserCTCPMessage(UserCTCPMsg);
    EXPECT_EQ(UserCTCPMsg.GetTarget(), "#target");
    EXPECT_EQ(UserCTCPMsg.GetText(), "CMessageModule::OnUserCTCPMessage");

    CActionMessage UserActionMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserActionMessage(UserActionMsg);
    EXPECT_EQ(UserActionMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserActionMsg.GetText(), "CLegacyModule::OnUserAction");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserActionMessage(UserActionMsg);
    EXPECT_EQ(UserActionMsg.GetTarget(), "#target");
    EXPECT_EQ(UserActionMsg.GetText(), "CMessageModule::OnUserActionMessage");

    CTextMessage UserTextMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserTextMessage(UserTextMsg);
    EXPECT_EQ(UserTextMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserTextMsg.GetText(), "CLegacyModule::OnUserMsg");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserTextMessage(UserTextMsg);
    EXPECT_EQ(UserTextMsg.GetTarget(), "#target");
    EXPECT_EQ(UserTextMsg.GetText(), "CMessageModule::OnUserTextMessage");

    CNoticeMessage UserNoticeMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserNoticeMessage(UserNoticeMsg);
    EXPECT_EQ(UserNoticeMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserNoticeMsg.GetText(), "CLegacyModule::OnUserNotice");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserNoticeMessage(UserNoticeMsg);
    EXPECT_EQ(UserNoticeMsg.GetTarget(), "#target");
    EXPECT_EQ(UserNoticeMsg.GetText(), "CMessageModule::OnUserNoticeMessage");

    CJoinMessage UserJoinMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserJoinMessage(UserJoinMsg);
    EXPECT_EQ(UserJoinMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserJoinMsg.GetKey(), "CLegacyModule::OnUserJoin");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserJoinMessage(UserJoinMsg);
    EXPECT_EQ(UserJoinMsg.GetTarget(), "#target");
    EXPECT_EQ(UserJoinMsg.GetKey(), "CMessageModule::OnUserJoinMessage");

    CPartMessage UserPartMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserPartMessage(UserPartMsg);
    EXPECT_EQ(UserPartMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserPartMsg.GetReason(), "CLegacyModule::OnUserPart");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserPartMessage(UserPartMsg);
    EXPECT_EQ(UserPartMsg.GetTarget(), "#target");
    EXPECT_EQ(UserPartMsg.GetReason(), "CMessageModule::OnUserPartMessage");

    CTopicMessage UserTopicMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserTopicMessage(UserTopicMsg);
    EXPECT_EQ(UserTopicMsg.GetTarget(), "#legacy");
    EXPECT_EQ(UserTopicMsg.GetTopic(), "CLegacyModule::OnUserTopic");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserTopicMessage(UserTopicMsg);
    EXPECT_EQ(UserTopicMsg.GetTarget(), "#target");
    EXPECT_EQ(UserTopicMsg.GetTopic(), "CMessageModule::OnUserTopicMessage");

    CQuitMessage UserQuitMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnUserQuitMessage(UserQuitMsg);
    EXPECT_EQ(UserQuitMsg.GetReason(), "CLegacyModule::OnUserQuit");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnUserQuitMessage(UserQuitMsg);
    EXPECT_EQ(UserQuitMsg.GetReason(), "CMessageModule::OnUserQuitMessage");

    CCTCPMessage CTCPReply;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnCTCPReplyMessage(CTCPReply);
    EXPECT_EQ(CTCPReply.GetNick().GetNick(), "legacy");
    EXPECT_EQ(CTCPReply.GetText(), "CLegacyModule::OnCTCPReply");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnCTCPReplyMessage(CTCPReply);
    EXPECT_EQ(CTCPReply.GetNick().GetNick(), "nick");
    EXPECT_EQ(CTCPReply.GetText(), "CMessageModule::OnCTCPReplyMessage");

    CCTCPMessage PrivCTCP;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnPrivCTCPMessage(PrivCTCP);
    EXPECT_EQ(PrivCTCP.GetNick().GetNick(), "legacy");
    EXPECT_EQ(PrivCTCP.GetText(), "CLegacyModule::OnPrivCTCP");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnPrivCTCPMessage(PrivCTCP);
    EXPECT_EQ(PrivCTCP.GetNick().GetNick(), "nick");
    EXPECT_EQ(PrivCTCP.GetText(), "CMessageModule::OnPrivCTCPMessage");

    CCTCPMessage ChanCTCP;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnChanCTCPMessage(ChanCTCP);
    EXPECT_EQ(ChanCTCP.GetNick().GetNick(), "legacy");
    EXPECT_EQ(ChanCTCP.GetText(), "CLegacyModule::OnChanCTCP");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnChanCTCPMessage(ChanCTCP);
    EXPECT_EQ(ChanCTCP.GetNick().GetNick(), "nick");
    EXPECT_EQ(ChanCTCP.GetText(), "CMessageModule::OnChanCTCPMessage");

    CActionMessage PrivAction;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnPrivActionMessage(PrivAction);
    EXPECT_EQ(PrivAction.GetNick().GetNick(), "legacy");
    EXPECT_EQ(PrivAction.GetText(), "CLegacyModule::OnPrivAction");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnPrivActionMessage(PrivAction);
    EXPECT_EQ(PrivAction.GetNick().GetNick(), "nick");
    EXPECT_EQ(PrivAction.GetText(), "CMessageModule::OnPrivActionMessage");

    CActionMessage ChanAction;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnChanActionMessage(ChanAction);
    EXPECT_EQ(ChanAction.GetNick().GetNick(), "legacy");
    EXPECT_EQ(ChanAction.GetText(), "CLegacyModule::OnChanAction");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnChanActionMessage(ChanAction);
    EXPECT_EQ(ChanAction.GetNick().GetNick(), "nick");
    EXPECT_EQ(ChanAction.GetText(), "CMessageModule::OnChanActionMessage");

    CTextMessage PrivMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnPrivTextMessage(PrivMsg);
    EXPECT_EQ(PrivMsg.GetNick().GetNick(), "legacy");
    EXPECT_EQ(PrivMsg.GetText(), "CLegacyModule::OnPrivMsg");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnPrivTextMessage(PrivMsg);
    EXPECT_EQ(PrivMsg.GetNick().GetNick(), "nick");
    EXPECT_EQ(PrivMsg.GetText(), "CMessageModule::OnPrivTextMessage");

    CTextMessage ChanMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnChanTextMessage(ChanMsg);
    EXPECT_EQ(ChanMsg.GetNick().GetNick(), "legacy");
    EXPECT_EQ(ChanMsg.GetText(), "CLegacyModule::OnChanMsg");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnChanTextMessage(ChanMsg);
    EXPECT_EQ(ChanMsg.GetNick().GetNick(), "nick");
    EXPECT_EQ(ChanMsg.GetText(), "CMessageModule::OnChanTextMessage");

    CNoticeMessage PrivNotice;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnPrivNoticeMessage(PrivNotice);
    EXPECT_EQ(PrivNotice.GetNick().GetNick(), "legacy");
    EXPECT_EQ(PrivNotice.GetText(), "CLegacyModule::OnPrivNotice");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnPrivNoticeMessage(PrivNotice);
    EXPECT_EQ(PrivNotice.GetNick().GetNick(), "nick");
    EXPECT_EQ(PrivNotice.GetText(), "CMessageModule::OnPrivNoticeMessage");

    CNoticeMessage ChanNotice;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnChanNoticeMessage(ChanNotice);
    EXPECT_EQ(ChanNotice.GetNick().GetNick(), "legacy");
    EXPECT_EQ(ChanNotice.GetText(), "CLegacyModule::OnChanNotice");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnChanNoticeMessage(ChanNotice);
    EXPECT_EQ(ChanNotice.GetNick().GetNick(), "nick");
    EXPECT_EQ(ChanNotice.GetText(), "CMessageModule::OnChanNoticeMessage");

    CTopicMessage TopicMsg;
    LegacyMod.eAction = CModule::HALT;
    Modules.OnTopicMessage(TopicMsg);
    EXPECT_EQ(TopicMsg.GetNick().GetNick(), "legacy");
    EXPECT_EQ(TopicMsg.GetTopic(), "CLegacyModule::OnTopic");
    LegacyMod.eAction = CModule::CONTINUE;
    Modules.OnTopicMessage(TopicMsg);
    EXPECT_EQ(TopicMsg.GetNick().GetNick(), "nick");
    EXPECT_EQ(TopicMsg.GetTopic(), "CMessageModule::OnTopicMessage");

    CNumericMessage NumericMsg;
    Modules.OnNumericMessage(NumericMsg);
    EXPECT_EQ(TopicMsg.GetNick().GetNick(), "nick");
    EXPECT_EQ(NumericMsg.GetCode(), 123u);

    Modules.clear();
}
