#include "znc.h"
#include "IRCSock.h"
#include "DCCBounce.h"
#include "Client.h"
#include <time.h>

CIRCSock::CIRCSock(CUser* pUser) : Csock() {
	m_pUser = pUser;
	m_bISpoofReleased = false;
	m_bKeepNick = true;
	m_bAuthed = false;
	m_bOrigNickPending = false;
	EnableReadLine();
	m_Nick.SetIdent(pUser->GetIdent());
	m_Nick.SetHost(pUser->GetVHost());

	m_uMaxNickLen = 9;
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
}

CIRCSock::~CIRCSock() {
	const vector<CChan*>& vChans = m_pUser->GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		vChans[a]->Reset();
	}

	m_pUser->IRCDisconnected();

	for (map<CString, CChan*>::iterator a = m_msChans.begin(); a != m_msChans.end(); a++) {
		delete a->second;
	}

	if (!m_bISpoofReleased) {
		CZNC::Get().ReleaseISpoof();
	}

	PutIRC("QUIT :" + m_pUser->GetQuitMsg());
	m_msChans.clear();
}

void CIRCSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
		sLine.RightChomp();
	}

	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") IRC -> ZNC [" << sLine << "]" << endl);

#ifdef _MODULES
	CGlobalModules& GMods = CZNC::Get().GetModules();
	GMods.SetUser(m_pUser);
	if (GMods.OnRaw(sLine) || m_pUser->GetModules().OnRaw(sLine)) {
		return;
	}
	GMods.SetUser(NULL);
