#include "main.h"
#include "UserSock.h"
#include "User.h"
#include "znc.h"
#include "IRCSock.h"
#include "DCCBounce.h"
#include "DCCSock.h"
#include "Server.h"

void CUserSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
		sLine.RightChomp();
	}

	DEBUG_ONLY(cout << GetSockName() << " <- [" << sLine << "]" << endl);

	if (m_bAuthed) {
		MODULECALLRET(OnUserRaw(sLine));
	}

	CString sCommand = sLine.Token(0);

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
	} else if (sCommand.CaseCmp("PASS") == 0) {
		if (!m_bAuthed) {
			m_bGotPass = true;
			m_sPass = sLine.Token(1);

			if (m_sPass.find(":") != CString::npos) {
				m_sUser = m_sPass.Token(0, false, ':');
				m_sPass = m_sPass.Token(1, true, ':');
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
	} else if (sCommand.CaseCmp("JOIN") == 0) {
		CString sChan = sLine.Token(1);
		CString sKey = sLine.Token(2);

		if (sChan.Left(1) == ":") {
			sChan.LeftChomp();
		}

		if (m_pUser) {
			CChan* pChan = m_pUser->FindChan(sChan);

			if (pChan) {
				pChan->JoinUser(false, sKey);
				return;
			}
		}
	} else if (sCommand.CaseCmp("QUIT") == 0) {
		if (m_pIRCSock) {
			m_pIRCSock->UserDisconnected();
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

				CModule* pModule = m_pUser->GetZNC()->GetModules().FindModule(sModule);

				if (pModule) {
					pModule->OnModNotice(sMsg);
				} else if ((pModule = m_pUser->GetModules().FindModule(sModule))) {
					pModule->OnModNotice(sMsg);
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

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
		}

#ifdef _MODULES
		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			MODULECALLRET(OnUserCTCPReply(sTarget, sCTCP));

			sMsg = "\001" + sCTCP + "\001";
		} else {
			MODULECALLRET(OnUserNotice(sTarget, sMsg));
		}
#endif

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

			if (strncasecmp(sCTCP.c_str(), "DCC ", 4) == 0) {
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
						} else {
							MODULECALLRET(OnDCCUserSend(sTarget, uLongIP, uPort, sFile, uFileSize));
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
							PutServ(":" + sTarget + "!znc@znc.com PRIVMSG " + GetNick() + " :\001DCC ACCEPT " + sFile + " " + CString::ToString(uResumePort) + " " + CString::ToString(uResumeSize) + "\001");
						} else {
							PutStatus("DCC -> [" + GetNick() + "][" + sFile + "] Unable to find send to initiate resume.");
						}
					} else {
						CDCCBounce* pSock = (CDCCBounce*) m_pZNC->GetManager().FindSockByLocalPort(uResumePort);
						if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString::ToString(pSock->GetUserPort()) + " " + sCTCP.Token(4) + "\001");
						}
					}
				} else if (sType.CaseCmp("ACCEPT") == 0) {
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
					} else {
						// Need to lookup the connection by port, filter the port, and forward to the user
						TSocketManager<Csock>& Manager = m_pZNC->GetManager();

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
					pModule->OnModCTCP(sCTCP);
				} else {
					PutStatus("No such module [" + sModule + "]");
				}
#endif
				return;
			}

			MODULECALLRET(OnUserCTCP(sTarget, sCTCP));
			PutIRC("PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
			return;
		}

		if ((m_pUser) && (sTarget.CaseCmp(CString(m_pUser->GetStatusPrefix() + "status")) == 0)) {
			MODULECALLRET(OnStatusCommand(sMsg));
			UserCommand(sMsg);
			return;
		}

		if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
#ifdef _MODULES
			if (m_pUser) {
				CString sModule = sTarget;
				sModule.LeftChomp(m_pUser->GetStatusPrefix().length());

				CModule* pModule = m_pUser->GetModules().FindModule(sModule);
				if (pModule) {
					pModule->OnModCommand(sMsg);
				} else {
					PutStatus("No such module [" + sModule + "]");
				}
			}
