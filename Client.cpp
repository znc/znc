#include "main.h"
#include "Client.h"
#include "User.h"
#include "znc.h"
#include "IRCSock.h"
#include "DCCBounce.h"
#include "DCCSock.h"
#include "Server.h"

void CClient::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
		sLine.RightChomp();
	}

	DEBUG_ONLY(cout << "(" << ((m_pUser) ? m_pUser->GetUserName() : CString("")) << ") CLI -> ZNC [" << sLine << "]" << endl);

#ifdef _MODULES
	if (m_bAuthed) {
		CZNC::Get().GetModules().SetClient(this);
		MODULECALLRET(OnUserRaw(sLine));
		CZNC::Get().GetModules().SetClient(NULL);
	}
#endif

	CString sCommand = sLine.Token(0);

	if (sCommand.CaseCmp("PASS") == 0) {
		if (!m_bAuthed) {
			m_bGotPass = true;
			m_sPass = sLine.Token(1);

			if (m_sPass.find(":") != CString::npos) {
				m_sUser = m_sPass.Token(0, false, ":");
				m_sPass = m_sPass.Token(1, true, ":");
			}

			if ((m_bGotNick) && (m_bGotUser)) {
				AuthUser();
			}
		}

		return;		// Don't forward this msg.  ZNC has already registered us.
	} else if (sCommand.CaseCmp("NICK") == 0) {
		CString sNick = sLine.Token(1);
		if (sNick.Left(1) == ":") {
			sNick.LeftChomp();
		}

		if (!m_bAuthed) {
			m_sNick = sNick;
			m_bGotNick = true;

			if ((m_bGotPass) && (m_bGotUser)) {
				AuthUser();
			}
			return;		// Don't forward this msg.  ZNC will handle nick changes until auth is complete
		}

		if ((m_pUser) && (sNick.CaseCmp(m_pUser->GetNick()) == 0)) {
			m_uKeepNickCounter++;
			// If the user is changing his nick to the conifg nick, set keepnick to the config value
			if (m_pUser->GetKeepNick() && !m_pIRCSock->GetKeepNick()) {
				m_pIRCSock->SetKeepNick(true);
				PutStatusNotice("Reset KeepNick back to true");
			}
		}

		if (m_pUser && GetNick().CaseCmp(m_pUser->GetNick()) == 0) {
			// If the user changes his nick away from the config nick, we shut off keepnick for this session
			if (m_pUser->GetKeepNick()) {
				m_pIRCSock->SetKeepNick(false);
				PutStatusNotice("Set KeepNick to false");
			}
		}
	} else if (sCommand.CaseCmp("USER") == 0) {
		if ((!m_bAuthed) && (m_sUser.empty())) {
			m_sUser = sLine.Token(1);
		}

		m_bGotUser = true;

		if ((m_bGotPass) && (m_bGotNick)) {
			AuthUser();
		}

		return;		// Don't forward this msg.  ZNC has already registered us.
	}

	if (!m_pUser) {
		Close();
		return;
	}

	if (sCommand.CaseCmp("ZNC") == 0) {
		PutStatus("Hello.  How may I help you?");
		return;
	} else if (sCommand.CaseCmp("DETACH") == 0) {
		if (m_pUser) {
			CString sChan = sLine.Token(1);

			if (sChan.empty()) {
				PutStatusNotice("Usage: /detach <#chan>");
				return;
			}

			CChan* pChan = m_pUser->FindChan(sChan);
			if (!pChan) {
				PutStatusNotice("You are not on [" + sChan + "]");
				return;
			}

			pChan->DetachUser();
			PutStatusNotice("Detached from [" + sChan + "]");
			return;
		}
	} else if (sCommand.CaseCmp("PONG") == 0) {
		return;	// Block pong replies, we already responded to the pings
	} else if (sCommand.CaseCmp("JOIN") == 0) {
		CString sChans = sLine.Token(1);
		CString sKey = sLine.Token(2);

		if (sChans.Left(1) == ":") {
			sChans.LeftChomp();
		}

		if (m_pUser) {
			VCString vChans;
			sChans.Split(",", vChans, false);
			sChans.clear();

			for (unsigned int a = 0; a < vChans.size(); a++) {
				CString sChannel = vChans[a];
				CZNC::Get().GetModules().SetClient(this);
				MODULECALLCONT(OnUserJoin(sChannel, sKey));
				CZNC::Get().GetModules().SetClient(NULL);

				CChan* pChan = m_pUser->FindChan(sChannel);

				if (pChan) {
					pChan->JoinUser(false, sKey);
					continue;
				}

				if (!sChannel.empty()) {
					sChans += (sChans.empty()) ? sChannel : CString("," + sChannel);
				}
			}

			if (sChans.empty()) {
				return;
			}

			sLine = "JOIN " + sChans;

			if (!sKey.empty()) {
				sLine += " " + sKey;
			}
		}
	} else if (sCommand.CaseCmp("PART") == 0) {
		CString sChan = sLine.Token(1);
		CString sMessage = sLine.Token(2, true);

		if (sMessage.Left(1) == ":") {
			sMessage.LeftChomp();
		}

		CZNC::Get().GetModules().SetClient(this);
		MODULECALLRET(OnUserPart(sChan, sMessage));
		CZNC::Get().GetModules().SetClient(NULL);

		if (m_pUser) {
			CChan* pChan = m_pUser->FindChan(sChan);

			if (pChan && !pChan->IsOn()) {
				PutStatusNotice("Removing channel [" + sChan + "]");
				m_pUser->DelChan(sChan);
				return;
			}
		}

		sLine = "PART " + sChan;

		if (!sMessage.empty()) {
			sLine += " :" + sMessage;
		}
	} else if (sCommand.CaseCmp("QUIT") == 0) {
		if (m_pIRCSock) {
			m_pUser->UserDisconnected(this);
		}

		Close();	// Treat a client quit as a detach
		return;		// Don't forward this msg.  We don't want the client getting us disconnected.
	} else if (sCommand.CaseCmp("NOTICE") == 0) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true);

		if (sMsg.Left(1) == ":") {
			sMsg.LeftChomp();
		}

		if ((!m_pUser) || (sTarget.CaseCmp(CString(m_pUser->GetStatusPrefix() + "status"))) == 0) {
			return;
		}

		if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
