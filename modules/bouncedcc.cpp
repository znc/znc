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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

using std::set;

class CBounceDCCMod;

class CDCCBounce : public CSocket {
public:
	CDCCBounce(CBounceDCCMod* pMod, unsigned long uLongIP, unsigned short uPort,
			const CString& sFileName, const CString& sRemoteNick,
			const CString& sRemoteIP, bool bIsChat = false);
	CDCCBounce(CBounceDCCMod* pMod, const CString& sHostname, unsigned short uPort,
			const CString& sRemoteNick, const CString& sRemoteIP,
			const CString& sFileName, int iTimeout = 60, bool bIsChat = false);
	virtual ~CDCCBounce();

	static unsigned short DCCRequest(const CString& sNick, unsigned long uLongIP, unsigned short uPort, const CString& sFileName, bool bIsChat, CBounceDCCMod* pMod, const CString& sRemoteIP);

	void ReadLine(const CString& sData);
	void ReadData(const char* data, size_t len) override;
	void ReadPaused() override;
	void Timeout() override;
	void ConnectionRefused() override;
	void ReachedMaxBuffer() override;
	void SockError(int iErrno, const CString& sDescription) override;
	void Connected() override;
	void Disconnected() override;
	Csock* GetSockObj(const CString& sHost, unsigned short uPort) override;
	void Shutdown();
	void PutServ(const CString& sLine);
	void PutPeer(const CString& sLine);
	bool IsPeerConnected() { return (m_pPeer) ? m_pPeer->IsConnected() : false; }

	// Setters
	void SetPeer(CDCCBounce* p) { m_pPeer = p; }
	void SetRemoteIP(const CString& s) { m_sRemoteIP = s; }
	void SetRemoteNick(const CString& s) { m_sRemoteNick = s; }
	void SetRemote(bool b) { m_bIsRemote = b; }
	// !Setters

	// Getters
	unsigned short GetUserPort() const { return m_uRemotePort; }
	const CString& GetRemoteAddr() const { return m_sRemoteIP; }
	const CString& GetRemoteNick() const { return m_sRemoteNick; }
	const CString& GetFileName() const { return m_sFileName; }
	CDCCBounce* GetPeer() const { return m_pPeer; }
	bool IsRemote() { return m_bIsRemote; }
	bool IsChat() { return m_bIsChat; }
	// !Getters
private:
protected:
	CString                      m_sRemoteNick;
	CString                      m_sRemoteIP;
	CString                      m_sConnectIP;
	CString                      m_sLocalIP;
	CString                      m_sFileName;
	CBounceDCCMod*               m_pModule;
	CDCCBounce*                  m_pPeer;
	unsigned short               m_uRemotePort;
	bool                         m_bIsChat;
	bool                         m_bIsRemote;

	static const unsigned int    m_uiMaxDCCBuffer;
	static const unsigned int    m_uiMinDCCBuffer;
};

// If we buffer more than this in memory, we will throttle the receiving side
const unsigned int CDCCBounce::m_uiMaxDCCBuffer = 10 * 1024;
// If less than this is in the buffer, the receiving side continues
const unsigned int CDCCBounce::m_uiMinDCCBuffer = 2 * 1024;

class CBounceDCCMod : public CModule {
public:
	void ListDCCsCommand(const CString& sLine) {
		CTable Table;
		Table.AddColumn("Type");
		Table.AddColumn("State");
		Table.AddColumn("Speed");
		Table.AddColumn("Nick");
		Table.AddColumn("IP");
		Table.AddColumn("File");

		set<CSocket*>::const_iterator it;
		for (it = BeginSockets(); it != EndSockets(); ++it) {
			CDCCBounce* pSock = (CDCCBounce*) *it;
			CString sSockName = pSock->GetSockName();

			if (!(pSock->IsRemote())) {
				Table.AddRow();
				Table.SetCell("Nick", pSock->GetRemoteNick());
				Table.SetCell("IP", pSock->GetRemoteAddr());

				if (pSock->IsChat()) {
					Table.SetCell("Type", "Chat");
				} else {
					Table.SetCell("Type", "Xfer");
					Table.SetCell("File", pSock->GetFileName());
				}

				CString sState = "Waiting";
				if ((pSock->IsConnected()) || (pSock->IsPeerConnected())) {
					sState = "Halfway";
					if ((pSock->IsConnected()) && (pSock->IsPeerConnected())) {
						sState = "Connected";
					}
				}
				Table.SetCell("State", sState);
			}
		}

		if (PutModule(Table) == 0) {
			PutModule("You have no active DCCs.");
		}
	}

