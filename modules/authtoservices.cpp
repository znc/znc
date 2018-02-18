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

#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

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

class CAuthToServices : public CModule {
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
    MODCONSTRUCTOR(CAuthToServices) {
        AddCommand("Help", t_d("search"), t_d("Generate this output"),
                   [=](const CString& sLine) { PrintHelp(sLine); });
        AddCommand("SetAccount", t_d("account"),
                   t_d("Set your services account name"),
                   [=](const CString& sLine) { SetAccountCommand(sLine); });
        AddCommand("ClearAccount", "", t_d("Clear your services account name"),
                   [=](const CString& sLine) { ClearAccountCommand(sLine); });
        AddCommand("SetPassword", t_d("password"),
                   t_d("Set your services password"),
                   [=](const CString& sLine) { SetPasswordCommand(sLine); });
        AddCommand("ClearPassword", "", t_d("Clear your services password"),
                   [=](const CString& sLine) { ClearPasswordCommand(sLine); });
        AddCommand("SetNSCommand", t_d("cmd new-pattern"),
                   t_d("Set pattern for NickServ commands"),
                   [=](const CString& sLine) { SetNSCommandCommand(sLine); });
        AddCommand("ViewNSCommands", "",
                   t_d("Show patterns for NickServ commands"),
                   [=](const CString& sLine) { ViewNSCommandsCommand(sLine); });
        AddCommand("SetNSName", t_d("nickname"),
                   t_d("Set NickServ name (Useful on networks like EpiKnet, "
                       "where NickServ is named Themis"),
                   [=](const CString& sLine) { SetNSNameCommand(sLine); });
        AddCommand("ClearNSName", "",
                   t_d("Reset NickServ name to default (NickServ)"),
                   [=](const CString& sLine) { ClearNSNameCommand(sLine); });
        AddCommand("SaslMechanism", t_d("[mechanism[ ...]]"),
                   t_d("Set the SASL mechanisms to be attempted (in order)"),
                   [=](const CString& sLine) { SaslMechanismCommand(sLine); });
        AddCommand("RequireSasl", t_d("[yes|no]"),
                   t_d("Don't connect unless SASL authentication succeeds"),
                   [=](const CString& sLine) { RequireSaslCommand(sLine); });
        AddCommand("TrySasl", t_d("[yes|no]"),
                   t_d("Attempt SASL authentication on connection"),
                   [=](const CString& sLine) { TrySaslCommand(sLine); });
    }

    void SetAccountCommand(const CString& sLine) {
        if (sLine.Token(1, true).empty()) {
            PutModule(t_f("Username is currently set to '{1}'")(GetNV("Account")));
            return;
        }

        SetNV("Account", sLine.Token(1, true));
        PutModule(t_f("Username has been set to [{1}]")(GetNV("Account")));
    }

    void ClearAccountCommand(const CString& sLine) {
        DelNV("Account");
    }

    void SetPasswordCommand(const CString& sLine) {
        if (sLine.Token(1, true).empty()) {
            if (GetNV("Password").empty()) {
                PutModule(t_s("Password was not supplied"));
            } else {
                PutModule(t_s("Password was supplied"));
            }
            return;
        }

        SetNV("Password", sLine.Token(1, true));
    }

    void ClearPasswordCommand(const CString& sLine) {
        DelNV("Password");
    }

    void SetNSCommandCommand(const CString& sLine) {
        CString sCmd = sLine.Token(1);
        CString sNewCmd = sLine.Token(2, true);
        if (sCmd.Equals("IDENTIFY")) {
            SetNV("IdentifyCmd", sNewCmd);
        } else {
            PutModule(
                t_s("No such editable command. See ViewNSCommands for list."));
            return;
        }
        PutModule(t_s("Ok"));
    }

    void ViewNSCommandsCommand(const CString& sLine) {
        PutModule("IDENTIFY " + GetNV("IdentifyCmd"));
    }

    void SetNSNameCommand(const CString& sLine) {
        SetNV("NickServName", sLine.Token(1, true));
        PutModule(t_s("NickServ name set"));
    }

    void ClearNSNameCommand(const CString& sLine) {
        DelNV("NickServName");
    }