#ifdef _MODULES
			if (m_pUser) {
				CString sModule = sTarget;
				sModule.LeftChomp(m_pUser->GetStatusPrefix().length());

				CModule* pModule = CZNC::Get().GetModules().FindModule(sModule);

				if (pModule) {
					pModule->SetClient(this);
					pModule->OnModNotice(sMsg);
					pModule->SetClient(NULL);
				} else if ((pModule = m_pUser->GetModules().FindModule(sModule))) {
					pModule->SetClient(this);
					pModule->OnModNotice(sMsg);
					pModule->SetClient(NULL);
				} else {
					PutStatus("No such module [" + sModule + "]");
				}
			}
#endif
			return;
		}

		if (sMsg.WildCmp("DCC * (*)")) {
			sMsg = "DCC " + sLine.Token(3) + " (" + ((m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP()) + ")";
		}

#ifdef _MODULES
		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			CZNC::Get().GetModules().SetClient(this);
			MODULECALLRET(OnUserCTCPReply(sTarget, sCTCP));
			CZNC::Get().GetModules().SetClient(NULL);

			sMsg = "\001" + sCTCP + "\001";
		} else {
			CZNC::Get().GetModules().SetClient(this);
			MODULECALLRET(OnUserNotice(sTarget, sMsg));
			CZNC::Get().GetModules().SetClient(NULL);
		}
