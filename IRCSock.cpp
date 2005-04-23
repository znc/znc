#include "znc.h"
#include "IRCSock.h"
#include "DCCBounce.h"
#include "UserSock.h"
#include <time.h>

CIRCSock::CIRCSock(CZNC* pZNC, CUser* pUser) : Csock() {
	m_pZNC = pZNC;
	m_pUser = pUser;
	m_pUserSock = NULL;
	m_uQueryBufferCount = 50;
	m_bISpoofReleased = false;
	m_bKeepNick = true;
	m_bAuthed = false;
	EnableReadLine();
	m_RawBuffer.SetLineCount(100);		// This should be more than enough raws, especially since we are buffering the MOTD separately
	m_Nick.SetIdent(pUser->GetIdent());
	m_Nick.SetHost(pUser->GetVHost());
}

CIRCSock::~CIRCSock() {
	const vector<CChan*>& vChans = m_pUser->GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		vChans[a]->SetIsOn(false);
	}

	if (m_pUserSock) {
		m_pUserSock->IRCDisconnected();
		m_pUserSock = NULL;
	}

	for (map<string, CChan*>::iterator a = m_msChans.begin(); a != m_msChans.end(); a++) {
		delete a->second;
	}

	if (!m_bISpoofReleased) {
		m_pZNC->ReleaseISpoof();
	}

	PutServ("QUIT :" + m_pUser->GetQuitMsg());
	m_msChans.clear();
}

