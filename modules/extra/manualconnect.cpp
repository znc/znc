/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * If you have any questions or suggestions, please contact
 * Author crocket <crockabiscuit@gmail.com>
 */

#include <znc/User.h>
#include <znc/IRCNetwork.h>

class CManualConnect : public CModule {
public:
	MODCONSTRUCTOR(CManualConnect) {}

	virtual ~CManualConnect() {
	}

	virtual bool OnBoot() {
		const map<CString,CUser*> &msUsers=CZNC::Get().GetUserMap();
		map<CString,CUser*>::const_iterator uEnd;
		// Iterator through all users.
		for(map<CString,CUser*>::const_iterator uIt=msUsers.begin(); uIt != uEnd; ++uIt) {
			const vector<CIRCNetwork*>& vNet=uIt->second->GetNetworks();
			vector<CIRCNetwork*>::const_iterator nEnd=vNet.end();
			// Set IRCConnectEnabled to false for all networks in each user.
			for(vector<CIRCNetwork*>::const_iterator nIt=vNet.begin(); nIt != nEnd; ++nIt)
				(*nIt)->SetIRCConnectEnabled(false);
		}

		return true;
	}

};

template<> void TModInfo<CManualConnect>(CModInfo& Info) {
	Info.SetWikiPage("manualconnect");
}

GLOBALMODULEDEFS(CManualConnect, "While znc is tarted, it prevents znc from connecting to networks with IRCConnectEnabled=yes in znc.conf")

