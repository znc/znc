/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Query.h>

using std::set;
using std::map;
using std::vector;

#define CALLMOD(MOD, CLIENT, USER, NETWORK, FUNC) {  \
	CModule *pModule = nullptr;  \
	if (NETWORK && (pModule = (NETWORK)->GetModules().FindModule(MOD))) {  \
		try {  \
			pModule->SetClient(CLIENT);  \
			pModule->FUNC;  \
			pModule->SetClient(nullptr);  \
		} catch (const CModule::EModException& e) {  \
			if (e == CModule::UNLOAD) {  \
				(NETWORK)->GetModules().UnloadModule(MOD);  \
			}  \
		}  \
	} else if ((pModule = (USER)->GetModules().FindModule(MOD))) {  \
		try {  \
			pModule->SetClient(CLIENT);  \
			pModule->SetNetwork(NETWORK);  \
			pModule->FUNC;  \
			pModule->SetClient(nullptr);  \
			pModule->SetNetwork(nullptr);  \
		} catch (const CModule::EModException& e) {  \
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
			pModule->SetClient(nullptr);  \
			pModule->SetNetwork(nullptr);  \
			pModule->SetUser(nullptr);  \
		} catch (const CModule::EModException& e) {  \
			if (e == CModule::UNLOAD) {  \
					CZNC::Get().GetModules().UnloadModule(MOD);  \
			}  \
		}  \
	} else {  \
		PutStatus("No such module [" + MOD + "]");  \
	}  \
}

CClient::~CClient() {
	if (m_spAuth) {
		CClientAuth* pAuth = (CClientAuth*) &(*m_spAuth);
		pAuth->Invalidate();
	}
	if (m_pUser != nullptr) {
		m_pUser->AddBytesRead(GetBytesRead());
		m_pUser->AddBytesWritten(GetBytesWritten());
	}
}

void CClient::SendRequiredPasswordNotice() {
	PutClient(":irc.znc.in 464 " + GetNick() + " :Password required");
	PutClient(":irc.znc.in NOTICE AUTH :*** "
			  "You need to send your password. "
			  "Configure your client to send a server password.");
	PutClient(":irc.znc.in NOTICE AUTH :*** "
			  "To connect now, you can use /quote PASS <username>:<password>, "
			  "or /quote PASS <username>/<network>:<password> to connect to a specific network.");
}

