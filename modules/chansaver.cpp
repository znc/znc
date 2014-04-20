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

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>

using std::vector;

class CChanSaverMod : public CModule {
public:
	MODCONSTRUCTOR(CChanSaverMod) {
		m_bWriteConf = false;

		switch (GetType()) {
			case CModInfo::GlobalModule:
				LoadUsers();
				break;
			case CModInfo::UserModule:
				LoadUser(*GetUser());
				break;
			case CModInfo::NetworkModule:
				LoadNetwork(*GetNetwork());
				break;
		}
	}

	virtual ~CChanSaverMod() {
	}

	void LoadUsers() {
		std::map<CString, CUser *> vUsers = CZNC::Get().GetUserMap();
		for (std::map<CString, CUser *>::iterator it = vUsers.begin(); it != vUsers.end(); ++it) {
			LoadUser(*it->second);
		}
	}

	void LoadUser(CUser &user) {
		vector<CIRCNetwork*> vNetworks = user.GetNetworks();
		for (vector<CIRCNetwork*>::iterator it = vNetworks.begin(); it != vNetworks.end(); ++it) {
			CIRCNetwork &network = **it;
			LoadNetwork(network);
		}
	}

	void LoadNetwork(CIRCNetwork &network) {
		const vector<CChan*>& vChans = network.GetChans();

		for (vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
			CChan &chan = **it;

			// If that channel isn't yet in the config,
			// we'll have to add it...
			if (!chan.InConfig()) {
				chan.SetInConfig(true);
				m_bWriteConf = true;
			}
		}
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (m_bWriteConf) {
			CZNC::Get().WriteConfig();
			m_bWriteConf = false;
		}

		return CONTINUE;
	}

	virtual void OnMode2(const CNick* pOpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) {
		// This is called when we join (ZNC requests the channel modes
		// on join) *and* when someone changes the channel keys.
		// We ignore channel key "*" because of some broken nets.
		if (uMode != 'k' || bNoChange || !bAdded || sArg == "*")
			return;

		Channel.SetKey(sArg);
		m_bWriteConf = true;
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick() && !Channel.InConfig()) {
			Channel.SetInConfig(true);
			CZNC::Get().WriteConfig();
		}
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick() && Channel.InConfig()) {
			Channel.SetInConfig(false);
			CZNC::Get().WriteConfig();
		}
	}

private:
	bool m_bWriteConf;
};

template<> void TModInfo<CChanSaverMod>(CModInfo& Info) {
	Info.SetWikiPage("chansaver");
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
}

USERMODULEDEFS(CChanSaverMod, "Keep config up-to-date when user joins/parts")
