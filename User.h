#ifndef _USER_H
#define _USER_H

#ifdef _MODULES
#include "Modules.h"
#endif

#include <vector>
#include <set>
#include "Nick.h"

using std::vector;
using std::set;

class CZNC;
class CChan;
class CServer;
class CIRCSock;
class CUserSock;

class CUser {
public:
	CUser(const string& sUserName, CZNC* pZNC);
	virtual ~CUser();

	CChan* FindChan(const string& sName);
	bool AddChan(CChan* pChan);
	bool AddChan(const string& sName, unsigned int uBufferCount = 50);
	bool DelChan(const string& sName);
	bool AddServer(const string& sName);
	bool AddServer(const string& sName, unsigned short uPort, const string& sPass = "", bool bSSL = false);
	CServer* GetNextServer();
	bool CheckPass(const string& sPass);
	bool AddAllowedHost(const string& sHostMask);
	bool IsHostAllowed(const string& sHostMask);

#ifdef _MODULES
	// Modules
	CModules& GetModules() { return m_Modules; }
	// !Modules
#endif

	bool OnBoot();
	bool IsUserAttached();

	bool PutIRC(const string& sLine);
	bool PutUser(const string& sLine);
	bool PutStatus(const string& sLine);
	bool PutModule(const string& sModule, const string& sLine);

	string GetLocalIP();

	bool SendFile(const string& sRemoteNick, const string& sFileName, const string& sModuleName = "");
	bool GetFile(const string& sRemoteNick, const string& sRemoteIP, unsigned short uRemotePort, const string& sFileName, unsigned long uFileSize, const string& sModuleName = "");
	bool ResumeFile(const string& sRemoteNick, unsigned short uPort, unsigned long uFileSize);
	string GetCurNick();

	// Setters
	void SetNick(const string& s);
	void SetAltNick(const string& s);
	void SetIdent(const string& s);
	void SetRealName(const string& s);
	void SetVHost(const string& s);
	void SetPass(const string& s, bool bHashed);
	void SetUseClientIP(bool b);
	void SetKeepNick(bool b);
	void SetDenyLoadMod(bool b);
	bool SetStatusPrefix(const string& s);
	void SetDefaultChanModes(const string& s);
	void SetIRCNick(const CNick& n);
	void SetIRCServer(const string& s);
	void SetQuitMsg(const string& s);
	// !Setters

	// Getters
	CUserSock* GetUserSock();
	CIRCSock* GetIRCSock();
	TSocketManager<Csock>* GetManager();
	CZNC* GetZNC();
	const string& GetUserName() const;
	const string& GetNick() const;
	const string& GetAltNick() const;
	const string& GetIdent() const;
	const string& GetRealName() const;
	const string& GetVHost() const;
	const string& GetPass() const;

	const string& GetCurPath() const;
	const string& GetDLPath() const;
	const string& GetModPath() const;
	const string& GetHomePath() const;
	const string& GetDataPath() const;
	string GetPemLocation() const;

	bool UseClientIP() const;
	bool KeepNick() const;
	bool DenyLoadMod() const;
	const string& GetStatusPrefix() const;
	const string& GetDefaultChanModes() const;
	const vector<CChan*>& GetChans() const;
	const CNick& GetIRCNick() const;
	const string& GetIRCServer() const;
	const string& GetQuitMsg() const;
	// !Getters
private:
protected:
	CZNC*			m_pZNC;

	string			m_sUserName;
	string			m_sNick;
	string			m_sAltNick;
	string			m_sIdent;
	string			m_sRealName;
	string			m_sVHost;
	string			m_sPass;
	string			m_sStatusPrefix;
	string			m_sDefaultChanModes;
	CNick			m_IRCNick;
	string			m_sIRCServer;
	string			m_sQuitMsg;

	bool				m_bPassHashed;
	bool				m_bUseClientIP;
	bool				m_bKeepNick;
	bool				m_bDenyLoadMod;
	vector<CServer*>	m_vServers;
	vector<CChan*>		m_vChans;
	set<string>			m_ssAllowedHosts;
	unsigned int		m_uServerIdx;

#ifdef _MODULES
	CModules		m_Modules;
#endif
};

#endif // !_USER_H
