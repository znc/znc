#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CSampleTimer : public CTimer {
public:

	CSampleTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const string& sLabel, const string& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CSampleTimer() {}

private:
protected:
	virtual void RunJob() {
		m_pModule->PutModule("TEST!!!!");
	}
};

class CSampleMod : public CModule {
public:
	MODCONSTRUCTOR(CSampleMod) {}

	virtual bool OnLoad(const string& sArgs) {
		PutModule("I'm being loaded with the arguments: [" + sArgs + "]");
		//AddTimer(new CSampleTimer(this, 300, 0, "Sample", "Sample timer for sample things."));
		//AddTimer(new CSampleTimer(this, 5, 20, "Another", "Another sample timer."));
		//AddTimer(new CSampleTimer(this, 25000, 5, "Third", "A third sample timer."));
		return true;
	}

	virtual ~CSampleMod() {
		PutModule("I'm being unloaded!");
	}

	virtual bool OnBoot() {
		// This is called when the app starts up (only modules that are loaded in the config will get this event)
		return true;
	}

	virtual string GetDescription() {
		return "To be used as a sample for writing modules.";
	}

	virtual void OnIRCConnected() {
		PutModule("You got connected BoyOh.");
	}

	virtual void OnIRCDisconnected() {
		PutModule("You got disconnected BoyOh.");
	}

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] " : "[1] [") + OpNick.GetNick() + "] opped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] " : "[1] [") + OpNick.GetNick() + "] deopped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] " : "[1] [") + OpNick.GetNick() + "] voiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] " : "[1] [") + OpNick.GetNick() + "] devoiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnRawMode(const CNick& OpNick, const CChan& Channel, const string& sModes, const string& sArgs) {
		PutModule("* " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs + " (" + Channel.GetName() + ")");
	}

	virtual bool OnRaw(string& sLine) {
	//	PutModule("OnRaw() [" + sLine + "]");
		return false;
	}

	virtual bool OnUserRaw(string& sLine) {
	//	PutModule("UserRaw() [" + sLine + "]");
		return false;
	}

	virtual void OnKick(const CNick& OpNick, const string& sKickedNick, const CChan& Channel, const string& sMessage) {
		PutModule("[" + OpNick.GetNick() + "] kicked [" + sKickedNick + "] from [" + Channel.GetName() + "] with the msg [" + sMessage + "]");
	}

	virtual void OnQuit(const CNick& Nick, const string& sMessage, const vector<CChan*> vChans) {
		PutModule("* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ") (" + sMessage + ")");
	}

	virtual void OnJoin(const CNick& Nick, const CChan& Channel) {
		PutModule("* Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	virtual void OnPart(const CNick& Nick, const CChan& Channel) {
		PutModule("* Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	virtual void OnNick(const CNick& OldNick, const string& sNewNick, const vector<CChan*> vChans) {
		PutModule("* " + OldNick.GetNick() + " is now known as " + sNewNick);
	}

	virtual bool OnUserCTCPReply(const CNick& Nick, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] userctcpreply [" + sMessage + "]");
		sMessage = "\037" + sMessage + "\037";

		return false;
	}

	virtual bool OnCTCPReply(const CNick& Nick, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] ctcpreply [" + sMessage + "]");

		return false;
	}

	virtual bool OnUserCTCP(const string& sTarget, string& sMessage) {
		PutModule("[" + sTarget + "] userctcp [" + sMessage + "]");

		return false;
	}

	virtual bool OnPrivCTCP(const CNick& Nick, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privctcp [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return false;
	}

	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] chanctcp [" + sMessage + "] to [" + Channel.GetName() + "]");
		sMessage = "\00311,5 " + sMessage + " \003";

		return false;
	}

	virtual bool OnUserNotice(const string& sTarget, string& sMessage) {
		PutModule("[" + sTarget + "] usernotice [" + sMessage + "]");
		sMessage = "\037" + sMessage + "\037";

		return false;
	}

	virtual bool OnPrivNotice(const CNick& Nick, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privnotice [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return false;
	}

	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] channotice [" + sMessage + "] to [" + Channel.GetName() + "]");
		sMessage = "\00311,5 " + sMessage + " \003";

		return false;
	}

	virtual bool OnUserMsg(const string& sTarget, string& sMessage) {
		PutModule("[" + sTarget + "] usermsg [" + sMessage + "]");
		sMessage = "\0034" + sMessage + "\003";

		return false;
	}

	virtual bool OnPrivMsg(const CNick& Nick, string& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privmsg [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return false;
	}

	virtual bool OnChanMsg(const CNick& Nick, const CChan& Channel, string& sMessage) {
		if (sMessage == "!ping") {
			PutIRC("PRIVMSG " + Channel.GetName() + " :PONG?");
		}

		sMessage = "x " + sMessage + " x";

		return false;
	}

	virtual void OnModCommand(const string& sCommand) {
		if (strcasecmp(sCommand.c_str(), "TIMERS") == 0) {
			ListTimers();
		}
	}

	virtual bool OnStatusCommand(const string& sCommand) {
		if (strcasecmp(sCommand.c_str(), "SAMPLE") == 0) {
			PutModule("Hi, I'm your friendly sample module.");
			return true;
		}

		return false;
	}
};

MODULEDEFS(CSampleMod)