void CIRCSock::ReadLine(const string& sData) {
	string sLine = sData;

	while ((CUtils::Right(sLine, 1) == "\r") || (CUtils::Right(sLine, 1) == "\n")) {
		CUtils::RightChomp(sLine);
	}

	DEBUG_ONLY(cout << GetSockName() << " <- [" << sLine << "]" << endl);

#ifdef _MODULES
	if (m_pUser->GetModules().OnRaw(sLine)) {
		return;
	}
#endif

	if (strncasecmp(sLine.c_str(), "PING ", 5) == 0) {
		PutServ("PONG " + sLine.substr(5));
	} else if (CUtils::wildcmp(":* * *", sLine.c_str())) { //"^:(\\S+) (\\d\\d\\d) (.*?) (.*)$", vCap)) {
		string sCmd = CUtils::Token(sLine, 1);

		if ((sCmd.length() == 3) && (isdigit(sCmd[0])) && (isdigit(sCmd[1])) && (isdigit(sCmd[2]))) {
			string sServer = CUtils::Token(sLine, 0); CUtils::LeftChomp(sServer);
			unsigned int uRaw = strtoul(sCmd.c_str(), NULL, 10);
			string sNick = CUtils::Token(sLine, 2);
			string sRest = CUtils::Token(sLine, 3, true);

			switch (uRaw) {
				case 1:	{// :irc.server.com 001 nick :Welcome to the Internet Relay Network nick
					SetTimeout(900);	// Now that we are connected, let nature take its course
					PutServ("WHO " + sNick);
#ifdef _MODULES
					m_pUser->GetModules().OnIRCConnected();
#endif
					m_bAuthed = true;
					m_pUser->PutStatus("Connected!");

					if (m_pUserSock) {
						string sClientNick = m_pUserSock->GetNick();
						if (strcasecmp(sClientNick.c_str(), sNick.c_str()) != 0) {
							// If they connected with a nick that doesn't match the one we got on irc, then we need to update them
							PutUser(":" + sClientNick + "!" + m_Nick.GetIdent() + "@" + m_Nick.GetHost() + " NICK :" + sNick);
						}
					}

					SetNick(sNick);

					m_RawBuffer.Clear();
					m_RawBuffer.AddLine(":" + sServer + " " + sCmd + " ", " " + sRest);

					// Now that we are connected, we need to join our chans
					const vector<CChan*>& vChans = m_pUser->GetChans();

					for (unsigned int a = 0; a < vChans.size(); a++) {
						PutServ("JOIN " + vChans[a]->GetName() + " " + vChans[a]->GetKey());
					}

					m_pZNC->ReleaseISpoof();
					m_bISpoofReleased = true;

					break;
				}
				case 2:
				case 3:
				case 4:
				case 5:
				case 250:	// highest connection count
				case 251:	// user count
				case 252:	// oper count
				case 254:	// channel count
				case 255:	// client count
				case 265:	// local users
				case 266:	// global users
					m_RawBuffer.AddLine(":" + sServer + " " + sCmd + " ", " " + sRest);
					break;
				case 375:	// begin motd
					m_vsMotdBuffer.clear();
				case 372:	// motd
				case 376:	// end motd
					m_vsMotdBuffer.push_back(sLine);
					break;
				case 471: // :irc.server.net 471 nick #chan :Cannot join channel (+l)
				case 473: // :irc.server.net 473 nick #chan :Cannot join channel (+i)
				case 475: // :irc.server.net 475 nick #chan :Cannot join channel (+k)
					if (m_pUserSock) {
						CChan* pChan = m_pUser->FindChan(sRest.substr(0, sRest.find(' ')));

						if ((pChan) && (!pChan->IsOn())) {
							if (!pChan->DecClientRequests()) {
								return;
							}
						}
					}

					break;
				case 433: {
					string sBadNick = CUtils::Token(sRest, 0);
					string sConfNick = m_pUser->GetNick();

					if (sNick == "*") {
						string sAltNick = m_pUser->GetAltNick();

						if (strcasecmp(sBadNick.c_str(), sConfNick.c_str()) == 0) {
							if ((!sAltNick.empty()) && (strcasecmp(sConfNick.c_str(), sAltNick.c_str()) != 0)) {
								PutServ("NICK " + sAltNick);
							} else {
								PutServ("NICK " + CUtils::Left(sConfNick, 8) + "-");
							}
						} else if (strcasecmp(sBadNick.c_str(), sAltNick.c_str()) == 0) {
							PutServ("NICK " + CUtils::Left(sConfNick, 8) + "-");
						} else if (strcasecmp(sBadNick.c_str(), string(CUtils::Left(sConfNick, 8) + "-").c_str()) == 0) {
							PutServ("NICK " + CUtils::Left(sConfNick, 8) + "|");
						} else if (strcasecmp(sBadNick.c_str(), string(CUtils::Left(sConfNick, 8) + "|").c_str()) == 0) {
							PutServ("NICK " + CUtils::Left(sConfNick, 8) + "^");
						} else {
							char cLetter = 0;
							if (sBadNick.empty()) {
									Close();
									return;
							}

							cLetter = CUtils::Right(sBadNick, 1)[0];

							if (cLetter == 'z') {
								Close();
								return;
							}

							string sSend = "NICK " + CUtils::Left(sConfNick, 8) + cLetter++;
							PutServ(sSend);
						}

						return;
					} else {
						// :irc.server.net 433 mynick badnick :Nickname is already in use.
						if ((m_bKeepNick) && (m_pUser->KeepNick())) {
							if (strcasecmp(sBadNick.c_str(), sConfNick.c_str()) == 0) {
								if ((!m_pUserSock) || (!m_pUserSock->DecKeepNickCounter())) {
									return;
								}
							}
						}
					}
					break;
				}
				case 315: {
					// :irc.server.com 315 yournick #chan :End of /WHO list.
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sLine, 3));

					if (pChan) {
						pChan->SetWhoDone();
					}

					break;
				}
				case 331: {
					// :irc.server.com 331 yournick #chan :No topic is set.
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sLine, 3));

					if (pChan) {
						pChan->SetTopic("");
					}

					break;
				}
				case 332: {
					// :irc.server.com 332 yournick #chan :This is a topic
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sLine, 3));

					if (pChan) {
						string sTopic = CUtils::Token(sLine, 4, true);
						CUtils::LeftChomp(sTopic);
						pChan->SetTopic(sTopic);
					}

					break;
				}
				case 333: {
					// :irc.server.com 333 yournick #chan setternick 1112320796
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sLine, 3));

					if (pChan) {
						string sNick = CUtils::Token(sLine, 4);
						unsigned long ulDate = strtoul(CUtils::Token(sLine, 5).c_str(), NULL, 10);

						pChan->SetTopicOwner(sNick);
						pChan->SetTopicDate(ulDate);
					}

					break;
				}
				case 352: {
					// :irc.yourserver.com 352 yournick #chan ident theirhost.com irc.theirserver.com theirnick H :0 Real Name
					string sServer = CUtils::Token(sLine, 0);
					string sNick = CUtils::Token(sLine, 7);
					string sIdent = CUtils::Token(sLine, 4);
					string sHost = CUtils::Token(sLine, 5);

					CUtils::LeftChomp(sServer);

					if (strcasecmp(sNick.c_str(), GetNick().c_str()) == 0) {
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
				case 324: {	// MODE
					CUtils::Trim(sRest);
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sRest, 0));

					if (pChan) {
						pChan->SetModes(CUtils::Token(sRest, 1, true));
					}
				}
					break;
				case 353: {	// NAMES
					CUtils::Trim(sRest);
					// Todo: allow for non @+= server msgs
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sRest, 1));
					if (pChan) {
						string sNicks = CUtils::Token(sRest, 2, true);
						if (CUtils::Left(sNicks, 1) == ":") {
							CUtils::LeftChomp(sNicks);
						}

						pChan->AddNicks(sNicks);
					}
				}
					break;
				case 366: {	// end of names list
					PutUser(sLine);	// First send them the raw

					// :irc.server.com 366 nick #chan :End of /NAMES list.
					CChan* pChan = m_pUser->FindChan(CUtils::Token(sRest, 0));

					if (pChan) {
						if (IsUserAttached()) {
							const vector<string>& vsBuffer = pChan->GetBuffer();

							if (vsBuffer.size()) {
								PutUser(":***!znc@znc.com PRIVMSG " + pChan->GetName() + " :Buffer Playback...");

								for (unsigned int a = 0; a < vsBuffer.size(); a++) {
									PutUser(vsBuffer[a]);
								}

								if (!pChan->KeepBuffer()) {
									pChan->ClearBuffer();
								}

								PutUser(":***!znc@znc.com PRIVMSG " + pChan->GetName() + " :Playback Complete.");
							}
						}

						if (!pChan->IsOn()) {
							pChan->SetIsOn(true);
							PutServ("MODE " + pChan->GetName());

							// If we are the only one in the chan, set our default modes
							if (pChan->GetNickCount() == 1) {
								string sModes = pChan->GetDefaultModes();

								if (sModes.empty()) {
									sModes = m_pUser->GetDefaultChanModes();
								}

								if (!sModes.empty()) {
									PutServ("MODE " + pChan->GetName() + " " + sModes);
								}
							}
						}
					}

					return;	// return so we don't send them the raw twice
				}
			}
		} else { //if (CUtils::wildcmp(":*!*@* * *", sLine.c_str())) {
			string sNickMask = CUtils::Token(sLine, 0);
			CUtils::LeftChomp(sNickMask);

			string sNick = CUtils::Token(sNickMask, 0, false, '!');
			string sCmd = CUtils::Token(sLine, 1);
			string sRest = CUtils::Token(sLine, 2, true);

			if (strcasecmp(sCmd.c_str(), "NICK") == 0) {
				string sNewNick = sRest;
				bool bIsVisible = false;

				if (CUtils::Left(sNewNick, 1) == ":") {
					CUtils::LeftChomp(sNewNick);
				}

				vector<CChan*> vFoundChans;
				const vector<CChan*>& vChans = m_pUser->GetChans();

				for (unsigned int a = 0; a < vChans.size(); a++) {
					CChan* pChan = vChans[a];

					if (pChan->ChangeNick(sNick, sNewNick)) {
						vFoundChans.push_back(pChan);

						if (!pChan->IsDetached()) {
							bIsVisible = true;
						}
					}
				}

				if (strcasecmp(sNick.c_str(), GetNick().c_str()) == 0) {
					SetNick(sNewNick);
					if (strcasecmp(sNick.c_str(), m_pUser->GetNick().c_str()) == 0) {
						// If the user changes his nick away from the config nick, we shut off keepnick for this session
						m_bKeepNick = false;
					}
				} else if (strcasecmp(sNick.c_str(), m_pUser->GetNick().c_str()) == 0) {
					KeepNick();
				}
#ifdef _MODULES
				m_pUser->GetModules().OnNick(sNickMask, sNewNick, vFoundChans);
#endif

				if (!bIsVisible) {
					return;
				}
			} else if (strcasecmp(sCmd.c_str(), "QUIT") == 0) {
				string sMessage = sRest;
				bool bIsVisible = false;

				if (CUtils::Left(sMessage, 1) == ":") {
					CUtils::LeftChomp(sMessage);
				}

				// :nick!ident@host.com QUIT :message
				CNick Nick(sNickMask);

				vector<CChan*> vFoundChans;
				const vector<CChan*>& vChans = m_pUser->GetChans();

				for (unsigned int a = 0; a < vChans.size(); a++) {
					CChan* pChan = vChans[a];

					if (pChan->RemNick(sNick)) {
						vFoundChans.push_back(pChan);

						if (!pChan->IsDetached()) {
							bIsVisible = true;
						}
					}
				}

				if (strcasecmp(Nick.GetNick().c_str(), m_pUser->GetNick().c_str()) == 0) {
					KeepNick();
				}

#ifdef _MODULES
				m_pUser->GetModules().OnQuit(Nick, sMessage, vFoundChans);
#endif

				if (!bIsVisible) {
					return;
				}
			} else if (strcasecmp(sCmd.c_str(), "JOIN") == 0) {
				string sChan = CUtils::Token(sRest, 0);
				if (CUtils::Left(sChan, 1) == ":") {
					CUtils::LeftChomp(sChan);
				}

				if (strcasecmp(sNick.c_str(), GetNick().c_str()) == 0) {
					m_pUser->AddChan(sChan);
				}

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->AddNick(sNickMask);
#ifdef _MODULES
					m_pUser->GetModules().OnJoin(sNickMask, *pChan);
#endif
					if (pChan->IsDetached()) {
						return;
					}
				}
			} else if (strcasecmp(sCmd.c_str(), "PART") == 0) {
				string sChan = CUtils::Token(sRest, 0);
				if (CUtils::Left(sChan, 1) == ":") {
					CUtils::LeftChomp(sChan);
				}

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->RemNick(sNick);
#ifdef _MODULES
					m_pUser->GetModules().OnPart(sNickMask, *pChan);
#endif
				}

				if (strcasecmp(sNick.c_str(), GetNick().c_str()) == 0) {
					m_pUser->DelChan(sChan);
				}

				if ((pChan) && (pChan->IsDetached())) {
					return;
				}
			} else if (strcasecmp(sCmd.c_str(), "MODE") == 0) {
				string sChan = CUtils::Token(sRest, 0);
				string sModes = CUtils::Token(sRest, 1, true);

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->ModeChange(sModes, sNick);

					if (pChan->IsDetached()) {
						return;
					}
				}
			} else if (strcasecmp(sCmd.c_str(), "KICK") == 0) {
				// :opnick!ident@host.com KICK #chan nick :msg
				string sChan = CUtils::Token(sRest, 0);
				string sKickedNick = CUtils::Token(sRest, 1);
				string sMsg = CUtils::Token(sRest, 2, true);
				CUtils::LeftChomp(sMsg);

				CChan* pChan = m_pUser->FindChan(sChan);

				if (pChan) {
					pChan->RemNick(sKickedNick);
#ifdef _MODULES
					m_pUser->GetModules().OnKick(sNickMask, sKickedNick, *pChan, sMsg);
#endif
				}

				if (strcasecmp(GetNick().c_str(), sKickedNick.c_str()) == 0) {
					string sKey;

					if (pChan) {
						sKey = pChan->GetKey();
						pChan->SetIsOn(false);
					}

					PutServ("JOIN " + sChan + " " + sKey);
				}

				if ((pChan) && (pChan->IsDetached())) {
					return;
				}
			} else if (strcasecmp(sCmd.c_str(), "NOTICE") == 0) {
				// :nick!ident@host.com NOTICE #chan :Message

				CNick Nick(sNickMask);

				string sTarget = CUtils::Token(sRest, 0);
				string sMsg = CUtils::Token(sRest, 1, true);
				CUtils::LeftChomp(sMsg);

				if (CUtils::wildcmp("\001*\001", sMsg.c_str())) {
					CUtils::LeftChomp(sMsg);
					CUtils::RightChomp(sMsg);

					if (strcasecmp(sTarget.c_str(), GetNick().c_str()) == 0) {
						if (OnCTCPReply(sNickMask, sMsg)) {
							return;
						}
					}

					PutUser(":" + sNickMask + " NOTICE " + sTarget + " :\001" + sMsg + "\001");
					return;
				} else {
					if (strcasecmp(sTarget.c_str(), GetNick().c_str()) == 0) {
						if (OnPrivNotice(sNickMask, sMsg)) {
							return;
						}
					} else {
						if (OnChanNotice(sNickMask, sTarget, sMsg)) {
							return;
						}
					}
				}

				PutUser(":" + sNickMask + " NOTICE " + sTarget + " :" + sMsg);
				return;
			} else if (strcasecmp(sCmd.c_str(), "TOPIC") == 0) {
				// :nick!ident@host.com TOPIC #chan :This is a topic
				CChan* pChan = m_pUser->FindChan(CUtils::Token(sLine, 2));

				if (pChan) {
					CNick Nick(sNickMask);
					string sTopic = CUtils::Token(sLine, 3, true);
					CUtils::LeftChomp(sTopic);
					pChan->SetTopicOwner(Nick.GetNick());
					pChan->SetTopicDate((unsigned long) time(NULL));	// @todo use local time
					pChan->SetTopic(sTopic);
				}
			} else if (strcasecmp(sCmd.c_str(), "PRIVMSG") == 0) {
				// :nick!ident@host.com PRIVMSG #chan :Message

				CNick Nick(sNickMask);
				string sTarget = CUtils::Token(sRest, 0);
				string sMsg = CUtils::Token(sRest, 1, true);

				if (CUtils::Left(sMsg, 1) == ":") {
					CUtils::LeftChomp(sMsg);
				}

				if (CUtils::wildcmp("\001*\001", sMsg.c_str())) {
					CUtils::LeftChomp(sMsg);
					CUtils::RightChomp(sMsg);

					if (strcasecmp(sTarget.c_str(), GetNick().c_str()) == 0) {
						if (OnPrivCTCP(sNickMask, sMsg)) {
							return;
						}
					} else {
						if (OnChanCTCP(sNickMask, sTarget, sMsg)) {
							return;
						}
					}

					PutUser(":" + sNickMask + " PRIVMSG " + sTarget + " :\001" + sMsg + "\001");
					return;
				} else {
					if (strcasecmp(sTarget.c_str(), GetNick().c_str()) == 0) {
						if (OnPrivMsg(sNickMask, sMsg)) {
							return;
						}
					} else {
						if (OnChanMsg(sNickMask, sTarget, sMsg)) {
							return;
						}
					}

					PutUser(":" + sNickMask + " PRIVMSG " + sTarget + " :" + sMsg);
					return;
				}
			}
		}
	}

	PutUser(sLine);
}

