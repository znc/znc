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
	CUser(const CString& sUserName, CZNC* pZNC);
	virtual ~CUser();

	CChan* FindChan(const CString& sName);
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName);
	bool DelChan(const CString& sName);
	CServer* FindServer(const CString& sName);
	bool DelServer(const CString& sName);
	bool AddServer(const CString& sName);
	bool AddServer(const CString& sName, unsigned short uPort, const CString& sPass = "", bool bSSL = false);
	CServer* GetNextServer();
	bool CheckPass(const CString& sPass);
	bool AddAllowedHost(const CString& sHostMask);
	bool IsHostAllowed(const CString& sHostMask);
	bool IsValid(CString& sErrMsg);
	static bool IsValidUserName(const CString& sUserName);

#ifdef _MODULES
	// Modules
	CModules& GetModules() { return m_Modules; }
	// !Modules
#endif

	bool OnBoot();
	bool IsUserAttached();

	bool PutIRC(const CString& sLine);
	bool PutUser(const CString& sLine);
	bool PutStatus(const CString& sLine);
	bool PutModule(const CString& sModule, const CString& sLine);

	CString GetLocalIP();

	bool SendFile(const CString& sRemoteNick, const CString& sFileName, const CString& sModuleName = "");
	bool GetFile(const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sFileName, unsigned long uFileSize, const CString& sModuleName = "");
	bool ResumeFile(const CString& sRemoteNick, unsigned short uPort, unsigned long uFileSize);
	CString GetCurNick();

	// Setters
	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);
	void SetVHost(const CString& s);
	void SetPass(const CString& s, bool bHashed);
	void SetUseClientIP(bool b);
	void SetKeepNick(bool b);
	void SetDenyLoadMod(bool b);
	bool SetStatusPrefix(const CString& s);
	void SetDefaultChanModes(const CString& s);
	void SetIRCNick(const CNick& n);
	void SetIRCServer(const CString& s);
	void SetQuitMsg(const CString& s);
	void SetVersionReply(const CString& s);
	void SetBufferCount(unsigned int u);
	void SetKeepBuffer(bool b);
	// !Setters

	// Getters
	CUserSock* GetUserSock();
	CIRCSock* GetIRCSock();
	TSocketManager<Csock>* GetManager();
	CZNC* GetZNC();
	const CString& GetUserName() const;
	const CString& GetNick() const;
	const CString& GetAltNick() const;
	const CString& GetIdent() const;
	const CString& GetRealName() const;
	const CString& GetVHost() const;
	const CString& GetPass() const;

	const CString& GetCurPath() const;
	const CString& GetDLPath() const;
	const CString& GetModPath() const;
	const CString& GetHomePath() const;
	const CString& GetDataPath() const;
	CString GetPemLocation() const;

	bool UseClientIP() const;
	bool KeepNick() const;
	bool DenyLoadMod() const;
	const CString& GetStatusPrefix() const;
	const CString& GetDefaultChanModes() const;
	const vector<CChan*>& GetChans() const;
	const vector<CServer*>& GetServers() const;
	const CNick& GetIRCNick() const;
	const CString& GetIRCServer() const;
	CString GetQuitMsg() const;
	CString GetVersionReply() const;
	unsigned int GetBufferCount() const;
	bool KeepBuffer() const;
	// !Getters
private:
protected:
	CZNC*			m_pZNC;

	CString			m_sUserName;
	CString			m_sNick;
	CString			m_sAltNick;
	CString			m_sIdent;
	CString			m_sRealName;
	CString			m_sVHost;
	CString			m_sPass;
	CString			m_sStatusPrefix;
	CString			m_sDefaultChanModes;
	CNick			m_IRCNick;
	CString			m_sIRCServer;
	CString			m_sQuitMsg;
	CString			m_sVersionReply;

	bool				m_bPassHashed;
	bool				m_bUseClientIP;
	bool				m_bKeepNick;
	bool				m_bDenyLoadMod;
	bool				m_bKeepBuffer;

	vector<CServer*>	m_vServers;
	vector<CChan*>		m_vChans;
	set<CString>		m_ssAllowedHosts;
	unsigned int		m_uServerIdx;
	unsigned int		m_uBufferCount;

#ifdef _MODULES
	CModules		m_Modules;
#endif
};

#endif // !_USER_H
