/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
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

void CClient::UserCommand(const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	if (sLine.empty()) {
		return;
	}

	CString sCommand = sLine.Token(0);

	if (sCommand.CaseCmp("HELP") == 0) {
		HelpUser();
	} else if (sCommand.CaseCmp("LISTNICKS") == 0) {
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
		CIRCSock* pIRCSock = (!m_pUser) ? NULL : m_pUser->GetIRCSock();
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

		for (map<CString,CNick*>::const_iterator a = msNicks.begin(); a != msNicks.end(); a++) {
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

		unsigned int uTableIdx = 0;
		CString sTmp;

		while (Table.GetLine(uTableIdx++, sTmp)) {
			PutStatus(sTmp);
		}
	} else if (sCommand.CaseCmp("DETACH") == 0) {
		if (m_pUser) {
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
		}
	} else if (sCommand.CaseCmp("VERSION") == 0) {
		PutStatus(CZNC::GetTag());
	} else if (sCommand.CaseCmp("MOTD") == 0 || sCommand.CaseCmp("ShowMOTD") == 0) {
		if (!SendMotd()) {
			PutStatus("There is no MOTD set.");
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("Rehash") == 0) {
		CString sRet;

		if (CZNC::Get().RehashConfig(sRet)) {
			PutStatus("Rehashing succeeded!");
		} else {
			PutStatus("Rehashing failed: " + sRet);
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("SaveConfig") == 0) {
		if (CZNC::Get().WriteConfig()) {
			PutStatus("Wrote config to [" + CZNC::Get().GetConfigFile() + "]");
		} else {
			PutStatus("Error while trying to write config.");
		}
	} else if (sCommand.CaseCmp("LISTCLIENTS") == 0) {
		if (m_pUser) {
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

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sTmp;

				while (Table.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("LISTUSERS") == 0) {
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("Clients");
		Table.AddColumn("OnIRC");
		Table.AddColumn("IRC Server");
		Table.AddColumn("IRC User");

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
			Table.AddRow();
			Table.SetCell("Username", it->first);
			Table.SetCell("Clients", CString(it->second->GetClients().size()));
			if (!it->second->IsIRCConnected()) {
				Table.SetCell("OnIRC", "No");
			} else {
				Table.SetCell("OnIRC", "Yes");
				Table.SetCell("IRC Server", it->second->GetIRCServer());
				Table.SetCell("IRC User", it->second->GetIRCNick().GetNickMask());
			}
		}

		if (Table.size()) {
			unsigned int uTableIdx = 0;
			CString sTmp;

			while (Table.GetLine(uTableIdx++, sTmp)) {
				PutStatus(sTmp);
			}
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("SetMOTD") == 0) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: SetMOTD <Message>");
		} else {
			CZNC::Get().SetMotd(sMessage);
			PutStatus("MOTD set to [" + sMessage + "]");
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("AddMOTD") == 0) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			PutStatus("Usage: AddMOTD <Message>");
		} else {
			CZNC::Get().AddMotd(sMessage);
			PutStatus("Added [" + sMessage + "] to MOTD");
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("ClearMOTD") == 0) {
		CZNC::Get().ClearMotd();
		PutStatus("Cleared MOTD");
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("BROADCAST") == 0) {
		CZNC::Get().Broadcast(sLine.Token(1, true));
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("SHUTDOWN") == 0) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			sMessage = "ZNC is being shutdown NOW!!";
		}

		CZNC::Get().Broadcast(sMessage);
		usleep(100000);	// Sleep for 10ms to attempt to allow the previous Broadcast() to go through to all users

		throw CException(CException::EX_Shutdown);
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("RESTART") == 0) {
		CString sMessage = sLine.Token(1, true);

		if (sMessage.empty()) {
			sMessage = "ZNC is being restarted NOW!!";
		}

		CZNC::Get().Broadcast(sMessage);
		throw CException(CException::EX_Restart);
	} else if (sCommand.CaseCmp("JUMP") == 0 ||
			sCommand.CaseCmp("CONNECT") == 0) {
		if (m_pUser) {
			if (!m_pUser->HasServers()) {
				PutStatus("You don't have any servers added.");
				return;
			}

			if (m_pIRCSock) {
				m_pIRCSock->Quit();
				PutStatus("Jumping to the next server in the list...");
			} else {
				PutStatus("Connecting...");
			}

			m_pUser->SetIRCConnectEnabled(true);
			m_pUser->CheckIRCConnect();
			return;
		}
	} else if (sCommand.CaseCmp("DISCONNECT") == 0) {
		if (m_pUser) {
			if (m_pIRCSock)
				m_pIRCSock->Quit();
			m_pUser->SetIRCConnectEnabled(false);
			PutStatus("Disconnected from IRC. Use 'connect' to reconnect.");
			return;
		}
	} else if (sCommand.CaseCmp("ENABLECHAN") == 0) {
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
	} else if (sCommand.CaseCmp("LISTCHANS") == 0) {
		if (m_pUser) {
			const vector<CChan*>& vChans = m_pUser->GetChans();
			CIRCSock* pIRCSock = (!m_pUser) ? NULL : m_pUser->GetIRCSock();
			const CString& sPerms = (pIRCSock) ? pIRCSock->GetPerms() : "";

			if (!vChans.size()) {
				PutStatus("You have no channels defined");
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

			for (unsigned int a = 0; a < vChans.size(); a++) {
				CChan* pChan = vChans[a];
				Table.AddRow();
				Table.SetCell("Name", pChan->GetPermStr() + pChan->GetName());
				Table.SetCell("Status", ((vChans[a]->IsOn()) ? ((vChans[a]->IsDetached()) ? "Detached" : "Joined") : ((vChans[a]->IsDisabled()) ? "Disabled" : "Trying")));
				Table.SetCell("Conf", CString((pChan->InConfig()) ? "yes" : ""));
				Table.SetCell("Buf", CString((pChan->KeepBuffer()) ? "*" : "") + CString(pChan->GetBufferCount()));
				Table.SetCell("Modes", pChan->GetModeString());
				Table.SetCell("Users", CString(pChan->GetNickCount()));

				for (unsigned int b = 0; b < sPerms.size(); b++) {
					CString sPerm;
					sPerm += sPerms[b];
					Table.SetCell(sPerm, CString(pChan->GetPermCount(sPerms[b])));
				}
			}

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sTmp;

				while (Table.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		}
	} else if (sCommand.CaseCmp("ADDSERVER") == 0) {
		CString sServer = sLine.Token(1);

		if (sServer.empty()) {
			PutStatus("Usage: AddServer <host> [[+]port] [pass]");
			return;
		}

		if (m_pUser->FindServer(sServer)) {
			PutStatus("That server already exists");
			return;
		}

		if (m_pUser && m_pUser->AddServer(sLine.Token(1, true))) {
			PutStatus("Server added");
		} else {
			PutStatus("Unable to add that server");
		}
	} else if (sCommand.CaseCmp("REMSERVER") == 0 || sCommand.CaseCmp("DELSERVER") == 0) {
		CString sServer = sLine.Token(1);

		if (sServer.empty()) {
			PutStatus("Usage: RemServer <host>");
			return;
		}

		const vector<CServer*>& vServers = m_pUser->GetServers();

		if (vServers.size() <= 0) {
			PutStatus("You don't have any servers added.");
			return;
		}

		if (m_pUser && m_pUser->DelServer(sServer)) {
			PutStatus("Server removed");
		} else {
			PutStatus("No such server");
		}
	} else if (sCommand.CaseCmp("LISTSERVERS") == 0) {
		if (m_pUser) {
		    if (m_pUser->HasServers()) {
			const vector<CServer*>& vServers = m_pUser->GetServers();
			CTable Table;
			Table.AddColumn("Host");
			Table.AddColumn("Port");
			Table.AddColumn("SSL");
			Table.AddColumn("Pass");

			for (unsigned int a = 0; a < vServers.size(); a++) {
				CServer* pServer = vServers[a];
				Table.AddRow();
				Table.SetCell("Host", pServer->GetName());
				Table.SetCell("Port", CString(pServer->GetPort()));
				Table.SetCell("SSL", (pServer->IsSSL()) ? "SSL" : "");
				Table.SetCell("Pass", pServer->GetPass());
			}

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sTmp;

				while (Table.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		    } else {
			PutStatus("You don't have any servers added.");
		    }
		}
	} else if (sCommand.CaseCmp("TOPICS") == 0) {
		if (m_pUser) {
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

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sTmp;

				while (Table.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		}
	} else if (sCommand.CaseCmp("SEND") == 0) {
		CString sToNick = sLine.Token(1);
		CString sFile = sLine.Token(2);
		CString sAllowedPath = m_pUser->GetDLPath();
		CString sAbsolutePath;

		if ((sToNick.empty()) || (sFile.empty())) {
			PutStatus("Usage: Send <nick> <file>");
			return;
		}

		sAbsolutePath = CDir::ChangeDir(m_pUser->GetDLPath(), sFile, CZNC::Get().GetHomePath());

		if (sAbsolutePath.Left(sAllowedPath.length()) != sAllowedPath) {
			PutStatus("Illegal path.");
			return;
		}

		if (m_pUser) {
			m_pUser->SendFile(sToNick, sFile);
		}
	} else if (sCommand.CaseCmp("GET") == 0) {
		CString sFile = sLine.Token(1);
		CString sAllowedPath = m_pUser->GetDLPath();
		CString sAbsolutePath;

		if (sFile.empty()) {
			PutStatus("Usage: Get <file>");
			return;
		}

		sAbsolutePath = CDir::ChangeDir(m_pUser->GetDLPath(), sFile, CZNC::Get().GetHomePath());

		if (sAbsolutePath.Left(sAllowedPath.length()) != sAllowedPath) {
			PutStatus("Illegal path.");
			return;
		}

		if (m_pUser) {
			m_pUser->SendFile(GetNick(), sFile);
		}
	} else if (sCommand.CaseCmp("LISTDCCS") == 0) {
		CSockManager& Manager = CZNC::Get().GetManager();

		CTable Table;
		Table.AddColumn("Type");
		Table.AddColumn("State");
		Table.AddColumn("Speed");
		Table.AddColumn("Nick");
		Table.AddColumn("IP");
		Table.AddColumn("File");

		for (unsigned int a = 0; a < Manager.size(); a++) {
			const CString& sSockName = Manager[a]->GetSockName();

			if (strncasecmp(sSockName.c_str(), "DCC::", 5) == 0) {
				if (strncasecmp(sSockName.c_str() +5, "XFER::REMOTE::", 14) == 0) {
					continue;
				}

				if (strncasecmp(sSockName.c_str() +5, "CHAT::REMOTE::", 14) == 0) {
					continue;
				}

				if (strncasecmp(sSockName.c_str() +5, "SEND", 4) == 0) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Sending");
					Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
					Table.SetCell("Speed", CString((int)(pSock->GetAvgWrite() / 1024.0)) + " KiB/s");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (strncasecmp(sSockName.c_str() +5, "GET", 3) == 0) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Getting");
					Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
					Table.SetCell("Speed", CString((int)(pSock->GetAvgRead() / 1024.0)) + " KiB/s");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (strncasecmp(sSockName.c_str() +5, "LISTEN", 6) == 0) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Sending");
					Table.SetCell("State", "Waiting");
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (strncasecmp(sSockName.c_str() +5, "XFER::LOCAL", 11) == 0) {
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
				} else if (strncasecmp(sSockName.c_str() +5, "CHAT::LOCAL", 11) == 0) {
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

		if (Table.size()) {
			unsigned int uTableIdx = 0;
			CString sTmp;

			while (Table.GetLine(uTableIdx++, sTmp)) {
				PutStatus(sTmp);
			}
		} else {
			PutStatus("You have no active DCCs.");
		}
	} else if ((sCommand.CaseCmp("LISTMODS") == 0) || (sCommand.CaseCmp("LISTMODULES") == 0)) {
#ifdef _MODULES
		if (m_pUser->IsAdmin()) {
		    CModules& GModules = CZNC::Get().GetModules();

		    if (!GModules.size()) {
				PutStatus("No global modules loaded.");
			} else {
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Description");

				for (unsigned int b = 0; b < GModules.size(); b++) {
					GTable.AddRow();
					GTable.SetCell("Name", GModules[b]->GetModName());
					GTable.SetCell("Description", GModules[b]->GetDescription().Ellipsize(128));
				}

				unsigned int uTableIdx = 0;
				CString sTmp;

				while (GTable.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		}

		if (m_pUser) {
			CModules& Modules = m_pUser->GetModules();

			if (!Modules.size()) {
				PutStatus("You have no modules loaded.");
				return;
			}

			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");

			for (unsigned int b = 0; b < Modules.size(); b++) {
				Table.AddRow();
				Table.SetCell("Name", Modules[b]->GetModName());
				Table.SetCell("Description", Modules[b]->GetDescription().Ellipsize(128));
			}

			unsigned int uTableIdx = 0;
			CString sTmp;
			while (Table.GetLine(uTableIdx++, sTmp)) {
				PutStatus(sTmp);
			}
		}
#else
		PutStatus("Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("LISTAVAILMODS") == 0) || (sCommand.CaseCmp("LISTAVAILABLEMODULES") == 0)) {
#ifdef _MODULES
		if (m_pUser->DenyLoadMod()) {
			PutStatus("Access Denied.");
			return;
		}

		if (m_pUser->IsAdmin()) {
			set<CModInfo> ssGlobalMods;
			CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, true);

			if (!ssGlobalMods.size()) {
				PutStatus("No global modules available.");
			} else {
				CTable GTable;
				GTable.AddColumn("Name");
				GTable.AddColumn("Description");
				set<CModInfo>::iterator it;

				for (it = ssGlobalMods.begin(); it != ssGlobalMods.end(); it++) {
					const CModInfo& Info = *it;
					GTable.AddRow();
					GTable.SetCell("Name", (CZNC::Get().GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
					GTable.SetCell("Description", Info.GetDescription().Ellipsize(128));
				}

				unsigned int uTableIdx = 0;
				CString sTmp;

				while (GTable.GetLine(uTableIdx++, sTmp)) {
					PutStatus(sTmp);
				}
			}
		}

		if (m_pUser) {
			set<CModInfo> ssUserMods;
			CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

			if (!ssUserMods.size()) {
				PutStatus("No user modules available.");
				return;
			}

			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Description");
			set<CModInfo>::iterator it;

			for (it = ssUserMods.begin(); it != ssUserMods.end(); it++) {
				const CModInfo& Info = *it;
				Table.AddRow();
				Table.SetCell("Name", (m_pUser->GetModules().FindModule(Info.GetName()) ? "*" : " ") + Info.GetName());
				Table.SetCell("Description", Info.GetDescription().Ellipsize(128));
			}

			unsigned int uTableIdx = 0;
			CString sTmp;
			while (Table.GetLine(uTableIdx++, sTmp)) {
				PutStatus(sTmp);
			}
		}
#else
		PutStatus("Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("LOADMOD") == 0) || (sCommand.CaseCmp("LOADMODULE") == 0)) {
		CString sMod;
		CString sArgs;
		bool bGlobal = false;

		if (sLine.Token(1).CaseCmp("-global") == 0) {
		    sMod = sLine.Token(2);

		    if (!m_pUser->IsAdmin()) {
				PutStatus("Unable to load global module [" + sMod + "] Access Denied.");
				return;
		    }

		    sArgs = sLine.Token(3, true);
		    bGlobal = true;
		} else {
		    sMod = sLine.Token(1);
		    sArgs = sLine.Token(2, true);
		}

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to load [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: LoadMod [-global] <module> [args]");
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
			PutStatus("Unable to load module [" + sMod + "] [" + sModRet + "]");
			return;
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to load [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("UNLOADMOD") == 0) || (sCommand.CaseCmp("UNLOADMODULE") == 0)) {
		CString sMod;
		bool bGlobal = false;

		if (sLine.Token(1).CaseCmp("-global") == 0) {
		    sMod = sLine.Token(2);

		    if (!m_pUser->IsAdmin()) {
				PutStatus("Unable to unload global module [" + sMod + "] Access Denied.");
				return;
		    }

		    bGlobal = true;
		} else
		    sMod = sLine.Token(1);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to unload [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: UnloadMod [-global] <module>");
			return;
		}

		CString sModRet;

		if (bGlobal) {
		    CZNC::Get().GetModules().UnloadModule(sMod, sModRet);
		} else {
		    m_pUser->GetModules().UnloadModule(sMod, sModRet);
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to unload [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("RELOADMOD") == 0) || (sCommand.CaseCmp("RELOADMODULE") == 0)) {
		CString sMod;
		CString sArgs;
		bool bGlobal = false;

		if (sLine.Token(1).CaseCmp("-global") == 0) {
		    sMod = sLine.Token(2);

		    if (!m_pUser->IsAdmin()) {
				PutStatus("Unable to reload global module [" + sMod + "] Access Denied.");
				return;
		    }

		    sArgs = sLine.Token(3, true);
		    bGlobal = true;
		} else {
		    sMod = sLine.Token(1);
		    sArgs = sLine.Token(2, true);
		}

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to reload [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: ReloadMod [-global] <module> [args]");
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
	} else if (sCommand.CaseCmp("SETVHOST") == 0 && (m_pUser->IsAdmin() || !m_pUser->DenySetVHost())) {
		CString sVHost = sLine.Token(1);

		if (sVHost.empty()) {
			PutStatus("Usage: SetVHost <VHost>");
			return;
		}

		m_pUser->SetVHost(sVHost);
		PutStatus("Set VHost to [" + m_pUser->GetVHost() + "]");
	} else if (sCommand.CaseCmp("CLEARVHOST") == 0 && (m_pUser->IsAdmin() || !m_pUser->DenySetVHost())) {
		m_pUser->SetVHost("");
		PutStatus("VHost Cleared");
	} else if (sCommand.CaseCmp("PLAYBUFFER") == 0) {
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
	} else if (sCommand.CaseCmp("CLEARBUFFER") == 0) {
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
	} else if (sCommand.CaseCmp("CLEARALLCHANNELBUFFERS") == 0) {
		vector<CChan*>::const_iterator it;
		const vector<CChan*>& vChans = m_pUser->GetChans();

		for (it = vChans.begin(); it != vChans.end(); it++) {
			(*it)->ClearBuffer();
		}
		PutStatus("All channel buffers have been cleared");
	} else if (sCommand.CaseCmp("SETBUFFER") == 0) {
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

		if (!pChan->IsOn()) {
			PutStatus("You are not on [" + sChan + "] [trying]");
			return;
		}

		unsigned int uLineCount = strtoul(sLine.Token(2).c_str(), NULL, 10);

		if (uLineCount > 500) {
			PutStatus("Max linecount is 500.");
			return;
		}

		pChan->SetBufferCount(uLineCount);

		PutStatus("BufferCount for [" + sChan + "] set to [" + CString(pChan->GetBufferCount()) + "]");
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("TRAFFIC") == 0) {
		CZNC::Get().UpdateTrafficStats();
		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("In");
		Table.AddColumn("Out");
		Table.AddColumn("Total");
		unsigned long long users_total_in = 0;
		unsigned long long users_total_out = 0;
		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
			Table.AddRow();
			Table.SetCell("Username", it->first);
			Table.SetCell("In", CString::ToByteStr(it->second->BytesRead()));
			Table.SetCell("Out", CString::ToByteStr(it->second->BytesWritten()));
			Table.SetCell("Total", CString::ToByteStr(it->second->BytesRead() + it->second->BytesWritten()));
			users_total_in += it->second->BytesRead();
			users_total_out += it->second->BytesWritten();
		}
		Table.AddRow();
		Table.SetCell("Username", "<Users>");
		Table.SetCell("In", CString::ToByteStr(users_total_in));
		Table.SetCell("Out", CString::ToByteStr(users_total_out));
		Table.SetCell("Total", CString::ToByteStr(users_total_in + users_total_out));

		Table.AddRow();
		Table.SetCell("Username", "<ZNC>");
		Table.SetCell("In", CString::ToByteStr(CZNC::Get().BytesRead()));
		Table.SetCell("Out", CString::ToByteStr(CZNC::Get().BytesWritten()));
		Table.SetCell("Total", CString::ToByteStr(CZNC::Get().BytesRead() + CZNC::Get().BytesWritten()));

		Table.AddRow();
		Table.SetCell("Username", "<Total>");
		Table.SetCell("In", CString::ToByteStr(users_total_in + CZNC::Get().BytesRead()));
		Table.SetCell("Out", CString::ToByteStr(users_total_out + CZNC::Get().BytesWritten()));
		Table.SetCell("Total", CString::ToByteStr(users_total_in + CZNC::Get().BytesRead() + users_total_out + CZNC::Get().BytesWritten()));

		if (Table.size()) {
			unsigned int uTableIdx = 0;
			CString sTmp;
			while (Table.GetLine(uTableIdx++, sTmp)) {
				PutStatus(sTmp);
			}
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("UPTIME") == 0) {
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
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Prints which version of znc this is");

	Table.AddRow();
	Table.SetCell("Command", "ListDCCs");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "List all active DCCs");

	Table.AddRow();
	Table.SetCell("Command", "ListMods");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "List all loaded modules");

	Table.AddRow();
	Table.SetCell("Command", "ListAvailMods");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "List all available modules");

	Table.AddRow();
	Table.SetCell("Command", "ListChans");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "List all channels");

	Table.AddRow();
	Table.SetCell("Command", "ListNicks");
	Table.SetCell("Arguments", "<#chan>");
	Table.SetCell("Description", "List all nicks on a channel");

	if (!m_pUser->IsAdmin()) { // If they are an admin we will add this command below with an argument
		Table.AddRow();
		Table.SetCell("Command", "ListClients");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "List all clients connected to your znc user");
	}

	Table.AddRow();
	Table.SetCell("Command", "ListServers");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "List all servers");

	Table.AddRow();
	Table.SetCell("Command", "AddServer");
	Table.SetCell("Arguments", "<host> [[+]port] [pass]");
	Table.SetCell("Description", "Add a server to the list");

	Table.AddRow();
	Table.SetCell("Command", "RemServer");
	Table.SetCell("Arguments", "<host>");
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
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Show topics in all channels");

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
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Clear the channel buffers");

	Table.AddRow();
	Table.SetCell("Command", "SetBuffer");
	Table.SetCell("Arguments", "<#chan> [linecount]");
	Table.SetCell("Description", "Set the buffer count for a channel");

	if (m_pUser->IsAdmin() || !m_pUser->DenySetVHost()) {
		Table.AddRow();
		Table.SetCell("Command", "SetVHost");
		Table.SetCell("Arguments", "<vhost (ip preferred)>");
		Table.SetCell("Description", "Set the VHost for this connection");

		Table.AddRow();
		Table.SetCell("Command", "ClearVHost");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Clear the VHost for this connection");
	}

	Table.AddRow();
	Table.SetCell("Command", "Jump");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Jump to the next server in the list");

	Table.AddRow();
	Table.SetCell("Command", "Disconnect");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Disconnect from IRC");

	Table.AddRow();
	Table.SetCell("Command", "Connect");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Reconnect to IRC");

	Table.AddRow();
	Table.SetCell("Command", "Send");
	Table.SetCell("Arguments", "<nick> <file>");
	Table.SetCell("Description", "Send a shell file to a nick on IRC");

	Table.AddRow();
	Table.SetCell("Command", "Get");
	Table.SetCell("Arguments", "<file>");
	Table.SetCell("Description", "Send a shell file to yourself");

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
	}

	Table.AddRow();
	Table.SetCell("Command", "ShowMOTD");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Show the message of the day");

	if (m_pUser->IsAdmin()) {
		Table.AddRow();
		Table.SetCell("Command", "SetMOTD");
		Table.SetCell("Arguments", "<Message>");
		Table.SetCell("Description", "Set the message of the day");

		Table.AddRow();
		Table.SetCell("Command", "AddMOTD");
		Table.SetCell("Arguments", "<Message>");
		Table.SetCell("Description", "Append <Message> to MOTD");

		Table.AddRow();
		Table.SetCell("Command", "ClearMOTD");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Clear the MOTD");

		Table.AddRow();
		Table.SetCell("Command", "Rehash");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Reload znc.conf from disk");

		Table.AddRow();
		Table.SetCell("Command", "SaveConfig");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Save the current settings to disk");

		Table.AddRow();
		Table.SetCell("Command", "ListUsers");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "List all users/clients connected to znc");

		Table.AddRow();
		Table.SetCell("Command", "ListClients");
		Table.SetCell("Arguments", "[User]");
		Table.SetCell("Description", "List all clients connected to your znc user");

		Table.AddRow();
		Table.SetCell("Command", "Traffic");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Show basic traffic stats for all znc users");

		Table.AddRow();
		Table.SetCell("Command", "Uptime");
		Table.SetCell("Arguments", "");
		Table.SetCell("Description", "Show how long ZNC is already running");

		Table.AddRow();
		Table.SetCell("Command", "Broadcast");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Broadcast a message to all users");

		Table.AddRow();
		Table.SetCell("Command", "Shutdown");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Shutdown znc completely");

		Table.AddRow();
		Table.SetCell("Command", "Restart");
		Table.SetCell("Arguments", "[message]");
		Table.SetCell("Description", "Restarts znc");
	}

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutStatus(sLine);
		}
	}
}
