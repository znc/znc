/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#define NV_REQUIRE_AUTH "require_auth"
#define NV_MECHANISMS "mechanisms"

class Mechanisms : public VCString {
  public:
    void SetIndex(unsigned int uiIndex) { m_uiIndex = uiIndex; }

    unsigned int GetIndex() const { return m_uiIndex; }

    bool HasNext() const { return size() > (m_uiIndex + 1); }

    void IncrementIndex() { m_uiIndex++; }

    CString GetCurrent() const { return at(m_uiIndex); }

    CString GetNext() const {
        if (HasNext()) {
            return at(m_uiIndex + 1);
        }

        return "";
    }

  private:
    unsigned int m_uiIndex = 0;
};

class CSASLMod : public CModule {
    const struct {
        const char* szName;
        CDelayedTranslation sDescription;
        bool bDefault;
    } SupportedMechanisms[2] = {
        {"EXTERNAL", t_d("TLS certificate, for use with the *cert module"),
         true},
        {"PLAIN", t_d("Plain text negotiation, this should work always if the "
                      "network supports SASL"),
         true}};

  public:
    MODCONSTRUCTOR(CSASLMod) {
        AddCommand("Help", t_d("search"), t_d("Generate this output"),
                   [=](const CString& sLine) { PrintHelp(sLine); });
        AddCommand("Set", t_d("[<username> [<password>]]"),
                   t_d("Set username and password for the mechanisms that need "
                       "them. Password is optional. Without parameters, "
                       "returns information about current settings."),
                   [=](const CString& sLine) { Set(sLine); });
        AddCommand("Mechanism", t_d("[mechanism[ ...]]"),
                   t_d("Set the mechanisms to be attempted (in order)"),
                   [=](const CString& sLine) { SetMechanismCommand(sLine); });
        AddCommand("RequireAuth", t_d("[yes|no]"),
                   t_d("Don't connect unless SASL authentication succeeds"),
                   [=](const CString& sLine) { RequireAuthCommand(sLine); });
        AddCommand("Verbose", "yes|no", "Set verbosity level, useful to debug",
                   [&](const CString& sLine) {
                       m_bVerbose = sLine.Token(1, true).ToBool();
                       PutModule("Verbose: " + CString(m_bVerbose));
                   });

        m_bAuthenticated = false;
    }

    void PrintHelp(const CString& sLine) {
        HandleHelpCommand(sLine);

        CTable Mechanisms;
        Mechanisms.AddColumn(t_s("Mechanism"));
        Mechanisms.AddColumn(t_s("Description"));

        for (const auto& it : SupportedMechanisms) {
            Mechanisms.AddRow();
            Mechanisms.SetCell(t_s("Mechanism"), it.szName);
            Mechanisms.SetCell(t_s("Description"), it.sDescription.Resolve());
        }

        PutModule(t_s("The following mechanisms are available:"));
        PutModule(Mechanisms);
    }

    void Set(const CString& sLine) {
        if (sLine.Token(1).empty()) {
            CString sUsername = GetNV("username");
            CString sPassword = GetNV("password");

            if (sUsername.empty()) {
                PutModule(t_s("Username is currently not set"));
            } else {
                PutModule(t_f("Username is currently set to '{1}'")(sUsername));
            }
            if (sPassword.empty()) {
                PutModule(t_s("Password was not supplied"));
            } else {
                PutModule(t_s("Password was supplied"));
            }
            return;
        }

        SetNV("username", sLine.Token(1));
        SetNV("password", sLine.Token(2));

        PutModule(t_f("Username has been set to [{1}]")(GetNV("username")));
        PutModule(t_f("Password has been set to [{1}]")(GetNV("password")));
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

        PutModule(t_f("Current mechanisms set: {1}")(GetMechanismsString()));
    }

    void RequireAuthCommand(const CString& sLine) {
        if (!sLine.Token(1).empty()) {
            SetNV(NV_REQUIRE_AUTH, sLine.Token(1));
        }

        if (GetNV(NV_REQUIRE_AUTH).ToBool()) {
            PutModule(t_s("We require SASL negotiation to connect"));
        } else {
            PutModule(t_s("We will connect even if SASL fails"));
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

    void CheckRequireAuth() {
        if (!m_bAuthenticated && GetNV(NV_REQUIRE_AUTH).ToBool()) {
            GetNetwork()->SetIRCConnectEnabled(false);
            PutModule(t_s("Disabling network, we require authentication."));
            PutModule(t_s("Use 'RequireAuth no' to disable."));
        }
    }

    void Authenticate(const CString& sLine) {
        if (m_Mechanisms.GetCurrent().Equals("PLAIN") && sLine.Equals("+")) {
            CString sAuthLine = GetNV("username") + '\0' + GetNV("username") +
                                '\0' + GetNV("password");
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

    EModRet OnRawMessage(CMessage& msg) override {
        if (msg.GetCommand().Equals("AUTHENTICATE")) {
            Authenticate(msg.GetParam(0));
            return HALT;
        }
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (msg.GetCode() == 903) {
            /* SASL success! */
            if (m_bVerbose) {
                PutModule(
                    t_f("{1} mechanism succeeded.")(m_Mechanisms.GetCurrent()));
            }
            GetNetwork()->GetIRCSock()->ResumeCap();
            m_bAuthenticated = true;
            DEBUG("sasl: Authenticated with mechanism ["
                  << m_Mechanisms.GetCurrent() << "]");
        } else if (msg.GetCode() == 904 ||
                   msg.GetCode() == 905) {
            DEBUG("sasl: Mechanism [" << m_Mechanisms.GetCurrent()
                                      << "] failed.");
            if (m_bVerbose) {
                PutModule(
                    t_f("{1} mechanism failed.")(m_Mechanisms.GetCurrent()));
            }

            if (m_Mechanisms.HasNext()) {
                m_Mechanisms.IncrementIndex();
                PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
            } else {
                CheckRequireAuth();
                GetNetwork()->GetIRCSock()->ResumeCap();
            }
        } else if (msg.GetCode() == 906) {
            /* CAP wasn't paused? */
            DEBUG("sasl: Reached 906.");
            CheckRequireAuth();
        } else if (msg.GetCode() == 907) {
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

    void OnIRCDisconnected() override { m_bAuthenticated = false; }

    CString GetWebMenuTitle() override { return t_s("SASL"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
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
            Row["Description"] = it.sDescription.Resolve();
        }

        return true;
    }

  private:
    Mechanisms m_Mechanisms;
    bool m_bAuthenticated;
    bool m_bVerbose = false;
};

template <>
void TModInfo<CSASLMod>(CModInfo& Info) {
    Info.SetWikiPage("sasl");
}

NETWORKMODULEDEFS(CSASLMod, t_s("Adds support for sasl authentication "
                                "capability to authenticate to an IRC server"))
