/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "Chan.h"
#include "User.h"
#include "Modules.h"
#include "FileUtils.h"

class CDCCMod;

class CDCCSock : public CSocket {
public:
	CDCCSock(CDCCMod* pMod, const CString& sRemoteNick, const CString& sLocalFile, unsigned long uFileSize = 0, CFile* pFile = NULL);
	CDCCSock(CDCCMod* pMod, const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sLocalFile, unsigned long uFileSize);
	virtual ~CDCCSock();

	virtual void ReadData(const char* data, size_t len);
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();
	virtual void Connected();
	virtual void Disconnected();
	void SendPacket();
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	CFile* OpenFile(bool bWrite = true);
	bool Seek(unsigned int uPos);

	// Setters
	void SetRemoteIP(const CString& s) { m_sRemoteIP = s; }
	void SetRemoteNick(const CString& s) { m_sRemoteNick = s; }
	void SetFileName(const CString& s) { m_sFileName = s; }
	void SetFileOffset(unsigned long u) { m_uBytesSoFar = u; }
	// !Setters

	// Getters
	unsigned short GetUserPort() const { return m_uRemotePort; }
	const CString& GetRemoteNick() const { return m_sRemoteNick; }
	const CString& GetFileName() const { return m_sFileName; }
	const CString& GetLocalFile() const { return m_sLocalFile; }
	CFile* GetFile() { return m_pFile; }
	double GetProgress() const { return ((m_uFileSize) && (m_uBytesSoFar)) ? (double) (((double) m_uBytesSoFar / (double) m_uFileSize) *100.0) : 0; }
	bool IsSend() const { return m_bSend; }
	//const CString& GetRemoteIP() const { return m_sRemoteIP; }
	// !Getters
private:
protected:
	CString         m_sRemoteNick;
	CString         m_sRemoteIP;
	CString         m_sFileName;
	CString         m_sLocalFile;
	CString         m_sSendBuf;
	unsigned short  m_uRemotePort;
	unsigned long   m_uFileSize;
	unsigned long   m_uBytesSoFar;
	bool            m_bSend;
	bool            m_bNoDelFile;
	CFile*          m_pFile;
	CDCCMod*        m_pModule;
};

class CDCCMod : public CModule {
public:
	MODCONSTRUCTOR(CDCCMod) {
		AddHelpCommand();
		AddCommand("Send",          static_cast<CModCommand::ModCmdFunc>(&CDCCMod::SendCommand),
			"<nick> <file>");
		AddCommand("Get",           static_cast<CModCommand::ModCmdFunc>(&CDCCMod::GetCommand),
			"<file>");
		AddCommand("ListTransfers", static_cast<CModCommand::ModCmdFunc>(&CDCCMod::ListTransfersCommand));
	}

	virtual ~CDCCMod() {}

#ifndef MOD_DCC_ALLOW_EVERYONE
	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!m_pUser->IsAdmin()) {
			sMessage = "You must be admin to use the DCC module";
			return false;
		}

		return true;
	}
