/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/znc.h>
#include <znc/WebModules.h>

using std::vector;

#define CALLMOD(MOD, CLIENT, USER, NETWORK, FUNC) {  \
	CModule *pModule = NULL;  \
	if (NETWORK && (pModule = (NETWORK)->GetModules().FindModule(MOD))) {  \
		try {  \
			pModule->SetClient(CLIENT);  \
			pModule->FUNC;  \
			pModule->SetClient(NULL);  \
		} catch (CModule::EModException e) {  \
			if (e == CModule::UNLOAD) {  \
				(NETWORK)->GetModules().UnloadModule(MOD);  \
			}  \
		}  \
	} else if ((pModule = (USER)->GetModules().FindModule(MOD))) {  \
		try {  \
			pModule->SetClient(CLIENT);  \
			pModule->SetNetwork(NETWORK);  \
			pModule->FUNC;  \
			pModule->SetClient(NULL);  \
			pModule->SetNetwork(NULL);  \
		} catch (CModule::EModException e) {  \
			if (e == CModule::UNLOAD) {  \
				(USER)->GetModules().UnloadModule(MOD);  \
			}  \
		}  \
	} else if ((pModule = CZNC::Get().GetModules().FindModule(MOD))) {  \
		try {  \
			pModule->SetClient(CLIENT);  \
			pModule->SetNetwork(NETWORK);  \
			pModule->SetUser(USER);  \
			pModule->FUNC;  \
			pModule->SetClient(NULL);  \
			pModule->SetNetwork(NULL);  \
			pModule->SetUser(NULL);  \
		} catch (CModule::EModException e) {  \
			if (e == CModule::UNLOAD) {  \
					CZNC::Get().GetModules().UnloadModule(MOD);  \
			}  \
		}  \
	} else {  \
		PutStatus("No such module [" + MOD + "]");  \
	}  \
}

CClient::~CClient() {
	if (!m_spAuth.IsNull()) {
		CClientAuth* pAuth = (CClientAuth*) &(*m_spAuth);
		pAuth->Invalidate();
	}
	if (m_pUser != NULL) {
		m_pUser->AddBytesRead(GetBytesRead());
		m_pUser->AddBytesWritten(GetBytesWritten());
	}
}

void CClient::SendRequiredPasswordNotice() {
	PutClient(":irc.znc.in 464 " + GetNick() + " :Password required");
	PutClient(":irc.znc.in NOTICE AUTH :*** "
			  "You need to send your password. "
			  "Try /quote PASS <username>:<password>");
}