#endif
			return;
		}

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " PRIVMSG " + sTarget + " :" + sMsg);
		}

		MODULECALLRET(OnUserMsg(sTarget, sMsg));
		PutIRC("PRIVMSG " + sTarget + " :" + sMsg);
		return;
	}

	PutIRC(sLine);
}

void CUserSock::SetNick(const CString& s) {
   m_sNick = s;

	if (m_pUser) {
		if (m_sNick.CaseCmp(m_pUser->GetNick()) == 0) {
			m_uKeepNickCounter = 0;
		}
	}
}

bool CUserSock::DecKeepNickCounter() {
	if (!m_uKeepNickCounter) {
		return false;
	}

	m_uKeepNickCounter--;
	return true;
}

void CUserSock::UserCommand(const CString& sLine) {
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
	} else if (sCommand.CaseCmp("SHUTDOWN") == 0) {
		CString sQuitMsg = sLine.Token(1, true);

		if (!sQuitMsg.empty()) {
			m_pUser->SetQuitMsg(sQuitMsg);
		}

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

			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Status");
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
		TSocketManager<Csock>& Manager = m_pZNC->GetManager();

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
		m_pUser->GetModules().LoadModule(sMod, sArgs, m_pUser, sModRet);
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
		PutStatus("I'm too stupid to help you with [" + sCommand + "]");
	}
}

void CUserSock::HelpUser() {
	CTable Table;
	Table.AddColumn("Command");
	Table.AddColumn("Arguments");
	Table.AddColumn("Description");

	Table.AddRow(); Table.SetCell("Command", "Version");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "Prints which version of znc this is");
	Table.AddRow(); Table.SetCell("Command", "ListDCCs");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all active DCCs");
	Table.AddRow(); Table.SetCell("Command", "ListMods");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all loaded modules");
	Table.AddRow(); Table.SetCell("Command", "ListChans");	Table.SetCell("Arguments", "");						Table.SetCell("Description", "List all channels");
	Table.AddRow(); Table.SetCell("Command", "ListNicks");	Table.SetCell("Arguments", "<#chan>");				Table.SetCell("Description", "List all nicks on a channel");
	Table.AddRow(); Table.SetCell("Command", "ListServers");	Table.SetCell("Arguments", "");					Table.SetCell("Description", "List all servers");
	Table.AddRow(); Table.SetCell("Command", "AddServer");	Table.SetCell("Arguments", "<host> [[+]port] [pass]");	Table.SetCell("Description", "Add a server to the list");
	Table.AddRow(); Table.SetCell("Command", "RemServer");	Table.SetCell("Arguments", "<host>");				Table.SetCell("Description", "Remove a server from the list");
	Table.AddRow(); Table.SetCell("Command", "Topics");		Table.SetCell("Arguments", "");						Table.SetCell("Description", "Show topics in all channels");
	Table.AddRow(); Table.SetCell("Command", "SetBuffer");	Table.SetCell("Arguments", "<#chan> [linecount]");	Table.SetCell("Description", "Set the buffer count for a channel");
	Table.AddRow(); Table.SetCell("Command", "Shutdown");	Table.SetCell("Arguments", "[message]");			Table.SetCell("Description", "Shutdown znc completely");
	Table.AddRow(); Table.SetCell("Command", "Jump");		Table.SetCell("Arguments", "");						Table.SetCell("Description", "Jump to the next server in the list");
	Table.AddRow(); Table.SetCell("Command", "Send");		Table.SetCell("Arguments", "<nick> <file>");		Table.SetCell("Description", "Send a shell file to a nick on IRC");
	Table.AddRow(); Table.SetCell("Command", "Get");		Table.SetCell("Arguments", "<file>");				Table.SetCell("Description", "Send a shell file to yourself");
	Table.AddRow(); Table.SetCell("Command", "LoadMod");	Table.SetCell("Arguments", "<module>");				Table.SetCell("Description", "Load a module");
	Table.AddRow(); Table.SetCell("Command", "UnloadMod");	Table.SetCell("Arguments", "<module>");				Table.SetCell("Description", "Unload a module");
	Table.AddRow(); Table.SetCell("Command", "ReloadMod");	Table.SetCell("Arguments", "<module>");				Table.SetCell("Description", "Reload a module");

	if (Table.size()) {
		unsigned int uTableIdx = 0;
		CString sLine;

		while (Table.GetLine(uTableIdx++, sLine)) {
			PutStatus(sLine);
		}
	}
}

