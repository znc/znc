/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"

class CConnectThrottleMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CConnectThrottleMod) {}
	virtual ~CConnectThrottleMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		unsigned int timeout = sArgs.ToUInt();

		if (sArgs.empty()) {
			timeout = 60; // 1 min
		} else if (timeout == 0 && sArgs != "0") {
			sMessage = "Invalid argument, must be a positive number "
				"which is the time one has to wait after failed login attempts";
			return false;
		}

		// SetTTL() wants milliseconds
		m_Cache.SetTTL(timeout * 1000);

		return true;
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
		const CString &sRemoteIP = Auth->GetRemoteIP();

		if (sRemoteIP.empty())
			return CONTINUE;

		if (m_Cache.HasItem(sRemoteIP)) {
			// refresh their ban
			m_Cache.AddItem(sRemoteIP);

			Auth->RefuseLogin("Please try again later - reconnecting too fast");
			return HALT;
		}

		return CONTINUE;
	}

private:
	TCacheMap<CString>	m_Cache;
};

GLOBALMODULEDEFS(CConnectThrottleMod, "Limit the number of login attempts a user can make per time")
