#ifndef _ZNC_H
#define _ZNC_H

#include "main.h"
#include "FileUtils.h"
#ifdef _MODULES
#include "Modules.h"
#endif
#include <set>
#include <map>
using std::map;
using std::set;

class CUser;
class CClient;

class CZNC {
public:
	CZNC();
	virtual ~CZNC();

	void DeleteUsers();
	int Loop();
	void ReleaseISpoof();
	bool WritePidFile(int iPid);
	CUser* GetUser(const CString& sUser);
	Csock* FindSockByName(const CString& sSockName);
	bool Listen();
	bool ParseConfig(const CString& sConfig);
	bool IsHostAllowed(const CString& sHostMask);
	void InitDirs(const CString& sArgvPath);
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
	TSocketManager<Csock>& GetManager() { return m_Manager; }
#ifdef _MODULES
	CGlobalModules& GetModules() { return *m_pModules; }
#endif
	unsigned short GetListenPort() const { return m_uListenPort; }
	const CString& GetStatusPrefix() const { return m_sStatusPrefix; }
	const CString& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CUtils::MakeDir(m_sCurPath); } return m_sCurPath; }
	const CString& GetModPath() const { if (!CFile::Exists(m_sModPath)) { CUtils::MakeDir(m_sModPath); } return m_sModPath; }
	const CString& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CUtils::MakeDir(m_sHomePath); } return m_sHomePath; }
	const CString& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CUtils::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	const CString& GetConfPath() const { if (!CFile::Exists(m_sConfPath)) { CUtils::MakeDir(m_sConfPath); } return m_sConfPath; }
	const CString& GetConfBackupPath() const { if (!CFile::Exists(m_sConfBackupPath)) { CUtils::MakeDir(m_sConfBackupPath); } return m_sConfBackupPath; }
	const CString& GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CUtils::MakeDir(m_sUserPath); } return m_sUserPath; }
	CString GetPemLocation() const { return GetZNCPath() + "/znc.pem"; }
	bool WritePemFile();
	const CString& GetISpoofFile() const { return m_sISpoofFile; }
	const CString& GetISpoofFormat() const { return m_sISpoofFormat; }
	const VCString& GetVHosts() const { return m_vsVHosts; }

	bool IsSSL() const {
#ifdef HAVE_LIBSSL
		return m_bSSL;
#endif
		return false;
	}
	// !Getters

	// Static allocator
	static CZNC& Get() {
		static CZNC* pZNC = new CZNC;
		return *pZNC;
	}

	CUser* FindUser(const CString& sUsername);
	bool DeleteUser(const CString& sUsername);
	bool AddUser(CUser* pUser);
	const map<CString,CUser*> & GetUserMap() const { return( m_msUsers ); }

	// Message of the Day
	void SetMotd(const CString& sMessage) { ClearMotd(); AddMotd(sMessage); }
	void AddMotd(const CString& sMessage) { if (!sMessage.empty()) { m_vsMotd.push_back(sMessage); } }
	void ClearMotd() { m_vsMotd.clear(); }
	const VCString& GetMotd() const { return m_vsMotd; }
	// !MOTD

private:
protected:
	unsigned short			m_uListenPort;
	map<CString,CUser*>		m_msUsers;
	set<CUser*>				m_ssDelUsers;
	TSocketManager<Csock>	m_Manager;

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
	bool					m_bISpoofLocked;
	bool					m_bSSL;
	map<CString,CUser*>::iterator	m_itUserIter;	// This needs to be reset to m_msUsers.begin() if anything is added or removed to the map
#ifdef _MODULES
	CGlobalModules*			m_pModules;
#endif
};

#endif // !_ZNC_H
