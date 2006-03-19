#ifndef _USER_H
#define _USER_H

#include "main.h"
#ifdef _MODULES
#include "Modules.h"
#endif

#include <vector>
#include <set>
#include "Nick.h"
#include "FileUtils.h"
#include "Buffer.h"

using std::vector;
using std::set;

class CZNC;
class CChan;
class CServer;
class CIRCSock;
class CClient;
class CBackNickTimer;
class CAwayNickTimer;
class CKeepNickTimer;
class CJoinTimer;
class CMiscTimer;

class CUser {
public:
	CUser(const CString& sUserName);
	virtual ~CUser();

	bool PrintLine(CFile& File, const CString& sName, const CString& sValue);
	bool WriteConfig(CFile& File);
	CChan* FindChan(const CString& sName) const;
	bool AddChan(CChan* pChan);
	bool AddChan(const CString& sName, bool bInConfig);
	bool DelChan(const CString& sName);
	void JoinChans();
	CServer* FindServer(const CString& sName);
	bool DelServer(const CString& sName);
	bool AddServer(const CString& sName, bool bIPV6 = false);
	bool AddServer(const CString& sName, unsigned short uPort, const CString& sPass = "", bool bSSL = false, bool bIPV6 = false);
	CServer* GetNextServer();
	CServer* GetCurrentServer();
	bool CheckPass(const CString& sPass);
	bool AddAllowedHost(const CString& sHostMask);
	bool IsHostAllowed(const CString& sHostMask);
	bool IsValid(CString& sErrMsg, bool bSkipPass = false) const;
	static bool IsValidUserName(const CString& sUserName);
	bool IsLastServer();
	bool ConnectPaused();

	void DelClients();
#ifdef _MODULES
	void DelModules();
#endif

#ifdef _MODULES
	// Modules
	CModules& GetModules() { return *m_pModules; }
	const CModules& GetModules() const { return *m_pModules; }
	// !Modules
#endif

	// Buffers
	void AddRawBuffer(const CString& sPre, const CString& sPost) { m_RawBuffer.AddLine(sPre, sPost); }
	void AddMotdBuffer(const CString& sPre, const CString& sPost) { m_MotdBuffer.AddLine(sPre, sPost); }
	void AddQueryBuffer(const CString& sPre, const CString& sPost) { m_QueryBuffer.AddLine(sPre, sPost); }
	void ClearRawBuffer() { m_RawBuffer.Clear(); }
	void ClearMotdBuffer() { m_MotdBuffer.Clear(); }
	void ClearQueryBuffer() { m_QueryBuffer.Clear(); }
	// !Buffers
	bool OnBoot();