	void UseClientIPCommand(const CString& sLine) {
		CString sValue = sLine.Token(1, true);

		if (!sValue.empty()) {
			SetNV("UseClientIP", sValue);
		}

		PutModule("UseClientIP: " + CString(GetNV("UseClientIP").ToBool()));
	}

	MODCONSTRUCTOR(CBounceDCCMod) {
		AddHelpCommand();
		AddCommand("ListDCCs", static_cast<CModCommand::ModCmdFunc>(&CBounceDCCMod::ListDCCsCommand),
			"", "List all active DCCs");
		AddCommand("UseClientIP", static_cast<CModCommand::ModCmdFunc>(&CBounceDCCMod::UseClientIPCommand),
			"<true|false>");
	}

	virtual ~CBounceDCCMod() {}

	CString GetLocalDCCIP() {
		return GetUser()->GetLocalDCCIP();
	}

	bool UseClientIP() {
		return GetNV("UseClientIP").ToBool();
	}

	EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override {
		if (sMessage.StartsWith("DCC ")) {
			CString sType = sMessage.Token(1, false, " ", false, "\"", "\"", true);
			CString sFile = sMessage.Token(2, false, " ", false, "\"", "\"", false);
			unsigned long uLongIP = sMessage.Token(3, false, " ", false, "\"", "\"", true).ToULong();
			unsigned short uPort = sMessage.Token(4, false, " ", false, "\"", "\"", true).ToUShort();
			unsigned long uFileSize = sMessage.Token(5, false, " ", false, "\"", "\"", true).ToULong();
			CString sIP = GetLocalDCCIP();

			if (!UseClientIP()) {
				uLongIP = CUtils::GetLongIP(GetClient()->GetRemoteIP());
			}

			if (sType.Equals("CHAT")) {
				unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, "", true, this, "");
				if (uBNCPort) {
					PutIRC("PRIVMSG " + sTarget + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + "\001");
				}
			} else if (sType.Equals("SEND")) {
				// DCC SEND readme.txt 403120438 5550 1104
				unsigned short uBNCPort = CDCCBounce::DCCRequest(sTarget, uLongIP, uPort, sFile, false, this, "");
				if (uBNCPort) {
					PutIRC("PRIVMSG " + sTarget + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
				}
			} else if (sType.Equals("RESUME")) {
				// PRIVMSG user :DCC RESUME "znc.o" 58810 151552
				unsigned short uResumePort = sMessage.Token(3).ToUShort();

				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;

					if (pSock->GetLocalPort() == uResumePort) {
						PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			} else if (sType.Equals("ACCEPT")) {
				// Need to lookup the connection by port, filter the port, and forward to the user

				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;
					if (pSock->GetUserPort() == sMessage.Token(3).ToUShort()) {
						PutIRC("PRIVMSG " + sTarget + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			}

			return HALTCORE;
		}

		return CONTINUE;
	}

	EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
		CIRCNetwork* pNetwork = GetNetwork();
		if (sMessage.StartsWith("DCC ") && pNetwork->IsUserAttached()) {
			// DCC CHAT chat 2453612361 44592
			CString sType = sMessage.Token(1, false, " ", false, "\"", "\"", true);
			CString sFile = sMessage.Token(2, false, " ", false, "\"", "\"", false);
			unsigned long uLongIP = sMessage.Token(3, false, " ", false, "\"", "\"", true).ToULong();
			unsigned short uPort = sMessage.Token(4, false, " ", false, "\"", "\"", true).ToUShort();
			unsigned long uFileSize = sMessage.Token(5, false, " ", false, "\"", "\"", true).ToULong();

			if (sType.Equals("CHAT")) {
				CNick FromNick(Nick.GetNickMask());
				unsigned short uBNCPort = CDCCBounce::DCCRequest(FromNick.GetNick(), uLongIP, uPort, "", true, this, CUtils::GetIP(uLongIP));
				if (uBNCPort) {
					CString sIP = GetLocalDCCIP();
					PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + pNetwork->GetCurNick() + " :\001DCC CHAT chat " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + "\001");
				}
			} else if (sType.Equals("SEND")) {
				// DCC SEND readme.txt 403120438 5550 1104
				unsigned short uBNCPort = CDCCBounce::DCCRequest(Nick.GetNick(), uLongIP, uPort, sFile, false, this, CUtils::GetIP(uLongIP));
				if (uBNCPort) {
					CString sIP = GetLocalDCCIP();
					PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + pNetwork->GetCurNick() + " :\001DCC SEND " + sFile + " " + CString(CUtils::GetLongIP(sIP)) + " " + CString(uBNCPort) + " " + CString(uFileSize) + "\001");
				}
			} else if (sType.Equals("RESUME")) {
				// Need to lookup the connection by port, filter the port, and forward to the user
				unsigned short uResumePort = sMessage.Token(3).ToUShort();

				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;

					if (pSock->GetLocalPort() == uResumePort) {
						PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + pNetwork->GetCurNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetUserPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			} else if (sType.Equals("ACCEPT")) {
				// Need to lookup the connection by port, filter the port, and forward to the user
				set<CSocket*>::const_iterator it;
				for (it = BeginSockets(); it != EndSockets(); ++it) {
					CDCCBounce* pSock = (CDCCBounce*) *it;

					if (pSock->GetUserPort() == sMessage.Token(3).ToUShort()) {
						PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + pNetwork->GetCurNick() + " :\001DCC " + sType + " " + sFile + " " + CString(pSock->GetLocalPort()) + " " + sMessage.Token(4) + "\001");
					}
				}
			}

			return HALTCORE;
		}

		return CONTINUE;
	}
};

