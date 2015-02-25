/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 * This was originally written by cycomate.
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
 * Autorejoin module
 * rejoin channel (after a delay) when kicked
 * Usage: LoadModule = rejoin [delay]
 *
 */

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

class CRejoinJob: public CTimer {
public:
	CRejoinJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
	: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {
	}

	virtual ~CRejoinJob() {}

protected:
	void RunJob() override {
		CIRCNetwork* pNetwork = GetModule()->GetNetwork();
		CChan* pChan = pNetwork->FindChan(GetName().Token(1, true));

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
	MODCONSTRUCTOR(CRejoinMod) {
		AddHelpCommand();
		AddCommand("SetDelay", static_cast<CModCommand::ModCmdFunc>(&CRejoinMod::OnSetDelayCommand), "<secs>", "Set the rejoin delay");
		AddCommand("ShowDelay", static_cast<CModCommand::ModCmdFunc>(&CRejoinMod::OnShowDelayCommand), "", "Show the rejoin delay");
	}
	virtual ~CRejoinMod() {}

	bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
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

	void OnSetDelayCommand(const CString& sCommand) {
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
	}

	void OnShowDelayCommand(const CString& sCommand) {
		if (delay)
			PutModule("Rejoin delay enabled, " + CString(delay) + " seconds");
		else
			PutModule("Rejoin delay disabled");
	}

	void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& pChan, const CString& sMessage) override {
		if (GetNetwork()->GetCurNick().Equals(sKickedNick)) {
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
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("You might enter the number of seconds to wait before rejoining.");
}

NETWORKMODULEDEFS(CRejoinMod, "Autorejoin on kick")
