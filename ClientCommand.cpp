/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Client.h"
#include "Chan.h"
#include "FileUtils.h"
#include "IRCSock.h"
#include "Listener.h"
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

	const CString sCommand = sLine.Token(0);

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

		const map<CString,CNick>& msNicks = pChan->GetNicks();
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

		for (map<CString,CNick>::const_iterator a = msNicks.begin(); a != msNicks.end(); ++a) {
			Table.AddRow();

			for (unsigned int b = 0; b < sPerms.size(); b++) {
				if (a->second.HasPerm(sPerms[b])) {
					CString sPerm;
					sPerm += sPerms[b];
					Table.SetCell(sPerm, sPerm);
				}
			}

			Table.SetCell("Nick", a->second.GetNick());
			Table.SetCell("Ident", a->second.GetIdent());
			Table.SetCell("Host", a->second.GetHost());
		}

		PutStatus(Table);
	} else if (sCommand.Equals("DETACH")) {
		CString sChan = sLine.Token(1);

		if (sChan.empty()) {
			PutStatus("Usage: Detach <#chan>");
			return;
		}

		const vector<CChan*>& vChans = m_pUser->GetChans();
		vector<CChan*>::const_iterator it;
		unsigned int uMatches = 0, uDetached = 0;
		for (it = vChans.begin(); it != vChans.end(); ++it) {
			if (!(*it)->GetName().WildCmp(sChan))
				continue;
			uMatches++;

			if ((*it)->IsDetached())
				continue;
			uDetached++;
			(*it)->DetachUser();
		}

		PutStatus("There were [" + CString(uMatches) + "] channels matching [" + sChan + "]");
		PutStatus("Detached [" + CString(uDetached) + "] channels");
	} else if (sCommand.Equals("VERSION")) {
		const char *features = "IPv6: "
#ifdef HAVE_IPV6
			"yes"
#else
			"no"
#endif
			", SSL: "
#ifdef HAVE_LIBSSL
			"yes"
#else
			"no"
#endif
			", c-ares: "
#ifdef HAVE_C_ARES
			"yes";
#else
			"no";
#endif
		PutStatus(CZNC::GetTag());
		PutStatus(features);
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

		CString sArgs = sLine.Token(1, true);
		CServer *pServer = NULL;

		if (!sArgs.empty()) {
			pServer = m_pUser->FindServer(sArgs);
			if (!pServer) {
				PutStatus("Server [" + sArgs + "] not found");
				return;
			}
			m_pUser->SetNextServer(pServer);

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

		m_pUser->SetIRCConnectEnabled(true);
		m_pUser->CheckIRCConnect();
		return;
	} else if (sCommand.Equals("DISCONNECT")) {
		if (GetIRCSock()) {
			CString sQuitMsg = sLine.Token(1, true);
			GetIRCSock()->Quit(sQuitMsg);
		}

		m_pUser->SetIRCConnectEnabled(false);
		PutStatus("Disconnected from IRC. Use 'connect' to reconnect.");
		return;
	} else if (sCommand.Equals("ENABLECHAN")) {
		CString sChan = sLine.Token(1, true);

		if (sChan.empty()) {
			PutStatus("Usage: EnableChan <channel>");
		} else {
			const vector<CChan*>& vChans = m_pUser->GetChans();
			vector<CChan*>::const_iterator it;
			unsigned int uMatches = 0, uEnabled = 0;
			for (it = vChans.begin(); it != vChans.end(); ++it) {
				if (!(*it)->GetName().WildCmp(sChan))
					continue;
				uMatches++;

				if (!(*it)->IsDisabled())
					continue;
				uEnabled++;
				(*it)->Enable();
			}

			PutStatus("There were [" + CString(uMatches) + "] channels matching [" + sChan + "]");
			PutStatus("Enabled [" + CString(uEnabled) + "] channels");
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
		return;
	} else if (sCommand.Equals("LISTAVAILMODS") || sCommand.Equals("LISTAVAILABLEMODULES")) {
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

		if (b)
			sModRet = "Loaded module [" + sMod + "] " + sModRet;

		PutStatus(sModRet);
		return;
	} else if (sCommand.Equals("UNLOADMOD") || sCommand.Equals("UNLOADMODULE")) {
		CString sMod;
		sMod = sLine.Token(1);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to unload [" + sMod + "] Access Denied.");
			return;
		}
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
		return;
	} else if ((sCommand.Equals("UPDATEMOD") || sCommand.Equals("UPDATEMODULE")) && m_pUser->IsAdmin() ) {
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
	} else if ((sCommand.Equals("REMBINDHOST") || sCommand.Equals("REMVHOST") || sCommand.Equals("DELVHOST")) && m_pUser->IsAdmin()) {
		CString sHost = sLine.Token(1);

		if (sHost.empty()) {
			PutStatus("Usage: RemBindHost <host>");
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

		VCString::const_iterator it;
		for (it = vsHosts.begin(); it != vsHosts.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Host", *it);
		}
		PutStatus(Table);
	} else if ((sCommand.Equals("SETBINDHOST") || sCommand.Equals("SETVHOST")) && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		CString sHost = sLine.Token(1);

		if (sHost.empty()) {
			PutStatus("Usage: SetBindHost <host>");
			return;
		}

		if (sHost.Equals(m_pUser->GetBindHost())) {
			PutStatus("You already have this bind host!");
			return;
		}

		const VCString& vsHosts = CZNC::Get().GetBindHosts();
		if (!m_pUser->IsAdmin() && !vsHosts.empty()) {
			VCString::const_iterator it;
			bool bFound = false;

			for (it = vsHosts.begin(); it != vsHosts.end(); ++it) {
				if (sHost.Equals(*it)) {
					bFound = true;
					break;
				}
			}

			if (!bFound) {
				PutStatus("You may not use this bind host. See [ListBindHosts] for a list");
				return;
			}
		}

		m_pUser->SetBindHost(sHost);
		PutStatus("Set bind host to [" + m_pUser->GetBindHost() + "]");
	} else if ((sCommand.Equals("CLEARBINDHOST") || sCommand.Equals("CLEARVHOST")) && (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost())) {
		m_pUser->SetBindHost("");
		PutStatus("Bind Host Cleared");
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

		const vector<CChan*>& vChans = m_pUser->GetChans();
		vector<CChan*>::const_iterator it;
		unsigned int uMatches = 0;
		for (it = vChans.begin(); it != vChans.end(); ++it) {
			if (!(*it)->GetName().WildCmp(sChan))
				continue;
			uMatches++;

			(*it)->ClearBuffer();
		}
		PutStatus("The buffer for [" + CString(uMatches) + "] channels matching [" + sChan + "] has been cleared");
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

		unsigned int uLineCount = sLine.Token(2).ToUInt();

		const vector<CChan*>& vChans = m_pUser->GetChans();
		vector<CChan*>::const_iterator it;
		unsigned int uMatches = 0, uFail = 0;
		for (it = vChans.begin(); it != vChans.end(); ++it) {
			if (!(*it)->GetName().WildCmp(sChan))
				continue;
			uMatches++;

			if (!(*it)->SetBufferCount(uLineCount))
				uFail++;
		}

		PutStatus("BufferCount for [" + CString(uMatches - uFail) +
				"] channels was set to [" + CString(uLineCount) + "]");
		if (uFail > 0) {
			PutStatus("Setting BufferCount failed for [" + CString(uFail) + "] channels, "
					"max buffer count is " + CString(CZNC::Get().GetMaxBufferSize()));
		}
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

		vector<CListener*>::const_iterator it;
		const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();

		for (it = vpListeners.begin(); it < vpListeners.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Port", CString((*it)->GetPort()));
			Table.SetCell("BindHost", ((*it)->GetBindHost().empty() ? CString("*") : (*it)->GetBindHost()));
			Table.SetCell("SSL", CString((*it)->IsSSL()));

			EAddrType eAddr = (*it)->GetAddrType();
			Table.SetCell("Proto", (eAddr == ADDR_ALL ? "All" : (eAddr == ADDR_IPV4ONLY ? "IPv4" : "IPv6")));

			CListener::EAcceptType eAccept = (*it)->GetAcceptType();
			Table.SetCell("IRC/Web", (eAccept == CListener::ACCEPT_ALL ? "All" : (eAccept == CListener::ACCEPT_IRC ? "IRC" : "Web")));
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
			PutStatus("Usage: AddPort <[+]port> <ipv4|ipv6|all> <web|irc|all> [bindhost]");
		} else {
			bool bSSL = (sPort.Left(1).Equals("+"));
			const CString sBindHost = sLine.Token(4);

			CListener* pListener = new CListener(uPort, sBindHost, bSSL, eAddr, eAccept);

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

void CClient::HelpUser() {
	CTable Table;
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");

	PutStatus("In the following list all occurences of <#chan> support wildcards (* and ?)");
	PutStatus("(Except ListNicks)");

	Table.AddRow();
	Table.SetCell("Command", "Version");
	Table.SetCell("Description", "Print which version of ZNC this is");

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
		Table.SetCell("Command", "AddBindHost");
		Table.SetCell("Arguments", "<host (IP preferred)>");
		Table.SetCell("Description", "Adds a bind host for normal users to use");

		Table.AddRow();
		Table.SetCell("Command", "RemBindHost");
		Table.SetCell("Arguments", "<host>");
		Table.SetCell("Description", "Removes a bind host from the list");
	}

	if (m_pUser->IsAdmin() || !m_pUser->DenySetBindHost()) {
		Table.AddRow();
		Table.SetCell("Command", "ListBindHosts");
		Table.SetCell("Description", "Shows the configured list of bind hosts");

		Table.AddRow();
		Table.SetCell("Command", "SetBindHost");
		Table.SetCell("Arguments", "<host (IP preferred)>");
		Table.SetCell("Description", "Set the bind host for this connection");

		Table.AddRow();
		Table.SetCell("Command", "ClearBindHost");
		Table.SetCell("Description", "Clear the bind host for this connection");
	}

	Table.AddRow();
	Table.SetCell("Command", "Jump [server]");
	Table.SetCell("Description", "Jump to the next or the specified server");

	Table.AddRow();
	Table.SetCell("Command", "Disconnect");
	Table.SetCell("Arguments", "[message]");
	Table.SetCell("Description", "Disconnect from IRC");

	Table.AddRow();
	Table.SetCell("Command", "Connect");
	Table.SetCell("Description", "Reconnect to IRC");

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
		Table.SetCell("Command", "ListPorts");
		Table.SetCell("Description", "Show all active listeners");

		Table.AddRow();
		Table.SetCell("Command", "AddPort");
		Table.SetCell("Arguments", "<arguments>");
		Table.SetCell("Description", "Add another port for ZNC to listen on");

		Table.AddRow();
		Table.SetCell("Command", "DelPort");
		Table.SetCell("Arguments", "<arguments>");
		Table.SetCell("Description", "Remove a port from ZNC");

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
