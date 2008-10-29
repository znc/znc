/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _ZNC_H
#define _ZNC_H

#include "Client.h"
#include "Csocket.h"
#include "FileUtils.h"
#ifdef _MODULES
#include "Modules.h"
#endif
#include <map>

using std::map;

class CUser;
class CListener;

class CSockManager : public TSocketManager<Csock> {
public:
	CSockManager() : TSocketManager<Csock>() {}
	virtual ~CSockManager() {}

	virtual bool ListenHost(u_short iPort, const CString& sSockName, const CString& sBindHost, int isSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		CSListener L(iPort, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(isSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		if (bIsIPv6) {
			L.SetAFRequire(CSSockAddr::RAF_INET6);
		}
#endif

		return Listen(L, pcSock);
	}

	virtual bool ListenAll(u_short iPort, const CString& sSockName, int isSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return ListenHost(iPort, sSockName, "", isSSL, iMaxConns, pcSock, iTimeout, bIsIPv6);
	}

	virtual u_short ListenRand(const CString& sSockName, const CString& sBindHost, int isSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		unsigned short uPort = 0;
		CSListener L(0, sBindHost);

		L.SetSockName(sSockName);
		L.SetIsSSL(isSSL);
		L.SetTimeout(iTimeout);
		L.SetMaxConns(iMaxConns);

#ifdef HAVE_IPV6
		if (bIsIPv6) {
			L.SetAFRequire(CSSockAddr::RAF_INET6);
		}
#endif

		Listen(L, pcSock, &uPort);

		return uPort;
	}

	virtual u_short ListenAllRand(const CString& sSockName, int isSSL = false, int iMaxConns = SOMAXCONN, Csock *pcSock = NULL, u_int iTimeout = 0, bool bIsIPv6 = false) {
		return(ListenRand(sSockName, "", isSSL, iMaxConns, pcSock, iTimeout, bIsIPv6));
	}

	virtual bool Connect(const CString& sHostname, u_short iPort , const CString& sSockName, int iTimeout = 60, bool isSSL = false, const CString& sBindHost = "", Csock *pcSock = NULL) {
		CSConnection C(sHostname, iPort, iTimeout);

		C.SetSockName(sSockName);
		C.SetIsSSL(isSSL);
		C.SetBindHost(sBindHost);

		return TSocketManager<Csock>::Connect(C, pcSock);
	}
private:
protected:
};

class CConnectUserTimer;

class CZNC {
public:
	CZNC();
	virtual ~CZNC();

	void DeleteUsers();
	int Loop();
	bool WriteISpoof(CUser* pUser);
	void ReleaseISpoof();
	bool WritePidFile(int iPid);
	bool DeletePidFile();
	CUser* GetUser(const CString& sUser);
	Csock* FindSockByName(const CString& sSockName);
	bool IsHostAllowed(const CString& sHostMask) const;
	void InitDirs(const CString& sArgvPath, const CString& sDataDir);
	bool OnBoot();
	CString ExpandConfigPath(const CString& sConfigFile);
	bool WriteNewConfig(CString& sConfigFile);
	bool WriteConfig();
	bool ParseConfig(const CString& sConfig);
	bool RehashConfig(CString& sError);
	static CString GetVersion();
	static CString GetTag(bool bIncludeVersion = true);
	CString GetUptime() const;
	// This returns the path to the .so and to the data dir
	// which is where static data (webadmin skins) are saved
	bool FindModPath(const CString& sModule, CString& sModPath,
			CString& sDataPath) const;
	void ClearVHosts();
	bool AddVHost(const CString& sHost);
	bool RemVHost(const CString& sHost);
	void Broadcast(const CString& sMessage, bool bAdminOnly = false,
			CUser* pSkipUser = NULL, CClient* pSkipClient = NULL);
	void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
	void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }
	void UpdateTrafficStats();

	// Authenticate a user.
	// The result is passed back via callbacks to CAuthBase.
	// CSmartPtr handles freeing this pointer!
	void AuthUser(CSmartPtr<CAuthBase> AuthClass);

	// Setters
	void SetNeedRehash(bool b) { m_bNeedRehash = b; }
	void SetStatusPrefix(const CString& s) { m_sStatusPrefix = (s.empty()) ? "*" : s; }
	void SetISpoofFile(const CString& s) { m_sISpoofFile = s; }
	void SetISpoofFormat(const CString& s) { m_sISpoofFormat = (s.empty()) ? "global { reply \"%\" }" : s; }
	// !Setters

	// Getters
	bool GetNeedRehash() const { return m_bNeedRehash; }
	CSockManager& GetManager() { return m_Manager; }
#ifdef _MODULES
	CGlobalModules& GetModules() { return *m_pModules; }
#endif
	const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
	const CString& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CDir::MakeDir(m_sCurPath); } return m_sCurPath; }
	const CString& GetModPath() const { if (!CFile::Exists(m_sModPath)) { CDir::MakeDir(m_sModPath); } return m_sModPath; }
	const CString& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CDir::MakeDir(m_sHomePath); } return m_sHomePath; }
	const CString& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CDir::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	const CString& GetConfPath() const { if (!CFile::Exists(m_sConfPath)) { CDir::MakeDir(m_sConfPath); } return m_sConfPath; }
	const CString& GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CDir::MakeDir(m_sUserPath); } return m_sUserPath; }
	CString GetPemLocation() const { return GetZNCPath() + "/znc.pem"; }
	const CString& GetConfigFile() const { return m_sConfigFile; }
	bool WritePemFile(bool bEncPem = false);
	const CString& GetISpoofFile() const { return m_sISpoofFile; }
	const CString& GetISpoofFormat() const { return m_sISpoofFormat; }
	const VCString& GetVHosts() const { return m_vsVHosts; }
	const vector<CListener*>& GetListeners() const { return m_vpListeners; }
	time_t TimeStarted() const { return m_TimeStarted; }
	// !Getters

	// Static allocator
	static CZNC& Get();
	CUser* FindUser(const CString& sUsername);
	bool DeleteUser(const CString& sUsername);
	bool AddUser(CUser* pUser, CString& sErrorRet);
	const map<CString,CUser*> & GetUserMap() const { return(m_msUsers); }

	// Message of the Day
	void SetMotd(const CString& sMessage) { ClearMotd(); AddMotd(sMessage); }
	void AddMotd(const CString& sMessage) { if (!sMessage.empty()) { m_vsMotd.push_back(sMessage); } }
	void ClearMotd() { m_vsMotd.clear(); }
	const VCString& GetMotd() const { return m_vsMotd; }
	// !MOTD

	// Create a CIRCSocket. Return false if user cant connect
	bool ConnectUser(CUser *pUser);
	// This creates a CConnectUserTimer if we haven't got one yet
	void EnableConnectUser();
	void DisableConnectUser();

