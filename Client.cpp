/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Client.h"
#include "Chan.h"
#include "DCCBounce.h"
#include "IRCSock.h"
#include "Timers.h"
#include "User.h"
#include "znc.h"

#define CALLMOD(MOD, CLIENT, USER, FUNC) {				\
	CModule* pModule = CZNC::Get().GetModules().FindModule(MOD);	\
	if (pModule) {							\
		try {							\
			pModule->SetClient(CLIENT);			\
			pModule->SetUser(USER);				\
			pModule->FUNC;					\
			pModule->SetClient(NULL);			\
			pModule->SetUser(NULL);				\
		} catch (CModule::EModException e) {			\
			if (e == CModule::UNLOAD) {			\
				CZNC::Get().GetModules().UnloadModule(MOD);	\
			}						\
		}							\
	} else {							\
		pModule = (USER)->GetModules().FindModule(MOD);	\
		if (pModule) {						\
			try {						\
				pModule->SetClient(CLIENT);		\
				pModule->FUNC;				\
				pModule->SetClient(NULL);		\
			} catch (CModule::EModException e) {		\
				if (e == CModule::UNLOAD) {		\
					(USER)->GetModules().UnloadModule(MOD);	\
				}					\
			}						\
		} else {						\
			PutStatus("No such module [" + MOD + "]");	\
		}							\
	}								\
}

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

	sLine.TrimRight("\n\r");

	DEBUG_ONLY(cout << "(" << ((m_pUser) ? m_pUser->GetUserName() : GetRemoteIP()) << ") CLI -> ZNC [" << sLine << "]" << endl);

#ifdef _MODULES
	if (IsAttached()) {
		MODULECALL(OnUserRaw(sLine), m_pUser, this, return);
	} else {
		if (CZNC::Get().GetModules().OnUnknownUserRaw(this, sLine))
			return;
	}
