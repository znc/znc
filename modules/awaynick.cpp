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

// @todo handle raw 433 (nick in use)
#include <znc/IRCSock.h>
#include <znc/IRCNetwork.h>

class CAwayNickMod;

class CAwayNickTimer : public CTimer {
public:
	CAwayNickTimer(CAwayNickMod& Module);

private:
	virtual void RunJob();

private:
	CAwayNickMod& m_Module;
};

class CBackNickTimer : public CTimer {
public:
	CBackNickTimer(CModule& Module)
		: CTimer(&Module, 3, 1, "BackNickTimer", "Set your nick back when you reattach"),
		  m_Module(Module) {}

private:
	virtual void RunJob() {
		CIRCNetwork* pNetwork = m_Module.GetNetwork();

		if (pNetwork->IsUserAttached() && pNetwork->IsIRCConnected()) {
			CString sConfNick = pNetwork->GetNick();
			m_Module.PutIRC("NICK " + sConfNick);
		}
	}

private:
	CModule& m_Module;
};

class CAwayNickMod : public CModule {
public:
	MODCONSTRUCTOR(CAwayNickMod) {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty())
			m_sFormat = sArgs;
		else
			m_sFormat = GetNV("nick");

		if (m_sFormat.empty()) {
			m_sFormat = "zz_%nick%";
		}

		SetNV("nick", m_sFormat);

		return true;
	}

	virtual ~CAwayNickMod() {
	}

	void StartAwayNickTimer() {
		RemTimer("AwayNickTimer");
		if (FindTimer("BackNickTimer")) {
			// Client disconnected before we got set back, so do nothing.
			RemTimer("BackNickTimer");
			return;
		}
		AddTimer(new CAwayNickTimer(*this));
	}

	void StartBackNickTimer() {
		CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();

		if (pIRCSock) {
			CString sConfNick = m_pNetwork->GetNick();

			if (pIRCSock->GetNick().Equals(m_sAwayNick.Left(pIRCSock->GetNick().length()))) {
				RemTimer("BackNickTimer");
				AddTimer(new CBackNickTimer(*this));
			}
		}
	}

	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick,
			CString& sIdent, CString& sRealName) {
		if (m_pNetwork && !m_pNetwork->IsUserAttached()) {
			m_sAwayNick = m_sFormat;

			// ExpandString doesn't know our nick yet, so do it by hand.
			m_sAwayNick.Replace("%nick%", sNick);

			// We don't limit this to NICKLEN, because we dont know
			// NICKLEN yet.
			sNick = m_sAwayNick = m_pNetwork->ExpandString(m_sAwayNick);
		}
		return CONTINUE;
	}

	virtual void OnIRCDisconnected() {
		RemTimer("AwayNickTimer");
		RemTimer("BackNickTimer");
	}

	virtual void OnClientLogin() {
		StartBackNickTimer();
	}

	virtual void OnClientDisconnect() {
		if (!m_pNetwork->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);
		if (sCommand.Equals("TIMERS")) {
			ListTimers();
		}
		else if (sCommand.Equals("SET")) {
			CString sFormat = sLine.Token(1);

			if (!sFormat.empty()) {
				m_sFormat = sFormat;
				SetNV("nick", m_sFormat);
			}

			if (m_pNetwork) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Equals("SHOW")) {
			if (m_pNetwork) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Equals("HELP")) {
			PutModule("Commands are: show, timers, set [awaynick]");
		}
	}

	CString GetAwayNick() {
		unsigned int uLen = 9;
		CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();

		if (pIRCSock) {
			uLen = pIRCSock->GetMaxNickLen();
		}

		m_sAwayNick = m_pNetwork->ExpandString(m_sFormat).Left(uLen);
		return m_sAwayNick;
	}

private:
	CString m_sFormat;
	CString m_sAwayNick;
};

CAwayNickTimer::CAwayNickTimer(CAwayNickMod& Module)
	: CTimer(&Module, 30, 1, "AwayNickTimer", "Set your nick while you're detached"),
	  m_Module(Module) {}

void CAwayNickTimer::RunJob() {
	CIRCNetwork* pNetwork = m_Module.GetNetwork();

	if (!pNetwork->IsUserAttached() && pNetwork->IsIRCConnected()) {
		m_Module.PutIRC("NICK " + m_Module.GetAwayNick());
	}
}

template<> void TModInfo<CAwayNickMod>(CModInfo& Info) {
	Info.SetWikiPage("awaynick");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("This will be your nickname while you are away. Examples: nick_off or zzz_nick.");
}

NETWORKMODULEDEFS(CAwayNickMod, "Change your nick while you are away")
