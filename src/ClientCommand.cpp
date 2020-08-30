/*
 * Copyright (C) 2004-2020 ZNC, see the NOTICE file for details.
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

#include <znc/Chan.h>
#include <znc/Client.h>
#include <znc/FileUtils.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Query.h>
#include <znc/Server.h>
#include <znc/User.h>

using std::map;
using std::set;
using std::vector;

void CClient::UserCommand(CString& sLine) {
    if (!m_pUser) {
        return;
    }

    if (sLine.empty()) {
        return;
    }

    bool bReturn = false;
    NETWORKMODULECALL(OnStatusCommand(sLine), m_pUser, m_pNetwork, this,
                      &bReturn);
    if (bReturn) return;

    const CString sCommand = sLine.Token(0);

    if (sCommand.Equals("HELP")) {
        HelpUser(sLine.Token(1));
    } else if (sCommand.Equals("LISTNICKS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sChan = sLine.Token(1);

        if (sChan.empty()) {
            PutStatus(t_s("Usage: ListNicks <#chan>"));
            return;
        }

        CChan* pChan = m_pNetwork->FindChan(sChan);

        if (!pChan) {
            PutStatus(t_f("You are not on [{1}]")(sChan));
            return;
        }

        if (!pChan->IsOn()) {
            PutStatus(t_f("You are not on [{1}] (trying)")(sChan));
            return;
        }

        const map<CString, CNick>& msNicks = pChan->GetNicks();
        CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();
        const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

        if (msNicks.empty()) {
            PutStatus(t_f("No nicks on [{1}]")(sChan));
            return;
        }

        CTable Table;

        for (unsigned int p = 0; p < sPerms.size(); p++) {
            CString sPerm;
            sPerm += sPerms[p];
            Table.AddColumn(sPerm);
        }

        Table.AddColumn(t_s("Nick"));
        Table.AddColumn(t_s("Ident"));
        Table.AddColumn(t_s("Host"));

        for (const auto& it : msNicks) {
            Table.AddRow();

            for (unsigned int b = 0; b < sPerms.size(); b++) {
                if (it.second.HasPerm(sPerms[b])) {
                    CString sPerm;
                    sPerm += sPerms[b];
                    Table.SetCell(sPerm, sPerm);
                }
            }

            Table.SetCell(t_s("Nick"), it.second.GetNick());
            Table.SetCell(t_s("Ident"), it.second.GetIdent());
            Table.SetCell(t_s("Host"), it.second.GetHost());
        }

        PutStatus(Table);
    } else if (sCommand.Equals("ATTACH")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sPatterns = sLine.Token(1, true);

        if (sPatterns.empty()) {
            PutStatus(t_s("Usage: Attach <#chans>"));
            return;
        }

        set<CChan*> sChans = MatchChans(sPatterns);
        unsigned int uAttachedChans = AttachChans(sChans);

        PutStatus(t_p("There was {1} channel matching [{2}]",
                      "There were {1} channels matching [{2}]",
                      sChans.size())(sChans.size(), sPatterns));
        PutStatus(t_p("Attached {1} channel", "Attached {1} channels",
                      uAttachedChans)(uAttachedChans));
    } else if (sCommand.Equals("DETACH")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sPatterns = sLine.Token(1, true);

        if (sPatterns.empty()) {
            PutStatus(t_s("Usage: Detach <#chans>"));
            return;
        }

        set<CChan*> sChans = MatchChans(sPatterns);
        unsigned int uDetached = DetachChans(sChans);

        PutStatus(t_p("There was {1} channel matching [{2}]",
                      "There were {1} channels matching [{2}]",
                      sChans.size())(sChans.size(), sPatterns));
        PutStatus(t_p("Detached {1} channel", "Detached {1} channels",
                      uDetached)(uDetached));
    } else if (sCommand.Equals("VERSION")) {
        PutStatus(CZNC::GetTag());
        PutStatus(CZNC::GetCompileOptionsString());
    } else if (sCommand.Equals("MOTD") || sCommand.Equals("ShowMOTD")) {
        if (!SendMotd()) {
            PutStatus(t_s("There is no MOTD set."));
        }
    } else if (m_pUser->IsAdmin() && sCommand.Equals("Rehash")) {
        CString sRet;

        if (CZNC::Get().RehashConfig(sRet)) {
            PutStatus(t_s("Rehashing succeeded!"));
        } else {
            PutStatus(t_f("Rehashing failed: {1}")(sRet));
        }
    } else if (m_pUser->IsAdmin() && sCommand.Equals("SaveConfig")) {
        if (CZNC::Get().WriteConfig()) {
            PutStatus(t_f("Wrote config to {1}")(CZNC::Get().GetConfigFile()));
        } else {
            PutStatus(t_s("Error while trying to write config."));
        }
    } else if (sCommand.Equals("LISTCLIENTS")) {
        CUser* pUser = m_pUser;
        CString sNick = sLine.Token(1);

        if (!sNick.empty()) {
            if (!m_pUser->IsAdmin()) {
                PutStatus(t_s("Usage: ListClients"));
                return;
            }

            pUser = CZNC::Get().FindUser(sNick);

            if (!pUser) {
                PutStatus(t_f("No such user: {1}")(sNick));
                return;
            }
        }

        vector<CClient*> vClients = pUser->GetAllClients();

        if (vClients.empty()) {
            PutStatus(t_s("No clients are connected"));
            return;
        }

        CTable Table;
        Table.AddColumn(t_s("Host", "listclientscmd"));
        Table.AddColumn(t_s("Network", "listclientscmd"));
        Table.AddColumn(t_s("Identifier", "listclientscmd"));

        for (const CClient* pClient : vClients) {
            Table.AddRow();
            Table.SetCell(t_s("Host", "listclientscmd"),
                          pClient->GetRemoteIP());
            if (pClient->GetNetwork()) {
                Table.SetCell(t_s("Network", "listclientscmd"),
                              pClient->GetNetwork()->GetName());
            }
            Table.SetCell(t_s("Identifier", "listclientscmd"),
                          pClient->GetIdentifier());
        }

        PutStatus(Table);
    } else if (m_pUser->IsAdmin() && sCommand.Equals("LISTUSERS")) {
        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
        CTable Table;
        Table.AddColumn(t_s("Username", "listuserscmd"));
        Table.AddColumn(t_s("Networks", "listuserscmd"));
        Table.AddColumn(t_s("Clients", "listuserscmd"));

        for (const auto& it : msUsers) {
            Table.AddRow();
            Table.SetCell(t_s("Username", "listuserscmd"), it.first);
            Table.SetCell(t_s("Networks", "listuserscmd"),
                          CString(it.second->GetNetworks().size()));
            Table.SetCell(t_s("Clients", "listuserscmd"),
                          CString(it.second->GetAllClients().size()));
        }

        PutStatus(Table);
    } else if (m_pUser->IsAdmin() && sCommand.Equals("LISTALLUSERNETWORKS")) {
        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
        CTable Table;
        Table.AddColumn(t_s("Username", "listallusernetworkscmd"));
        Table.AddColumn(t_s("Network", "listallusernetworkscmd"));
        Table.AddColumn(t_s("Clients", "listallusernetworkscmd"));
        Table.AddColumn(t_s("On IRC", "listallusernetworkscmd"));
        Table.AddColumn(t_s("IRC Server", "listallusernetworkscmd"));
        Table.AddColumn(t_s("IRC User", "listallusernetworkscmd"));
        Table.AddColumn(t_s("Channels", "listallusernetworkscmd"));

        for (const auto& it : msUsers) {
            Table.AddRow();
            Table.SetCell(t_s("Username", "listallusernetworkscmd"), it.first);
            Table.SetCell(t_s("Network", "listallusernetworkscmd"), t_s("N/A"));
            Table.SetCell(t_s("Clients", "listallusernetworkscmd"),
                          CString(it.second->GetUserClients().size()));

            const vector<CIRCNetwork*>& vNetworks = it.second->GetNetworks();

            for (const CIRCNetwork* pNetwork : vNetworks) {
                Table.AddRow();
                if (pNetwork == vNetworks.back()) {
                    Table.SetCell(t_s("Username", "listallusernetworkscmd"),
                                  "`-");
                } else {
                    Table.SetCell(t_s("Username", "listallusernetworkscmd"),
                                  "|-");
                }
                Table.SetCell(t_s("Network", "listallusernetworkscmd"),
                              pNetwork->GetName());
                Table.SetCell(t_s("Clients", "listallusernetworkscmd"),
                              CString(pNetwork->GetClients().size()));
                if (pNetwork->IsIRCConnected()) {
                    Table.SetCell(t_s("On IRC", "listallusernetworkscmd"),
                                  t_s("Yes", "listallusernetworkscmd"));
                    Table.SetCell(t_s("IRC Server", "listallusernetworkscmd"),
                                  pNetwork->GetIRCServer());
                    Table.SetCell(t_s("IRC User", "listallusernetworkscmd"),
                                  pNetwork->GetIRCNick().GetNickMask());
                    Table.SetCell(t_s("Channels", "listallusernetworkscmd"),
                                  CString(pNetwork->GetChans().size()));
                } else {
                    Table.SetCell(t_s("On IRC", "listallusernetworkscmd"),
                                  t_s("No", "listallusernetworkscmd"));
                }
            }
        }

        PutStatus(Table);
    } else if (m_pUser->IsAdmin() && sCommand.Equals("SetMOTD")) {
        CString sMessage = sLine.Token(1, true);

        if (sMessage.empty()) {
            PutStatus(t_s("Usage: SetMOTD <message>"));
        } else {
            CZNC::Get().SetMotd(sMessage);
            PutStatus(t_f("MOTD set to: {1}")(sMessage));
        }
    } else if (m_pUser->IsAdmin() && sCommand.Equals("AddMOTD")) {
        CString sMessage = sLine.Token(1, true);

        if (sMessage.empty()) {
            PutStatus(t_s("Usage: AddMOTD <message>"));
        } else {
            CZNC::Get().AddMotd(sMessage);
            PutStatus(t_f("Added [{1}] to MOTD")(sMessage));
        }
    } else if (m_pUser->IsAdmin() && sCommand.Equals("ClearMOTD")) {
        CZNC::Get().ClearMotd();
        PutStatus(t_s("Cleared MOTD"));
    } else if (m_pUser->IsAdmin() && sCommand.Equals("BROADCAST")) {
        CZNC::Get().Broadcast(sLine.Token(1, true));
    } else if (m_pUser->IsAdmin() &&
               (sCommand.Equals("SHUTDOWN") || sCommand.Equals("RESTART"))) {
        bool bRestart = sCommand.Equals("RESTART");
        CString sMessage = sLine.Token(1, true);
        bool bForce = false;

        if (sMessage.Token(0).Equals("FORCE")) {
            bForce = true;
            sMessage = sMessage.Token(1, true);
        }

        if (sMessage.empty()) {
            // No t_s here because language of user can be different
            sMessage = (bRestart ? "ZNC is being restarted NOW!"
                                 : "ZNC is being shut down NOW!");
        }

        if (!CZNC::Get().WriteConfig() && !bForce) {
            PutStatus(
                t_f("ERROR: Writing config file to disk failed! Aborting. Use "
                    "{1} FORCE to ignore.")(sCommand.AsUpper()));
        } else {
            CZNC::Get().Broadcast(sMessage);
            throw CException(bRestart ? CException::EX_Restart
                                      : CException::EX_Shutdown);
        }
    } else if (sCommand.Equals("JUMP") || sCommand.Equals("CONNECT")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        if (!m_pNetwork->HasServers()) {
            PutStatus(t_s("You don't have any servers added."));
            return;
        }

        CString sArgs = sLine.Token(1, true);
        sArgs.Trim();
        CServer* pServer = nullptr;

        if (!sArgs.empty()) {
            pServer = m_pNetwork->FindServer(sArgs);
            if (!pServer) {
                PutStatus(t_f("Server [{1}] not found")(sArgs));
                return;
            }
            m_pNetwork->SetNextServer(pServer);

            // If we are already connecting to some server,
            // we have to abort that attempt
            Csock* pIRCSock = GetIRCSock();
            if (pIRCSock && !pIRCSock->IsConnected()) {
                pIRCSock->Close();
            }
        }

        if (!pServer) {
            pServer = m_pNetwork->GetNextServer(false);
        }

        if (GetIRCSock()) {
            GetIRCSock()->Quit();
            if (pServer)
                PutStatus(t_f("Connecting to {1}...")(pServer->GetName()));
            else
                PutStatus(t_s("Jumping to the next server in the list..."));
        } else {
            if (pServer)
                PutStatus(t_f("Connecting to {1}...")(pServer->GetName()));
            else
                PutStatus(t_s("Connecting..."));
        }

        m_pNetwork->SetIRCConnectEnabled(true);
        return;
    } else if (sCommand.Equals("DISCONNECT")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        if (GetIRCSock()) {
            CString sQuitMsg = sLine.Token(1, true);
            GetIRCSock()->Quit(sQuitMsg);
        }

        m_pNetwork->SetIRCConnectEnabled(false);
        PutStatus(t_s("Disconnected from IRC. Use 'connect' to reconnect."));
        return;
    } else if (sCommand.Equals("ENABLECHAN")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sPatterns = sLine.Token(1, true);

        if (sPatterns.empty()) {
            PutStatus(t_s("Usage: EnableChan <#chans>"));
        } else {
            set<CChan*> sChans = MatchChans(sPatterns);

            unsigned int uEnabled = 0;
            for (CChan* pChan : sChans) {
                if (!pChan->IsDisabled()) continue;
                uEnabled++;
                pChan->Enable();
            }

            PutStatus(t_p("There was {1} channel matching [{2}]",
                          "There were {1} channels matching [{2}]",
                          sChans.size())(sChans.size(), sPatterns));
            PutStatus(t_p("Enabled {1} channel", "Enabled {1} channels",
                          uEnabled)(uEnabled));
        }
    } else if (sCommand.Equals("DISABLECHAN")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sPatterns = sLine.Token(1, true);

        if (sPatterns.empty()) {
            PutStatus(t_s("Usage: DisableChan <#chans>"));
        } else {
            set<CChan*> sChans = MatchChans(sPatterns);

            unsigned int uDisabled = 0;
            for (CChan* pChan : sChans) {
                if (pChan->IsDisabled()) continue;
                uDisabled++;
                pChan->Disable();
            }

            PutStatus(t_p("There was {1} channel matching [{2}]",
                          "There were {1} channels matching [{2}]",
                          sChans.size())(sChans.size(), sPatterns));
            PutStatus(t_p("Disabled {1} channel", "Disabled {1} channels",
                          uDisabled)(uDisabled));
        }
    } else if (sCommand.Equals("LISTCHANS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CIRCNetwork* pNetwork = m_pNetwork;

        const CString sUser = sLine.Token(1);
        const CString sNetwork = sLine.Token(2);

        if (!sUser.empty()) {
            if (!m_pUser->IsAdmin()) {
                PutStatus(t_s("Usage: ListChans"));
                return;
            }

            CUser* pUser = CZNC::Get().FindUser(sUser);

            if (!pUser) {
                PutStatus(t_f("No such user [{1}]")(sUser));
                return;
            }

            pNetwork = pUser->FindNetwork(sNetwork);
            if (!pNetwork) {
                PutStatus(t_f("User [{1}] doesn't have network [{2}]")(
                    sUser, sNetwork));
                return;
            }
        }

        const vector<CChan*>& vChans = pNetwork->GetChans();
        CIRCSock* pIRCSock = pNetwork->GetIRCSock();
        CString sPerms = pIRCSock ? pIRCSock->GetPerms() : "";

        if (vChans.empty()) {
            PutStatus(t_s("There are no channels defined."));
            return;
        }

        CTable Table;
        Table.AddColumn(t_s("Index", "listchans"));
        Table.AddColumn(t_s("Name", "listchans"));
        Table.AddColumn(t_s("Status", "listchans"));
        Table.AddColumn(t_s("In config", "listchans"));
        Table.AddColumn(t_s("Buffer", "listchans"));
        Table.AddColumn(t_s("Clear", "listchans"));
        Table.AddColumn(t_s("Modes", "listchans"));
        Table.AddColumn(t_s("Users", "listchans"));

        for (char cPerm : sPerms) {
            Table.AddColumn(CString(cPerm));
        }

        unsigned int uNumDetached = 0, uNumDisabled = 0, uNumJoined = 0,
                     uChanIndex = 1;

        for (const CChan* pChan : vChans) {
            Table.AddRow();
            Table.SetCell(t_s("Index", "listchans"), CString(uChanIndex));
            Table.SetCell(t_s("Name", "listchans"),
                          pChan->GetPermStr() + pChan->GetName());
            Table.SetCell(
                t_s("Status", "listchans"),
                pChan->IsOn()
                    ? (pChan->IsDetached() ? t_s("Detached", "listchans")
                                           : t_s("Joined", "listchans"))
                    : (pChan->IsDisabled() ? t_s("Disabled", "listchans")
                                           : t_s("Trying", "listchans")));
            Table.SetCell(
                t_s("In config", "listchans"),
                CString(pChan->InConfig() ? t_s("yes", "listchans") : ""));
            Table.SetCell(t_s("Buffer", "listchans"),
                          CString(pChan->HasBufferCountSet() ? "*" : "") +
                              CString(pChan->GetBufferCount()));
            Table.SetCell(
                t_s("Clear", "listchans"),
                CString(pChan->HasAutoClearChanBufferSet() ? "*" : "") +
                    CString(pChan->AutoClearChanBuffer()
                                ? t_s("yes", "listchans")
                                : ""));
            Table.SetCell(t_s("Modes", "listchans"), pChan->GetModeString());
            Table.SetCell(t_s("Users", "listchans"),
                          CString(pChan->GetNickCount()));

            std::map<char, unsigned int> mPerms = pChan->GetPermCounts();
            for (char cPerm : sPerms) {
                Table.SetCell(CString(cPerm), CString(mPerms[cPerm]));
            }

            if (pChan->IsDetached()) uNumDetached++;
            if (pChan->IsOn()) uNumJoined++;
            if (pChan->IsDisabled()) uNumDisabled++;

            uChanIndex++;
        }

        PutStatus(Table);
        PutStatus(t_f("Total: {1}, Joined: {2}, Detached: {3}, Disabled: {4}")(
            vChans.size(), uNumJoined, uNumDetached, uNumDisabled));
    } else if (sCommand.Equals("ADDNETWORK")) {
        if (!m_pUser->IsAdmin() && !m_pUser->HasSpaceForNewNetwork()) {
            PutStatus(t_s(
                "Network number limit reached. Ask an admin to increase the "
                "limit for you, or delete unneeded networks using /znc "
                "DelNetwork <name>"));
            return;
        }

        CString sNetwork = sLine.Token(1);

        if (sNetwork.empty()) {
            PutStatus(t_s("Usage: AddNetwork <name>"));
            return;
        }
        if (!CIRCNetwork::IsValidNetwork(sNetwork)) {
            PutStatus(t_s("Network name should be alphanumeric"));
            return;
        }

        CString sNetworkAddError;
        if (m_pUser->AddNetwork(sNetwork, sNetworkAddError)) {
            PutStatus(
                t_f("Network added. Use /znc JumpNetwork {1}, or connect to "
                    "ZNC with username {2} (instead of just {3}) to connect to "
                    "it.")(sNetwork, m_pUser->GetUsername() + "/" + sNetwork,
                           m_pUser->GetUsername()));
        } else {
            PutStatus(t_s("Unable to add that network"));
            PutStatus(sNetworkAddError);
        }
    } else if (sCommand.Equals("DELNETWORK")) {
        CString sNetwork = sLine.Token(1);

        if (sNetwork.empty()) {
            PutStatus(t_s("Usage: DelNetwork <name>"));
            return;
        }

        if (m_pNetwork && m_pNetwork->GetName().Equals(sNetwork)) {
            SetNetwork(nullptr);
        }

        if (m_pUser->DeleteNetwork(sNetwork)) {
            PutStatus(t_s("Network deleted"));
        } else {
            PutStatus(
                t_s("Failed to delete network, perhaps this network doesn't "
                    "exist"));
        }
    } else if (sCommand.Equals("LISTNETWORKS")) {
        CUser* pUser = m_pUser;

        if (m_pUser->IsAdmin() && !sLine.Token(1).empty()) {
            pUser = CZNC::Get().FindUser(sLine.Token(1));

            if (!pUser) {
                PutStatus(t_f("User {1} not found")(sLine.Token(1)));
                return;
            }
        }

        const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();

        CTable Table;
        Table.AddColumn(t_s("Network", "listnetworks"));
        Table.AddColumn(t_s("On IRC", "listnetworks"));
        Table.AddColumn(t_s("IRC Server", "listnetworks"));
        Table.AddColumn(t_s("IRC User", "listnetworks"));
        Table.AddColumn(t_s("Channels", "listnetworks"));

        for (const CIRCNetwork* pNetwork : vNetworks) {
            Table.AddRow();
            Table.SetCell(t_s("Network", "listnetworks"), pNetwork->GetName());
            if (pNetwork->IsIRCConnected()) {
                Table.SetCell(t_s("On IRC", "listnetworks"),
                              t_s("Yes", "listnetworks"));
                Table.SetCell(t_s("IRC Server", "listnetworks"),
                              pNetwork->GetIRCServer());
                Table.SetCell(t_s("IRC User", "listnetworks"),
                              pNetwork->GetIRCNick().GetNickMask());
                Table.SetCell(t_s("Channels", "listnetworks"),
                              CString(pNetwork->GetChans().size()));
            } else {
                Table.SetCell(t_s("On IRC", "listnetworks"),
                              t_s("No", "listnetworks"));
            }
        }

        if (PutStatus(Table) == 0) {
            PutStatus(t_s("No networks", "listnetworks"));
        }
    } else if (sCommand.Equals("MOVENETWORK")) {
        if (!m_pUser->IsAdmin()) {
            PutStatus(t_s("Access denied."));
            return;
        }

        CString sOldUser = sLine.Token(1);
        CString sOldNetwork = sLine.Token(2);
        CString sNewUser = sLine.Token(3);
        CString sNewNetwork = sLine.Token(4);

        if (sOldUser.empty() || sOldNetwork.empty() || sNewUser.empty()) {
            PutStatus(t_s(
                "Usage: MoveNetwork <old user> <old network> <new user> [new "
                "network]"));
            return;
        }
        if (sNewNetwork.empty()) {
            sNewNetwork = sOldNetwork;
        }

        CUser* pOldUser = CZNC::Get().FindUser(sOldUser);
        if (!pOldUser) {
            PutStatus(t_f("Old user {1} not found.")(sOldUser));
            return;
        }

        CIRCNetwork* pOldNetwork = pOldUser->FindNetwork(sOldNetwork);
        if (!pOldNetwork) {
            PutStatus(t_f("Old network {1} not found.")(sOldNetwork));
            return;
        }

        CUser* pNewUser = CZNC::Get().FindUser(sNewUser);
        if (!pNewUser) {
            PutStatus(t_f("New user {1} not found.")(sNewUser));
            return;
        }

        if (pNewUser->FindNetwork(sNewNetwork)) {
            PutStatus(t_f("User {1} already has network {2}.")(sNewUser,
                                                               sNewNetwork));
            return;
        }

        if (!CIRCNetwork::IsValidNetwork(sNewNetwork)) {
            PutStatus(t_f("Invalid network name [{1}]")(sNewNetwork));
            return;
        }

        const CModules& vMods = pOldNetwork->GetModules();
        for (CModule* pMod : vMods) {
            CString sOldModPath = pOldNetwork->GetNetworkPath() + "/moddata/" +
                                  pMod->GetModName();
            CString sNewModPath = pNewUser->GetUserPath() + "/networks/" +
                                  sNewNetwork + "/moddata/" +
                                  pMod->GetModName();

            CDir oldDir(sOldModPath);
            for (CFile* pFile : oldDir) {
                if (pFile->GetShortName() != ".registry") {
                    PutStatus(
                        t_f("Some files seem to be in {1}. You might want to "
                            "move them to {2}")(sOldModPath, sNewModPath));
                    break;
                }
            }

            pMod->MoveRegistry(sNewModPath);
        }

        CString sNetworkAddError;
        CIRCNetwork* pNewNetwork =
            pNewUser->AddNetwork(sNewNetwork, sNetworkAddError);

        if (!pNewNetwork) {
            PutStatus(t_f("Error adding network: {1}")(sNetworkAddError));
            return;
        }

        pNewNetwork->Clone(*pOldNetwork, false);

        if (m_pNetwork && m_pNetwork->GetName().Equals(sOldNetwork) &&
            m_pUser == pOldUser) {
            SetNetwork(nullptr);
        }

        if (pOldUser->DeleteNetwork(sOldNetwork)) {
            PutStatus(t_s("Success."));
        } else {
            PutStatus(
                t_s("Copied the network to new user, but failed to delete old "
                    "network"));
        }
    } else if (sCommand.Equals("JUMPNETWORK")) {
        CString sNetwork = sLine.Token(1);

        if (sNetwork.empty()) {
            PutStatus(t_s("No network supplied."));
            return;
        }

        if (m_pNetwork && (m_pNetwork->GetName() == sNetwork)) {
            PutStatus(t_s("You are already connected with this network."));
            return;
        }

        CIRCNetwork* pNetwork = m_pUser->FindNetwork(sNetwork);
        if (pNetwork) {
            PutStatus(t_f("Switched to {1}")(sNetwork));
            SetNetwork(pNetwork);
        } else {
            PutStatus(t_f("You don't have a network named {1}")(sNetwork));
        }
    } else if (sCommand.Equals("ADDSERVER")) {
        CString sServer = sLine.Token(1);

        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        if (sServer.empty()) {
            PutStatus(t_s("Usage: AddServer <host> [[+]port] [pass]"));
            return;
        }

        if (m_pNetwork->AddServer(sLine.Token(1, true))) {
            PutStatus(t_s("Server added"));
        } else {
            PutStatus(
                t_s("Unable to add that server. Perhaps the server is already "
                    "added or openssl is disabled?"));
        }
    } else if (sCommand.Equals("REMSERVER") || sCommand.Equals("DELSERVER")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sServer = sLine.Token(1);
        unsigned short uPort = sLine.Token(2).ToUShort();
        CString sPass = sLine.Token(3);

        if (sServer.empty()) {
            PutStatus(t_s("Usage: DelServer <host> [port] [pass]"));
            return;
        }

        if (!m_pNetwork->HasServers()) {
            PutStatus(t_s("You don't have any servers added."));
            return;
        }

        if (m_pNetwork->DelServer(sServer, uPort, sPass)) {
            PutStatus(t_s("Server removed"));
        } else {
            PutStatus(t_s("No such server"));
        }
    } else if (sCommand.Equals("LISTSERVERS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        if (m_pNetwork->HasServers()) {
            const vector<CServer*>& vServers = m_pNetwork->GetServers();
            CServer* pCurServ = m_pNetwork->GetCurrentServer();
            CTable Table;
            Table.AddColumn(t_s("Host", "listservers"));
            Table.AddColumn(t_s("Port", "listservers"));
            Table.AddColumn(t_s("SSL", "listservers"));
            Table.AddColumn(t_s("Password", "listservers"));

            for (const CServer* pServer : vServers) {
                Table.AddRow();
                Table.SetCell(
                    t_s("Host", "listservers"),
                    pServer->GetName() + (pServer == pCurServ ? "*" : ""));
                Table.SetCell(t_s("Port", "listservers"),
                              CString(pServer->GetPort()));
                Table.SetCell(
                    t_s("SSL", "listservers"),
                    (pServer->IsSSL()) ? t_s("SSL", "listservers|cell") : "");
                Table.SetCell(t_s("Password", "listservers"),
                              pServer->GetPass().empty() ? "" : "******");
            }

            PutStatus(Table);
        } else {
            PutStatus(t_s("You don't have any servers added."));
        }
    } else if (sCommand.Equals("AddTrustedServerFingerprint")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }
        CString sFP = sLine.Token(1);
        if (sFP.empty()) {
            PutStatus(t_s("Usage: AddTrustedServerFingerprint <fi:ng:er>"));
            return;
        }
        m_pNetwork->AddTrustedFingerprint(sFP);
        PutStatus(t_s("Done."));
    } else if (sCommand.Equals("DelTrustedServerFingerprint")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }
        CString sFP = sLine.Token(1);
        if (sFP.empty()) {
            PutStatus(t_s("Usage: DelTrustedServerFingerprint <fi:ng:er>"));
            return;
        }
        m_pNetwork->DelTrustedFingerprint(sFP);
        PutStatus(t_s("Done."));
    } else if (sCommand.Equals("ListTrustedServerFingerprints")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }
        const SCString& ssFPs = m_pNetwork->GetTrustedFingerprints();
        if (ssFPs.empty()) {
            PutStatus(t_s("No fingerprints added."));
        } else {
            int k = 0;
            for (const CString& sFP : ssFPs) {
                PutStatus(CString(++k) + ". " + sFP);
            }
        }
    } else if (sCommand.Equals("TOPICS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        const vector<CChan*>& vChans = m_pNetwork->GetChans();
        CTable Table;
        Table.AddColumn(t_s("Channel", "topicscmd"));
        Table.AddColumn(t_s("Set By", "topicscmd"));
        Table.AddColumn(t_s("Topic", "topicscmd"));

        for (const CChan* pChan : vChans) {
            Table.AddRow();
            Table.SetCell(t_s("Channel", "topicscmd"), pChan->GetName());
            Table.SetCell(t_s("Set By", "topicscmd"), pChan->GetTopicOwner());
            Table.SetCell(t_s("Topic", "topicscmd"), pChan->GetTopic());
        }

        PutStatus(Table);
    } else if (sCommand.Equals("LISTMODS") || sCommand.Equals("LISTMODULES")) {
        const auto PrintModules = [this](const CModules& Modules) {
            CTable Table;
            Table.AddColumn(t_s("Name", "listmods"));
            Table.AddColumn(t_s("Arguments", "listmods"));
            Table.SetStyle(CTable::ListStyle);
            for (const CModule* pMod : Modules) {
                Table.AddRow();
                Table.SetCell(t_s("Name", "listmods"), pMod->GetModName());
                Table.SetCell(t_s("Arguments", "listmods"), pMod->GetArgs());
            }
            PutStatus(Table);
        };
        if (m_pUser->IsAdmin()) {
            const CModules& GModules = CZNC::Get().GetModules();

            PutStatus("");
            if (!GModules.size()) {
                PutStatus(t_s("No global modules loaded."));
            } else {
                PutStatus(t_s("Global modules:"));
                PrintModules(GModules);
            }
        }

        const CModules& UModules = m_pUser->GetModules();

        PutStatus("");
        if (!UModules.size()) {
            PutStatus(t_s("Your user has no modules loaded."));
        } else {
            PutStatus(t_s("User modules:"));
            PrintModules(UModules);
        }

        if (m_pNetwork) {
            const CModules& NetworkModules = m_pNetwork->GetModules();
            PutStatus("");
            if (NetworkModules.empty()) {
                PutStatus(t_s("This network has no modules loaded."));
            } else {
                PutStatus(t_s("Network modules:"));
                PrintModules(NetworkModules);
            }
        }

        return;
    } else if (sCommand.Equals("LISTAVAILMODS") ||
               sCommand.Equals("LISTAVAILABLEMODULES")) {
        if (m_pUser->DenyLoadMod()) {
            PutStatus(t_s("Access denied."));
            return;
        }

        const auto PrintModules = [this](const set<CModInfo>& ssModules) {
            CTable Table;
            Table.AddColumn(t_s("Name", "listavailmods"));
            Table.AddColumn(t_s("Description", "listavailmods"));
            Table.SetStyle(CTable::ListStyle);

            for (const CModInfo& Info : ssModules) {
                Table.AddRow();
                Table.SetCell(
                    t_s("Name", "listavailmods"),
                    (CZNC::Get().GetModules().FindModule(Info.GetName())
                         ? "*"
                         : " ") +
                        Info.GetName());
                Table.SetCell(t_s("Description", "listavailmods"),
                              Info.GetDescription().Ellipsize(128));
            }

            PutStatus(Table);
        };

        if (m_pUser->IsAdmin()) {
            set<CModInfo> ssGlobalMods;
            CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods,
                                                      CModInfo::GlobalModule);

            PutStatus("");
            if (ssGlobalMods.empty()) {
                PutStatus(t_s("No global modules available."));
            } else {
                PutStatus(t_s("Global modules:"));
                PrintModules(ssGlobalMods);
            }
        }

        set<CModInfo> ssUserMods;
        CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

        PutStatus("");
        if (ssUserMods.empty()) {
            PutStatus(t_s("No user modules available."));
        } else {
            PutStatus(t_s("User modules:"));
            PrintModules(ssUserMods);
        }

        set<CModInfo> ssNetworkMods;
        CZNC::Get().GetModules().GetAvailableMods(ssNetworkMods,
                                                  CModInfo::NetworkModule);

        PutStatus("");
        if (ssNetworkMods.empty()) {
            PutStatus(t_s("No network modules available."));
        } else {
            PutStatus(t_s("Network modules:"));
            PrintModules(ssNetworkMods);
        }
        return;
    } else if (sCommand.Equals("LOADMOD") || sCommand.Equals("LOADMODULE")) {
        CModInfo::EModuleType eType;
        CString sType = sLine.Token(1);
        CString sMod = sLine.Token(2);
        CString sArgs = sLine.Token(3, true);

        // TODO use proper library for parsing arguments
        if (sType.Equals("--type=global")) {
            eType = CModInfo::GlobalModule;
        } else if (sType.Equals("--type=user")) {
            eType = CModInfo::UserModule;
        } else if (sType.Equals("--type=network")) {
            eType = CModInfo::NetworkModule;
        } else {
            sMod = sType;
            sArgs = sLine.Token(2, true);
            sType = "default";
            // Will be set correctly later
            eType = CModInfo::UserModule;
        }

        if (m_pUser->DenyLoadMod()) {
            PutStatus(t_f("Unable to load {1}: Access denied.")(sMod));
            return;
        }

        if (sMod.empty()) {
            PutStatus(t_s(
                "Usage: LoadMod [--type=global|user|network] <module> [args]"));
            return;
        }

        CModInfo ModInfo;
        CString sRetMsg;
        if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
            PutStatus(t_f("Unable to load {1}: {2}")(sMod, sRetMsg));
            return;
        }

        if (sType.Equals("default")) {
            eType = ModInfo.GetDefaultType();
        }

        if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
            PutStatus(
                t_f("Unable to load global module {1}: Access denied.")(sMod));
            return;
        }

        if (eType == CModInfo::NetworkModule && !m_pNetwork) {
            PutStatus(
                t_f("Unable to load network module {1}: Not connected with a "
                    "network.")(sMod));
            return;
        }

        CString sModRet;
        bool bLoaded = false;

        switch (eType) {
            case CModInfo::GlobalModule:
                bLoaded = CZNC::Get().GetModules().LoadModule(
                    sMod, sArgs, eType, nullptr, nullptr, sModRet);
                break;
            case CModInfo::UserModule:
                bLoaded = m_pUser->GetModules().LoadModule(
                    sMod, sArgs, eType, m_pUser, nullptr, sModRet);
                break;
            case CModInfo::NetworkModule:
                bLoaded = m_pNetwork->GetModules().LoadModule(
                    sMod, sArgs, eType, m_pUser, m_pNetwork, sModRet);
                break;
            default:
                sModRet = t_s("Unknown module type");
        }

        if (bLoaded) {
            if (sModRet.empty()) {
                PutStatus(t_f("Loaded module {1}")(sMod));
            } else {
                PutStatus(t_f("Loaded module {1}: {2}")(sMod, sModRet));
            }
        } else {
            PutStatus(t_f("Unable to load module {1}: {2}")(sMod, sModRet));
        }

        return;
    } else if (sCommand.Equals("UNLOADMOD") ||
               sCommand.Equals("UNLOADMODULE")) {
        CModInfo::EModuleType eType = CModInfo::UserModule;
        CString sType = sLine.Token(1);
        CString sMod = sLine.Token(2);

        // TODO use proper library for parsing arguments
        if (sType.Equals("--type=global")) {
            eType = CModInfo::GlobalModule;
        } else if (sType.Equals("--type=user")) {
            eType = CModInfo::UserModule;
        } else if (sType.Equals("--type=network")) {
            eType = CModInfo::NetworkModule;
        } else {
            sMod = sType;
            sType = "default";
        }

        if (m_pUser->DenyLoadMod()) {
            PutStatus(t_f("Unable to unload {1}: Access denied.")(sMod));
            return;
        }

        if (sMod.empty()) {
            PutStatus(
                t_s("Usage: UnloadMod [--type=global|user|network] <module>"));
            return;
        }

        if (sType.Equals("default")) {
            CModInfo ModInfo;
            CString sRetMsg;
            if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
                PutStatus(
                    t_f("Unable to determine type of {1}: {2}")(sMod, sRetMsg));
                return;
            }

            eType = ModInfo.GetDefaultType();
        }

        if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
            PutStatus(t_f("Unable to unload global module {1}: Access denied.")(
                sMod));
            return;
        }

        if (eType == CModInfo::NetworkModule && !m_pNetwork) {
            PutStatus(
                t_f("Unable to unload network module {1}: Not connected with a "
                    "network.")(sMod));
            return;
        }

        CString sModRet;

        switch (eType) {
            case CModInfo::GlobalModule:
                CZNC::Get().GetModules().UnloadModule(sMod, sModRet);
                break;
            case CModInfo::UserModule:
                m_pUser->GetModules().UnloadModule(sMod, sModRet);
                break;
            case CModInfo::NetworkModule:
                m_pNetwork->GetModules().UnloadModule(sMod, sModRet);
                break;
            default:
                sModRet = t_f(
                    "Unable to unload module {1}: Unknown module type")(sMod);
        }

        PutStatus(sModRet);
        return;
    } else if (sCommand.Equals("RELOADMOD") ||
               sCommand.Equals("RELOADMODULE")) {
        CModInfo::EModuleType eType;
        CString sType = sLine.Token(1);
        CString sMod = sLine.Token(2);
        CString sArgs = sLine.Token(3, true);

        if (m_pUser->DenyLoadMod()) {
            PutStatus(t_s("Unable to reload modules. Access denied."));
            return;
        }

        // TODO use proper library for parsing arguments
        if (sType.Equals("--type=global")) {
            eType = CModInfo::GlobalModule;
        } else if (sType.Equals("--type=user")) {
            eType = CModInfo::UserModule;
        } else if (sType.Equals("--type=network")) {
            eType = CModInfo::NetworkModule;
        } else {
            sMod = sType;
            sArgs = sLine.Token(2, true);
            sType = "default";
            // Will be set correctly later
            eType = CModInfo::UserModule;
        }

        if (sMod.empty()) {
            PutStatus(
                t_s("Usage: ReloadMod [--type=global|user|network] <module> "
                    "[args]"));
            return;
        }

        if (sType.Equals("default")) {
            CModInfo ModInfo;
            CString sRetMsg;
            if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
                PutStatus(t_f("Unable to reload {1}: {2}")(sMod, sRetMsg));
                return;
            }

            eType = ModInfo.GetDefaultType();
        }

        if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
            PutStatus(t_f("Unable to reload global module {1}: Access denied.")(
                sMod));
            return;
        }

        if (eType == CModInfo::NetworkModule && !m_pNetwork) {
            PutStatus(
                t_f("Unable to reload network module {1}: Not connected with a "
                    "network.")(sMod));
            return;
        }

        CString sModRet;

        switch (eType) {
            case CModInfo::GlobalModule:
                CZNC::Get().GetModules().ReloadModule(sMod, sArgs, nullptr,
                                                      nullptr, sModRet);
                break;
            case CModInfo::UserModule:
                m_pUser->GetModules().ReloadModule(sMod, sArgs, m_pUser,
                                                   nullptr, sModRet);
                break;
            case CModInfo::NetworkModule:
                m_pNetwork->GetModules().ReloadModule(sMod, sArgs, m_pUser,
                                                      m_pNetwork, sModRet);
                break;
            default:
                sModRet = t_f(
                    "Unable to reload module {1}: Unknown module type")(sMod);
        }

        PutStatus(sModRet);
        return;
    } else if ((sCommand.Equals("UPDATEMOD") ||
                sCommand.Equals("UPDATEMODULE")) &&
               m_pUser->IsAdmin()) {
        CString sMod = sLine.Token(1);

        if (sMod.empty()) {
            PutStatus(t_s("Usage: UpdateMod <module>"));
            return;
        }

        PutStatus(t_f("Reloading {1} everywhere")(sMod));
        if (CZNC::Get().UpdateModule(sMod)) {
            PutStatus(t_s("Done"));
        } else {
            PutStatus(
                t_f("Done, but there were errors, module {1} could not be "
                    "reloaded everywhere.")(sMod));
        }
    } else if ((sCommand.Equals("SETBINDHOST") ||
                sCommand.Equals("SETVHOST")) &&
               (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command. Try "
                "SetUserBindHost instead"));
            return;
        }
        CString sArg = sLine.Token(1);

        if (sArg.empty()) {
            PutStatus(t_s("Usage: SetBindHost <host>"));
            return;
        }

        if (sArg.Equals(m_pNetwork->GetBindHost())) {
            PutStatus(t_s("You already have this bind host!"));
            return;
        }

        m_pNetwork->SetBindHost(sArg);
        PutStatus(t_f("Set bind host for network {1} to {2}")(
            m_pNetwork->GetName(), m_pNetwork->GetBindHost()));
    } else if (sCommand.Equals("SETUSERBINDHOST") &&
               (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
        CString sArg = sLine.Token(1);

        if (sArg.empty()) {
            PutStatus(t_s("Usage: SetUserBindHost <host>"));
            return;
        }

        if (sArg.Equals(m_pUser->GetBindHost())) {
            PutStatus(t_s("You already have this bind host!"));
            return;
        }

        m_pUser->SetBindHost(sArg);
        PutStatus(t_f("Set default bind host to {1}")(m_pUser->GetBindHost()));
    } else if ((sCommand.Equals("CLEARBINDHOST") ||
                sCommand.Equals("CLEARVHOST")) &&
               (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command. Try "
                "ClearUserBindHost instead"));
            return;
        }
        m_pNetwork->SetBindHost("");
        PutStatus(t_s("Bind host cleared for this network."));
    } else if (sCommand.Equals("CLEARUSERBINDHOST") &&
               (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
        m_pUser->SetBindHost("");
        PutStatus(t_s("Default bind host cleared for your user."));
    } else if (sCommand.Equals("SHOWBINDHOST")) {
        if (m_pUser->GetBindHost().empty()) {
            PutStatus(t_s("This user's default bind host not set"));
        } else {
            PutStatus(t_f("This user's default bind host is {1}")(
                m_pUser->GetBindHost()));
        }
        if (m_pNetwork) {
            if (m_pNetwork->GetBindHost().empty()) {
                PutStatus(t_s("This network's bind host not set"));
            } else {
                PutStatus(t_f("This network's default bind host is {1}")(
                    m_pNetwork->GetBindHost()));
            }
        }
    } else if (sCommand.Equals("PLAYBUFFER")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sBuffer = sLine.Token(1);

        if (sBuffer.empty()) {
            PutStatus(t_s("Usage: PlayBuffer <#chan|query>"));
            return;
        }

        if (m_pNetwork->IsChan(sBuffer)) {
            CChan* pChan = m_pNetwork->FindChan(sBuffer);

            if (!pChan) {
                PutStatus(t_f("You are not on {1}")(sBuffer));
                return;
            }

            if (!pChan->IsOn()) {
                PutStatus(t_f("You are not on {1} (trying to join)")(sBuffer));
                return;
            }

            if (pChan->GetBuffer().IsEmpty()) {
                PutStatus(t_f("The buffer for channel {1} is empty")(sBuffer));
                return;
            }

            pChan->SendBuffer(this);
        } else {
            CQuery* pQuery = m_pNetwork->FindQuery(sBuffer);

            if (!pQuery) {
                PutStatus(t_f("No active query with {1}")(sBuffer));
                return;
            }

            if (pQuery->GetBuffer().IsEmpty()) {
                PutStatus(t_f("The buffer for {1} is empty")(sBuffer));
                return;
            }

            pQuery->SendBuffer(this);
        }
    } else if (sCommand.Equals("CLEARBUFFER")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sBuffer = sLine.Token(1);

        if (sBuffer.empty()) {
            PutStatus(t_s("Usage: ClearBuffer <#chan|query>"));
            return;
        }

        unsigned int uMatches = 0;
        vector<CChan*> vChans = m_pNetwork->FindChans(sBuffer);
        for (CChan* pChan : vChans) {
            uMatches++;

            pChan->ClearBuffer();
        }

        vector<CQuery*> vQueries = m_pNetwork->FindQueries(sBuffer);
        for (CQuery* pQuery : vQueries) {
            uMatches++;

            m_pNetwork->DelQuery(pQuery->GetName());
        }

        PutStatus(t_p("{1} buffer matching {2} has been cleared",
                      "{1} buffers matching {2} have been cleared",
                      uMatches)(uMatches, sBuffer));
    } else if (sCommand.Equals("CLEARALLCHANNELBUFFERS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        for (CChan* pChan : m_pNetwork->GetChans()) {
            pChan->ClearBuffer();
        }
        PutStatus(t_s("All channel buffers have been cleared"));
    } else if (sCommand.Equals("CLEARALLQUERYBUFFERS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        m_pNetwork->ClearQueryBuffer();
        PutStatus(t_s("All query buffers have been cleared"));
    } else if (sCommand.Equals("CLEARALLBUFFERS")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        for (CChan* pChan : m_pNetwork->GetChans()) {
            pChan->ClearBuffer();
        }
        m_pNetwork->ClearQueryBuffer();
        PutStatus(t_s("All buffers have been cleared"));
    } else if (sCommand.Equals("SETBUFFER")) {
        if (!m_pNetwork) {
            PutStatus(t_s(
                "You must be connected with a network to use this command"));
            return;
        }

        CString sBuffer = sLine.Token(1);

        if (sBuffer.empty()) {
            PutStatus(t_s("Usage: SetBuffer <#chan|query> [linecount]"));
            return;
        }

        unsigned int uLineCount = sLine.Token(2).ToUInt();
        unsigned int uMatches = 0, uFail = 0;
        vector<CChan*> vChans = m_pNetwork->FindChans(sBuffer);
        for (CChan* pChan : vChans) {
            uMatches++;

            if (!pChan->SetBufferCount(uLineCount)) uFail++;
        }

        vector<CQuery*> vQueries = m_pNetwork->FindQueries(sBuffer);
        for (CQuery* pQuery : vQueries) {
            uMatches++;

            if (!pQuery->SetBufferCount(uLineCount)) uFail++;
        }

        if (uFail > 0) {
            PutStatus(t_p("Setting buffer size failed for {1} buffer",
                          "Setting buffer size failed for {1} buffers",
                          uFail)(uFail));
            PutStatus(t_p("Maximum buffer size is {1} line",
                          "Maximum buffer size is {1} lines",
                          CZNC::Get().GetMaxBufferSize())(
                CZNC::Get().GetMaxBufferSize()));
        } else {
            PutStatus(t_p("Size of every buffer was set to {1} line",
                          "Size of every buffer was set to {1} lines",
                          uLineCount)(uLineCount));
        }
    } else if (m_pUser->IsAdmin() && sCommand.Equals("TRAFFIC")) {
        CZNC::TrafficStatsPair Users, ZNC, Total;
        CZNC::TrafficStatsMap traffic =
            CZNC::Get().GetTrafficStats(Users, ZNC, Total);

        CTable Table;
        Table.AddColumn(t_s("Username", "trafficcmd"));
        Table.AddColumn(t_s("In", "trafficcmd"));
        Table.AddColumn(t_s("Out", "trafficcmd"));
        Table.AddColumn(t_s("Total", "trafficcmd"));

        for (const auto& it : traffic) {
            Table.AddRow();
            Table.SetCell(t_s("Username", "trafficcmd"), it.first);
            Table.SetCell(t_s("In", "trafficcmd"),
                          CString::ToByteStr(it.second.first));
            Table.SetCell(t_s("Out", "trafficcmd"),
                          CString::ToByteStr(it.second.second));
            Table.SetCell(
                t_s("Total", "trafficcmd"),
                CString::ToByteStr(it.second.first + it.second.second));
        }

        Table.AddRow();
        Table.SetCell(t_s("Username", "trafficcmd"),
                      t_s("<Users>", "trafficcmd"));
        Table.SetCell(t_s("In", "trafficcmd"), CString::ToByteStr(Users.first));
        Table.SetCell(t_s("Out", "trafficcmd"),
                      CString::ToByteStr(Users.second));
        Table.SetCell(t_s("Total", "trafficcmd"),
                      CString::ToByteStr(Users.first + Users.second));

        Table.AddRow();
        Table.SetCell(t_s("Username", "trafficcmd"),
                      t_s("<ZNC>", "trafficcmd"));
        Table.SetCell(t_s("In", "trafficcmd"), CString::ToByteStr(ZNC.first));
        Table.SetCell(t_s("Out", "trafficcmd"), CString::ToByteStr(ZNC.second));
        Table.SetCell(t_s("Total", "trafficcmd"),
                      CString::ToByteStr(ZNC.first + ZNC.second));

        Table.AddRow();
        Table.SetCell(t_s("Username", "trafficcmd"),
                      t_s("<Total>", "trafficcmd"));
        Table.SetCell(t_s("In", "trafficcmd"), CString::ToByteStr(Total.first));
        Table.SetCell(t_s("Out", "trafficcmd"),
                      CString::ToByteStr(Total.second));
        Table.SetCell(t_s("Total", "trafficcmd"),
                      CString::ToByteStr(Total.first + Total.second));

        PutStatus(Table);
    } else if (sCommand.Equals("UPTIME")) {
        PutStatus(t_f("Running for {1}")(CZNC::Get().GetUptime()));
    } else if (m_pUser->IsAdmin() &&
               (sCommand.Equals("LISTPORTS") || sCommand.Equals("ADDPORT") ||
                sCommand.Equals("DELPORT"))) {
        UserPortCommand(sLine);
    } else {
        PutStatus(t_s("Unknown command, try 'Help'"));
    }
}

void CClient::UserPortCommand(CString& sLine) {
    const CString sCommand = sLine.Token(0);

    if (sCommand.Equals("LISTPORTS")) {
        CTable Table;
        Table.AddColumn(t_s("Port", "listports"));
        Table.AddColumn(t_s("BindHost", "listports"));
        Table.AddColumn(t_s("SSL", "listports"));
        Table.AddColumn(t_s("Protocol", "listports"));
        Table.AddColumn(t_s("IRC", "listports"));
        Table.AddColumn(t_s("Web", "listports"));

        const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();

        for (const CListener* pListener : vpListeners) {
            Table.AddRow();
            Table.SetCell(t_s("Port", "listports"),
                          CString(pListener->GetPort()));
            Table.SetCell(
                t_s("BindHost", "listports"),
                (pListener->GetBindHost().empty() ? CString("*")
                                                  : pListener->GetBindHost()));
            Table.SetCell(t_s("SSL", "listports"),
                          pListener->IsSSL() ? t_s("yes", "listports|ssl")
                                             : t_s("no", "listports|ssl"));

            EAddrType eAddr = pListener->GetAddrType();
            Table.SetCell(t_s("Protocol", "listports"),
                          eAddr == ADDR_ALL ? t_s("IPv4 and IPv6", "listports")
                                            : (eAddr == ADDR_IPV4ONLY
                                                   ? t_s("IPv4", "listports")
                                                   : t_s("IPv6", "listports")));

            CListener::EAcceptType eAccept = pListener->GetAcceptType();
            Table.SetCell(t_s("IRC", "listports"),
                          eAccept == CListener::ACCEPT_ALL ||
                                  eAccept == CListener::ACCEPT_IRC
                              ? t_s("yes", "listports|irc")
                              : t_s("no", "listports|irc"));
            Table.SetCell(t_s("Web", "listports"),
                          eAccept == CListener::ACCEPT_ALL ||
                                  eAccept == CListener::ACCEPT_HTTP
                              ? t_f("yes, on {1}", "listports|irc")(
                                    pListener->GetURIPrefix() + "/")
                              : t_s("no", "listports|web"));
        }

        PutStatus(Table);

        return;
    }

    CString sPort = sLine.Token(1);
    CString sAddr = sLine.Token(2);
    EAddrType eAddr = ADDR_ALL;

    if (sAddr.Equals("IPV4")) {
        eAddr = ADDR_IPV4ONLY;
    } else if (sAddr.Equals("IPV6")) {
        eAddr = ADDR_IPV6ONLY;
    } else if (sAddr.Equals("ALL")) {
        eAddr = ADDR_ALL;
    } else {
        sAddr.clear();
    }

    unsigned short uPort = sPort.ToUShort();

    if (sCommand.Equals("ADDPORT")) {
        CListener::EAcceptType eAccept = CListener::ACCEPT_ALL;
        CString sAccept = sLine.Token(3);

        if (sAccept.Equals("WEB")) {
            eAccept = CListener::ACCEPT_HTTP;
        } else if (sAccept.Equals("IRC")) {
            eAccept = CListener::ACCEPT_IRC;
        } else if (sAccept.Equals("ALL")) {
            eAccept = CListener::ACCEPT_ALL;
        } else {
            sAccept.clear();
        }

        if (sPort.empty() || sAddr.empty() || sAccept.empty()) {
            PutStatus(
                t_s("Usage: AddPort <[+]port> <ipv4|ipv6|all> <web|irc|all> "
                    "[bindhost [uriprefix]]"));
        } else {
            bool bSSL = (sPort.StartsWith("+"));
            const CString sBindHost = sLine.Token(4);
            const CString sURIPrefix = sLine.Token(5);

            CListener* pListener = new CListener(uPort, sBindHost, sURIPrefix,
                                                 bSSL, eAddr, eAccept);

            if (!pListener->Listen()) {
                auto e = errno;
                delete pListener;
                PutStatus(t_f("Unable to bind: {1}")(CString(strerror(e))));
            } else {
                if (CZNC::Get().AddListener(pListener)) {
                    PutStatus(t_s("Port added"));
                } else {
                    PutStatus(t_s("Couldn't add port"));
                }
            }
        }
    } else if (sCommand.Equals("DELPORT")) {
        if (sPort.empty() || sAddr.empty()) {
            PutStatus(t_s("Usage: DelPort <port> <ipv4|ipv6|all> [bindhost]"));
        } else {
            const CString sBindHost = sLine.Token(3);

            CListener* pListener =
                CZNC::Get().FindListener(uPort, sBindHost, eAddr);

            if (pListener) {
                CZNC::Get().DelListener(pListener);
                PutStatus(t_s("Deleted Port"));
            } else {
                PutStatus(t_s("Unable to find a matching port"));
            }
        }
    }
}

void CClient::HelpUser(const CString& sFilter) {
    CTable Table;
    Table.AddColumn(t_s("Command", "helpcmd"));
    Table.AddColumn(t_s("Description", "helpcmd"));
    Table.SetStyle(CTable::ListStyle);

    if (sFilter.empty()) {
        PutStatus(
            t_s("In the following list all occurrences of <#chan> support "
                "wildcards (* and ?) except ListNicks"));
    }

    const auto AddCommandHelp = [&](const CString& sCmd, const CString& sArgs,
                                    const CString& sDesc) {
        if (sFilter.empty() || sCmd.StartsWith(sFilter) ||
            sCmd.AsLower().WildCmp(sFilter.AsLower())) {
            Table.AddRow();
            Table.SetCell(t_s("Command", "helpcmd"),
                          sCmd + (sArgs.empty() ? "" : " ") + sArgs);
            Table.SetCell(t_s("Description", "helpcmd"), sDesc);
        }
    };

    AddCommandHelp(
        "Version", "",
        t_s("Print which version of ZNC this is", "helpcmd|Version|desc"));

    AddCommandHelp("ListMods", "",
                   t_s("List all loaded modules", "helpcmd|ListMods|desc"));
    AddCommandHelp(
        "ListAvailMods", "",
        t_s("List all available modules", "helpcmd|ListAvailMods|desc"));
    if (!m_pUser->IsAdmin()) {
        // If they are an admin we will add this command below with an argument
        AddCommandHelp("ListChans", "",
                       t_s("List all channels", "helpcmd|ListChans|desc"));
    }
    AddCommandHelp(
        "ListNicks", t_s("<#chan>", "helpcmd|ListNicks|args"),
        t_s("List all nicks on a channel", "helpcmd|ListNicks|desc"));
    if (!m_pUser->IsAdmin()) {
        AddCommandHelp("ListClients", "",
                       t_s("List all clients connected to your ZNC user",
                           "helpcmd|ListClients|desc"));
    }
    AddCommandHelp("ListServers", "",
                   t_s("List all servers of current IRC network",
                       "helpcmd|ListServers|desc"));

    AddCommandHelp(
        "AddNetwork", t_s("<name>", "helpcmd|AddNetwork|args"),
        t_s("Add a network to your user", "helpcmd|AddNetwork|desc"));
    AddCommandHelp(
        "DelNetwork", t_s("<name>", "helpcmd|DelNetwork|args"),
        t_s("Delete a network from your user", "helpcmd|DelNetwork|desc"));
    AddCommandHelp("ListNetworks", "",
                   t_s("List all networks", "helpcmd|ListNetworks|desc"));
    if (m_pUser->IsAdmin()) {
        AddCommandHelp("MoveNetwork",
                       t_s("<old user> <old network> <new user> [new network]",
                           "helpcmd|MoveNetwork|args"),
                       t_s("Move an IRC network from one user to another",
                           "helpcmd|MoveNetwork|desc"));
    }
    AddCommandHelp(
        "JumpNetwork", t_s("<network>", "helpcmd|JumpNetwork|args"),
        t_s("Jump to another network (Alternatively, you can connect to ZNC "
            "several times, using `user/network` as username)",
            "helpcmd|JumpNetwork|desc"));

    AddCommandHelp("AddServer",
                   t_s("<host> [[+]port] [pass]", "helpcmd|AddServer|args"),
                   t_s("Add a server to the list of alternate/backup servers "
                       "of current IRC network.",
                       "helpcmd|AddServer|desc"));
    AddCommandHelp("DelServer",
                   t_s("<host> [port] [pass]", "helpcmd|DelServer|args"),
                   t_s("Remove a server from the list of alternate/backup "
                       "servers of current IRC network",
                       "helpcmd|DelServer|desc"));

    AddCommandHelp(
        "AddTrustedServerFingerprint",
        t_s("<fi:ng:er>", "helpcmd|AddTrustedServerFingerprint|args"),
        t_s("Add a trusted server SSL certificate fingerprint (SHA-256) to "
            "current IRC network.",
            "helpcmd|AddTrustedServerFingerprint|desc"));
    AddCommandHelp(
        "DelTrustedServerFingerprint",
        t_s("<fi:ng:er>", "helpcmd|DelTrustedServerFingerprint|args"),
        t_s("Delete a trusted server SSL certificate from current IRC network.",
            "helpcmd|DelTrustedServerFingerprint|desc"));
    AddCommandHelp(
        "ListTrustedServerFingerprints", "",
        t_s("List all trusted server SSL certificates of current IRC network.",
            "helpcmd|ListTrustedServerFingerprints|desc"));

    AddCommandHelp("EnableChan", t_s("<#chans>", "helpcmd|EnableChan|args"),
                   t_s("Enable channels", "helpcmd|EnableChan|desc"));
    AddCommandHelp("DisableChan", t_s("<#chans>", "helpcmd|DisableChan|args"),
                   t_s("Disable channels", "helpcmd|DisableChan|desc"));
    AddCommandHelp("Attach", t_s("<#chans>", "helpcmd|Attach|args"),
                   t_s("Attach to channels", "helpcmd|Attach|desc"));
    AddCommandHelp("Detach", t_s("<#chans>", "helpcmd|Detach|args"),
                   t_s("Detach from channels", "helpcmd|Detach|desc"));
    AddCommandHelp(
        "Topics", "",
        t_s("Show topics in all your channels", "helpcmd|Topics|desc"));

    AddCommandHelp(
        "PlayBuffer", t_s("<#chan|query>", "helpcmd|PlayBuffer|args"),
        t_s("Play back the specified buffer", "helpcmd|PlayBuffer|desc"));
    AddCommandHelp(
        "ClearBuffer", t_s("<#chan|query>", "helpcmd|ClearBuffer|args"),
        t_s("Clear the specified buffer", "helpcmd|ClearBuffer|desc"));
    AddCommandHelp("ClearAllBuffers", "",
                   t_s("Clear all channel and query buffers",
                       "helpcmd|ClearAllBuffers|desc"));
    AddCommandHelp("ClearAllChannelBuffers", "",
                   t_s("Clear the channel buffers",
                       "helpcmd|ClearAllChannelBuffers|desc"));
    AddCommandHelp(
        "ClearAllQueryBuffers", "",
        t_s("Clear the query buffers", "helpcmd|ClearAllQueryBuffers|desc"));
    AddCommandHelp("SetBuffer",
                   t_s("<#chan|query> [linecount]", "helpcmd|SetBuffer|args"),
                   t_s("Set the buffer count", "helpcmd|SetBuffer|desc"));

    if (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost()) {
        AddCommandHelp("SetBindHost",
                       t_s("<host (IP preferred)>", "helpcmd|SetBindHost|args"),
                       t_s("Set the bind host for this network",
                           "helpcmd|SetBindHost|desc"));
        AddCommandHelp(
            "SetUserBindHost",
            t_s("<host (IP preferred)>", "helpcmd|SetUserBindHost|args"),
            t_s("Set the default bind host for this user",
                "helpcmd|SetUserBindHost|desc"));
        AddCommandHelp("ClearBindHost", "",
                       t_s("Clear the bind host for this network",
                           "helpcmd|ClearBindHost|desc"));
        AddCommandHelp("ClearUserBindHost", "",
                       t_s("Clear the default bind host for this user",
                           "helpcmd|ClearUserBindHost|desc"));
    }

    AddCommandHelp(
        "ShowBindHost", "",
        t_s("Show currently selected bind host", "helpcmd|ShowBindHost|desc"));
    AddCommandHelp(
        "Jump", t_s("[server]", "helpcmd|Jump|args"),
        t_s("Jump to the next or the specified server", "helpcmd|Jump|desc"));
    AddCommandHelp("Disconnect", t_s("[message]", "helpcmd|Disconnect|args"),
                   t_s("Disconnect from IRC", "helpcmd|Disconnect|desc"));
    AddCommandHelp("Connect", "",
                   t_s("Reconnect to IRC", "helpcmd|Connect|desc"));
    AddCommandHelp(
        "Uptime", "",
        t_s("Show for how long ZNC has been running", "helpcmd|Uptime|desc"));

    if (!m_pUser->DenyLoadMod()) {
        AddCommandHelp("LoadMod",
                       t_s("[--type=global|user|network] <module> [args]",
                           "helpcmd|LoadMod|args"),
                       t_s("Load a module", "helpcmd|LoadMod|desc"));
        AddCommandHelp("UnloadMod",
                       t_s("[--type=global|user|network] <module>",
                           "helpcmd|UnloadMod|args"),
                       t_s("Unload a module", "helpcmd|UnloadMod|desc"));
        AddCommandHelp("ReloadMod",
                       t_s("[--type=global|user|network] <module> [args]",
                           "helpcmd|ReloadMod|args"),
                       t_s("Reload a module", "helpcmd|ReloadMod|desc"));
        if (m_pUser->IsAdmin()) {
            AddCommandHelp(
                "UpdateMod", t_s("<module>", "helpcmd|UpdateMod|args"),
                t_s("Reload a module everywhere", "helpcmd|UpdateMod|desc"));
        }
    }

    AddCommandHelp(
        "ShowMOTD", "",
        t_s("Show ZNC's message of the day", "helpcmd|ShowMOTD|desc"));

    if (m_pUser->IsAdmin()) {
        AddCommandHelp(
            "SetMOTD", t_s("<message>", "helpcmd|SetMOTD|args"),
            t_s("Set ZNC's message of the day", "helpcmd|SetMOTD|desc"));
        AddCommandHelp(
            "AddMOTD", t_s("<message>", "helpcmd|AddMOTD|args"),
            t_s("Append <message> to ZNC's MOTD", "helpcmd|AddMOTD|desc"));
        AddCommandHelp("ClearMOTD", "",
                       t_s("Clear ZNC's MOTD", "helpcmd|ClearMOTD|desc"));
        AddCommandHelp(
            "ListPorts", "",
            t_s("Show all active listeners", "helpcmd|ListPorts|desc"));
        AddCommandHelp("AddPort",
                       t_s("<[+]port> <ipv4|ipv6|all> <web|irc|all> [bindhost "
                           "[uriprefix]]",
                           "helpcmd|AddPort|args"),
                       t_s("Add another port for ZNC to listen on",
                           "helpcmd|AddPort|desc"));
        AddCommandHelp(
            "DelPort",
            t_s("<port> <ipv4|ipv6|all> [bindhost]", "helpcmd|DelPort|args"),
            t_s("Remove a port from ZNC", "helpcmd|DelPort|desc"));
        AddCommandHelp(
            "Rehash", "",
            t_s("Reload global settings, modules, and listeners from znc.conf",
                "helpcmd|Rehash|desc"));
        AddCommandHelp("SaveConfig", "",
                       t_s("Save the current settings to disk",
                           "helpcmd|SaveConfig|desc"));
        AddCommandHelp("ListUsers", "",
                       t_s("List all ZNC users and their connection status",
                           "helpcmd|ListUsers|desc"));
        AddCommandHelp("ListAllUserNetworks", "",
                       t_s("List all ZNC users and their networks",
                           "helpcmd|ListAllUserNetworks|desc"));
        AddCommandHelp("ListChans",
                       t_s("[user <network>]", "helpcmd|ListChans|args"),
                       t_s("List all channels", "helpcmd|ListChans|desc"));
        AddCommandHelp(
            "ListClients", t_s("[user]", "helpcmd|ListClients|args"),
            t_s("List all connected clients", "helpcmd|ListClients|desc"));
        AddCommandHelp("Traffic", "",
                       t_s("Show basic traffic stats for all ZNC users",
                           "helpcmd|Traffic|desc"));
        AddCommandHelp("Broadcast", t_s("[message]", "helpcmd|Broadcast|args"),
                       t_s("Broadcast a message to all ZNC users",
                           "helpcmd|Broadcast|desc"));
        AddCommandHelp(
            "Shutdown", t_s("[message]", "helpcmd|Shutdown|args"),
            t_s("Shut down ZNC completely", "helpcmd|Shutdown|desc"));
        AddCommandHelp("Restart", t_s("[message]", "helpcmd|Restart|args"),
                       t_s("Restart ZNC", "helpcmd|Restart|desc"));
    }

    if (Table.empty()) {
        PutStatus(t_f("No matches for '{1}'")(sFilter));
    } else {
        PutStatus(Table);
    }
}
