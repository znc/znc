#ifndef _MODULES_H
#define _MODULES_H

#include "main.h"
#include <dlfcn.h>
#include <vector>
using std::vector;

#define MODULEDEFS(CLASS) extern "C" { CModule* Load(void* p, CUser* pUser, const CString& sModName); void Unload(CModule* pMod); double GetVersion(); } double GetVersion() { return VERSION; } CModule* Load(void* p, CUser* pUser, const CString& sModName) { return new CLASS(p, pUser, sModName); } void Unload(CModule* pMod) { if (pMod) { delete pMod; } }
#define MODCONSTRUCTOR(CLASS) CLASS(void *pDLL, CUser* pUser, const CString& sModName) : CModule(pDLL, pUser, sModName)

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


class CModInfo {
public:

	CModInfo(const CString& sName, const CString& sPath, bool bSystem) {
		m_bSystem = bSystem;
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
	bool IsSystem() const { return m_bSystem; }
	// !Getters
private:
protected:
	bool	m_bSystem;
	CString	m_sName;
	CString	m_sPath;
};

class CModule {
public:
	CModule(void* pDLL, CUser* pUser, const CString& sModName);
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

	void Unload();
	virtual CString GetDescription();

	virtual bool OnLoad(const CString& sArgs);
	virtual bool OnBoot();
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, const CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, const CChan& Channel, const CString& sModes, const CString& sArgs);

	virtual EModRet OnUserRaw(CString& sLine);
	virtual EModRet OnRaw(CString& sLine);

	virtual EModRet OnStatusCommand(const CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& Nick, const CString& sOpNick, const CChan& Channel, const CString& sMessage);
	virtual void OnJoin(const CNick& Nick, const CChan& Channel);
	virtual void OnPart(const CNick& Nick, const CChan& Channel);

	virtual EModRet OnUserCTCPReply(const CNick& Nick, CString& sMessage);
	virtual EModRet OnCTCPReply(const CNick& Nick, CString& sMessage);
	virtual EModRet OnUserCTCP(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivCTCP(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanCTCP(const CNick& Nick, const CChan& Channel, CString& sMessage);
	virtual EModRet OnUserMsg(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivMsg(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanMsg(const CNick& Nick, const CChan& Channel, CString& sMessage);
	virtual EModRet OnUserNotice(const CString& sTarget, CString& sMessage);
	virtual EModRet OnPrivNotice(const CNick& Nick, CString& sMessage);
	virtual EModRet OnChanNotice(const CNick& Nick, const CChan& Channel, CString& sMessage);

	void * GetDLL();
	static double GetVersion() { return VERSION; }

	virtual bool PutIRC(const CString& sLine);
	virtual bool PutUser(const CString& sLine);
	virtual bool PutStatus(const CString& sLine);
	virtual bool PutModule(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");
	virtual bool PutModNotice(const CString& sLine, const CString& sIdent = "znc", const CString& sHost = "znc.com");

	const CString& GetModName();
	CString GetModNick();

	// Timer stuff
	bool AddTimer(CTimer* pTimer);
	bool AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles = 0, const CString& sDescription = "");
	bool RemTimer(const CString& sLabel);
	bool UnlinkTimer(CTimer* pTimer);
	CTimer* FindTimer(const CString& sLabel);
	virtual void ListTimers();
	// !Timer stuff

	bool LoadRegistry();
	bool SaveRegistry();
	bool SetNV(const CString & sName, const CString & sValue, bool bWriteToDisk = true);
	CString GetNV(const CString & sName);
	bool DelNV(const CString & sName, bool bWriteToDisk = true);
	MCString::iterator FindNV(const CString & sName) { return m_mssRegistry.find(sName); }
	MCString::iterator EndNV() { return m_mssRegistry.end(); }
	MCString::iterator BeginNV() { return m_mssRegistry.begin(); }
	void DelNV(MCString::iterator it) { m_mssRegistry.erase(it); }

protected:
	vector<CTimer*>			m_vTimers;
	void*					m_pDLL;
	TSocketManager<Csock>*	m_pManager;
	CUser*					m_pUser;
	CString					m_sModName;
private:
	MCString				m_mssRegistry; //!< way to save name/value pairs. Note there is no encryption involved in this
};

class CModules : public vector<CModule*> {
public:
	CModules();
	virtual ~CModules();

	void UnloadAll();

	virtual bool OnLoad(const CString& sArgs);	// Return false to abort
	virtual bool OnBoot();						// Return false to abort
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const CString& sFile, unsigned long uFileSize);

	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, const CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, const CChan& Channel, const CString& sModes, const CString& sArgs);

	virtual bool OnUserRaw(CString& sLine);
	virtual bool OnRaw(CString& sLine);

	virtual bool OnStatusCommand(const CString& sCommand);
	virtual void OnModCommand(const CString& sCommand);
	virtual void OnModNotice(const CString& sMessage);
	virtual void OnModCTCP(const CString& sMessage);

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	virtual void OnKick(const CNick& Nick, const CString& sOpNick, const CChan& Channel, const CString& sMessage);
	virtual void OnJoin(const CNick& Nick, const CChan& Channel);
	virtual void OnPart(const CNick& Nick, const CChan& Channel);

	virtual bool OnUserCTCPReply(const CNick& Nick, CString& sMessage);
	virtual bool OnCTCPReply(const CNick& Nick, CString& sMessage);
	virtual bool OnUserCTCP(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivCTCP(const CNick& Nick, CString& sMessage);
	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, CString& sMessage);
	virtual bool OnUserMsg(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivMsg(const CNick& Nick, CString& sMessage);
	virtual bool OnChanMsg(const CNick& Nick, const CChan& Channel, CString& sMessage);
	virtual bool OnUserNotice(const CString& sTarget, CString& sMessage);
	virtual bool OnPrivNotice(const CNick& Nick, CString& sMessage);
	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, CString& sMessage);

	CModule* FindModule(const CString& sModule);
	bool LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);
	bool UnloadModule(const CString& sModule);
	bool UnloadModule(const CString& sModule, CString& sRetMsg);
	bool ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);

	static void GetAvailableMods(set<CModInfo>& ssMods, CZNC* pZNC);
};

#endif // !_MODULES_H
