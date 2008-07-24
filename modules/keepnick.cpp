/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "User.h"
#include "IRCSock.h"

class CKeepNickMod;

class CKeepNickTimer : public CTimer {
public:
	CKeepNickTimer(CKeepNickMod *pMod);
	~CKeepNickTimer() {}

	void RunJob();

private:
	CKeepNickMod*	m_pMod;
};

class CKeepNickMod : public CModule {
public:
	MODCONSTRUCTOR(CKeepNickMod) {}

	~CKeepNickMod() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) {
		m_pTimer = NULL;

		// Check if we need to start the timer
		if (m_pUser->IsIRCConnected())
			OnIRCConnected();

		return true;
	}

	void KeepNick() {
		if (!m_pTimer)
			// No timer means we are turned off
			return;

		CIRCSock* pIRCSock = GetUser()->GetIRCSock();

		if (!pIRCSock)
			return;

		// Do we already have the nick we want?
		if (pIRCSock->GetNick().CaseCmp(GetNick()) == 0)
			return;

		PutIRC("NICK " + GetNick());
	}

	CString GetNick() {
		CString sConfNick = m_pUser->GetNick();
		CIRCSock* pIRCSock = GetUser()->GetIRCSock();

		if (pIRCSock)
			sConfNick = sConfNick.Left(pIRCSock->GetMaxNickLen());

		return sConfNick;
	}

	void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {
		if (sNewNick == GetUser()->GetIRCSock()->GetNick()) {
			// We are changing our own nick
			if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
				// We are changing our nick away from the conf setting.
				// Let's assume the user wants this and disable
				// this module (to avoid fighting nickserv).
				Disable();
			} else if (sNewNick.CaseCmp(GetNick()) == 0) {
				// We are changing our nick to the conf setting,
				// so we don't need that timer anymore.
				Disable();
			}
			return;
		}

		// If the nick we want is free now, be fast and get the nick
		if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
			KeepNick();
		}
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		// If someone with the nick we want quits, be fast and get the nick
		if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
			KeepNick();
		}
	}

	void OnIRCDisconnected() {
		// No way we can do something if we aren't connected to IRC.
		Disable();
	}

	void OnIRCConnected() {
		if (GetUser()->GetIRCSock()->GetNick().CaseCmp(GetNick()) != 0) {
			// We don't have the nick we want, try to get it
			Enable();
		}
	}

	void Enable() {
		if (m_pTimer)
			return;

		m_pTimer = new CKeepNickTimer(this);
		AddTimer(m_pTimer);
	}

	void Disable() {
		if (!m_pTimer)
			return;

		m_pTimer->Stop();
		RemTimer(m_pTimer->GetName());
		m_pTimer = NULL;
	}

	void OnModCommand(const CString& sCommand) {
		CString sCmd = sCommand.AsUpper();

		if (sCmd == "ENABLE") {
			Enable();
			PutModule("Trying to get your primary nick");
		} else if (sCmd == "DISABLE") {
			Disable();
			PutModule("No longer trying to get your primary nick");
		} else if (sCmd == "STATE") {
			if (m_pTimer)
				PutUser("Currently trying to get your primary nick");
			else
				PutUser("Currently disabled, try 'enable'");
		} else {
			PutUser("Commands: Enable, Disable, State");
		}
	}

private:
	// If this is NULL, we are turned off for some reason
	CKeepNickTimer*	m_pTimer;
};

CKeepNickTimer::CKeepNickTimer(CKeepNickMod *pMod) : CTimer(pMod, 30, 0,
		"KeepNickTimer", "Tries to acquire this user's primary nick") {
	m_pMod = pMod;
}

void CKeepNickTimer::RunJob() {
	m_pMod->KeepNick();
}

MODULEDEFS(CKeepNickMod, "Keep trying for your primary nick")