#endif

	bool SendFile(const CString& sRemoteNick, const CString& sFileName) {
		CString sFullPath = CDir::ChangeDir(GetSavePath(), sFileName, CZNC::Get().GetHomePath());
		CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sFullPath);

		CFile* pFile = pSock->OpenFile(false);

		if (!pFile) {
			delete pSock;
			return false;
		}

		unsigned short uPort = CZNC::Get().GetManager().ListenRand("DCC::LISTEN::" + sRemoteNick, m_pUser->GetLocalDCCIP(), false, SOMAXCONN, pSock, 120);

		if (m_pUser->GetNick().Equals(sRemoteNick)) {
			PutUser(":*dcc!znc@znc.in PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(m_pUser->GetLocalDCCIP())) + " "
				+ CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
		} else {
			PutIRC("PRIVMSG " + sRemoteNick + " :\001DCC SEND " + pFile->GetShortName() + " " + CString(CUtils::GetLongIP(m_pUser->GetLocalDCCIP())) + " "
			    + CString(uPort) + " " + CString(pFile->GetSize()) + "\001");
		}

		PutModule("DCC -> [" + sRemoteNick + "][" + pFile->GetShortName() + "] - Attempting Send.");
		return true;
	}

	bool GetFile(const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sFileName, unsigned long uFileSize) {
		if (CFile::Exists(sFileName)) {
			PutModule("DCC <- [" + sRemoteNick + "][" + sFileName + "] - File already exists.");
			return false;
		}

		CDCCSock* pSock = new CDCCSock(this, sRemoteNick, sRemoteIP, uRemotePort, sFileName, uFileSize);

		if (!pSock->OpenFile()) {
			delete pSock;
			return false;
		}

		if (!CZNC::Get().GetManager().Connect(sRemoteIP, uRemotePort, "DCC::GET::" + sRemoteNick, 60, false, m_pUser->GetLocalDCCIP(), pSock)) {
			PutModule("DCC <- [" + sRemoteNick + "][" + sFileName + "] - Unable to connect.");
			return false;
		}

		PutModule("DCC <- [" + sRemoteNick + "][" + sFileName + "] - Attempting to connect to [" + sRemoteIP + "]");
		return true;
	}

	void SendCommand(const CString& sLine) {
		CString sToNick = sLine.Token(1);
		CString sFile = sLine.Token(2);
		CString sAllowedPath = GetSavePath();
		CString sAbsolutePath;

		if ((sToNick.empty()) || (sFile.empty())) {
			PutModule("Usage: Send <nick> <file>");
			return;
		}

		sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

		if (sAbsolutePath.empty()) {
			PutStatus("Illegal path.");
			return;
		}

		SendFile(sToNick, sFile);
	}

	void GetCommand(const CString& sLine) {
		CString sFile = sLine.Token(1);
		CString sAllowedPath = GetSavePath();
		CString sAbsolutePath;

		if (sFile.empty()) {
			PutModule("Usage: Get <file>");
			return;
		}

		sAbsolutePath = CDir::CheckPathPrefix(sAllowedPath, sFile);

		if (sAbsolutePath.empty()) {
			PutModule("Illegal path.");
			return;
		}

		SendFile(m_pUser->GetNick(), sFile);
	}

	void ListTransfersCommand(const CString& sLine) {
		CTable Table;
		Table.AddColumn("Type");
		Table.AddColumn("State");
		Table.AddColumn("Speed");
		Table.AddColumn("Nick");
		Table.AddColumn("IP");
		Table.AddColumn("File");

		set<CSocket*>::const_iterator it;
		for (it = BeginSockets(); it != EndSockets(); ++it) {
			CDCCSock* pSock = (CDCCSock*) *it;

			Table.AddRow();
			Table.SetCell("Nick", pSock->GetRemoteNick());
			Table.SetCell("IP", pSock->GetRemoteIP());
			Table.SetCell("File", pSock->GetFileName());

			if (pSock->IsSend()) {
				Table.SetCell("Type", "Sending");
			} else {
				Table.SetCell("Type", "Getting");
			}

			if (pSock->GetType() == Csock::LISTENER) {
				Table.SetCell("State", "Waiting");
			} else {
				Table.SetCell("State", CString::ToPercent(pSock->GetProgress()));
				Table.SetCell("Speed", CString((int)(pSock->GetAvgRead() / 1024.0)) + " KiB/s");
			}
		}

		if (PutModule(Table) == 0) {
			PutModule("You have no active DCC transfers.");
		}
	}

	virtual void OnModCTCP(const CString& sMessage) {
		if (sMessage.Equals("DCC RESUME ", false, 11)) {
			CString sFile = sMessage.Token(2);
			unsigned short uResumePort = sMessage.Token(3).ToUShort();
			unsigned long uResumeSize = sMessage.Token(4).ToULong();

			set<CSocket*>::const_iterator it;
			for (it = BeginSockets(); it != EndSockets(); ++it) {
				CDCCSock* pSock = (CDCCSock*) *it;

				if (pSock->GetLocalPort() == uResumePort) {
					if (pSock->Seek(uResumeSize)) {
						PutModule("DCC -> [" + pSock->GetRemoteNick() + "][" + pSock->GetFileName() + "] - Attempting to resume from file position [" + CString(uResumeSize) + "]");
						PutUser(":*dcc!znc@znc.in PRIVMSG " + m_pUser->GetNick() + " :\001DCC ACCEPT " + sFile + " " + CString(uResumePort) + " " + CString(uResumeSize) + "\001");
					} else {
						PutModule("DCC -> [" + m_pUser->GetNick() + "][" + sFile + "] Unable to find send to initiate resume.");
					}
				}

			}
		} else if (sMessage.Equals("DCC SEND ", false, 9)) {
			CString sLocalFile = CDir::CheckPathPrefix(GetSavePath(), sMessage.Token(2));
			if (sLocalFile.empty()) {
				PutModule("Bad DCC file: " + sMessage.Token(2));
			}
			unsigned long uLongIP = sMessage.Token(3).ToULong();
			unsigned short uPort = sMessage.Token(4).ToUShort();
			unsigned long uFileSize = sMessage.Token(5).ToULong();
			GetFile(m_pUser->GetCurNick(), CUtils::GetIP(uLongIP), uPort, sLocalFile, uFileSize);
		}
	}
};

