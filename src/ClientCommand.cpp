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

#include <znc/Chan.h>
#include <znc/FileUtils.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Server.h>
#include <znc/User.h>
#include <znc/Query.h>

using std::vector;
using std::set;
using std::map;

void CClient::UserCommand(CString& sLine) {
	if (!m_pUser) {
		return;
	}

	if (sLine.empty()) {
		return;
	}

	bool bReturn = false;
	NETWORKMODULECALL(OnStatusCommand(sLine), m_pUser, m_pNetwork, this, &bReturn);
	if (bReturn) return;

	const CString sCommand = sLine.Token(0);

	if (sCommand.Equals("HELP")) {
		HelpUser(sLine.Token(1));
	} else if (sCommand.Equals("LISTNICKS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: ListNicks <#chan>");
			return;
		}

		CChan* pChan = m_pNetwork->FindChan(sChan);

		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}

		if (!pChan->IsOn()) {
			PutStatus("You are not on [" + sChan + "] [trying]");
			return;
		}

		const map<CString,CNick>& msNicks = pChan->GetNicks();
		CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();
		const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

		if (msNicks.empty()) {
			PutStatus("No nicks on [" + sChan + "]");
			return;
		}

		CTable Table;

		for (unsigned int p = 0; p < sPerms.size(); p++) {
			CString sPerm;
			sPerm += sPerms[p];
			Table.AddColumn(sPerm);
		}

		Table.AddColumn("Nick");
		Table.AddColumn("Ident");
		Table.AddColumn("Host");

		for (const auto& it : msNicks) {
			Table.AddRow();

			for (unsigned int b = 0; b < sPerms.size(); b++) {
				if (it.second.HasPerm(sPerms[b])) {
					CString sPerm;
					sPerm += sPerms[b];
					Table.SetCell(sPerm, sPerm);
				}
			}

			Table.SetCell("Nick", it.second.GetNick());
			Table.SetCell("Ident", it.second.GetIdent());
			Table.SetCell("Host", it.second.GetHost());
		}

		PutStatus(Table);
	} else if (sCommand.Equals("DETACH")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sPatterns = sLine.Token(1, true);

		if (sPatterns.empty()) {
			PutStatus("Usage: Detach <#chans>");
			return;
		}

		VCString vsChans;
		sPatterns.Replace(",", " ");
		sPatterns.Split(" ", vsChans, false, "", "", true, true);

		set<CChan*> sChans;
		for (const CString& sChan : vsChans) {
			vector<CChan*> vChans = m_pNetwork->FindChans(sChan);
			sChans.insert(vChans.begin(), vChans.end());
		}

		unsigned int uDetached = 0;
		for (CChan* pChan : sChans) {
			if (pChan->IsDetached())
				continue;
			uDetached++;
			pChan->DetachUser();
		}

		PutStatus("There were [" + CString(sChans.size()) + "] channels matching [" + sPatterns + "]");
		PutStatus("Detached [" + CString(uDetached) + "] channels");
	} else if (sCommand.Equals("VERSION")) {
		PutStatus(CZNC::GetTag());
		PutStatus(CZNC::GetCompileOptionsString());
	} else if (sCommand.Equals("MOTD") || sCommand.Equals("ShowMOTD")) {
		if (!SendMotd()) {
			PutStatus("There is no MOTD set.");
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("Rehash")) {
		CString sRet;

		if (CZNC::Get().RehashConfig(sRet)) {
			PutStatus("Rehashing succeeded!");
		} else {
			PutStatus("Rehashing failed: " + sRet);
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("SaveConfig")) {
		if (CZNC::Get().WriteConfig()) {
			PutStatus("Wrote config to [" + CZNC::Get().GetConfigFile() + "]");
		} else {
			PutStatus("Error while trying to write config.");
		}
	} else if (sCommand.Equals("LISTCLIENTS")) {
		CUser* pUser = m_pUser;
		CString sNick = sLine.Token(1);

		if (!sNick.empty()) {
			if (!m_pUser->IsAdmin()) {
				PutStatus("Usage: ListClients");
				return;
			}

			pUser = CZNC::Get().FindUser(sNick);

			if (!pUser) {
				PutStatus("No such user [" + sNick + "]");
				return;
			}
		}

		vector<CClient*> vClients = pUser->GetAllClients();

		if (vClients.empty()) {
			PutStatus("No clients are connected");
			return;
		}

		CTable Table;
		Table.AddColumn("Host");
		Table.AddColumn("Network");
		Table.AddColumn("Identifier");

		for (const CClient* pClient : vClients) {
			Table.AddRow();
			Table.SetCell("Host", pClient->GetRemoteIP());
			if (pClient->GetNetwork()) {
				Table.SetCell("Network", pClient->GetNetwork()->GetName());
			}
			Table.SetCell("Identifier", pClient->GetIdentifier());
		}

		PutStatus(Table);
	} else if (m_pUser->IsAdmin() && sCommand.Equals("LISTUSERS")) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("Networks");
		Table.AddColumn("Clients");

		for (const auto& it : msUsers) {
			Table.AddRow();
			Table.SetCell("Username", it.first);
			Table.SetCell("Networks", CString(it.second->GetNetworks().size()));
			Table.SetCell("Clients", CString(it.second->GetAllClients().size()));
		}

		PutStatus(Table);
	} else if (m_pUser->IsAdmin() && sCommand.Equals("LISTALLUSERNETWORKS")) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("Network");
		Table.AddColumn("Clients");
		Table.AddColumn("OnIRC");
		Table.AddColumn("IRC Server");
		Table.AddColumn("IRC User");
		Table.AddColumn("Channels");

		for (const auto& it : msUsers) {
			Table.AddRow();
			Table.SetCell("Username", it.first);
			Table.SetCell("Network", "N/A");
			Table.SetCell("Clients", CString(it.second->GetUserClients().size()));

			const vector<CIRCNetwork*>& vNetworks = it.second->GetNetworks();

			for (const CIRCNetwork* pNetwork : vNetworks) {
				Table.AddRow();
				if (pNetwork == vNetworks.back()) {
					Table.SetCell("Username", "`-");
				} else {
					Table.SetCell("Username", "|-");
				}
				Table.SetCell("Network", pNetwork->GetName());
				Table.SetCell("Clients", CString(pNetwork->GetClients().size()));
				if (pNetwork->IsIRCConnected()) {
					Table.SetCell("OnIRC", "Yes");
					Table.SetCell("IRC Server", pNetwork->GetIRCServer());
					Table.SetCell("IRC User", pNetwork->GetIRCNick().GetNickMask());
					Table.SetCell("Channels", CString(pNetwork->GetChans().size()));
				} else {
					Table.SetCell("OnIRC", "No");
				}
			}
		}

		PutStatus(Table);
	} else if (m_pUser->IsAdmin() && sCommand.Equals("SetMOTD")) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: SetMOTD <message>");
		} else {
			CZNC::Get().SetMotd(sMessage);
			PutStatus("MOTD set to [" + sMessage + "]");
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("AddMOTD")) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: AddMOTD <message>");
		} else {
			CZNC::Get().AddMotd(sMessage);
			PutStatus("Added [" + sMessage + "] to MOTD");
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("ClearMOTD")) {
		CZNC::Get().ClearMotd();
		PutStatus("Cleared MOTD");
	} else if (m_pUser->IsAdmin() && sCommand.Equals("BROADCAST")) {
		CZNC::Get().Broadcast(sLine.Token(1, true));
	} else if (m_pUser->IsAdmin() && (sCommand.Equals("SHUTDOWN") || sCommand.Equals("RESTART"))) {
		bool bRestart = sCommand.Equals("RESTART");
		CString sMessage = sLine.Token(1, true);
		bool bForce = false;

		if (sMessage.Token(0).Equals("FORCE")) {
			bForce = true;
			sMessage = sMessage.Token(1, true);
		}

		if (sMessage.empty()) {
			sMessage = (bRestart ? "ZNC is being restarted NOW!" : "ZNC is being shut down NOW!");
		}

		if(!CZNC::Get().WriteConfig() && !bForce) {
			PutStatus("ERROR: Writing config file to disk failed! Aborting. Use " +
				sCommand.AsUpper() + " FORCE to ignore.");
		} else {
			CZNC::Get().Broadcast(sMessage);
			throw CException(bRestart ? CException::EX_Restart : CException::EX_Shutdown);
		}
	} else if (sCommand.Equals("JUMP") || sCommand.Equals("CONNECT")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		if (!m_pNetwork->HasServers()) {
			PutStatus("You don't have any servers added.");
			return;
		}

		CString sArgs = sLine.Token(1, true);
		sArgs.Trim();
		CServer *pServer = nullptr;

		if (!sArgs.empty()) {
			pServer = m_pNetwork->FindServer(sArgs);
			if (!pServer) {
				PutStatus("Server [" + sArgs + "] not found");
				return;
			}
			m_pNetwork->SetNextServer(pServer);

			// If we are already connecting to some server,
			// we have to abort that attempt
			Csock *pIRCSock = GetIRCSock();
			if (pIRCSock && !pIRCSock->IsConnected()) {
				pIRCSock->Close();
			}
		}

		if (GetIRCSock()) {
			GetIRCSock()->Quit();
			if (pServer)
				PutStatus("Connecting to [" + pServer->GetName() + "]...");
			else
				PutStatus("Jumping to the next server in the list...");
		} else {
			if (pServer)
				PutStatus("Connecting to [" + pServer->GetName() + "]...");
			else
				PutStatus("Connecting...");
		}

		m_pNetwork->SetIRCConnectEnabled(true);
		return;
	} else if (sCommand.Equals("DISCONNECT")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		if (GetIRCSock()) {
			CString sQuitMsg = sLine.Token(1, true);
			GetIRCSock()->Quit(sQuitMsg);
		}

		m_pNetwork->SetIRCConnectEnabled(false);
		PutStatus("Disconnected from IRC. Use 'connect' to reconnect.");
		return;
	} else if (sCommand.Equals("ENABLECHAN")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sPatterns = sLine.Token(1, true);

		if (sPatterns.empty()) {
			PutStatus("Usage: EnableChan <#chans>");
		} else {
			VCString vsChans;
			sPatterns.Replace(",", " ");
			sPatterns.Split(" ", vsChans, false, "", "", true, true);

			set<CChan*> sChans;
			for (const CString& sChan : vsChans) {
				vector<CChan*> vChans = m_pNetwork->FindChans(sChan);
				sChans.insert(vChans.begin(), vChans.end());
			}

			unsigned int uEnabled = 0;
			for (CChan* pChan : sChans) {
				if (!pChan->IsDisabled())
					continue;
				uEnabled++;
				pChan->Enable();
			}

			PutStatus("There were [" + CString(sChans.size()) + "] channels matching [" + sPatterns + "]");
			PutStatus("Enabled [" + CString(uEnabled) + "] channels");
		}
	} else if (sCommand.Equals("DISABLECHAN")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sPatterns = sLine.Token(1, true);

		if (sPatterns.empty()) {
			PutStatus("Usage: DisableChan <#chans>");
		} else {
			VCString vsChans;
			sPatterns.Replace(",", " ");
			sPatterns.Split(" ", vsChans, false, "", "", true, true);

			set<CChan*> sChans;
			for (const CString& sChan : vsChans) {
				vector<CChan*> vChans = m_pNetwork->FindChans(sChan);
				sChans.insert(vChans.begin(), vChans.end());
			}

			unsigned int uDisabled = 0;
			for (CChan* pChan : sChans) {
				if (pChan->IsDisabled())
					continue;
				uDisabled++;
				pChan->Disable();
			}

			PutStatus("There were [" + CString(sChans.size()) + "] channels matching [" + sPatterns + "]");
			PutStatus("Disabled [" + CString(uDisabled) + "] channels");
		}
	} else if (sCommand.Equals("LISTCHANS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CUser* pUser = m_pUser;
		CIRCNetwork* pNetwork = m_pNetwork;

		const CString sNick = sLine.Token(1);
		const CString sNetwork = sLine.Token(2);

		if (!sNick.empty()) {
			if (!m_pUser->IsAdmin()) {
				PutStatus("Usage: ListChans");
				return;
			}

			pUser = CZNC::Get().FindUser(sNick);

			if (!pUser) {
				PutStatus("No such user [" + sNick + "]");
				return;
			}

			pNetwork = pUser->FindNetwork(sNetwork);
			if (!pNetwork) {
				PutStatus("No such network for user [" + sNetwork + "]");
				return;
			}
		}

		const vector<CChan*>& vChans = pNetwork->GetChans();
		CIRCSock* pIRCSock = pNetwork->GetIRCSock();
		const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

		if (vChans.empty()) {
			PutStatus("There are no channels defined.");
			return;
		}

		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Status");
		Table.AddColumn("Conf");
		Table.AddColumn("Buf");
		Table.AddColumn("Clear");
		Table.AddColumn("Modes");
		Table.AddColumn("Users");

		for (unsigned int p = 0; p < sPerms.size(); p++) {
			CString sPerm;
			sPerm += sPerms[p];
			Table.AddColumn(sPerm);
		}

		unsigned int uNumDetached = 0, uNumDisabled = 0,
			uNumJoined = 0;

		for (const CChan *pChan : vChans) {
			Table.AddRow();
			Table.SetCell("Name", pChan->GetPermStr() + pChan->GetName());
			Table.SetCell("Status", ((pChan->IsOn()) ? ((pChan->IsDetached()) ? "Detached" : "Joined") : ((pChan->IsDisabled()) ? "Disabled" : "Trying")));
			Table.SetCell("Conf", CString((pChan->InConfig()) ? "yes" : ""));
			Table.SetCell("Buf", CString((pChan->HasBufferCountSet()) ? "*" : "") + CString(pChan->GetBufferCount()));
			Table.SetCell("Clear", CString((pChan->HasAutoClearChanBufferSet()) ? "*" : "") + CString((pChan->AutoClearChanBuffer()) ? "yes" : ""));
			Table.SetCell("Modes", pChan->GetModeString());
			Table.SetCell("Users", CString(pChan->GetNickCount()));

			map<char, unsigned int> mPerms = pChan->GetPermCounts();
			for (unsigned int b = 0; b < sPerms.size(); b++) {
				char cPerm = sPerms[b];
				Table.SetCell(CString(cPerm), CString(mPerms[cPerm]));
			}

			if(pChan->IsDetached()) uNumDetached++;
			if(pChan->IsOn()) uNumJoined++;
			if(pChan->IsDisabled()) uNumDisabled++;
		}

		PutStatus(Table);
		PutStatus("Total: " + CString(vChans.size()) + " - Joined: " + CString(uNumJoined) +
			" - Detached: " + CString(uNumDetached) + " - Disabled: " + CString(uNumDisabled));
	} else if (sCommand.Equals("ADDNETWORK")) {
		if (!m_pUser->IsAdmin() && !m_pUser->HasSpaceForNewNetwork()) {
			PutStatus("Network number limit reached. Ask an admin to increase the limit for you, or delete unneeded networks using /znc DelNetwork <name>");
			return;
		}

		CString sNetwork = sLine.Token(1);

		if (sNetwork.empty()) {
			PutStatus("Usage: AddNetwork <name>");
			return;
		}
		if (!CIRCNetwork::IsValidNetwork(sNetwork)) {
			PutStatus("Network name should be alphanumeric");
			return;
		}

		CString sNetworkAddError;
		if (m_pUser->AddNetwork(sNetwork, sNetworkAddError)) {
			PutStatus("Network added. Use /znc JumpNetwork " + sNetwork + ", or connect to ZNC with username " + m_pUser->GetUserName() + "/" + sNetwork + " (instead of just " + m_pUser->GetUserName() + ") to connect to it.");
		} else {
			PutStatus("Unable to add that network");
			PutStatus(sNetworkAddError);
		}
	} else if (sCommand.Equals("DELNETWORK")) {
		CString sNetwork = sLine.Token(1);

		if (sNetwork.empty()) {
			PutStatus("Usage: DelNetwork <name>");
			return;
		}

		if (m_pNetwork && m_pNetwork->GetName().Equals(sNetwork)) {
			SetNetwork(nullptr);
		}

		if (m_pUser->DeleteNetwork(sNetwork)) {
			PutStatus("Network deleted");
		} else {
			PutStatus("Failed to delete network");
			PutStatus("Perhaps this network doesn't exist");
		}
	} else if (sCommand.Equals("LISTNETWORKS")) {
		CUser *pUser = m_pUser;

		if (m_pUser->IsAdmin() && !sLine.Token(1).empty()) {
			pUser = CZNC::Get().FindUser(sLine.Token(1));

			if (!pUser) {
				PutStatus("User not found " + sLine.Token(1));
				return;
			}
		}

		const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();

		CTable Table;
		Table.AddColumn("Network");
		Table.AddColumn("OnIRC");
		Table.AddColumn("IRC Server");
		Table.AddColumn("IRC User");
		Table.AddColumn("Channels");

		for (const CIRCNetwork* pNetwork : vNetworks) {
			Table.AddRow();
			Table.SetCell("Network", pNetwork->GetName());
			if (pNetwork->IsIRCConnected()) {
				Table.SetCell("OnIRC", "Yes");
				Table.SetCell("IRC Server", pNetwork->GetIRCServer());
				Table.SetCell("IRC User", pNetwork->GetIRCNick().GetNickMask());
				Table.SetCell("Channels", CString(pNetwork->GetChans().size()));
			} else {
				Table.SetCell("OnIRC", "No");
			}
		}

		if (PutStatus(Table) == 0) {
			PutStatus("No networks");
		}
	} else if (sCommand.Equals("MOVENETWORK")) {
		if (!m_pUser->IsAdmin()) {
			PutStatus("Access Denied.");
			return;
		}

		CString sOldUser = sLine.Token(1);
		CString sOldNetwork = sLine.Token(2);
		CString sNewUser = sLine.Token(3);
		CString sNewNetwork = sLine.Token(4);

		if (sOldUser.empty() || sOldNetwork.empty() || sNewUser.empty()) {
			PutStatus("Usage: MoveNetwork <old user> <old network> <new user> [new network]");
			return;
		}
		if (sNewNetwork.empty()) {
			sNewNetwork = sOldNetwork;
		}

		CUser* pOldUser = CZNC::Get().FindUser(sOldUser);
		if (!pOldUser) {
			PutStatus("Old user [" + sOldUser + "] not found.");
			return;
		}

		CIRCNetwork* pOldNetwork = pOldUser->FindNetwork(sOldNetwork);
		if (!pOldNetwork) {
			PutStatus("Old network [" + sOldNetwork + "] not found.");
			return;
		}

		CUser* pNewUser = CZNC::Get().FindUser(sNewUser);
		if (!pNewUser) {
			PutStatus("New user [" + sOldUser + "] not found.");
			return;
		}

		if (pNewUser->FindNetwork(sNewNetwork)) {
			PutStatus("User [" + sNewUser + "] already has network [" + sNewNetwork + "].");
			return;
		}

		if (!CIRCNetwork::IsValidNetwork(sNewNetwork)) {
			PutStatus("Invalid network name [" + sNewNetwork + "]");
			return;
		}

		const CModules& vMods = pOldNetwork->GetModules();
		for (CModule* pMod : vMods) {
			CString sOldModPath = pOldNetwork->GetNetworkPath() + "/moddata/" + pMod->GetModName();
			CString sNewModPath = pNewUser->GetUserPath() + "/networks/" + sNewNetwork + "/moddata/" + pMod->GetModName();

			CDir oldDir(sOldModPath);
			for (CDir::iterator it = oldDir.begin(); it != oldDir.end(); ++it) {
				if ((*it)->GetShortName() != ".registry") {
					PutStatus("Some files seem to be in [" + sOldModPath + "]. You might want to move them to [" + sNewModPath + "]");
					break;
				}
			}

			pMod->MoveRegistry(sNewModPath);
		}

		CString sNetworkAddError;
		CIRCNetwork* pNewNetwork = pNewUser->AddNetwork(sNewNetwork, sNetworkAddError);

		if (!pNewNetwork) {
			PutStatus("Error adding network:" + sNetworkAddError);
			return;
		}

		pNewNetwork->Clone(*pOldNetwork, false);

		if (m_pNetwork && m_pNetwork->GetName().Equals(sOldNetwork) && m_pUser == pOldUser) {
			SetNetwork(nullptr);
		}

		if (pOldUser->DeleteNetwork(sOldNetwork)) {
			PutStatus("Success.");
		} else {
			PutStatus("Copied the network to new user, but failed to delete old network");
		}
	} else if (sCommand.Equals("JUMPNETWORK")) {
		CString sNetwork = sLine.Token(1);

		if (sNetwork.empty()) {
			PutStatus("No network supplied.");
			return;
		}

		if (m_pNetwork && (m_pNetwork->GetName() == sNetwork)) {
			PutStatus("You are already connected with this network.");
			return;
		}

		CIRCNetwork *pNetwork = m_pUser->FindNetwork(sNetwork);
		if (pNetwork) {
			PutStatus("Switched to " + sNetwork);
			SetNetwork(pNetwork);
		} else {
			PutStatus("You don't have a network named " + sNetwork);
		}
	} else if (sCommand.Equals("ADDSERVER")) {
		CString sServer = sLine.Token(1);

		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		if (sServer.empty()) {
			PutStatus("Usage: AddServer <host> [[+]port] [pass]");
			return;
		}

		if (m_pNetwork->AddServer(sLine.Token(1, true))) {
			PutStatus("Server added");
		} else {
			PutStatus("Unable to add that server");
			PutStatus("Perhaps the server is already added or openssl is disabled?");
		}
	} else if (sCommand.Equals("REMSERVER") || sCommand.Equals("DELSERVER")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sServer = sLine.Token(1);
		unsigned short uPort = sLine.Token(2).ToUShort();
		CString sPass = sLine.Token(3);

		if (sServer.empty()) {
			PutStatus("Usage: DelServer <host> [port] [pass]");
			return;
		}

		if (!m_pNetwork->HasServers()) {
			PutStatus("You don't have any servers added.");
			return;
		}

		if (m_pNetwork->DelServer(sServer, uPort, sPass)) {
			PutStatus("Server removed");
		} else {
			PutStatus("No such server");
		}
	} else if (sCommand.Equals("LISTSERVERS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		if (m_pNetwork->HasServers()) {
			const vector<CServer*>& vServers = m_pNetwork->GetServers();
			CServer* pCurServ = m_pNetwork->GetCurrentServer();
			CTable Table;
			Table.AddColumn("Host");
			Table.AddColumn("Port");
			Table.AddColumn("SSL");
			Table.AddColumn("Pass");

			for (const CServer* pServer : vServers) {
				Table.AddRow();
				Table.SetCell("Host", pServer->GetName() + (pServer == pCurServ ? "*" : ""));
				Table.SetCell("Port", CString(pServer->GetPort()));
				Table.SetCell("SSL", (pServer->IsSSL()) ? "SSL" : "");
				Table.SetCell("Pass", pServer->GetPass());
			}

			PutStatus(Table);
		} else {
			PutStatus("You don't have any servers added.");
		}
	} else if (sCommand.Equals("AddTrustedServerFingerprint")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}
		CString sFP = sLine.Token(1);
		if (sFP.empty()) {
			PutStatus("Usage: AddTrustedServerFingerprint <fi:ng:er>");
			return;
		}
		m_pNetwork->AddTrustedFingerprint(sFP);
		PutStatus("Done.");
	} else if (sCommand.Equals("DelTrustedServerFingerprint")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}
		CString sFP = sLine.Token(1);
		if (sFP.empty()) {
			PutStatus("Usage: DelTrustedServerFingerprint <fi:ng:er>");
			return;
		}
		m_pNetwork->DelTrustedFingerprint(sFP);
		PutStatus("Done.");
	} else if (sCommand.Equals("ListTrustedServerFingerprints")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}
		const SCString& ssFPs = m_pNetwork->GetTrustedFingerprints();
		if (ssFPs.empty()) {
			PutStatus("No fingerprints added.");
		} else {
			int k = 0;
			for (const CString& sFP : ssFPs) {
				PutStatus(CString(++k) + ". " + sFP);
			}
		}
	} else if (sCommand.Equals("TOPICS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		const vector<CChan*>& vChans = m_pNetwork->GetChans();
		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Set By");
		Table.AddColumn("Topic");

		for (const CChan* pChan : vChans) {
			Table.AddRow();
			Table.SetCell("Name", pChan->GetName());
			Table.SetCell("Set By", pChan->GetTopicOwner());
			Table.SetCell("Topic", pChan->GetTopic());
		}

		PutStatus(Table);
	} else if (sCommand.Equals("LISTMODS") || sCommand.Equals("LISTMODULES")) {
		if (m_pUser->IsAdmin()) {
			CModules& GModules = CZNC::Get().GetModules();

			if (!GModules.size()) {
				PutStatus("No global modules loaded.");
			} else {
				PutStatus("Global modules:");
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Arguments");

				for (const CModule* pMod : GModules) {
					GTable.AddRow();
					GTable.SetCell("Name", pMod->GetModName());
					GTable.SetCell("Arguments", pMod->GetArgs());
				}

				PutStatus(GTable);
			}
		}

		CModules& Modules = m_pUser->GetModules();

		if (!Modules.size()) {
			PutStatus("Your user has no modules loaded.");
		} else {
			PutStatus("User modules:");
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Arguments");

			for (const CModule* pMod : Modules) {
				Table.AddRow();
				Table.SetCell("Name", pMod->GetModName());
				Table.SetCell("Arguments", pMod->GetArgs());
			}

			PutStatus(Table);
		}

		if (m_pNetwork) {
			CModules& NetworkModules = m_pNetwork->GetModules();
			if (NetworkModules.empty()) {
				PutStatus("This network has no modules loaded.");
			} else {
				PutStatus("Network modules:");
				CTable Table;
				Table.AddColumn("Name");
				Table.AddColumn("Arguments");

				for (const CModule* pMod : NetworkModules) {
					Table.AddRow();
					Table.SetCell("Name", pMod->GetModName());
					Table.SetCell("Arguments", pMod->GetArgs());
				}

				PutStatus(Table);
			}
		}

		return;
	} else if (sCommand.Equals("LISTAVAILMODS") || sCommand.Equals("LISTAVAILABLEMODULES")) {
		if (m_pUser->DenyLoadMod()) {
			PutStatus("Access Denied.");
			return;
		}

		if (m_pUser->IsAdmin()) {
			set<CModInfo> ssGlobalMods;
			CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, CModInfo::GlobalModule);

			if (ssGlobalMods.empty()) {
				PutStatus("No global modules available.");
			} else {
				PutStatus("Global modules:");
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Description");

				for (const CModInfo& Info : ssGlobalMods) {
					GTable.AddRow();
					GTable.SetCell("Name", (CZNC::Get().GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
					GTable.SetCell("Description", Info.GetDescription().Ellipsize(128));
				}

				PutStatus(GTable);
			}
		}

		set<CModInfo> ssUserMods;
		CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

		if (ssUserMods.empty()) {
			PutStatus("No user modules available.");
		} else {
			PutStatus("User modules:");
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");

			for (const CModInfo& Info : ssUserMods) {
				Table.AddRow();
				Table.SetCell("Name", (m_pUser->GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			PutStatus(Table);
		}

		set<CModInfo> ssNetworkMods;
		CZNC::Get().GetModules().GetAvailableMods(ssNetworkMods, CModInfo::NetworkModule);

		if (ssNetworkMods.empty()) {
			PutStatus("No network modules available.");
		} else {
			PutStatus("Network modules:");
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");

			for (const CModInfo& Info : ssNetworkMods) {
				Table.AddRow();
				Table.SetCell("Name", ((m_pNetwork && m_pNetwork->GetModules().FindModule(Info.GetName())) ? "*" : " ") + Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			PutStatus(Table);
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
			PutStatus("Unable to load [" + sMod + "]: Access Denied.");
			return;
		}

		if (sMod.empty()) {
			PutStatus("Usage: LoadMod [--type=global|user|network] <module> [args]");
			return;
		}

		CModInfo ModInfo;
		CString sRetMsg;
		if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
			PutStatus("Unable to find modinfo [" + sMod + "] [" + sRetMsg + "]");
			return;
		}

		if (sType.Equals("default")) {
			eType = ModInfo.GetDefaultType();
		}

		if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
			PutStatus("Unable to load global module [" + sMod + "]: Access Denied.");
			return;
		}

		if (eType == CModInfo::NetworkModule && !m_pNetwork) {
			PutStatus("Unable to load network module [" + sMod + "] Not connected with a network.");
			return;
		}

		CString sModRet;
		bool b = false;

		switch (eType) {
		case CModInfo::GlobalModule:
			b = CZNC::Get().GetModules().LoadModule(sMod, sArgs, eType, nullptr, nullptr, sModRet);
			break;
		case CModInfo::UserModule:
			b = m_pUser->GetModules().LoadModule(sMod, sArgs, eType, m_pUser, nullptr, sModRet);
			break;
		case CModInfo::NetworkModule:
				b = m_pNetwork->GetModules().LoadModule(sMod, sArgs, eType, m_pUser, m_pNetwork, sModRet);
				break;
		default:
			sModRet = "Unable to load module [" + sMod + "]: Unknown module type";
		}

		if (b)
			sModRet = "Loaded module [" + sMod + "] " + sModRet;

		PutStatus(sModRet);
		return;
	} else if (sCommand.Equals("UNLOADMOD") || sCommand.Equals("UNLOADMODULE")) {
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
			PutStatus("Unable to unload [" + sMod + "] Access Denied.");
			return;
		}

		if (sMod.empty()) {
			PutStatus("Usage: UnloadMod [--type=global|user|network] <module>");
			return;
		}

		if (sType.Equals("default")) {
			CModInfo ModInfo;
			CString sRetMsg;
			if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
				PutStatus("Unable to find modinfo [" + sMod + "] [" + sRetMsg + "]");
				return;
			}

			eType = ModInfo.GetDefaultType();
		}

		if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
			PutStatus("Unable to unload global module [" + sMod + "]: Access Denied.");
			return;
		}

		if (eType == CModInfo::NetworkModule && !m_pNetwork) {
			PutStatus("Unable to unload network module [" + sMod + "] Not connected with a network.");
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
			sModRet = "Unable to unload module [" + sMod + "]: Unknown module type";
		}

		PutStatus(sModRet);
		return;
	} else if (sCommand.Equals("RELOADMOD") || sCommand.Equals("RELOADMODULE")) {
		CModInfo::EModuleType eType;
		CString sType = sLine.Token(1);
		CString sMod = sLine.Token(2);
		CString sArgs = sLine.Token(3, true);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to reload modules. Access Denied.");
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
			PutStatus("Usage: ReloadMod [--type=global|user|network] <module> [args]");
			return;
		}

		if (sType.Equals("default")) {
			CModInfo ModInfo;
			CString sRetMsg;
			if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
				PutStatus("Unable to find modinfo for [" + sMod + "] [" + sRetMsg + "]");
				return;
			}

			eType = ModInfo.GetDefaultType();
		}

		if (eType == CModInfo::GlobalModule && !m_pUser->IsAdmin()) {
			PutStatus("Unable to reload global module [" + sMod + "]: Access Denied.");
			return;
		}

		if (eType == CModInfo::NetworkModule && !m_pNetwork) {
			PutStatus("Unable to load network module [" + sMod + "] Not connected with a network.");
			return;
		}

		CString sModRet;

		switch (eType) {
		case CModInfo::GlobalModule:
			CZNC::Get().GetModules().ReloadModule(sMod, sArgs, nullptr, nullptr, sModRet);
			break;
		case CModInfo::UserModule:
			m_pUser->GetModules().ReloadModule(sMod, sArgs, m_pUser, nullptr, sModRet);
			break;
		case CModInfo::NetworkModule:
			m_pNetwork->GetModules().ReloadModule(sMod, sArgs, m_pUser, m_pNetwork, sModRet);
			break;
		default:
			sModRet = "Unable to reload module [" + sMod + "]: Unknown module type";
		}

		PutStatus(sModRet);
		return;
	} else if ((sCommand.Equals("UPDATEMOD") || sCommand.Equals("UPDATEMODULE")) && m_pUser->IsAdmin() ) {
		CString sMod = sLine.Token(1);

		if (sMod.empty()) {
			PutStatus("Usage: UpdateMod <module>");
			return;
		}

		PutStatus("Reloading [" + sMod + "] everywhere");
		if (CZNC::Get().UpdateModule(sMod)) {
			PutStatus("Done");
		} else {
			PutStatus("Done, but there were errors, [" + sMod + "] could not be loaded everywhere.");
		}
	} else if ((sCommand.Equals("ADDBINDHOST") || sCommand.Equals("ADDVHOST")) && m_pUser->IsAdmin()) {
		CString sHost = sLine.Token(1);

		if (sHost.empty()) {
			PutStatus("Usage: AddBindHost <host>");
			return;
		}

		if (CZNC::Get().AddBindHost(sHost)) {
			PutStatus("Done");
		} else {
			PutStatus("The host [" + sHost + "] is already in the list");
		}
	} else if ((sCommand.Equals("REMBINDHOST") || sCommand.Equals("DELBINDHOST") || sCommand.Equals("REMVHOST") || sCommand.Equals("DELVHOST")) && m_pUser->IsAdmin()) {
		CString sHost = sLine.Token(1);

		if (sHost.empty()) {
			PutStatus("Usage: DelBindHost <host>");
			return;
		}

		if (CZNC::Get().RemBindHost(sHost)) {
			PutStatus("Done");
		} else {
			PutStatus("The host [" + sHost + "] is not in the list");
		}
	} else if ((sCommand.Equals("LISTBINDHOSTS") || sCommand.Equals("LISTVHOSTS")) && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		const VCString& vsHosts = CZNC::Get().GetBindHosts();

		if (vsHosts.empty()) {
			PutStatus("No bind hosts configured");
			return;
		}

		CTable Table;
		Table.AddColumn("Host");

		for (const CString& sHost : vsHosts) {
			Table.AddRow();
			Table.SetCell("Host", sHost);
		}
		PutStatus(Table);
	} else if ((sCommand.Equals("SETBINDHOST") || sCommand.Equals("SETVHOST")) && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command. Try SetUserBindHost instead");
			return;
		}
		CString sArg = sLine.Token(1);

		if (sArg.empty()) {
			PutStatus("Usage: SetBindHost <host>");
			return;
		}

		if (sArg.Equals(m_pNetwork->GetBindHost())) {
			PutStatus("You already have this bind host!");
			return;
		}

		const VCString& vsHosts = CZNC::Get().GetBindHosts();
		if (!m_pUser->IsAdmin() && !vsHosts.empty()) {
			bool bFound = false;

			for (const CString& sHost : vsHosts) {
				if (sArg.Equals(sHost)) {
					bFound = true;
					break;
				}
			}

			if (!bFound) {
				PutStatus("You may not use this bind host. See [ListBindHosts] for a list");
				return;
			}
		}

		m_pNetwork->SetBindHost(sArg);
		PutStatus("Set bind host for network [" + m_pNetwork->GetName() + "] to [" + m_pNetwork->GetBindHost() + "]");
	} else if (sCommand.Equals("SETUSERBINDHOST") && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		CString sArg = sLine.Token(1);

		if (sArg.empty()) {
			PutStatus("Usage: SetUserBindHost <host>");
			return;
		}

		if (sArg.Equals(m_pUser->GetBindHost())) {
			PutStatus("You already have this bind host!");
			return;
		}

		const VCString& vsHosts = CZNC::Get().GetBindHosts();
		if (!m_pUser->IsAdmin() && !vsHosts.empty()) {
			bool bFound = false;

			for (const CString& sHost : vsHosts) {
				if (sArg.Equals(sHost)) {
					bFound = true;
					break;
				}
			}

			if (!bFound) {
				PutStatus("You may not use this bind host. See [ListBindHosts] for a list");
				return;
			}
		}

		m_pUser->SetBindHost(sArg);
		PutStatus("Set bind host to [" + m_pUser->GetBindHost() + "]");
	} else if ((sCommand.Equals("CLEARBINDHOST") || sCommand.Equals("CLEARVHOST")) && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command. Try ClearUserBindHost instead");
			return;
		}
		m_pNetwork->SetBindHost("");
		PutStatus("Bind host cleared for this network.");
	} else if (sCommand.Equals("CLEARUSERBINDHOST") && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		m_pUser->SetBindHost("");
		PutStatus("Bind host cleared for your user.");
	} else if (sCommand.Equals("SHOWBINDHOST")) {
		PutStatus("This user's default bind host " + (m_pUser->GetBindHost().empty() ? "not set" : "is [" + m_pUser->GetBindHost() + "]"));
		if (m_pNetwork) {
			PutStatus("This network's bind host " + (m_pNetwork->GetBindHost().empty() ? "not set" : "is [" + m_pNetwork->GetBindHost() + "]"));
		}
	} else if (sCommand.Equals("PLAYBUFFER")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sBuffer = sLine.Token(1);

		if (sBuffer.empty()) {
			PutStatus("Usage: PlayBuffer <#chan|query>");
			return;
		}

		if (m_pNetwork->IsChan(sBuffer)) {
			CChan* pChan = m_pNetwork->FindChan(sBuffer);

			if (!pChan) {
				PutStatus("You are not on [" + sBuffer + "]");
				return;
			}

			if (!pChan->IsOn()) {
				PutStatus("You are not on [" + sBuffer + "] [trying]");
				return;
			}

			if (pChan->GetBuffer().IsEmpty()) {
				PutStatus("The buffer for [" + sBuffer + "] is empty");
				return;
			}

			pChan->SendBuffer(this);
		} else {
			CQuery* pQuery = m_pNetwork->FindQuery(sBuffer);

			if (!pQuery) {
				PutStatus("No active query with [" + sBuffer + "]");
				return;
			}

			if (pQuery->GetBuffer().IsEmpty()) {
				PutStatus("The buffer for [" + sBuffer + "] is empty");
				return;
			}

			pQuery->SendBuffer(this);
		}
	} else if (sCommand.Equals("CLEARBUFFER")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sBuffer = sLine.Token(1);

		if (sBuffer.empty()) {
			PutStatus("Usage: ClearBuffer <#chan|query>");
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

		PutStatus("[" + CString(uMatches) + "] buffers matching [" + sBuffer + "] have been cleared");
	} else if (sCommand.Equals("CLEARALLCHANNELBUFFERS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		for (CChan* pChan : m_pNetwork->GetChans()) {
			pChan->ClearBuffer();
		}
		PutStatus("All channel buffers have been cleared");
	} else if (sCommand.Equals("CLEARALLQUERYBUFFERS")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		for (CQuery* pQuery : m_pNetwork->GetQueries()) {
			m_pNetwork->DelQuery(pQuery->GetName());
		}
		PutStatus("All query buffers have been cleared");
	} else if (sCommand.Equals("SETBUFFER")) {
		if (!m_pNetwork) {
			PutStatus("You must be connected with a network to use this command");
			return;
		}

		CString sBuffer = sLine.Token(1);

		if (sBuffer.empty()) {
			PutStatus("Usage: SetBuffer <#chan|query> [linecount]");
			return;
		}

		unsigned int uLineCount = sLine.Token(2).ToUInt();
		unsigned int uMatches = 0, uFail = 0;
		vector<CChan*> vChans = m_pNetwork->FindChans(sBuffer);
		for (CChan* pChan : vChans) {
			uMatches++;

			if (!pChan->SetBufferCount(uLineCount))
				uFail++;
		}

		vector<CQuery*> vQueries = m_pNetwork->FindQueries(sBuffer);
		for (CQuery* pQuery : vQueries) {
			uMatches++;

			if (!pQuery->SetBufferCount(uLineCount))
				uFail++;
		}

		PutStatus("BufferCount for [" + CString(uMatches - uFail) +
				"] buffer was set to [" + CString(uLineCount) + "]");
		if (uFail > 0) {
			PutStatus("Setting BufferCount failed for [" + CString(uFail) + "] buffers, "
					"max buffer count is " + CString(CZNC::Get().GetMaxBufferSize()));
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("TRAFFIC")) {
		CZNC::TrafficStatsPair Users, ZNC, Total;
		CZNC::TrafficStatsMap traffic = CZNC::Get().GetTrafficStats(Users, ZNC, Total);

		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("In");
		Table.AddColumn("Out");
		Table.AddColumn("Total");

		for (const auto& it : traffic) {
			Table.AddRow();
			Table.SetCell("Username", it.first);
			Table.SetCell("In", CString::ToByteStr(it.second.first));
			Table.SetCell("Out", CString::ToByteStr(it.second.second));
			Table.SetCell("Total", CString::ToByteStr(it.second.first + it.second.second));
		}

		Table.AddRow();
		Table.SetCell("Username", "<Users>");
		Table.SetCell("In", CString::ToByteStr(Users.first));
		Table.SetCell("Out", CString::ToByteStr(Users.second));
		Table.SetCell("Total", CString::ToByteStr(Users.first + Users.second));

		Table.AddRow();
		Table.SetCell("Username", "<ZNC>");
		Table.SetCell("In", CString::ToByteStr(ZNC.first));
		Table.SetCell("Out", CString::ToByteStr(ZNC.second));
		Table.SetCell("Total", CString::ToByteStr(ZNC.first + ZNC.second));

		Table.AddRow();
		Table.SetCell("Username", "<Total>");
		Table.SetCell("In", CString::ToByteStr(Total.first));
		Table.SetCell("Out", CString::ToByteStr(Total.second));
		Table.SetCell("Total", CString::ToByteStr(Total.first + Total.second));

		PutStatus(Table);
	} else if (sCommand.Equals("UPTIME")) {
		PutStatus("Running for " + CZNC::Get().GetUptime());
	} else if (m_pUser->IsAdmin() &&
			(sCommand.Equals("LISTPORTS") || sCommand.Equals("ADDPORT") || sCommand.Equals("DELPORT"))) {
		UserPortCommand(sLine);
	} else {
		PutStatus("Unknown command [" + sCommand + "] try 'Help'");
	}
}

void CClient::UserPortCommand(CString& sLine) {
	const CString sCommand = sLine.Token(0);

	if (sCommand.Equals("LISTPORTS")) {
		CTable Table;
		Table.AddColumn("Port");
		Table.AddColumn("BindHost");
		Table.AddColumn("SSL");
		Table.AddColumn("Proto");
		Table.AddColumn("IRC/Web");
		Table.AddColumn("URIPrefix");

		vector<CListener*>::const_iterator it;
		const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();

		for (const CListener* pListener : vpListeners) {
			Table.AddRow();
			Table.SetCell("Port", CString(pListener->GetPort()));
			Table.SetCell("BindHost", (pListener->GetBindHost().empty() ? CString("*") : pListener->GetBindHost()));
			Table.SetCell("SSL", CString(pListener->IsSSL()));

			EAddrType eAddr = pListener->GetAddrType();
			Table.SetCell("Proto", (eAddr == ADDR_ALL ? "All" : (eAddr == ADDR_IPV4ONLY ? "IPv4" : "IPv6")));

			CListener::EAcceptType eAccept = pListener->GetAcceptType();
			Table.SetCell("IRC/Web", (eAccept == CListener::ACCEPT_ALL ? "All" : (eAccept == CListener::ACCEPT_IRC ? "IRC" : "Web")));
			Table.SetCell("URIPrefix", pListener->GetURIPrefix() + "/");
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
			PutStatus("Usage: AddPort <[+]port> <ipv4|ipv6|all> <web|irc|all> [bindhost [uriprefix]]");
		} else {
			bool bSSL = (sPort.Left(1).Equals("+"));
			const CString sBindHost = sLine.Token(4);
			const CString sURIPrefix = sLine.Token(5);

			CListener* pListener = new CListener(uPort, sBindHost, sURIPrefix, bSSL, eAddr, eAccept);

			if (!pListener->Listen()) {
				delete pListener;
				PutStatus("Unable to bind [" + CString(strerror(errno)) + "]");
			} else {
				if (CZNC::Get().AddListener(pListener))
					PutStatus("Port Added");
				else
					PutStatus("Error?!");
			}
		}
	} else if (sCommand.Equals("DELPORT")) {
		if (sPort.empty() || sAddr.empty()) {
			PutStatus("Usage: DelPort <port> <ipv4|ipv6|all> [bindhost]");
		} else {
			const CString sBindHost = sLine.Token(3);

			CListener* pListener = CZNC::Get().FindListener(uPort, sBindHost, eAddr);

			if (pListener) {
				CZNC::Get().DelListener(pListener);
				PutStatus("Deleted Port");
			} else {
				PutStatus("Unable to find a matching port");
			}
		}
	}
}

static void AddCommandHelp(CTable& Table, const CString& sCmd, const CString& sArgs, const CString& sDesc, const CString& sFilter = "")
{
	if (sFilter.empty() || sCmd.StartsWith(sFilter) || sCmd.AsLower().WildCmp(sFilter.AsLower())) {
		Table.AddRow();
		Table.SetCell("Command", sCmd);
		Table.SetCell("Arguments", sArgs);
		Table.SetCell("Description", sDesc);
	}
}

void CClient::HelpUser(const CString& sFilter) {
	CTable Table;
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");

	if (sFilter.empty()) {
		PutStatus("In the following list all occurrences of <#chan> support wildcards (* and ?)");
		PutStatus("(Except ListNicks)");
	}

	AddCommandHelp(Table, "Version", "", "Print which version of ZNC this is", sFilter);

	AddCommandHelp(Table, "ListMods", "", "List all loaded modules", sFilter);
	AddCommandHelp(Table, "ListAvailMods", "", "List all available modules", sFilter);
	if (!m_pUser->IsAdmin()) { // If they are an admin we will add this command below with an argument
		AddCommandHelp(Table, "ListChans", "", "List all channels", sFilter);
	}
	AddCommandHelp(Table, "ListNicks", "<#chan>", "List all nicks on a channel", sFilter);
	if (!m_pUser->IsAdmin()) {
		AddCommandHelp(Table, "ListClients", "", "List all clients connected to your ZNC user", sFilter);
	}
	AddCommandHelp(Table, "ListServers", "", "List all servers of current IRC network", sFilter);

	AddCommandHelp(Table, "AddNetwork", "<name>", "Add a network to your user", sFilter);
	AddCommandHelp(Table, "DelNetwork", "<name>", "Delete a network from your user", sFilter);
	AddCommandHelp(Table, "ListNetworks", "", "List all networks", sFilter);
	if (m_pUser->IsAdmin()) {
		AddCommandHelp(Table, "MoveNetwork", "<old user> <old network> <new user> [new network]", "Move an IRC network from one user to another", sFilter);
	}
	AddCommandHelp(Table, "JumpNetwork", "<network>", "Jump to another network (Alternatively, you can connect to ZNC several times, using `user/network` as username)", sFilter);

	AddCommandHelp(Table, "AddServer", "<host> [[+]port] [pass]", "Add a server to the list of alternate/backup servers of current IRC network.", sFilter);
	AddCommandHelp(Table, "DelServer", "<host> [port] [pass]", "Remove a server from the list of alternate/backup servers of current IRC network", sFilter);

	AddCommandHelp(Table, "AddTrustedServerFingerprint", "<fi:ng:er>", "Add a trusted server SSL certificate fingerprint (SHA-256) to current IRC network.", sFilter);
	AddCommandHelp(Table, "DelTrustedServerFingerprint", "<fi:ng:er>", "Delete a trusted server SSL certificate from current IRC network.", sFilter);
	AddCommandHelp(Table, "ListTrustedServerFingerprints", "", "List all trusted server SSL certificates of current IRC network.", sFilter);

	AddCommandHelp(Table, "EnableChan", "<#chans>", "Enable channels", sFilter);
	AddCommandHelp(Table, "DisableChan", "<#chans>", "Disable channels", sFilter);
	AddCommandHelp(Table, "Detach", "<#chans>", "Detach from channels", sFilter);
	AddCommandHelp(Table, "Topics", "", "Show topics in all your channels", sFilter);

	AddCommandHelp(Table, "PlayBuffer", "<#chan|query>", "Play back the specified buffer", sFilter);
	AddCommandHelp(Table, "ClearBuffer", "<#chan|query>", "Clear the specified buffer", sFilter);
	AddCommandHelp(Table, "ClearAllChannelBuffers", "", "Clear the channel buffers", sFilter);
	AddCommandHelp(Table, "ClearAllQueryBuffers", "", "Clear the query buffers", sFilter);
	AddCommandHelp(Table, "SetBuffer", "<#chan|query> [linecount]", "Set the buffer count", sFilter);

	if (m_pUser->IsAdmin()) {
		AddCommandHelp(Table, "AddBindHost", "<host (IP preferred)>", "Adds a bind host for normal users to use", sFilter);
		AddCommandHelp(Table, "DelBindHost", "<host>", "Removes a bind host from the list", sFilter);
	}

	if (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost()) {
		AddCommandHelp(Table, "ListBindHosts", "", "Shows the configured list of bind hosts", sFilter);
		AddCommandHelp(Table, "SetBindHost", "<host (IP preferred)>", "Set the bind host for this connection", sFilter);
		AddCommandHelp(Table, "SetUserBindHost", "<host (IP preferred)>", "Set the default bind host for this user", sFilter);
		AddCommandHelp(Table, "ClearBindHost", "", "Clear the bind host for this connection", sFilter);
		AddCommandHelp(Table, "ClearUserBindHost", "", "Clear the default bind host for this user", sFilter);
	}

	AddCommandHelp(Table, "ShowBindHost", "", "Show currently selected bind host", sFilter);
	AddCommandHelp(Table, "Jump", "[server]", "Jump to the next or the specified server", sFilter);
	AddCommandHelp(Table, "Disconnect", "[message]", "Disconnect from IRC", sFilter);
	AddCommandHelp(Table, "Connect", "", "Reconnect to IRC", sFilter);
	AddCommandHelp(Table, "Uptime", "", "Show for how long ZNC has been running", sFilter);

	if (!m_pUser->DenyLoadMod()) {
		AddCommandHelp(Table, "LoadMod", "[--type=global|user|network] <module>", "Load a module", sFilter);
		AddCommandHelp(Table, "UnloadMod", "[--type=global|user|network] <module>", "Unload a module", sFilter);
		AddCommandHelp(Table, "ReloadMod", "[--type=global|user|network] <module>", "Reload a module", sFilter);
		if (m_pUser->IsAdmin()) {
			AddCommandHelp(Table, "UpdateMod", "<module>", "Reload a module everywhere", sFilter);
		}
	}

	AddCommandHelp(Table, "ShowMOTD", "", "Show ZNC's message of the day", sFilter);

	if (m_pUser->IsAdmin()) {
		AddCommandHelp(Table, "SetMOTD", "<message>", "Set ZNC's message of the day", sFilter);
		AddCommandHelp(Table, "AddMOTD", "<message>",  "Append <message> to ZNC's MOTD", sFilter);
		AddCommandHelp(Table, "ClearMOTD", "", "Clear ZNC's MOTD", sFilter);
		AddCommandHelp(Table, "ListPorts", "", "Show all active listeners", sFilter);
		AddCommandHelp(Table, "AddPort", "<[+]port> <ipv4|ipv6|all> <web|irc|all> [bindhost [uriprefix]]", "Add another port for ZNC to listen on", sFilter);
		AddCommandHelp(Table, "DelPort", "<port> <ipv4|ipv6|all> [bindhost]", "Remove a port from ZNC", sFilter);
		AddCommandHelp(Table, "Rehash", "", "Reload znc.conf from disk", sFilter);
		AddCommandHelp(Table, "SaveConfig", "", "Save the current settings to disk", sFilter);
		AddCommandHelp(Table, "ListUsers", "", "List all ZNC users and their connection status", sFilter);
		AddCommandHelp(Table, "ListAllUserNetworks", "", "List all ZNC users and their networks", sFilter);
		AddCommandHelp(Table, "ListChans", "[user <network>]", "List all channels", sFilter);
		AddCommandHelp(Table, "ListClients", "[user]", "List all connected clients", sFilter);
		AddCommandHelp(Table, "Traffic", "", "Show basic traffic stats for all ZNC users", sFilter);
		AddCommandHelp(Table, "Broadcast", "[message]", "Broadcast a message to all ZNC users", sFilter);
		AddCommandHelp(Table, "Shutdown", "[message]", "Shut down ZNC completely", sFilter);
		AddCommandHelp(Table, "Restart", "[message]", "Restart ZNC", sFilter);
	}

	if (Table.empty()) {
		PutStatus("No matches for '" + sFilter + "'");
	} else {
		PutStatus(Table);
	}
}
