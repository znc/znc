/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 * Copyright (C) 2008 Michael "Svedrin" Ziegler diese-addy@funzt-halt.net
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

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

class CAutoReplyMod : public CModule {
  public:
    MODCONSTRUCTOR(CAutoReplyMod) {
        AddHelpCommand();
        AddCommand("Set", t_d("<reply>"), t_d("Sets a new reply"),
                   [=](const CString& sLine) { OnSetCommand(sLine); });
        AddCommand("Show", "", t_d("Displays the current query reply"),
                   [=](const CString& sLine) { OnShowCommand(sLine); });
        m_Messaged.SetTTL(1000 * 120);
    }

    ~CAutoReplyMod() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        if (!sArgs.empty()) {
            SetReply(sArgs);
        }

        return true;
    }

    void SetReply(const CString& sReply) { SetNV("Reply", sReply); }

    CString GetReply() {
        CString sReply = GetNV("Reply");
        if (sReply.empty()) {
            sReply = "%nick% is currently away, try again later";
            SetReply(sReply);
        }

        return ExpandString(sReply);
    }

    void Handle(const CString& sNick) {
        CIRCSock* pIRCSock = GetNetwork()->GetIRCSock();
        if (!pIRCSock)
            // WTF?
            return;
        if (sNick == pIRCSock->GetNick()) return;
        if (m_Messaged.HasItem(sNick)) return;

        if (GetNetwork()->IsUserAttached()) return;

        m_Messaged.AddItem(sNick);
        PutIRC("NOTICE " + sNick + " :" + GetReply());
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        Handle(Nick.GetNick());
        return CONTINUE;
    }

    void OnShowCommand(const CString& sCommand) {
        CString sReply = GetReply();
        PutModule(t_f("Current reply is: {1} ({2})")(GetNV("Reply"), sReply));
    }

    void OnSetCommand(const CString& sCommand) {
        SetReply(sCommand.Token(1, true));
        PutModule(
            t_f("New reply set to: {1} ({2})")(GetNV("Reply"), GetReply()));
    }

  private:
    TCacheMap<CString> m_Messaged;
};

template <>
void TModInfo<CAutoReplyMod>(CModInfo& Info) {
    Info.SetWikiPage("autoreply");
    Info.AddType(CModInfo::NetworkModule);
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s(
        "You might specify a reply text. It is used when automatically "
        "answering queries, if you are not connected to ZNC."));
}

USERMODULEDEFS(CAutoReplyMod, t_s("Reply to queries when you are away"))