bool CUserSock::ConnectionFrom(const CString& sHost, int iPort) {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionFrom(" << sHost << ", " << iPort << ")" << endl);
	return m_pZNC->IsHostAllowed(sHost);
}

void CUserSock::AuthUser() {
	if (!m_pZNC) {
		DEBUG_ONLY(cout << "znc not set!" << endl);
		Close();
		return;
	}

	CUser* pUser = m_pZNC->GetUser(m_sUser);

	if ((!pUser) || (!pUser->CheckPass(m_sPass))) {
		if (pUser) {
			pUser->PutStatus("Another user attempted to login as you, with a bad password.");
		}

		PutStatus("Bad username and/or password.");
		PutServ(":irc.znc.com 464 " + GetNick() + " :Password Incorrect");
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

		CIRCSock* pIRCSock = (CIRCSock*) m_pZNC->FindSockByName("IRC::" + pUser->GetUserName());

		if (pIRCSock) {
			m_pIRCSock = pIRCSock;
			pIRCSock->UserConnected(this);
		}

		VOIDMODULECALL(OnUserAttached());
	}
}

void CUserSock::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected();" << endl);
	SetTimeout(900);	// Now that we are connected, let nature take its course
}

void CUserSock::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
}

void CUserSock::Disconnected() {
	if (m_pIRCSock) {
		m_pIRCSock->UserDisconnected();
		m_pIRCSock = NULL;
	}

	VOIDMODULECALL(OnUserDetached());
}

void CUserSock::IRCConnected(CIRCSock* pIRCSock) {
	m_pIRCSock = pIRCSock;
}

void CUserSock::BouncedOff() {
	PutStatusNotice("You are being disconnected because another user just authenticated as you.");
	m_pIRCSock = NULL;
	Close();
}

void CUserSock::IRCDisconnected() {
	m_pIRCSock = NULL;
}

Csock* CUserSock::GetSockObj(const CString& sHost, int iPort) {
	CUserSock* pSock = new CUserSock(sHost, iPort);
	pSock->SetZNC(m_pZNC);

	return pSock;
}

void CUserSock::PutIRC(const CString& sLine) {
	if (m_pIRCSock) {
		m_pIRCSock->PutServ(sLine);
	}
}

void CUserSock::PutServ(const CString& sLine) {
	DEBUG_ONLY(cout << GetSockName() << " -> [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CUserSock::PutStatusNotice(const CString& sLine) {
	PutModNotice("status", sLine);
}

void CUserSock::PutStatus(const CString& sLine) {
	PutModule("status", sLine);
}

void CUserSock::PutModNotice(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << GetSockName() << " -> [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com NOTICE " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com NOTICE " + GetNick() + " :" + sLine + "\r\n");
}

void CUserSock::PutModule(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << GetSockName() << " -> [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com PRIVMSG " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.com PRIVMSG " + GetNick() + " :" + sLine + "\r\n");
}

CString CUserSock::GetNick() const {
	CString sRet;

	if ((m_bAuthed) && (m_pIRCSock)) {
		sRet = m_pIRCSock->GetNick();
	}

	return (sRet.empty()) ? m_sNick : sRet;
}

CString CUserSock::GetNickMask() const {
	if (m_pIRCSock) {
		return m_pIRCSock->GetNickMask();
	}

	return GetNick() + "!" + m_pUser->GetIdent() + "@" + m_pUser->GetVHost();
}
