/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "IRCSock.h"
#include "Chan.h"
#include "Client.h"
#include "User.h"
#include "znc.h"

// These are used in OnGeneralCTCP()
const time_t CIRCSock::m_uCTCPFloodTime = 5;
const unsigned int CIRCSock::m_uCTCPFloodCount = 5;

CIRCSock::CIRCSock(CUser* pUser) : CZNCSock() {
	m_pUser = pUser;
	m_bAuthed = false;
	m_bNamesx = false;
	m_bUHNames = false;
	EnableReadLine();
	m_Nick.SetIdent(pUser->GetIdent());
	m_Nick.SetHost(pUser->GetBindHost());

	m_uMaxNickLen = 9;
	m_uCapPaused = 0;
	m_lastCTCP = 0;
	m_uNumCTCP = 0;
	m_sPerms = "*!@%+";
	m_sPermModes = "qaohv";
	m_mueChanModes['b'] = ListArg;
	m_mueChanModes['e'] = ListArg;
	m_mueChanModes['I'] = ListArg;
	m_mueChanModes['k'] = HasArg;
	m_mueChanModes['l'] = ArgWhenSet;
	m_mueChanModes['p'] = NoArg;
	m_mueChanModes['s'] = NoArg;
	m_mueChanModes['t'] = NoArg;
	m_mueChanModes['i'] = NoArg;
	m_mueChanModes['n'] = NoArg;

	pUser->SetIRCSocket(this);

	// RFC says a line can have 512 chars max, but we don't care ;)
	SetMaxBufferThreshold(1024);
}

CIRCSock::~CIRCSock() {
	if (!m_bAuthed) {
		MODULECALL(OnIRCConnectionError(this), m_pUser, NULL, NOTHING);
	}

	const vector<CChan*>& vChans = m_pUser->GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		vChans[a]->Reset();
	}

	m_pUser->IRCDisconnected();

	for (map<CString, CChan*>::iterator a = m_msChans.begin(); a != m_msChans.end(); ++a) {
		delete a->second;
	}

	Quit();
	m_msChans.clear();
	GetUser()->AddBytesRead(GetBytesRead());
	GetUser()->AddBytesWritten(GetBytesWritten());
}

void CIRCSock::Quit(const CString& sQuitMsg) {
	if (!m_bAuthed) {
		Close(CLT_NOW);
		return;
	}
	if (!sQuitMsg.empty()) {
		PutIRC("QUIT :" + sQuitMsg);
	} else {
		PutIRC("QUIT :" + m_pUser->ExpandString(m_pUser->GetQuitMsg()));
	}
	Close(CLT_AFTERWRITE);
}

void CIRCSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	sLine.TrimRight("\n\r");

	DEBUG("(" << m_pUser->GetUserName() << ") IRC -> ZNC [" << sLine << "]");

	MODULECALL(OnRaw(sLine), m_pUser, NULL, return);

	if (sLine.Equals("PING ", false, 5)) {
		// Generate a reply and don't forward this to any user,
		// we don't want any PING forwarded
		PutIRC("PONG " + sLine.substr(5));
		return;
	} else if (sLine.Token(1).Equals("PONG")) {
		// Block PONGs, we already responded to the pings
		return;
	} else if (sLine.Equals("ERROR ", false, 6)) {
		//ERROR :Closing Link: nick[24.24.24.24] (Excess Flood)
		CString sError(sLine.substr(6));

		if (sError.Left(1) == ":") {
			sError.LeftChomp();
		}

		m_pUser->PutStatus("Error from Server [" + sError + "]");
		return;
	}

	CString sCmd = sLine.Token(1);

	if ((sCmd.length() == 3) && (isdigit(sCmd[0])) && (isdigit(sCmd[1])) && (isdigit(sCmd[2]))) {
		CString sServer = sLine.Token(0); sServer.LeftChomp();
		unsigned int uRaw = sCmd.ToUInt();
		CString sNick = sLine.Token(2);
		CString sRest = sLine.Token(3, true);

		switch (uRaw) {
			case 1: { // :irc.server.com 001 nick :Welcome to the Internet Relay Network nick
				if (m_bAuthed && sServer == "irc.znc.in") {
					// m_bAuthed == true => we already received another 001 => we might be in a traffic loop
					m_pUser->PutStatus("ZNC seems to be connected to itself, disconnecting...");
					Quit();
					return;
				}

				m_pUser->SetIRCServer(sServer);
				SetTimeout(540, TMO_READ);  // Now that we are connected, let nature take its course
				PutIRC("WHO " + sNick);

				m_bAuthed = true;
				m_pUser->PutStatus("Connected!");

				vector<CClient*>& vClients = m_pUser->GetClients();

				for (unsigned int a = 0; a < vClients.size(); a++) {
					CClient* pClient = vClients[a];
					CString sClientNick = pClient->GetNick(false);

					if (!sClientNick.Equals(sNick)) {
						// If they connected with a nick that doesn't match the one we got on irc, then we need to update them
						pClient->PutClient(":" + sClientNick + "!" + m_Nick.GetIdent() + "@" + m_Nick.GetHost() + " NICK :" + sNick);
					}
				}

				SetNick(sNick);

				MODULECALL(OnIRCConnected(), m_pUser, NULL, NOTHING);

				m_pUser->ClearRawBuffer();
				m_pUser->AddRawBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);

				break;
			}
			case 5:
				ParseISupport(sRest);
				m_pUser->UpdateExactRawBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);
				break;
			case 2:
			case 3:
			case 4:
			case 250:  // highest connection count
			case 251:  // user count
			case 252:  // oper count
			case 254:  // channel count
			case 255:  // client count
			case 265:  // local users
			case 266:  // global users
				m_pUser->UpdateRawBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);
				break;
			case 305:
				m_pUser->SetIRCAway(false);
				break;
			case 306:
				m_pUser->SetIRCAway(true);
				break;
			case 324: {  // MODE
				sRest.Trim();
				CChan* pChan = m_pUser->FindChan(sRest.Token(0));

				if (pChan) {
					pChan->SetModes(sRest.Token(1, true));
				}
			}
				break;
			case 329: {
				sRest.Trim();
				CChan* pChan = m_pUser->FindChan(sRest.Token(0));

				if (pChan) {
					unsigned long ulDate = sLine.Token(4).ToULong();
					pChan->SetCreationDate(ulDate);
				}
			}
				break;
			case 331: {
				// :irc.server.com 331 yournick #chan :No topic is set.
				CChan* pChan = m_pUser->FindChan(sLine.Token(3));

				if (pChan) {
					pChan->SetTopic("");
				}

				break;
			}
			case 332: {
				// :irc.server.com 332 yournick #chan :This is a topic
				CChan* pChan = m_pUser->FindChan(sLine.Token(3));

				if (pChan) {
					CString sTopic = sLine.Token(4, true);
					sTopic.LeftChomp();
					pChan->SetTopic(sTopic);
				}

				break;
			}
			case 333: {
				// :irc.server.com 333 yournick #chan setternick 1112320796
				CChan* pChan = m_pUser->FindChan(sLine.Token(3));

				if (pChan) {
					sNick = sLine.Token(4);
					unsigned long ulDate = sLine.Token(5).ToULong();

					pChan->SetTopicOwner(sNick);
					pChan->SetTopicDate(ulDate);
				}

				break;
			}
			case 352: {
				// :irc.yourserver.com 352 yournick #chan ident theirhost.com irc.theirserver.com theirnick H :0 Real Name
				sServer = sLine.Token(0);
				sNick = sLine.Token(7);
				CString sIdent = sLine.Token(4);
				CString sHost = sLine.Token(5);

				sServer.LeftChomp();

				if (sNick.Equals(GetNick())) {
					m_Nick.SetIdent(sIdent);
					m_Nick.SetHost(sHost);
				}

				m_pUser->SetIRCNick(m_Nick);
				m_pUser->SetIRCServer(sServer);

				const vector<CChan*>& vChans = m_pUser->GetChans();

				for (unsigned int a = 0; a < vChans.size(); a++) {
					vChans[a]->OnWho(sNick, sIdent, sHost);
				}

				break;
			}
			case 353: {  // NAMES
				sRest.Trim();
				// Todo: allow for non @+= server msgs
				CChan* pChan = m_pUser->FindChan(sRest.Token(1));
				// If we don't know that channel, some client might have
				// requested a /names for it and we really should forward this.
				if (pChan) {
					CString sNicks = sRest.Token(2, true);
					if (sNicks.Left(1) == ":") {
						sNicks.LeftChomp();
					}

					pChan->AddNicks(sNicks);
				}

				ForwardRaw353(sLine);

				// We forwarded it already, so return
				return;
			}
			case 366: {  // end of names list
				m_pUser->PutUser(sLine);  // First send them the raw

				// :irc.server.com 366 nick #chan :End of /NAMES list.
				CChan* pChan = m_pUser->FindChan(sRest.Token(0));

				if (pChan) {
					if (pChan->IsOn()) {
						// If we are the only one in the chan, set our default modes
						if (pChan->GetNickCount() == 1) {
							CString sModes = pChan->GetDefaultModes();

							if (sModes.empty()) {
								sModes = m_pUser->GetDefaultChanModes();
							}

							if (!sModes.empty()) {
								PutIRC("MODE " + pChan->GetName() + " " + sModes);
							}
						}
					}
				}

				return;  // return so we don't send them the raw twice
			}
			case 375:  // begin motd
			case 422:  // MOTD File is missing
				m_pUser->ClearMotdBuffer();
			case 372:  // motd
			case 376:  // end motd
				m_pUser->AddMotdBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);
				break;
			case 437:
				// :irc.server.net 437 * badnick :Nick/channel is temporarily unavailable
				// :irc.server.net 437 mynick badnick :Nick/channel is temporarily unavailable
				// :irc.server.net 437 mynick badnick :Cannot change nickname while banned on channel
				if (m_pUser->IsChan(sRest.Token(0)) || sNick != "*")
					break;
			case 432: // :irc.server.com 432 * nick :Erroneous Nickname: Illegal characters
			case 433: {
				CString sBadNick = sRest.Token(0);

				if (!m_bAuthed) {
					SendAltNick(sBadNick);
					return;
				}
				break;
			}
			case 451:
				// :irc.server.com 451 CAP :You have not registered
				// Servers that dont support CAP will give us this error, dont send it to the client
				if (sNick.Equals("CAP"))
					return;
			case 470: {
				// :irc.unreal.net 470 mynick [Link] #chan1 has become full, so you are automatically being transferred to the linked channel #chan2
				// :mccaffrey.freenode.net 470 mynick #electronics ##electronics :Forwarding to another channel

				// freenode style numeric
				CChan* pChan = m_pUser->FindChan(sRest.Token(0));
				if (!pChan) {
					// unreal style numeric
					pChan = m_pUser->FindChan(sRest.Token(1));
				}
				if (pChan) {
					pChan->Disable();
					m_pUser->PutStatus("Channel [" + pChan->GetName() + "] is linked to "
							"another channel and was thus disabled.");
				}
				break;
			}
		}
	} else {
		CNick Nick(sLine.Token(0).LeftChomp_n());
		sCmd = sLine.Token(1);
		CString sRest = sLine.Token(2, true);

		if (sCmd.Equals("NICK")) {
			CString sNewNick = sRest;
			bool bIsVisible = false;

			if (sNewNick.Left(1) == ":") {
				sNewNick.LeftChomp();
			}

			vector<CChan*> vFoundChans;
			const vector<CChan*>& vChans = m_pUser->GetChans();

			for (unsigned int a = 0; a < vChans.size(); a++) {
				CChan* pChan = vChans[a];

				if (pChan->ChangeNick(Nick.GetNick(), sNewNick)) {
					vFoundChans.push_back(pChan);

					if (!pChan->IsDetached()) {
						bIsVisible = true;
					}
				}
			}

			// Todo: use nick compare function here
			if (Nick.GetNick().Equals(GetNick())) {
				// We are changing our own nick, the clients always must see this!
				bIsVisible = true;
				SetNick(sNewNick);
			}

			MODULECALL(OnNick(Nick, sNewNick, vFoundChans), m_pUser, NULL, NOTHING);

			if (!bIsVisible) {
				return;
			}
		} else if (sCmd.Equals("QUIT")) {
			CString sMessage = sRest;
			bool bIsVisible = false;

			if (sMessage.Left(1) == ":") {
				sMessage.LeftChomp();
			}

			// :nick!ident@host.com QUIT :message

			if (Nick.GetNick().Equals(GetNick())) {
				m_pUser->PutStatus("You quit [" + sMessage + "]");
				// We don't call module hooks and we don't
				// forward this quit to clients (Some clients
				// disconnect if they receive such a QUIT)
				return;
			}

			vector<CChan*> vFoundChans;
			const vector<CChan*>& vChans = m_pUser->GetChans();

			for (unsigned int a = 0; a < vChans.size(); a++) {
				CChan* pChan = vChans[a];

				if (pChan->RemNick(Nick.GetNick())) {
					vFoundChans.push_back(pChan);

					if (!pChan->IsDetached()) {
						bIsVisible = true;
					}
				}
			}

			MODULECALL(OnQuit(Nick, sMessage, vFoundChans), m_pUser, NULL, NOTHING);

			if (!bIsVisible) {
				return;
			}
		} else if (sCmd.Equals("JOIN")) {
			CString sChan = sRest.Token(0);
			if (sChan.Left(1) == ":") {
				sChan.LeftChomp();
			}

			CChan* pChan;

			// Todo: use nick compare function
			if (Nick.GetNick().Equals(GetNick())) {
				m_pUser->AddChan(sChan, false);
				pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->ResetJoinTries();
					pChan->Enable();
					pChan->SetIsOn(true);
				}
			} else {
				pChan = m_pUser->FindChan(sChan);
			}

			if (pChan) {
				pChan->AddNick(Nick.GetNickMask());
				MODULECALL(OnJoin(Nick.GetNickMask(), *pChan), m_pUser, NULL, NOTHING);

				if (pChan->IsDetached()) {
					return;
				}
			}
		} else if (sCmd.Equals("PART")) {
			CString sChan = sRest.Token(0);
			if (sChan.Left(1) == ":") {
				sChan.LeftChomp();
			}
			CString sMsg = sRest.Token(1, true).TrimPrefix_n(":");

			CChan* pChan = m_pUser->FindChan(sChan);
			bool bDetached = false;
			if (pChan) {
				pChan->RemNick(Nick.GetNick());
				MODULECALL(OnPart(Nick.GetNickMask(), *pChan, sMsg), m_pUser, NULL, NOTHING);

				if (pChan->IsDetached())
					bDetached = true;
			}

			// Todo: use nick compare function
			if (Nick.GetNick().Equals(GetNick())) {
				m_pUser->DelChan(sChan);
			}

			/*
			 * We use this boolean because
			 * m_pUser->DelChan() will delete this channel
			 * and thus we would dereference an
			 * already-freed pointer!
			 */
			if (bDetached) {
				return;
			}
		} else if (sCmd.Equals("MODE")) {
			CString sTarget = sRest.Token(0);
			CString sModes = sRest.Token(1, true);
			if (sModes.Left(1) == ":")
				sModes = sModes.substr(1);

			CChan* pChan = m_pUser->FindChan(sTarget);
			if (pChan) {
				pChan->ModeChange(sModes, &Nick);

				if (pChan->IsDetached()) {
					return;
				}
			} else if (sTarget == m_Nick.GetNick()) {
				CString sModeArg = sModes.Token(0);
				bool bAdd = true;
/* no module call defined (yet?)
				MODULECALL(OnRawUserMode(*pOpNick, *this, sModeArg, sArgs), m_pUser, NULL, );
*/
				for (unsigned int a = 0; a < sModeArg.size(); a++) {
					const unsigned char& uMode = sModeArg[a];

					if (uMode == '+') {
						bAdd = true;
					} else if (uMode == '-') {
						bAdd = false;
					} else {
						if (bAdd) {
							m_scUserModes.insert(uMode);
						} else {
							m_scUserModes.erase(uMode);
						}
					}
				}
			}
		} else if (sCmd.Equals("KICK")) {
			// :opnick!ident@host.com KICK #chan nick :msg
			CString sChan = sRest.Token(0);
			CString sKickedNick = sRest.Token(1);
			CString sMsg = sRest.Token(2, true);
			sMsg.LeftChomp();

			CChan* pChan = m_pUser->FindChan(sChan);

			if (pChan) {
				MODULECALL(OnKick(Nick, sKickedNick, *pChan, sMsg), m_pUser, NULL, NOTHING);
				// do not remove the nick till after the OnKick call, so modules
				// can do Chan.FindNick or something to get more info.
				pChan->RemNick(sKickedNick);
			}

			if (GetNick().Equals(sKickedNick) && pChan) {
				pChan->SetIsOn(false);

				// Don't try to rejoin!
				pChan->Disable();
			}

			if ((pChan) && (pChan->IsDetached())) {
				return;
			}
		} else if (sCmd.Equals("NOTICE")) {
			// :nick!ident@host.com NOTICE #chan :Message
			CString sTarget = sRest.Token(0);
			CString sMsg = sRest.Token(1, true);
			sMsg.LeftChomp();

			if (sMsg.WildCmp("\001*\001")) {
				sMsg.LeftChomp();
				sMsg.RightChomp();

				if (sTarget.Equals(GetNick())) {
					if (OnCTCPReply(Nick, sMsg)) {
						return;
					}
				}

				m_pUser->PutUser(":" + Nick.GetNickMask() + " NOTICE " + sTarget + " :\001" + sMsg + "\001");
				return;
			} else {
				if (sTarget.Equals(GetNick())) {
					if (OnPrivNotice(Nick, sMsg)) {
						return;
					}
				} else {
					if (OnChanNotice(Nick, sTarget, sMsg)) {
						return;
					}
				}
			}

			if (Nick.GetNick().Equals(m_pUser->GetIRCServer())) {
				m_pUser->PutUser(":" + Nick.GetNick() + " NOTICE " + sTarget + " :" + sMsg);
			} else {
				m_pUser->PutUser(":" + Nick.GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
			}

			return;
		} else if (sCmd.Equals("TOPIC")) {
			// :nick!ident@host.com TOPIC #chan :This is a topic
			CChan* pChan = m_pUser->FindChan(sLine.Token(2));

			if (pChan) {
				CString sTopic = sLine.Token(3, true);
				sTopic.LeftChomp();

				MODULECALL(OnTopic(Nick, *pChan, sTopic), m_pUser, NULL, return)

				pChan->SetTopicOwner(Nick.GetNick());
				pChan->SetTopicDate((unsigned long) time(NULL));
				pChan->SetTopic(sTopic);

				if (pChan->IsDetached()) {
					return; // Don't forward this
				}

				sLine = ":" + Nick.GetNickMask() + " TOPIC " + pChan->GetName() + " :" + sTopic;
			}
		} else if (sCmd.Equals("PRIVMSG")) {
			// :nick!ident@host.com PRIVMSG #chan :Message
			CString sTarget = sRest.Token(0);
			CString sMsg = sRest.Token(1, true);

			if (sMsg.Left(1) == ":") {
				sMsg.LeftChomp();
			}

			if (sMsg.WildCmp("\001*\001")) {
				sMsg.LeftChomp();
				sMsg.RightChomp();

				if (sTarget.Equals(GetNick())) {
					if (OnPrivCTCP(Nick, sMsg)) {
						return;
					}
				} else {
					if (OnChanCTCP(Nick, sTarget, sMsg)) {
						return;
					}
				}

				m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + sTarget + " :\001" + sMsg + "\001");
				return;
			} else {
				if (sTarget.Equals(GetNick())) {
					if (OnPrivMsg(Nick, sMsg)) {
						return;
					}
				} else {
					if (OnChanMsg(Nick, sTarget, sMsg)) {
						return;
					}
				}

				m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + sTarget + " :" + sMsg);
				return;
			}
		} else if (sCmd.Equals("WALLOPS")) {
			// :blub!dummy@rox-8DBEFE92 WALLOPS :this is a test
			CString sMsg = sRest.Token(0, true);

			if (sMsg.Left(1) == ":") {
				sMsg.LeftChomp();
			}

			if (!m_pUser->IsUserAttached()) {
				m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " WALLOPS ", ":" + m_pUser->AddTimestamp(sMsg), false);
			}
		} else if (sCmd.Equals("CAP")) {
			// CAPs are supported only before authorization.
			if (!m_bAuthed) {
				// sRest.Token(0) is most likely "*". No idea why, the
				// CAP spec don't mention this, but all implementations
				// I've seen add this extra asterisk
				CString sSubCmd = sRest.Token(1);

				// If the caplist of a reply is too long, it's split
				// into multiple replies. A "*" is prepended to show
				// that the list was split into multiple replies.
				// This is useful mainly for LS. For ACK and NAK
				// replies, there's no real need for this, because
				// we request only 1 capability per line.
				// If we will need to support broken servers or will
				// send several requests per line, need to delay ACK
				// actions until all ACK lines are received and
				// to recognize past request of NAK by 100 chars
				// of this reply.
				CString sArgs;
				if (sRest.Token(2) == "*") {
					sArgs = sRest.Token(3, true).TrimPrefix_n(":");
				} else {
					sArgs = sRest.Token(2, true).TrimPrefix_n(":");
				}

				if (sSubCmd == "LS") {
					VCString vsTokens;
					VCString::iterator it;
					sArgs.Split(" ", vsTokens, false);

					for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
						if (OnServerCapAvailable(*it) || *it == "multi-prefix" || *it == "userhost-in-names") {
							m_ssPendingCaps.insert(*it);
						}
					}
				} else if (sSubCmd == "ACK") {
					sArgs.Trim();
					MODULECALL(OnServerCapResult(sArgs, true), m_pUser, NULL, NOTHING);
					if ("multi-prefix" == sArgs) {
						m_bNamesx = true;
					} else if ("userhost-in-names" == sArgs) {
						m_bUHNames = true;
					}
					m_ssAcceptedCaps.insert(sArgs);
				} else if (sSubCmd == "NAK") {
					// This should work because there's no [known]
					// capability with length of name more than 100 characters.
					sArgs.Trim();
					MODULECALL(OnServerCapResult(sArgs, false), m_pUser, NULL, NOTHING);
				}

				SendNextCap();
			}
			// Don't forward any CAP stuff to the client
			return;
		}
	}

	m_pUser->PutUser(sLine);
}