void CClient::ReadLine(const CString& sData) {
	CString sLine = sData;

	sLine.TrimRight("\n\r");

	DEBUG("(" << GetFullName() << ") CLI -> ZNC [" << sLine << "]");

	MCString mssTags;
	if (sLine.StartsWith("@")) {
		mssTags = CUtils::GetMessageTags(sLine);
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
	if (sCommand.StartsWith(":")) {
		// Evil client! Sending a nickmask prefix on client's command
		// is bad, bad, bad, bad, bad, bad, bad, bad, BAD, B A D!
		sLine = sLine.Token(1, true);
		sCommand = sLine.Token(0);
	}

	if (!IsAttached()) { // The following commands happen before authentication with ZNC
		if (sCommand.Equals("PASS")) {
			m_bGotPass = true;

			CString sAuthLine = sLine.Token(1, true).TrimPrefix_n();
			ParsePass(sAuthLine);

			AuthUser();
			return;  // Don't forward this msg.  ZNC has already registered us.
		} else if (sCommand.Equals("NICK")) {
			CString sNick = sLine.Token(1).TrimPrefix_n();

			m_sNick = sNick;
			m_bGotNick = true;

			AuthUser();
			return;  // Don't forward this msg.  ZNC will handle nick changes until auth is complete
		} else if (sCommand.Equals("USER")) {
			CString sAuthLine = sLine.Token(1);

			if (m_sUser.empty() && !sAuthLine.empty()) {
				ParseUser(sAuthLine);
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
		CString sMsg = sLine.Token(1, true).TrimPrefix_n();
		NETWORKMODULECALL(OnUserQuit(sMsg), m_pUser, m_pNetwork, this, &bReturn);
		if (bReturn) return;
		Close(Csock::CLT_AFTERWRITE); // Treat a client quit as a detach
		return;                       // Don't forward this msg.  We don't want the client getting us disconnected.
	} else if (sCommand.Equals("PROTOCTL")) {
		VCString vsTokens;
		sLine.Token(1, true).Split(" ", vsTokens, false);

		for (const CString& sToken : vsTokens) {
			if (sToken == "NAMESX") {
				m_bNamesx = true;
			} else if (sToken == "UHNAMES") {
				m_bUHNames = true;
			}
		}
		return;  // If the server understands it, we already enabled namesx / uhnames
	} else if (sCommand.Equals("NOTICE")) {
		CString sTargets = sLine.Token(1).TrimPrefix_n();
		CString sMsg = sLine.Token(2, true).TrimPrefix_n();

		VCString vTargets;
		sTargets.Split(",", vTargets, false);

		for (CString& sTarget : vTargets) {
			if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
				if (!sTarget.Equals("status")) {
					CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModNotice(sMsg));
				}
				continue;
			}

			bool bContinue = false;
			if (sMsg.WildCmp("\001*\001")) {
				CString sCTCP = sMsg;
				sCTCP.LeftChomp();
				sCTCP.RightChomp();

				if (sCTCP.Token(0) == "VERSION") {
					sCTCP += " via " + CZNC::GetTag(false);
				}

				NETWORKMODULECALL(OnUserCTCPReply(sTarget, sCTCP), m_pUser, m_pNetwork, this, &bContinue);
				if (bContinue) continue;

				sMsg = "\001" + sCTCP + "\001";
			} else {
				NETWORKMODULECALL(OnUserNotice(sTarget, sMsg), m_pUser, m_pNetwork, this, &bContinue);
				if (bContinue) continue;
			}

			if (!GetIRCSock()) {
				// Some lagmeters do a NOTICE to their own nick, ignore those.
				if (!sTarget.Equals(m_sNick))
					PutStatus("Your notice to [" + sTarget + "] got lost, "
							"you are not connected to IRC!");
				continue;
			}

			if (m_pNetwork) {
				CChan* pChan = m_pNetwork->FindChan(sTarget);

				if ((pChan) && (!pChan->AutoClearChanBuffer())) {
					pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " NOTICE " + _NAMEDFMT(sTarget) + " :{text}", sMsg, nullptr, mssTags);
				}

				// Relay to the rest of the clients that may be connected to this user
				const vector<CClient*>& vClients = GetClients();

				for (CClient* pClient : vClients) {
					if (pClient->HasEchoMessage() || (pClient != this && (m_pNetwork->IsChan(sTarget) || pClient->HasSelfMessage()))) {
						pClient->PutClient(":" + GetNickMask() + " NOTICE " + sTarget + " :" + sMsg);
					}
				}

				PutIRC("NOTICE " + sTarget + " :" + sMsg);
			}
		}

		return;
	} else if (sCommand.Equals("PRIVMSG")) {
		CString sTargets = sLine.Token(1);
		CString sMsg = sLine.Token(2, true).TrimPrefix_n();

		VCString vTargets;
		sTargets.Split(",", vTargets, false);

		for (CString& sTarget : vTargets) {
			bool bContinue = false;
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
					continue;
				}

				if (m_pNetwork) {
					if (sCTCP.Token(0).Equals("ACTION")) {
						CString sMessage = sCTCP.Token(1, true);
						NETWORKMODULECALL(OnUserAction(sTarget, sMessage), m_pUser, m_pNetwork, this, &bContinue);
						if (bContinue) continue;
						sCTCP = "ACTION " + sMessage;

						if (m_pNetwork->IsChan(sTarget)) {
							CChan* pChan = m_pNetwork->FindChan(sTarget);

							if (pChan && (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline())) {
								pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :\001ACTION {text}\001", sMessage, nullptr, mssTags);
							}
						} else {
							if (!m_pUser->AutoClearQueryBuffer() || !m_pNetwork->IsUserOnline()) {
								CQuery* pQuery = m_pNetwork->AddQuery(sTarget);
								if (pQuery) {
									pQuery->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :\001ACTION {text}\001", sMessage, nullptr, mssTags);
								}
							}
						}

						// Relay to the rest of the clients that may be connected to this user
						const vector<CClient*>& vClients = GetClients();

						for (CClient* pClient : vClients) {
							if (pClient->HasEchoMessage() || (pClient != this && (m_pNetwork->IsChan(sTarget) || pClient->HasSelfMessage()))) {
								pClient->PutClient(":" + GetNickMask() + " PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
							}
						}
					} else {
						NETWORKMODULECALL(OnUserCTCP(sTarget, sCTCP), m_pUser, m_pNetwork, this, &bContinue);
						if (bContinue) continue;
					}

					PutIRC("PRIVMSG " + sTarget + " :\001" + sCTCP + "\001");
				}

				continue;
			}

			if (sTarget.TrimPrefix(m_pUser->GetStatusPrefix())) {
				if (sTarget.Equals("status")) {
					UserCommand(sMsg);
				} else {
					CALLMOD(sTarget, this, m_pUser, m_pNetwork, OnModCommand(sMsg));
				}
				continue;
			}

			NETWORKMODULECALL(OnUserMsg(sTarget, sMsg), m_pUser, m_pNetwork, this, &bContinue);
			if (bContinue) continue;

			if (!GetIRCSock()) {
				// Some lagmeters do a PRIVMSG to their own nick, ignore those.
				if (!sTarget.Equals(m_sNick))
					PutStatus("Your message to [" + sTarget + "] got lost, "
							"you are not connected to IRC!");
				continue;
			}

			if (m_pNetwork) {
				if (m_pNetwork->IsChan(sTarget)) {
					CChan* pChan = m_pNetwork->FindChan(sTarget);

					if ((pChan) && (!pChan->AutoClearChanBuffer() || !m_pNetwork->IsUserOnline())) {
						pChan->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :{text}", sMsg, nullptr, mssTags);
					}
				} else {
					if (!m_pUser->AutoClearQueryBuffer() || !m_pNetwork->IsUserOnline()) {
						CQuery* pQuery = m_pNetwork->AddQuery(sTarget);
						if (pQuery) {
							pQuery->AddBuffer(":" + _NAMEDFMT(GetNickMask()) + " PRIVMSG " + _NAMEDFMT(sTarget) + " :{text}", sMsg, nullptr, mssTags);
						}
					}
				}

				PutIRC("PRIVMSG " + sTarget + " :" + sMsg);

				// Relay to the rest of the clients that may be connected to this user
				const vector<CClient*>& vClients = GetClients();

				for (CClient* pClient : vClients) {
					if (pClient->HasEchoMessage() || (pClient != this && (m_pNetwork->IsChan(sTarget) || pClient->HasSelfMessage()))) {
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
		CString sPatterns = sLine.Token(1, true);

		if (sPatterns.empty()) {
			PutStatusNotice("Usage: /detach <#chans>");
			return;
		}

		VCString vsChans;
		sPatterns.Replace(",", " ");
		sPatterns.Split(" ", vsChans, false, "", "", true, true);

		set<CChan*> sChans;
		for (const CString& sChan : vsChans) {
			vector<CChan*> vChans = m_pNetwork->FindChans(sChan);
			sChans.insert(vChans.begin(), vChans.end());
		}

		unsigned int uDetached = 0;
		for (CChan* pChan : sChans) {
			if (pChan->IsDetached())
				continue;
			uDetached++;
			pChan->DetachUser();
		}

		PutStatusNotice("There were [" + CString(sChans.size()) + "] channels matching [" + sPatterns + "]");
		PutStatusNotice("Detached [" + CString(uDetached) + "] channels");

		return;
	} else if (sCommand.Equals("JOIN")) {
		CString sChans = sLine.Token(1).TrimPrefix_n();
		CString sKeys = sLine.Token(2);

		VCString vsChans;
		sChans.Split(",", vsChans, false);
		sChans.clear();

		VCString vsKeys;
		sKeys.Split(",", vsKeys, true);
		sKeys.clear();

		for (unsigned int a = 0; a < vsChans.size(); a++) {
			CString sChannel = vsChans[a];
			CString sKey = (a < vsKeys.size()) ? vsKeys[a] : "";
			bool bContinue = false;
			NETWORKMODULECALL(OnUserJoin(sChannel, sKey), m_pUser, m_pNetwork, this, &bContinue);
			if (bContinue) continue;

			CChan* pChan = m_pNetwork->FindChan(sChannel);
			if (pChan) {
				if (pChan->IsDetached())
					pChan->AttachUser(this);
				else
					pChan->JoinUser(sKey);
				continue;
			}

			if (!sChannel.empty()) {
				sChans += (sChans.empty()) ? sChannel : CString("," + sChannel);

				if (!vsKeys.empty()) {
					sKeys += (sKeys.empty()) ? sKey : CString("," + sKey);
				}
			}
		}

		if (sChans.empty()) {
			return;
		}

		sLine = "JOIN " + sChans;

		if (!sKeys.empty()) {
			sLine += " " + sKeys;
		}
	} else if (sCommand.Equals("PART")) {
		CString sChans = sLine.Token(1).TrimPrefix_n();
		CString sMessage = sLine.Token(2, true).TrimPrefix_n();

		VCString vsChans;
		sChans.Split(",", vsChans, false);
		sChans.clear();

		for (CString& sChan : vsChans) {
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
			for (const CChan* pChan : vChans) {
				if (!(pChan->IsDetached())) {
					PutClient(":" + m_pNetwork->GetIRCNick().GetNickMask() + " PART " + pChan->GetName());
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

const vector<CClient*>& CClient::GetClients() const {
	if (m_pNetwork) {
		return m_pNetwork->GetClients();
	}

	return m_pUser->GetUserClients();
}

const CIRCSock* CClient::GetIRCSock() const {
	if (m_pNetwork) {
		return m_pNetwork->GetIRCSock();
	}

	return nullptr;
}

CIRCSock* CClient::GetIRCSock() {
	if (m_pNetwork) {
		return m_pNetwork->GetIRCSock();
	}

	return nullptr;
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

	for (const CString& sLine : vsMotd) {
		if (m_pNetwork) {
			PutStatusNotice(m_pNetwork->ExpandString(sLine));
		} else {
			PutStatusNotice(m_pUser->ExpandString(sLine));
		}
	}

	return true;
}

void CClient::AuthUser() {
	if (!m_bGotNick || !m_bGotUser || !m_bGotPass || m_bInCap || IsAttached())
		return;

	m_spAuth = std::make_shared<CClientAuth>(this, m_sUser, m_sPass);

	CZNC::Get().AuthUser(m_spAuth);
}

CClientAuth::CClientAuth(CClient* pClient, const CString& sUsername, const CString& sPassword)
		: CAuthBase(sUsername, sPassword, pClient), m_pClient(pClient) {
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
	m_pSock = nullptr;
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
	SetTimeout(CIRCNetwork::NO_TRAFFIC_TIMEOUT, TMO_READ);

	SetSockName("USR::" + m_pUser->GetUserName());
	SetEncoding(m_pUser->GetClientEncoding());

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
	SetNetwork(nullptr, true, false);

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

CString CClient::GetFullName() const {
	if (!m_pUser)
		return GetRemoteIP();
	CString sFullName = m_pUser->GetUserName();
	if (!m_sIdentifier.empty())
		sFullName += "@" + m_sIdentifier;
	if (m_pNetwork)
		sFullName += "/" + m_pNetwork->GetName();
	return sFullName;
}

void CClient::PutClient(const CString& sLine) {
	bool bReturn = false;
	CString sCopy = sLine;
	NETWORKMODULECALL(OnSendToClient(sCopy, *this), m_pUser, m_pNetwork, this, &bReturn);
	if (bReturn) return;
	DEBUG("(" << GetFullName() << ") ZNC -> CLI [" << sCopy << "]");
	Write(sCopy + "\r\n");
}

void CClient::PutClient(const CMessage& Message)
{
	CString sLine = Message.ToString(CMessage::ExcludeTags);

	// TODO: introduce a module hook that gives control over the tags that are sent
	MCString mssTags;

	if (HasServerTime()) {
		CString sServerTime = Message.GetTag("time");
		if (!sServerTime.empty()) {
			mssTags["time"] = sServerTime;
		} else {
			mssTags["time"] = CUtils::FormatServerTime(Message.GetTime());
		}
	}

	if (HasBatch()) {
		CString sBatch = Message.GetTag("batch");
		if (!sBatch.empty()) {
			mssTags["batch"] = sBatch;
		}
	}

	if (!mssTags.empty()) {
		CUtils::SetMessageTags(sLine, mssTags);
	}

	PutClient(sLine);
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
	Write(":" + m_pUser->GetStatusPrefix() + sModule + ".znc.in NOTICE * :" + sLine + "\r\n");
}

void CClient::PutModule(const CString& sModule, const CString& sLine) {
	if (!m_pUser) {
		return;
	}

	DEBUG("(" << GetFullName() << ") ZNC -> CLI [:" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " << GetNick() << " :" << sLine << "]");

	VCString vsLines;
	sLine.Split("\n", vsLines);
	for (const CString& s : vsLines) {
		Write(":" + m_pUser->GetStatusPrefix() + ((sModule.empty()) ? "status" : sModule) + "!znc@znc.in PRIVMSG " + GetNick() + " :" + s + "\r\n");
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

	return GetNick() + "!" + (m_pNetwork ? m_pNetwork->GetIdent() : m_pUser->GetIdent()) + "@" + sHost;
}

bool CClient::IsValidIdentifier(const CString& sIdentifier) {
	// ^[-\w]+$

	if (sIdentifier.empty()) {
		return false;
	}

	const char *p = sIdentifier.c_str();
	while (*p) {
		if (*p != '_' && *p != '-' && !isalnum(*p)) {
			return false;
		}

		p++;
	}

	return true;
}

void CClient::RespondCap(const CString& sResponse)
{
	PutClient(":irc.znc.in CAP " + GetNick() + " " + sResponse);
}

void CClient::HandleCap(const CString& sLine)
{
	// This is not exactly correct, but this is protection from "CAP :END"
	CString sSubCmd = sLine.Token(1).TrimPrefix_n(":");

	if (sSubCmd.Equals("LS")) {
		SCString ssOfferCaps;
		for (const auto& it : m_mCoreCaps) {
			bool bServerDependent = std::get<0>(it.second);
			if (!bServerDependent || m_ssServerDependentCaps.count(it.first) > 0)
				ssOfferCaps.insert(it.first);
		}
		GLOBALMODULECALL(OnClientCapLs(this, ssOfferCaps), NOTHING);
		CString sRes = CString(" ").Join(ssOfferCaps.begin(), ssOfferCaps.end());
		RespondCap("LS :" + sRes);
		m_bInCap = true;
		if (sLine.Token(2).ToInt() >= 302) {
			m_bCapNotify = true;
		}
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
		sLine.Token(2, true).TrimPrefix_n(":").Split(" ", vsTokens, false);

		for (const CString& sToken : vsTokens) {
			bool bVal = true;
			CString sCap = sToken;
			if (sCap.TrimPrefix("-"))
				bVal = false;

			bool bAccepted = false;
			const auto& it = m_mCoreCaps.find(sCap);
			if (m_mCoreCaps.end() != it) {
				bool bServerDependent = std::get<0>(it->second);
				bAccepted = !bServerDependent || m_ssServerDependentCaps.count(sCap) > 0;
			}
			GLOBALMODULECALL(IsClientCapSupported(this, sCap, bVal), &bAccepted);

			if (!bAccepted) {
				// Some unsupported capability is requested
				RespondCap("NAK :" + sLine.Token(2, true).TrimPrefix_n(":"));
				return;
			}
		}

		// All is fine, we support what was requested
		for (const CString& sToken : vsTokens) {
			bool bVal = true;
			CString sCap = sToken;
			if (sCap.TrimPrefix("-"))
				bVal = false;

			auto handler_it = m_mCoreCaps.find(sCap);
			if (m_mCoreCaps.end() != handler_it) {
				const auto& handler = std::get<1>(handler_it->second);
				handler(bVal);
			}
			GLOBALMODULECALL(OnClientCapRequest(this, sCap, bVal), NOTHING);

			if (bVal) {
				m_ssAcceptedCaps.insert(sCap);
			} else {
				m_ssAcceptedCaps.erase(sCap);
			}
		}

		RespondCap("ACK :" + sLine.Token(2, true).TrimPrefix_n(":"));
	} else if (sSubCmd.Equals("LIST")) {
		CString sList = CString(" ").Join(m_ssAcceptedCaps.begin(), m_ssAcceptedCaps.end());
		RespondCap("LIST :" + sList);
	} else {
		PutClient(":irc.znc.in 410 " + GetNick() + " " + sSubCmd + " :Invalid CAP subcommand");
	}
}

void CClient::ParsePass(const CString& sAuthLine) {
	// [user[@identifier][/network]:]password

	const size_t uColon = sAuthLine.find(":");
	if (uColon != CString::npos) {
		m_sPass = sAuthLine.substr(uColon + 1);

		ParseUser(sAuthLine.substr(0, uColon));
	} else {
		m_sPass = sAuthLine;
	}
}

void CClient::ParseUser(const CString& sAuthLine) {
	// user[@identifier][/network]

	const size_t uSlash = sAuthLine.rfind("/");
	if (uSlash != CString::npos) {
		m_sNetwork = sAuthLine.substr(uSlash + 1);

		ParseIdentifier(sAuthLine.substr(0, uSlash));
	} else {
		ParseIdentifier(sAuthLine);
	}
}

void CClient::ParseIdentifier(const CString& sAuthLine) {
	// user[@identifier]

	const size_t uAt = sAuthLine.rfind("@");
	if (uAt != CString::npos) {
		const CString sId = sAuthLine.substr(uAt + 1);

		if (IsValidIdentifier(sId)) {
			m_sIdentifier = sId;
			m_sUser = sAuthLine.substr(0, uAt);
		} else {
			m_sUser = sAuthLine;
		}
	} else {
		m_sUser = sAuthLine;
	}
}

void CClient::NotifyServerDependentCaps(const SCString& ssCaps)
{
	for (const CString& sCap : ssCaps) {
		const auto& it = m_mCoreCaps.find(sCap);
		if (m_mCoreCaps.end() != it) {
			bool bServerDependent = std::get<0>(it->second);
			if (bServerDependent) {
				m_ssServerDependentCaps.insert(sCap);
			}
		}
	}

	if (HasCapNotify() && !m_ssServerDependentCaps.empty()) {
		CString sCaps = CString(" ").Join(m_ssServerDependentCaps.begin(), m_ssServerDependentCaps.end());
		PutClient(":irc.znc.in CAP " + GetNick() + " NEW :" + sCaps);
	}
}

void CClient::ClearServerDependentCaps()
{
	if (HasCapNotify() && !m_ssServerDependentCaps.empty()) {
		CString sCaps = CString(" ").Join(m_ssServerDependentCaps.begin(), m_ssServerDependentCaps.end());
		PutClient(":irc.znc.in CAP " + GetNick() + " DEL :" + sCaps);

		for (const CString& sCap : m_ssServerDependentCaps) {
			const auto& it = m_mCoreCaps.find(sCap);
			if (m_mCoreCaps.end() != it) {
				const auto& handler = std::get<1>(it->second);
				handler(false);
			}
		}
	}

	m_ssServerDependentCaps.clear();
}
