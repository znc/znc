#ifndef _ZNC_H
#define _ZNC_H

#include "main.h"
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
	CUser* GetUser(const string& sUser);
	Csock* FindSockByName(const string& sSockName);
	bool Listen();
	bool ParseConfig(const string& sConfigFile);
	bool IsHostAllowed(const string& sHostMask);
	void InitDirs(const string& sArgvPath);
	bool OnBoot();

	// Getters
	TSocketManager<Csock>& GetManager() { return m_Manager; }
	unsigned int GetListenPort() const { return m_uListenPort; }
	const string& GetCurPath() const { if (!CFile::Exists(m_sCurPath)) { CUtils::MakeDir(m_sCurPath); } return m_sCurPath; }
	const string& GetDLPath() const { if (!CFile::Exists(m_sDLPath)) { CUtils::MakeDir(m_sDLPath); } return m_sDLPath; }
	const string& GetModPath() const { if (!CFile::Exists(m_sModPath)) { CUtils::MakeDir(m_sModPath); } return m_sModPath; }
	const string& GetHomePath() const { if (!CFile::Exists(m_sHomePath)) { CUtils::MakeDir(m_sHomePath); } return m_sHomePath; }
	const string& GetZNCPath() const { if (!CFile::Exists(m_sZNCPath)) { CUtils::MakeDir(m_sZNCPath); } return m_sZNCPath; }
	const string& GetDataPath() const { if (!CFile::Exists(m_sDataPath)) { CUtils::MakeDir(m_sDataPath); } return m_sDataPath; }

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

private:
protected:
	unsigned short			m_uListenPort;
	map<string,CUser*>		m_msUsers;
	TSocketManager<Csock>	m_Manager;

	string					m_sCurPath;
	string					m_sDLPath;
	string					m_sModPath;
	string					m_sHomePath;
	string					m_sZNCPath;
	string					m_sDataPath;

	string					m_sISpoofFile;
	string					m_sOrigISpoof;
	string					m_sPidFile;
	bool					m_bISpoofLocked;
	bool					m_bSSL;
	map<string,CUser*>::iterator	m_itUserIter;	// This needs to be reset to m_msUsers.begin() if anything is added or removed to the map
};

#endif // !_ZNC_H