#endif

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
		}

		PutIRC("NOTICE " + sTarget + " :" + sMsg);
		return;
	} else if (sCommand.CaseCmp("PRIVMSG") == 0) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true);

		if (sMsg.Left(1) == ":") {
			sMsg.LeftChomp();
		}

		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			if (strncasecmp(sCTCP.c_str(), "DCC ", 4) == 0 && m_pUser && m_pUser->BounceDCCs()) {
				CString sType = sCTCP.Token(1);
				CString sFile = sCTCP.Token(2);
				unsigned long uLongIP = strtoul(sCTCP.Token(3).c_str(), NULL, 10);
				unsigned short uPort = strtoul(sCTCP.Token(4).c_str(), NULL, 10);
				unsigned long uFileSize = strtoul(sCTCP.Token(5).c_str(), NULL, 10);
				CString sIP = (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP();

				if (!m_pUser->UseClientIP()) {
					uLongIP = CUtils::GetLongIP(GetRemoteIP());
				}

				if (sType.CaseCmp("CHAT") == 0) {
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
					} else {
						unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, "", true, m_pUser, (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP(), "");
						if (uBNCPort) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC CHAT chat " + CString::ToString(CUtils::GetLongIP(sIP)) + " " + CString::ToString(uBNCPort) + "\001");
						}
					}
				} else if (sType.CaseCmp("SEND") == 0) {
					// DCC SEND readme.txt 403120438 5550 1104

					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
						if ((!m_pUser) || (sTarget.CaseCmp(CString(m_pUser->GetStatusPrefix() + "status")) == 0)) {
							if (!m_pUser) {
								return;
							}

							CString sPath = m_pUser->GetDLPath();
							if (!CFile::Exists(sPath)) {
								PutStatus("Could not create [" + sPath + "] directory.");
								return;
							} else if (!CFile::IsDir(sPath)) {
								PutStatus("Error: [" + sPath + "] is not a directory.");
								return;
							}

							CString sLocalFile = sPath + "/" + sFile;

							if (m_pUser) {
								m_pUser->GetFile(GetNick(), CUtils::GetIP(uLongIP), uPort, sLocalFile, uFileSize);
							}
#ifdef _MODULES
						} else {
							CZNC::Get().GetModules().SetClient(this);
							MODULECALLRET(OnDCCUserSend(sTarget, uLongIP, uPort, sFile, uFileSize));
							CZNC::Get().GetModules().SetClient(NULL);
#endif
						}
					} else {
						unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, sFile, false, m_pUser, (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP(), "");
						if (uBNCPort) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC SEND " + sFile + " " + CString::ToString(CUtils::GetLongIP(sIP)) + " " + CString::ToString(uBNCPort) + " " + CString::ToString(uFileSize) + "\001");
						}
					}
				} else if (sType.CaseCmp("RESUME") == 0) {
					// PRIVMSG user :DCC RESUME "znc.o" 58810 151552
					unsigned short uResumePort = atoi(sCTCP.Token(3).c_str());
					unsigned long uResumeSize = strtoul(sCTCP.Token(4).c_str(), NULL, 10);

					// Need to lookup the connection by port, filter the port, and forward to the user
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
						if ((m_pUser) && (m_pUser->ResumeFile(sTarget, uResumePort, uResumeSize))) {
							PutClient(":" + sTarget + "!znc@znc.com PRIVMSG " + GetNick() + " :\001DCC ACCEPT " + sFile + " " + CString::ToString(uResumePort) + " " + CString::ToString(uResumeSize) + "\001");
						} else {
							PutStatus("DCC -> [" + GetNick() + "][" + sFile + "] Unable to find send to initiate resume.");
						}
					} else {
						CDCCBounce* pSock = (CDCCBounce*) CZNC::Get().GetManager().FindSockByLocalPort(uResumePort);
						if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString::ToString(pSock->GetUserPort()) + " " + sCTCP.Token(4) + "\001");
						}
					}
				} else if (sType.CaseCmp("ACCEPT") == 0) {
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
					} else {
						// Need to lookup the connection by port, filter the port, and forward to the user
						TSocketManager<Csock>& Manager = CZNC::Get().GetManager();

						for (unsigned int a = 0; a < Manager.size(); a++) {
							CDCCBounce* pSock = (CDCCBounce*) Manager[a];

							if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
								if (pSock->GetUserPort() == atoi(sCTCP.Token(3).c_str())) {
									PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString::ToString(pSock->GetLocalPort()) + " " + sCTCP.Token(4) + "\001");
								}
							}
						}
					}
				}

				return;
			}

			if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