void CIRCSock::SendNextCap() {
	if (!m_uCapPaused) {
		if (m_ssPendingCaps.empty()) {
			// We already got all needed ACK/NAK replies.
			PutIRC("CAP END");
		} else {
			CString sCap = *m_ssPendingCaps.begin();
			m_ssPendingCaps.erase(m_ssPendingCaps.begin());
			PutIRC("CAP REQ :" + sCap);
		}
	}
}

void CIRCSock::PauseCap() {
	++m_uCapPaused;
}

void CIRCSock::ResumeCap() {
	--m_uCapPaused;
	SendNextCap();
}

bool CIRCSock::OnServerCapAvailable(const CString& sCap) {
	MODULECALL(OnServerCapAvailable(sCap), m_pUser, NULL, return true);
	return false;
}

bool CIRCSock::OnCTCPReply(CNick& Nick, CString& sMessage) {
	MODULECALL(OnCTCPReply(Nick, sMessage), m_pUser, NULL, return true);

	return false;
}

bool CIRCSock::OnPrivCTCP(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivCTCP(Nick, sMessage), m_pUser, NULL, return true);

	if (sMessage.TrimPrefix("ACTION ")) {
		MODULECALL(OnPrivAction(Nick, sMessage), m_pUser, NULL, return true);

		if (!m_pUser->IsUserAttached()) {
			// If the user is detached, add to the buffer
			m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " PRIVMSG ", " :\001ACTION " + m_pUser->AddTimestamp(sMessage) + "\001");
		}

		sMessage = "ACTION " + sMessage;
	}

	// This handles everything which wasn't handled yet
	return OnGeneralCTCP(Nick, sMessage);
}

