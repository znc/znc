/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifdef _MODULES

#ifndef _MODULES_H
#define _MODULES_H

#include "FileUtils.h"
#include "Utils.h"
#include <set>
#include <vector>

using std::vector;
using std::set;

// Forward Declarations
class CAuthBase;
class CChan;
class CClient;
// !Forward Declarations

// User Module Macros
#ifdef REQUIRESSL
#ifndef HAVE_LIBSSL
#error -
#error -
#error This module only works when znc is compiled with OpenSSL support
#error -
#error -
#endif
#endif

typedef void* ModHandle;

#define MODCONSTRUCTOR(CLASS) \
	CLASS(ModHandle pDLL, CUser* pUser, const CString& sModName, \
			const CString& sModPath) \
			: CModule(pDLL, pUser, sModName, sModPath)
#define MODULEDEFS(CLASS, DESCRIPTION) \
	extern "C" { \
		CString ZNCModDescription() { return DESCRIPTION; } \
		bool ZNCModGlobal() { return false; } \
		double ZNCModVersion() { return VERSION; } \
		CModule* ZNCModLoad(ModHandle p, CUser* pUser, const CString& sModName, \
				const CString& sModPath) \
		{ return new CLASS(p, pUser, sModName, sModPath); } \
		void ZNCModUnload(CModule* pMod) { if (pMod) { delete pMod; } } \
	}
// !User Module Macros

// Global Module Macros
#define GLOBALMODCONSTRUCTOR(CLASS) \
	CLASS(ModHandle pDLL, const CString& sModName, const CString& sModPath) \
			: CGlobalModule(pDLL, sModName, sModPath)
#define GLOBALMODULEDEFS(CLASS, DESCRIPTION) \
	extern "C" { \
		CString ZNCModDescription() { return DESCRIPTION; } \
		bool ZNCModGlobal() { return true; } \
		double ZNCModVersion() { return VERSION; } \
		CGlobalModule* ZNCModLoad(ModHandle p, const CString& sModName, \
				const CString& sModPath) \
		{ return new CLASS(p, sModName, sModPath); } \
		void ZNCModUnload(CGlobalModule* pMod) { if (pMod) { delete pMod; } } \
	}
// !Global Module Macros

// Forward Declarations
class CZNC;
class CUser;
class CNick;
class CChan;
class Csock;
class CModule;
class CFPTimer;
class CSockManager;
// !Forward Declarations

class CTimer : public CCron {
public:
	CTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription);

	virtual ~CTimer();

	// Setters
	void SetModule(CModule* p);
	void SetDescription(const CString& s);
	// !Setters

	// Getters
	CModule* GetModule() const;
	const CString& GetDescription() const;
	// !Getters
private:
protected:
	CModule*	m_pModule;
	CString		m_sDescription;
};

typedef void (*FPTimer_t)(CModule *, CFPTimer *);

class CFPTimer : public CTimer {
public:
	CFPTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {
		m_pFBCallback = NULL;
	}

	virtual ~CFPTimer() {}

	void SetFPCallback(FPTimer_t p) { m_pFBCallback = p; }

protected:
	virtual void RunJob() {
		if (m_pFBCallback) {
			m_pFBCallback(m_pModule, this);
		}
	}

private:
	FPTimer_t	m_pFBCallback;
};

class CSocket : public Csock {
public:
	CSocket(CModule* pModule);
	CSocket(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CSocket();

	using Csock::Connect;
	using Csock::Listen;

	// This defaults to closing the socket, feel free to override
	virtual void ReachedMaxBuffer();

	bool Connect(const CString& sHostname, unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 60);
	bool Listen(unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 0);
	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");

	// Setters
	void SetModule(CModule* p);
	// !Setters

	// Getters
	CModule* GetModule() const;
	// !Getters
private:
protected:
	CModule*	m_pModule;
};

class CModInfo {
public:
	CModInfo() {}
	CModInfo(const CString& sName, const CString& sPath, bool bSystem, bool bGlobal) {
		m_bSystem = bSystem;
		m_bGlobal = bGlobal;
		m_sName = sName;
		m_sPath = sPath;
	}
	~CModInfo() {}

	bool operator < (const CModInfo& Info) const {
		return (GetName() < Info.GetName());
	}

	// Getters
	const CString& GetName() const { return m_sName; }
	const CString& GetPath() const { return m_sPath; }
	const CString& GetDescription() const { return m_sDescription; }
	bool IsSystem() const { return m_bSystem; }
	bool IsGlobal() const { return m_bGlobal; }
	// !Getters