#ifdef _MODULES
				CString sModule = sTarget;
				sModule.LeftChomp(m_pUser->GetStatusPrefix().length());

				CModule* pModule = m_pUser->GetModules().FindModule(sModule);
				if (pModule) {
					pModule->SetClient(this);
					pModule->OnModCTCP(sCTCP);
					pModule->SetClient(NULL);
				} else {
					PutStatus("No such module [" + sModule + "]");
				}
#endif
				return;
			}

#ifdef _MODULES
			CZNC::Get().GetModules().SetClient(this);
			MODULECALLRET(OnUserCTCP(sTarget, sCTCP));
			CZNC::Get().GetModules().SetClient(NULL);
#endif
			PutIRC("PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
			return;
		}

		if ((m_pUser) && (sTarget.CaseCmp(CString(m_pUser->GetStatusPrefix() + "status")) == 0)) {
#ifdef _MODULES
			CZNC::Get().GetModules().SetClient(this);
			MODULECALLRET(OnStatusCommand(sMsg));
			CZNC::Get().GetModules().SetClient(NULL);
#endif
			UserCommand(sMsg);
			return;
		}

		if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
#ifdef _MODULES
			if (m_pUser) {
				CString sModule = sTarget;
				sModule.LeftChomp(m_pUser->GetStatusPrefix().length());

				CModule* pModule = CZNC::Get().GetModules().FindModule(sModule);

				if (pModule) {
					pModule->SetClient(this);
					pModule->SetUser(m_pUser);
					pModule->OnModCommand(sMsg);
					pModule->SetClient(NULL);
					pModule->SetUser(NULL);
				} else {
					pModule = m_pUser->GetModules().FindModule(sModule);
					if (pModule) {
						pModule->SetClient(this);
						pModule->OnModCommand(sMsg);
						pModule->SetClient(NULL);
					} else {
						PutStatus("No such module [" + sModule + "]");
					}
				}
			}
#endif
			return;
		}

#ifdef _MODULES
		CZNC::Get().GetModules().SetClient(this);
		MODULECALLRET(OnUserMsg(sTarget, sMsg));
		CZNC::Get().GetModules().SetClient(NULL);
#endif

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " PRIVMSG " + sTarget + " :" + sMsg);
		}

		PutIRC("PRIVMSG " + sTarget + " :" + sMsg);

		// Relay to the rest of the clients that may be connected to this user

		if (m_pUser && m_pUser->IsChan(sTarget)) {
			vector<CClient*>& vClients = m_pUser->GetClients();

			for (unsigned int a = 0; a < vClients.size(); a++) {
				CClient* pClient = vClients[a];

				if (pClient != this) {
					pClient->PutClient(":" + GetNickMask() + " PRIVMSG " + sTarget + " :" + sMsg);
				}
			}
		}

		return;
	}

	PutIRC(sLine);
}

void CClient::SetNick(const CString& s) {
   m_sNick = s;

	if (m_pUser) {
		if (m_sNick.CaseCmp(m_pUser->GetNick()) == 0) {
			m_uKeepNickCounter = 0;
		}
	}
}