bool CIRCSock::OnGeneralCTCP(CNick& Nick, CString& sMessage) {
	const MCString& mssCTCPReplies = m_pUser->GetCTCPReplies();
	CString sQuery = sMessage.Token(0).AsUpper();
	MCString::const_iterator it = mssCTCPReplies.find(sQuery);
	bool bHaveReply = false;
	CString sReply;

	if (it != mssCTCPReplies.end()) {
		sReply = m_pUser->ExpandString(it->second);
		bHaveReply = true;
	}

	if (!bHaveReply && !m_pUser->IsUserAttached()) {
		if (sQuery == "VERSION") {
			sReply = CZNC::GetTag();
		} else if (sQuery == "PING") {
			sReply = sMessage.Token(1, true);
		}
	}

	if (!sReply.empty()) {
		time_t now = time(NULL);
		// If the last CTCP is older than m_uCTCPFloodTime, reset the counter
		if (m_lastCTCP + m_uCTCPFloodTime < now)
			m_uNumCTCP = 0;
		m_lastCTCP = now;
		// If we are over the limit, don't reply to this CTCP
		if (m_uNumCTCP >= m_uCTCPFloodCount) {
			DEBUG("CTCP flood detected - not replying to query");
			return false;
		}
		m_uNumCTCP++;

		PutIRC("NOTICE " + Nick.GetNick() + " :\001" + sQuery + " " + sReply + "\001");
		return true;
	}

	return false;
}

