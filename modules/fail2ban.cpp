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
	MODCONSTRUCTOR(CFailToBanMod) {}
	virtual ~CFailToBanMod() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		CString sTimeout = sArgs.Token(0);
		CString sAttempts = sArgs.Token(1);
		unsigned int timeout = sTimeout.ToUInt();

		if (sAttempts.empty())
			m_uiAllowedFailed = 2;
		else
			m_uiAllowedFailed = sAttempts.ToUInt();;

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

	void OnModCommand(const CString& sCommand) override {
		PutModule("This module can only be configured through its arguments.");
		PutModule("The module argument is the number of minutes an IP");
		PutModule("is blocked after a failed login.");
	}

	void OnClientConnect(CZNCSock* pClient, const CString& sHost, unsigned short uPort) override {
		unsigned int *pCount = m_Cache.GetItem(sHost);
		if (sHost.empty() || pCount == NULL || *pCount < m_uiAllowedFailed) {
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
