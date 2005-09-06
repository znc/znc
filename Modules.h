#ifndef _MODULES_H
#define _MODULES_H

#include "main.h"
#include "FileUtils.h"
#include <dlfcn.h>
#include <vector>
#include <set>
using std::vector;
using std::set;

// User Module Macros
#ifdef REQUIRESSL
#error -
#error -
#error This module only works when znc is compiled with OpenSSL support
#error -
#error -
#endif

#define MODCONSTRUCTOR(CLASS) \
	CLASS(void *pDLL, CUser* pUser, const CString& sModName) : CModule(pDLL, pUser, sModName)
#define MODULEDEFS(CLASS, DESCRIPTION) \
	extern "C" { \
		CString GetDescription() { return DESCRIPTION; } \
		bool IsGlobal() { return false; } \
		CModule* Load(void* p, CUser* pUser, const CString& sModName); \
		void Unload(CModule* pMod); double GetVersion(); } \
		double GetVersion() { return VERSION; } \
		CModule* Load(void* p, CUser* pUser, const CString& sModName) { return new CLASS(p, pUser, sModName); } \
		void Unload(CModule* pMod) { if (pMod) { delete pMod; } \
	}
// !User Module Macros

// Global Module Macros
#define GLOBALMODCONSTRUCTOR(CLASS) \
	CLASS(void *pDLL, CZNC* pZNC, const CString& sModName) : CGlobalModule(pDLL, pZNC, sModName)
#define GLOBALMODULEDEFS(CLASS, DESCRIPTION) \
	extern "C" { \
		CString GetDescription() { return DESCRIPTION; } \
		bool IsGlobal() { return true; } \
		CGlobalModule* Load(void* p, CZNC* pZNC, const CString& sModName); \
		void Unload(CGlobalModule* pMod); double GetVersion(); } \
		double GetVersion() { return VERSION; } \
		CGlobalModule* Load(void* p, CZNC* pZNC, const CString& sModName) { return new CLASS(p, pZNC, sModName); } \
		void Unload(CGlobalModule* pMod) { if (pMod) { delete pMod; } \
	}
// !Global Module Macros

		//const char* GetDescription() { static char sz[] = DESCRIPTION; return sz; } 
