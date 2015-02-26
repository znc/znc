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

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;

#ifdef HAVE_PTHREAD
class CSampleJob : public CModuleJob {
public:
	CSampleJob(CModule *pModule) : CModuleJob(pModule, "sample", "Message the user after a delay") {}

	~CSampleJob() {
		if (wasCancelled()) {
			GetModule()->PutModule("Sample job cancelled");
		} else {
			GetModule()->PutModule("Sample job destroyed");
		}
	}

	void runThread() override {
		// Cannot safely use GetModule() in here, because this runs in its
		// own thread and such an access would require synchronisation
		// between this thread and the main thread!

		for (int i = 0; i < 10; i++) {
			// Regularly check if we were cancelled
			if (wasCancelled())
				return;
			sleep(1);
		}
	}

	void runMain() override {
		GetModule()->PutModule("Sample job done");
	}
};
#endif

class CSampleTimer : public CTimer {
public:

	CSampleTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CSampleTimer() {}

private:
protected:
	void RunJob() override {
		GetModule()->PutModule("TEST!!!!");
	}
};

class CSampleMod : public CModule {
public:
	MODCONSTRUCTOR(CSampleMod) {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		PutModule("I'm being loaded with the arguments: [" + sArgs + "]");
		//AddTimer(new CSampleTimer(this, 300, 0, "Sample", "Sample timer for sample things."));
		//AddTimer(new CSampleTimer(this, 5, 20, "Another", "Another sample timer."));
		//AddTimer(new CSampleTimer(this, 25000, 5, "Third", "A third sample timer."));
#ifdef HAVE_PTHREAD
		AddJob(new CSampleJob(this));
#endif
		return true;
	}

	virtual ~CSampleMod() {
		PutModule("I'm being unloaded!");
	}

	bool OnBoot() override {
		// This is called when the app starts up (only modules that are loaded in the config will get this event)
		return true;
	}

	void OnIRCConnected() override {
		PutModule("You got connected BoyOh.");
	}

	void OnIRCDisconnected() override {
		PutModule("You got disconnected BoyOh.");
	}

	EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName) override {
		sRealName += " - ZNC";
		return CONTINUE;
	}

	EModRet OnBroadcast(CString& sMessage) override {
		PutModule("------ [" + sMessage + "]");
		sMessage = "======== [" + sMessage + "] ========";
		return CONTINUE;
	}

	void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) override {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] set mode [" + Channel.GetName() + ((bAdded) ? "] +" : "] -") + CString(uMode) + " " + Nick.GetNick());
	}

	void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] opped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] deopped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] voiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) override {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] devoiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override {
		PutModule("* " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs + " (" + Channel.GetName() + ")");
	}

	EModRet OnRaw(CString& sLine) override {
		// PutModule("OnRaw() [" + sLine + "]");
		return CONTINUE;
	}

	EModRet OnUserRaw(CString& sLine) override {
		// PutModule("UserRaw() [" + sLine + "]");
		return CONTINUE;
	}

	void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) override {
		PutModule("[" + OpNick.GetNick() + "] kicked [" + sKickedNick + "] from [" + Channel.GetName() + "] with the msg [" + sMessage + "]");
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		PutModule("* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ") (" + sMessage + ")");
	}

	EModRet OnTimerAutoJoin(CChan& Channel) override {
		PutModule("Attempting to join " + Channel.GetName());
		return CONTINUE;
	}

	void OnJoin(const CNick& Nick, CChan& Channel) override {
		PutModule("* Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override {
		PutModule("* Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	EModRet OnInvite(const CNick& Nick, const CString& sChan) override {
		if (sChan.Equals("#test")) {
			PutModule(Nick.GetNick() + " invited us to " + sChan + ", ignoring invites to " + sChan);
			return HALT;
		}

		PutModule(Nick.GetNick() + " invited us to " + sChan);
		return CONTINUE;
	}

	void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) override {
		PutModule("* " + OldNick.GetNick() + " is now known as " + sNewNick);
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

	EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override {
		PutModule("[" + Nick.GetNick() + "] chanctcp [" + sMessage + "] to [" + Channel.GetName() + "]");
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

	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override {
		PutModule("[" + Nick.GetNick() + "] channotice [" + sMessage + "] to [" + Channel.GetName() + "]");
		sMessage = "\00311,5 " + sMessage + " \003";

		return CONTINUE;
	}

	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override {
		PutModule("* " + Nick.GetNick() + " changes topic on " + Channel.GetName() + " to '" + sTopic + "'");

		return CONTINUE;
	}

	EModRet OnUserTopic(CString& sTarget, CString& sTopic) override {
		PutModule("* " + GetClient()->GetNick() + " changed topic on " + sTarget + " to '" + sTopic + "'");

		return CONTINUE;
	}

	EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
		PutModule("[" + sTarget + "] usermsg [" + sMessage + "]");
		sMessage = "Sample: \0034" + sMessage + "\003";

		return CONTINUE;
	}

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
			PutModule("Hi, I'm your friendly sample module.");
			return HALT;
		}

		return CONTINUE;
	}
};

template<> void TModInfo<CSampleMod>(CModInfo& Info) {
	Info.SetWikiPage("sample");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Description of module arguments goes here.");
}

MODULEDEFS(CSampleMod, "To be used as a sample for writing modules")

