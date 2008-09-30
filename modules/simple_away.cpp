/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"

class CSimpleAway;

class CSimpleAwayJob : public CTimer
{
public:
	CSimpleAwayJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CSimpleAwayJob() {}

protected:
	virtual void RunJob();
};

class CSimpleAway : public CModule
{
public:
	MODCONSTRUCTOR(CSimpleAway)
	{
		m_sReason = "Auto away at %s";
		m_iAwayWait = 60;
		m_bClientSetAway = false;
	}

	virtual ~CSimpleAway()
	{
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		CString sMyArgs = sArgs;

		if (sMyArgs.Token(0) == "-notimer") {
			m_iAwayWait = 0;
			sMyArgs = sMyArgs.Token(1, true);
		} else if (sMyArgs.Token(0) == "-timer") {
			m_iAwayWait = sMyArgs.Token(1).ToInt();
			sMyArgs = sMyArgs.Token(2, true);
		}
		if (!sMyArgs.empty()) {
			m_sReason = sMyArgs;
		}

		return true;
	}

	virtual void OnIRCConnected()
	{
		if (m_pUser->IsUserAttached())
			Back();
		else
			Away();
	}

	virtual void OnUserAttached()
	{
		Back();
	}

	virtual void OnUserDetached()
	{
		/* There might still be other clients */
		if (!m_pUser->IsUserAttached())
			StartTimer();
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString sCmdName = sCommand.Token(0);

		if (sCmdName == "disabletimer") {
			m_iAwayWait = 0;
			PutModule("Timer disabled");
		} else if (sCmdName == "settimer") {
			int iSetting = sCommand.Token(1).ToInt();

			m_iAwayWait = iSetting;

			if (iSetting == 0)
				PutModule("Timer disabled");
			else
				PutModule("Timer set to "
						+ CString(iSetting) + " seconds");
		} else if (sCmdName == "timer") {
			PutModule("Current timer setting: "
					+ CString(m_iAwayWait) + " seconds");
		} else if (sCmdName == "reason") {
			CString sReason = sCommand.Token(1, true);

			if (!sReason.empty()) {
				m_sReason = sReason;
				PutModule("Reason set (Use %s for away time)");
			} else
				PutModule("Current away reason would be: " + GetAway());
		} else {
			PutModule("Commands: disabletimer, settimer <x>, timer, reason [text]");
		}
	}

	virtual EModRet OnUserRaw(CString &sLine)
	{
		const CString sCmd = sLine.Token(0);
		const CString sArg = sLine.Token(1, true).Trim_n(" ");

		if (!sCmd.Equals("AWAY"))
			return CONTINUE;

		// When a client sets us away, we don't touch that away message
		if (sArg.empty() || sArg == ":")
			m_bClientSetAway = false;
		else
			m_bClientSetAway = true;

		return CONTINUE;
	}

	void StartTimer()
	{
		CSimpleAwayJob *p;

		RemTimer("simple_away");

		p = new CSimpleAwayJob(this, m_iAwayWait, 1, "simple_away",
				"Sets you away after detach");
		AddTimer(p);
	}

	CString GetAway()
	{
		time_t iTime = time(NULL);
		char *pTime = ctime(&iTime);
		CString sTime;
		CString sReason = m_sReason;

		if (sReason.empty())
			sReason = "Auto away at %s";

		if (pTime) {
			sTime = pTime;
			sTime.Trim();

			sReason.Replace("%s", sTime);
		}

		return sReason;
	}

	void Away()
	{
		CString sReason = GetAway();

		if (!m_bClientSetAway)
			PutIRC("AWAY :" + sReason);
	}

	void Back()
	{
		if (!m_bClientSetAway)
			PutIRC("AWAY");
		RemTimer("simple_away");
	}

private:

	bool	m_bClientSetAway;
	time_t	m_iAwayWait;
	CString	m_sReason;
};


void CSimpleAwayJob::RunJob()
{
	CSimpleAway *p = (CSimpleAway *)m_pModule;

	p->Away();
}

MODULEDEFS(CSimpleAway, "Auto away when last client disconnects")
