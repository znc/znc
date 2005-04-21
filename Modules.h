#ifndef _MODULES_H
#define _MODULES_H

#include "main.h"
#include <dlfcn.h>
#include <string.h>
#include <vector>
using std::vector;
using std::string;

#define VERSION 0.029
#define MODULEDEFS(CLASS) extern "C" { CModule* Load(void* p, CUser* pUser, const string& sModName); void Unload(CModule* pMod); double GetVersion(); } double GetVersion() { return VERSION; } CModule* Load(void* p, CUser* pUser, const string& sModName) { return new CLASS(p, pUser, sModName); } void Unload(CModule* pMod) { if (pMod) { delete pMod; } }
#define MODCONSTRUCTOR(CLASS) CLASS(void *pDLL, CUser* pUser, const string& sModName) : CModule(pDLL, pUser, sModName) 

// Forward Declarations
class CUser;
class CNick;
class CChan;
class Csock;
class CModule;
template<class T> class TSocketManager;
// !Forward Declarations

class CTimer : public CCron {
public:
	CTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const string& sLabel, const string& sDescription);

	virtual ~CTimer();

	// Setters
	void SetModule(CModule* p);
	void SetDescription(const string& s);
	// !Setters

	// Getters
	CModule* GetModule() const;
	const string& GetDescription() const;
	// !Getters
private:
protected:
	CModule*	m_pModule;
	string		m_sDescription;
};

class CModule {
public:  
	CModule(void* pDLL, CUser* pUser, const string& sModName);
	virtual ~CModule();

	virtual string GetDescription();

	virtual bool OnLoad(const string& sArgs);
	virtual bool OnBoot();
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const string& sFile, unsigned long uFileSize);

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, const CChan& Channel, const string& sModes, const string& sArgs);

	virtual bool OnUserRaw(string& sLine);
	virtual bool OnRaw(string& sLine);

	virtual bool OnStatusCommand(const string& sCommand);
	virtual void OnModCommand(const string& sCommand);
	virtual void OnModNotice(const string& sMessage);
	virtual void OnModCTCP(const string& sMessage);

	virtual void OnQuit(const CNick& Nick, const string& sMessage);
	virtual void OnNick(const CNick& Nick, const string& sNewNick);
	virtual void OnKick(const CNick& Nick, const string& sOpNick, const CChan& Channel, const string& sMessage);
	virtual void OnJoin(const CNick& Nick, const CChan& Channel);
	virtual void OnPart(const CNick& Nick, const CChan& Channel);

	virtual bool OnUserCTCPReply(const CNick& Nick, string& sMessage);
	virtual bool OnCTCPReply(const CNick& Nick, string& sMessage);
	virtual bool OnUserCTCP(const string& sTarget, string& sMessage);
	virtual bool OnPrivCTCP(const CNick& Nick, string& sMessage);
	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, string& sMessage);
	virtual bool OnUserMsg(const string& sTarget, string& sMessage);
	virtual bool OnPrivMsg(const CNick& Nick, string& sMessage);
	virtual bool OnChanMsg(const CNick& Nick, const CChan& Channel, string& sMessage);
	virtual bool OnUserNotice(const string& sTarget, string& sMessage);
	virtual bool OnPrivNotice(const CNick& Nick, string& sMessage);
	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, string& sMessage);

	void * GetDLL();
	static double GetVersion() { return VERSION; }

	virtual bool PutIRC(const string& sLine);
	virtual bool PutUser(const string& sLine);
	virtual bool PutStatus(const string& sLine);
	virtual bool PutModule(const string& sLine, const string& sIdent = "znc", const string& sHost = "znc.com");
	virtual bool PutModNotice(const string& sLine, const string& sIdent = "znc", const string& sHost = "znc.com");

	const string& GetModName();
	string GetModNick();

	// Timer stuff
	bool AddTimer(CTimer* pTimer);
	bool RemTimer(const string& sLabel);
	bool UnlinkTimer(CTimer* pTimer);
	CTimer* FindTimer(const string& sLabel);
	virtual void ListTimers();
	// !Timer stuff

protected:
	vector<CTimer*>			m_vTimers;
	void*					m_pDLL;
	TSocketManager<Csock>*	m_pManager;
	CUser*					m_pUser;
	string					m_sModName;
};

class CModules : public vector<CModule*> {
public: 
	CModules();
	virtual ~CModules();

	void UnloadAll();

	virtual bool OnLoad(const string& sArgs);
	virtual bool OnBoot();
	virtual void OnUserAttached();
	virtual void OnUserDetached();
	virtual void OnIRCDisconnected();
	virtual void OnIRCConnected();

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, const string& sFile, unsigned long uFileSize);

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange);
	virtual void OnRawMode(const CNick& OpNick, const CChan& Channel, const string& sModes, const string& sArgs);

	virtual bool OnUserRaw(string& sLine);
	virtual bool OnRaw(string& sLine);

	virtual bool OnStatusCommand(const string& sCommand);
	virtual void OnModCommand(const string& sCommand);
	virtual void OnModNotice(const string& sMessage);
	virtual void OnModCTCP(const string& sMessage);

	virtual void OnQuit(const CNick& Nick, const string& sMessage);
	virtual void OnNick(const CNick& Nick, const string& sNewNick);
	virtual void OnKick(const CNick& Nick, const string& sOpNick, const CChan& Channel, const string& sMessage);
	virtual void OnJoin(const CNick& Nick, const CChan& Channel);
	virtual void OnPart(const CNick& Nick, const CChan& Channel);

	virtual bool OnUserCTCPReply(const CNick& Nick, string& sMessage);
	virtual bool OnCTCPReply(const CNick& Nick, string& sMessage);
	virtual bool OnUserCTCP(const string& sTarget, string& sMessage);
	virtual bool OnPrivCTCP(const CNick& Nick, string& sMessage);
	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, string& sMessage);
	virtual bool OnUserMsg(const string& sTarget, string& sMessage);
	virtual bool OnPrivMsg(const CNick& Nick, string& sMessage);
	virtual bool OnChanMsg(const CNick& Nick, const CChan& Channel, string& sMessage);
	virtual bool OnUserNotice(const string& sTarget, string& sMessage);
	virtual bool OnPrivNotice(const CNick& Nick, string& sMessage);
	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, string& sMessage);

	CModule* FindModule(const string& sModule);
	bool LoadModule(const string& sModule, const string& sArgs, CUser* pUser, string& sRetMsg);
	bool UnloadModule(const string& sModule, string& sRetMsg);
	bool ReloadModule(const string& sModule, const string& sArgs, CUser* pUser, string& sRetMsg);
};

#endif // !_MODULES_H