void CIRCSock::KeepNick() {
	const string& sConfNick = m_pUser->GetNick();

	if ((m_bAuthed) && (m_bKeepNick) && (m_pUser->KeepNick()) && (strcasecmp(GetNick().c_str(), sConfNick.c_str()) != 0)) {
		PutServ("NICK " + sConfNick);
	}
}

bool CIRCSock::OnCTCPReply(const string& sNickMask, string& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnCTCPReply(sNickMask, sMessage)) {
		return true;
	}
#endif

	return false;
}

bool CIRCSock::OnPrivCTCP(const string& sNickMask, string& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnPrivCTCP(sNickMask, sMessage)) {
		return true;
	}
#endif

	// DCC CHAT chat 2453612361 44592
	if (strncasecmp(sMessage.c_str(), "DCC ", 4) == 0) {
		string sType = CUtils::Token(sMessage, 1);
		string sFile = CUtils::Token(sMessage, 2);
		unsigned long uLongIP = strtoul(CUtils::Token(sMessage, 3).c_str(), NULL, 10);
		unsigned short uPort = strtoul(CUtils::Token(sMessage, 4).c_str(), NULL, 10);
		unsigned long uFileSize = strtoul(CUtils::Token(sMessage, 5).c_str(), NULL, 10);

		if (strcasecmp(sType.c_str(), "CHAT") == 0) {
			if (m_pUserSock) {
				CNick FromNick(sNickMask);
				unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, "", true, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));

				if (uBNCPort) {
					PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC CHAT chat " + CUtils::ToString(CUtils::GetLongIP(GetLocalIP())) + " " + CUtils::ToString(uBNCPort) + "\001");
				}
			}
		} else if (strcasecmp(sType.c_str(), "SEND") == 0) {
			// DCC SEND readme.txt 403120438 5550 1104
			CNick FromNick(sNickMask);

			unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, sFile, false, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));
			if (uBNCPort) {
				PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC SEND " + sFile + " " + CUtils::ToString(CUtils::GetLongIP(GetLocalIP())) + " " + CUtils::ToString(uBNCPort) + " " + CUtils::ToString(uFileSize) + "\001");
			}
		} else if (strcasecmp(sType.c_str(), "RESUME") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			CDCCBounce* pSock = (CDCCBounce*) m_pZNC->GetManager().FindSockByLocalPort(atoi(CUtils::Token(sMessage, 3).c_str()));

			if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
				PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CUtils::ToString(pSock->GetUserPort()) + " " + CUtils::Token(sMessage, 4) + "\001");
			}
		} else if (strcasecmp(sType.c_str(), "ACCEPT") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			TSocketManager<Csock>& Manager = m_pZNC->GetManager();

			for (unsigned int a = 0; a < Manager.size(); a++) {
				CDCCBounce* pSock = (CDCCBounce*) Manager[a];

				if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
					if (pSock->GetUserPort() == atoi(CUtils::Token(sMessage, 3).c_str())) {
						PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CUtils::ToString(pSock->GetLocalPort()) + " " + CUtils::Token(sMessage, 4) + "\001");
					}
				}
			}
		}

		return true;
	}

	return false;
}