void CClient::ReadLine(const CString& sData) {
	CString sLine = sData;

	sLine.TrimRight("\n\r");

	DEBUG("(" << GetFullName() << ") CLI -> ZNC [" << sLine << "]");

	if (sLine.Left(1) == "@") {
		// TODO support message-tags properly
		sLine = sLine.Token(1, true);
	}

	bool bReturn = false;
	if (IsAttached()) {
		NETWORKMODULECALL(OnUserRaw(sLine), m_pUser, m_pNetwork, this, &bReturn);
	} else {
		GLOBALMODULECALL(OnUnknownUserRaw(this, sLine), &bReturn);
	}
	if (bReturn) return;

	CString sCommand = sLine.Token(0);
	if (sCommand.Left(1) == ":") {
		// Evil client! Sending a nickmask prefix on client's command
		// is bad, bad, bad, bad, bad, bad, bad, bad, BAD, B A D!
		sLine = sLine.Token(1, true);
		sCommand = sLine.Token(0);
	}

	if (!IsAttached()) { // The following commands happen before authentication with ZNC
		if (sCommand.Equals("PASS")) {
			m_bGotPass = true;

			CString sAuthLine = sLine.Token(1, true).TrimPrefix_n();

			// [user[/network]:]password
			if (sAuthLine.find(":") == CString::npos) {
				m_sPass = sAuthLine;
				sAuthLine = "";
			} else {
				m_sPass = sAuthLine.Token(1, true, ":");
				sAuthLine = sAuthLine.Token(0, false, ":");
			}

			if (!sAuthLine.empty()) {
				m_sUser = sAuthLine.Token(0, false, "/");
				m_sNetwork = sAuthLine.Token(1, true, "/");
			}

			AuthUser();
			return;  // Don't forward this msg.  ZNC has already registered us.
		} else if (sCommand.Equals("NICK")) {
			CString sNick = sLine.Token(1).TrimPrefix_n();

			m_sNick = sNick;
			m_bGotNick = true;

			AuthUser();
			return;  // Don't forward this msg.  ZNC will handle nick changes until auth is complete
		} else if (sCommand.Equals("USER")) {
			// user[/network]
			CString sAuthLine = sLine.Token(1);

			if (m_sUser.empty() && !sAuthLine.empty()) {
				m_sUser = sAuthLine.Token(0, false, "/");
				m_sNetwork = sAuthLine.Token(1, true, "/");
			}

			m_bGotUser = true;
			if (m_bGotPass) {
				AuthUser();
			} else if (!m_bInCap) {
				SendRequiredPasswordNotice();
			}

			return;  // Don't forward this msg.  ZNC has already registered us.
		}
	}

	if (sCommand.Equals("CAP")) {
		HandleCap(sLine);

		// Don't let the client talk to the server directly about CAP,
		// we don't want anything enabled that ZNC does not support.
		return;
	}

	if (!m_pUser) {
		// Only CAP, NICK, USER and PASS are allowed before login
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
			if (sModCommand.empty())
				CALLMOD(sTarget, this, m_pUser, m_pNetwork, PutModule("Hello. How may I help you?"))
			else
				CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModCommand(sModCommand))
		}
		return;
	} else if (sCommand.Equals("PING")) {
		// All PONGs are generated by ZNC. We will still forward this to
		// the ircd, but all PONGs from irc will be blocked.
		if (sLine.length() >= 5)
			PutClient(":irc.znc.in PONG irc.znc.in " + sLine.substr(5));
		else
			PutClient(":irc.znc.in PONG irc.znc.in");
	} else if (sCommand.Equals("PONG")) {
		// Block PONGs, we already responded to the pings
		return;
	} else if (sCommand.Equals("QUIT")) {
		Close(Csock::CLT_AFTERWRITE); // Treat a client quit as a detach
		return;                       // Don't forward this msg.  We don't want the client getting us disconnected.
	} else if (sCommand.Equals("PROTOCTL")) {
		VCString vsTokens;
		VCString::const_iterator it;
		sLine.Token(1, true).Split(" ", vsTokens, false);

		for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
			if (*it == "NAMESX") {
				m_bNamesx = true;
			} else if (*it == "UHNAMES") {
				m_bUHNames = true;
			}
		}
		return;  // If the server understands it, we already enabled namesx / uhnames
	} else if (sCommand.Equals("NOTICE")) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true).TrimPrefix_n();

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
			if (!sTarget.Equals("status")) {
				CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModNotice(sMsg));
			}
			return;
		}

		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			NETWORKMODULECALL(OnUserCTCPReply(sTarget, sCTCP), m_pUser, m_pNetwork, this, &bReturn);
			if (bReturn) return;

			sMsg = "\001" + sCTCP + "\001";
		} else {
			NETWORKMODULECALL(OnUserNotice(sTarget, sMsg), m_pUser, m_pNetwork, this, &bReturn);
			if (bReturn) return;
		}

		if (!GetIRCSock()) {
			// Some lagmeters do a NOTICE to their own nick, ignore those.
			if (!sTarget.Equals(m_sNick))
				PutStatus("Your notice to [" + sTarget + "] got lost, "
						"you are not connected to IRC!");
			return;
		}

		if (m_pNetwork) {
			CChan* pChan = m_pNetwork->FindChan(sTarget);

			if ((pChan) && (!pChan->AutoClearChanBuffer())) {
				pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " NOTICE " + _NAMEDFMT(sTarget) + " :{text}", sMsg);
			}

			// Relay to the rest of the clients that may be connected to this user
			if (m_pNetwork->IsChan(sTarget)) {
				vector<CClient*>& vClients = GetClients();

				for (unsigned int a = 0; a < vClients.size(); a++) {
					CClient* pClient = vClients[a];

					if (pClient != this) {
						pClient->PutClient(":" + GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
					}
				}
			}

			PutIRC("NOTICE " + sTarget + " :" + sMsg);
			return;
		}
	} else if (sCommand.Equals("PRIVMSG")) {
		CString sTarget = sLine.Token(1);
		CString sMsg = sLine.Token(2, true).TrimPrefix_n();

		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
				if (sTarget.Equals("status")) {
					StatusCTCP(sCTCP);
				} else {
					CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModCTCP(sCTCP));
				}
				return;
			}

			if (m_pNetwork) {
				CChan* pChan = m_pNetwork->FindChan(sTarget);

				if (sCTCP.Token(0).Equals("ACTION")) {
					CString sMessage = sCTCP.Token(1, true);
					NETWORKMODULECALL(OnUserAction(sTarget, sMessage), m_pUser, m_pNetwork, this, &bReturn);
					if (bReturn) return;
					sCTCP = "ACTION " + sMessage;

					if (pChan && (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline())) {
						pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :\001ACTION {text}\001", sMessage);
					}

					// Relay to the rest of the clients that may be connected to this user
					if (m_pNetwork->IsChan(sTarget)) {
						vector<CClient*>& vClients = GetClients();

						for (unsigned int a = 0; a < vClients.size(); a++) {
							CClient* pClient = vClients[a];

							if (pClient != this) {
								pClient->PutClient(":" + GetNickMask() + " PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
							}
						}
					}
				} else {
					NETWORKMODULECALL(OnUserCTCP(sTarget, sCTCP), m_pUser, m_pNetwork, this, &bReturn);
					if (bReturn) return;
				}

				PutIRC("PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
			}

			return;
		}

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
			if (sTarget.Equals("status")) {
				UserCommand(sMsg);
			} else {
				CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModCommand(sMsg));
			}
			return;
		}

		NETWORKMODULECALL(OnUserMsg(sTarget, sMsg), m_pUser, m_pNetwork, this, &bReturn);
		if (bReturn) return;

		if (!GetIRCSock()) {
			// Some lagmeters do a PRIVMSG to their own nick, ignore those.
			if (!sTarget.Equals(m_sNick))
				PutStatus("Your message to [" + sTarget + "] got lost, "
						"you are not connected to IRC!");
			return;
		}

		if (m_pNetwork) {
			CChan* pChan = m_pNetwork->FindChan(sTarget);

			if ((pChan) && (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline())) {
				pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :{text}", sMsg);
			}

			PutIRC("PRIVMSG " + sTarget + " :" + sMsg);

			// Relay to the rest of the clients that may be connected to this user

			if (m_pNetwork->IsChan(sTarget)) {
				vector<CClient*>& vClients = GetClients();

				for (unsigned int a = 0; a < vClients.size(); a++) {
					CClient* pClient = vClients[a];

					if (pClient != this) {
						pClient->PutClient(":" + GetNickMask() + " PRIVMSG " + sTarget + " :" + sMsg);
					}
				}
			}
		}

		return;
	}

	if (!m_pNetwork) {
		return; // The following commands require a network
	}

	if (sCommand.Equals("DETACH")) {
		CString sChannels = sLine.Token(1).TrimPrefix_n();

		if (sChannels.empty()) {
			PutStatusNotice("Usage: /detach <#chan>");
			return;
		}

		VCString vChans;
		sChannels.Split(",", vChans, false);
		sChannels.clear();

		for (VCString::const_iterator channelIterator = vChans.begin();
				channelIterator != vChans.end();
				++channelIterator)
		{
			CString sChannel = *channelIterator;

			CChan *pChannel = m_pNetwork->FindChan(sChannel);
			if (pChannel) {
				pChannel->DetachUser();
			} else {
				PutStatusNotice("You are not on [" + sChannel + "]");
			}
		}

		return;
	} else if (sCommand.Equals("JOIN")) {
		CString sChans = sLine.Token(1).TrimPrefix_n();
		CString sKey = sLine.Token(2);

		VCString vChans;
		sChans.Split(",", vChans, false);
		sChans.clear();

		for (unsigned int a = 0; a < vChans.size(); a++) {
			CString sChannel = vChans[a];
			bool bContinue = false;
			NETWORKMODULECALL(OnUserJoin(sChannel, sKey), m_pUser, m_pNetwork, this, &bContinue);
			if (bContinue) continue;

			CChan* pChan = m_pNetwork->FindChan(sChannel);
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
		CString sChans = sLine.Token(1).TrimPrefix_n();
		CString sMessage = sLine.Token(2, true).TrimPrefix_n();

		VCString vChans;
		sChans.Split(",", vChans, false);
		sChans.clear();

		for (VCString::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
			CString sChan = *it;
			bool bContinue = false;
			NETWORKMODULECALL(OnUserPart(sChan, sMessage), m_pUser, m_pNetwork, this, &bContinue);
			if (bContinue) continue;

			CChan* pChan = m_pNetwork->FindChan(sChan);

			if (pChan && !pChan->IsOn()) {
				PutStatusNotice("Removing channel [" + sChan + "]");
				m_pNetwork->DelChan(sChan);
			} else {
				sChans += (sChans.empty()) ? sChan : CString("," + sChan); 
			}
		}

		if (sChans.empty()) {
			return;
		}

		sLine = "PART " + sChans;

		if (!sMessage.empty()) {
			sLine += " :" + sMessage;
		}
	} else if (sCommand.Equals("TOPIC")) {
		CString sChan = sLine.Token(1);
		CString sTopic = sLine.Token(2, true).TrimPrefix_n();

		if (!sTopic.empty()) {
			NETWORKMODULECALL(OnUserTopic(sChan, sTopic), m_pUser, m_pNetwork, this, &bReturn);
			if (bReturn) return;
			sLine = "TOPIC " + sChan + " :" + sTopic;
		} else {
			NETWORKMODULECALL(OnUserTopicRequest(sChan), m_pUser, m_pNetwork, this, &bReturn);
			if (bReturn) return;
		}
	} else if (sCommand.Equals("MODE")) {
		CString sTarget = sLine.Token(1);
		CString sModes = sLine.Token(2, true);

		if (m_pNetwork->IsChan(sTarget) && sModes.empty()) {
			// If we are on that channel and already received a
			// /mode reply from the server, we can answer this
			// request ourself.

			CChan *pChan = m_pNetwork->FindChan(sTarget);
			if (pChan && pChan->IsOn() && !pChan->GetModeString().empty()) {
				PutClient(":" + m_pNetwork->GetIRCServer() + " 324 " + GetNick() + " " + sTarget + " " + pChan->GetModeString());
				if (pChan->GetCreationDate() > 0) {
					PutClient(":" + m_pNetwork->GetIRCServer() + " 329 " + GetNick() + " " + sTarget + " " + CString(pChan->GetCreationDate()));
				}
				return;
			}
		}
	}

	PutIRC(sLine);
}

