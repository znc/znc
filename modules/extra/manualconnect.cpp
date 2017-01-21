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
		// Iterator through all users.
		map<CString,CUser*>::const_iterator uEnd=msUsers.end();
		for(map<CString,CUser*>::const_iterator uIt=msUsers.begin(); uIt != uEnd; ++uIt) {
			CUser *pUser=uIt->second;
			if(pUser == NULL) {
				CUtils::PrintError("znc has NULL CUser pointer in its user map.");
				return false;
			}

			const vector<CIRCNetwork*>& vNet=pUser->GetNetworks();
			// Set IRCConnectEnabled to false for all networks in each user.
			vector<CIRCNetwork*>::const_iterator nEnd=vNet.end();
			for(vector<CIRCNetwork*>::const_iterator nIt=vNet.begin(); nIt != nEnd; ++nIt) {
				CIRCNetwork* pNet=*nIt;
				if(pNet == NULL) {
					CUtils::PrintError("User "+pUser->GetUserName()+" has NULL in GetNetworks()");
					return false;
				}
				pNet->SetIRCConnectEnabled(false);
			}
		}

		return true;
	}

};

template<> void TModInfo<CManualConnect>(CModInfo& Info) {
	Info.SetWikiPage("manualconnect");
}

GLOBALMODULEDEFS(CManualConnect, "While znc is tarted, it prevents znc from connecting to networks with IRCConnectEnabled=true in znc.conf")

