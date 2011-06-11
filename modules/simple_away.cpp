/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"

#define SIMPLE_AWAY_DEFAULT_REASON "Auto away at %s"
#define SIMPLE_AWAY_DEFAULT_TIME   60


class CSimpleAway;

class CSimpleAwayJob : public CTimer {
public:
	CSimpleAwayJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CSimpleAwayJob() {}

protected:
	virtual void RunJob();
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
	}

	virtual ~CSimpleAway() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
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

		return true;
	}

	virtual void OnIRCConnected() {
		if (m_pUser->IsUserAttached())
			SetBack();
		else
			SetAway(false);
	}

	virtual void OnClientLogin() {
		SetBack();
	}

	virtual void OnClientDisconnect() {
		/* There might still be other clients */
		if (!m_pUser->IsUserAttached())
			SetAway();
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);

		if (sCommand.Equals("help")) {
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Description");
			Table.AddRow();
			Table.SetCell("Command", "Reason [<text>]");
			Table.SetCell("Description", "Prints and optionally sets the away reason.");
			Table.AddRow();
			Table.SetCell("Command", "Timer");
			Table.SetCell("Description", "Prints the current time to wait before setting you away.");
			Table.AddRow();
			Table.SetCell("Command", "SetTimer <time>");
			Table.SetCell("Description", "Sets the time to wait before setting you away (in seconds).");
			Table.AddRow();
			Table.SetCell("Command", "DisableTimer");
			Table.SetCell("Description", "Disables the wait time before setting you away.");
			PutModule(Table);

			PutModule("In the away reason, %s will be replaced with the time you were set away.");

		} else if (sCommand.Equals("reason")) {
			CString sReason = sLine.Token(1, true);

			if (!sReason.empty()) {
				SetReason(sReason);
				PutModule("Away reason set");
			} else {
				PutModule("Away reason: " + m_sReason);
				PutModule("Current away reason would be: " + ExpandReason());
			}

		} else if (sCommand.Equals("timer")) {
			PutModule("Current timer setting: "
					+ CString(m_iAwayWait) + " seconds");

		} else if (sCommand.Equals("settimer")) {
			SetAwayWait(sLine.Token(1).ToUInt());

			if (m_iAwayWait == 0)
				PutModule("Timer disabled");
			else
				PutModule("Timer set to "
						+ CString(m_iAwayWait) + " seconds");

		} else if (sCommand.Equals("disabletimer")) {
			SetAwayWait(0);
			PutModule("Timer disabled");

		} else {
			PutModule("Unknown command. Try 'help'.");
		}
	}

	virtual EModRet OnUserRaw(CString &sLine) {
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
		iTime += (time_t)(m_pUser->GetTimezoneOffset() * 60 * 60); // offset is in hours
		CString sTime = ctime(&iTime);
		sTime.Trim();
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
	((CSimpleAway*)m_pModule)->SetAway(false);
}

template<> void TModInfo<CSimpleAway>(CModInfo& Info) {
	Info.SetWikiPage("simple_away");
}

MODULEDEFS(CSimpleAway, "Auto away when last client disconnects")