#endif

	if (strncasecmp(sLine.c_str(), "PING ", 5) == 0) {
		PutIRC("PONG " + sLine.substr(5));
	} else if (sLine.WildCmp(":* * *")) { //"^:(\\S+) (\\d\\d\\d) (.*?) (.*)$", vCap)) {
		CString sCmd = sLine.Token(1);

		if ((sCmd.length() == 3) && (isdigit(sCmd[0])) && (isdigit(sCmd[1])) && (isdigit(sCmd[2]))) {
			CString sServer = sLine.Token(0); sServer.LeftChomp();
			unsigned int uRaw = strtoul(sCmd.c_str(), NULL, 10);
			CString sNick = sLine.Token(2);
			CString sRest = sLine.Token(3, true);

			switch (uRaw) {
				case 1:	{// :irc.server.com 001 nick :Welcome to the Internet Relay Network nick
					m_pUser->SetIRCServer(sServer);
					SetTimeout(900);	// Now that we are connected, let nature take its course
					PutIRC("WHO " + sNick);

					m_pUser->StartAwayNickTimer();

					VOIDMODULECALL(OnIRCConnected());

					m_bAuthed = true;
					m_pUser->PutStatus("Connected!");

					vector<CClient*>& vClients = m_pUser->GetClients();

					for (unsigned int a = 0; a < vClients.size(); a++) {
						CClient* pClient = vClients[a];
						CString sClientNick = pClient->GetNick();

						if (sClientNick.CaseCmp(sNick) != 0) {
							// If they connected with a nick that doesn't match the one we got on irc, then we need to update them
							pClient->PutClient(":" + sClientNick + "!" + m_Nick.GetIdent() + "@" + m_Nick.GetHost() + " NICK :" + sNick);
						}
					}

					SetNick(sNick);

					m_pUser->ClearRawBuffer();
					m_pUser->AddRawBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);

					// Now that we are connected, we need to join our chans
					const vector<CChan*>& vChans = m_pUser->GetChans();

					for (unsigned int a = 0; a < vChans.size(); a++) {
						PutIRC("JOIN " + vChans[a]->GetName() + " " + vChans[a]->GetKey());
					}

					CZNC::Get().ReleaseISpoof();
					m_bISpoofReleased = true;

					break;
				}
				case 5:
					ParseISupport(sRest);
				case 2:
				case 3:
				case 4:
				case 250:	// highest connection count
				case 251:	// user count
				case 252:	// oper count
				case 254:	// channel count
				case 255:	// client count
				case 265:	// local users
				case 266:	// global users
					m_pUser->AddRawBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);
					break;
				case 375:	// begin motd
					m_pUser->ClearMotdBuffer();
				case 372:	// motd
				case 376:	// end motd
					m_pUser->AddMotdBuffer(":" + sServer + " " + sCmd + " ", " " + sRest);
					break;
				case 471: // :irc.server.net 471 nick #chan :Cannot join channel (+l)
				case 473: // :irc.server.net 473 nick #chan :Cannot join channel (+i)
				case 475: // :irc.server.net 475 nick #chan :Cannot join channel (+k)
					if (m_pUser->IsUserAttached()) {
						CChan* pChan = m_pUser->FindChan(sRest.substr(0, sRest.find(' ')));

						if ((pChan) && (!pChan->IsOn())) {
							if (!pChan->DecClientRequests()) {
								return;
							}
						}
					}

					break;
				case 433: {
					unsigned int uMax = GetMaxNickLen();
					CString sBadNick = sRest.Token(0);
					CString sConfNick = m_pUser->GetNick().Left(uMax);

					if (sNick == "*" || sNick.CaseCmp(CNick::Concat(sConfNick, m_pUser->GetAwaySuffix(), GetMaxNickLen())) == 0) {
						CString sAltNick = m_pUser->GetAltNick().Left(uMax);

						if (sBadNick.CaseCmp(sConfNick) == 0) {
							if ((!sAltNick.empty()) && (sConfNick.CaseCmp(sAltNick) != 0)) {
								PutIRC("NICK " + sAltNick);
							} else {
								PutIRC("NICK " + sConfNick.Left(uMax -1) + "-");
							}
						} else if (sBadNick.CaseCmp(sAltNick) == 0) {
							PutIRC("NICK " + sConfNick.Left(uMax -1) + "-");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "-")) == 0) {
							PutIRC("NICK " + sConfNick.Left(uMax -1) + "|");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "|")) == 0) {
							PutIRC("NICK " + sConfNick.Left(uMax -1) + "^");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "^")) == 0) {
							PutIRC("NICK " + sConfNick.Left(uMax -1) + "a");
						} else {
							char cLetter = 0;
							if (sBadNick.empty()) {
								Close();
								return;
							}

							cLetter = sBadNick.Right(1)[0];

							if (cLetter == 'z') {
								Close();
								return;
							}

							CString sSend = "NICK " + sConfNick.Left(uMax -1) + ++cLetter;
							PutIRC(sSend);
						}

						return;
					} else {
						// :irc.server.net 433 mynick badnick :Nickname is already in use.
						if ((m_bKeepNick) && (m_pUser->GetKeepNick())) {
							if (sBadNick.CaseCmp(sConfNick) == 0) {
								vector<CClient*>& vClients = m_pUser->GetClients();

								for (unsigned int a = 0; a < vClients.size(); a++) {
									CClient* pClient = vClients[a];

									if (!pClient || !pClient->DecKeepNickCounter()) {
										SetOrigNickPending(false);
										return;
									}
								}
							}
						}
					}
					break;
				}
				case 315: {
					// :irc.server.com 315 yournick #chan :End of /WHO list.
					CChan* pChan = m_pUser->FindChan(sLine.Token(3));

					if (pChan) {
						pChan->SetWhoDone();
					}

					break;
				}
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
						CString sNick = sLine.Token(4);
						unsigned long ulDate = strtoul(sLine.Token(5).c_str(), NULL, 10);

						pChan->SetTopicOwner(sNick);
						pChan->SetTopicDate(ulDate);
					}

					break;
				}
				case 352: {
					// :irc.yourserver.com 352 yournick #chan ident theirhost.com irc.theirserver.com theirnick H :0 Real Name
					CString sServer = sLine.Token(0);
					CString sNick = sLine.Token(7);
					CString sIdent = sLine.Token(4);
					CString sHost = sLine.Token(5);

					sServer.LeftChomp();

					if (sNick.CaseCmp(GetNick()) == 0) {
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
					sRest.Trim();
					CChan* pChan = m_pUser->FindChan(sRest.Token(0));

					if (pChan) {
						pChan->SetModes(sRest.Token(1, true));
					}
				}
					break;
				case 353: {	// NAMES
					sRest.Trim();
					// Todo: allow for non @+= server msgs
					CChan* pChan = m_pUser->FindChan(sRest.Token(1));
					if (pChan) {
						CString sNicks = sRest.Token(2, true);
						if (sNicks.Left(1) == ":") {
							sNicks.LeftChomp();
						}

						pChan->AddNicks(sNicks);
					}
				}
					break;
				case 366: {	// end of names list
					m_pUser->PutUser(sLine);	// First send them the raw

					// :irc.server.com 366 nick #chan :End of /NAMES list.
					CChan* pChan = m_pUser->FindChan(sRest.Token(0));

					if (pChan) {
						if (m_pUser->IsUserAttached()) {
							const vector<CString>& vsBuffer = pChan->GetBuffer();

							if (vsBuffer.size()) {
								m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + pChan->GetName() + " :Buffer Playback...");

								for (unsigned int a = 0; a < vsBuffer.size(); a++) {
									m_pUser->PutUser(vsBuffer[a]);
								}

								if (!pChan->KeepBuffer()) {
									pChan->ClearBuffer();
								}

								m_pUser->PutUser(":***!znc@znc.com PRIVMSG " + pChan->GetName() + " :Playback Complete.");
							}
						}

						if (!pChan->IsOn()) {
							pChan->SetIsOn(true);
							PutIRC("MODE " + pChan->GetName());

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

					return;	// return so we don't send them the raw twice
				}
			}
		} else { //if (CUtils::wildcmp(":*!*@* * *", sLine.c_str())) {
			CNick Nick(sLine.Token(0).LeftChomp_n());
			CString sCmd = sLine.Token(1);
			CString sRest = sLine.Token(2, true);

			if (sCmd.CaseCmp("NICK") == 0) {
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
				if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
					SetNick(sNewNick);
				} else if (Nick.GetNick().CaseCmp(m_pUser->GetNick()) == 0) {
					KeepNick(true);
				}

				VOIDMODULECALL(OnNick(Nick, sNewNick, vFoundChans));

				if (!bIsVisible) {
					return;
				}
			} else if (sCmd.CaseCmp("QUIT") == 0) {
				CString sMessage = sRest;
				bool bIsVisible = false;

				if (sMessage.Left(1) == ":") {
					sMessage.LeftChomp();
				}

				// :nick!ident@host.com QUIT :message

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

				if (Nick.GetNick().CaseCmp(m_pUser->GetNick()) == 0) {
					KeepNick(true);
				}

				VOIDMODULECALL(OnQuit(Nick, sMessage, vFoundChans));

				if (!bIsVisible) {
					return;
				}
			} else if (sCmd.CaseCmp("JOIN") == 0) {
				CString sChan = sRest.Token(0);
				if (sChan.Left(1) == ":") {
					sChan.LeftChomp();
				}

				// Todo: use nick compare function
				if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
					m_pUser->AddChan(sChan, false);
				}

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->AddNick(Nick.GetNickMask());
					VOIDMODULECALL(OnJoin(Nick.GetNickMask(), *pChan));

					if (pChan->IsDetached()) {
						return;
					}
				}
			} else if (sCmd.CaseCmp("PART") == 0) {
				CString sChan = sRest.Token(0);
				if (sChan.Left(1) == ":") {
					sChan.LeftChomp();
				}

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->RemNick(Nick.GetNick());
					VOIDMODULECALL(OnPart(Nick.GetNickMask(), *pChan));
				}

				// Todo: use nick compare function
				if (Nick.GetNick().CaseCmp(GetNick()) == 0) {
					m_pUser->DelChan(sChan);
				}

				if ((pChan) && (pChan->IsDetached())) {
					return;
				}
			} else if (sCmd.CaseCmp("MODE") == 0) {
				CString sChan = sRest.Token(0);
				CString sModes = sRest.Token(1, true);

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->ModeChange(sModes, Nick.GetNick());

					if (pChan->IsDetached()) {
						return;
					}
				}
			} else if (sCmd.CaseCmp("KICK") == 0) {
				// :opnick!ident@host.com KICK #chan nick :msg
				CString sChan = sRest.Token(0);
				CString sKickedNick = sRest.Token(1);
				CString sMsg = sRest.Token(2, true);
				sMsg.LeftChomp();

				CChan* pChan = m_pUser->FindChan(sChan);

				if (pChan) {
					pChan->RemNick(sKickedNick);
					VOIDMODULECALL(OnKick(Nick.GetNickMask(), sKickedNick, *pChan, sMsg));
				}

				if (GetNick().CaseCmp(sKickedNick) == 0) {
					CString sKey;

					if (pChan) {
						sKey = pChan->GetKey();
						pChan->SetIsOn(false);
					}

					PutIRC("JOIN " + sChan + " " + sKey);
				}

				if ((pChan) && (pChan->IsDetached())) {
					return;
				}
			} else if (sCmd.CaseCmp("NOTICE") == 0) {
				// :nick!ident@host.com NOTICE #chan :Message
				CString sTarget = sRest.Token(0);
				CString sMsg = sRest.Token(1, true);
				sMsg.LeftChomp();

				if (sMsg.WildCmp("\001*\001")) {
					sMsg.LeftChomp();
					sMsg.RightChomp();

					if (sTarget.CaseCmp(GetNick()) == 0) {
						if (OnCTCPReply(Nick, sMsg)) {
							return;
						}
					}

					m_pUser->PutUser(":" + Nick.GetNickMask() + " NOTICE " + sTarget + " :\001" + sMsg + "\001");
					return;
				} else {
					if (sTarget.CaseCmp(GetNick()) == 0) {
						if (OnPrivNotice(Nick, sMsg)) {
							return;
						}
					} else {
						if (OnChanNotice(Nick, sTarget, sMsg)) {
							return;
						}
					}
				}

				if (Nick.GetNick().CaseCmp(m_pUser->GetIRCServer()) == 0) {
					m_pUser->PutUser(":" + Nick.GetNick() + " NOTICE " + sTarget + " :" + sMsg);
				} else {
					m_pUser->PutUser(":" + Nick.GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
				}

				return;
			} else if (sCmd.CaseCmp("TOPIC") == 0) {
				// :nick!ident@host.com TOPIC #chan :This is a topic
				CChan* pChan = m_pUser->FindChan(sLine.Token(2));

				if (pChan) {
					CString sTopic = sLine.Token(3, true);
					sTopic.LeftChomp();
					pChan->SetTopicOwner(Nick.GetNick());
					pChan->SetTopicDate((unsigned long) time(NULL));
					pChan->SetTopic(sTopic);
				}
			} else if (sCmd.CaseCmp("PRIVMSG") == 0) {
				// :nick!ident@host.com PRIVMSG #chan :Message
				CString sTarget = sRest.Token(0);
				CString sMsg = sRest.Token(1, true);

				if (sMsg.Left(1) == ":") {
					sMsg.LeftChomp();
				}

				if (sMsg.WildCmp("\001*\001")) {
					sMsg.LeftChomp();
					sMsg.RightChomp();

					if (sTarget.CaseCmp(GetNick()) == 0) {
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
					if (sTarget.CaseCmp(GetNick()) == 0) {
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
			}
		}
	}

	m_pUser->PutUser(sLine);
}

void CIRCSock::KeepNick(bool bForce) {
	const CString& sConfNick = m_pUser->GetNick();
	CString sAwayNick = CNick::Concat(sConfNick, m_pUser->GetAwaySuffix(), GetMaxNickLen());

	if (m_bAuthed && m_bKeepNick && (!IsOrigNickPending() || bForce) && m_pUser->GetKeepNick() && GetNick().CaseCmp(sConfNick) != 0 && GetNick().CaseCmp(sAwayNick) != 0) {
		PutIRC("NICK " + sConfNick);
		SetOrigNickPending(true);
	}
}

bool CIRCSock::OnCTCPReply(CNick& Nick, CString& sMessage) {
	MODULECALL(OnCTCPReply(Nick, sMessage));

	return false;
}

bool CIRCSock::OnPrivCTCP(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivCTCP(Nick, sMessage));

	if (strncasecmp(sMessage.c_str(), "DCC ", 4) == 0 && m_pUser && m_pUser->BounceDCCs()) {
		// DCC CHAT chat 2453612361 44592
		CString sType = sMessage.Token(1);
		CString sFile = sMessage.Token(2);
		unsigned long uLongIP = strtoul(sMessage.Token(3).c_str(), NULL, 10);
		unsigned short uPort = strtoul(sMessage.Token(4).c_str(), NULL, 10);
		unsigned long uFileSize = strtoul(sMessage.Token(5).c_str(), NULL, 10);

		if (sType.CaseCmp("CHAT") == 0) {
			if (m_pUser->IsUserAttached()) {
				CNick FromNick(Nick.GetNickMask());
				unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, "", true, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));

				if (uBNCPort) {
					m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + GetNick() + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(GetLocalIP())) + " " + CString(uBNCPort) + "\001");
				}
			}
		} else if (sType.CaseCmp("SEND") == 0) {
			// DCC SEND readme.txt 403120438 5550 1104
			unsigned short uBNCPort = CDCCBounce::DCCRequest(Nick.GetNick(), uLongIP, uPort, sFile, false, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));
			if (uBNCPort) {
				m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + GetNick() + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(GetLocalIP())) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
			}
		} else if (sType.CaseCmp("RESUME") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			CDCCBounce* pSock = (CDCCBounce*) CZNC::Get().GetManager().FindSockByLocalPort(atoi(sMessage.Token(3).c_str()));

			if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
				m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sMessage.Token(4) + "\001");
			}
		} else if (sType.CaseCmp("ACCEPT") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			CSockManager& Manager = CZNC::Get().GetManager();

			for (unsigned int a = 0; a < Manager.size(); a++) {
				CDCCBounce* pSock = (CDCCBounce*) Manager[a];

				if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
					if (pSock->GetUserPort() == atoi(sMessage.Token(3).c_str())) {
						m_pUser->PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			}
		}

		return true;
	} else {
		const MCString& mssCTCPReplies = m_pUser->GetCTCPReplies();
		MCString::const_iterator it = mssCTCPReplies.find(sMessage.AsUpper());
		CString sQuery = sMessage.Token(0).AsUpper();
		CString sReply;

		if (it != mssCTCPReplies.end()) {
			sReply = it->second;
		}

		if (sReply.empty() && sQuery == "VERSION" && !m_pUser->IsUserAttached()) {
			sReply = "ZNC by prozac - http://znc.sourceforge.net";
		}

		if (!sReply.empty()) {
			PutIRC("NOTICE " + Nick.GetNick() + " :\001" + sQuery + " " + sReply + "\001");
			return true;
		}
	}

	return false;
}

