/*
 * Copyright (C) 2004-2022 ZNC, see the NOTICE file for details.
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
    CString m_sNewNotifyOn;
    bool m_bNewOnly{};
    bool m_bOnDisconnect{};
    bool m_bNotifyOnNewIP{};
    bool m_bNotifyOnNewClientID{};

    set<CString> m_sClientsSeenIP;
    set<CString> m_sClientsSeenID;

    void SaveSettings() {
        SetNV("method", m_sMethod);
        SetNV("newonly", m_bNewOnly ? "1" : "0");
        SetNV("newnotifyon", m_sNewNotifyOn);
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
                   t_d("Turns notifications for unseen connections on or off"),
                   [=](const CString& sLine) { OnNewOnlyCommand(sLine); });
        AddCommand("NewNotifyOn", t_d("<ip|clientid|both>"),
                   t_d("Specifies whether you want to be notified about new connections with new IPs, new ClientIDs connecting or in bot cases"),
                   [=](const CString& sLine) { OnNewNotifyOn(sLine); });
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

        if (m_sNewNotifyOn != "ip" && m_sNewNotifyOn != "clientid" &&
            m_sNewNotifyOn != "both") {
            m_sNewNotifyOn = "ip";
        }

        // default = off for these:

        m_bNewOnly = (GetNV("newonly") == "1");
        m_bOnDisconnect = (GetNV("ondisconnect") == "1");

        return true;
    }

    void OnClientLogin() override {
        CString sRemoteIP = GetClient()->GetRemoteIP();
        CString sRemoteClientID = GetClient()->GetIdentifier();

        CString& sClientNameMessage = sRemoteIP;
        if (m_bNotifyOnNewClientID and sRemoteClientID != "") {
            sClientNameMessage = sRemoteClientID;
        }

        auto sendLoginNotification = [&]() {
            SendNotification(t_p("<This message is impossible for 1 client>",
                                 "Another client ({1}) authenticated as your user. "
                                 "Use the 'ListClients' command to see all {2} "
                                 "clients.",
                                 GetUser()->GetAllClients().size())(
                sClientNameMessage, GetUser()->GetAllClients().size()));
        };

        if (m_bNewOnly) {
            // see if we actually got a new client
            // TODO: replace setName.find(...) == setName.end() with !setName.contains() once ZNC uses C++20
            if ((m_bNotifyOnNewIP       && (m_sClientsSeenIP.find(sRemoteIP)       == m_sClientsSeenIP.end())) ||
                (m_bNotifyOnNewClientID && (m_sClientsSeenID.find(sRemoteClientID) == m_sClientsSeenID.end()))) {
                sendLoginNotification();
            }
        }
        else {
            sendLoginNotification();
        }

        // the set<> will automatically disregard duplicates:
        m_sClientsSeenIP.insert(sRemoteIP);
        m_sClientsSeenID.insert(sRemoteClientID);
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

    void OnNewNotifyOn(const CString& sCommand) {
        const CString sArg = sCommand.Token(1, true).AsLower();

        if (sArg != "ip" && sArg != "clientid" && sArg != "both") {
            PutModule(t_s("Usage: NewNotifyOn <ip|clientid|both>"));
            return;
        }

        if (sArg == "both") {
            m_bNotifyOnNewIP = true;
            m_bNotifyOnNewClientID = true;
        } else if (sArg == "ip") {
            m_bNotifyOnNewIP = true;
        } else if (sArg == "clientid") {
            m_bNotifyOnNewClientID = true;
        }

        m_sNewNotifyOn = sArg;
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
            t_f("Current settings: Method: {1}, for unseen only: "
                "{2}, unseen notify method: {3}, notify on disconnecting clients: {4}")(
                m_sMethod, m_bNewOnly, m_sNewNotifyOn, m_bOnDisconnect));
    }
};

template <>
void TModInfo<CClientNotifyMod>(CModInfo& Info) {
    Info.SetWikiPage("clientnotify");
}

USERMODULEDEFS(CClientNotifyMod,
               t_s("Notifies you when another IRC client logs into or out of "
                   "your account. Configurable."))
