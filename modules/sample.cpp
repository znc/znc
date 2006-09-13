//! @author prozac@rottenboy.com

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CSampleTimer : public CTimer {
public:

	CSampleTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
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

	virtual bool OnLoad(const CString& sArgs) {
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

	virtual void OnIRCConnected() {
		PutModule("You got connected BoyOh.");
	}

	virtual void OnIRCDisconnected() {
		PutModule("You got disconnected BoyOh.");
	}

	virtual EModRet OnBroadcast(CString& sMessage) {
		PutModule("------ [" + sMessage + "]");
		sMessage = "======== [" + sMessage + "] ========";
		return CONTINUE;
	}

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] set mode [" + Channel.GetName() + ((bAdded) ? "] +" : "] -") + CString(uMode) + " " + Nick.GetNick());
	}

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] opped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] deopped [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] voiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		PutModule(((bNoChange) ? "[0] [" : "[1] [") + OpNick.GetNick() + "] devoiced [" + Nick.GetNick() + "] on [" + Channel.GetName() + "]");
	}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
		PutModule("* " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs + " (" + Channel.GetName() + ")");
	}

	virtual EModRet OnRaw(CString& sLine) {
	//	PutModule("OnRaw() [" + sLine + "]");
		return CONTINUE;
	}

	virtual EModRet OnUserRaw(CString& sLine) {
	//	PutModule("UserRaw() [" + sLine + "]");
		return CONTINUE;
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		PutModule("[" + OpNick.GetNick() + "] kicked [" + sKickedNick + "] from [" + Channel.GetName() + "] with the msg [" + sMessage + "]");
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		PutModule("* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ") (" + sMessage + ")");
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		PutModule("* Joins: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel) {
		PutModule("* Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "!" + Nick.GetHost() + ")");
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		PutModule("* " + OldNick.GetNick() + " is now known as " + sNewNick);
	}

	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) {
		PutModule("[" + sTarget + "] userctcpreply [" + sMessage + "]");
		sMessage = "\037" + sMessage + "\037";

		return CONTINUE;
	}

	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] ctcpreply [" + sMessage + "]");

		return CONTINUE;
	}

	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage) {
		PutModule("[" + sTarget + "] userctcp [" + sMessage + "]");

		return CONTINUE;
	}

	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privctcp [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return CONTINUE;
	}

	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] chanctcp [" + sMessage + "] to [" + Channel.GetName() + "]");
		sMessage = "\00311,5 " + sMessage + " \003";

		return CONTINUE;
	}

	virtual EModRet OnUserNotice(const CString& sTarget, CString& sMessage) {
		PutModule("[" + sTarget + "] usernotice [" + sMessage + "]");
		sMessage = "\037" + sMessage + "\037";

		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privnotice [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return CONTINUE;
	}

	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] channotice [" + sMessage + "] to [" + Channel.GetName() + "]");
		sMessage = "\00311,5 " + sMessage + " \003";

		return CONTINUE;
	}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		PutModule("[" + sTarget + "] usermsg [" + sMessage + "]");
		sMessage = "\0034" + sMessage + "\003";

		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		PutModule("[" + Nick.GetNick() + "] privmsg [" + sMessage + "]");
		sMessage = "\002" + sMessage + "\002";

		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		if (sMessage == "!ping") {
			PutIRC("PRIVMSG " + Channel.GetName() + " :PONG?");
		}

		sMessage = "x " + sMessage + " x";

		PutModule(sMessage);

		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		if (strcasecmp(sCommand.c_str(), "TIMERS") == 0) {
			ListTimers();
		}
	}

	virtual EModRet OnStatusCommand(const CString& sCommand) {
		if (strcasecmp(sCommand.c_str(), "SAMPLE") == 0) {
			PutModule("Hi, I'm your friendly sample module.");
			return HALT;
		}

		return CONTINUE;
	}
};

MODULEDEFS(CSampleMod, "To be used as a sample for writing modules")

