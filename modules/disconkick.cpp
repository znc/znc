/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>

using std::vector;

class CKickClientOnIRCDisconnect: public CModule {
public:
	MODCONSTRUCTOR(CKickClientOnIRCDisconnect) {}

	void OnIRCDisconnected()
	{
		const vector<CChan*>& vChans = m_pNetwork->GetChans();

		for(vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
		{
			if((*it)->IsOn()) {
				PutUser(":ZNC!znc@znc.in KICK " + (*it)->GetName() + " " + m_pNetwork->GetIRCNick().GetNick()
					+ " :You have been disconnected from the IRC server");
			}
		}
	}
};

template<> void TModInfo<CKickClientOnIRCDisconnect>(CModInfo& Info) {
	Info.SetWikiPage("disconkick");
}

USERMODULEDEFS(CKickClientOnIRCDisconnect, "Kicks the client from all channels when the connection to the IRC server is lost")
