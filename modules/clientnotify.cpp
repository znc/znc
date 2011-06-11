/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "User.h"

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
				CString(m_pUser->GetClients().size())  + " clients.");

			// the set<> will automatically disregard duplicates:
			m_sClientsSeen.insert(m_pClient->GetRemoteIP());
		}
	}

	void OnClientDisconnect() {
		if(m_bOnDisconnect) {
			SendNotification("A client disconnected from your user. "
				"Use the 'ListClients' command to see the " +
				CString(m_pUser->GetClients().size()) + " remaining client(s).");
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

MODULEDEFS(CClientNotifyMod, "Notifies you when another IRC client logs into or out of your account. Configurable.")

