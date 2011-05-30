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
#include "User.h"
#include "znc.h"
#include "WebModules.h"

#define CALLMOD(MOD, CLIENT, USER, FUNC) {                                      \
	CModule* pModule = CZNC::Get().GetModules().FindModule(MOD);            \
	if (pModule) {                                                          \
		try {                                                           \
			pModule->SetClient(CLIENT);                             \
			pModule->SetUser(USER);                                 \
			pModule->FUNC;                                          \
			pModule->SetClient(NULL);                               \
			pModule->SetUser(NULL);                                 \
		} catch (CModule::EModException e) {                            \
			if (e == CModule::UNLOAD) {                             \
				CZNC::Get().GetModules().UnloadModule(MOD);     \
			}                                                       \
		}                                                               \
	} else {                                                                \
		pModule = (USER)->GetModules().FindModule(MOD);                 \
		if (pModule) {                                                  \
			try {                                                   \
				pModule->SetClient(CLIENT);                     \
				pModule->FUNC;                                  \
				pModule->SetClient(NULL);                       \
			} catch (CModule::EModException e) {                    \
				if (e == CModule::UNLOAD) {                     \
					(USER)->GetModules().UnloadModule(MOD); \
				}                                               \
			}                                                       \
		} else {                                                        \
			PutStatus("No such module [" + MOD + "]");              \
		}                                                               \
	}                                                                       \
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