CDCCBounce::CDCCBounce(CBounceDCCMod* pMod, unsigned long uLongIP, unsigned short uPort,
		const CString& sFileName, const CString& sRemoteNick,
		const CString& sRemoteIP, bool bIsChat) : CSocket(pMod) {
	m_uRemotePort = uPort;
	m_sConnectIP = CUtils::GetIP(uLongIP);
	m_sRemoteIP = sRemoteIP;
	m_sFileName = sFileName;
	m_sRemoteNick = sRemoteNick;
	m_pModule = pMod;
	m_bIsChat = bIsChat;
	m_sLocalIP = pMod->GetLocalDCCIP();
	m_pPeer = NULL;
	m_bIsRemote = false;

	if (bIsChat) {
		EnableReadLine();
	} else {
		DisableReadLine();
	}
}

CDCCBounce::CDCCBounce(CBounceDCCMod* pMod, const CString& sHostname, unsigned short uPort,
		const CString& sRemoteNick, const CString& sRemoteIP, const CString& sFileName,
		int iTimeout, bool bIsChat) : CSocket(pMod, sHostname, uPort, iTimeout) {
	m_uRemotePort = 0;
	m_bIsChat = bIsChat;
	m_pModule = pMod;
	m_pPeer = NULL;
	m_sRemoteNick = sRemoteNick;
	m_sFileName = sFileName;
	m_sRemoteIP = sRemoteIP;
	m_bIsRemote = false;

	SetMaxBufferThreshold(10240);
	if (bIsChat) {
		EnableReadLine();
	} else {
		DisableReadLine();
	}
}

CDCCBounce::~CDCCBounce() {
	if (m_pPeer) {
		m_pPeer->Shutdown();
		m_pPeer = NULL;
	}
}

void CDCCBounce::ReadLine(const CString& sData) {
	CString sLine = sData.TrimRight_n("\r\n");

	DEBUG(GetSockName() << " <- [" << sLine << "]");

	PutPeer(sLine);
}

void CDCCBounce::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");

	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Too long line received");
	Close();
}

void CDCCBounce::ReadData(const char* data, size_t len) {
	if (m_pPeer) {
		m_pPeer->Write(data, len);

		size_t BufLen = m_pPeer->GetInternalWriteBuffer().length();

		if (BufLen >= m_uiMaxDCCBuffer) {
			DEBUG(GetSockName() << " The send buffer is over the "
					"limit (" << BufLen <<"), throttling");
			PauseRead();
		}
	}
}

void CDCCBounce::ReadPaused() {
	if (!m_pPeer || m_pPeer->GetInternalWriteBuffer().length() <= m_uiMinDCCBuffer)
		UnPauseRead();
}

