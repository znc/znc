/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */


#include "Modules.h"
#include "User.h"
#include "Chan.h"

class CKickClientOnIRCDisconnect: public CModule {
public:
	MODCONSTRUCTOR(CKickClientOnIRCDisconnect) {}

	void OnIRCDisconnected()
	{
		const vector<CChan*>& vChans = m_pUser->GetChans();

		for(vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
		{
			if((*it)->IsOn()) {
				PutUser(":ZNC!znc@znc.in KICK " + (*it)->GetName() + " " + m_pUser->GetIRCNick().GetNick()
					+ " :You have been disconnected from the IRC server");
			}
		}
	}
};

template<> void TModInfo<CKickClientOnIRCDisconnect>(CModInfo& Info) {
	Info.SetWikiPage("disconkick");
}

MODULEDEFS(CKickClientOnIRCDisconnect, "Kicks the client from all channels when the connection to the IRC server is lost")