bool CIRCSock::OnPrivNotice(const string& sNickMask, string& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnPrivNotice(sNickMask, sMessage)) {
		return true;
	}
#endif
	if (!m_pUserSock) {
		// If the user is detached, add to the buffer
		m_QueryBuffer.AddLine(":" + sNickMask + " NOTICE ", " :" + sMessage);
	}

	return false;
}

bool CIRCSock::OnPrivMsg(const string& sNickMask, string& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnPrivMsg(sNickMask, sMessage)) {
		return true;
	}
#endif
	if (!m_pUserSock) {
		// If the user is detached, add to the buffer
		m_QueryBuffer.AddLine(":" + sNickMask + " PRIVMSG ", " :" + sMessage);
	}

	return false;
}

bool CIRCSock::OnChanCTCP(const string& sNickMask, const string& sChan, string& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
#ifdef _MODULES
		if ((m_pUser->GetModules().OnChanCTCP(sNickMask, *pChan, sMessage)) || (pChan->IsDetached())) {
			return true;
		}
#endif
	}

	return false;
}

bool CIRCSock::OnChanNotice(const string& sNickMask, const string& sChan, string& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
#ifdef _MODULES
		if (m_pUser->GetModules().OnChanNotice(sNickMask, *pChan, sMessage)) {
			return true;
		}
#endif
		if ((pChan->KeepBuffer()) || (!m_pUserSock)) {
			pChan->AddBuffer(":" + sNickMask + " NOTICE " + sChan + " :" + sMessage);
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

bool CIRCSock::OnChanMsg(const string& sNickMask, const string& sChan, string& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
#ifdef _MODULES
		if (m_pUser->GetModules().OnChanMsg(sNickMask, *pChan, sMessage)) {
			return true;
		}
#endif
		if ((pChan->KeepBuffer()) || (!m_pUserSock)) {
			pChan->AddBuffer(":" + sNickMask + " PRIVMSG " + sChan + " :" + sMessage);
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

void CIRCSock::UserConnected(CUserSock* pUserSock) {
	if (m_pUserSock) {
		m_pUserSock->BouncedOff();
	}

	m_pUserSock = pUserSock;

	if (m_RawBuffer.IsEmpty()) {
		PutUser(":irc.znc.com 001 " + m_pUserSock->GetNick() + " :- Welcome to ZNC -");
	} else {
		unsigned int uIdx = 0;
		string sLine;

		while (m_RawBuffer.GetLine(GetNick(), sLine, uIdx++)) {
			PutUser(sLine);
		}
	}

	// Send the cached MOTD
	for (unsigned int a = 0; a < m_vsMotdBuffer.size(); a++) {
		PutUser(m_vsMotdBuffer[a]);
	}

	const vector<CChan*>& vChans = m_pUser->GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		if ((vChans[a]->IsOn()) && (!vChans[a]->IsDetached())) {
			vChans[a]->JoinUser();
		}
	}

	string sBufLine;
	while (m_QueryBuffer.GetNextLine(GetNick(), sBufLine)) {
		PutUser(sBufLine);
	}
}

void CIRCSock::UserDisconnected() {
	m_pUserSock = NULL;
}

void CIRCSock::PutServ(const string& sLine) {
	DEBUG_ONLY(cout << GetSockName() << " -> [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CIRCSock::PutUser(const string& sLine) {
	if (m_pUserSock) {
		m_pUserSock->PutServ(sLine);
	}
}

void CIRCSock::PutStatus(const string& sLine) {
	if (m_pUserSock) {
		m_pUserSock->PutStatus(sLine);
	}
}

void CIRCSock::SetNick(const string& sNick) {
	m_Nick.SetNick(sNick);

	if (m_pUserSock) {
		m_pUserSock->SetNick(sNick);
	}
}

void CIRCSock::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected()" << endl);

	CUserSock* pUserSock = (CUserSock*) m_pZNC->FindSockByName("USR::" + m_pUser->GetUserName());

	if (pUserSock) {
		m_pUserSock = pUserSock;
		pUserSock->IRCConnected(this);
	}

	if (!m_sPass.empty()) {
		PutServ("PASS " + m_sPass);
	}

	PutServ("NICK " + m_pUser->GetNick());
	PutServ("USER " + m_pUser->GetIdent() + " \"" + m_pUser->GetIdent() + "\" \"" + m_pUser->GetIdent() + "\" :" + m_pUser->GetRealName());
}

void CIRCSock::Disconnected() {
#ifdef _MODULES
	DEBUG_ONLY(cout << "OnIRCDisconnected()" << endl);
	m_pUser->GetModules().OnIRCDisconnected();
#endif

	DEBUG_ONLY(cout << GetSockName() << " == Disconnected()" << endl);
	m_pUser->PutStatus("Disconnected from IRC.  Reconnecting...");
}

void CIRCSock::SockError(int iErrno) {
	DEBUG_ONLY(cout << GetSockName() << " == SockError(" << iErrno << ")" << endl);
	m_pUser->PutStatus("Disconnected from IRC.  Reconnecting...");
}

void CIRCSock::Timeout() {
	DEBUG_ONLY(cout << GetSockName() << " == Timeout()" << endl);
	m_pUser->PutStatus("IRC connection timed out.  Reconnecting...");
}

void CIRCSock::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
	m_pUser->PutStatus("Connection Refused.  Reconnecting...");
}

