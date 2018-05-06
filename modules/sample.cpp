/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

/*
 * NOTE: This module's primary purpose is to provide a sample of the various
 * things that modules can do. It is not written to be an actual module that
 * you would enable on a sample installation, and serves more as a developer 
 * "example" rather than an actual module that would be used.
 */

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;

#ifdef HAVE_PTHREAD
class CSampleJob : public CModuleJob {
  public:
    CSampleJob(CModule* pModule)
        : CModuleJob(pModule, "sample", "An example module that messages the user after a delay.") {}

    ~CSampleJob() override {
        if (wasCancelled()) {
            GetModule()->PutModule(GetModule()->t_s("Sample job cancelled"));
        } else {
            GetModule()->PutModule(GetModule()->t_s("Sample job destroyed"));
        }
    }

    void runThread() override {
        // Cannot safely use GetModule() in here, because this runs in its
        // own thread and such an access would require synchronisation
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
        PutModule(t_s("You have been connected."));
    }

    void OnIRCDisconnected() override {
        PutModule(t_s("You have been disconnected."));
    }

    EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent,
                              CString& sRealName) override {
        sRealName += " - Running on ZNC";
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

    void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel,
                const CString& sMessage) override {
        PutModule(t_f("{1} kicked {2} from {3} with the msg {4}")(
            OpNick.GetNick(), sKickedNick, Channel.GetName(), sMessage));
    }

    void OnQuit(const CNick& Nick, const CString& sMessage,
                const vector<CChan*>& vChans) override {
        PutModule(t_p("* {1} ({2}@{3}) quits ({4}) from channel: {6}",
                      "* {1} ({2}@{3}) quits ({4}) from {5} channels: {6}",
                      vChans.size())(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(), sMessage,
            vChans.size(), CString(", ").Join(vChans.begin(), vChans.end())));
    }

    EModRet OnTimerAutoJoin(CChan& Channel) override {
        PutModule(t_f("Attempting to join {1}")(Channel.GetName()));
        return CONTINUE;
    }

    void OnJoin(const CNick& Nick, CChan& Channel) override {
        PutModule(t_f("* {1} ({2}@{3}) joins {4}")(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(),
            Channel.GetName()));
    }

    void OnPart(const CNick& Nick, CChan& Channel,
                const CString& sMessage) override {
        PutModule(t_f("* {1} ({2}@{3}) parts {4}")(
            Nick.GetNick(), Nick.GetIdent(), Nick.GetHost(),
            Channel.GetName()));
    }

    EModRet OnInvite(const CNick& Nick, const CString& sChan) override {
        if (sChan.Equals("#test")) {
            PutModule(t_f("{1} invited us to {2}, ignoring invites to {2}")(
                Nick.GetNick(), sChan));
            return HALT;
        }

        PutModule(t_f("{1} invited us to {2}")(Nick.GetNick(), sChan));
        return CONTINUE;
    }

    void OnNick(const CNick& OldNick, const CString& sNewNick,
                const vector<CChan*>& vChans) override {
        PutModule(t_f("{1} is now known as {2}")(OldNick.GetNick(), sNewNick));
    }

    EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override {
        PutModule("[" + sTarget + "] userctcpreply [" + sMessage + "]");
        sMessage = "\037" + sMessage + "\037";

        return CONTINUE;
    }

    EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] ctcpreply [" + sMessage + "]");

        return CONTINUE;
    }

    EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override {
        PutModule("[" + sTarget + "] userctcp [" + sMessage + "]");

        return CONTINUE;
    }

    EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] privctcp [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanCTCP(CNick& Nick, CChan& Channel,
                       CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] chanctcp [" + sMessage + "] to [" +
                  Channel.GetName() + "]");
        sMessage = "\00311,5 " + sMessage + " \003";

        return CONTINUE;
    }

    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        PutModule("[" + sTarget + "] usernotice [" + sMessage + "]");
        sMessage = "\037" + sMessage + "\037";

        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] privnotice [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] channotice [" + sMessage +
                  "] to [" + Channel.GetName() + "]");
        sMessage = "\00311,5 " + sMessage + " \003";

        return CONTINUE;
    }

    EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override {
        PutModule(t_f("{1} changes topic on {2} to {3}")(
            Nick.GetNick(), Channel.GetName(), sTopic));

        return CONTINUE;
    }

    EModRet OnUserTopic(CString& sTarget, CString& sTopic) override {
        PutModule(t_f("{1} changes topic on {2} to {3}")(GetClient()->GetNick(),
                                                         sTarget, sTopic));

        return CONTINUE;
    }
    // Appends "Sample:" to an outgoing message and colors it red.
    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        PutModule("[" + sTarget + "] usermsg [" + sMessage + "]");
        sMessage = "Sample: \0034" + sMessage + "\003";

        return CONTINUE;
    }
    
    // Bolds an incoming message.
    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        PutModule("[" + Nick.GetNick() + "] privmsg [" + sMessage + "]");
        sMessage = "\002" + sMessage + "\002";

        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
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
