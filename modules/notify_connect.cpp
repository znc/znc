/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/znc.h>
#include <znc/User.h>

class CNotifyConnectMod : public CModule {
public:
	MODCONSTRUCTOR(CNotifyConnectMod) {}

	virtual void OnClientLogin() {
		SendAdmins(m_pUser->GetUserName() + " attached (from " + m_pClient->GetRemoteIP() + ")");
	}

	virtual void OnClientDisconnect() {
		SendAdmins(m_pUser->GetUserName() + " detached (gone: " + m_pClient->GetRemoteIP() + ")");
	}

private:
	void SendAdmins(const CString &msg) {
		CZNC::Get().Broadcast(msg, true, NULL, GetClient());
	}
};

template<> void TModInfo<CNotifyConnectMod>(CModInfo& Info) {
	Info.SetWikiPage("notify_connect");
}

GLOBALMODULEDEFS(CNotifyConnectMod, "Notifies all admin users when a client connects or disconnects.")
