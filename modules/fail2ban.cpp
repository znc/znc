/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
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
		unsigned int timeout = sArgs.ToUInt();

		if (sArgs.empty()) {
			timeout = 1;
		} else if (timeout == 0 && sArgs != "0") {
			sMessage = "Invalid argument, must be the number of minutes "
				"IPs are blocked after a failed login";
			return false;
		}

		// SetTTL() wants milliseconds
		m_Cache.SetTTL(timeout * 60 * 1000);

		return true;
	}

	virtual void OnModCommand(const CString& sCommand) {
		PutModule("This module can only be configured through its arguments.");
		PutModule("The module argument is the number of minutes an IP");
		PutModule("is blocked after a failed login.");
	}

	virtual void OnClientConnect(CClient* pClient, const CString& sHost, unsigned short uPort) {
		if (sHost.empty() || !m_Cache.HasItem(sHost)) {
			return;
		}

		// refresh their ban
		m_Cache.AddItem(sHost);

		pClient->PutClient("ERROR :Closing link [Please try again later - reconnecting too fast]");
		pClient->Close(Csock::CLT_AFTERWRITE);
	}

	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
		m_Cache.AddItem(sRemoteIP);
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		// e.g. webadmin ends up here
		const CString& sRemoteIP = Auth->GetRemoteIP();

		if (sRemoteIP.empty())
			return CONTINUE;

		if (m_Cache.HasItem(sRemoteIP)) {
			// OnFailedLoginAttempt() will refresh their ban
			Auth->RefuseLogin("Please try again later - reconnecting too fast");
			return HALT;
		}

		return CONTINUE;
	}

private:
	TCacheMap<CString>	m_Cache;
};

GLOBALMODULEDEFS(CFailToBanMod, "Block IPs for some time after a failed login")
