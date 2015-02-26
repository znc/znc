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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

#define SIMPLE_AWAY_DEFAULT_REASON "Auto away at %s"
#define SIMPLE_AWAY_DEFAULT_TIME   60


class CSimpleAway;

class CSimpleAwayJob : public CTimer {
public:
	CSimpleAwayJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CSimpleAwayJob() {}

protected:
	void RunJob() override;
};

class CSimpleAway : public CModule {
private:
	CString      m_sReason;
	unsigned int m_iAwayWait;
	bool         m_bClientSetAway;
	bool         m_bWeSetAway;

public:
	MODCONSTRUCTOR(CSimpleAway) {
		m_sReason        = SIMPLE_AWAY_DEFAULT_REASON;
		m_iAwayWait      = SIMPLE_AWAY_DEFAULT_TIME;
		m_bClientSetAway = false;
		m_bWeSetAway     = false;

		AddHelpCommand();
		AddCommand("Reason", static_cast<CModCommand::ModCmdFunc>(&CSimpleAway::OnReasonCommand), "[<text>]", "Prints or sets the away reason (%s is replaced with the time you were set away)");
		AddCommand("Timer", static_cast<CModCommand::ModCmdFunc>(&CSimpleAway::OnTimerCommand), "", "Prints the current time to wait before setting you away");
		AddCommand("SetTimer", static_cast<CModCommand::ModCmdFunc>(&CSimpleAway::OnSetTimerCommand), "<seconds>", "Sets the time to wait before setting you away");
		AddCommand("DisableTimer", static_cast<CModCommand::ModCmdFunc>(&CSimpleAway::OnDisableTimerCommand), "", "Disables the wait time before setting you away");
	}

	virtual ~CSimpleAway() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		CString sReasonArg;

		// Load AwayWait
		CString sFirstArg = sArgs.Token(0);
		if (sFirstArg.Equals("-notimer")) {
			SetAwayWait(0);
			sReasonArg = sArgs.Token(1, true);
		} else if (sFirstArg.Equals("-timer")) {
			SetAwayWait(sArgs.Token(1).ToUInt());
			sReasonArg = sArgs.Token(2, true);
		} else {
			CString sAwayWait = GetNV("awaywait");
			if (!sAwayWait.empty())
				SetAwayWait(sAwayWait.ToUInt(), false);
			sReasonArg = sArgs;
		}

		// Load Reason
		if (!sReasonArg.empty()) {
			SetReason(sReasonArg);
		} else {
			CString sSavedReason = GetNV("reason");
			if (!sSavedReason.empty())
				SetReason(sSavedReason, false);
		}

		// Set away on load, required if loaded via webadmin
		if (GetNetwork()->IsIRCConnected() && !GetNetwork()->IsUserAttached())
			SetAway(false);

		return true;
	}

	void OnIRCConnected() override {
		if (GetNetwork()->IsUserAttached())
			SetBack();
		else
			SetAway(false);
	}

	void OnClientLogin() override {
		SetBack();
	}

	void OnClientDisconnect() override {
		/* There might still be other clients */
		if (!GetNetwork()->IsUserAttached())
			SetAway();
	}

	void OnReasonCommand(const CString& sLine) {
		CString sReason = sLine.Token(1, true);

		if (!sReason.empty()) {
			SetReason(sReason);
			PutModule("Away reason set");
		} else {
			PutModule("Away reason: " + m_sReason);
			PutModule("Current away reason would be: " + ExpandReason());
		}
	}

	void OnTimerCommand(const CString& sLine) {
		PutModule("Current timer setting: "
				+ CString(m_iAwayWait) + " seconds");
	}

	void OnSetTimerCommand(const CString& sLine) {
		SetAwayWait(sLine.Token(1).ToUInt());

		if (m_iAwayWait == 0)
			PutModule("Timer disabled");
		else
			PutModule("Timer set to "
					+ CString(m_iAwayWait) + " seconds");
	}

	void OnDisableTimerCommand(const CString& sLine) {
		SetAwayWait(0);
		PutModule("Timer disabled");
	}

	EModRet OnUserRaw(CString &sLine) override {
		if (!sLine.Token(0).Equals("AWAY"))
			return CONTINUE;

		// If a client set us away, we don't touch that away message
		const CString sArg = sLine.Token(1, true).Trim_n(" ");
		if (sArg.empty() || sArg == ":")
			m_bClientSetAway = false;
		else
			m_bClientSetAway = true;

		m_bWeSetAway = false;

		return CONTINUE;
	}

	void SetAway(bool bTimer = true) {
		if (bTimer) {
			RemTimer("simple_away");
			AddTimer(new CSimpleAwayJob(this, m_iAwayWait, 1,
				"simple_away", "Sets you away after detach"));
		} else {
			if (!m_bClientSetAway) {
				PutIRC("AWAY :" + ExpandReason());
				m_bWeSetAway = true;
			}
		}
	}

	void SetBack() {
		RemTimer("simple_away");
		if (m_bWeSetAway) {
			PutIRC("AWAY");
			m_bWeSetAway = false;
		}
	}

private:
	CString ExpandReason() {
		CString sReason = m_sReason;
		if (sReason.empty())
			sReason = SIMPLE_AWAY_DEFAULT_REASON;

		time_t iTime = time(NULL);
		CString sTime = CUtils::CTime(iTime, GetUser()->GetTimezone());
		sReason.Replace("%s", sTime);

		return sReason;
	}

/* Settings */
	void SetReason(CString& sReason, bool bSave = true) {
		if (bSave)
			SetNV("reason", sReason);
		m_sReason = sReason;
	}

	void SetAwayWait(unsigned int iAwayWait, bool bSave = true) {
		if (bSave)
			SetNV("awaywait", CString(iAwayWait));
		m_iAwayWait = iAwayWait;
	}

};

void CSimpleAwayJob::RunJob() {
	((CSimpleAway*)GetModule())->SetAway(false);
}

template<> void TModInfo<CSimpleAway>(CModInfo& Info) {
	Info.SetWikiPage("simple_away");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("You might enter up to 3 arguments, like -notimer awaymessage or -timer 5 awaymessage.");
}

NETWORKMODULEDEFS(CSimpleAway, "This module will automatically set you away on IRC while you are disconnected from the bouncer.")
