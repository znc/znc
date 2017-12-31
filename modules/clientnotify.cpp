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

#include <znc/znc.h>
#include <znc/User.h>

using std::set;

class CClientNotifyMod : public CModule {
  protected:
    CString m_sMethod;
    bool m_bNewOnly{};
    bool m_bOnDisconnect{};

    set<CString> m_sClientsSeen;

    void SaveSettings() {
        SetNV("method", m_sMethod);
        SetNV("newonly", m_bNewOnly ? "1" : "0");
        SetNV("ondisconnect", m_bOnDisconnect ? "1" : "0");
    }

    void SendNotification(const CString& sMessage) {
        if (m_sMethod == "message") {
            GetUser()->PutStatus(sMessage, nullptr, GetClient());
        } else if (m_sMethod == "notice") {
            GetUser()->PutStatusNotice(sMessage, nullptr, GetClient());
        }
    }

  public:
    MODCONSTRUCTOR(CClientNotifyMod) {
        AddHelpCommand();
        AddCommand("Method", t_d("<message|notice|off>"),
                   t_d("Sets the notify method"),
                   [=](const CString& sLine) { OnMethodCommand(sLine); });
        AddCommand("NewOnly", t_d("<on|off>"),
                   t_d("Turns notifications for unseen IP addresses on or off"),
                   [=](const CString& sLine) { OnNewOnlyCommand(sLine); });
        AddCommand(
            "OnDisconnect", t_d("<on|off>"),
            t_d("Turns notifications for clients disconnecting on or off"),
            [=](const CString& sLine) { OnDisconnectCommand(sLine); });
        AddCommand("Show", "", t_d("Shows the current settings"),
                   [=](const CString& sLine) { OnShowCommand(sLine); });
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_sMethod = GetNV("method");

        if (m_sMethod != "notice" && m_sMethod != "message" &&
            m_sMethod != "off") {
            m_sMethod = "message";
        }

        // default = off for these:

        m_bNewOnly = (GetNV("newonly") == "1");
        m_bOnDisconnect = (GetNV("ondisconnect") == "1");

        return true;
    }

    void OnClientLogin() override {
        CString sRemoteIP = GetClient()->GetRemoteIP();
        if (!m_bNewOnly ||
            m_sClientsSeen.find(sRemoteIP) == m_sClientsSeen.end()) {
            SendNotification(t_p("<This message is impossible for 1 client>",
                                 "Another client authenticated as your user. "
                                 "Use the 'ListClients' command to see all {1} "
                                 "clients.",
                                 GetUser()->GetAllClients().size())(
                GetUser()->GetAllClients().size()));

            // the set<> will automatically disregard duplicates:
            m_sClientsSeen.insert(sRemoteIP);
        }
    }

    void OnClientDisconnect() override {
        if (m_bOnDisconnect) {
            SendNotification(t_p("<This message is impossible for 1 client>",
                                 "A client disconnected from your user. Use "
                                 "the 'ListClients' command to see the {1} "
                                 "remaining clients.",
                                 GetUser()->GetAllClients().size())(
                GetUser()->GetAllClients().size()));
        }
    }

    void OnMethodCommand(const CString& sCommand) {
        const CString sArg = sCommand.Token(1, true).AsLower();

        if (sArg != "notice" && sArg != "message" && sArg != "off") {
            PutModule(t_s("Usage: Method <message|notice|off>"));
            return;
        }

        m_sMethod = sArg;
        SaveSettings();
        PutModule(t_s("Saved."));
    }

    void OnNewOnlyCommand(const CString& sCommand) {
        const CString sArg = sCommand.Token(1, true).AsLower();

        if (sArg.empty()) {
            PutModule(t_s("Usage: NewOnly <on|off>"));
            return;
        }

        m_bNewOnly = sArg.ToBool();
        SaveSettings();
        PutModule(t_s("Saved."));
    }

    void OnDisconnectCommand(const CString& sCommand) {
        const CString sArg = sCommand.Token(1, true).AsLower();

        if (sArg.empty()) {
            PutModule(t_s("Usage: OnDisconnect <on|off>"));
            return;
        }

        m_bOnDisconnect = sArg.ToBool();
        SaveSettings();
        PutModule(t_s("Saved."));
    }

    void OnShowCommand(const CString& sLine) {
        PutModule(
            t_f("Current settings: Method: {1}, for unseen IP addresses only: "
                "{2}, notify on disconnecting clients: {3}")(
                m_sMethod, m_bNewOnly, m_bOnDisconnect));
    }
};

template <>
void TModInfo<CClientNotifyMod>(CModInfo& Info) {
    Info.SetWikiPage("clientnotify");
}

USERMODULEDEFS(CClientNotifyMod,
               t_s("Notifies you when another IRC client logs into or out of "
                   "your account. Configurable."))
