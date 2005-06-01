#ifndef _ZNC_H
#define _ZNC_H

#include "main.h"
#include "FileUtils.h"
#include "Modules.h"
#include <map>
using std::map;

class CUser;
class CUserSock;

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
	CString GetConfigPath(const CString& sConfigFile);
	bool WriteNewConfig(const CString& sConfig);
	static CString GetTag(bool bIncludeVersion = true);
	CString FindModPath(const CString& sModule) const;

	// Getters
	TSocketManager<Csock>& GetManager() { return m_Manager; }
	CGlobalModules& GetModules() { return *m_pModules; }
	unsigned short GetListenPort() const { return m_uListenPort; }
	const CString& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CUtils::MakeDir(m_sCurPath); } return m_sCurPath; }
	const CString& GetDLPath() const { if (!CFile::Exists(m_sDLPath)) { CUtils::MakeDir(m_sDLPath); } return m_sDLPath; }
	const CString& GetModPath() const { if (!CFile::Exists(m_sModPath)) { CUtils::MakeDir(m_sModPath); } return m_sModPath; }
	const CString& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CUtils::MakeDir(m_sHomePath); } return m_sHomePath; }
	const CString& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CUtils::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	const CString& GetDataPath() const { if (!CFile::Exists(m_sDataPath)) { CUtils::MakeDir(m_sDataPath); } return m_sDataPath; }
	CString GetPemLocation() const { return GetZNCPath() + "/znc.pem"; }

	bool IsSSL() const {
#ifdef HAVE_LIBSSL
		return m_bSSL;
#endif
		return false;
	}
	// !Getters

	// Static allocator
	static CZNC* New() {
		static CZNC* pZNC = new CZNC;
		return pZNC;
	}

	const map<CString,CUser*> & GetUserMap() const { return( m_msUsers ); }

private:
protected:
	unsigned short			m_uListenPort;
	map<CString,CUser*>		m_msUsers;
	TSocketManager<Csock>	m_Manager;

	CString					m_sCurPath;
	CString					m_sDLPath;
	CString					m_sModPath;
	CString					m_sHomePath;
	CString					m_sZNCPath;
	CString					m_sDataPath;

	CString					m_sISpoofFile;
	CString					m_sOrigISpoof;
	CString					m_sISpoofFormat;
	CString					m_sPidFile;
	CLockFile				m_LockFile;
	bool					m_bISpoofLocked;
	bool					m_bSSL;
	map<CString,CUser*>::iterator	m_itUserIter;	// This needs to be reset to m_msUsers.begin() if anything is added or removed to the map
	CGlobalModules*			m_pModules;
};

#endif // !_ZNC_H