bool CIRCSock::OnPrivNotice(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivNotice(Nick, sMessage));

	if (!m_pUser->IsUserAttached()) {
		// If the user is detached, add to the buffer
		m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " NOTICE ", " :" + sMessage);
	}

	return false;
}

bool CIRCSock::OnPrivMsg(CNick& Nick, CString& sMessage) {
	MODULECALL(OnPrivMsg(Nick, sMessage));

	if (!m_pUser->IsUserAttached()) {
		// If the user is detached, add to the buffer
		m_pUser->AddQueryBuffer(":" + Nick.GetNickMask() + " PRIVMSG ", " :" + sMessage);
	}

	return false;
}

bool CIRCSock::OnChanCTCP(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		// Record a /me
		if (sMessage.Token(0).CaseCmp("ACTION") == 0 && (pChan->KeepBuffer() || !m_pUser->IsUserAttached())) {
			pChan->AddBuffer(":" + Nick.GetNickMask() + " PRIVMSG " + sChan + " :\001" + sMessage + "\001");
		} else {
			MODULECALL(OnChanCTCP(Nick, *pChan, sMessage));
		}
	}

	return (pChan && pChan->IsDetached());
}

bool CIRCSock::OnChanNotice(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		MODULECALL(OnChanNotice(Nick, *pChan, sMessage));

		if ((pChan->KeepBuffer()) || (!m_pUser->IsUserAttached())) {
			pChan->AddBuffer(":" + Nick.GetNickMask() + " NOTICE " + sChan + " :" + sMessage);
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

bool CIRCSock::OnChanMsg(CNick& Nick, const CString& sChan, CString& sMessage) {
	CChan* pChan = m_pUser->FindChan(sChan);
	if (pChan) {
		MODULECALL(OnChanMsg(Nick, *pChan, sMessage));

		if (pChan->KeepBuffer() || !m_pUser->IsUserAttached()) {
			pChan->AddBuffer(":" + Nick.GetNickMask() + " PRIVMSG " + sChan + " :" + sMessage);
		}
	}

	return ((pChan) && (pChan->IsDetached()));
}

void CIRCSock::PutIRC(const CString& sLine) {
	DEBUG_ONLY(cout << "(" << m_pUser->GetUserName() << ") ZNC -> IRC [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CIRCSock::SetNick(const CString& sNick) {
	m_Nick.SetNick(sNick);
	m_pUser->SetIRCNick(m_Nick);
}

void CIRCSock::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected()" << endl);
	m_pUser->IRCConnected(this);

	if (!m_sPass.empty()) {
		PutIRC("PASS " + m_sPass);
	}

	PutIRC("NICK " + m_pUser->GetNick());
	PutIRC("USER " + m_pUser->GetIdent() + " \"" + m_pUser->GetIdent() + "\" \"" + m_pUser->GetIdent() + "\" :" + m_pUser->GetRealName());
}

void CIRCSock::Disconnected() {
	VOIDMODULECALL(OnIRCDisconnected());

	DEBUG_ONLY(cout << GetSockName() << " == Disconnected()" << endl);
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("Disconnected from IRC.  Reconnecting...");
	}
}

void CIRCSock::SockError(int iErrno) {
	DEBUG_ONLY(cout << GetSockName() << " == SockError(" << iErrno << ")" << endl);
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("Disconnected from IRC.  Reconnecting...");
	}
}

