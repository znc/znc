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

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;

#ifdef HAVE_PTHREAD
class CSampleJob : public CModuleJob {
  public:
    CSampleJob(CModule* pModule)
        : CModuleJob(pModule, "sample", "Message the user after a delay") {}

    ~CSampleJob() override {
        if (wasCancelled()) {
            GetModule()->PutModule(GetModule()->t_s("Sample job cancelled"));
        } else {
            GetModule()->PutModule(GetModule()->t_s("Sample job destroyed"));
        }
    }

    void runThread() override {
        // Cannot safely use GetModule() in here, because this runs in its
        // own thread and such an access would require synchronization
        // between this thread and the main thread!

        for (int i = 0; i < 10; i++) {
            // Regularly check if we were cancelled
            if (wasCancelled()) return;
            sleep(1);
        }
    }

    void runMain() override {
        GetModule()->PutModule(GetModule()->t_s("Sample job done"));
    }
};
#endif

class CSampleTimer : public CTimer {
  public:
    CSampleTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles,
                 const CString& sLabel, const CString& sDescription)
        : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
    ~CSampleTimer() override {}

  private:
  protected:
    void RunJob() override {
        GetModule()->PutModule(GetModule()->t_s("TEST!!!!"));
    }
};

class CSampleMod : public CModule {
  public:
    MODCONSTRUCTOR(CSampleMod) {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        PutModule(t_f("I'm being loaded with the arguments: {1}")(sArgs));
// AddTimer(new CSampleTimer(this, 300, 0, "Sample", "Sample timer for sample
// things."));
// AddTimer(new CSampleTimer(this, 5, 20, "Another", "Another sample timer."));
// AddTimer(new CSampleTimer(this, 25000, 5, "Third", "A third sample timer."));
#ifdef HAVE_PTHREAD
        AddJob(new CSampleJob(this));
#endif
        return true;
    }

    ~CSampleMod() override { PutModule(t_s("I'm being unloaded!")); }

    bool OnBoot() override {
        // This is called when the app starts up (only modules that are loaded
        // in the config will get this event)
        return true;
    }

    void OnIRCConnected() override {
        PutModule(t_s("You got connected BoyOh."));
    }

    void OnIRCDisconnected() override {
        PutModule(t_s("You got disconnected BoyOh."));
    }

    EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent,
                              CString& sRealName) override {
        sRealName += " - ZNC";
        return CONTINUE;
    }

    EModRet OnBroadcast(CString& sMessage) override {
        PutModule("------ [" + sMessage + "]");
        sMessage = "======== [" + sMessage + "] ========";
        return CONTINUE;
    }

    void OnChanPermission(const CNick& OpNick, const CNick& Nick,
                          CChan& Channel, unsigned char uMode, bool bAdded,
                          bool bNoChange) override {
        PutModule(t_f("{1} {2} set mode on {3} {4}{5} {6}")(
            bNoChange, OpNick.GetNick(), Channel.GetName(), bAdded ? '+' : '-',
            uMode, Nick.GetNick()));
    }

    void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel,
              bool bNoChange) override {
        PutModule(t_f("{1} {2} opped {3} on {4}")(
            bNoChange, OpNick.GetNick(), Nick.GetNick(), Channel.GetName()));
    }

    void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                bool bNoChange) override {
        PutModule(t_f("{1} {2} deopped {3} on {4}")(
            bNoChange, OpNick.GetNick(), Nick.GetNick(), Channel.GetName()));
    }

    void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                 bool bNoChange) override {
        PutModule(t_f("{1} {2} voiced {3} on {4}")(
            bNoChange, OpNick.GetNick(), Nick.GetNick(), Channel.GetName()));
    }

    void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel,
                   bool bNoChange) override {
        PutModule(t_f("{1} {2} devoiced {3} on {4}")(
            bNoChange, OpNick.GetNick(), Nick.GetNick(), Channel.GetName()));
    }

    void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes,
                   const CString& sArgs) override {
        PutModule(t_f("* {1} sets mode: {2} {3} on {4}")(
            OpNick.GetNick(), sModes, sArgs, Channel.GetName()));
    }

    EModRet OnRaw(CString& sLine) override {
        // PutModule(t_f("OnRaw(): {1}")(sLine));
        return CONTINUE;
    }

    EModRet OnUserRaw(CString& sLine) override {
        // PutModule(t_f("OnUserRaw(): {1}")(sLine));
        return CONTINUE;
    }

    void OnKickMessage(CKickMessage& Message) override {
        const CNick& OpNick = Message.GetNick();
        const CString sKickedNick = Message.GetKickedNick();
        CChan& Channel = *Message.GetChan();
        const CString sMessage = Message.GetReason();
        PutModule(t_f("{1} kicked {2} from {3} with the msg {4}")(
            OpNick.GetNick(), sKickedNick, Channel.GetName(), sMessage));
    }

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        const CString sMessage = Message.GetReason();
        VCString vsChans;
        for (CChan* pChan : vChans) {
            vsChans.push_back(pChan->GetName());
        }

        PutModule(t_p("* {1} ({2}@{3}) quits ({4}) from channel: {6}",
                      "* {1} ({2}@{3}) quits ({4}) from {5} channels: {6}",
                      vChans.size())(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(), sMessage,
            vChans.size(), CString(", ").Join(vsChans.begin(), vsChans.end())));
    }

    EModRet OnTimerAutoJoin(CChan& Channel) override {
        PutModule(t_f("Attempting to join {1}")(Channel.GetName()));
        return CONTINUE;
    }

    void OnJoinMessage(CJoinMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        PutModule(t_f("* {1} ({2}@{3}) joins {4}")(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(),
            Channel.GetName()));
    }

    void OnPartMessage(CPartMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        const CString sMessage = Message.GetReason();
        PutModule(t_f("* {1} ({2}@{3}) parts {4} with message: {5}")(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(), Channel.GetName(),
            sMessage));
    }

    EModRet OnInviteMessage(CInviteMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        const CString Channel = Message.GetParam(1);
        if (Channel.Equals("#test")) {
            PutModule(t_f("{1} invited us to {2}, ignoring invites to {2}")(
                Nick.GetNick(), Channel));
            return HALT;
        }

        PutModule(t_f("{1} invited us to {2}")(Nick.GetNick(), Channel));
        return CONTINUE;
    }

    void OnNickMessage(CNickMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& OldNick = Message.GetNick();
        const CString sNewNick = Message.GetNewNick();
        PutModule(t_f("{1} is now known as {2}")(OldNick.GetNick(), sNewNick));
    }

    EModRet OnUserCTCPReplyMessage(CCTCPMessage& Message) override {
        CString sTarget = Message.GetTarget();
        CString sMessage = Message.GetText();
        PutModule("[" + sTarget + "] userctcpreply [" + sMessage + "]");
        sMessage = "\037" + sMessage + "\037";

        return CONTINUE;
    }

    EModRet OnCTCPReplyMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        PutModule("[" + Nick.GetNick() + "] ctcpreply [" + sMessage + "]");

        return CONTINUE;
    }

    EModRet OnUserCTCPMessage(CCTCPMessage& Message) override {
        CString sTarget = Message.GetTarget();
        CString sMessage = Message.GetText();
        PutModule("[" + sTarget + "] userctcp [" + sMessage + "]");

        return CONTINUE;
    }

    EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        PutModule("[" + Nick.GetNick() + "] privctcp [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanCTCPMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        PutModule("[" + Nick.GetNick() + "] chanctcp [" + sMessage + "] to [" +
                  Channel.GetName() + "]");
        sMessage = "\00311,5 " + sMessage + " \003";

        return CONTINUE;
    }

    EModRet OnUserNoticeMessage(CNoticeMessage& Message) override {
        CIRCNetwork* pNetwork = GetNetwork();
        CString sTarget = Message.GetTarget();
        CString sMessage = Message.GetText();
        PutModule("[" + sTarget + "] usernotice [" + sMessage + "]");
        sMessage = "\037" + sMessage + "\037";

        return CONTINUE;
    }

    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        PutModule("[" + Nick.GetNick() + "] privnotice [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        PutModule("[" + Nick.GetNick() + "] channotice [" + sMessage +
                  "] to [" + Channel.GetName() + "]");
        sMessage = "\00311,5 " + sMessage + " \003";

        return CONTINUE;
    }

    EModRet OnTopicMessage(CTopicMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        CString sTopic = Message.GetTopic();
        PutModule(t_f("{1} changes topic on {2} to {3}")(
            Nick.GetNick(), Channel.GetName(), sTopic));

        return CONTINUE;
    }

    EModRet OnUserTopicMessage(CTopicMessage& Message) override {
        CChan& Channel = *Message.GetChan();
        CString sTopic = Message.GetTopic();
        PutModule(t_f("{1} changes topic on {2} to {3}")(
            GetClient()->GetNick(), Channel.GetName(), sTopic));

        return CONTINUE;
    }

    // Appends "Sample:" to an outgoing message and colors it red.
    EModRet OnUserTextMessage(CTextMessage& Message) override {
        CIRCNetwork* pNetwork = GetNetwork();
        CString sTarget = Message.GetTarget();
        CString sMessage = Message.GetText();
        PutModule("[" + sTarget + "] usermsg [" + sMessage + "]");
        sMessage = "Sample: \0034" + sMessage + "\003";

        return CONTINUE;
    }

    // Bolds an incoming message.
    EModRet OnPrivTextMessage(CTextMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        PutModule("[" + Nick.GetNick() + "] privmsg [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanTextMessage(CTextMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        if (sMessage == "!ping") {
            PutIRC("PRIVMSG " + Channel.GetName() + " :PONG?");
        }

        sMessage = "x " + sMessage + " x";

        PutModule(sMessage);

        return CONTINUE;
    }

    void OnModCommand(const CString& sCommand) override {
        if (sCommand.Equals("TIMERS")) {
            ListTimers();
        }
    }

    EModRet OnStatusCommand(CString& sCommand) override {
        if (sCommand.Equals("SAMPLE")) {
            PutModule(t_s("Hi, I'm your friendly sample module."));
            return HALT;
        }

        return CONTINUE;
    }
};

template <>
void TModInfo<CSampleMod>(CModInfo& Info) {
    Info.SetWikiPage("sample");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("Description of module arguments goes here."));
}

MODULEDEFS(CSampleMod, t_s("To be used as a sample for writing modules"))
