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

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <algorithm>

static const struct {
	const char *szName;
	const char *szDescription;
	const bool  bDefault;
} SupportedMechanisms[] = {
	{ "EXTERNAL",           "TLS certificate, for use with the *cert module", true },
	{ "PLAIN",              "Plain text negotiation, this should work always if the network supports SASL", true }
};

#define NV_REQUIRE_AUTH     "require_auth"
#define NV_MECHANISMS       "mechanisms"

class Mechanisms : public VCString {
public:
	void SetIndex(unsigned int uiIndex) {
		m_uiIndex = uiIndex;
	}

	unsigned int GetIndex() const {
		return m_uiIndex;
	}

	bool HasNext() const {
		return size() > (m_uiIndex + 1);
	}

	void IncrementIndex() {
		m_uiIndex++;
	}

	CString GetCurrent() const {
		return at(m_uiIndex);
	}

	CString GetNext() const {
		if (HasNext()) {
			return at(m_uiIndex + 1);
		}

		return "";
	}

private:
	unsigned int m_uiIndex;
};

class CSASLMod : public CModule {
public:
	MODCONSTRUCTOR(CSASLMod) {
		AddCommand("Help",        static_cast<CModCommand::ModCmdFunc>(&CSASLMod::PrintHelp),
			"search", "Generate this output");
		AddCommand("Set",         static_cast<CModCommand::ModCmdFunc>(&CSASLMod::Set),
			"<username> [<password>]", "Set username and password for the mechanisms that need them. Password is optional");
		AddCommand("Mechanism",   static_cast<CModCommand::ModCmdFunc>(&CSASLMod::SetMechanismCommand),
			"[mechanism[ ...]]", "Set the mechanisms to be attempted (in order)");
		AddCommand("RequireAuth", static_cast<CModCommand::ModCmdFunc>(&CSASLMod::RequireAuthCommand),
			"[yes|no]", "Don't connect unless SASL authentication succeeds");

		m_bAuthenticated = false;
	}

	void PrintHelp(const CString& sLine) {
		HandleHelpCommand(sLine);

		CTable Mechanisms;
		Mechanisms.AddColumn("Mechanism");
		Mechanisms.AddColumn("Description");

		for (const auto& it : SupportedMechanisms) {
			Mechanisms.AddRow();
			Mechanisms.SetCell("Mechanism",   it.szName);
			Mechanisms.SetCell("Description", it.szDescription);
		}

		PutModule("The following mechanisms are available:");
		PutModule(Mechanisms);
	}

	void Set(const CString& sLine) {
		SetNV("username", sLine.Token(1));
		SetNV("password", sLine.Token(2));

		PutModule("Username has been set to [" + GetNV("username") + "]");
		PutModule("Password has been set to [" + GetNV("password") + "]");
	}

	void SetMechanismCommand(const CString& sLine) {
		CString sMechanisms = sLine.Token(1, true).AsUpper();

		if (!sMechanisms.empty()) {
			VCString vsMechanisms;
			sMechanisms.Split(" ", vsMechanisms);

			for (const CString& sMechanism : vsMechanisms) {
				if (!SupportsMechanism(sMechanism)) {
					PutModule("Unsupported mechanism: " + sMechanism);
					return;
				}
			}

			SetNV(NV_MECHANISMS, sMechanisms);
		}

		PutModule("Current mechanisms set: " + GetMechanismsString());
	}

	void RequireAuthCommand(const CString& sLine) {
		if (!sLine.Token(1).empty()) {
			SetNV(NV_REQUIRE_AUTH, sLine.Token(1));
		}

		if (GetNV(NV_REQUIRE_AUTH).ToBool()) {
			PutModule("We require SASL negotiation to connect");
		} else {
			PutModule("We will connect even if SASL fails");
		}
	}

	bool SupportsMechanism(const CString& sMechanism) const {
		for (const auto& it : SupportedMechanisms) {
			if (sMechanism.Equals(it.szName)) {
				return true;
			}
		}

		return false;
	}

	CString GetMechanismsString() const {
		if (GetNV(NV_MECHANISMS).empty()) {
			CString sDefaults = "";

			for (const auto& it : SupportedMechanisms) {
				if (it.bDefault) {
					if (!sDefaults.empty()) {
						sDefaults += " ";
					}

					sDefaults += it.szName;
				}
			}

			return sDefaults;
		}

		return GetNV(NV_MECHANISMS);
	}