void CClient::SetNick(const CString& s) {
	m_sNick = s;
}

void CClient::SetNetwork(CIRCNetwork* pNetwork, bool bDisconnect, bool bReconnect) {
	if (bDisconnect) {
		if (m_pNetwork) {
			m_pNetwork->ClientDisconnected(this);

			// Tell the client they are no longer in these channels.
			const vector<CChan*>& vChans = m_pNetwork->GetChans();
			for (vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
				if (!((*it)->IsDetached())) {
					PutClient(":" + m_pNetwork->GetIRCNick().GetNickMask() + " PART " + (*it)->GetName());
				}
			}
		} else if (m_pUser) {
			m_pUser->UserDisconnected(this);
		}
	}

	m_pNetwork = pNetwork;

	if (bReconnect) {
		if (m_pNetwork) {
			m_pNetwork->ClientConnected(this);
		} else if (m_pUser) {
			m_pUser->UserConnected(this);
		}
	}
}

vector<CClient*>& CClient::GetClients() {
	if (m_pNetwork) {
		return m_pNetwork->GetClients();
	}

	return m_pUser->GetUserClients();
}

const CIRCSock* CClient::GetIRCSock() const {
	if (m_pNetwork) {
		return m_pNetwork->GetIRCSock();
	}

	return NULL;
}

