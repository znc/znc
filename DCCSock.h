#ifndef _DCCSOCK_H
#define _DCCSOCK_H

#include "main.h"
#include "Utils.h"
#include "User.h"

class CDCCSock : public Csock {
public:
	CDCCSock(CUser* pUser, const string& sRemoteNick, const string& sLocalFile, const string& sModuleName, unsigned long uFileSize = 0, CFile* pFile = NULL) : Csock() {
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

	CDCCSock(CUser* pUser, const string& sRemoteNick, const string& sRemoteIP, unsigned short uRemotePort, const string& sLocalFile, unsigned long uFileSize, const string& sModuleName) : Csock() {
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

/*	CDCCSock(CUser* pUser, const string& sHostname, int iport, int itimeout = 60) : Csock(sHostname, iport, itimeout) {
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
	Csock* GetSockObj(const string& sHost, int iPort);
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
	void SetRemoteIP(const string& s) { m_sRemoteIP = s; }
	void SetRemoteNick(const string& s) { m_sRemoteNick = s; }
	void SetFileName(const string& s) { m_sFileName = s; }
	void SetFileOffset(unsigned long u) { m_uBytesSoFar = u; }
	// !Setters

	// Getters
	unsigned short GetUserPort() const { return m_uRemotePort; }
	const string& GetRemoteNick() const { return m_sRemoteNick; }
	const string& GetFileName() const { return m_sFileName; }
	const string& GetLocalFile() const { return m_sLocalFile; }
	const string& GetModuleName() const { return m_sModuleName; }
	CFile* GetFile() { return m_pFile; }
	double GetProgress() const { return ((m_uFileSize) && (m_uBytesSoFar)) ? (double) (((double) m_uBytesSoFar / (double) m_uFileSize) *100.0) : 0; }
	//const string& GetRemoteIP() const { return m_sRemoteIP; }
	// !Getters
private:
protected:
	string					m_sRemoteNick;
	string					m_sRemoteIP;
	string					m_sFileName;
	string					m_sLocalFile;
	string					m_sSendBuf;
	string					m_sModuleName;
	unsigned short			m_uRemotePort;
	unsigned long			m_uFileSize;
	unsigned long			m_uBytesSoFar;
	bool					m_bSend;
	bool					m_bNoDelFile;
	CFile*					m_pFile;
	CUser*					m_pUser;
};

#endif // !_DCCSOCK_H

