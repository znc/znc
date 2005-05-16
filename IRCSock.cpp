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
	m_MotdBuffer.SetLineCount(200);		// This should be more than enough motd lines
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
		vChans[a]->SetIsOn(false);
	}

	if (m_pUserSock) {
		m_pUserSock->IRCDisconnected();
		m_pUserSock = NULL;
	}

	for (map<CString, CChan*>::iterator a = m_msChans.begin(); a != m_msChans.end(); a++) {
		delete a->second;
	}

	if (!m_bISpoofReleased) {
		m_pZNC->ReleaseISpoof();
	}

	PutServ("QUIT :" + m_pUser->GetQuitMsg());
	m_msChans.clear();
}

void CIRCSock::ReadLine(const CString& sData) {
	CString sLine = sData;

	while ((sLine.Right(1) == "\r") || (sLine.Right(1) == "\n")) {
		sLine.RightChomp();
	}

	DEBUG_ONLY(cout << GetSockName() << " <- [" << sLine << "]" << endl);

#ifdef _MODULES
	if (m_pUser->GetModules().OnRaw(sLine)) {
		return;
	}
#endif

	if (strncasecmp(sLine.c_str(), "PING ", 5) == 0) {
		PutServ("PONG " + sLine.substr(5));
	} else if (sLine.WildCmp(":* * *")) { //"^:(\\S+) (\\d\\d\\d) (.*?) (.*)$", vCap)) {
		CString sCmd = sLine.Token(1);

		if ((sCmd.length() == 3) && (isdigit(sCmd[0])) && (isdigit(sCmd[1])) && (isdigit(sCmd[2]))) {
			CString sServer = sLine.Token(0); sServer.LeftChomp();
			unsigned int uRaw = strtoul(sCmd.c_str(), NULL, 10);
			CString sNick = sLine.Token(2);
			CString sRest = sLine.Token(3, true);

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
						CString sClientNick = m_pUserSock->GetNick();
						if (sClientNick.CaseCmp(sNick) != 0) {
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
					m_RawBuffer.AddLine(":" + sServer + " " + sCmd + " ", " " + sRest);
					break;
				case 375:	// begin motd
					m_MotdBuffer.Clear();
				case 372:	// motd
				case 376:	// end motd
					m_MotdBuffer.AddLine(":" + sServer + " " + sCmd + " ", " " + sRest);
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
					unsigned int uMax = GetMaxNickLen();
					CString sBadNick = sRest.Token(0);
					CString sConfNick = m_pUser->GetNick().Left(uMax);

					if (sNick == "*") {
						CString sAltNick = m_pUser->GetAltNick();

						if (sBadNick.CaseCmp(sConfNick) == 0) {
							if ((!sAltNick.empty()) && (sConfNick.CaseCmp(sAltNick) != 0)) {
								PutServ("NICK " + sAltNick);
							} else {
								PutServ("NICK " + sConfNick.Left(uMax -1) + "-");
							}
						} else if (sBadNick.CaseCmp(sAltNick) == 0) {
							PutServ("NICK " + sConfNick.Left(uMax -1) + "-");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "-")) == 0) {
							PutServ("NICK " + sConfNick.Left(uMax -1) + "|");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "|")) == 0) {
							PutServ("NICK " + sConfNick.Left(uMax -1) + "^");
						} else if (sBadNick.CaseCmp(CString(sConfNick.Left(uMax -1) + "^")) == 0) {
							PutServ("NICK " + sConfNick.Left(uMax -1) + "a");
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
							PutServ(sSend);
						}

						return;
					} else {
						// :irc.server.net 433 mynick badnick :Nickname is already in use.
						if ((m_bKeepNick) && (m_pUser->KeepNick())) {
							if (sBadNick.CaseCmp(sConfNick) == 0) {
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
					PutUser(sLine);	// First send them the raw

					// :irc.server.com 366 nick #chan :End of /NAMES list.
					CChan* pChan = m_pUser->FindChan(sRest.Token(0));

					if (pChan) {
						if (IsUserAttached()) {
							const vector<CString>& vsBuffer = pChan->GetBuffer();

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
								CString sModes = pChan->GetDefaultModes();

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
			CString sNickMask = sLine.Token(0);
			sNickMask.LeftChomp();

			CString sNick = sNickMask.Token(0, false, '!');
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

					if (pChan->ChangeNick(sNick, sNewNick)) {
						vFoundChans.push_back(pChan);

						if (!pChan->IsDetached()) {
							bIsVisible = true;
						}
					}
				}

				if (sNick.CaseCmp(GetNick()) == 0) {
					SetNick(sNewNick);
					if (sNick.CaseCmp(m_pUser->GetNick()) == 0) {
						// If the user changes his nick away from the config nick, we shut off keepnick for this session
						m_bKeepNick = false;
					}
				} else if (sNick.CaseCmp(m_pUser->GetNick()) == 0) {
					KeepNick();
				}
#ifdef _MODULES
				m_pUser->GetModules().OnNick(sNickMask, sNewNick, vFoundChans);
#endif

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

				if (Nick.GetNick().CaseCmp(m_pUser->GetNick()) == 0) {
					KeepNick();
				}

#ifdef _MODULES
				m_pUser->GetModules().OnQuit(Nick, sMessage, vFoundChans);
#endif

				if (!bIsVisible) {
					return;
				}
			} else if (sCmd.CaseCmp("JOIN") == 0) {
				CString sChan = sRest.Token(0);
				if (sChan.Left(1) == ":") {
					sChan.LeftChomp();
				}

				if (sNick.CaseCmp(GetNick()) == 0) {
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
			} else if (sCmd.CaseCmp("PART") == 0) {
				CString sChan = sRest.Token(0);
				if (sChan.Left(1) == ":") {
					sChan.LeftChomp();
				}

				CChan* pChan = m_pUser->FindChan(sChan);
				if (pChan) {
					pChan->RemNick(sNick);
#ifdef _MODULES
					m_pUser->GetModules().OnPart(sNickMask, *pChan);
#endif
				}

				if (sNick.CaseCmp(GetNick()) == 0) {
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
					pChan->ModeChange(sModes, sNick);

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
#ifdef _MODULES
					m_pUser->GetModules().OnKick(sNickMask, sKickedNick, *pChan, sMsg);
#endif
				}

				if (GetNick().CaseCmp(sKickedNick) == 0) {
					CString sKey;

					if (pChan) {
						sKey = pChan->GetKey();
						pChan->SetIsOn(false);
					}

					PutServ("JOIN " + sChan + " " + sKey);
				}

				if ((pChan) && (pChan->IsDetached())) {
					return;
				}
			} else if (sCmd.CaseCmp("NOTICE") == 0) {
				// :nick!ident@host.com NOTICE #chan :Message

				CNick Nick(sNickMask);

				CString sTarget = sRest.Token(0);
				CString sMsg = sRest.Token(1, true);
				sMsg.LeftChomp();

				if (sMsg.WildCmp("\001*\001")) {
					sMsg.LeftChomp();
					sMsg.RightChomp();

					if (sTarget.CaseCmp(GetNick()) == 0) {
						if (OnCTCPReply(sNickMask, sMsg)) {
							return;
						}
					}

					PutUser(":" + sNickMask + " NOTICE " + sTarget + " :\001" + sMsg + "\001");
					return;
				} else {
					if (sTarget.CaseCmp(GetNick()) == 0) {
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
			} else if (sCmd.CaseCmp("TOPIC") == 0) {
				// :nick!ident@host.com TOPIC #chan :This is a topic
				CChan* pChan = m_pUser->FindChan(sLine.Token(2));

				if (pChan) {
					CNick Nick(sNickMask);
					CString sTopic = sLine.Token(3, true);
					sTopic.LeftChomp();
					pChan->SetTopicOwner(Nick.GetNick());
					pChan->SetTopicDate((unsigned long) time(NULL));	// @todo use local time
					pChan->SetTopic(sTopic);
				}
			} else if (sCmd.CaseCmp("PRIVMSG") == 0) {
				// :nick!ident@host.com PRIVMSG #chan :Message

				CNick Nick(sNickMask);
				CString sTarget = sRest.Token(0);
				CString sMsg = sRest.Token(1, true);

				if (sMsg.Left(1) == ":") {
					sMsg.LeftChomp();
				}

				if (sMsg.WildCmp("\001*\001")) {
					sMsg.LeftChomp();
					sMsg.RightChomp();

					if (sTarget.CaseCmp(GetNick()) == 0) {
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
					if (sTarget.CaseCmp(GetNick()) == 0) {
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
	const CString& sConfNick = m_pUser->GetNick();

	if ((m_bAuthed) && (m_bKeepNick) && (m_pUser->KeepNick()) && (GetNick().CaseCmp(sConfNick) != 0)) {
		PutServ("NICK " + sConfNick);
	}
}

bool CIRCSock::OnCTCPReply(const CString& sNickMask, CString& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnCTCPReply(sNickMask, sMessage)) {
		return true;
	}
#endif

	return false;
}

bool CIRCSock::OnPrivCTCP(const CString& sNickMask, CString& sMessage) {
#ifdef _MODULES
	if (m_pUser->GetModules().OnPrivCTCP(sNickMask, sMessage)) {
		return true;
	}
#endif

	if (sMessage.CaseCmp("VERSION") == 0) {
		if (!IsUserAttached()) {
			CString sVersionReply = m_pUser->GetVersionReply();
			PutServ("NOTICE " + CNick(sNickMask).GetNick() + " :\001VERSION " + sVersionReply + "\001");
		}
	} else if (strncasecmp(sMessage.c_str(), "DCC ", 4) == 0) {
		// DCC CHAT chat 2453612361 44592
		CString sType = sMessage.Token(1);
		CString sFile = sMessage.Token(2);
		unsigned long uLongIP = strtoul(sMessage.Token(3).c_str(), NULL, 10);
		unsigned short uPort = strtoul(sMessage.Token(4).c_str(), NULL, 10);
		unsigned long uFileSize = strtoul(sMessage.Token(5).c_str(), NULL, 10);

		if (sType.CaseCmp("CHAT") == 0) {
			if (m_pUserSock) {
				CNick FromNick(sNickMask);
				unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, "", true, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));

				if (uBNCPort) {
					PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC CHAT chat " + CString::ToString(CUtils::GetLongIP(GetLocalIP())) + " " + CString::ToString(uBNCPort) + "\001");
				}
			}
		} else if (sType.CaseCmp("SEND") == 0) {
			// DCC SEND readme.txt 403120438 5550 1104
			CNick FromNick(sNickMask);

			unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, sFile, false, m_pUser, GetLocalIP(), CUtils::GetIP(uLongIP));
			if (uBNCPort) {
				PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC SEND " + sFile + " " + CString::ToString(CUtils::GetLongIP(GetLocalIP())) + " " + CString::ToString(uBNCPort) + " " + CString::ToString(uFileSize) + "\001");
			}
		} else if (sType.CaseCmp("RESUME") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			CDCCBounce* pSock = (CDCCBounce*) m_pZNC->GetManager().FindSockByLocalPort(atoi(sMessage.Token(3).c_str()));

			if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
				PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString::ToString(pSock->GetUserPort()) + " " + sMessage.Token(4) + "\001");
			}
		} else if (sType.CaseCmp("ACCEPT") == 0) {
			// Need to lookup the connection by port, filter the port, and forward to the user
			TSocketManager<Csock>& Manager = m_pZNC->GetManager();

			for (unsigned int a = 0; a < Manager.size(); a++) {
				CDCCBounce* pSock = (CDCCBounce*) Manager[a];

				if ((pSock) && (strncasecmp(pSock->GetSockName().c_str(), "DCC::", 5) == 0)) {
					if (pSock->GetUserPort() == atoi(sMessage.Token(3).c_str())) {
						PutUser(":" + sNickMask + " PRIVMSG " + GetNick() + " :\001DCC " + sType + " " + sFile + " " + CString::ToString(pSock->GetLocalPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			}
		}

		return true;
	}

	return false;
}

bool CIRCSock::OnPrivNotice(const CString& sNickMask, CString& sMessage) {
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

bool CIRCSock::OnPrivMsg(const CString& sNickMask, CString& sMessage) {
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

bool CIRCSock::OnChanCTCP(const CString& sNickMask, const CString& sChan, CString& sMessage) {
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

bool CIRCSock::OnChanNotice(const CString& sNickMask, const CString& sChan, CString& sMessage) {
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

bool CIRCSock::OnChanMsg(const CString& sNickMask, const CString& sChan, CString& sMessage) {
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
		CString sLine;

		while (m_RawBuffer.GetLine(GetNick(), sLine, uIdx++)) {
			PutUser(sLine);
		}
	}

	// Send the cached MOTD
	if (m_MotdBuffer.IsEmpty()) {
		PutServ("MOTD");
	} else {
		unsigned int uIdx = 0;
		CString sLine;

		while (m_MotdBuffer.GetLine(GetNick(), sLine, uIdx++)) {
			PutUser(sLine);
		}
	}

	const vector<CChan*>& vChans = m_pUser->GetChans();
	for (unsigned int a = 0; a < vChans.size(); a++) {
		if ((vChans[a]->IsOn()) && (!vChans[a]->IsDetached())) {
			vChans[a]->JoinUser(true);
		}
	}

	CString sBufLine;
	while (m_QueryBuffer.GetNextLine(GetNick(), sBufLine)) {
		PutUser(sBufLine);
	}
}

void CIRCSock::UserDisconnected() {
	m_pUserSock = NULL;
}

void CIRCSock::PutServ(const CString& sLine) {
	DEBUG_ONLY(cout << GetSockName() << " -> [" << sLine << "]" << endl);
	Write(sLine + "\r\n");
}

void CIRCSock::PutUser(const CString& sLine) {
	if (m_pUserSock) {
		m_pUserSock->PutServ(sLine);
	}
}

void CIRCSock::PutStatus(const CString& sLine) {
	if (m_pUserSock) {
		m_pUserSock->PutStatus(sLine);
	}
}

void CIRCSock::SetNick(const CString& sNick) {
	m_Nick.SetNick(sNick);
	m_pUser->SetIRCNick(m_Nick);

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

void CIRCSock::ParseISupport(const CString& sLine) {
	unsigned int i = 0;
	CString sArg = sLine.Token(i++);

	while (!sArg.empty()) {
		CString sName = sArg.Token(0, false, '=');
		CString sValue = sArg.Token(1, true, '=');

		if (sName.CaseCmp("PREFIX") == 0) {
			CString sPrefixes = sValue.Token(1, false, ')');
			CString sPermModes = sValue.Token(0, false, ')');
			sPermModes.LeftTrim("(");

			if (!sPrefixes.empty() && sPermModes.size() == sPrefixes.size()) {
				m_sPerms = sPrefixes;
				m_sPermModes = sPermModes;
			}
		} else if (sName.CaseCmp("CHANMODES") == 0) {
			unsigned int uMax = sValue.ToUInt();

			if (uMax) {
				m_uMaxNickLen = uMax;
			}
		} else if (sName.CaseCmp("CHANMODES") == 0) {
			if (!sValue.empty()) {
				m_mueChanModes.clear();

				for (unsigned int a = 0; a < 4; a++) {
					CString sModes = sValue.Token(a, false, ',');

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