#endif

	CString sCommand = sLine.Token(0);
	if (sCommand.Left(1) == ":") {
		// Evil client! Sending a nickmask prefix on client's command
		// is bad, bad, bad, bad, bad, bad, bad, bad, BAD, B A D!
		sLine = sLine.Token(1, true);
		sCommand = sLine.Token(0);
	}

	if (sCommand.Equals("PASS")) {
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
	} else if (sCommand.Equals("NICK")) {
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
			// No need to forward it
			return;
		}
	} else if (sCommand.Equals("USER")) {
		if (!IsAttached()) {
			if (m_sUser.empty()) {
				m_sUser = sLine.Token(1);
			}

			m_bGotUser = true;

			if ((m_bGotPass) && (m_bGotNick)) {
				AuthUser();
			} else if (!m_bGotPass) {
				PutClient(":irc.znc.in NOTICE AUTH :*** "
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

	if (sCommand.Equals("ZNC")) {
		CString sTarget = sLine.Token(1);
		CString sModCommand;

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
			sModCommand = sLine.Token(2, true);
		} else {
			sTarget  = "status";
			sModCommand = sLine.Token(1, true);
		}

		if (sTarget.Equals("status")) {
			if (sModCommand.empty())
				PutStatus("Hello. How may I help you?");
			else
				UserCommand(sModCommand);
		} else {
#ifdef _MODULES
			if (sModCommand.empty())
				CALLMOD(sTarget, this, m_pUser, PutModule("Hello. How may I help you?"))
			else
				CALLMOD(sTarget, this, m_pUser, OnModCommand(sModCommand))
#endif
		}
		return;
	} else if (sCommand.Equals("DETACH")) {
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
	} else if (sCommand.Equals("PING")) {
		CString sTarget = sLine.Token(1);

		// If the client meant to ping us or we can be sure the server
		// won't answer the ping (=no server connected) -> PONG back.
		// else: It's the server's job to send a PONG.
		if (sTarget.Equals("irc.znc.in") || !m_pIRCSock) {
			PutClient("PONG " + sLine.substr(5));
			return;
		}
	} else if (sCommand.Equals("PONG")) {
		return;	// Block pong replies, we already responded to the pings
	} else if (sCommand.Equals("JOIN")) {
		CString sChans = sLine.Token(1);
		CString sKey = sLine.Token(2);

		if (sChans.Left(1) == ":") {
			sChans.LeftChomp();
		}

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
	} else if (sCommand.Equals("PART")) {
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

		CChan* pChan = m_pUser->FindChan(sChan);

		if (pChan && !pChan->IsOn()) {
			PutStatusNotice("Removing channel [" + sChan + "]");
			m_pUser->DelChan(sChan);
			return;
		}

		sLine = "PART " + sChan;

		if (!sMessage.empty()) {
			sLine += " :" + sMessage;
		}
	} else if (sCommand.Equals("TOPIC")) {
		CString sChan = sLine.Token(1);
		CString sTopic = sLine.Token(2, true);

		if (!sTopic.empty()) {
			if (sTopic.Left(1) == ":")
				sTopic.LeftChomp();
			MODULECALL(OnUserTopic(sChan, sTopic), m_pUser, this, return);
			sLine = "TOPIC " + sChan + " :" + sTopic;
		} else {
			MODULECALL(OnUserTopicRequest(sChan), m_pUser, this, return);
		}
	} else if (sCommand.Equals("MODE")) {
		CString sTarget = sLine.Token(1);
		CString sModes = sLine.Token(2, true);

		if (m_pUser->IsChan(sTarget)) {
			CChan *pChan = m_pUser->FindChan(sTarget);

			if (pChan && sModes.empty() && !pChan->GetModeString().empty()) {
				PutClient(":" + m_pUser->GetIRCServer() + " 324 " + GetNick() + " " + sTarget + " " + pChan->GetModeString());
				if (pChan->GetCreationDate() > 0) {
					PutClient(":" + m_pUser->GetIRCServer() + " 329 " + GetNick() + " " + sTarget + " " + CString(pChan->GetCreationDate()));
				}
				return;
			}
		}
	} else if (sCommand.Equals("QUIT")) {
		m_pUser->UserDisconnected(this);

		Close();	// Treat a client quit as a detach
		return;		// Don't forward this msg.  We don't want the client getting us disconnected.
	} else if (sCommand.Equals("PROTOCTL")) {
		unsigned int i = 1;
		while (!sLine.Token(i).empty()) {
			if (sLine.Token(i).Equals("NAMESX")) {
				m_bNamesx = true;
			} else if (sLine.Token(i).Equals("UHNAMES")) {
				m_bUHNames = true;
			}
			i++;
		}
		return;	// If the server understands it, we already enabled namesx / uhnames
	} else if (sCommand.Equals("NOTICE")) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true);

		if (sMsg.Left(1) == ":") {
			sMsg.LeftChomp();
		}

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
#ifdef _MODULES
			if (!sTarget.Equals("status")) {
				CALLMOD(sTarget, this, m_pUser, OnModNotice(sMsg));
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
			// Some lagmeters do a NOTICE to their own nick, ignore those.
			if (!sTarget.Equals(m_sNick))
				PutStatus("Your notice to [" + sTarget + "] got lost, "
						"you are not connected to IRC!");
			return;
		}

		CChan* pChan = m_pUser->FindChan(sTarget);

		if ((pChan) && (pChan->KeepBuffer())) {
			pChan->AddBuffer(":" + GetNickMask() + " NOTICE " + sTarget + " :" + m_pUser->AddTimestamp(sMsg));
		}

		// Relay to the rest of the clients that may be connected to this user
		if (m_pUser->IsChan(sTarget)) {
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
	} else if (sCommand.Equals("PRIVMSG")) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true);

		if (sMsg.Left(1) == ":") {
			sMsg.LeftChomp();
		}

		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			if (sCTCP.Equals("DCC ", false, 4) && m_pUser->BounceDCCs()) {
				CString sType = sCTCP.Token(1);
				CString sFile = sCTCP.Token(2);
				unsigned long uLongIP = sCTCP.Token(3).ToULong();
				unsigned short uPort = sCTCP.Token(4).ToUShort();
				unsigned long uFileSize = sCTCP.Token(5).ToULong();
				CString sIP = (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP();

				if (!m_pUser->UseClientIP()) {
					uLongIP = CUtils::GetLongIP(GetRemoteIP());
				}

				if (sType.Equals("CHAT")) {
					if (!sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
						unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, "", true, m_pUser, (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP(), "");
						if (uBNCPort) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + "\001");
						}
					}
				} else if (sType.Equals("SEND")) {
					// DCC SEND readme.txt 403120438 5550 1104

					if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
						if (sTarget.Equals("status")) {
							CString sPath = m_pUser->GetDLPath();
							if (!CFile::Exists(sPath)) {
								PutStatus("Could not create [" + sPath + "] directory.");
								return;
							} else if (!CFile::IsDir(sPath)) {
								PutStatus("Error: [" + sPath + "] is not a directory.");
								return;
							}

							CString sLocalFile = sPath + "/" + sFile;

							m_pUser->GetFile(GetNick(), CUtils::GetIP(uLongIP), uPort, sLocalFile, uFileSize);
						} else {
							MODULECALL(OnDCCUserSend(CString(m_pUser->GetStatusPrefix() + sTarget), uLongIP, uPort, sFile, uFileSize), m_pUser, this, return);
						}
					} else {
						unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, sFile, false, m_pUser, (m_pIRCSock) ? m_pIRCSock->GetLocalIP() : GetLocalIP(), "");
						if (uBNCPort) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
						}
					}
				} else if (sType.Equals("RESUME")) {
					// PRIVMSG user :DCC RESUME "znc.o" 58810 151552
					unsigned short uResumePort = sCTCP.Token(3).ToUShort();
					unsigned long uResumeSize = sCTCP.Token(4).ToULong();

					// Need to lookup the connection by port, filter the port, and forward to the user
					CString sStatusPrefix = m_pUser->GetStatusPrefix();
					if (sTarget.Equals(sStatusPrefix, false, sStatusPrefix.length())) {
						if (m_pUser->ResumeFile(uResumePort, uResumeSize)) {
							PutClient(":" + sTarget + "!znc@znc.in PRIVMSG " + GetNick() + " :\001DCC ACCEPT " + sFile + " " + CString(uResumePort) + " " + CString(uResumeSize) + "\001");
						} else {
							PutStatus("DCC -> [" + GetNick() + "][" + sFile + "] Unable to find send to initiate resume.");
						}
					} else {
						CDCCBounce* pSock = (CDCCBounce*) CZNC::Get().GetManager().FindSockByLocalPort(uResumePort);
						if (pSock && pSock->GetSockName().Equals("DCC::", false, 5)) {
							PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sCTCP.Token(4) + "\001");
						}
					}
				} else if (sType.Equals("ACCEPT")) {
					CString sStatusPrefix = m_pUser->GetStatusPrefix();
					if (!sTarget.Equals(sStatusPrefix, false, sStatusPrefix.length())) {
						// Need to lookup the connection by port, filter the port, and forward to the user
						CSockManager& Manager = CZNC::Get().GetManager();

						for (unsigned int a = 0; a < Manager.size(); a++) {
							CDCCBounce* pSock = (CDCCBounce*) Manager[a];

							if (pSock && pSock->GetSockName().Equals("DCC::", false, 5)) {
								if (pSock->GetUserPort() == sCTCP.Token(3).ToUShort()) {
									PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sCTCP.Token(4) + "\001");
								}
							}
						}
					}
				}

				return;
			}

			if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
				if (sTarget.Equals("status")) {
					StatusCTCP(sCTCP);
				} else {
#ifdef _MODULES
					CALLMOD(sTarget, this, m_pUser, OnModCTCP(sCTCP));
#endif
				}
				return;
			}

			CChan* pChan = m_pUser->FindChan(sTarget);

			if (sCTCP.Token(0).Equals("ACTION")) {
				CString sMessage = sCTCP.Token(1, true);
				MODULECALL(OnUserAction(sTarget, sMessage), m_pUser, this, return);
				sCTCP = "ACTION " + sMessage;

				if (pChan && pChan->KeepBuffer()) {
					pChan->AddBuffer(":" + GetNickMask() + " PRIVMSG " + sTarget + " :\001ACTION " + m_pUser->AddTimestamp(sMessage) + "\001");
				}

				// Relay to the rest of the clients that may be connected to this user
				if (m_pUser->IsChan(sTarget)) {
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

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
			if (sTarget.Equals("status")) {
				MODULECALL(OnStatusCommand(sMsg), m_pUser, this, return);
				UserCommand(sMsg);
			} else {
#ifdef _MODULES
				CALLMOD(sTarget, this, m_pUser, OnModCommand(sMsg));
#endif
			}
			return;
		}

		MODULECALL(OnUserMsg(sTarget, sMsg), m_pUser, this, return);

		if (!m_pIRCSock) {
			// Some lagmeters do a PRIVMSG to their own nick, ignore those.
			if (!sTarget.Equals(m_sNick))
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

		if (m_pUser->IsChan(sTarget)) {
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
}

void CClient::StatusCTCP(const CString& sLine) {
	CString sCommand = sLine.Token(0);

	if (sCommand.Equals("PING")) {
		PutStatusNotice("\001PING " + sLine.Token(1, true) + "\001");
	} else if (sCommand.Equals("VERSION")) {
		PutStatusNotice("\001VERSION " + CZNC::GetTag() + "\001");
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
	PutClient(":irc.znc.in 464 " + GetNick() + " :" + sReason);
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

	MODULECALL(OnClientLogin(), m_pUser, this, );
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
	SetTimeout(240, TMO_READ);	// Now that we are connected, let nature take its course
}

void CClient::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
}

void CClient::Disconnected() {
	DEBUG_ONLY(cout << GetSockName() << " == Disconnected()" << endl);
	if (m_pUser) {
		m_pUser->UserDisconnected(this);
	}

	m_pIRCSock = NULL;

	MODULECALL(OnClientDisconnect(), m_pUser, this, );
}

void CClient::ReachedMaxBuffer() {
	DEBUG_ONLY(cout << GetSockName() << " == ReachedMaxBuffer()" << endl);
	if (IsAttached()) {
		PutClient("ERROR :Closing link [Too long raw line]");
	}
	Close();
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

void CClient::PutIRC(const CString& sLine) {
	if (m_pIRCSock) {
		m_pIRCSock->PutIRC(sLine);
	}
}

void CClient::PutClient(const CString& sLine) {
	DEBUG_ONLY(cout << "(" << ((m_pUser) ? m_pUser->GetUserName() : GetRemoteIP()) << ") ZNC -> CLI [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CClient::PutStatusNotice(const CString& sLine) {
	PutModNotice("status", sLine);
}

unsigned int CClient::PutStatus(const CTable& table) {
	unsigned int idx = 0;
	CString sLine;
	while (table.GetLine(idx++, sLine))
		PutStatus(sLine);
	return idx - 1;
}

void CClient::PutStatus(const CString& sLine) {
	PutModule("status", sLine);
}

void CClient::PutModNotice(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " + GetNick() + " :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " << GetNick() << " :" << sLine << "]" << endl);
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " + GetNick() + " :" + sLine + "\r\n");
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
		sHost = "irc.znc.in";
	}

	return GetNick() + "!" + m_pUser->GetIdent() + "@" + sHost;
}