    void RequireSaslCommand(const CString& sLine) {
        if(!sLine.Token(1).empty()) {
            SetNV("RequireSASL", sLine.Token(1));
        }

        if(GetNV("RequireSASL").ToBool()) {
            PutModule(t_s("We require SASL negotiation to connect"));
        } else {
            PutModule(t_s("We will connect even if SASL fails"));
        }
    }

    void TrySaslCommand(const CString& sLine) {
        if(!sLine.Token(1).empty()) {
            SetNV("TrySASL", sLine.Token(1));
        }

        if(TrySasl()) {
            PutModule(t_s("We attempt SASL negotiation"));
        } else {
            PutModule(t_s("We will not attempt SASL negotiation"));
        }
    }

    void SaslMechanismCommand(const CString& sLine) {
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

            SetNV("SASLMechanisms", sMechanisms);
        }

        PutModule(t_f("Current mechanisms set: {1}")(GetMechanismsString()));
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
        if (GetNV("SASLMechanisms").empty()) {
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

        return GetNV("SASLMechanisms");
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

    ~CAuthToServices() override {}

    bool LoadOtherModuleNV(const CString &otherModule, MCString& result) {
        CString sPath = GetNetwork()->GetNetworkPath() + "/moddata/"
                        + otherModule + "/.registry";
        return result.ReadFromDisk(sPath) == MCString::MCS_SUCCESS;
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        if (!sArgs.empty() && sArgs != "<hidden>") {
            SetNV("Password", sArgs);
            SetArgs("<hidden>");
        }

        if(!GetNV("NotFirstLoad").ToBool()) {
            if (LoadOtherModuleNV("sasl", m_mssSaslRegistry)) {
                SetNV("RequireSASL", m_mssSaslRegistry["require_auth"]);
                SetNV("SASLMechanisms", m_mssSaslRegistry["mechanisms"]);
                SetNV("Account", m_mssSaslRegistry["username"]);
                SetNV("Password", m_mssSaslRegistry["password"]);

                PutModule(t_s("Migrated configuration from SASL module."));
            } else if (LoadOtherModuleNV("nickserv", m_mssNickservRegistry)) {
                SetNV("Password", m_mssNickservRegistry["Password"]);
                SetNV("IdentifyCmd", m_mssNickservRegistry["IdentifyCmd"]);
                SetNV("NickServName", m_mssSaslRegistry["NickServName"]);

                PutModule(t_s("Migrated configuration from NickServ module."));
            }
        }

        SetNV("NotFirstLoad", "true");

        if (GetNV("IdentifyCmd").empty()) {
            SetNV("IdentifyCmd", "NICKSERV IDENTIFY {account} {password}");
        }

        return true;
    }

    const CString GetAccount() {
        return (!GetNV("Account").empty()
                ? GetNV("Account")
                : GetNetwork()->GetIRCNick().GetNick());
    }

    // NickServ authentication

    void NickServAuthenticate() {
        MCString msValues;
        msValues["account"] = GetAccount();
        msValues["password"] = GetNV("Password");
        PutIRC(CString::NamedFormat(GetNV("IdentifyCmd"), msValues));
    }

    void HandleMessage(CNick& Nick, const CString& sMessage) {
        CString sNickServName = (!GetNV("NickServName").empty())
                                    ? GetNV("NickServName")
                                    : "NickServ";
        if (!GetNV("Password").empty() && Nick.NickEquals(sNickServName) &&
                (sMessage.find("msg") != CString::npos ||
                 sMessage.find("authenticate") != CString::npos ||
                 sMessage.find("choose a different nickname") != CString::npos ||
                 sMessage.find("please choose a different nick") != CString::npos ||
                 sMessage.find("If this is your nick, identify yourself with") !=
                     CString::npos ||
                 sMessage.find("If this is your nick, type") != CString::npos ||
                 sMessage.find("This is a registered nickname, please identify") !=
                     CString::npos ||
                 sMessage.find("is a registered nick - you must auth to account") !=
                     CString::npos ||
                 sMessage.StripControls_n().find(
                     "type /NickServ IDENTIFY password") != CString::npos ||
                 sMessage.StripControls_n().find(
                     "type /msg NickServ IDENTIFY password") != CString::npos) &&
                sMessage.AsUpper().find("IDENTIFY") != CString::npos &&
                sMessage.find("help") == CString::npos) {
            NickServAuthenticate();
        }
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }

    // SASL authentication

    void CheckRequireSasl() {
        if (!m_bAuthenticated && GetNV("RequireSASL").ToBool()) {
            GetNetwork()->SetIRCConnectEnabled(false);
            PutModule(t_s("Disabling network, we require authentication."));
            PutModule(t_s("Use 'RequireSasl no' to disable."));
        }
    }

    bool TrySasl() {
        return GetNV("TrySASL").ToBool() || GetNV("TrySASL").empty();
    }

    void SaslAuthenticate(const CString& sLine) {
        if (m_Mechanisms.GetCurrent().Equals("PLAIN") && sLine.Equals("+")) {
            CString sAuthLine = GetAccount() + '\0' + GetAccount() +
                                '\0' + GetNV("Password");
            sAuthLine.Base64Encode();
            PutIRC("AUTHENTICATE " + sAuthLine);
        } else {
            /* Send blank authenticate for other mechanisms (like EXTERNAL). */
            PutIRC("AUTHENTICATE +");
        }
    }

    bool OnServerCapAvailable(const CString& sCap) override {
        return TrySasl() && sCap.Equals("sasl");
    }

    void OnServerCapResult(const CString& sCap, bool bSuccess) override {
        if (sCap.Equals("sasl")) {
            if (TrySasl() && bSuccess) {
                GetMechanismsString().Split(" ", m_Mechanisms);

                if (m_Mechanisms.empty()) {
                    CheckRequireSasl();
                    return;
                }

                GetNetwork()->GetIRCSock()->PauseCap();

                m_Mechanisms.SetIndex(0);
                PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
            } else {
                CheckRequireSasl();
            }
        }
    }

    EModRet OnRawMessage(CMessage& msg) override {
        if (msg.GetCommand().Equals("AUTHENTICATE")) {
            if(TrySasl()) {
                SaslAuthenticate(msg.GetParam(0));
                return HALT;
            }
        }
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (msg.GetCode() == 903) {
            /* SASL success! */
            GetNetwork()->GetIRCSock()->ResumeCap();
            m_bAuthenticated = true;
            DEBUG("sasl: Authenticated with mechanism ["
                  << m_Mechanisms.GetCurrent() << "]");
        } else if (msg.GetCode() == 904 ||
                   msg.GetCode() == 905) {
            DEBUG("sasl: Mechanism [" << m_Mechanisms.GetCurrent()
                                      << "] failed.");

            if (m_Mechanisms.HasNext()) {
                m_Mechanisms.IncrementIndex();
                PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
            } else {
                CheckRequireSasl();
                GetNetwork()->GetIRCSock()->ResumeCap();
            }
        } else if (msg.GetCode() == 906) {
            /* CAP wasn't paused? */
            DEBUG("sasl: Reached 906.");
            CheckRequireSasl();
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
        /* Just in case something slipped through, perhaps the server doesn't
         * respond to our CAP negotiation. */

        CheckRequireSasl();
    }

    void OnIRCDisconnected() override { m_bAuthenticated = false; }

    CString GetWebMenuTitle() override { return t_s("Services authentication"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "index") {
            // only accept requests to index
            return false;
        }

        if (WebSock.IsPost()) {
            SetNV("Account", WebSock.GetParam("account"));

            CString sPassword = WebSock.GetParam("password");
            if (!sPassword.empty()) {
                SetNV("Password", sPassword);
            }

            SetNV("RequireSASL", WebSock.GetParam("require_sasl"));
            SetNV("TrySASL", WebSock.GetParam("try_sasl"));

            SetNV("SASLMechanisms", WebSock.GetParam("mechanisms"));
        }

        Tmpl["Account"] = GetNV("Account");
        Tmpl["Password"] = GetNV("Password");
        Tmpl["TrySasl"] = TrySasl() ? "true" : "false";
        Tmpl["RequireSasl"] = GetNV("RequireSASL");
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
    MCString m_mssSaslRegistry;
    MCString m_mssNickservRegistry;
    bool m_bAuthenticated;
};

template <>
void TModInfo<CAuthToServices>(CModInfo& Info) {
    Info.SetWikiPage("authtoservices");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s("Please enter your nickserv password."));
}

NETWORKMODULEDEFS(CAuthToServices,
                  t_s("Auths you with IRC services using SASL or NickServ"))
