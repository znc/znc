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

class CNotifyConnectMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CNotifyConnectMod) {}

	virtual void OnClientLogin()
	{
		SendAdmins(m_pUser->GetUserName() + " attached");
	}

	virtual void OnClientDisconnect()
	{
		SendAdmins(m_pUser->GetUserName() + " detached");
	}

private:
	void SendAdmins(const CString &msg)
	{
		CZNC::Get().Broadcast(msg, true, NULL, GetClient());
	}
};

GLOBALMODULEDEFS(CNotifyConnectMod, "");
