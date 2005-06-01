#include "DCCSock.h"

void CDCCSock::ReadData(const char* data, int len) {
	if (!m_pFile) {
		DEBUG_ONLY(cout << "File not open! closing get." << endl);
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File not open!");
		Close();
	}

	if (m_bSend) {
		m_sSendBuf.append(data, len);

		while (m_sSendBuf.size() >= 4) {
			unsigned int iRemoteSoFar;
			memcpy(&iRemoteSoFar, m_sSendBuf.data(), 4);
			iRemoteSoFar = ntohl(iRemoteSoFar);

			if (iRemoteSoFar >= m_uBytesSoFar) {
				SendPacket();
			}

			m_sSendBuf.erase(0, 4);
		}
	} else {
		m_pFile->Write(data, len);
		m_uBytesSoFar += len;
		unsigned long uSoFar = htonl(m_uBytesSoFar);
		Write((char*) &uSoFar, sizeof(unsigned long));

		if (m_uBytesSoFar >= m_uFileSize) {
			Close();
		}
	}
}

void CDCCSock::ConnectionRefused() {
	DEBUG_ONLY(cout << GetSockName() << " == ConnectionRefused()" << endl);
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Connection Refused.");
}

void CDCCSock::Timeout() {
	DEBUG_ONLY(cout << GetSockName() << " == Timeout()" << endl);
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Timed Out.");
}

void CDCCSock::SockError(int iErrno) {
	DEBUG_ONLY(cout << GetSockName() << " == SockError(" << iErrno << ")" << endl);
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Socket Error [" + CString::ToString(iErrno) + "]");
}

void CDCCSock::Connected() {
	DEBUG_ONLY(cout << GetSockName() << " == Connected(" << GetRemoteIP() << ")" << endl);
	m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Transfer Started.");

	if (m_bSend) {
		SendPacket();
	}

	SetTimeout(120);
}

void CDCCSock::Disconnected() {
	DEBUG_ONLY(cout << GetSockName() << " == Disconnected()" << endl);
	if (m_uBytesSoFar > m_uFileSize) {
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - TooMuchData!");
	} else if (m_uBytesSoFar == m_uFileSize) {
		if (m_bSend) {
			m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Completed! - Sent [" + m_sLocalFile + "] at [" + CString::ToKBytes(GetAvgWrite() / 1000.0) + "]");
		} else {
			m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Completed! - Saved to [" + m_sLocalFile + "] at [" + CString::ToKBytes(GetAvgRead() / 1000.0) + "]");
		}
	} else {
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - Incomplete!");
	}
}

void CDCCSock::SendPacket() {
	if (!m_pFile) {
		m_pUser->PutModule(m_sModuleName, ((m_bSend) ? "DCC -> [" : "DCC <- [") + m_sRemoteNick + "][" + m_sFileName + "] - File closed prematurely.");
		Close();
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
		return false;
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

		if (!m_pFile->Open(O_RDONLY)) {
			delete m_pFile;
			m_pFile = NULL;
			m_pUser->PutModule(m_sModuleName, "DCC -> [" + m_sRemoteNick + "] - Could not open file [" + m_sLocalFile + "]");
			return NULL;
		}

		m_uFileSize = m_pFile->GetSize();
	}

	m_sFileName = m_pFile->GetShortName();

	return m_pFile;
}

