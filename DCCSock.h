#ifndef _DCCSOCK_H
#define _DCCSOCK_H

#include "main.h"
#include "Utils.h"
#include "FileUtils.h"
#include "User.h"

class CDCCSock : public Csock {
public:
	CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sLocalFile, const CString& sModuleName, unsigned long uFileSize = 0, CFile* pFile = NULL) : Csock() {
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
	}

	CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sLocalFile, unsigned long uFileSize, const CString& sModuleName) : Csock() {
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
	}

/*	CDCCSock(CUser* pUser, const CString& sHostname, unsigned short uPort, int itimeout = 60) : Csock(sHostname, uPort, itimeout) {
		m_uRemotePort = 0;
		m_uBytesSoFar = 0;
		m_uFileSize = 0;
		m_pFile = NULL;
		m_pUser = pUser;
		m_bNoDelFile = false;
	}
*/
	virtual ~CDCCSock() {
		if ((m_pFile) && (!m_bNoDelFile)) {
			m_pFile->Close();
			delete m_pFile;
		}
	}

	virtual void ReadData(const char* data, int len);
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Timeout();
	virtual void Connected();
	virtual void Disconnected();
	void SendPacket();
	Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	CFile* OpenFile(bool bWrite = true);
	bool Seek(unsigned int uPos) {
		if (m_pFile) {
			if (m_pFile->Seek(uPos)) {
				m_uBytesSoFar = uPos;
				return true;
			}
		}

		return false;
	}

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
	const CString& GetModuleName() const { return m_sModuleName; }
	CFile* GetFile() { return m_pFile; }
	double GetProgress() const { return ((m_uFileSize) && (m_uBytesSoFar)) ? (double) (((double) m_uBytesSoFar / (double) m_uFileSize) *100.0) : 0; }
	//const CString& GetRemoteIP() const { return m_sRemoteIP; }
	// !Getters
private:
protected:
	CString					m_sRemoteNick;
	CString					m_sRemoteIP;
	CString					m_sFileName;
	CString					m_sLocalFile;
	CString					m_sSendBuf;
	CString					m_sModuleName;
	unsigned short			m_uRemotePort;
	unsigned long			m_uFileSize;
	unsigned long			m_uBytesSoFar;
	bool					m_bSend;
	bool					m_bNoDelFile;
	CFile*					m_pFile;
	CUser*					m_pUser;
};

#endif // !_DCCSOCK_H

