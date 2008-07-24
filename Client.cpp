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
#include "Timers.h"
#include "User.h"
#include "znc.h"

CClient::~CClient() {
	if (!m_spAuth.IsNull()) {
		CClientAuth* pAuth = (CClientAuth*) &(*m_spAuth);
		pAuth->SetClient(NULL);
	}
	if (m_pUser != NULL) {
		m_pUser->AddBytesRead(GetBytesRead());
		m_pUser->AddBytesWritten(GetBytesWritten());
	}
	if (m_pTimeout) {
		CZNC::Get().GetManager().DelCronByAddr(m_pTimeout);
	}
}

void CClient::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
		sLine.RightChomp();
	}

	DEBUG_ONLY(cout << "(" << ((m_pUser) ? m_pUser->GetUserName() : CString("")) << ") CLI -> ZNC [" << sLine << "]" << endl);

#ifdef _MODULES
	if (IsAttached()) {
		MODULECALL(OnUserRaw(sLine), m_pUser, this, return);
	}
#endif

	CString sCommand = sLine.Token(0);
	if (sCommand.Left(1) == ":") {
		// Evil client! Sending a nickmask prefix on client's command
		// is bad, bad, bad, bad, bad, bad, bad, bad, BAD, B A D!
		sLine = sLine.Token(1, true);
		sCommand = sLine.Token(0);
	}

	if (sCommand.CaseCmp("PASS") == 0) {
		if (!IsAttached()) {
			m_bGotPass = true;
			m_sPass = sLine.Token(1);

			if (m_sPass.find(":") != CString::npos) {
				m_sUser = m_sPass.Token(0, false, ":");
				m_sPass = m_sPass.Token(1, true, ":");
			}

			if ((m_bGotNick) && (m_bGotUser)) {
				AuthUser();
			}

			return;		// Don't forward this msg.  ZNC has already registered us.
		}
	} else if (sCommand.CaseCmp("NICK") == 0) {
		CString sNick = sLine.Token(1);
		if (sNick.Left(1) == ":") {
			sNick.LeftChomp();
		}

		if (!IsAttached()) {
			m_sNick = sNick;
			m_bGotNick = true;

			if ((m_bGotPass) && (m_bGotUser)) {
				AuthUser();
			}
			return;		// Don't forward this msg.  ZNC will handle nick changes until auth is complete
		}

		if (!m_pIRCSock) {
			// No need to check against IRC nick or to forward it
			return;
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
		if (!IsAttached()) {
			if (m_sUser.empty()) {
				m_sUser = sLine.Token(1);
			}

			m_bGotUser = true;

			if ((m_bGotPass) && (m_bGotNick)) {
				AuthUser();
			} else if (!m_bGotPass) {
				PutClient(":irc.znc.com NOTICE AUTH :*** "
					"You need to send your password. "
					"Try /quote PASS <username>:<password>");
			}

			return;		// Don't forward this msg.  ZNC has already registered us.
		}
	}

	if (!m_pUser) {
		// Only NICK, USER and PASS are allowed before login
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
	} else if (sCommand.CaseCmp("PING") == 0) {
		CString sTarget = sLine.Token(1);

		// If the client meant to ping us or we can be sure the server
		// won't answer the ping (=no server connected) -> PONG back.
		// else: It's the server's job to send a PONG.
		if (sTarget.CaseCmp("irc.znc.com") == 0 || !m_pIRCSock) {
			PutClient("PONG " + sLine.substr(5));
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
				MODULECALL(OnUserJoin(sChannel, sKey), m_pUser, this, continue);

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

		if (sChan.Left(1) == ":") {
			// I hate those broken clients, I hate them so much, I really hate them...
			sChan.LeftChomp();
		}
		if (sMessage.Left(1) == ":") {
			sMessage.LeftChomp();
		}

		MODULECALL(OnUserPart(sChan, sMessage), m_pUser, this, return);

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
	} else if (sCommand.CaseCmp("TOPIC") == 0) {
		CString sChan = sLine.Token(1);
		CString sTopic = sLine.Token(2, true);
		bool bUnset = false;

		if (sTopic.Left(1) == ":") {
			sTopic.LeftChomp();
			if (sTopic.empty())
				bUnset = true;
		}

		MODULECALL(OnUserTopic(sChan, sTopic), m_pUser, this, return);

		sLine = "TOPIC " + sChan;

		if (!sTopic.empty() || bUnset) {
			sLine += " :" + sTopic;
		}
	} else if (sCommand.CaseCmp("MODE") == 0) {
		CString sTarget = sLine.Token(1);
		CString sModes = sLine.Token(2, true);

		if (m_pUser && m_pUser->IsChan(sTarget)) {
		    CChan *pChan = m_pUser->FindChan(sTarget);

		    if (pChan && sModes.empty()) {
			PutClient(":" + m_pUser->GetIRCServer() + " 324 " + GetNick() + " " + sTarget + " " + pChan->GetModeString());
			if (pChan->GetCreationDate() > 0) {
			    PutClient(":" + m_pUser->GetIRCServer() + " 329 " + GetNick() + " " + sTarget + " " + CString(pChan->GetCreationDate()));
			}
			return;
		    }
		}
	} else if (sCommand.CaseCmp("QUIT") == 0) {
		if (m_pUser) {
			m_pUser->UserDisconnected(this);
		}

		Close();	// Treat a client quit as a detach
		return;		// Don't forward this msg.  We don't want the client getting us disconnected.
	} else if (sCommand.CaseCmp("PROTOCTL") == 0) {
		unsigned int i = 1;
		while (!sLine.Token(i).empty()) {
			if (sLine.Token(i).CaseCmp("NAMESX") == 0) {
				m_bNamesx = true;
			} else if (sLine.Token(i).CaseCmp("UHNAMES") == 0) {
				m_bUHNames = true;
			}
			i++;
		}
		return;	// If the server understands it, we already enabled namesx / uhnames
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

			MODULECALL(OnUserCTCPReply(sTarget, sCTCP), m_pUser, this, return);

			sMsg = "\001" + sCTCP + "\001";
		} else {
			MODULECALL(OnUserNotice(sTarget, sMsg), m_pUser, this, return);
		}
#endif

		if (!m_pIRCSock) {
			PutStatus("Your message to [" + sTarget + "] got lost, "
					"you are not connected to IRC!");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " NOTICE " + sTarget + " :" + m_pUser->AddTimestamp(sMsg));
		}

		// Relay to the rest of the clients that may be connected to this user
		if (m_pUser && m_pUser->IsChan(sTarget)) {
			vector<CClient*>& vClients = m_pUser->GetClients();

			for (unsigned int a = 0; a < vClients.size(); a++) {
				CClient* pClient = vClients[a];

				if (pClient != this) {
					pClient->PutClient(":" + GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
				}
			}
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
							PutIRC("PRIVMSG " + sTarget + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + "\001");
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
							MODULECALL(OnDCCUserSend(sTarget, uLongIP, uPort, sFile, uFileSize), m_pUser, this, return);
						}
					} else {
						unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, sFile, false, m_pUser, (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP(), "");
						if (uBNCPort) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
						}
					}
				} else if (sType.CaseCmp("RESUME") == 0) {
					// PRIVMSG user :DCC RESUME "znc.o" 58810 151552
					unsigned short uResumePort = atoi(sCTCP.Token(3).c_str());
					unsigned long uResumeSize = strtoul(sCTCP.Token(4).c_str(), NULL, 10);

					// Need to lookup the connection by port, filter the port, and forward to the user
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
						if ((m_pUser) && (m_pUser->ResumeFile(uResumePort, uResumeSize))) {
							PutClient(":" + sTarget + "!znc@znc.com PRIVMSG " + GetNick() + " :\001DCC ACCEPT " + sFile + " " + CString(uResumePort) + " " + CString(uResumeSize) + "\001");
						} else {
							PutStatus("DCC -> [" + GetNick() + "][" + sFile + "] Unable to find send to initiate resume.");
						}
					} else {
						CDCCBounce* pSock = (CDCCBounce*) CZNC::Get().GetManager().FindSockByLocalPort(uResumePort);
						if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sCTCP.Token(4) + "\001");
						}
					}
				} else if (sType.CaseCmp("ACCEPT") == 0) {
					if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
					} else {
						// Need to lookup the connection by port, filter the port, and forward to the user
						CSockManager& Manager = CZNC::Get().GetManager();

						for (unsigned int a = 0; a < Manager.size(); a++) {
							CDCCBounce* pSock = (CDCCBounce*) Manager[a];

							if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
								if (pSock->GetUserPort() == atoi(sCTCP.Token(3).c_str())) {
									PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sCTCP.Token(4) + "\001");
								}
							}
						}
					}
				}

				return;
			}

			if (strncasecmp(sTarget.c_str(), m_pUser->GetStatusPrefix().c_str(), m_pUser->GetStatusPrefix().length()) == 0) {
				CString sModule = sTarget;
				sModule.LeftChomp(m_pUser->GetStatusPrefix().length());

				if (sModule == "status") {
					StatusCTCP(sCTCP);

					return;
				}

#ifdef _MODULES
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

			CChan* pChan = m_pUser->FindChan(sTarget);

			if (sCTCP.Token(0).CaseCmp("ACTION") == 0) {
				CString sMessage = sCTCP.Token(1, true);
				MODULECALL(OnUserAction(sTarget, sMessage), m_pUser, this, return);
				sCTCP = "ACTION " + sMessage;

				if (pChan && pChan->KeepBuffer()) {
					pChan->AddBuffer(":" + GetNickMask() + " PRIVMSG " + sTarget + " :\001ACTION " + m_pUser->AddTimestamp(sMessage) + "\001");
				}

				// Relay to the rest of the clients that may be connected to this user
				if (m_pUser && m_pUser->IsChan(sTarget)) {
					vector<CClient*>& vClients = m_pUser->GetClients();

					for (unsigned int a = 0; a < vClients.size(); a++) {
						CClient* pClient = vClients[a];

						if (pClient != this) {
							pClient->PutClient(":" + GetNickMask() + " PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
						}
					}
				}
			} else {
				MODULECALL(OnUserCTCP(sTarget, sCTCP), m_pUser, this, return);
			}

			PutIRC("PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
			return;
		}

		if ((m_pUser) && (sTarget.CaseCmp(CString(m_pUser->GetStatusPrefix() + "status")) == 0)) {
			MODULECALL(OnStatusCommand(sMsg), m_pUser, this, return);
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

		MODULECALL(OnUserMsg(sTarget, sMsg), m_pUser, this, return);

		if (!m_pIRCSock) {
			PutStatus("Your message to [" + sTarget + "] got lost, "
					"you are not connected to IRC!");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " PRIVMSG " + sTarget + " :" + m_pUser->AddTimestamp(sMsg));
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

void CClient::StatusCTCP(const CString& sLine) {
	CString sCommand = sLine.Token(0);

	if (sCommand.CaseCmp("PING") == 0) {
		PutStatusNotice("\001PING " + sLine.Token(1, true) + "\001");
	} else if (sCommand.CaseCmp("VERSION") == 0) {
		PutStatusNotice("\001VERSION " + CZNC::GetTag() + "\001");
	}
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
	} else if (sCommand.CaseCmp("MOTD") == 0) {
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

	Table.AddRow();
	Table.SetCell("Command", "Version");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Prints which version of znc this is");

	Table.AddRow();
	Table.SetCell("Command", "MOTD");
	Table.SetCell("Arguments", "");
	Table.SetCell("Description", "Show the message of the day");

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

	if (m_pUser) {
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

		if (m_pUser->IsAdmin()) {
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
			Table.SetCell("Command", "Broadcast");
			Table.SetCell("Arguments", "[message]");
			Table.SetCell("Description", "Broadcast a message to all users");

			Table.AddRow();
			Table.SetCell("Command", "Shutdown");
			Table.SetCell("Arguments", "[message]");
			Table.SetCell("Description", "Shutdown znc completely");
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
	/*
#ifdef _MODULES
	if (CZNC::Get().GetModules().OnLoginAttempt(m_sUser, m_sPass, *this)) {
		return;
	}
#endif

	CUser* pUser = CZNC::Get().GetUser(m_sUser);

	if (pUser && pUser->CheckPass(m_sPass)) {
		AcceptLogin(*pUser);
	} else {
		if (pUser) {
			pUser->PutStatus("Another client attempted to login as you, with a bad password.");
		}

		RefuseLogin();
	}
	*/

	m_spAuth = new CClientAuth(this, m_sUser, m_sPass);

	CZNC::Get().AuthUser(m_spAuth);
}

CClientAuth::CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword)
		: CAuthBase(sUsername, sPassword, pClient->GetRemoteIP()) {
	m_pClient = pClient;
}

void CClientAuth::RefuseLogin(const CString& sReason) {
	if (m_pClient) {
		m_pClient->RefuseLogin(sReason);
	}
#ifdef _MODULES
	CZNC::Get().GetModules().OnFailedLogin(GetUsername(), GetRemoteIP());
#endif
}

void CClient::RefuseLogin(const CString& sReason) {
	if (m_pTimeout) {
		m_pTimeout->Stop();
		m_pTimeout = NULL;
	}

	PutStatus("Bad username and/or password.");
	PutClient(":irc.znc.com 464 " + GetNick() + " :" + sReason);
	Close();
}

void CClientAuth::AcceptLogin(CUser& User) {
	if (m_pClient) {
		m_pClient->AcceptLogin(User);
	}
}

void CClient::AcceptLogin(CUser& User) {
	m_sPass = "";
	m_pUser = &User;

	if (m_pTimeout) {
		m_pTimeout->Stop();
		m_pTimeout = NULL;
	}

	SetSockName("USR::" + m_pUser->GetUserName());

	m_pIRCSock = (CIRCSock*) CZNC::Get().FindSockByName("IRC::" + m_pUser->GetUserName());
	m_pUser->UserConnected(this);

	SendMotd();

	MODULECALL(OnUserAttached(), m_pUser, this, );
}

void CClient::StartLoginTimeout() {
	m_pTimeout = new CClientTimeout(this);
	CZNC::Get().GetManager().AddCron(m_pTimeout);
}

void CClient::LoginTimeout() {
	PutClient("ERROR :Closing link [Timeout]");
	Close(Csock::CLT_AFTERWRITE);
	if (m_pTimeout) {
		m_pTimeout->Stop();
		m_pTimeout = NULL;
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
	if (m_pUser) {
		m_pUser->UserDisconnected(this);
	}

	m_pIRCSock = NULL;

	MODULECALL(OnUserDetached(), m_pUser, this, );
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
	pSock->StartLoginTimeout();
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

CString CClient::GetNick(bool bAllowIRCNick) const {
	CString sRet;

	if ((bAllowIRCNick) && (IsAttached()) && (m_pIRCSock)) {
		sRet = m_pIRCSock->GetNick();
	}

	return (sRet.empty()) ? m_sNick : sRet;
}

CString CClient::GetNickMask() const {
	if (m_pIRCSock && m_pIRCSock->IsAuthed()) {
		return m_pIRCSock->GetNickMask();
	}

	CString sHost = m_pUser->GetVHost();
	if (sHost.empty()) {
		sHost = "irc.znc.com";
	}

	return GetNick() + "!" + m_pUser->GetIdent() + "@" + sHost;
}