bool CClient::DecKeepNickCounter() {
	if (!m_uKeepNickCounter) {
		return false;
	}

	m_uKeepNickCounter--;
	return true;
}

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
		const CString& sPerms = m_pUser->GetIRCSock()->GetPerms();

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
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutStatus(sLine);
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
	} else if (sCommand.CaseCmp("MOTD") == 0) {
		if (!SendMotd()) {
			PutStatus("There is no MOTD set.");
		}
	} else if (m_pUser->IsAdmin() && sCommand.CaseCmp("SaveConfig") == 0) {
		CZNC::Get().WriteConfig();
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
				CString sLine;

				while (Table.GetLine(uTableIdx++, sLine)) {
					PutStatus(sLine);
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
			Table.SetCell("Clients", CString::ToString(it->second->GetClients().size()));
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
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutStatus(sLine);
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
	} else if (sCommand.CaseCmp("JUMP") == 0) {
		if (m_pUser) {
			if (m_pIRCSock) {
				m_pIRCSock->Close();
				PutStatus("Jumping to the next server in the list...");
				return;
			}
		}
	} else if (sCommand.CaseCmp("LISTCHANS") == 0) {
		if (m_pUser) {
			const vector<CChan*>& vChans = m_pUser->GetChans();
			const CString& sPerms = m_pUser->GetIRCSock()->GetPerms();

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
				Table.SetCell("Status", ((vChans[a]->IsOn()) ? ((vChans[a]->IsDetached()) ? "Detached" : "Joined") : "Trying"));
				Table.SetCell("Conf", CString((pChan->InConfig()) ? "yes" : ""));
				Table.SetCell("Buf", CString((pChan->KeepBuffer()) ? "*" : "") + CString::ToString(pChan->GetBufferCount()));
				Table.SetCell("Modes", pChan->GetModeString());
				Table.SetCell("Users", CString::ToString(pChan->GetNickCount()));

				for (unsigned int b = 0; b < sPerms.size(); b++) {
					CString sPerm;
					sPerm += sPerms[b];
					Table.SetCell(sPerm, CString::ToString(pChan->GetPermCount(sPerms[b])));
				}
			}

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sLine;

				while (Table.GetLine(uTableIdx++, sLine)) {
					PutStatus(sLine);
				}
			}
		}
	} else if (sCommand.CaseCmp("ADDSERVER") == 0) {
		CString sServer = sLine.Token(1);

		if (sServer.empty()) {
			PutStatus("Usage: AddServer <host> [[+]port] [pass]");
			return;
		}

		if (m_pUser->FindServer(sLine.Token(1))) {
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

		if (vServers.size() <= 1) {
			PutStatus("You must have at least one server at all times.");
			return;
		}

		if (m_pUser && m_pUser->DelServer(sServer)) {
			PutStatus("Server removed");
		} else {
			PutStatus("No such server");
		}
	} else if (sCommand.CaseCmp("LISTSERVERS") == 0) {
		if (m_pUser) {
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
				Table.SetCell("Port", CString::ToString(pServer->GetPort()));
				Table.SetCell("SSL", (pServer->IsSSL()) ? "SSL" : "");
				Table.SetCell("Pass", pServer->GetPass());
			}

			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sLine;

				while (Table.GetLine(uTableIdx++, sLine)) {
					PutStatus(sLine);
				}
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
				CString sLine;

				while (Table.GetLine(uTableIdx++, sLine)) {
					PutStatus(sLine);
				}
			}
		}
	} else if (sCommand.CaseCmp("SEND") == 0) {
		CString sToNick = sLine.Token(1);
		CString sFile = sLine.Token(2);

		if ((sToNick.empty()) || (sFile.empty())) {
			PutStatus("Usage: Send <nick> <file>");
			return;
		}

		if (m_pUser) {
			m_pUser->SendFile(sToNick, sFile);
		}
	} else if (sCommand.CaseCmp("GET") == 0) {
		CString sFile = sLine.Token(1);

		if (sFile.empty()) {
			PutStatus("Usage: Get <file>");
			return;
		}

		if (m_pUser) {
			m_pUser->SendFile(GetNick(), sFile);
		}
	} else if (sCommand.CaseCmp("LISTDCCS") == 0) {
		TSocketManager<Csock>& Manager = CZNC::Get().GetManager();

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
					Table.SetCell("Speed", CString::ToKBytes(pSock->GetAvgWrite() / 1000.0));
					Table.SetCell("Nick", pSock->GetRemoteNick());
					Table.SetCell("IP", pSock->GetRemoteIP());
					Table.SetCell("File", pSock->GetFileName());
				} else if (strncasecmp(sSockName.c_str() +5, "GET", 3) == 0) {
					CDCCSock* pSock = (CDCCSock*) Manager[a];

					Table.AddRow();
					Table.SetCell("Type", "Getting");
					Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
					Table.SetCell("Speed", CString::ToKBytes(pSock->GetAvgRead() / 1000.0));
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
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutStatus(sLine);
			}
		} else {
			PutStatus("You have no active DCCs.");
		}
	} else if ((sCommand.CaseCmp("LISTMODS") == 0) || (sCommand.CaseCmp("LISTMODULES") == 0)) {
#ifdef _MODULES
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

			unsigned int uTableIdx = 0; CString sLine;
			while (Table.GetLine(uTableIdx++, sLine)) {
				PutStatus(sLine);
			}
		}
