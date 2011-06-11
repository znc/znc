/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"

class CFailToBanMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CFailToBanMod) {}
	virtual ~CFailToBanMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		CString sTimeout = sArgs.Token(0);
		CString sAttempts = sArgs.Token(1);
		unsigned int timeout = sTimeout.ToUInt();

		if (sAttempts.empty())
			m_uiAllowedFailed = 2;
		else
			m_uiAllowedFailed = sAttempts.ToUInt();;

		if (sArgs.empty()) {
			timeout = 1;
		} else if (timeout == 0 || m_uiAllowedFailed == 0 || !sArgs.Token(2, true).empty()) {
			sMessage = "Invalid argument, must be the number of minutes "
				"IPs are blocked after a failed login and can be "
				"followed by number of allowed failed login attempts";
			return false;
		}

		// SetTTL() wants milliseconds
		m_Cache.SetTTL(timeout * 60 * 1000);

		return true;
	}

	virtual void OnPostRehash() {
		m_Cache.Clear();
	}

	void Add(const CString& sHost, unsigned int count) {
		m_Cache.AddItem(sHost, count, m_Cache.GetTTL());
	}

	virtual void OnModCommand(const CString& sCommand) {
		PutModule("This module can only be configured through its arguments.");
		PutModule("The module argument is the number of minutes an IP");
		PutModule("is blocked after a failed login.");
	}

	virtual void OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) {
		unsigned int *pCount = m_Cache.GetItem(sHost);
		if (sHost.empty() || pCount == NULL || *pCount < m_uiAllowedFailed) {
			return;
		}

		// refresh their ban
		Add(sHost, *pCount);

		pClient->Write("ERROR :Closing link [Please try again later - reconnecting too fast]\r\n");
		pClient->Close(Csock::CLT_AFTERWRITE);
	}

	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount)
			Add(sRemoteIP, *pCount + 1);
		else
			Add(sRemoteIP, 1);
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		// e.g. webadmin ends up here
		const CString& sRemoteIP = Auth->GetRemoteIP();

		if (sRemoteIP.empty())
			return CONTINUE;

		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount && *pCount >= m_uiAllowedFailed) {
			// OnFailedLogin() will refresh their ban
			Auth->RefuseLogin("Please try again later - reconnecting too fast");
			return HALT;
		}

		return CONTINUE;
	}

private:
	TCacheMap<CString, unsigned int> m_Cache;
	unsigned int                     m_uiAllowedFailed;
};

template<> void TModInfo<CFailToBanMod>(CModInfo& Info) {
	Info.SetWikiPage("fail2ban");
}

GLOBALMODULEDEFS(CFailToBanMod, "Block IPs for some time after a failed login")
