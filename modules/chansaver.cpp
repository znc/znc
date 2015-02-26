/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

class CChanSaverMod : public CModule {
public:
	MODCONSTRUCTOR(CChanSaverMod) {
		switch (GetType()) {
			case CModInfo::GlobalModule:
				LoadUsers();
				break;
			case CModInfo::UserModule:
				LoadUser(GetUser());
				break;
			case CModInfo::NetworkModule:
				LoadNetwork(GetNetwork());
				break;
		}
	}

	virtual ~CChanSaverMod() {
	}

	void LoadUsers() {
		const std::map<CString, CUser*>& vUsers = CZNC::Get().GetUserMap();
		for (const auto& user : vUsers) {
			LoadUser(user.second);
		}
	}

	void LoadUser(CUser* pUser) {
		const std::vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();
		for (const CIRCNetwork* pNetwork : vNetworks) {
			LoadNetwork(pNetwork);
		}
	}

	void LoadNetwork(const CIRCNetwork* pNetwork) {
		const std::vector<CChan*>& vChans = pNetwork->GetChans();
		for (CChan* pChan : vChans) {
			// If that channel isn't yet in the config,
			// we'll have to add it...
			if (!pChan->InConfig()) {
				pChan->SetInConfig(true);
			}
		}
	}

	void OnJoin(const CNick& Nick, CChan& Channel) override {
		if (!Channel.InConfig() && GetNetwork()->GetIRCNick().NickEquals(Nick.GetNick())) {
			Channel.SetInConfig(true);
		}
	}

	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override {
		if (Channel.InConfig() && GetNetwork()->GetIRCNick().NickEquals(Nick.GetNick())) {
			Channel.SetInConfig(false);
		}
	}
};

template<> void TModInfo<CChanSaverMod>(CModInfo& Info) {
	Info.SetWikiPage("chansaver");
	Info.AddType(CModInfo::NetworkModule);
	Info.AddType(CModInfo::GlobalModule);
}

USERMODULEDEFS(CChanSaverMod, "Keep config up-to-date when user joins/parts.")