#else
		PutStatus("Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("LOADMOD") == 0) || (sCommand.CaseCmp("LOADMODULE") == 0)) {
		CString sMod = sLine.Token(1);
		CString sArgs = sLine.Token(2, true);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to load [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: LoadMod <module> [args]");
			return;
		}

		CString sModRet;

		try {
			m_pUser->GetModules().LoadModule(sMod, sArgs, m_pUser, sModRet);
		} catch (CException e) {
			PutStatus("Unable to load module [" + sMod + "] [" + sModRet + "]");
			return;
		}

		PutStatus(sModRet);
#else
		PutStatus("Unable to load [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("UNLOADMOD") == 0) || (sCommand.CaseCmp("UNLOADMODULE") == 0)) {
		CString sMod = sLine.Token(1);

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
		m_pUser->GetModules().UnloadModule(sMod, sModRet);
		PutStatus(sModRet);
#else
		PutStatus("Unable to unload [" + sMod + "] Modules are not enabled.");
#endif
		return;
	} else if ((sCommand.CaseCmp("RELOADMOD") == 0) || (sCommand.CaseCmp("RELOADMODULE") == 0)) {
		CString sMod = sLine.Token(1);
		CString sArgs = sLine.Token(2, true);

		if (m_pUser->DenyLoadMod()) {
			PutStatus("Unable to reload [" + sMod + "] Access Denied.");
			return;
		}
#ifdef _MODULES
		if (sMod.empty()) {
			PutStatus("Usage: ReloadMod <module> [args]");
			return;
		}

		CString sModRet;
		m_pUser->GetModules().ReloadModule(sMod, sArgs, m_pUser, sModRet);
		PutStatus(sModRet);
#else
		PutStatus("Unable to unload [" + sMod + "] Modules are not enabled.");
#endif
		return;
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

		PutStatus("BufferCount for [" + sChan + "] set to [" + CString::ToString(pChan->GetBufferCount()) + "]");
	} else {
		PutStatus("Unknown command [" + sCommand + "] try 'Help'");
	}
}

bool CClient::SendMotd() {
	const VCString& vsMotd = CZNC::Get().GetMotd();

	if (!vsMotd.size()) {
		return false;
	}

	for (unsigned int a = 0; a < vsMotd.size(); a++) {
		PutStatusNotice(vsMotd[a]);
	}

	return true;
}