CDCCSock::CDCCSock(CDCCMod* pMod, const CString& sRemoteNick, const CString& sLocalFile,
		unsigned long uFileSize, CFile* pFile) : CSocket(pMod) {
	m_sRemoteNick = sRemoteNick;
	m_uFileSize = uFileSize;
	m_uRemotePort = 0;
	m_uBytesSoFar = 0;
	m_pModule = pMod;
	m_pFile = pFile;
	m_sLocalFile = sLocalFile;
	m_bSend = true;
	m_bNoDelFile = false;
	SetMaxBufferThreshold(0);
}

CDCCSock::CDCCSock(CDCCMod* pMod, const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort,
		const CString& sLocalFile, unsigned long uFileSize) : CSocket(pMod) {
	m_sRemoteNick = sRemoteNick;
	m_sRemoteIP = sRemoteIP;
	m_uRemotePort = uRemotePort;
	m_uFileSize = uFileSize;
	m_uBytesSoFar = 0;
	m_pModule = pMod;
	m_pFile = NULL;
	m_sLocalFile = sLocalFile;
	m_bSend = false;
	m_bNoDelFile = false;
	SetMaxBufferThreshold(0);
}

CDCCSock::~CDCCSock() {
	if ((m_pFile) && (!m_bNoDelFile)) {
		m_pFile->Close();
		delete m_pFile;
	}
}

void CDCCSock::ReadData(const char* data, size_t len) {
	if (!m_pFile) {
		DEBUG("File not open! closing get.");
		m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File not open!");
		Close();
	}

	// DCC specs says the receiving end sends the number of bytes it
	// received so far as a 4 byte integer in network byte order, so we need
	// uint32_t to do the job portably. This also means that the maximum
	// file that we can transfer is 4 GiB big (see OpenFile()).
	if (m_bSend) {
		m_sSendBuf.append(data, len);

		while (m_sSendBuf.size() >= 4) {
			uint32_t iRemoteSoFar;
			memcpy(&iRemoteSoFar, m_sSendBuf.data(), sizeof(iRemoteSoFar));
			iRemoteSoFar = ntohl(iRemoteSoFar);

			if ((iRemoteSoFar + 65536) >= m_uBytesSoFar) {
				SendPacket();
			}

			m_sSendBuf.erase(0, 4);
		}
	} else {
		m_pFile->Write(data, len);
		m_uBytesSoFar += len;
		uint32_t uSoFar = htonl(m_uBytesSoFar);
		Write((char*) &uSoFar, sizeof(uSoFar));

		if (m_uBytesSoFar >= m_uFileSize) {
			Close();
		}
	}
}

void CDCCSock::ConnectionRefused() {
	DEBUG(GetSockName() << " == ConnectionRefused()");
	m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Connection Refused.");
}

void CDCCSock::Timeout() {
	DEBUG(GetSockName() << " == Timeout()");
	m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Timed Out.");
}

void CDCCSock::SockError(int iErrno) {
	DEBUG(GetSockName() << " == SockError(" << iErrno << ")");
	m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Socket Error [" + CString(iErrno) + "]");
}

void CDCCSock::Connected() {
	DEBUG(GetSockName() << " == Connected(" << GetRemoteIP() << ")");
	m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Transfer Started.");

	if (m_bSend) {
		SendPacket();
	}

	SetTimeout(120);
}