CIRCSock* CClient::GetIRCSock() {
	if (m_pNetwork) {
		return m_pNetwork->GetIRCSock();
	}

	return NULL;
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
		if (m_pNetwork) {
			PutStatusNotice(m_pNetwork->ExpandString(vsMotd[a]));
		} else {
			PutStatusNotice(m_pUser->ExpandString(vsMotd[a]));
		}
	}

	return true;
}

void CClient::AuthUser() {
	if (!m_bGotNick || !m_bGotUser || !m_bGotPass || m_bInCap || IsAttached())
		return;

	m_spAuth = new CClientAuth(this, m_sUser, m_sPass);

	CZNC::Get().AuthUser(m_spAuth);
}

CClientAuth::CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword)
		: CAuthBase(sUsername, sPassword, pClient) {
	m_pClient = pClient;
}

void CClientAuth::RefusedLogin(const CString& sReason) {
	if (m_pClient) {
		m_pClient->RefuseLogin(sReason);
	}
}

CString CAuthBase::GetRemoteIP() const {
	if (m_pSock)
		return m_pSock->GetRemoteIP();
	return "";
}

void CAuthBase::Invalidate() {
	m_pSock = NULL;
}

void CAuthBase::AcceptLogin(CUser& User) {
	if (m_pSock) {
		AcceptedLogin(User);
		Invalidate();
	}
}

