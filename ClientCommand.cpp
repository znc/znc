/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Client.h"
#include "Chan.h"
#include "DCCBounce.h"
#include "DCCSock.h"
#include "IRCSock.h"
#include "Server.h"
#include "User.h"
#include "znc.h"

void CClient::UserCommand(CString& sLine) {
	if (!m_pUser) {
		return;
	}

	if (sLine.empty()) {
		return;
	}

	MODULECALL(OnStatusCommand(sLine), m_pUser, this, return);

	CString sCommand = sLine.Token(0);

	if (sCommand.Equals("HELP")) {
		HelpUser();
	} else if (sCommand.Equals("LISTNICKS")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: ListNicks <#chan>");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sChan);

		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}

		if (!pChan->IsOn()) {
			PutStatus("You are not on [" + sChan + "] [trying]");
			return;
		}

		const map<CString,CNick*>& msNicks = pChan->GetNicks();
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();
		const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

		if (!msNicks.size()) {
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

		for (map<CString,CNick*>::const_iterator a = msNicks.begin(); a != msNicks.end(); ++a) {
			Table.AddRow();

			for (unsigned int b = 0; b < sPerms.size(); b++) {
				if (a->second->HasPerm(sPerms[b])) {
					CString sPerm;
					sPerm += sPerms[b];
					Table.SetCell(sPerm, sPerm);
				}
			}

			Table.SetCell("Nick", a->second->GetNick());
			Table.SetCell("Ident", a->second->GetIdent());
			Table.SetCell("Host", a->second->GetHost());
		}

		PutStatus(Table);
	} else if (sCommand.Equals("DETACH")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: Detach <#chan>");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sChan);
		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}

		PutStatus("Detaching you from [" + sChan + "]");
		pChan->DetachUser();
	} else if (sCommand.Equals("VERSION")) {
		PutStatus(CZNC::GetTag());
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

		vector<CClient*>& vClients = pUser->GetClients();

		if (vClients.empty()) {
			PutStatus("No clients are connected");
			return;
		}

		CTable Table;
		Table.AddColumn("Host");

		for (unsigned int a = 0; a < vClients.size(); a++) {
			Table.AddRow();
			Table.SetCell("Host", vClients[a]->GetRemoteIP());
		}

		PutStatus(Table);
	} else if (m_pUser->IsAdmin() && sCommand.Equals("LISTUSERS")) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("Clients");
		Table.AddColumn("OnIRC");
		Table.AddColumn("IRC Server");
		Table.AddColumn("IRC User");
		Table.AddColumn("Channels");

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Username", it->first);
			Table.SetCell("Clients", CString(it->second->GetClients().size()));
			if (!it->second->IsIRCConnected()) {
				Table.SetCell("OnIRC", "No");
			} else {
				Table.SetCell("OnIRC", "Yes");
				Table.SetCell("IRC Server", it->second->GetIRCServer());
				Table.SetCell("IRC User", it->second->GetIRCNick().GetNickMask());
				Table.SetCell("Channels", CString(it->second->GetChans().size()));
			}
		}

		PutStatus(Table);
	} else if (m_pUser->IsAdmin() && sCommand.Equals("SetMOTD")) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: SetMOTD <Message>");
		} else {
			CZNC::Get().SetMotd(sMessage);
			PutStatus("MOTD set to [" + sMessage + "]");
		}
	} else if (m_pUser->IsAdmin() && sCommand.Equals("AddMOTD")) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: AddMOTD <Message>");
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
		if (!m_pUser->HasServers()) {
			PutStatus("You don't have any servers added.");
			return;
		}

		if (GetIRCSock()) {
			GetIRCSock()->Quit();
			PutStatus("Jumping to the next server in the list...");
		} else {
			PutStatus("Connecting...");
		}

		m_pUser->SetIRCConnectEnabled(true);
		m_pUser->CheckIRCConnect();
		return;
	} else if (sCommand.Equals("DISCONNECT")) {
		// GetIRCSock() is only set after the low level connection
		// to the IRC server was established. Before this we can
		// only find the IRC socket by its name.
		if (GetIRCSock()) {
			GetIRCSock()->Quit();
		} else {
			Csock* pIRCSock;
			CString sSockName = "IRC::" + m_pUser->GetUserName();
			// This is *slow*, we try to avoid doing this
			pIRCSock = CZNC::Get().GetManager().FindSockByName(sSockName);
			if (pIRCSock)
				pIRCSock->Close();
		}

		m_pUser->SetIRCConnectEnabled(false);
		PutStatus("Disconnected from IRC. Use 'connect' to reconnect.");
		return;
	} else if (sCommand.Equals("ENABLECHAN")) {
		CString sChan = sLine.Token(1, true);

		if (sChan.empty()) {
			PutStatus("Usage: EnableChan <channel>");
		} else {
			CChan* pChan = m_pUser->FindChan(sChan);
			if (!pChan) {
				PutStatus("Channel [" + sChan + "] not found.");
				return;
			}

			pChan->Enable();
			PutStatus("Channel [" + sChan + "] enabled.");
		}
	} else if (sCommand.Equals("LISTCHANS")) {
		CUser* pUser = m_pUser;
		const CString sNick = sLine.Token(1);

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
		}

		const vector<CChan*>& vChans = pUser->GetChans();
		CIRCSock* pIRCSock = pUser->GetIRCSock();
		const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

		if (!vChans.size()) {
			PutStatus("There are no channels defined.");
			return;
		}

		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Status");
		Table.AddColumn("Conf");
		Table.AddColumn("Buf");
		Table.AddColumn("Modes");
		Table.AddColumn("Users");

		for (unsigned int p = 0; p < sPerms.size(); p++) {
			CString sPerm;
			sPerm += sPerms[p];
			Table.AddColumn(sPerm);
		}

		unsigned int uNumDetached = 0, uNumDisabled = 0,
			uNumJoined = 0;

		for (unsigned int a = 0; a < vChans.size(); a++) {
			const CChan* pChan = vChans[a];
			Table.AddRow();
			Table.SetCell("Name", pChan->GetPermStr() + pChan->GetName());
			Table.SetCell("Status", ((vChans[a]->IsOn()) ? ((vChans[a]->IsDetached()) ? "Detached" : "Joined") : ((vChans[a]->IsDisabled()) ? "Disabled" : "Trying")));
			Table.SetCell("Conf", CString((pChan->InConfig()) ? "yes" : ""));
			Table.SetCell("Buf", CString((pChan->KeepBuffer()) ? "*" : "") + CString(pChan->GetBufferCount()));
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
	} else if (sCommand.Equals("ADDSERVER")) {
		CString sServer = sLine.Token(1);

		if (sServer.empty()) {
			PutStatus("Usage: AddServer <host> [[+]port] [pass]");
			return;
		}

		if (m_pUser->AddServer(sLine.Token(1, true))) {
			PutStatus("Server added");
		} else {
			PutStatus("Unable to add that server");
			PutStatus("Perhaps the server is already added or openssl is disabled?");
		}
	} else if (sCommand.Equals("REMSERVER") || sCommand.Equals("DELSERVER")) {
		CString sServer = sLine.Token(1);
		unsigned short uPort = sLine.Token(2).ToUShort();
		CString sPass = sLine.Token(3);

		if (sServer.empty()) {
			PutStatus("Usage: RemServer <host> [port] [pass]");
			return;
		}

		if (!m_pUser->HasServers()) {
			PutStatus("You don't have any servers added.");
			return;
		}

		if (m_pUser->DelServer(sServer, uPort, sPass)) {
			PutStatus("Server removed");
		} else {
			PutStatus("No such server");
		}
	} else if (sCommand.Equals("LISTSERVERS")) {
		if (m_pUser->HasServers()) {
			const vector<CServer*>& vServers = m_pUser->GetServers();
			CServer* pCurServ = m_pUser->GetCurrentServer();
			CTable Table;
			Table.AddColumn("Host");
			Table.AddColumn("Port");
			Table.AddColumn("SSL");
			Table.AddColumn("Pass");

			for (unsigned int a = 0; a < vServers.size(); a++) {
				CServer* pServer = vServers[a];
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
	} else if (sCommand.Equals("TOPICS")) {
		const vector<CChan*>& vChans = m_pUser->GetChans();
		CTable Table;
		Table.AddColumn("Name");
		Table.AddColumn("Set By");
		Table.AddColumn("Topic");

		for (unsigned int a = 0; a < vChans.size(); a++) {
			CChan* pChan = vChans[a];
			Table.AddRow();
			Table.SetCell("Name", pChan->GetName());
			Table.SetCell("Set By", pChan->GetTopicOwner());
			Table.SetCell("Topic", pChan->GetTopic());
		}

		PutStatus(Table);
	} else if (sCommand.Equals("SEND")) {
		CString sToNick = sLine.Token(1);
		CString sFile = sLine.Token(2);
		CString sAllowedPath = m_pUser->GetDLPath();
		CString sAbsolutePath;

		if ((sToNick.empty()) || (sFile.empty())) {
			PutStatus("Usage: Send <nick> <file>");
			return;
		}

		sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

		if (sAbsolutePath.empty()) {
			PutStatus("Illegal path.");
			return;
		}

		m_pUser->SendFile(sToNick, sFile);
	} else if (sCommand.Equals("GET")) {
		CString sFile = sLine.Token(1);
		CString sAllowedPath = m_pUser->GetDLPath();
		CString sAbsolutePath;

		if (sFile.empty()) {
			PutStatus("Usage: Get <file>");
			return;
		}

		sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

		if (sAbsolutePath.empty()) {
			PutStatus("Illegal path.");
			return;
		}

		m_pUser->SendFile(GetNick(), sFile);
	} else if (sCommand.Equals("LISTDCCS")) {
		CSockManager& Manager = CZNC::Get().GetManager();

		CTable Table;
		Table.AddColumn("Type");
		Table.AddColumn("State");
		Table.AddColumn("Speed");
		Table.AddColumn("Nick");
		Table.AddColumn("IP");
		Table.AddColumn("File");

		for (unsigned int a = 0; a < Manager.size(); a++) {
			CString sSockName = Manager[a]->GetSockName();

			if (sSockName.TrimPrefix("DCC::")) {
				if (sSockName.Equals("XFER::REMOTE::", false, 14)) {
					continue;
				}

				if (sSockName.Equals("CHAT::REMOTE::", false, 14)) {
					continue;
				}

				if (sSockName.Equals("SEND", false, 4)) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Sending");
					Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
					Table.SetCell("Speed", CString((int)(pSock->GetAvgWrite() / 1024.0)) + " KiB/s");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (sSockName.Equals("GET", false, 3)) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Getting");
					Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
					Table.SetCell("Speed", CString((int)(pSock->GetAvgRead() / 1024.0)) + " KiB/s");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (sSockName.Equals("LISTEN", false, 6)) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Sending");
					Table.SetCell("State", "Waiting");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (sSockName.Equals("XFER::LOCAL", false, 11)) {
					CDCCBounce* pSock = (CDCCBounce*) Manager[a];

					CString sState = "Waiting";
					if ((pSock->IsConnected()) || (pSock->IsPeerConnected())) {
						sState = "Halfway";
						if ((pSock->IsPeerConnected()) && (pSock->IsPeerConnected())) {
							sState = "Connected";
						}
					}

					Table.AddRow();
					Table.SetCell("Type", "Xfer");
					Table.SetCell("State", sState);
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (sSockName.Equals("CHAT::LOCAL", false, 11)) {
					CDCCBounce* pSock = (CDCCBounce*) Manager[a];

					CString sState = "Waiting";
					if ((pSock->IsConnected()) || (pSock->IsPeerConnected())) {
						sState = "Halfway";
						if ((pSock->IsPeerConnected()) && (pSock->IsPeerConnected())) {
							sState = "Connected";
						}
					}

					Table.AddRow();
					Table.SetCell("Type", "Chat");
					Table.SetCell("State", sState);
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
				}
			}
		}

		if (PutStatus(Table) == 0) {
			PutStatus("You have no active DCCs.");
		}
	} else if (sCommand.Equals("LISTMODS") || sCommand.Equals("LISTMODULES")) {
#ifdef _MODULES
		if (m_pUser->IsAdmin()) {
			CModules& GModules = CZNC::Get().GetModules();

			if (!GModules.size()) {
				PutStatus("No global modules loaded.");
			} else {
				PutStatus("Global modules:");
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Arguments");

				for (unsigned int b = 0; b < GModules.size(); b++) {
					GTable.AddRow();
					GTable.SetCell("Name", GModules[b]->GetModName());
					GTable.SetCell("Arguments", GModules[b]->GetArgs());
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

			for (unsigned int b = 0; b < Modules.size(); b++) {
				Table.AddRow();
				Table.SetCell("Name", Modules[b]->GetModName());
				Table.SetCell("Arguments", Modules[b]->GetArgs());
			}

			PutStatus(Table);
		}
#else
		PutStatus("Modules are not enabled.");
#endif
		return;
	} else if (sCommand.Equals("LISTAVAILMODS") || sCommand.Equals("LISTAVAILABLEMODULES")) {
#ifdef _MODULES
		if (m_pUser->DenyLoadMod()) {
			PutStatus("Access Denied.");
			return;
		}

		if (m_pUser->IsAdmin()) {
			set<CModInfo> ssGlobalMods;
			CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, true);

			if (ssGlobalMods.empty()) {
				PutStatus("No global modules available.");
			} else {
				PutStatus("Global modules:");
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Description");
				set<CModInfo>::iterator it;

				for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); ++it) {
					const CModInfo& Info = *it;
					GTable.AddRow();
					GTable.SetCell("Name", (CZNC::Get().GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
					GTable.SetCell("Description", Info.GetDescription().Ellipsize(128));
				}

				PutStatus(GTable);
			}
		}

		set<CModInfo> ssUserMods;
		CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

		if (!ssUserMods.size()) {
			PutStatus("No user modules available.");
		} else {
			PutStatus("User modules:");
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");
			set<CModInfo>::iterator it;

			for (it = ssUserMods.begin(); it != ssUserMods.end(); ++it) {
				const CModInfo& Info = *it;
				Table.AddRow();
				Table.SetCell("Name", (m_pUser->GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			PutStatus(Table);
		}
#else
		PutStatus("Modules are not enabled.");
#endif
		return;
	} else if (sCommand.Equals("LOADMOD") || sCommand.Equals("LOADMODULE")) {
		CString sMod;
		CString sArgs;

		sMod = sLine.Token(1);
		sArgs = sLine.Token(2, true);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to load [" + sMod + "] Access Denied.");
			return;
		}

		if (sMod.empty()) {
			PutStatus("Usage: LoadMod <module> [args]");
			return;
		}

#ifdef _MODULES
		CModInfo ModInfo;
		CString sRetMsg;
		if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
			PutStatus("Unable to find modinfo [" + sMod + "] [" + sRetMsg + "]");
			return;
		}

		bool bGlobal = ModInfo.IsGlobal();

		if (bGlobal && !m_pUser->IsAdmin()) {
			PutStatus("Unable to load global module [" + sMod + "] Access Denied.");
			return;
		}

		CString sModRet;
		bool b;

		if (bGlobal) {
			b = CZNC::Get().GetModules().LoadModule(sMod, sArgs, NULL, sModRet);
		} else {
			b = m_pUser->GetModules().LoadModule(sMod, sArgs, m_pUser, sModRet);
		}

		if (!b) {
			PutStatus(sModRet);
			return;
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to load [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if (sCommand.Equals("UNLOADMOD") || sCommand.Equals("UNLOADMODULE")) {
		CString sMod;
		sMod = sLine.Token(1);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to unload [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: UnloadMod <module>");
			return;
		}

		CString sModRet;
		bool b;

		// First, try to unload the user module
		b = m_pUser->GetModules().UnloadModule(sMod, sModRet);
		if (!b && m_pUser->IsAdmin()) {
			// If that failed and the user is an admin, try to unload a global module
			b = CZNC::Get().GetModules().UnloadModule(sMod, sModRet);
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to unload [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if (sCommand.Equals("RELOADMOD") || sCommand.Equals("RELOADMODULE")) {
		CString sMod;
		CString sArgs;

		sMod = sLine.Token(1);
		sArgs = sLine.Token(2, true);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to reload modules. Access Denied.");
			return;
		}

		if (sMod.empty()) {
			PutStatus("Usage: ReloadMod <module> [args]");
			return;
		}

#ifdef _MODULES
		CModInfo ModInfo;
		CString sRetMsg;
		if (!CZNC::Get().GetModules().GetModInfo(ModInfo, sMod, sRetMsg)) {
			PutStatus("Unable to find modinfo for [" + sMod + "] [" + sRetMsg + "]");
			return;
		}

		bool bGlobal = ModInfo.IsGlobal();

		if (bGlobal && !m_pUser->IsAdmin()) {
			PutStatus("Unable to reload global module [" + sMod + "] Access Denied.");
			return;
		}

		CString sModRet;

		if (bGlobal) {
			CZNC::Get().GetModules().ReloadModule(sMod, sArgs, NULL, sModRet);
		} else {
			m_pUser->GetModules().ReloadModule(sMod, sArgs, m_pUser, sModRet);
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to unload [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.Equals("UPDATEMOD") || sCommand.Equals("UPDATEMODULE")) && m_pUser->IsAdmin() ) {
#ifndef _MODULES
		PutStatus("Modules are not enabled.");
#else
		CString sMod = sLine.Token(1);

		if (sMod.empty()) {
			PutStatus("Usage: UpdateMod <module>");
			return;
		}

		if (m_pUser->DenyLoadMod() || !m_pUser->IsAdmin()) {
			PutStatus("Unable to reload [" + sMod + "] Access Denied.");
			return;
		}

		PutStatus("Reloading [" + sMod + "] on all users...");
		if (CUser::UpdateModule(sMod)) {
			PutStatus("Done");
		} else {
			PutStatus("Done, but there were errors, some users no longer have ["
					+ sMod + "] loaded");
		}
#endif
	} else if (sCommand.Equals("ADDVHOST") && m_pUser->IsAdmin()) {
		CString sVHost = sLine.Token(1);

		if (sVHost.empty()) {
			PutStatus("Usage: AddVHost <VHost>");
			return;
		}

		if (CZNC::Get().AddVHost(sVHost)) {
			PutStatus("Done");
		} else {
			PutStatus("The VHost [" + sVHost + "] is already in the list");
		}
	} else if ((sCommand.Equals("REMVHOST") || sCommand.Equals("DELVHOST")) && m_pUser->IsAdmin()) {
		CString sVHost = sLine.Token(1);

		if (sVHost.empty()) {
			PutStatus("Usage: RemVHost <VHost>");
			return;
		}

		if (CZNC::Get().RemVHost(sVHost)) {
			PutStatus("Done");
		} else {
			PutStatus("The VHost [" + sVHost + "] is not in the list");
		}
	} else if (sCommand.Equals("LISTVHOSTS") && (m_pUser->IsAdmin() || !m_pUser->DenySetVHost())) {
		const VCString& vsVHosts = CZNC::Get().GetVHosts();

		if (vsVHosts.empty()) {
			PutStatus("No VHosts configured");
			return;
		}

		CTable Table;
		Table.AddColumn("VHost");

		VCString::const_iterator it;
		for (it = vsVHosts.begin(); it != vsVHosts.end(); ++it) {
			Table.AddRow();
			Table.SetCell("VHost", *it);
		}
		PutStatus(Table);
	} else if (sCommand.Equals("SETVHOST") && (m_pUser->IsAdmin() || !m_pUser->DenySetVHost())) {
		CString sVHost = sLine.Token(1);

		if (sVHost.empty()) {
			PutStatus("Usage: SetVHost <VHost>");
			return;
		}

		if (sVHost.Equals(m_pUser->GetVHost())) {
			PutStatus("You already have this VHost!");
			return;
		}

		const VCString& vsVHosts = CZNC::Get().GetVHosts();
		if (!m_pUser->IsAdmin() && !vsVHosts.empty()) {
			VCString::const_iterator it;
			bool bFound = false;

			for (it = vsVHosts.begin(); it != vsVHosts.end(); ++it) {
				if (sVHost.Equals(*it)) {
					bFound = true;
					break;
				}
			}

			if (!bFound) {
				PutStatus("You may not use this VHost. See [ListVHosts] for a list");
				return;
			}
		}

		m_pUser->SetVHost(sVHost);
		PutStatus("Set VHost to [" + m_pUser->GetVHost() + "]");
	} else if (sCommand.Equals("CLEARVHOST") && (m_pUser->IsAdmin() || !m_pUser->DenySetVHost())) {
		m_pUser->SetVHost("");
		PutStatus("VHost Cleared");
	} else if (sCommand.Equals("PLAYBUFFER")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: PlayBuffer <#chan>");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sChan);

		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}

		if (!pChan->IsOn()) {
			PutStatus("You are not on [" + sChan + "] [trying]");
			return;
		}

		if (pChan->GetBuffer().empty()) {
			PutStatus("The buffer for [" + sChan + "] is empty");
			return;
		}

		pChan->SendBuffer(this);
	} else if (sCommand.Equals("CLEARBUFFER")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: ClearBuffer <#chan>");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sChan);

		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}

		if (!pChan->IsOn()) {
			PutStatus("You are not on [" + sChan + "] [trying]");
			return;
		}

		pChan->ClearBuffer();
		PutStatus("The buffer for [" + sChan + "] has been cleared");
	} else if (sCommand.Equals("CLEARALLCHANNELBUFFERS")) {
		vector<CChan*>::const_iterator it;
		const vector<CChan*>& vChans = m_pUser->GetChans();

		for (it = vChans.begin(); it != vChans.end(); ++it) {
			(*it)->ClearBuffer();
		}
		PutStatus("All channel buffers have been cleared");
	} else if (sCommand.Equals("SETBUFFER")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: SetBuffer <#chan> [linecount]");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sChan);

		if (!pChan) {
			PutStatus("You are not on [" + sChan + "]");
			return;
		}


		unsigned int uLineCount = sLine.Token(2).ToUInt();

		if (uLineCount > 500) {
			PutStatus("Max linecount is 500.");
			return;
		}

		pChan->SetBufferCount(uLineCount);

		PutStatus("BufferCount for [" + sChan + "] set to [" + CString(pChan->GetBufferCount()) + "]");
	} else if (m_pUser->IsAdmin() && sCommand.Equals("TRAFFIC")) {
		CZNC::TrafficStatsPair Users, ZNC, Total;
		CZNC::TrafficStatsMap traffic = CZNC::Get().GetTrafficStats(Users, ZNC, Total);
		CZNC::TrafficStatsMap::const_iterator it;

		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("In");
		Table.AddColumn("Out");
		Table.AddColumn("Total");

		for (it = traffic.begin(); it != traffic.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Username", it->first);
			Table.SetCell("In", CString::ToByteStr(it->second.first));
			Table.SetCell("Out", CString::ToByteStr(it->second.second));
			Table.SetCell("Total", CString::ToByteStr(it->second.first + it->second.second));
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
	} else {
		PutStatus("Unknown command [" + sCommand + "] try 'Help'");
	}
}

void CClient::HelpUser() {
	CTable Table;
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");

	Table.AddRow();
	Table.SetCell("Command", "Version");
	Table.SetCell("Description", "Print which version of ZNC this is");

	Table.AddRow();
	Table.SetCell("Command", "ListDCCs");
	Table.SetCell("Description", "List all active DCCs");

	Table.AddRow();
	Table.SetCell("Command", "ListMods");
	Table.SetCell("Description", "List all loaded modules");

	Table.AddRow();
	Table.SetCell("Command", "ListAvailMods");
	Table.SetCell("Description", "List all available modules");

	if (!m_pUser->IsAdmin()) { // If they are an admin we will add this command below with an argument
		Table.AddRow();
		Table.SetCell("Command", "ListChans");
		Table.SetCell("Description", "List all channels");
	}

	Table.AddRow();
	Table.SetCell("Command", "ListNicks");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "List all nicks on a channel");

	if (!m_pUser->IsAdmin()) {
		Table.AddRow();
		Table.SetCell("Command", "ListClients");
		Table.SetCell("Description", "List all clients connected to your ZNC user");
	}

	Table.AddRow();
	Table.SetCell("Command", "ListServers");
	Table.SetCell("Description", "List all servers");

	Table.AddRow();
	Table.SetCell("Command", "AddServer");
	Table.SetCell("Arguments", "<host> [[+]port] [pass]");
	Table.SetCell("Description", "Add a server to the list");

	Table.AddRow();
	Table.SetCell("Command", "RemServer");
	Table.SetCell("Arguments", "<host> [port] [pass]");
	Table.SetCell("Description", "Remove a server from the list");

	Table.AddRow();
	Table.SetCell("Command", "Enablechan");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "Enable the channel");

	Table.AddRow();
	Table.SetCell("Command", "Detach");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "Detach from the channel");

	Table.AddRow();
	Table.SetCell("Command", "Topics");
	Table.SetCell("Description", "Show topics in all your channels");

	Table.AddRow();
	Table.SetCell("Command", "PlayBuffer");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "Play back the buffer for a given channel");

	Table.AddRow();
	Table.SetCell("Command", "ClearBuffer");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "Clear the buffer for a given channel");

	Table.AddRow();
	Table.SetCell("Command", "ClearAllChannelBuffers");
	Table.SetCell("Description", "Clear the channel buffers");

	Table.AddRow();
	Table.SetCell("Command", "SetBuffer");
	Table.SetCell("Arguments", "<#chan> [linecount]");
	Table.SetCell("Description", "Set the buffer count for a channel");

	if (m_pUser->IsAdmin()) {
		Table.AddRow();
		Table.SetCell("Command", "AddVHost");
		Table.SetCell("Arguments", "<vhost (IP preferred)>");
		Table.SetCell("Description", "Adds a VHost for normal users to use");

		Table.AddRow();
		Table.SetCell("Command", "RemVHost");
		Table.SetCell("Arguments", "<vhost>");
		Table.SetCell("Description", "Removes a VHost from the list");
	}

	if (m_pUser->IsAdmin() || !m_pUser->DenySetVHost()) {
		Table.AddRow();
		Table.SetCell("Command", "ListVHosts");
		Table.SetCell("Description", "Shows the configured list of vhosts");

		Table.AddRow();
		Table.SetCell("Command", "SetVHost");
		Table.SetCell("Arguments", "<vhost (IP preferred)>");
		Table.SetCell("Description", "Set the VHost for this connection");

		Table.AddRow();
		Table.SetCell("Command", "ClearVHost");
		Table.SetCell("Description", "Clear the VHost for this connection");
	}

	Table.AddRow();
	Table.SetCell("Command", "Jump");
	Table.SetCell("Description", "Jump to the next server in the list");

	Table.AddRow();
	Table.SetCell("Command", "Disconnect");
	Table.SetCell("Description", "Disconnect from IRC");

	Table.AddRow();
	Table.SetCell("Command", "Connect");
	Table.SetCell("Description", "Reconnect to IRC");

	Table.AddRow();
	Table.SetCell("Command", "Send");
	Table.SetCell("Arguments", "<nick> <file>");
	Table.SetCell("Description", "Send a shell file to a nick on IRC");

	Table.AddRow();
	Table.SetCell("Command", "Get");
	Table.SetCell("Arguments", "<file>");
	Table.SetCell("Description", "Send a shell file to yourself");

	Table.AddRow();
	Table.SetCell("Command", "Uptime");
	Table.SetCell("Description", "Show for how long ZNC has been running");

	if (!m_pUser->DenyLoadMod()) {
		Table.AddRow();
		Table.SetCell("Command", "LoadMod");
		Table.SetCell("Arguments", "<module>");
		Table.SetCell("Description", "Load a module");

		Table.AddRow();
		Table.SetCell("Command", "UnloadMod");
		Table.SetCell("Arguments", "<module>");
		Table.SetCell("Description", "Unload a module");

		Table.AddRow();
		Table.SetCell("Command", "ReloadMod");
		Table.SetCell("Arguments", "<module>");
		Table.SetCell("Description", "Reload a module");

		if (m_pUser->IsAdmin()) {
			Table.AddRow();
			Table.SetCell("Command", "UpdateMod");
			Table.SetCell("Arguments", "<module>");
			Table.SetCell("Description", "Reload a module on all users");
		}
	}

	Table.AddRow();
	Table.SetCell("Command", "ShowMOTD");
	Table.SetCell("Description", "Show ZNC's message of the day");

	if (m_pUser->IsAdmin()) {
		Table.AddRow();
		Table.SetCell("Command", "SetMOTD");
		Table.SetCell("Arguments", "<Message>");
		Table.SetCell("Description", "Set ZNC's message of the day");

		Table.AddRow();
		Table.SetCell("Command", "AddMOTD");
		Table.SetCell("Arguments", "<Message>");
		Table.SetCell("Description", "Append <Message> to ZNC's MOTD");

		Table.AddRow();
		Table.SetCell("Command", "ClearMOTD");
		Table.SetCell("Description", "Clear ZNC's MOTD");

		Table.AddRow();
		Table.SetCell("Command", "Rehash");
		Table.SetCell("Description", "Reload znc.conf from disk");

		Table.AddRow();
		Table.SetCell("Command", "SaveConfig");
		Table.SetCell("Description", "Save the current settings to disk");

		Table.AddRow();
		Table.SetCell("Command", "ListUsers");
		Table.SetCell("Description", "List all ZNC users and their connection status");

		Table.AddRow();
		Table.SetCell("Command", "ListChans");
		Table.SetCell("Arguments", "[User]");
		Table.SetCell("Description", "List all channels");

		Table.AddRow();
		Table.SetCell("Command", "ListClients");
		Table.SetCell("Arguments", "[User]");
		Table.SetCell("Description", "List all connected clients");

		Table.AddRow();
		Table.SetCell("Command", "Traffic");
		Table.SetCell("Description", "Show basic traffic stats for all ZNC users");

		Table.AddRow();
		Table.SetCell("Command", "Broadcast");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Broadcast a message to all ZNC users");

		Table.AddRow();
		Table.SetCell("Command", "Shutdown");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Shut down ZNC completely");

		Table.AddRow();
		Table.SetCell("Command", "Restart");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Restart ZNC");
	}

	PutStatus(Table);
}
