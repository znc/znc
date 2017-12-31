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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

using std::vector;
using std::map;

class CSendRaw_Mod : public CModule {
    void SendClient(const CString& sLine) {
        CUser* pUser = CZNC::Get().FindUser(sLine.Token(1));

        if (pUser) {
            CIRCNetwork* pNetwork = pUser->FindNetwork(sLine.Token(2));

            if (pNetwork) {
                pNetwork->PutUser(sLine.Token(3, true));
                PutModule(t_f("Sent [{1}] to {2}/{3}")(sLine.Token(3, true),
                                                       pUser->GetUserName(),
                                                       pNetwork->GetName()));
            } else {
                PutModule(t_f("Network {1} not found for user {2}")(
                    sLine.Token(2), sLine.Token(1)));
            }
        } else {
            PutModule(t_f("User {1} not found")(sLine.Token(1)));
        }
    }

    void SendServer(const CString& sLine) {
        CUser* pUser = CZNC::Get().FindUser(sLine.Token(1));

        if (pUser) {
            CIRCNetwork* pNetwork = pUser->FindNetwork(sLine.Token(2));

            if (pNetwork) {
                pNetwork->PutIRC(sLine.Token(3, true));
                PutModule(t_f("Sent [{1}] to IRC server of {2}/{3}")(
                    sLine.Token(3, true), pUser->GetUserName(),
                    pNetwork->GetName()));
            } else {
                PutModule(t_f("Network {1} not found for user {2}")(
                    sLine.Token(2), sLine.Token(1)));
            }
        } else {
            PutModule(t_f("User {1} not found")(sLine.Token(1)));
        }
    }

    void CurrentClient(const CString& sLine) {
        CString sData = sLine.Token(1, true);
        GetClient()->PutClient(sData);
    }

  public:
    ~CSendRaw_Mod() override {}

    bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
        if (!GetUser()->IsAdmin()) {
            sErrorMsg =
                t_s("You must have admin privileges to load this module");
            return false;
        }

        return true;
    }

    CString GetWebMenuTitle() override { return t_s("Send Raw"); }
    bool WebRequiresAdmin() override { return true; }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName == "index") {
            if (WebSock.IsPost()) {
                CUser* pUser = CZNC::Get().FindUser(
                    WebSock.GetParam("network").Token(0, false, "/"));
                if (!pUser) {
                    WebSock.GetSession()->AddError(t_s("User not found"));
                    return true;
                }

                CIRCNetwork* pNetwork = pUser->FindNetwork(
                    WebSock.GetParam("network").Token(1, false, "/"));
                if (!pNetwork) {
                    WebSock.GetSession()->AddError(t_s("Network not found"));
                    return true;
                }

                bool bToServer = WebSock.GetParam("send_to") == "server";
                const CString sLine = WebSock.GetParam("line");

                Tmpl["user"] = pUser->GetUserName();
                Tmpl[bToServer ? "to_server" : "to_client"] = "true";
                Tmpl["line"] = sLine;

                if (bToServer) {
                    pNetwork->PutIRC(sLine);
                } else {
                    pNetwork->PutUser(sLine);
                }

                WebSock.GetSession()->AddSuccess(t_s("Line sent"));
            }

            const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
            for (const auto& it : msUsers) {
                CTemplate& l = Tmpl.AddRow("UserLoop");
                l["Username"] = it.second->GetUserName();

                vector<CIRCNetwork*> vNetworks = it.second->GetNetworks();
                for (const CIRCNetwork* pNetwork : vNetworks) {
                    CTemplate& NetworkLoop = l.AddRow("NetworkLoop");
                    NetworkLoop["Username"] = it.second->GetUserName();
                    NetworkLoop["Network"] = pNetwork->GetName();
                }
            }

            return true;
        }

        return false;
    }

    MODCONSTRUCTOR(CSendRaw_Mod) {
        AddHelpCommand();
        AddCommand("Client", t_d("[user] [network] [data to send]"),
                   t_d("The data will be sent to the user's IRC client(s)"),
                   [=](const CString& sLine) { SendClient(sLine); });
        AddCommand("Server", t_d("[user] [network] [data to send]"),
                   t_d("The data will be sent to the IRC server the user is "
                       "connected to"),
                   [=](const CString& sLine) { SendServer(sLine); });
        AddCommand("Current", t_d("[data to send]"),
                   t_d("The data will be sent to your current client"),
                   [=](const CString& sLine) { CurrentClient(sLine); });
    }
};

template <>
void TModInfo<CSendRaw_Mod>(CModInfo& Info) {
    Info.SetWikiPage("send_raw");
}

USERMODULEDEFS(CSendRaw_Mod,
               t_s("Lets you send some raw IRC lines as/to someone else"))
