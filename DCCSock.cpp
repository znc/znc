/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "DCCSock.h"
#include "User.h"
#include "Utils.h"

CDCCSock::CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sLocalFile, const CString& sModuleName,
		unsigned long uFileSize, CFile* pFile) : CZNCSock() {
	m_sRemoteNick = sRemoteNick;
	m_uFileSize = uFileSize;
	m_uRemotePort = 0;
	m_uBytesSoFar = 0;
	m_pUser = pUser;
	m_pFile = pFile;
	m_sLocalFile = sLocalFile;
	m_sModuleName = sModuleName;
	m_bSend = true;
	m_bNoDelFile = false;

	m_pUser->AddDCCSock(this);
}

CDCCSock::CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort,
		const CString& sLocalFile, unsigned long uFileSize, const CString& sModuleName) : CZNCSock() {
	m_sRemoteNick = sRemoteNick;
	m_sRemoteIP = sRemoteIP;
	m_uRemotePort = uRemotePort;
	m_uFileSize = uFileSize;
	m_uBytesSoFar = 0;
	m_pUser = pUser;
	m_pFile = NULL;
	m_sLocalFile = sLocalFile;
	m_sModuleName = sModuleName;
	m_bSend = false;
	m_bNoDelFile = false;

	m_pUser->AddDCCSock(this);
}

CDCCSock::~CDCCSock() {
	if ((m_pFile) && (!m_bNoDelFile)) {
		m_pFile->Close();
		delete m_pFile;
	}

	m_pUser->AddBytesRead(GetBytesRead());
	m_pUser->AddBytesWritten(GetBytesWritten());

	m_pUser->DelDCCSock(this);
}

void CDCCSock::ReadData(const char* data, size_t len) {
	if (!m_pFile) {
		DEBUG("File not open! closing get.");
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File not open!");
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
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Connection Refused.");
}

void CDCCSock::Timeout() {
	DEBUG(GetSockName() << " == Timeout()");
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Timed Out.");
}

void CDCCSock::SockError(int iErrno) {
	DEBUG(GetSockName() << " == SockError(" << iErrno << ")");
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Socket Error [" + CString(iErrno) + "]");
}

void CDCCSock::Connected() {
	DEBUG(GetSockName() << " == Connected(" << GetRemoteIP() << ")");
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Transfer Started.");

	if (m_bSend) {
		SendPacket();
	}

	SetTimeout(120);
}

void CDCCSock::Disconnected() {
	const CString sStart = ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - ";

	DEBUG(GetSockName() << " == Disconnected()");

	if (m_uBytesSoFar > m_uFileSize) {
		m_pUser->PutModule(m_sModuleName, sStart + "TooMuchData!");
	} else if (m_uBytesSoFar == m_uFileSize) {
		if (m_bSend) {
			m_pUser->PutModule(m_sModuleName, sStart + "Completed! - Sent [" + m_sLocalFile +
					"] at [" + CString((int) (GetAvgWrite() / 1024.0)) + " KiB/s ]");
		} else {
			m_pUser->PutModule(m_sModuleName, sStart + "Completed! - Saved to [" + m_sLocalFile +
					"] at [" + CString((int) (GetAvgRead() / 1024.0)) + " KiB/s ]");
		}
	} else {
		m_pUser->PutModule(m_sModuleName, sStart + "Incomplete!");
	}
}

void CDCCSock::SendPacket() {
	if (!m_pFile) {
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File closed prematurely.");
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
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Error reading from file.");
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

	CDCCSock* pSock = new CDCCSock(m_pUser, m_sRemoteNick, m_sLocalFile, m_sModuleName, m_uFileSize, m_pFile);
	pSock->SetSockName("DCC::SEND::" + m_sRemoteNick);
	pSock->SetTimeout(120);
	pSock->SetFileName(m_sFileName);
	pSock->SetFileOffset(m_uBytesSoFar);
	m_bNoDelFile = true;

	return pSock;
}

CFile* CDCCSock::OpenFile(bool bWrite) {
	if ((m_pFile) || (m_sLocalFile.empty())) {
		m_pUser->PutModule(m_sModuleName, ((bWrite) ? "DCC <- [" : "DCC -> [") + m_sRemoteNick + "][" + m_sLocalFile + "] - Unable to open file.");
		return NULL;
	}

	m_pFile = new CFile(m_sLocalFile);

	if (bWrite) {
		if (m_pFile->Exists()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC <- [" + m_sRemoteNick + "] - File already exists [" + m_sLocalFile + "]");
			return NULL;
		}

		if (!m_pFile->Open(O_WRONLY | O_TRUNC | O_CREAT)) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC <- [" + m_sRemoteNick + "] - Could not open file [" + m_sLocalFile + "]");
			return NULL;
		}
	} else {
		if (!m_pFile->IsReg()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC -> [" + m_sRemoteNick + "] - Not a file [" + m_sLocalFile + "]");
			return NULL;
		}

		if (!m_pFile->Open()) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC -> [" + m_sRemoteNick + "] - Could not open file [" + m_sLocalFile + "]");
			return NULL;
		}

		// The DCC specs only allow file transfers with files smaller
		// than 4GiB (see ReadData()).
		unsigned long long uFileSize = m_pFile->GetSize();
		if (uFileSize > (unsigned long long) 0xffffffff) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC -> [" + m_sRemoteNick + "] - File too large (>4 GiB) [" + m_sLocalFile + "]");
			return NULL;
		}

		m_uFileSize = (unsigned long) uFileSize;
	}

	m_sFileName = m_pFile->GetShortName();

	return m_pFile;
}