void CClient::HelpUser() {
	CTable Table;
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");

	Table.AddRow(); Table.SetCell("Command", "Version");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "Prints which version of znc this is");
	Table.AddRow(); Table.SetCell("Command", "ListDCCs");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all active DCCs");
	Table.AddRow(); Table.SetCell("Command", "ListMods");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all loaded modules");
	Table.AddRow(); Table.SetCell("Command", "ListChans");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all channels");
	Table.AddRow(); Table.SetCell("Command", "ListNicks");	Table.SetCell("Arguments", "<#chan>");				Table.SetCell("Description", "List all nicks on a channel");

	if (!m_pUser->IsAdmin()) { // If they are an admin we will add this command below with an argument
		Table.AddRow(); Table.SetCell("Command", "ListClients");Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all clients connected to your znc user");
	}

	Table.AddRow(); Table.SetCell("Command", "ListServers");Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all servers");
	Table.AddRow(); Table.SetCell("Command", "AddServer");	Table.SetCell("Arguments", "<host> [[+]port] [pass]");	Table.SetCell("Description", "Add a server to the list");
	Table.AddRow(); Table.SetCell("Command", "RemServer");	Table.SetCell("Arguments", "<host>");				Table.SetCell("Description", "Remove a server from the list");
	Table.AddRow(); Table.SetCell("Command", "Topics");		Table.SetCell("Arguments", "");						Table.SetCell("Description", "Show topics in all channels");
	Table.AddRow(); Table.SetCell("Command", "SetBuffer");	Table.SetCell("Arguments", "<#chan> [linecount]");	Table.SetCell("Description", "Set the buffer count for a channel");
	Table.AddRow(); Table.SetCell("Command", "Jump");		Table.SetCell("Arguments", "");						Table.SetCell("Description", "Jump to the next server in the list");
	Table.AddRow(); Table.SetCell("Command", "Send");		Table.SetCell("Arguments", "<nick> <file>");		Table.SetCell("Description", "Send a shell file to a nick on IRC");
	Table.AddRow(); Table.SetCell("Command", "Get");		Table.SetCell("Arguments", "<file>");				Table.SetCell("Description", "Send a shell file to yourself");

	if (m_pUser) {
		if (!m_pUser->DenyLoadMod()) {
			Table.AddRow(); Table.SetCell("Command", "LoadMod");	Table.SetCell("Arguments", "<module>");		Table.SetCell("Description", "Load a module");
			Table.AddRow(); Table.SetCell("Command", "UnloadMod");	Table.SetCell("Arguments", "<module>");		Table.SetCell("Description", "Unload a module");
			Table.AddRow(); Table.SetCell("Command", "ReloadMod");	Table.SetCell("Arguments", "<module>");		Table.SetCell("Description", "Reload a module");
		}

		if (m_pUser->IsAdmin()) {
			Table.AddRow(); Table.SetCell("Command", "SaveConfig");	Table.SetCell("Arguments", "");				Table.SetCell("Description", "Save the current settings to disk");
			Table.AddRow(); Table.SetCell("Command", "ListUsers");	Table.SetCell("Arguments", "");				Table.SetCell("Description", "List all users/clients connected to znc");
			Table.AddRow(); Table.SetCell("Command", "ListClients");Table.SetCell("Arguments", "[User]");		Table.SetCell("Description", "List all clients connected to your znc user");
			Table.AddRow(); Table.SetCell("Command", "SetMOTD");	Table.SetCell("Arguments", "<Message>");	Table.SetCell("Description", "Set the message of the day");
			Table.AddRow(); Table.SetCell("Command", "AddMOTD");	Table.SetCell("Arguments", "<Message>");	Table.SetCell("Description", "Append <Message> to MOTD");
			Table.AddRow(); Table.SetCell("Command", "ClearMOTD");	Table.SetCell("Arguments", "");				Table.SetCell("Description", "Clear the MOTD");
			Table.AddRow(); Table.SetCell("Command", "Broadcast");	Table.SetCell("Arguments", "[message]");	Table.SetCell("Description", "Broadcast a message to all users");
			Table.AddRow(); Table.SetCell("Command", "Shutdown");	Table.SetCell("Arguments", "[message]");	Table.SetCell("Description", "Shutdown znc completely");
		}
	}

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutStatus(sLine);
		}
	}
}

