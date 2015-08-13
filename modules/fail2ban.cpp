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

class CFailToBanMod : public CModule {
public:
	MODCONSTRUCTOR(CFailToBanMod) {
		AddHelpCommand();
		AddCommand("Timeout",  static_cast<CModCommand::ModCmdFunc>(&CFailToBanMod::OnTimeoutCommand), "[minutes]", "The number of minutes IPs are blocked after a failed login.");
		AddCommand("Attempts", static_cast<CModCommand::ModCmdFunc>(&CFailToBanMod::OnAttemptsCommand), "[count]", "The number of allowed failed login attempts.");
		AddCommand("Ban",      static_cast<CModCommand::ModCmdFunc>(&CFailToBanMod::OnBanCommand), "<hosts>", "Ban the specified hosts.");
		AddCommand("Unban",    static_cast<CModCommand::ModCmdFunc>(&CFailToBanMod::OnUnbanCommand), "<hosts>", "Unban the specified hosts.");
		AddCommand("List",     static_cast<CModCommand::ModCmdFunc>(&CFailToBanMod::OnListCommand), "", "List banned hosts.");
	}
	virtual ~CFailToBanMod() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		CString sTimeout = sArgs.Token(0);
		CString sAttempts = sArgs.Token(1);
		unsigned int timeout = sTimeout.ToUInt();

		if (sAttempts.empty())
			m_uiAllowedFailed = 2;
		else
			m_uiAllowedFailed = sAttempts.ToUInt();

		if (sArgs.empty()) {
			timeout = 1;
		} else if (timeout == 0 || m_uiAllowedFailed == 0 || !sArgs.Token(2, true).empty()) {
			sMessage = "Invalid argument, must be the number of minutes "
				"IPs are blocked after a failed login and can be "
				"followed by number of allowed failed login attempts";
			return false;
		}

		// SetTTL() wants milliseconds
		m_Cache.SetTTL(timeout * 60 * 1000);

		return true;
	}

	void OnPostRehash() override {
		m_Cache.Clear();
	}

	void Add(const CString& sHost, unsigned int count) {
		m_Cache.AddItem(sHost, count, m_Cache.GetTTL());
	}

	bool Remove(const CString& sHost) {
		return m_Cache.RemItem(sHost);
	}

	void OnTimeoutCommand(const CString& sCommand) {
		CString sArg = sCommand.Token(1);

		if (!sArg.empty()) {
			unsigned int uTimeout = sArg.ToUInt();
			if (uTimeout == 0) {
				PutModule("Usage: Timeout [minutes]");
			} else {
				m_Cache.SetTTL(uTimeout * 60 * 1000);
				SetArgs(CString(m_Cache.GetTTL() / 60 / 1000) + " " + CString(m_uiAllowedFailed));
				PutModule("Timeout: " + CString(uTimeout) + " min");
			}
		} else {
			PutModule("Timeout: " + CString(m_Cache.GetTTL() / 60 / 1000) + " min");
		}
	}

	void OnAttemptsCommand(const CString& sCommand) {
		CString sArg = sCommand.Token(1);

		if (!sArg.empty()) {
			unsigned int uiAttempts = sArg.ToUInt();
			if (uiAttempts == 0) {
				PutModule("Usage: Attempts [count]");
			} else {
				m_uiAllowedFailed = uiAttempts;
				SetArgs(CString(m_Cache.GetTTL() / 60 / 1000) + " " + CString(m_uiAllowedFailed));
				PutModule("Attempts: " + CString(uiAttempts));
			}
		} else {
			PutModule("Attempts: " + CString(m_uiAllowedFailed));
		}
	}

	void OnBanCommand(const CString& sCommand) {
		CString sHosts = sCommand.Token(1, true);

		if (sHosts.empty()) {
			PutStatus("Usage: Ban <hosts>");
			return;
		}

		VCString vsHosts;
		sHosts.Replace(",", " ");
		sHosts.Split(" ", vsHosts, false, "", "", true, true);

		for (const CString& sHost : vsHosts) {
			Add(sHost, 0);
			PutModule("Banned: " + sHost);
		}
	}

	void OnUnbanCommand(const CString& sCommand) {
		CString sHosts = sCommand.Token(1, true);

		if (sHosts.empty()) {
			PutStatus("Usage: Unban <hosts>");
			return;
		}

		VCString vsHosts;
		sHosts.Replace(",", " ");
		sHosts.Split(" ", vsHosts, false, "", "", true, true);

		for (const CString& sHost : vsHosts) {
			if (Remove(sHost)) {
				PutModule("Unbanned: " + sHost);
			} else {
				PutModule("Ignored: " + sHost);
			}
		}
	}

	void OnListCommand(const CString& sCommand) {
		CTable Table;
		Table.AddColumn("Host");
		Table.AddColumn("Attempts");

		for (const auto& it : m_Cache.GetItems()) {
			Table.AddRow();
			Table.SetCell("Host", it.first);
			Table.SetCell("Attempts", CString(it.second));
		}

		if (Table.empty()) {
			PutModule("No bans");
		} else {
			PutModule(Table);
		}
	}

	void OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) override {
		unsigned int *pCount = m_Cache.GetItem(sHost);
		if (sHost.empty() || pCount == nullptr || *pCount < m_uiAllowedFailed) {
			return;
		}

		// refresh their ban
		Add(sHost, *pCount);

		pClient->Write("ERROR :Closing link [Please try again later - reconnecting too fast]\r\n");
		pClient->Close(Csock::CLT_AFTERWRITE);
	}

	void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) override {
		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount)
			Add(sRemoteIP, *pCount + 1);
		else
			Add(sRemoteIP, 1);
	}

	EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
		// e.g. webadmin ends up here
		const CString& sRemoteIP = Auth->GetRemoteIP();

		if (sRemoteIP.empty())
			return CONTINUE;

		unsigned int *pCount = m_Cache.GetItem(sRemoteIP);
		if (pCount && *pCount >= m_uiAllowedFailed) {
			// OnFailedLogin() will refresh their ban
			Auth->RefuseLogin("Please try again later - reconnecting too fast");
			return HALT;
		}

		return CONTINUE;
	}

private:
	TCacheMap<CString, unsigned int> m_Cache;
	unsigned int                     m_uiAllowedFailed;
};

template<> void TModInfo<CFailToBanMod>(CModInfo& Info) {
	Info.SetWikiPage("fail2ban");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("You might enter the time in minutes for the IP banning and the number of failed logins before any action is taken.");
}

GLOBALMODULEDEFS(CFailToBanMod, "Block IPs for some time after a failed login.")