	bool CheckRequireAuth() {
		if (!m_bAuthenticated && GetNV(NV_REQUIRE_AUTH).ToBool()) {
			GetNetwork()->SetIRCConnectEnabled(false);
			PutModule("Disabling network, we require authentication.");
			PutModule("Use 'RequireAuth no' to disable.");
			return true;
		}

		return false;
	}

	void Authenticate(const CString& sLine) {
		if (m_Mechanisms.GetCurrent().Equals("PLAIN") && sLine.Equals("+")) {
			CString sAuthLine = GetNV("username") + '\0' + GetNV("username")  + '\0' + GetNV("password");
			sAuthLine.Base64Encode();
			PutIRC("AUTHENTICATE " + sAuthLine);
		} else {
			/* Send blank authenticate for other mechanisms (like EXTERNAL). */
			PutIRC("AUTHENTICATE +");
		}
	}

	bool OnServerCapAvailable(const CString& sCap) override {
		return sCap.Equals("sasl");
	}

	void OnServerCapResult(const CString& sCap, bool bSuccess) override {
		if (sCap.Equals("sasl")) {
			if (bSuccess) {
				GetMechanismsString().Split(" ", m_Mechanisms);

				if (m_Mechanisms.empty()) {
					CheckRequireAuth();
					return;
				}

				GetNetwork()->GetIRCSock()->PauseCap();

				m_Mechanisms.SetIndex(0);
				PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
			} else {
				CheckRequireAuth();
			}
		}
	}

	EModRet OnRaw(CString &sLine) override {
		if (sLine.Token(0).Equals("AUTHENTICATE")) {
			Authenticate(sLine.Token(1, true));
		} else if (sLine.Token(1).Equals("903")) {
			/* SASL success! */
			GetNetwork()->GetIRCSock()->ResumeCap();
			m_bAuthenticated = true;
			DEBUG("sasl: Authenticated with mechanism [" << m_Mechanisms.GetCurrent() << "]");
		} else if (sLine.Token(1).Equals("904") || sLine.Token(1).Equals("905")) {
			DEBUG("sasl: Mechanism [" << m_Mechanisms.GetCurrent() << "] failed.");
			PutModule(m_Mechanisms.GetCurrent() + " mechanism failed.");

			if (m_Mechanisms.HasNext()) {
				m_Mechanisms.IncrementIndex();
				PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
			} else {
				CheckRequireAuth();
				GetNetwork()->GetIRCSock()->ResumeCap();
			}
		} else if (sLine.Token(1).Equals("906")) {
			/* CAP wasn't paused? */
			DEBUG("sasl: Reached 906.");
			CheckRequireAuth();
		} else if (sLine.Token(1).Equals("907")) {
			m_bAuthenticated = true;
			GetNetwork()->GetIRCSock()->ResumeCap();
			DEBUG("sasl: Received 907 -- We are already registered");
		} else {
			return CONTINUE;
		}

		return HALT;
	}

	void OnIRCConnected() override {
		/* Just incase something slipped through, perhaps the server doesn't
		 * respond to our CAP negotiation. */

		CheckRequireAuth();
	}

	void OnIRCDisconnected() override {
		m_bAuthenticated = false;
	}

	CString GetWebMenuTitle() override { return "SASL"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName != "index") {
			// only accept requests to index
			return false;
		}

		if (WebSock.IsPost()) {
			SetNV("username", WebSock.GetParam("username"));
			CString sPassword = WebSock.GetParam("password");
			if (!sPassword.empty()) {
				SetNV("password", sPassword);
			}
			SetNV(NV_REQUIRE_AUTH, WebSock.GetParam("require_auth"));
			SetNV(NV_MECHANISMS, WebSock.GetParam("mechanisms"));
		}

		Tmpl["Username"] = GetNV("username");
		Tmpl["Password"] = GetNV("password");
		Tmpl["RequireAuth"] = GetNV(NV_REQUIRE_AUTH);
		Tmpl["Mechanisms"] = GetMechanismsString();

		for (const auto& it : SupportedMechanisms) {
			CTemplate& Row = Tmpl.AddRow("MechanismLoop");
			CString sName(it.szName);
			Row["Name"] = sName;
			Row["Description"] = CString(it.szDescription);
		}

		return true;
	}

private:
	Mechanisms m_Mechanisms;
	bool m_bAuthenticated;
};

template<> void TModInfo<CSASLMod>(CModInfo& Info) {
	Info.SetWikiPage("sasl");
}

NETWORKMODULEDEFS(CSASLMod, "Adds support for sasl authentication capability to authenticate to an IRC server")