bool CClient::ConnectionFrom(const CString& sHost, unsigned short uPort) {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionFrom(" << sHost << ", " << uPort << ")" << endl);
	return CZNC::Get().IsHostAllowed(sHost);
}

void CClient::AuthUser() {
	CUser* pUser = CZNC::Get().GetUser(m_sUser);

	if ((!pUser) || (!pUser->CheckPass(m_sPass))) {
		if (pUser) {
			pUser->PutStatus("Another user attempted to login as you, with a bad password.");
		}

		PutStatus("Bad username and/or password.");
		PutClient(":irc.znc.com 464 " + GetNick() + " :Password Incorrect");
		Close();
	} else {
		m_sPass = "";
		m_pUser = pUser;

		if (!m_pUser->IsHostAllowed(GetRemoteIP())) {
			Close();
			return;
		}

		m_bAuthed = true;
		SetSockName("USR::" + pUser->GetUserName());

		m_pIRCSock = (CIRCSock*) CZNC::Get().FindSockByName("IRC::" + pUser->GetUserName());
		m_pUser->UserConnected(this);

		SendMotd();

		CZNC::Get().GetModules().SetClient(this);
		VOIDMODULECALL(OnUserAttached());
		CZNC::Get().GetModules().SetClient(NULL);
	}
}

void CClient::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected();" << endl);
	SetTimeout(900);	// Now that we are connected, let nature take its course
}

void CClient::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
}

void CClient::Disconnected() {
	if (m_pIRCSock) {
		m_pUser->UserDisconnected(this);
		m_pIRCSock = NULL;
	}

	CZNC::Get().GetModules().SetClient(this);
	VOIDMODULECALL(OnUserDetached());
	CZNC::Get().GetModules().SetClient(NULL);
}

void CClient::IRCConnected(CIRCSock* pIRCSock) {
	m_pIRCSock = pIRCSock;
}

void CClient::BouncedOff() {
	PutStatusNotice("You are being disconnected because another user just authenticated as you.");
	m_pIRCSock = NULL;
	Close();
}

void CClient::IRCDisconnected() {
	m_pIRCSock = NULL;
}

Csock* CClient::GetSockObj(const CString& sHost, unsigned short uPort) {
	CClient* pSock = new CClient(sHost, uPort);
	return pSock;
}

void CClient::PutIRC(const CString& sLine) {
	if (m_pIRCSock) {
		m_pIRCSock->PutIRC(sLine);
	}
}

void CClient::PutClient(const CString& sLine) {
	DEBUG_ONLY(cout << "(" << ((m_pUser) ? m_pUser->GetUserName() : CString("")) << ") ZNC -> CLI [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CClient::PutStatusNotice(const CString& sLine) {
	PutModNotice("status", sLine);
}

void CClient::PutStatus(const CString& sLine) {
	PutModule("status", sLine);
}

void CClient::PutModNotice(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com NOTICE " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com NOTICE " + GetNick() + " :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com PRIVMSG " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com PRIVMSG " + GetNick() + " :" + sLine + "\r\n");
}

CString CClient::GetNick() const {
	CString sRet;

	if ((m_bAuthed) && (m_pIRCSock)) {
		sRet = m_pIRCSock->GetNick();
	}

	return (sRet.empty()) ? m_sNick : sRet;
}

CString CClient::GetNickMask() const {
	if (m_pIRCSock) {
		return m_pIRCSock->GetNickMask();
	}

	return GetNick() + "!" + m_pUser->GetIdent() + "@" + m_pUser->GetVHost();
}