void CAuthBase::RefuseLogin(const CString& sReason) {
	if (!m_pSock)
		return;

	CUser* pUser = CZNC::Get().FindUser(GetUsername());

	// If the username is valid, notify that user that someone tried to
	// login. Use sReason because there are other reasons than "wrong
	// password" for a login to be rejected (e.g. fail2ban).
	if (pUser) {
		pUser->PutStatus("A client from [" + GetRemoteIP() + "] attempted "
				"to login as you, but was rejected [" + sReason + "].");
	}

	GLOBALMODULECALL(OnFailedLogin(GetUsername(), GetRemoteIP()), NOTHING);
	RefusedLogin(sReason);
	Invalidate();
}

void CClient::RefuseLogin(const CString& sReason) {
	PutStatus("Bad username and/or password.");
	PutClient(":irc.znc.in 464 " + GetNick() + " :" + sReason);
	Close(Csock::CLT_AFTERWRITE);
}

void CClientAuth::AcceptedLogin(CUser& User) {
	if (m_pClient) {
		m_pClient->AcceptLogin(User);
	}
}

void CClient::AcceptLogin(CUser& User) {
	m_sPass = "";
	m_pUser = &User;

	// Set our proper timeout and set back our proper timeout mode
	// (constructor set a different timeout and mode)
	SetTimeout(540, TMO_READ);

	SetSockName("USR::" + m_pUser->GetUserName());

	if (!m_sNetwork.empty()) {
		m_pNetwork = m_pUser->FindNetwork(m_sNetwork);
		if (!m_pNetwork) {
			PutStatus("Network (" + m_sNetwork + ") doesn't exist.");
		}
	} else if (!m_pUser->GetNetworks().empty()) {
		// If a user didn't supply a network, and they have a network called "default" then automatically use this network.
		m_pNetwork = m_pUser->FindNetwork("default");
		// If no "default" network, try "user" network. It's for compatibility with early network stuff in ZNC, which converted old configs to "user" network.
		if (!m_pNetwork) m_pNetwork = m_pUser->FindNetwork("user");
		// Otherwise, just try any network of the user.
		if (!m_pNetwork) m_pNetwork = *m_pUser->GetNetworks().begin();
		if (m_pNetwork && m_pUser->GetNetworks().size() > 1) {
			PutStatusNotice("You have several networks configured, but no network was specified for the connection.");
			PutStatusNotice("Selecting network [" + m_pNetwork->GetName() + "]. To see list of all configured networks, use /znc ListNetworks");
			PutStatusNotice("If you want to choose another network, use /znc JumpNetwork <network>, or connect to ZNC with username " + m_pUser->GetUserName() + "/<network> (instead of just " + m_pUser->GetUserName() + ")");
		}
	} else {
		PutStatusNotice("You have no networks configured. Use /znc AddNetwork <network> to add one.");
	}

	SetNetwork(m_pNetwork, false);

	SendMotd();

	NETWORKMODULECALL(OnClientLogin(), m_pUser, m_pNetwork, this, NOTHING);
}