	// Setters
	void SetName(const CString& s) { m_sName = s; }
	void SetPath(const CString& s) { m_sPath = s; }
	void SetDescription(const CString& s) { m_sDescription = s; }
	void SetSystem(bool b) { m_bSystem = b; }
	void SetGlobal(bool b) { m_bGlobal = b; }
	// !Setters
private:
protected:
	bool	m_bSystem;
	bool	m_bGlobal;
	CString	m_sName;
	CString	m_sPath;
	CString	m_sDescription;
};

class CModule {
public:
	CModule(ModHandle pDLL, CUser* pUser, const CString& sModName,
			const CString& sDataDir);
	CModule(ModHandle pDLL, const CString& sModName, const CString& sDataDir);
	virtual ~CModule();

	typedef enum {
		CONTINUE	= 1,
		HALT		= 2,
		HALTMODS	= 3,
		HALTCORE	= 4
	} EModRet;

	typedef enum {
		UNLOAD
	} EModException;

	void SetUser(CUser* pUser);
	void SetClient(CClient* pClient);
	void Unload();

	virtual bool OnLoad(const CString& sArgsi, CString& sMessage);
	virtual bool OnBoot();
	virtual void OnPreRehash();
	virtual void OnPostRehash();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();
	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName);
	virtual EModRet OnBroadcast(CString& sMessage);

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);
	virtual void OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange);

	virtual EModRet OnRaw(CString& sLine);

	virtual EModRet OnStatusCommand(CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage);
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	virtual void OnPart(const CNick& Nick, CChan& Channel);

	virtual void OnUserAttached(bool thisFunctionWasRemoved, bool UseOnClientLogin) {}
	virtual void OnUserDetached(bool thisFunctionWasRemoved, bool UseOnClientDisconnect) {}
	virtual void OnClientLogin();
	virtual void OnClientDisconnect();
	virtual EModRet OnUserRaw(CString& sLine);
	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage);
	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey);
	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage);
	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic);
	virtual EModRet OnUserTopicRequest(CString& sChannel);

	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage);
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic);
	virtual EModRet OnTimerAutoJoin(CChan& Channel);

	ModHandle GetDLL();
	static double GetCoreVersion() { return VERSION; }

	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");
	virtual unsigned int PutModule(const CTable& table, const CString& sIdent = "", const CString& sHost = "znc.in");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "", const CString& sHost = "znc.in");

	const CString& GetModName() const { return m_sModName; }
	// This is where constant module data (e.g. webadmin skins) are saved
	const CString& GetModDataDir() const { return m_sDataDir; }
	CString GetModNick() const;

	// Timer stuff
	bool AddTimer(CTimer* pTimer);
	bool AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles = 0, const CString& sDescription = "");
	bool RemTimer(const CString& sLabel);
	bool UnlinkTimer(CTimer* pTimer);
	CTimer* FindTimer(const CString& sLabel);
	vector<CTimer*>::const_iterator BeginTimers() const { return m_vTimers.begin(); }
	vector<CTimer*>::const_iterator EndTimers() const { return m_vTimers.end(); }
	virtual void ListTimers();
	// !Timer stuff

	// Socket stuff
	bool AddSocket(CSocket* pSocket);
	bool RemSocket(CSocket* pSocket);
	bool RemSocket(const CString& sSockName);
	bool UnlinkSocket(CSocket* pSocket);
	CSocket* FindSocket(const CString& sSockName);
	vector<CSocket*>::const_iterator BeginSockets() const { return m_vSockets.begin(); }
	vector<CSocket*>::const_iterator EndSockets() const { return m_vSockets.end(); }
	virtual void ListSockets();
	// !Socket stuff

	bool LoadRegistry();
	bool SaveRegistry();
	bool SetNV(const CString & sName, const CString & sValue, bool bWriteToDisk = true);
	CString GetNV(const CString & sName);
	bool DelNV(const CString & sName, bool bWriteToDisk = true);
	MCString::iterator FindNV(const CString & sName) { return m_mssRegistry.find(sName); }
	MCString::iterator EndNV() { return m_mssRegistry.end(); }
	MCString::iterator BeginNV() { return m_mssRegistry.begin(); }
	void DelNV(MCString::iterator it) { m_mssRegistry.erase(it); }
	bool ClearNV(bool bWriteToDisk = true);

	const CString& GetSavePath() const { if (!CFile::Exists(m_sSavePath)) { CDir::MakeDir(m_sSavePath); } return m_sSavePath; }

	// Setters
	void SetFake(bool b) { m_bFake = b; }
	void SetGlobal(bool b) { m_bGlobal = b; }
	void SetDescription(const CString& s) { m_sDescription = s; }
	void SetArgs(const CString& s) { m_sArgs = s; }
	// !Setters

	// Getters
	bool IsFake() const { return m_bFake; }
	bool IsGlobal() const { return m_bGlobal; }
	const CString& GetDescription() const { return m_sDescription; }
	const CString& GetArgs() const { return m_sArgs; }
	CUser* GetUser() { return m_pUser; }
	CClient* GetClient() { return m_pClient; }
	CSockManager* GetManager() { return m_pManager; }
	// !Getters

