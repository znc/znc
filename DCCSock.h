/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _DCCSOCK_H
#define _DCCSOCK_H

#include "zncconfig.h"
#include "FileUtils.h"

// Forward Declarations
class CUser;
// !Forward Declarations

class CDCCSock : public CZNCSock {
public:
	CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sLocalFile, const CString& sModuleName, unsigned long uFileSize = 0, CFile* pFile = NULL);
	CDCCSock(CUser* pUser, const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sLocalFile, unsigned long uFileSize, const CString& sModuleName);
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
	CString         m_sRemoteNick;
	CString         m_sRemoteIP;
	CString         m_sFileName;
	CString         m_sLocalFile;
	CString         m_sSendBuf;
	CString         m_sModuleName;
	unsigned short  m_uRemotePort;
	unsigned long   m_uFileSize;
	unsigned long   m_uBytesSoFar;
	bool            m_bSend;
	bool            m_bNoDelFile;
	CFile*          m_pFile;
	CUser*          m_pUser;
};

#endif // !_DCCSOCK_H

