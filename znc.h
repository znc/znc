//! @author prozac@rottenboy.com

#ifndef _ZNC_H
#define _ZNC_H

#include "main.h"
#include "FileUtils.h"
#include "Client.h"
#ifdef _MODULES
#include "Modules.h"
#endif
#include <set>
#include <map>
using std::map;
using std::set;

class CUser;
class CClient;
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
		return( ListenRand( sSockName, "", isSSL, iMaxConns, pcSock, iTimeout, bIsIPv6 ) );
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

class CZNC {
public:
	CZNC();
	virtual ~CZNC();

	void DeleteUsers();
	int Loop();
	bool WriteISpoof(CUser* pUser);
	void ReleaseISpoof();
	bool WritePidFile(int iPid);
	CUser* GetUser(const CString& sUser);
	Csock* FindSockByName(const CString& sSockName);
	bool ParseConfig(const CString& sConfig);
	bool IsHostAllowed(const CString& sHostMask);
	void InitDirs(const CString& sArgvPath, const CString& sDataDir);
	bool OnBoot();
	CString ExpandConfigPath(const CString& sConfigFile);
	bool WriteNewConfig(const CString& sConfig);
	bool WriteConfig();
	static CString GetTag(bool bIncludeVersion = true);
	CString FindModPath(const CString& sModule) const;
	void ClearVHosts();
	bool AddVHost(const CString& sHost);
	bool RemVHost(const CString& sHost);
	void Broadcast(const CString& sMessage, CUser* pUser = NULL);

	// Setters
	void SetStatusPrefix(const CString& s) { m_sStatusPrefix = (s.empty()) ? "*" : s; }
	void SetISpoofFile(const CString& s) { m_sISpoofFile = s; }
	void SetISpoofFormat(const CString& s) { m_sISpoofFormat = (s.empty()) ? "global { reply \"%\" }" : s; }
	// !Setters

	// Getters
	CSockManager& GetManager() { return m_Manager; }
#ifdef _MODULES
	CGlobalModules& GetModules() { return *m_pModules; }
#endif
	const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
	const CString& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CUtils::MakeDir(m_sCurPath); } return m_sCurPath; }
	const CString& GetModPath() const { if (!CFile::Exists(m_sModPath)) { CUtils::MakeDir(m_sModPath); } return m_sModPath; }
	const CString& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CUtils::MakeDir(m_sHomePath); } return m_sHomePath; }
	const CString& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CUtils::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	const CString& GetConfPath() const { if (!CFile::Exists(m_sConfPath)) { CUtils::MakeDir(m_sConfPath); } return m_sConfPath; }
	const CString& GetConfBackupPath() const { if (!CFile::Exists(m_sConfBackupPath)) { CUtils::MakeDir(m_sConfBackupPath); } return m_sConfBackupPath; }
	const CString& GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CUtils::MakeDir(m_sUserPath); } return m_sUserPath; }
	CString GetPemLocation() const { return GetZNCPath() + "/znc.pem"; }
	const CString& GetConfigFile() const { return m_sConfigFile; }
	bool WritePemFile( bool bEncPem = false );
	const CString& GetISpoofFile() const { return m_sISpoofFile; }
	const CString& GetISpoofFormat() const { return m_sISpoofFormat; }
	const VCString& GetVHosts() const { return m_vsVHosts; }
	const vector<CListener*>& GetListeners() const { return m_vpListeners; }
	// !Getters

	// Static allocator
	static CZNC& Get();
	CUser* FindUser(const CString& sUsername);
	bool DeleteUser(const CString& sUsername);
	bool AddUser(CUser* pUser, CString& sErrorRet);
	const map<CString,CUser*> & GetUserMap() const { return( m_msUsers ); }

	// Message of the Day
	void SetMotd(const CString& sMessage) { ClearMotd(); AddMotd(sMessage); }
	void AddMotd(const CString& sMessage) { if (!sMessage.empty()) { m_vsMotd.push_back(sMessage); } }
	void ClearMotd() { m_vsMotd.clear(); }
	const VCString& GetMotd() const { return m_vsMotd; }
	// !MOTD

private:
protected:
	vector<CListener*>		m_vpListeners;
	map<CString,CUser*>		m_msUsers;
	set<CUser*>				m_ssDelUsers;
	CSockManager			m_Manager;

	CString					m_sCurPath;
	CString					m_sModPath;
	CString					m_sHomePath;
	CString					m_sZNCPath;
	CString					m_sConfPath;
	CString					m_sConfBackupPath;
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
	map<CString,CUser*>::iterator	m_itUserIter;	// This needs to be reset to m_msUsers.begin() if anything is added or removed to the map
#ifdef _MODULES
	CGlobalModules*			m_pModules;
#endif
};

class CListener {
public:
	CListener(unsigned short uPort, const CString& sBindHost, bool bSSL, bool bIPV6) {
		m_uPort = uPort;
		m_sBindHost = sBindHost;
		m_bSSL = bSSL;
		m_bIPV6 = bIPV6;
	}

	virtual ~CListener() {}

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

	bool Listen() const {
		if (!m_uPort) {
			return false;
		}

		CClient* pClient = new CClient;

		bool bSSL = false;
#ifdef HAVE_LIBSSL
		if (IsSSL()) {
			bSSL = true;
			pClient->SetPemLocation(CZNC::Get().GetPemLocation());
		}
#endif

		return CZNC::Get().GetManager().ListenHost(m_uPort, "_LISTENER", m_sBindHost, bSSL, SOMAXCONN, pClient, 0, m_bIPV6);
	}
private:
protected:
	bool			m_bSSL;
	bool			m_bIPV6;
	unsigned short	m_uPort;
	CString			m_sBindHost;
};

#endif // !_ZNC_H