protected:
	bool			m_bFake;
	bool			m_bGlobal;
	CString			m_sDescription;
	vector<CTimer*>		m_vTimers;
	vector<CSocket*>	m_vSockets;
	ModHandle 		m_pDLL;
	CSockManager*		m_pManager;
	CUser*			m_pUser;
	CClient*		m_pClient;
	CString			m_sModName;
	CString			m_sDataDir;
	CString			m_sSavePath;
	CString			m_sArgs;
private:
	MCString		m_mssRegistry; //!< way to save name/value pairs. Note there is no encryption involved in this
};

class CModules : public vector<CModule*> {
public:
	CModules();
	~CModules();

	void SetUser(CUser* pUser) { m_pUser = pUser; }
	void SetClient(CClient* pClient) { m_pClient = pClient; }

	void UnloadAll();

	bool OnBoot();						// Return false to abort
	bool OnPreRehash();
	bool OnPostRehash();
	bool OnIRCDisconnected();
	bool OnIRCConnected();
	bool OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName);
	bool OnBroadcast(CString& sMessage);

	bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	bool OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	bool OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	bool OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	bool OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	bool OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	bool OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);
	bool OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange);

	bool OnRaw(CString& sLine);

	bool OnStatusCommand(CString& sCommand);
	bool OnModCommand(const CString& sCommand);
	bool OnModNotice(const CString& sMessage);
	bool OnModCTCP(const CString& sMessage);

	bool OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	bool OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	bool OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage);
	bool OnJoin(const CNick& Nick, CChan& Channel);
	bool OnPart(const CNick& Nick, CChan& Channel);

	bool OnClientLogin();
	bool OnClientDisconnect();
	bool OnUserRaw(CString& sLine);
	bool OnUserCTCPReply(CString& sTarget, CString& sMessage);
	bool OnUserCTCP(CString& sTarget, CString& sMessage);
	bool OnUserAction(CString& sTarget, CString& sMessage);
	bool OnUserMsg(CString& sTarget, CString& sMessage);
	bool OnUserNotice(CString& sTarget, CString& sMessage);
	bool OnUserJoin(CString& sChannel, CString& sKey);
	bool OnUserPart(CString& sChannel, CString& sMessage);
	bool OnUserTopic(CString& sChannel, CString& sTopic);
	bool OnUserTopicRequest(CString& sChannel);

	bool OnCTCPReply(CNick& Nick, CString& sMessage);
	bool OnPrivCTCP(CNick& Nick, CString& sMessage);
	bool OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage);
	bool OnPrivAction(CNick& Nick, CString& sMessage);
	bool OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);
	bool OnPrivMsg(CNick& Nick, CString& sMessage);
	bool OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);
	bool OnPrivNotice(CNick& Nick, CString& sMessage);
	bool OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);
	bool OnTopic(CNick& Nick, CChan& Channel, CString& sTopic);
	bool OnTimerAutoJoin(CChan& Channel);

	CModule* FindModule(const CString& sModule) const;
	bool LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg, bool bFake = false);
	bool UnloadModule(const CString& sModule);
	bool UnloadModule(const CString& sModule, CString& sRetMsg);
	bool ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);

	bool GetModInfo(CModInfo& ModInfo, const CString& sModule);
	void GetAvailableMods(set<CModInfo>& ssMods, bool bGlobal = false);

protected:
	CUser*		m_pUser;
	CClient*	m_pClient;
};

class CGlobalModule : public CModule {
public:
	CGlobalModule(ModHandle pDLL, const CString& sModName,
			const CString &sDataDir) : CModule(pDLL, sModName, sDataDir) {}
	virtual ~CGlobalModule() {}

	virtual EModRet OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan);
	virtual EModRet OnDeleteUser(CUser& User);
	virtual void OnClientConnect(CClient* pClient, const CString& sHost, unsigned short uPort);
	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth);
	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP);
	virtual EModRet OnUnknownUserRaw(CClient* pClient, CString& sLine);
private:
};

class CGlobalModules : public CModules {
public:
	CGlobalModules() : CModules() {}
	~CGlobalModules() {}

	bool OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan);
	bool OnDeleteUser(CUser& User);
	void OnClientConnect(CClient* pClient, const CString& sHost, unsigned short uPort);
	bool OnLoginAttempt(CSmartPtr<CAuthBase> Auth);
	void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP);
	bool OnUnknownUserRaw(CClient* pClient, CString& sLine);
private:
};

#endif // !_MODULES_H

#endif // _MODULES
