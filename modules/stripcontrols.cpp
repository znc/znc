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

#include <znc/Modules.h>

class CStripControlsMod : public CModule {
  public:
    MODCONSTRUCTOR(CStripControlsMod) {}

    EModRet OnChanCTCPMessage(CCTCPMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnChanTextMessage(CTextMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnPrivTextMessage(CTextMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnTopicMessage(CTopicMessage& Message) override {
        Message.SetText(Message.GetText().StripControls_n());
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& Message) override {
        // Strip topic from /list
        if (Message.GetCode() == 322) {  // RPL_LIST
            Message.SetParam(3, Message.GetParam(3).StripControls_n());
        }
        // Strip topic when joining channel
        else if (Message.GetCode() == 332) {  // RPL_TOPIC
            Message.SetParam(2, Message.GetParam(2).StripControls_n());
        }
        return CONTINUE;
    }
};

template <>
void TModInfo<CStripControlsMod>(CModInfo& Info) {
    Info.SetWikiPage("stripcontrols");
    Info.AddType(CModInfo::UserModule);
}

NETWORKMODULEDEFS(
    CStripControlsMod,
    t_s("Strips control codes (Colors, Bold, ..) from channel "
        "and private messages."))