bool CIRCSock::OnPrivNotice(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivNotice(Nick, sMessage), m_pUser, NULL, return true);

	if (!m_pUser->IsUserAttached()) {
		// If the user is detached, add to the buffer
		m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " NOTICE ", " :" + m_pUser->AddTimestamp(sMessage));
	}

	return false;
}

bool CIRCSock::OnPrivMsg(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivMsg(Nick, sMessage), m_pUser, NULL, return true);

	if (!m_pUser->IsUserAttached()) {
		// If the user is detached, add to the buffer
		m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " PRIVMSG ", " :" + m_pUser->AddTimestamp(sMessage));
	}

	return false;
}

bool CIRCSock::OnChanCTCP(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		MODULECALL(OnChanCTCP(Nick, *pChan, sMessage), m_pUser, NULL, return true);

		// Record a /me
		if (sMessage.TrimPrefix("ACTION ")) {
			MODULECALL(OnChanAction(Nick, *pChan, sMessage), m_pUser, NULL, return true);
			if (pChan->KeepBuffer() || !m_pUser->IsUserAttached() || pChan->IsDetached()) {
				pChan->AddBuffer(":" + Nick.GetNickMask() + " PRIVMSG " + sChan + " :\001ACTION " + m_pUser->AddTimestamp(sMessage) + "\001");
			}
			sMessage = "ACTION " + sMessage;
		}
	}

	if (OnGeneralCTCP(Nick, sMessage))
		return true;

	return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnChanNotice(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		MODULECALL(OnChanNotice(Nick, *pChan, sMessage), m_pUser, NULL, return true);

		if (pChan->KeepBuffer() || !m_pUser->IsUserAttached() || pChan->IsDetached()) {
			pChan->AddBuffer(":" + Nick.GetNickMask() + " NOTICE " + sChan + " :" + m_pUser->AddTimestamp(sMessage));
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

bool CIRCSock::OnChanMsg(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		MODULECALL(OnChanMsg(Nick, *pChan, sMessage), m_pUser, NULL, return true);

		if (pChan->KeepBuffer() || !m_pUser->IsUserAttached() || pChan->IsDetached()) {
			pChan->AddBuffer(":" + Nick.GetNickMask() + " PRIVMSG " + sChan + " :" + m_pUser->AddTimestamp(sMessage));
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

void CIRCSock::PutIRC(const CString& sLine) {
	DEBUG("(" << m_pUser->GetUserName() << ") ZNC -> IRC [" << sLine << "]");
	Write(sLine + "\r\n");
}

void CIRCSock::SetNick(const CString& sNick) {
	m_Nick.SetNick(sNick);
	m_pUser->SetIRCNick(m_Nick);
}

void CIRCSock::Connected() {
	DEBUG(GetSockName() << " == Connected()");

	CString sPass = m_sPass;
	CString sNick = m_pUser->GetNick();
	CString sIdent = m_pUser->GetIdent();
	CString sRealName = m_pUser->GetRealName();

	MODULECALL(OnIRCRegistration(sPass, sNick, sIdent, sRealName), m_pUser, NULL, return);

	PutIRC("CAP LS");

	if (!sPass.empty()) {
		PutIRC("PASS " + sPass);
	}

	PutIRC("NICK " + sNick);
	PutIRC("USER " + sIdent + " \"" + sIdent + "\" \"" + sIdent + "\" :" + sRealName);

	// SendAltNick() needs this
	m_Nick.SetNick(sNick);
}

void CIRCSock::Disconnected() {
	MODULECALL(OnIRCDisconnected(), m_pUser, NULL, NOTHING);

	DEBUG(GetSockName() << " == Disconnected()");
	if (!m_pUser->IsBeingDeleted() && m_pUser->GetIRCConnectEnabled() &&
			m_pUser->GetServers().size() != 0) {
		m_pUser->PutStatus("Disconnected from IRC. Reconnecting...");
	}
	m_pUser->ClearRawBuffer();
	m_pUser->ClearMotdBuffer();

	ResetChans();

	// send a "reset user modes" cmd to the client.
	// otherwise, on reconnect, it might think it still
	// had user modes that it actually doesn't have.
	CString sUserMode;
	for (set<unsigned char>::const_iterator it = m_scUserModes.begin(); it != m_scUserModes.end(); ++it) {
		sUserMode += *it;
	}
	if (!sUserMode.empty()) {
		m_pUser->PutUser(":" + m_pUser->GetIRCNick().GetNickMask() + " MODE " + m_pUser->GetIRCNick().GetNick() + " :-" + sUserMode);
	}

	// also clear the user modes in our space:
	m_scUserModes.clear();
}

void CIRCSock::SockError(int iErrno) {
	CString sError;

	if (iErrno == EDOM) {
		sError = "Your bind host could not be resolved";
	} else if (iErrno == EADDRNOTAVAIL) {
		// Csocket uses this if it can't resolve the dest host name
		// ...but it also does generate this if bind() fails -.-
		sError = strerror(iErrno);
		if (GetBindHost().empty())
			sError += " (Is your IRC server's host name valid?)";
		else
			sError += " (Is your IRC server's host name and ZNC bind host valid?)";
	} else {
		sError = strerror(iErrno);
	}

	DEBUG(GetSockName() << " == SockError(" << iErrno << " "
			<< sError << ")");
	if (!m_pUser->IsBeingDeleted()) {
		if (GetConState() != CST_OK)
			m_pUser->PutStatus("Cannot connect to IRC (" +
				sError + "). Retrying...");
		else
			m_pUser->PutStatus("Disconnected from IRC (" +
				sError + ").  Reconnecting...");
	}
	m_pUser->ClearRawBuffer();
	m_pUser->ClearMotdBuffer();

	ResetChans();
	m_scUserModes.clear();
}

void CIRCSock::Timeout() {
	DEBUG(GetSockName() << " == Timeout()");
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("IRC connection timed out.  Reconnecting...");
	}
	m_pUser->ClearRawBuffer();
	m_pUser->ClearMotdBuffer();

	ResetChans();
	m_scUserModes.empty();
}

void CIRCSock::ConnectionRefused() {
	DEBUG(GetSockName() << " == ConnectionRefused()");
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("Connection Refused.  Reconnecting...");
	}
	m_pUser->ClearRawBuffer();
	m_pUser->ClearMotdBuffer();
}

void CIRCSock::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	m_pUser->PutStatus("Received a too long line from the IRC server!");
	Quit();
}

void CIRCSock::ParseISupport(const CString& sLine) {
	VCString vsTokens;
	VCString::iterator it;

	sLine.Split(" ", vsTokens, false);

	for (it = vsTokens.begin(); it != vsTokens.end(); ++it) {
		CString sName = it->Token(0, false, "=");
		CString sValue = it->Token(1, true, "=");

		if (sName.Equals("PREFIX")) {
			CString sPrefixes = sValue.Token(1, false, ")");
			CString sPermModes = sValue.Token(0, false, ")");
			sPermModes.TrimLeft("(");

			if (!sPrefixes.empty() && sPermModes.size() == sPrefixes.size()) {
				m_sPerms = sPrefixes;
				m_sPermModes = sPermModes;
			}
		} else if (sName.Equals("CHANTYPES")) {
			m_pUser->SetChanPrefixes(sValue);
		} else if (sName.Equals("NICKLEN")) {
			unsigned int uMax = sValue.ToUInt();

			if (uMax) {
				m_uMaxNickLen = uMax;
			}
		} else if (sName.Equals("CHANMODES")) {
			if (!sValue.empty()) {
				m_mueChanModes.clear();

				for (unsigned int a = 0; a < 4; a++) {
					CString sModes = sValue.Token(a, false, ",");

					for (unsigned int b = 0; b < sModes.size(); b++) {
						m_mueChanModes[sModes[b]] = (EChanModeArgs) a;
					}
				}
			}
		} else if (sName.Equals("NAMESX")) {
			if (m_bNamesx)
				continue;
			m_bNamesx = true;
			PutIRC("PROTOCTL NAMESX");
		} else if (sName.Equals("UHNAMES")) {
			if (m_bUHNames)
				continue;
			m_bUHNames = true;
			PutIRC("PROTOCTL UHNAMES");
		}
	}
}

void CIRCSock::ForwardRaw353(const CString& sLine) const {
	vector<CClient*>& vClients = m_pUser->GetClients();
	vector<CClient*>::iterator it;

	for (it = vClients.begin(); it != vClients.end(); ++it) {
		ForwardRaw353(sLine, *it);
	}
}

void CIRCSock::ForwardRaw353(const CString& sLine, CClient* pClient) const {
	CString sNicks = sLine.Token(5, true);
	if (sNicks.Left(1) == ":")
		sNicks.LeftChomp();

	if ((!m_bNamesx || pClient->HasNamesx()) && (!m_bUHNames || pClient->HasUHNames())) {
		// Client and server have both the same UHNames and Namesx stuff enabled
		m_pUser->PutUser(sLine, pClient);
	} else {
		// Get everything except the actual user list
		CString sTmp = sLine.Token(0, false, " :") + " :";

		VCString vsNicks;
		VCString::const_iterator it;

		// This loop runs once for every nick on the channel
		sNicks.Split(" ", vsNicks, false);
		for (it = vsNicks.begin(); it != vsNicks.end(); ++it) {
			CString sNick = *it;
			if (sNick.empty())
				break;

			if (m_bNamesx && !pClient->HasNamesx() && IsPermChar(sNick[0])) {
				// Server has, client doesn't have NAMESX, so we just use the first perm char
				size_t pos = sNick.find_first_not_of(GetPerms());
				if (pos >= 2 && pos != CString::npos) {
					sNick = sNick[0] + sNick.substr(pos);
				}
			}

			if (m_bUHNames && !pClient->HasUHNames()) {
				// Server has, client hasnt UHNAMES,
				// so we strip away ident and host.
				sNick = sNick.Token(0, false, "!");
			}

			sTmp += sNick + " ";
		}
		// Strip away the spaces we inserted at the end
		sTmp.TrimRight(" ");
		m_pUser->PutUser(sTmp, pClient);
	}
}

void CIRCSock::SendAltNick(const CString& sBadNick) {
	const CString& sLastNick = m_Nick.GetNick();

	// We don't know the maximum allowed nick length yet, but we know which
	// nick we sent last. If sBadNick is shorter than that, we assume the
	// server truncated our nick.
	if (sBadNick.length() < sLastNick.length())
		m_uMaxNickLen = sBadNick.length();

	unsigned int uMax = m_uMaxNickLen;

	const CString& sConfNick = m_pUser->GetNick();
	const CString& sAltNick  = m_pUser->GetAltNick();
	CString sNewNick;

	if (sLastNick.Equals(sConfNick)) {
		if ((!sAltNick.empty()) && (!sConfNick.Equals(sAltNick))) {
			sNewNick = sAltNick;
		} else {
			sNewNick = sConfNick.Left(uMax - 1) + "-";
		}
	} else if (sLastNick.Equals(sAltNick)) {
		sNewNick = sConfNick.Left(uMax -1) + "-";
	} else if (sLastNick.Equals(CString(sConfNick.Left(uMax -1) + "-"))) {
		sNewNick = sConfNick.Left(uMax -1) + "|";
	} else if (sLastNick.Equals(CString(sConfNick.Left(uMax -1) + "|"))) {
		sNewNick = sConfNick.Left(uMax -1) + "^";
	} else if (sLastNick.Equals(CString(sConfNick.Left(uMax -1) + "^"))) {
		sNewNick = sConfNick.Left(uMax -1) + "a";
	} else {
		char cLetter = 0;
		if (sBadNick.empty()) {
			m_pUser->PutUser("No free nick available");
			Quit();
			return;
		}

		cLetter = sBadNick.Right(1)[0];

		if (cLetter == 'z') {
			m_pUser->PutUser("No free nick found");
			Quit();
			return;
		}

		sNewNick = sConfNick.Left(uMax -1) + ++cLetter;
	}
	PutIRC("NICK " + sNewNick);
	m_Nick.SetNick(sNewNick);
}

unsigned char CIRCSock::GetPermFromMode(unsigned char uMode) const {
	if (m_sPermModes.size() == m_sPerms.size()) {
		for (unsigned int a = 0; a < m_sPermModes.size(); a++) {
			if (m_sPermModes[a] == uMode) {
				return m_sPerms[a];
			}
		}
	}

	return 0;
}

CIRCSock::EChanModeArgs CIRCSock::GetModeType(unsigned char uMode) const {
	map<unsigned char, EChanModeArgs>::const_iterator it = m_mueChanModes.find(uMode);

	if (it == m_mueChanModes.end()) {
		return NoArg;
	}

	return it->second;
}

void CIRCSock::ResetChans() {
	for (map<CString, CChan*>::iterator a = m_msChans.begin(); a != m_msChans.end(); ++a) {
		a->second->Reset();
	}
}

