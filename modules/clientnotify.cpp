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

#include <znc/znc.h>
#include <znc/User.h>

using std::set;

class CClientNotifyMod : public CModule {
protected:
	CString m_sMethod;
	bool m_bNewOnly;
	bool m_bOnDisconnect;

	set<CString> m_sClientsSeen;

	void SaveSettings() {
		SetNV("method", m_sMethod);
		SetNV("newonly", m_bNewOnly ? "1" : "0");
		SetNV("ondisconnect", m_bOnDisconnect ? "1" : "0");
	}

	void SendNotification(const CString& sMessage) {
		if(m_sMethod == "message") {
			m_pUser->PutStatus(sMessage, NULL, m_pClient);
		}
		else if(m_sMethod == "notice") {
			m_pUser->PutStatusNotice(sMessage, NULL, m_pClient);
		}
	}

public:
	MODCONSTRUCTOR(CClientNotifyMod) {
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) {
		m_sMethod = GetNV("method");

		if(m_sMethod != "notice" && m_sMethod != "message" && m_sMethod != "off") {
			m_sMethod = "message";
		}

		// default = off for these:

		m_bNewOnly = (GetNV("newonly") == "1");
		m_bOnDisconnect = (GetNV("ondisconnect") == "1");

		return true;
	}

	void OnClientLogin() {
		if(!m_bNewOnly || m_sClientsSeen.find(m_pClient->GetRemoteIP()) == m_sClientsSeen.end()) {
			SendNotification("Another client authenticated as your user. "
				"Use the 'ListClients' command to see all " +
				CString(m_pUser->GetAllClients().size())  + " clients.");

			// the set<> will automatically disregard duplicates:
			m_sClientsSeen.insert(m_pClient->GetRemoteIP());
		}
	}

	void OnClientDisconnect() {
		if(m_bOnDisconnect) {
			SendNotification("A client disconnected from your user. "
				"Use the 'ListClients' command to see the " +
				CString(m_pUser->GetAllClients().size()) + " remaining client(s).");
		}
	}

	void OnModCommand(const CString& sCommand) {
		const CString& sCmd = sCommand.Token(0).AsLower();
		const CString& sArg = sCommand.Token(1, true).AsLower();

		if (sCmd.Equals("method") && !sArg.empty()) {
			if(sArg != "notice" && sArg != "message" && sArg != "off") {
				PutModule("Unknown method. Use one of: message / notice / off");
			}
			else {
				m_sMethod = sArg;
				SaveSettings();
				PutModule("Saved.");
			}
		}
		else if (sCmd.Equals("newonly") && !sArg.empty()) {
			m_bNewOnly = (sArg == "on" || sArg == "true");
			SaveSettings();
			PutModule("Saved.");
		}
		else if (sCmd.Equals("ondisconnect") && !sArg.empty()) {
			m_bOnDisconnect = (sArg == "on" || sArg == "true");
			SaveSettings();
			PutModule("Saved.");
		}
		else {
			PutModule("Current settings: Method: " + m_sMethod + ", for unseen IP addresses only: " + CString(m_bNewOnly) +
				", notify on disconnecting clients: " + CString(m_bOnDisconnect));
			PutModule("Commands: show, method <message|notice|off>, newonly <on|off>, ondisconnect <on|off>");
		}
	}
};

template<> void TModInfo<CClientNotifyMod>(CModInfo& Info) {
	Info.SetWikiPage("clientnotify");
}

USERMODULEDEFS(CClientNotifyMod, "Notifies you when another IRC client logs into or out of your account. Configurable.")