	bool PutIRC(const CString& sLine);
	bool PutUser(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatus(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutStatusNotice(const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);
	bool PutModule(const CString& sModule, const CString& sLine, CClient* pClient = NULL, CClient* pSkipClient = NULL);

	bool IsUserAttached() { return (m_vClients.size() > 0); }
	void UserConnected(CClient* pClient);
	void UserDisconnected(CClient* pClient);

	CString GetLocalIP();
	bool IsIRCConnected() { return m_bIRCConnected; }
	void IRCConnected(CIRCSock* pIRCSock);
	void IRCDisconnected();

	bool SendFile(const CString& sRemoteNick, const CString& sFileName, const CString& sModuleName = "");
	bool GetFile(const CString& sRemoteNick, const CString& sRemoteIP, unsigned short uRemotePort, const CString& sFileName, unsigned long uFileSize, const CString& sModuleName = "");
	bool ResumeFile(const CString& sRemoteNick, unsigned short uPort, unsigned long uFileSize);
	CString GetCurNick();
	bool Clone(const CUser& User, CString& sErrorRet);
	void StartAwayNickTimer();
	void StartBackNickTimer();
	void DelAwayNickTimer();
	void DelBackNickTimer();
	void BounceAllClients();

	// Setters
	void SetUserName(const CString& s);
	void SetNick(const CString& s);
	void SetAltNick(const CString& s);
	void SetAwaySuffix(const CString& s);
	void SetIdent(const CString& s);
	void SetRealName(const CString& s);
	void SetVHost(const CString& s);
	void SetPass(const CString& s, bool bHashed);
	void SetBounceDCCs(bool b);
	void SetMultiClients(bool b);
	void SetUseClientIP(bool b);
	void SetKeepNick(bool b);
	void SetDenyLoadMod(bool b);
	void SetAdmin(bool b);
	bool SetStatusPrefix(const CString& s);
	void SetDefaultChanModes(const CString& s);
	void SetIRCNick(const CNick& n);
	void SetIRCServer(const CString& s);
	void SetQuitMsg(const CString& s);
	bool AddCTCPReply(const CString& sCTCP, const CString& sReply);
	void SetBufferCount(unsigned int u);
	void SetKeepBuffer(bool b);
	void SetAutoCycle(bool b);
	void SetChanPrefixes(const CString& s) { m_sChanPrefixes = (s.empty()) ? "#&" : s; }
	void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }
	// !Setters

	// Getters
	vector<CClient*>& GetClients() { return m_vClients; }
	CIRCSock* GetIRCSock();
	const CString& GetUserName() const;
	const CString& GetNick() const;
	const CString& GetAltNick() const;
	const CString& GetAwaySuffix() const;
	const CString& GetIdent() const;
	const CString& GetRealName() const;
	const CString& GetVHost() const;
	const CString& GetPass() const;
	bool IsPassHashed() const;
	const set<CString>& GetAllowedHosts() const;

	const CString& GetChanPrefixes() const { return m_sChanPrefixes; }
	bool IsChan(const CString& sChan) const { return (sChan.size() && GetChanPrefixes().find(sChan[0]) != CString::npos); }

	const CString& GetUserPath() const { if (!CFile::Exists(m_sUserPath)) { CUtils::MakeDir(m_sUserPath); } return m_sUserPath; }
	const CString& GetDLPath() const { if (!CFile::Exists(m_sDLPath)) { CUtils::MakeDir(m_sDLPath); } return m_sDLPath; }

	bool UseClientIP() const;
	bool GetKeepNick() const;
	bool DenyLoadMod() const;
	bool IsAdmin() const;
	bool BounceDCCs() const;
	bool MultiClients() const;
	const CString& GetStatusPrefix() const;
	const CString& GetDefaultChanModes() const;
	const vector<CChan*>& GetChans() const;
	const vector<CServer*>& GetServers() const;
	const CNick& GetIRCNick() const;
	const CString& GetIRCServer() const;
	CString GetQuitMsg() const;
	const MCString& GetCTCPReplies() const;
	unsigned int GetBufferCount() const;
	bool KeepBuffer() const;
	bool AutoCycle() const;
	bool IsBeingDeleted() const { return m_bBeingDeleted; }
	// !Getters
private:
protected:
	time_t			m_uConnectTime;
	CString			m_sUserName;
	CString			m_sNick;
	CString			m_sAltNick;
	CString			m_sAwaySuffix;
	CString			m_sIdent;
	CString			m_sRealName;
	CString			m_sVHost;
	CString			m_sPass;
	CString			m_sStatusPrefix;
	CString			m_sDefaultChanModes;
	CString			m_sChanPrefixes;
	CNick			m_IRCNick;
	CString			m_sIRCServer;
	CString			m_sQuitMsg;
	MCString		m_mssCTCPReplies;

	// Paths
	CString			m_sUserPath;
	CString			m_sDLPath;
	// !Paths

	CBuffer						m_RawBuffer;
	CBuffer						m_MotdBuffer;
	CBuffer						m_QueryBuffer;
	bool				m_bIRCConnected;
	bool				m_bMultiClients;
	bool				m_bBounceDCCs;
	bool				m_bPassHashed;
	bool				m_bUseClientIP;
	bool				m_bKeepNick;
	bool				m_bDenyLoadMod;
	bool				m_bAdmin;
	bool				m_bKeepBuffer;
	bool				m_bAutoCycle;
	bool				m_bBeingDeleted;

	CBackNickTimer*		m_pBackNickTimer;
	CAwayNickTimer*		m_pAwayNickTimer;
	CKeepNickTimer*		m_pKeepNickTimer;
	CJoinTimer*			m_pJoinTimer;
	CMiscTimer*			m_pMiscTimer;

	vector<CServer*>	m_vServers;
	vector<CChan*>		m_vChans;
	vector<CClient*>	m_vClients;
	set<CString>		m_ssAllowedHosts;
	unsigned int		m_uServerIdx;
	unsigned int		m_uBufferCount;

#ifdef _MODULES
	CModules*		m_pModules;
#endif
};

#endif // !_USER_H