void CDCCBounce::Timeout() {
	DEBUG(GetSockName() << " == Timeout()");
	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		CString sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = " to [" + sHost + " " + CString(Csock::GetPort()) + "]";
		} else {
			sHost = ".";
		}

		m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout while connecting" + sHost);
	} else {
		m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Timeout waiting for incoming connection [" + Csock::GetLocalIP() + ":" + CString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::ConnectionRefused() {
	DEBUG(GetSockName() << " == ConnectionRefused()");

	CString sType = (m_bIsChat) ? "Chat" : "Xfer";
	CString sHost = Csock::GetHostName();
	if (!sHost.empty()) {
		sHost = " to [" + sHost + " " + CString(Csock::GetPort()) + "]";
	} else {
		sHost = ".";
	}

	m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Connection Refused while connecting" + sHost);
}

void CDCCBounce::SockError(int iErrno, const CString& sDescription) {
	DEBUG(GetSockName() << " == SockError(" << iErrno << ")");
	CString sType = (m_bIsChat) ? "Chat" : "Xfer";

	if (IsRemote()) {
		CString sHost = Csock::GetHostName();
		if (!sHost.empty()) {
			sHost = "[" + sHost + " " + CString(Csock::GetPort()) + "]";
		}

		m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + sDescription + "]" + sHost);
	} else {
		m_pModule->PutModule("DCC " + sType + " Bounce (" + m_sRemoteNick + "): Socket error [" + sDescription + "] [" + Csock::GetLocalIP() + ":" + CString(Csock::GetLocalPort()) + "]");
	}
}

void CDCCBounce::Connected() {
	SetTimeout(0);
	DEBUG(GetSockName() << " == Connected()");
}

void CDCCBounce::Disconnected() {
	DEBUG(GetSockName() << " == Disconnected()");
}

void CDCCBounce::Shutdown() {
	m_pPeer = NULL;
	DEBUG(GetSockName() << " == Close(); because my peer told me to");
	Close();
}

Csock* CDCCBounce::GetSockObj(const CString& sHost, unsigned short uPort) {
	Close();

	if (m_sRemoteIP.empty()) {
		m_sRemoteIP = sHost;
	}

	CDCCBounce* pSock = new CDCCBounce(m_pModule, sHost, uPort, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	CDCCBounce* pRemoteSock = new CDCCBounce(m_pModule, sHost, uPort, m_sRemoteNick, m_sRemoteIP, m_sFileName, m_bIsChat);
	pSock->SetPeer(pRemoteSock);
	pRemoteSock->SetPeer(pSock);
	pRemoteSock->SetRemote(true);
	pSock->SetRemote(false);

	CZNC::Get().GetManager().Connect(m_sConnectIP, m_uRemotePort, "DCC::" + CString((m_bIsChat) ? "Chat" : "XFER") + "::Remote::" + m_sRemoteNick, 60, false, m_sLocalIP, pRemoteSock);

	pSock->SetSockName(GetSockName());
	return pSock;
}

void CDCCBounce::PutServ(const CString& sLine) {
	DEBUG(GetSockName() << " -> [" << sLine << "]");
	Write(sLine + "\r\n");
}

void CDCCBounce::PutPeer(const CString& sLine) {
	if (m_pPeer) {
		m_pPeer->PutServ(sLine);
	} else {
		PutServ("*** Not connected yet ***");
	}
}

unsigned short CDCCBounce::DCCRequest(const CString& sNick, unsigned long uLongIP, unsigned short uPort, const CString& sFileName, bool bIsChat, CBounceDCCMod* pMod, const CString& sRemoteIP) {
	CDCCBounce* pDCCBounce = new CDCCBounce(pMod, uLongIP, uPort, sFileName, sNick, sRemoteIP, bIsChat);
	unsigned short uListenPort = CZNC::Get().GetManager().ListenRand("DCC::" + CString((bIsChat) ? "Chat" : "Xfer") + "::Local::" + sNick,
			pMod->GetLocalDCCIP(), false, SOMAXCONN, pDCCBounce, 120);

	return uListenPort;
}

template<> void TModInfo<CBounceDCCMod>(CModInfo& Info) {
	Info.SetWikiPage("bouncedcc");
}

USERMODULEDEFS(CBounceDCCMod, "Bounces DCC transfers through ZNC instead of sending them directly to the user. ")