// Forward Declarations
class CZNC;
class CUser;
class CNick;
class CChan;
class Csock;
class CModule;
class CFPTimer;
template<class T> class TSocketManager;
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
	CSocket(CModule* pModule, const CString& sLabel);
	CSocket(CModule* pModule, const CString& sLabel, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CSocket();

	bool Connect(const CString& sHostname, unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 60);
	bool Listen(unsigned short uPort, bool bSSL = false, unsigned int uTimeout = 0);
	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");

	// Setters
	void SetModule(CModule* p);
	void SetLabel(const CString& s);
	// !Setters

	// Getters
	CModule* GetModule() const;
	const CString& GetLabel() const;
	// !Getters
private:
protected:
	CModule*	m_pModule;
	CString		m_sLabel;
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
	virtual ~CModInfo() {}

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
	CModule(void* pDLL, CUser* pUser, const CString& sModName);
	CModule(void* pDLL, CZNC* pZNC, const CString& sModName);
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
	void Unload();

	virtual bool OnLoad(const CString& sArgs);
	virtual bool OnBoot();
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);

	virtual EModRet OnUserRaw(CString& sLine);
	virtual EModRet OnRaw(CString& sLine);

	virtual EModRet OnStatusCommand(const CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage);
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	virtual void OnPart(const CNick& Nick, CChan& Channel);

	virtual EModRet OnUserCTCPReply(const CNick& Nick, CString& sMessage);
	virtual EModRet OnCTCPReply(const CNick& Nick, CString& sMessage);
	virtual EModRet OnUserCTCP(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivCTCP(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanCTCP(const CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnUserMsg(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivMsg(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(const CNick& Nick, CChan& Channel, CString& sMessage);
	virtual EModRet OnUserNotice(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivNotice(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(const CNick& Nick, CChan& Channel, CString& sMessage);

	void * GetDLL();
	static double GetCoreVersion() { return VERSION; }

	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");

	const CString& GetModName() const;
	CString GetModNick() const;

	// Timer stuff
	bool AddTimer(CTimer* pTimer);
	bool AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles = 0, const CString& sDescription = "");
	bool RemTimer(const CString& sLabel);
	bool UnlinkTimer(CTimer* pTimer);
	CTimer* FindTimer(const CString& sLabel);
	virtual void ListTimers();
	// !Timer stuff

	// Socket stuff
	bool AddSocket(CSocket* pSocket);
	bool RemSocket(CSocket* pSocket);
	bool RemSocket(const CString& sLabel);
	bool UnlinkSocket(CSocket* pSocket);
	CSocket* FindSocket(const CString& sLabel);
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

	const CString& GetSavePath() const { if (!CFile::Exists(m_sSavePath)) { CUtils::MakeDir(m_sSavePath); } return m_sSavePath; }

	// Setters
	void SetDescription(const CString& s) { m_sDescription = s; }
	void SetArgs(const CString& s) { m_sArgs = s; }
	// !Setters

	// Getters
	const CString& GetDescription() const { return m_sDescription; }
	const CString& GetArgs() const { return m_sArgs; }
	CZNC* GetZNC() { return m_pZNC; }
	CUser* GetUser() { return m_pUser; }
	TSocketManager<Csock>* GetManager() { return m_pManager; }
	// !Getters

protected:
	CString					m_sDescription;
	vector<CTimer*>			m_vTimers;
	vector<CSocket*>		m_vSockets;
	void*					m_pDLL;
	TSocketManager<Csock>*	m_pManager;
	CUser*					m_pUser;
	CZNC*					m_pZNC;
	CString					m_sModName;
	CString					m_sSavePath;
	CString					m_sArgs;
private:
	MCString				m_mssRegistry; //!< way to save name/value pairs. Note there is no encryption involved in this
};

class CModules : public vector<CModule*> {
public:
	CModules(CZNC* pZNC);
	virtual ~CModules();

	void SetUser(CUser* pUser) { m_pUser = pUser; }

	void UnloadAll();

	virtual bool OnBoot();						// Return false to abort
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);

	virtual bool OnUserRaw(CString& sLine);
	virtual bool OnRaw(CString& sLine);

	virtual bool OnStatusCommand(const CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage);
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	virtual void OnPart(const CNick& Nick, CChan& Channel);

	virtual bool OnUserCTCPReply(const CNick& Nick, CString& sMessage);
	virtual bool OnCTCPReply(const CNick& Nick, CString& sMessage);
	virtual bool OnUserCTCP(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivCTCP(const CNick& Nick, CString& sMessage);
	virtual bool OnChanCTCP(const CNick& Nick, CChan& Channel, CString& sMessage);
	virtual bool OnUserMsg(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivMsg(const CNick& Nick, CString& sMessage);
	virtual bool OnChanMsg(const CNick& Nick, CChan& Channel, CString& sMessage);
	virtual bool OnUserNotice(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivNotice(const CNick& Nick, CString& sMessage);
	virtual bool OnChanNotice(const CNick& Nick, CChan& Channel, CString& sMessage);

	CModule* FindModule(const CString& sModule) const;
	bool LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);
	bool UnloadModule(const CString& sModule);
	bool UnloadModule(const CString& sModule, CString& sRetMsg);
	bool ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);
	CString FindModPath(const CString& sModule, CUser* pUser = NULL);

	bool GetModInfo(CModInfo& ModInfo, const CString& sModule);
	void GetAvailableMods(set<CModInfo>& ssMods, CZNC* pZNC, bool bGlobal = false);

protected:
	CZNC*	m_pZNC;
	CUser*	m_pUser;
};

class CGlobalModule : public CModule {
public:
	CGlobalModule(void* pDLL, CZNC* pZNC, const CString& sModName) : CModule(pDLL, pZNC, sModName) {}
	virtual ~CGlobalModule() {}

	virtual EModRet OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan);
private:
};

class CGlobalModules : public CModules {
public:
	CGlobalModules(CZNC* pZNC) : CModules(pZNC) {}
	virtual ~CGlobalModules() {}

	virtual bool OnConfigLine(const CString& sName, const CString& sValue, CUser* pUser, CChan* pChan);
private:
};

#endif // !_MODULES_H
