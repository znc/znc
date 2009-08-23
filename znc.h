/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _ZNC_H
#define _ZNC_H

#include "Client.h"
#include "FileUtils.h"
#ifdef _MODULES
#include "Modules.h"
#endif
#include "Socket.h"
#include <map>

using std::map;

class CListener;
class CUser;
class CConnectUserTimer;

class CZNC {
public:
	CZNC();
	~CZNC();

	void DeleteUsers();
	void Loop();
	bool WriteISpoof(CUser* pUser);
	void ReleaseISpoof();
	bool WritePidFile(int iPid);
	bool DeletePidFile();
	CUser* GetUser(const CString& sUser);
	Csock* FindSockByName(const CString& sSockName);
	bool IsHostAllowed(const CString& sHostMask) const;
	// This returns false if there are too many anonymous connections from this ip
	bool AllowConnectionFrom(const CString& sIP) const;
	void InitDirs(const CString& sArgvPath, const CString& sDataDir);
	bool OnBoot();
	CString ExpandConfigPath(const CString& sConfigFile);
	bool WriteNewConfig(const CString& sConfigFile);
	bool WriteConfig();
	bool ParseConfig(const CString& sConfig);
	bool RehashConfig(CString& sError);
	static CString GetVersion();
	static CString GetTag(bool bIncludeVersion = true);
	CString GetUptime() const;
	void ClearVHosts();
	bool AddVHost(const CString& sHost);
	bool RemVHost(const CString& sHost);
	void Broadcast(const CString& sMessage, bool bAdminOnly = false,
			CUser* pSkipUser = NULL, CClient* pSkipClient = NULL);
	void AddBytesRead(unsigned long long u) { m_uBytesRead += u; }
	void AddBytesWritten(unsigned long long u) { m_uBytesWritten += u; }
	unsigned long long BytesRead() const { return m_uBytesRead; }
	unsigned long long BytesWritten() const { return m_uBytesWritten; }

	// Traffic fun
	typedef std::pair<unsigned long long, unsigned long long> TrafficStatsPair;
	typedef std::map<CString, TrafficStatsPair> TrafficStatsMap;
	// Returns a map which maps user names to <traffic in, traffic out>
	// while also providing the traffic of all users together, traffic which
	// couldn't be accounted to any particular user and the total traffic
	// generated through ZNC.
	TrafficStatsMap GetTrafficStats(TrafficStatsPair &Users,
			TrafficStatsPair &ZNC, TrafficStatsPair &Total);

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
	const CSockManager& GetManager() const { return m_Manager; }
#ifdef _MODULES
	CGlobalModules& GetModules() { return *m_pModules; }
#endif
	const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
	const CString& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CDir::MakeDir(m_sCurPath); } return m_sCurPath; }
	const CString& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CDir::MakeDir(m_sHomePath); } return m_sHomePath; }
	const CString& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CDir::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	CString GetConfPath() const;
	CString GetUserPath() const;
	CString GetModPath() const;
	CString GetPemLocation() const { return GetZNCPath() + "/znc.pem"; }
	const CString& GetConfigFile() const { return m_sConfigFile; }
	bool WritePemFile();
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

protected:
	time_t				m_TimeStarted;

	bool				m_bNeedRehash;
	vector<CListener*>		m_vpListeners;
	map<CString,CUser*>		m_msUsers;
	map<CString,CUser*>		m_msDelUsers;
	CSockManager			m_Manager;

	CString					m_sCurPath;
	CString					m_sHomePath;
	CString					m_sZNCPath;

	CString					m_sConfigFile;
	CString					m_sStatusPrefix;
	CString					m_sISpoofFile;
	CString					m_sOrigISpoof;
	CString					m_sISpoofFormat;
	CString					m_sPidFile;
	VCString				m_vsVHosts;
	VCString				m_vsMotd;
	CFile					m_LockFile;
	CFile*					m_pISpoofLockFile;
	unsigned int				m_uiConnectDelay;
	unsigned int				m_uiAnonIPLimit;
#ifdef _MODULES
	CGlobalModules*			m_pModules;
#endif
	unsigned long long		m_uBytesRead;
	unsigned long long		m_uBytesWritten;
	CConnectUserTimer		*m_pConnectUserTimer;
};

class CRealListener : public CZNCSock {
public:
	CRealListener() : CZNCSock() {}
	virtual ~CRealListener() {}

	virtual bool ConnectionFrom(const CString& sHost, unsigned short uPort) {
		DEBUG(GetSockName() << " == ConnectionFrom(" << sHost << ", " << uPort << ")");
		return CZNC::Get().IsHostAllowed(sHost);
	}

	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort) {
		CClient *pClient = new CClient(sHost, uPort);
		if (CZNC::Get().AllowConnectionFrom(sHost)) {
#ifdef _MODULES
			CZNC::Get().GetModules().OnClientConnect(pClient, sHost, uPort);
#endif
		} else {
			pClient->RefuseLogin("Too many anonymous connections from your IP");
#ifdef _MODULES
			CZNC::Get().GetModules().OnFailedLogin("", sHost);
#endif
		}
		return pClient;
	}

	virtual void SockError(int iErrno) {
		DEBUG(GetSockName() << " == SockError(" << strerror(iErrno) << ")");
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

	~CListener() {
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
	CRealListener* GetRealListener() const { return m_pListener; }
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
