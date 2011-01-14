/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "User.h"

class CNotifyConnectMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CNotifyConnectMod) {}

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

GLOBALMODULEDEFS(CNotifyConnectMod, "Notifies all admin users when a client connects or disconnects.")