void CClient::Timeout() {
	PutClient("ERROR :Closing link [Timeout]");
}

void CClient::Connected() {
	DEBUG(GetSockName() << " == Connected();");
}

void CClient::ConnectionRefused() {
	DEBUG(GetSockName() << " == ConnectionRefused()");
}

void CClient::Disconnected() {
	DEBUG(GetSockName() << " == Disconnected()");
	CIRCNetwork* pNetwork = m_pNetwork;
	SetNetwork(NULL, true, false);

	if (m_pUser) {
		NETWORKMODULECALL(OnClientDisconnect(), m_pUser, pNetwork, this, NOTHING);
	}
}

void CClient::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	if (IsAttached()) {
		PutClient("ERROR :Closing link [Too long raw line]");
	}
	Close();
}

void CClient::BouncedOff() {
	PutStatusNotice("You are being disconnected because another user just authenticated as you.");
	Close(Csock::CLT_AFTERWRITE);
}

void CClient::PutIRC(const CString& sLine) {
	if (m_pNetwork) {
		m_pNetwork->PutIRC(sLine);
	}
}

CString CClient::GetFullName() {
	if (!m_pUser)
		return GetRemoteIP();
	if (!m_pNetwork)
		return m_pUser->GetUserName();
	return m_pUser->GetUserName() + "/" + m_pNetwork->GetName();
}

void CClient::PutClient(const CString& sLine) {
	DEBUG("(" << GetFullName() << ") ZNC -> CLI [" << sLine << "]");
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

	DEBUG("(" << GetFullName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " << GetNick() << " :" << sLine << "]");
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " + GetNick() + " :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
	VCString vsLines;
	VCString::iterator it;
	if (!m_pUser) {
		return;
	}

	DEBUG("(" << GetFullName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " << GetNick() << " :" << sLine << "]");

	sLine.Split("\n", vsLines);
	for (it = vsLines.begin(); it != vsLines.end(); ++it) {
		Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " + GetNick() + " :" + (*it) + "\r\n");
	}
}

CString CClient::GetNick(bool bAllowIRCNick) const {
	CString sRet;

	const CIRCSock *pSock = GetIRCSock();
	if (bAllowIRCNick && pSock && pSock->IsAuthed()) {
		sRet = pSock->GetNick();
	}

	return (sRet.empty()) ? m_sNick : sRet;
}

CString CClient::GetNickMask() const {
	if (GetIRCSock() && GetIRCSock()->IsAuthed()) {
		return GetIRCSock()->GetNickMask();
	}

	CString sHost = m_pNetwork ? m_pNetwork->GetBindHost() : m_pUser->GetBindHost();
	if (sHost.empty()) {
		sHost = "irc.znc.in";
	}

	return GetNick() + "!" + (m_pNetwork ? m_pNetwork->GetBindHost() : m_pUser->GetIdent()) + "@" + sHost;
}

void CClient::RespondCap(const CString& sResponse)
{
	PutClient(":irc.znc.in CAP " + GetNick() + " " + sResponse);
}