private:
	bool DoRehash(CString& sError);
	// Returns true if something was done
	bool HandleUserDeletion();
	// Backup znc.conf
	bool BackupConfig() const;

protected:
	time_t				m_TimeStarted;

	bool				m_bNeedRehash;
	vector<CListener*>		m_vpListeners;
	map<CString,CUser*>		m_msUsers;
	map<CString,CUser*>		m_msDelUsers;
	CSockManager			m_Manager;

	CString					m_sCurPath;
	CString					m_sModPath;
	CString					m_sHomePath;
	CString					m_sZNCPath;
	CString					m_sConfPath;
	CString					m_sUserPath;

	CString					m_sConfigFile;
	CString					m_sStatusPrefix;
	CString					m_sISpoofFile;
	CString					m_sOrigISpoof;
	CString					m_sISpoofFormat;
	CString					m_sPidFile;
	VCString				m_vsVHosts;
	VCString				m_vsMotd;
	CLockFile				m_LockFile;
	CLockFile*				m_pISpoofLockFile;
	uint					m_uiConnectDelay;
#ifdef _MODULES
	CGlobalModules*			m_pModules;
#endif
	unsigned long long		m_uBytesRead;
	unsigned long long		m_uBytesWritten;
	CConnectUserTimer		*m_pConnectUserTimer;
};

class CRealListener : public Csock {
public:
	CRealListener() : Csock() {}
	~CRealListener() {}

	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort) {
		DEBUG_ONLY(cout << GetSockName() << " == ConnectionFrom(" << sHost << ", " << uPort << ")" << endl);
		return CZNC::Get().IsHostAllowed(sHost);
	}

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort) {
		CClient *pClient = new CClient(sHost, uPort);
		CZNC::Get().GetModules().OnClientConnect(pClient, sHost, uPort);
		return pClient;
	}

	virtual void SockError(int iErrno) {
		DEBUG_ONLY(cout << GetSockName() << " == SockError(" << strerror(iErrno) << ")" << endl);
		if (iErrno == EMFILE) {
			// We have too many open fds, let's close this listening port to be able to continue
			// to work, next rehash will (try to) reopen it.
			Close();
		}
	}
};

class CListener {
public:
	CListener(unsigned short uPort, const CString& sBindHost, bool bSSL, bool bIPV6) {
		m_uPort = uPort;
		m_sBindHost = sBindHost;
		m_bSSL = bSSL;
		m_bIPV6 = bIPV6;
		m_pListener = NULL;
	}

	virtual ~CListener() {
		if (m_pListener)
			CZNC::Get().GetManager().DelSockByAddr(m_pListener);
	}

	// Setters
	void SetSSL(bool b) { m_bSSL = b; }
	void SetIPV6(bool b) { m_bIPV6 = b; }
	void SetPort(unsigned short u) { m_uPort = u; }
	void SetBindHost(const CString& s) { m_sBindHost = s; }
	// !Setters

	// Getters
	bool IsSSL() const { return m_bSSL; }
	bool IsIPV6() const { return m_bIPV6; }
	unsigned short GetPort() const { return m_uPort; }
	const CString& GetBindHost() const { return m_sBindHost; }
	// !Getters

	bool Listen() {
		if (!m_uPort || m_pListener) {
			return false;
		}

		m_pListener = new CRealListener;

		bool bSSL = false;
#ifdef HAVE_LIBSSL
		if (IsSSL()) {
			bSSL = true;
			m_pListener->SetPemLocation(CZNC::Get().GetPemLocation());
		}
#endif

		return CZNC::Get().GetManager().ListenHost(m_uPort, "_LISTENER", m_sBindHost, bSSL, SOMAXCONN,
				m_pListener, 0, m_bIPV6);
	}
private:
protected:
	bool			m_bSSL;
	bool			m_bIPV6;
	unsigned short	m_uPort;
	CString			m_sBindHost;
	CRealListener*		m_pListener;
};

#endif // !_ZNC_H