void CClient::ReadLine(const CString& sData) {
	CString sLine = sData;

	sLine.TrimRight("\n\r");

	DEBUG("(" << ((m_pUser) ? m_pUser->GetUserName() : GetRemoteIP()) << ") CLI -> ZNC [" << sLine << "]");

	if (IsAttached()) {
		MODULECALL(OnUserRaw(sLine), m_pUser, this, return);
	} else {
		GLOBALMODULECALL(OnUnknownUserRaw(sLine), m_pUser, this, return);
	}

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
			if (m_sPass.Left(1) == ":")
				m_sPass.LeftChomp();

			if (m_sPass.find(":") != CString::npos) {
				m_sUser = m_sPass.Token(0, false, ":");
				m_sPass = m_sPass.Token(1, true, ":");
			}

			AuthUser();
			return;  // Don't forward this msg.  ZNC has already registered us.
		}
	} else if (sCommand.Equals("NICK")) {
		CString sNick = sLine.Token(1);
		if (sNick.Left(1) == ":") {
			sNick.LeftChomp();
		}

		if (!IsAttached()) {
			m_sNick = sNick;
			m_bGotNick = true;

			AuthUser();
			return;  // Don't forward this msg.  ZNC will handle nick changes until auth is complete
		}
	} else if (sCommand.Equals("USER")) {
		if (!IsAttached()) {
			if (m_sUser.empty()) {
				m_sUser = sLine.Token(1);
			}

			m_bGotUser = true;

			if (m_bGotPass) {
				AuthUser();
			} else {
				PutClient(":irc.znc.in NOTICE AUTH :*** "
					"You need to send your password. "
					"Try /quote PASS <username>:<password>");
			}

			return;  // Don't forward this msg.  ZNC has already registered us.
		}
	} else if (sCommand.Equals("CAP")) {
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
				CALLMOD(sTarget, this, m_pUser, PutModule("Hello. How may I help you?"))
			else
				CALLMOD(sTarget, this, m_pUser, OnModCommand(sModCommand))
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
		// All PONGs are generated by znc. We will still forward this to
		// the ircd, but all PONGs from irc will be blocked.
		if (sLine.length() >= 5)
			PutClient(":irc.znc.in PONG irc.znc.in " + sLine.substr(5));
		else
			PutClient(":irc.znc.in PONG irc.znc.in");
	} else if (sCommand.Equals("PONG")) {
		// Block PONGs, we already responded to the pings
		return;
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

			// If we are on that channel and already received a
			// /mode reply from the server, we can answer this
			// request ourself.
			if (pChan && pChan->IsOn() && sModes.empty() && !pChan->GetModeString().empty()) {
				PutClient(":" + m_pUser->GetIRCServer() + " 324 " + GetNick() + " " + sTarget + " " + pChan->GetModeString());
				if (pChan->GetCreationDate() > 0) {
					PutClient(":" + m_pUser->GetIRCServer() + " 329 " + GetNick() + " " + sTarget + " " + CString(pChan->GetCreationDate()));
				}
				return;
			}
		}
	} else if (sCommand.Equals("QUIT")) {
		m_pUser->UserDisconnected(this);

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
		CString sMsg = sLine.Token(2, true);

		if (sMsg.Left(1) == ":") {
			sMsg.LeftChomp();
		}

		if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
			if (!sTarget.Equals("status")) {
				CALLMOD(sTarget, this, m_pUser, OnModNotice(sMsg));
			}
			return;
		}

		if (sMsg.WildCmp("\001*\001")) {
			CString sCTCP = sMsg;
			sCTCP.LeftChomp();
			sCTCP.RightChomp();

			MODULECALL(OnUserCTCPReply(sTarget, sCTCP), m_pUser, this, return);

			sMsg = "\001" + sCTCP + "\001";
		} else {
			MODULECALL(OnUserNotice(sTarget, sMsg), m_pUser, this, return);
		}

		if (!GetIRCSock()) {
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

			if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
				if (sTarget.Equals("status")) {
					StatusCTCP(sCTCP);
				} else {
					CALLMOD(sTarget, this, m_pUser, OnModCTCP(sCTCP));
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
				UserCommand(sMsg);
			} else {
				CALLMOD(sTarget, this, m_pUser, OnModCommand(sMsg));
			}
			return;
		}

		MODULECALL(OnUserMsg(sTarget, sMsg), m_pUser, this, return);

		if (!GetIRCSock()) {
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

const CIRCSock* CClient::GetIRCSock() const {
	return m_pUser->GetIRCSock();
}

CIRCSock* CClient::GetIRCSock() {
	return m_pUser->GetIRCSock();
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
		PutStatusNotice(m_pUser->ExpandString(vsMotd[a]));
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

	GLOBALMODULECALL(OnFailedLogin(GetUsername(), GetRemoteIP()), NULL, NULL, NOTHING);
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

	m_pUser->UserConnected(this);

	SendMotd();

	MODULECALL(OnClientLogin(), m_pUser, this, NOTHING);
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
	if (m_pUser) {
		m_pUser->UserDisconnected(this);
	}

	MODULECALL(OnClientDisconnect(), m_pUser, this, NOTHING);
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
	m_pUser->PutIRC(sLine);
}

void CClient::PutClient(const CString& sLine) {
	DEBUG("(" << ((m_pUser) ? m_pUser->GetUserName() : GetRemoteIP()) << ") ZNC -> CLI [" << sLine << "]");
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

	DEBUG("(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " << GetNick() << " :" << sLine << "]");
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in NOTICE " + GetNick() + " :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG("(" << m_pUser->GetUserName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " << GetNick() << " :" << sLine << "]");
	Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " + GetNick() + " :" + sLine + "\r\n");
}

CString CClient::GetNick(bool bAllowIRCNick) const {
	CString sRet;

	if ((bAllowIRCNick) && (IsAttached()) && (GetIRCSock())) {
		sRet = GetIRCSock()->GetNick();
	}

	return (sRet.empty()) ? m_sNick : sRet;
}

CString CClient::GetNickMask() const {
	if (GetIRCSock() && GetIRCSock()->IsAuthed()) {
		return GetIRCSock()->GetNickMask();
	}

	CString sHost = m_pUser->GetBindHost();
	if (sHost.empty()) {
		sHost = "irc.znc.in";
	}

	return GetNick() + "!" + m_pUser->GetIdent() + "@" + sHost;
}

void CClient::RespondCap(const CString& sResponse)
{
	PutClient(":irc.znc.in CAP " + GetNick() + " " + sResponse);
}

void CClient::HandleCap(const CString& sLine)
{
	CString sSubCmd = sLine.Token(1);

	if (sSubCmd.Equals("LS")) {
		SCString ssOfferCaps;
		GLOBALMODULECALL(OnClientCapLs(ssOfferCaps), m_pUser, this, NOTHING);
		CString sRes;
		for (SCString::iterator i = ssOfferCaps.begin(); i != ssOfferCaps.end(); ++i) {
			sRes += *i + " ";
		}
		RespondCap("LS :" + sRes + "userhost-in-names multi-prefix");
		m_bInCap = true;
	} else if (sSubCmd.Equals("END")) {
		m_bInCap = false;
		AuthUser();
	} else if (sSubCmd.Equals("REQ")) {
		VCString vsTokens;
		VCString::iterator it;
		sLine.Token(2, true).TrimPrefix_n(":").Split(" ", vsTokens, false);

		for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
			bool bVal = true;
			CString sCap = *it;
			if (sCap.TrimPrefix("-"))
				bVal = false;

			bool bAccepted = ("multi-prefix" == sCap) || ("userhost-in-names" == sCap);
			GLOBALMODULECALL(IsClientCapSupported(sCap, bVal), m_pUser, this, bAccepted = true);

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
			}
			GLOBALMODULECALL(OnClientCapRequest(*it, bVal), m_pUser, this, NOTHING);

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
			GLOBALMODULECALL(IsClientCapSupported(*i, false), m_pUser, this, bRemoving = true);
			if (bRemoving) {
				GLOBALMODULECALL(OnClientCapRequest(*i, false), m_pUser, this, NOTHING);
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
		CString sList = "";
		for (SCString::iterator i = ssRemoved.begin(); i != ssRemoved.end(); ++i) {
			m_ssAcceptedCaps.erase(*i);
			sList += "-" + *i + " ";
		}
		RespondCap("ACK :" + sList.TrimSuffix_n(" "));
	}
}
