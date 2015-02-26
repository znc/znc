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

#include <znc/znc.h>
#include <znc/User.h>

class CNotifyConnectMod : public CModule {
public:
	MODCONSTRUCTOR(CNotifyConnectMod) {}

	void OnClientLogin() override {
		SendAdmins(GetUser()->GetUserName() + " attached (from " + GetClient()->GetRemoteIP() + ")");
	}

	void OnClientDisconnect() override {
		SendAdmins(GetUser()->GetUserName() + " detached (from " + GetClient()->GetRemoteIP() + ")");
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