void CDCCSock::Disconnected() {
	const CString sStart = ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - ";

	DEBUG(GetSockName() << " == Disconnected()");

	if (m_uBytesSoFar > m_uFileSize) {
		m_pModule->PutModule(sStart + "TooMuchData!");
	} else if (m_uBytesSoFar == m_uFileSize) {
		if (m_bSend) {
			m_pModule->PutModule(sStart + "Completed! - Sent [" + m_sLocalFile +
					"] at [" + CString((int) (GetAvgWrite() / 1024.0)) + " KiB/s ]");
		} else {
			m_pModule->PutModule(sStart + "Completed! - Saved to [" + m_sLocalFile +
					"] at [" + CString((int) (GetAvgRead() / 1024.0)) + " KiB/s ]");
		}
	} else {
		m_pModule->PutModule(sStart + "Incomplete!");
	}
}

void CDCCSock::SendPacket() {
	if (!m_pFile) {
		m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File closed prematurely.");
		Close();
		return;
	}

	if (GetInternalWriteBuffer().size() > 1024 * 1024) {
		// There is still enough data to be written, don't add more
		// stuff to that buffer.
		DEBUG("SendPacket(): Skipping send, buffer still full enough [" << GetInternalWriteBuffer().size() << "]["
				<< m_sRemoteNick << "][" << m_sFileName << "]");
		return;
	}

	char szBuf[4096];
	int iLen = m_pFile->Read(szBuf, 4096);

	if (iLen < 0) {
		m_pModule->PutModule(((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Error reading from file.");
		Close();
		return;
	}

	if (iLen > 0) {
		Write(szBuf, iLen);
		m_uBytesSoFar += iLen;
	}
}

Csock* CDCCSock::GetSockObj(const CString& sHost, unsigned short uPort) {
	Close();

	CDCCSock* pSock = new CDCCSock(m_pModule, m_sRemoteNick, m_sLocalFile, m_uFileSize, m_pFile);
	pSock->SetSockName("DCC::SEND::" + m_sRemoteNick);
	pSock->SetTimeout(120);
	pSock->SetFileName(m_sFileName);
	pSock->SetFileOffset(m_uBytesSoFar);
	m_bNoDelFile = true;

	return pSock;
}

CFile* CDCCSock::OpenFile(bool bWrite) {
	if ((m_pFile) || (m_sLocalFile.empty())) {
		m_pModule->PutModule(((bWrite) ? "DCC <- [" : "DCC -> [") + m_sRemoteNick + "][" + m_sLocalFile + "] - Unable to open file.");
		return NULL;
	}

	m_pFile = new CFile(m_sLocalFile);

	if (bWrite) {
		if (m_pFile->Exists()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pModule->PutModule("DCC <- [" + m_sRemoteNick + "] - File already exists [" + m_sLocalFile + "]");
			return NULL;
		}

		if (!m_pFile->Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			delete m_pFile;
			m_pFile = NULL;
			m_pModule->PutModule("DCC <- [" + m_sRemoteNick + "] - Could not open file [" + m_sLocalFile + "]");
			return NULL;
		}
	} else {
		if (!m_pFile->IsReg()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pModule->PutModule("DCC -> [" + m_sRemoteNick + "] - Not a file [" + m_sLocalFile + "]");
			return NULL;
		}

		if (!m_pFile->Open()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pModule->PutModule("DCC -> [" + m_sRemoteNick + "] - Could not open file [" + m_sLocalFile + "]");
			return NULL;
		}

		// The DCC specs only allow file transfers with files smaller
		// than 4GiB (see ReadData()).
		unsigned long long uFileSize = m_pFile->GetSize();
		if (uFileSize > (unsigned long long) 0xffffffff) {
			delete m_pFile;
			m_pFile = NULL;
			m_pModule->PutModule("DCC -> [" + m_sRemoteNick + "] - File too large (>4 GiB) [" + m_sLocalFile + "]");
			return NULL;
		}

		m_uFileSize = (unsigned long) uFileSize;
	}

	m_sFileName = m_pFile->GetShortName();

	return m_pFile;
}

bool CDCCSock::Seek(unsigned int uPos) {
	if (m_pFile) {
		if (m_pFile->Seek(uPos)) {
			m_uBytesSoFar = uPos;
			return true;
		}
	}

	return false;
}

MODULEDEFS(CDCCMod, "This module allows you to transfer files to and from ZNC")

