#ifndef _DCCBOUNCE_H
#define _DCCBOUNCE_H

#include "main.h"
#include "Utils.h"
#include "User.h"

class CDCCBounce : public Csock {
public:
	CDCCBounce(CUser* pUser, unsigned long uLongIP, unsigned short uPort, const string& sFileName, const string& sRemoteNick, const string& sRemoteIP, string sLocalIP, bool bIsChat = false) : Csock() {
		m_uRemotePort = uPort;
		m_sConnectIP = CUtils::GetIP(uLongIP);
		m_sRemoteIP = sRemoteIP;
		m_sFileName = sFileName;
		m_sRemoteNick = sRemoteNick;
		m_pUser = pUser;
		m_pManager = pUser->GetManager();
		m_bIsChat = bIsChat;
		m_sLocalIP = sLocalIP;
		m_pPeer = NULL;
		m_bIsRemote = false;

		if (bIsChat) {
			EnableReadLine();
		}
	}

	CDCCBounce(const string& sHostname, int iport, CUser* pUser, const string& sRemoteNick, const string& sRemoteIP, const string& sFileName, int itimeout = 60, bool bIsChat = false) : Csock(sHostname, iport, itimeout) {
		m_uRemotePort = 0;
		m_bIsChat = bIsChat;
		m_pManager = pUser->GetManager();
		m_pUser = pUser;
		m_pPeer = NULL;
		m_sRemoteNick = sRemoteNick;
		m_sFileName = sFileName;
		m_sRemoteIP = sRemoteIP;
		m_bIsRemote = false;

		if (bIsChat) {
			EnableReadLine();
		}
	}
	virtual ~CDCCBounce() {
		if (m_pPeer) {
			m_pPeer->Shutdown();
			m_pPeer = NULL;
		}
	}

	static unsigned short DCCRequest(const string& sNick, unsigned long uLongIP, unsigned short uPort, const string& sFileName, bool bIsChat, CUser* pUser, const string& sLocalIP, const string& sRemoteIP);

	void ReadLine(const string& sData);
	virtual void ReadData(const char* data, int len);
	virtual void Timeout();
	virtual void ConnectionRefused();
	virtual void SockError(int iErrno);
	virtual void Connected();
	virtual void Disconnected();
	void Shutdown();
	Csock* GetSockObj(const string& sHost, int iPort);
	void PutServ(const string& sLine);
	void PutPeer(const string& sLine);
	bool IsPeerConnected() { return (m_pPeer) ? m_pPeer->IsConnected() : false; }

	// Setters
	void SetPeer(CDCCBounce* p) { m_pPeer = p; }
	void SetRemoteIP(const string& s) { m_sRemoteIP = s; }
	void SetRemoteNick(const string& s) { m_sRemoteNick = s; }
	void SetManager(TSocketManager<Csock>* p) { m_pManager = p; }
	void SetRemote(bool b) { m_bIsRemote = b; }
	// !Setters

	// Getters
	unsigned short GetUserPort() const { return m_uRemotePort; }
	const string& GetRemoteIP() const { return m_sRemoteIP; }
	const string& GetRemoteNick() const { return m_sRemoteNick; }
	const string& GetFileName() const { return m_sFileName; }
	CDCCBounce* GetPeer() const { return m_pPeer; }
	TSocketManager<Csock>* GetManager() const { return m_pManager; }
	bool IsRemote() { return m_bIsRemote; }
	// !Getters
private:
protected:
	string					m_sRemoteNick;
	string					m_sRemoteIP;
	string					m_sConnectIP;
	string					m_sLocalIP;
	string					m_sFileName;
	CUser*					m_pUser;
	CDCCBounce*				m_pPeer;
	TSocketManager<Csock>*	m_pManager;
	unsigned short			m_uRemotePort;
	bool					m_bIsChat;
	bool					m_bIsRemote;
};

#endif // !_DCCBOUNCE_H