void CIRCSock::Timeout() {
	DEBUG_ONLY(cout << GetSockName() << " == Timeout()" << endl);
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("IRC connection timed out.  Reconnecting...");
	}
}

void CIRCSock::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
	if (!m_pUser->IsBeingDeleted()) {
		m_pUser->PutStatus("Connection Refused.  Reconnecting...");
	}
}

void CIRCSock::ParseISupport(const CString& sLine) {
	unsigned int i = 0;
	CString sArg = sLine.Token(i++);

	while (!sArg.empty()) {
		CString sName = sArg.Token(0, false, "=");
		CString sValue = sArg.Token(1, true, "=");

		if (sName.CaseCmp("PREFIX") == 0) {
			CString sPrefixes = sValue.Token(1, false, ")");
			CString sPermModes = sValue.Token(0, false, ")");
			sPermModes.TrimLeft("(");

			if (!sPrefixes.empty() && sPermModes.size() == sPrefixes.size()) {
				m_sPerms = sPrefixes;
				m_sPermModes = sPermModes;
			}
		} else if (sName.CaseCmp("CHANTYPES") == 0) {
			m_pUser->SetChanPrefixes(sValue);
		} else if (sName.CaseCmp("NICKLEN") == 0) {
			unsigned int uMax = sValue.ToUInt();

			if (uMax) {
				m_uMaxNickLen = uMax;
			}
		} else if (sName.CaseCmp("CHANMODES") == 0) {
			if (!sValue.empty()) {
				m_mueChanModes.clear();

				for (unsigned int a = 0; a < 4; a++) {
					CString sModes = sValue.Token(a, false, ",");

					for (unsigned int b = 0; b < sModes.size(); b++) {
						m_mueChanModes[sModes[b]] = (EChanModeArgs) a;
					}
				}
			}
		}

		sArg = sLine.Token(i++);
	}
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