void CClient::HandleCap(const CString& sLine)
{
	//TODO support ~ and = modifiers
	CString sSubCmd = sLine.Token(1);

	if (sSubCmd.Equals("LS")) {
		SCString ssOfferCaps;
		GLOBALMODULECALL(OnClientCapLs(this, ssOfferCaps), NOTHING);
		CString sRes;
		for (SCString::iterator i = ssOfferCaps.begin(); i != ssOfferCaps.end(); ++i) {
			sRes += *i + " ";
		}
		RespondCap("LS :" + sRes + "userhost-in-names multi-prefix znc.in/server-time-iso");
		m_bInCap = true;
	} else if (sSubCmd.Equals("END")) {
		m_bInCap = false;
		if (!IsAttached()) {
			if (!m_pUser && m_bGotUser && !m_bGotPass) {
				SendRequiredPasswordNotice();
			} else {
				AuthUser();
			}
		}
	} else if (sSubCmd.Equals("REQ")) {
		VCString vsTokens;
		VCString::iterator it;
		sLine.Token(2, true).TrimPrefix_n(":").Split(" ", vsTokens, false);

		for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
			bool bVal = true;
			CString sCap = *it;
			if (sCap.TrimPrefix("-"))
				bVal = false;

			bool bAccepted = ("multi-prefix" == sCap) || ("userhost-in-names" == sCap) || ("znc.in/server-time-iso" == sCap);
			GLOBALMODULECALL(IsClientCapSupported(this, sCap, bVal), &bAccepted);

			if (!bAccepted) {
				// Some unsupported capability is requested
				RespondCap("NAK :" + sLine.Token(2, true).TrimPrefix_n(":"));
				return;
			}
		}

		// All is fine, we support what was requested
		for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
			bool bVal = true;
			if (it->TrimPrefix("-"))
				bVal = false;

			if ("multi-prefix" == *it) {
				m_bNamesx = bVal;
			} else if ("userhost-in-names" == *it) {
				m_bUHNames = bVal;
			} else if ("znc.in/server-time-iso" == *it) {
				m_bServerTime = bVal;
			}
			GLOBALMODULECALL(OnClientCapRequest(this, *it, bVal), NOTHING);

			if (bVal) {
				m_ssAcceptedCaps.insert(*it);
			} else {
				m_ssAcceptedCaps.erase(*it);
			}
		}

		RespondCap("ACK :" + sLine.Token(2, true).TrimPrefix_n(":"));
	} else if (sSubCmd.Equals("LIST")) {
		CString sList = "";
		for (SCString::iterator i = m_ssAcceptedCaps.begin(); i != m_ssAcceptedCaps.end(); ++i) {
			sList += *i + " ";
		}
		RespondCap("LIST :" + sList.TrimSuffix_n(" "));
	} else if (sSubCmd.Equals("CLEAR")) {
		SCString ssRemoved;
		for (SCString::iterator i = m_ssAcceptedCaps.begin(); i != m_ssAcceptedCaps.end(); ++i) {
			bool bRemoving = false;
			GLOBALMODULECALL(IsClientCapSupported(this, *i, false), &bRemoving);
			if (bRemoving) {
				GLOBALMODULECALL(OnClientCapRequest(this, *i, false), NOTHING);
				ssRemoved.insert(*i);
			}
		}
		if (m_bNamesx) {
			m_bNamesx = false;
			ssRemoved.insert("multi-prefix");
		}
		if (m_bUHNames) {
			m_bUHNames = false;
			ssRemoved.insert("userhost-in-names");
		}
		if (m_bServerTime) {
			m_bServerTime = false;
			ssRemoved.insert("znc.in/server-time-iso");
		}
		CString sList = "";
		for (SCString::iterator i = ssRemoved.begin(); i != ssRemoved.end(); ++i) {
			m_ssAcceptedCaps.erase(*i);
			sList += "-" + *i + " ";
		}
		RespondCap("ACK :" + sList.TrimSuffix_n(" "));
	} else {
		PutClient(":irc.znc.in 410 " + GetNick() + " :Invalid CAP subcommand");
	}
}
