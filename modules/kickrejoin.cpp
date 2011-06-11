/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This was originally written by cycomate.
 *
 * Autorejoin module
 * rejoin channel (after a delay) when kicked
 * Usage: LoadModule = rejoin [delay]
 *
 */

#include "Chan.h"
#include "User.h"

class CRejoinJob: public CTimer {
public:
	CRejoinJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
	: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {
	}

	virtual ~CRejoinJob() {}

protected:
	virtual void RunJob() {
		CUser* user = m_pModule->GetUser();
		CChan* pChan = user->FindChan(GetName().Token(1, true));

		if (pChan) {
			pChan->Enable();
			GetModule()->PutIRC("JOIN " + pChan->GetName() + " " + pChan->GetKey());
		}
	}
};

class CRejoinMod : public CModule {
private:
	unsigned int delay;

public:
	MODCONSTRUCTOR(CRejoinMod) {}
	virtual ~CRejoinMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
		if (sArgs.empty()) {
			CString sDelay = GetNV("delay");

			if (sDelay.empty())
				delay = 10;
			else
				delay = sDelay.ToUInt();
		} else {
			int i = sArgs.ToInt();
			if ((i == 0 && sArgs == "0") || i > 0)
				delay = i;
			else {
				sErrorMsg = "Illegal argument, "
					"must be a positive number or 0";
				return false;
			}
		}

		return true;
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0).AsLower();

		if (sCmdName == "setdelay") {
			int i;
			i = sCommand.Token(1).ToInt();

			if (i < 0) {
				PutModule("Negative delays don't make any sense!");
				return;
			}

			delay = i;
			SetNV("delay", CString(delay));

			if (delay)
				PutModule("Rejoin delay set to " + CString(delay) + " seconds");
			else
				PutModule("Rejoin delay disabled");
		} else if (sCmdName == "showdelay") {
			if (delay)
				PutModule("Rejoin delay enabled, " + CString(delay) + " seconds");
			else
				PutModule("Rejoin delay disabled");
		} else {
			PutModule("Commands: setdelay <secs>, showdelay");
		}
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& pChan, const CString& sMessage) {
		if (m_pUser->GetCurNick().Equals(sKickedNick)) {
			if (!delay) {
				PutIRC("JOIN " + pChan.GetName() + " " + pChan.GetKey());
				pChan.Enable();
				return;
			}
			AddTimer(new CRejoinJob(this, delay, 1, "Rejoin " + pChan.GetName(),
						"Rejoin channel after a delay"));
		}
	}
};

template<> void TModInfo<CRejoinMod>(CModInfo& Info) {
	Info.SetWikiPage("kickrejoin");
}

MODULEDEFS(CRejoinMod, "Autorejoin on kick")
