/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

// @todo handle raw 433 (nick in use)
#include "IRCSock.h"
#include "User.h"

class CAwayNickMod;

class CAwayNickTimer : public CTimer {
public:
	CAwayNickTimer(CAwayNickMod& Module);

private:
	virtual void RunJob();

private:
	CAwayNickMod& m_Module;
};

class CBackNickTimer : public CTimer {
public:
	CBackNickTimer(CModule& Module)
		: CTimer(&Module, 3, 1, "BackNickTimer", "Set your nick back when you reattach"),
		  m_Module(Module) {}

private:
	virtual void RunJob() {
		CUser* pUser = m_Module.GetUser();

		if (pUser->IsUserAttached() && pUser->IsIRCConnected()) {
			CString sConfNick = pUser->GetNick();
			m_Module.PutIRC("NICK " + sConfNick);
		}
	}

private:
	CModule& m_Module;
};

class CAwayNickMod : public CModule {
public:
	MODCONSTRUCTOR(CAwayNickMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty())
			m_sFormat = sArgs;
		else
			m_sFormat = GetNV("nick");

		if (m_sFormat.empty()) {
			m_sFormat = "zz_%nick%";
		}

		SetNV("nick", m_sFormat);

		return true;
	}

	virtual ~CAwayNickMod() {
	}

	void StartAwayNickTimer() {
		RemTimer("AwayNickTimer");
		if (FindTimer("BackNickTimer")) {
			// Client disconnected before we got set back, so do nothing.
			RemTimer("BackNickTimer");
			return;
		}
		AddTimer(new CAwayNickTimer(*this));
	}

	void StartBackNickTimer() {
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			CString sConfNick = m_pUser->GetNick();

			if (pIRCSock->GetNick().Equals(m_sAwayNick.Left(pIRCSock->GetNick().length()))) {
				RemTimer("BackNickTimer");
				AddTimer(new CBackNickTimer(*this));
			}
		}
	}

	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick,
			CString& sIdent, CString& sRealName) {
		if (m_pUser && !m_pUser->IsUserAttached()) {
			m_sAwayNick = m_sFormat;

			// ExpandString doesn't know our nick yet, so do it by hand.
			m_sAwayNick.Replace("%nick%", sNick);

			// We don't limit this to NICKLEN, because we dont know
			// NICKLEN yet.
			sNick = m_sAwayNick = m_pUser->ExpandString(m_sAwayNick);
		}
		return CONTINUE;
	}

	virtual void OnIRCDisconnected() {
		RemTimer("AwayNickTimer");
		RemTimer("BackNickTimer");
	}

	virtual void OnClientLogin() {
		StartBackNickTimer();
	}

	virtual void OnClientDisconnect() {
		if (!m_pUser->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);
		if (sCommand.Equals("TIMERS")) {
			ListTimers();
		}
		else if (sCommand.Equals("SET")) {
			CString sFormat = sLine.Token(1);

			if (!sFormat.empty()) {
				m_sFormat = sFormat;
				SetNV("nick", m_sFormat);
			}

			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Equals("SHOW")) {
			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Equals("HELP")) {
			PutModule("Commands are: show, timers, set [awaynick]");
		}
	}

	CString GetAwayNick() {
		unsigned int uLen = 9;
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			uLen = pIRCSock->GetMaxNickLen();
		}

		m_sAwayNick = m_pUser->ExpandString(m_sFormat).Left(uLen);
		return m_sAwayNick;
	}

private:
	CString m_sFormat;
	CString m_sAwayNick;
};

CAwayNickTimer::CAwayNickTimer(CAwayNickMod& Module)
	: CTimer(&Module, 30, 1, "AwayNickTimer", "Set your nick while you're detached"),
	  m_Module(Module) {}

void CAwayNickTimer::RunJob() {
	CUser* pUser = m_Module.GetUser();

	if (!pUser->IsUserAttached() && pUser->IsIRCConnected()) {
		m_Module.PutIRC("NICK " + m_Module.GetAwayNick());
	}
}

template<> void TModInfo<CAwayNickMod>(CModInfo& Info) {
	Info.SetWikiPage("awaynick");
}

MODULEDEFS(CAwayNickMod, "Change your nick while you are away")
