/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _MODULES_H
#define _MODULES_H

#include "zncconfig.h"
#include "WebModules.h"
#include "main.h"
#include <set>
#include <queue>

using std::set;

// Forward Declarations
class CAuthBase;
class CChan;
class CClient;
class CWebSock;
class CTemplate;
class CIRCSock;
class CModule;
class CGlobalModule;
class CModInfo;
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

template<class M> void TModInfo(CModInfo& Info) {}

template<class M> CModule* TModLoad(ModHandle p, CUser* pUser,
		const CString& sModName, const CString& sModPath) {
	return new M(p, pUser, sModName, sModPath);
}
template<class M> CGlobalModule* TModLoadGlobal(ModHandle p,
		const CString& sModName, const CString& sModPath) {
	return new M(p, sModName, sModPath);
}

#if HAVE_VISIBILITY
# define MODULE_EXPORT __attribute__((__visibility__("default")))
#else
# define MODULE_EXPORT
#endif

#define MODCOMMONDEFS(CLASS, DESCRIPTION, GLOBAL, LOADER) \
	extern "C" { \
		MODULE_EXPORT bool ZNCModInfo(double dCoreVersion, CModInfo& Info); \
		bool ZNCModInfo(double dCoreVersion, CModInfo& Info) { \
			if (dCoreVersion != VERSION) \
				return false; \
			Info.SetDescription(DESCRIPTION); \
			Info.SetGlobal(GLOBAL); \
			LOADER; \
			TModInfo<CLASS>(Info); \
			return true; \
		} \
	}

/** Instead of writing a constructor, you should call this macro. It accepts all
 *  the necessary arguments and passes them on to CModule's constructor. You
 *  should assume that there are no arguments to the constructor.
 *
 *  Usage:
 *  \code
 *  class MyModule : public CModule {
 *      MODCONSTRUCTOR(MyModule) {
 *          // Your own constructor's code here
 *      }
 *  }
 *  \endcode
 *
 *  @param CLASS The name of your module's class.
 *  @see For global modules you need GLOBALMODCONSTRUCTOR.
 */
#define MODCONSTRUCTOR(CLASS) \
	CLASS(ModHandle pDLL, CUser* pUser, const CString& sModName, \
			const CString& sModPath) \
			: CModule(pDLL, pUser, sModName, sModPath)

/** At the end of your source file, you must call this macro in global context.
 *  It defines some static functions which ZNC needs to load this module.
 *  @param CLASS The name of your module's class.
 *  @param DESCRIPTION A short description of your module.
 *  @see For global modules you need GLOBALMODULEDEFS.
 */
#define MODULEDEFS(CLASS, DESCRIPTION) \
	MODCOMMONDEFS(CLASS, DESCRIPTION, false, Info.SetLoader(TModLoad<CLASS>))
// !User Module Macros

// Global Module Macros
/** This works exactly like MODCONSTRUCTOR, but for global modules. */
#define GLOBALMODCONSTRUCTOR(CLASS) \
	CLASS(ModHandle pDLL, const CString& sModName, const CString& sModPath) \
			: CGlobalModule(pDLL, sModName, sModPath)

/** This works exactly like MODULEDEFS, but for global modules. */
#define GLOBALMODULEDEFS(CLASS, DESCRIPTION) \
	MODCOMMONDEFS(CLASS, DESCRIPTION, true, Info.SetGlobalLoader(TModLoadGlobal<CLASS>))
// !Global Module Macros

// Forward Declarations
class CZNC;
class CUser;
class CNick;
class CChan;
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
	CModule*   m_pModule;
	CString    m_sDescription;
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
	FPTimer_t  m_pFBCallback;
};

class CModInfo {
public:
	typedef CModule* (*ModLoader)(ModHandle p, CUser* pUser, const CString& sModName, const CString& sModPath);
	typedef CGlobalModule* (*GlobalModLoader)(ModHandle p, const CString& sModName, const CString& sModPath);

	CModInfo() {
		m_fGlobalLoader = NULL;
		m_fLoader = NULL;
	}
	CModInfo(const CString& sName, const CString& sPath, bool bGlobal) {
		m_bGlobal = bGlobal;
		m_sName = sName;
		m_sPath = sPath;
		m_fGlobalLoader = NULL;
		m_fLoader = NULL;
	}
	~CModInfo() {}

	bool operator < (const CModInfo& Info) const {
		return (GetName() < Info.GetName());
	}

	// Getters
	const CString& GetName() const { return m_sName; }
	const CString& GetPath() const { return m_sPath; }
	const CString& GetDescription() const { return m_sDescription; }
	const CString& GetWikiPage() const { return m_sWikiPage; }
	bool IsGlobal() const { return m_bGlobal; }
	ModLoader GetLoader() const { return m_fLoader; }
	GlobalModLoader GetGlobalLoader() const { return m_fGlobalLoader; }
	// !Getters

	// Setters
	void SetName(const CString& s) { m_sName = s; }
	void SetPath(const CString& s) { m_sPath = s; }
	void SetDescription(const CString& s) { m_sDescription = s; }
	void SetWikiPage(const CString& s) { m_sWikiPage = s; }
	void SetGlobal(bool b) { m_bGlobal = b; }
	void SetLoader(ModLoader fLoader) { m_fLoader = fLoader; }
	void SetGlobalLoader(GlobalModLoader fGlobalLoader) { m_fGlobalLoader = fGlobalLoader; }
	// !Setters
private:
protected:
	bool            m_bGlobal;
	CString         m_sName;
	CString         m_sPath;
	CString         m_sDescription;
	CString         m_sWikiPage;
	ModLoader       m_fLoader;
	GlobalModLoader m_fGlobalLoader;
};

/** A helper class for handling commands in modules. */
class CModCommand {
public:
	/// Type for the callback function that handles the actual command.
	typedef void (CModule::*ModCmdFunc)(const CString& sLine);

	/// Default constructor, needed so that this can be saved in a std::map.
	CModCommand();

	/** Construct a new CModCommand.
	 * @param sCmd The name of the command.
	 * @param func The command's callback function.
	 * @param sArgs Help text describing the arguments to this command.
	 * @param sDesc Help text describing what this command does.
	 */
	CModCommand(const CString& sCmd, ModCmdFunc func, const CString& sArgs, const CString& sDesc);

	/** Copy constructor, needed so that this can be saved in a std::map.
	 * @param other Object to copy from.
	 */
	CModCommand(const CModCommand& other);

	/** Assignment operator, needed so that this can be saved in a std::map.
	 * @param other Object to copy from.
	 */
	CModCommand& operator=(const CModCommand& other);

	/** Initialize a CTable so that it can be used with AddHelp().
	 * @param Table The instance of CTable to initialize.
	 */
	static void InitHelp(CTable& Table);

	/** Add this command to the CTable instance.
	 * @param Table Instance of CTable to which this should be added.
	 * @warning The Table should be initialized via InitHelp().
	 */
	void AddHelp(CTable& Table) const;

	const CString& GetCommand() const { return m_sCmd; }
	ModCmdFunc GetFunction() const { return m_pFunc; }
	const CString& GetArgs() const { return m_sArgs; }
	const CString& GetDescription() const { return m_sDesc; }

	void Call(CModule *pMod, const CString& sLine) const { (pMod->*m_pFunc)(sLine); }

private:
	CString m_sCmd;
	ModCmdFunc m_pFunc;
	CString m_sArgs;
	CString m_sDesc;
};

/** The base class for your own ZNC modules.
 *
 *  If you want to write a module for znc, you will have to implement a class
 *  which inherits from this class. You should override some of the "On*"
 *  functions in this class. These function will then be called by ZNC when the
 *  associated event happens.
 *
 *  If such a module hook is called with a non-const reference to e.g. a
 *  CString, then it is free to modify that argument to influence ZNC's
 *  behavior.
 *
 *  @see MODCONSTRUCTOR and MODULEDEFS
 */
class CModule {
public:
	CModule(ModHandle pDLL, CUser* pUser, const CString& sModName,
			const CString& sDataDir);
	CModule(ModHandle pDLL, const CString& sModName, const CString& sDataDir);
	virtual ~CModule();

	/** This enum is just used for return from module hooks. Based on this
	 *  return, ZNC then decides what to do with the event which caused the
	 *  module hook.
	 */
	typedef enum {
		/** ZNC will continue event processing normally. This is what
		 *  you should return normally.
		 */
		CONTINUE = 1,
		/** This is the same as both CModule::HALTMODS and
		 * CModule::HALTCORE together.
		 */
		HALT     = 2,
		/** Stop sending this even to other modules which were not
		 *  called yet. Internally, the event is handled normally.
		 */
		HALTMODS = 3,
		/** Continue calling other modules. When done, ignore the event
		 *  in the ZNC core. (For most module hooks this means that a
		 *  given event won't be forwarded to the connected users)
		 */
		HALTCORE = 4
	} EModRet;

	typedef enum {
		/** Your module can throw this enum at any given time. When this
		 * is thrown, the module will be unloaded.
		 */
		UNLOAD
	} EModException;

	void SetUser(CUser* pUser);
	void SetClient(CClient* pClient);

	/** This function throws CModule::UNLOAD which causes this module to be unloaded.
	 */
	void Unload() { throw UNLOAD; }

	/** This module hook is called when a module is loaded
	 *  @param sArgsi The arguments for the modules.
	 *  @param sMessage A message that may be displayed to the user after
	 *                  loading the module. Useful for returning error messages.
	 *  @return true if the module loaded successfully, else false.
	 */
	virtual bool OnLoad(const CString& sArgsi, CString& sMessage);
	/** This module hook is called during ZNC startup. Only modules loaded
	 *  from znc.conf get this call.
	 *  @return false to abort ZNC startup.
	 */
	virtual bool OnBoot();


	/** Modules which can only be used with an active user session have to return true here.
	 *  @return false for modules that can do stuff for non-logged in web users as well.
	 */
	virtual bool WebRequiresLogin() { return true; }
	/** Return true if this module should only be usable for admins on the web.
	 *  @return false if normal users can use this module's web pages as well.
	 */
	virtual bool WebRequiresAdmin() { return false; }
	/** Return the title of the module's section in the web interface's side bar.
	 *  @return The Title.
	 */
	virtual CString GetWebMenuTitle() { return ""; }
	/** For WebMods: Called before the list of registered SubPages will be checked.
	 *  Important: If you return true, you need to take care of calling WebSock.Close!
	 *  This allows for stuff like returning non-templated data, long-polling and other fun.
	 *  @param WebSock The active request.
	 *  @param sPageName The name of the page that has been requested.
	 *  @return true if you handled the page request or false if the name is to be checked
	 *          against the list of registered SubPages and their permission settings.
	 */
	virtual bool OnWebPreRequest(CWebSock& WebSock, const CString& sPageName);
	/** If OnWebPreRequest returned false, and the RequiresAdmin/IsAdmin check has been passed,
	 *  this method will be called with the page name. It will also be called for pages that
	 *  have NOT been specifically registered with AddSubPage.
	 *  @param WebSock The active request.
	 *  @param sPageName The name of the page that has been requested.
	 *  @param Tmpl The active template. You can add variables, loops and stuff to it.
	 *  @return You MUST return true if you want the template to be evaluated and sent to the browser.
	 *          Return false if you called Redirect() or PrintErrorPage(). If you didn't, a 404 page will be sent.
	 */
	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl);
	/** Registers a sub page for the sidebar.
	 *  @param spSubPage The SubPage instance.
	 */
	virtual void AddSubPage(TWebSubPage spSubPage) { m_vSubPages.push_back(spSubPage); }
	/** Removes all registered (AddSubPage'd) SubPages.
	 */
	virtual void ClearSubPages() { m_vSubPages.clear(); }
	/** Returns a list of all registered SubPages. Don't mess with it too much.
	 *  @return The List.
	 */
	virtual VWebSubPages& GetSubPages() { return m_vSubPages; }
	/** Using this hook, module can embed web stuff directly to different places.
	 *  This method is called whenever embededded modules I/O happens.
	 *  Name of used .tmpl file (if any) is up to caller.
	 *  @param WebSock Socket for web connection, don't do bad things with it.
	 *  @param sPageName Describes the place where web stuff is embedded to.
	 *  @param Tmpl Template. Depending on context, you can do various stuff with it.
	 *  @return If you don't need to embed web stuff to the specified place, just return false.
	 *          Exact meaning of return value is up to caller, and depends on context.
	 */
	virtual bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl);


	/** Called just before znc.conf is rehashed */
	virtual void OnPreRehash();
	/** This module hook is called after a <em>successful</em> rehash. */
	virtual void OnPostRehash();
	/** This module hook is called when a user gets disconnected from IRC. */
	virtual void OnIRCDisconnected();
	/** This module hook is called after a successful login to IRC. */
	virtual void OnIRCConnected();
	/** This module hook is called just before ZNC tries to establish a
	 *  connection to an IRC server.
	 *  @param pIRCSock The socket that will be used for the connection.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnIRCConnecting(CIRCSock *pIRCSock);
	/** This module hook is called when a CIRCSock fails to connect or
	 *  a module returned HALTCORE from OnIRCConnecting.
	 *  @param pIRCSock The socket that failed to connect.
	 */
	virtual void OnIRCConnectionError(CIRCSock *pIRCSock);
	/** This module hook is called before loging in to the IRC server. The
	 *  low-level connection is established at this point, but SSL
	 *  handshakes didn't necessarily finish yet.
	 *  @param sPass The server password that will be used.
	 *  @param sNick The nick that will be used.
	 *  @param sIdent The protocol identity that will be used. This is not
	 *                the ident string that is transfered via e.g. oidentd!
	 *  @param sRealName The real name that will be used.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName);
	/** This module hook is called when a message is broadcasted to all users.
	 *  @param sMessage The message that is broadcasted.
	 *  @return see CModule::EModRet
	 */
	virtual EModRet OnBroadcast(CString& sMessage);

	/** This module hook is called when a user mode on a channel changes.
	 *  @param OpNick The nick who sent the mode change.
	 *  @param Nick The nick whose channel mode changes.
	 *  @param Channel The channel on which the user mode is changed.
	 *  @param uMode The mode character that is changed, e.g. '@' for op.
	 *  @param bAdded True if the mode is added, else false.
	 *  @param bNoChange true if this mode change doesn't change anything
	 *                   because the nick already had this permission.
	 *  @see CIRCSock::GetModeType() for converting uMode into a mode (e.g.
	 *       'o' for op).
	 */
	virtual void OnChanPermission(const CNick& OpNick, const CNick& Nick, CChan& Channel, unsigned char uMode, bool bAdded, bool bNoChange);
	/** Called when a nick is opped on a channel */
	virtual void OnOp(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	/** Called when a nick is deopped on a channel */
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	/** Called when a nick is voiced on a channel */
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	/** Called when a nick is devoiced on a channel */
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, CChan& Channel, bool bNoChange);
	/** Called on an individual channel mode change.
	 *  @param OpNick The nick who changes the channel mode.
	 *  @param Channel The channel whose mode is changed.
	 *  @param uMode The mode character that is changed.
	 *  @param sArg The argument to the mode character, if any.
	 *  @param bAdded True if this mode is added ("+"), else false.
	 *  @param bNoChange True if this mode was already effective before.
	 */
	virtual void OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange);
	/** Called on any channel mode change. This is called before the more
	 *  detailed mode hooks like e.g. OnOp() and OnMode().
	 *  @param OpNick The nick who changes the channel mode.
	 *  @param Channel The channel whose mode is changed.
	 *  @param sModes The raw mode change, e.g. "+s-io".
	 *  @param sArgs All arguments to the mode change from sModes.
	 */
	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs);

	/** Called on any raw IRC line received from the <em>IRC server</em>.
	 *  @param sLine The line read from the server.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnRaw(CString& sLine);

	/** Called when a command to *status is sent.
	 *  @param sCommand The command sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnStatusCommand(CString& sCommand);
	/** Called when a command to your module is sent, e.g. query to *modname.
	 *  @param sCommand The command that was sent.
	 */
	virtual void OnModCommand(const CString& sCommand);
	/** This is similar to OnModCommand(), but it is only called if
	 * HandleCommand didn't find any that wants to handle this. This is only
	 * called if HandleCommand() is called, which practically means that
	 * this is only called if you don't overload OnModCommand().
	 *  @param sCommand The command that was sent.
	 */
	virtual void OnUnknownModCommand(const CString& sCommand);
	/** Called when a your module nick was sent a notice.
	 *  @param sMessage The message which was sent.
	 */
	virtual void OnModNotice(const CString& sMessage);
	/** Called when your module nick was sent a CTCP message. OnModCommand()
	 *  won't be called for this message.
	 *  @param sMessage The message which was sent.
	 */
	virtual void OnModCTCP(const CString& sMessage);

	/** Called when a nick quit from IRC.
	 *  @param Nick The nick which quit.
	 *  @param sMessage The quit message.
	 *  @param vChans List of channels which you and nick share.
	 */
	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans);
	/** Called when a nickname change occurs. If we are changing our nick,
	 *  sNewNick will equal m_pIRCSock->GetNick().
	 *  @param Nick The nick which changed its nickname
	 *  @param sNewNick The new nickname.
	 *  @param vChans Channels which we and nick share.
	 */
	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans);
	/** Called when a nick is kicked from a channel.
	 *  @param OpNick The nick which generated the kick.
	 *  @param sKickedNick The nick which was kicked.
	 *  @param Channel The channel on which this kick occurs.
	 *  @param sMessage The kick message.
	 */
	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage);
	/** Called when a nick joins a channel.
	 *  @param Nick The nick who joined.
	 *  @param Channel The channel which was joined.
	 */
	virtual void OnJoin(const CNick& Nick, CChan& Channel);
	/** Called when a nick parts a channel.
	 *  @param Nick The nick who parted.
	 *  @param Channel The channel which was parted.
	 *  @param sMessage The part message.
	 */
	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage);

	/** Called before a channel buffer is played back to a client.
	 *  @param Chan The channel which will be played back.
	 *  @param Client The client the buffer will be played back to.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client);
	/** Called after a channel buffer was played back to a client.
	 *  @param Chan The channel which was played back.
	 *  @param Client The client the buffer was played back to.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client);
	/** Called when for each line during a channel's buffer play back.
	 *  @param Chan The channel this playback is from.
	 *  @param Client The client the buffer is played back to.
	 *  @param sLine The current line of buffer playback. This is a raw IRC
	 *               traffic line!
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine);
	/** Called when a line from the query buffer is played back.
	 *  @param Client The client this line will go to.
	 *  @param sLine The raw IRC traffic line from the buffer.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnPrivBufferPlayLine(CClient& Client, CString& sLine);

	/** Called when a client successfully logged in to ZNC. */
	virtual void OnClientLogin();
	/** Called when a client disconnected from ZNC. */
	virtual void OnClientDisconnect();
	/** This module hook is called when a client sends a raw traffic line to ZNC.
	 *  @param sLine The raw traffic line sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserRaw(CString& sLine);
	/** This module hook is called when a client sends a CTCP reply.
	 *  @param sTarget The target for the CTCP reply. Could be a channel
	 *                 name or a nick name.
	 *  @param sMessage The CTCP reply message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage);
	/** This module hook is called when a client sends a CTCP request.
	 *  @param sTarget The target for the CTCP request. Could be a channel
	 *                 name or a nick name.
	 *  @param sMessage The CTCP request message.
	 *  @return See CModule::EModRet.
	 *  @note This is not called for CTCP ACTION messages, use
	 *        CModule::OnUserAction() instead.
	 */
	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage);
	/** Called when a client sends a CTCP ACTION request ("/me").
	 *  @param sTarget The target for the CTCP ACTION. Could be a channel
	 *                 name or a nick name.
	 *  @param sMessage The action message.
	 *  @return See CModule::EModRet.
	 *  @note CModule::OnUserCTCP() will not be called for this message.
	 */
	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage);
	/** This module hook is called when a user sends a normal IRC message.
	 *  @param sTarget The target of the message. Could be a channel name or
	 *                 a nick name.
	 *  @param sMessage The message which was sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage);
	/** This module hook is called when a user sends a notice message.
	 *  @param sTarget The target of the message. Could be a channel name or
	 *                 a nick name.
	 *  @param sMessage The message which was sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage);
	/** This hooks is called when a user sends a JOIN message.
	 *  @param sChannel The channel name the join is for.
	 *  @param sKey The key for the channel.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserJoin(CString& sChannel, CString& sKey);
	/** This hooks is called when a user sends a PART message.
	 *  @param sChannel The channel name the part is for.
	 *  @param sMessage The part message the client sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage);
	/** This module hook is called when a user wants to change a channel topic.
	 *  @param sChannel The channel.
	 *  @param sTopic The new topic which the user sent.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic);
	/** This hook is called when a user requests a channel's topic.
	 *  @param sChannel The channel for which the request is.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnUserTopicRequest(CString& sChannel);

	/** Called when we receive a CTCP reply <em>from IRC</em>.
	 *  @param Nick The nick the CTCP reply is from.
	 *  @param sMessage The CTCP reply message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage);
	/** Called when we receive a private CTCP request <em>from IRC</em>.
	 *  @param Nick The nick the CTCP request is from.
	 *  @param sMessage The CTCP request message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage);
	/** Called when we receive a channel CTCP request <em>from IRC</em>.
	 *  @param Nick The nick the CTCP request is from.
	 *  @param Channel The channel to which the request was sent.
	 *  @param sMessage The CTCP request message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage);
	/** Called when we receive a private CTCP ACTION ("/me" in query) <em>from IRC</em>.
	 *  This is called after CModule::OnPrivCTCP().
	 *  @param Nick The nick the action came from.
	 *  @param sMessage The action message
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage);
	/** Called when we receive a channel CTCP ACTION ("/me" in a channel) <em>from IRC</em>.
	 *  This is called after CModule::OnChanCTCP().
	 *  @param Nick The nick the action came from.
	 *  @param Channel The channel the action was sent to.
	 *  @param sMessage The action message
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage);
	/** Called when we receive a private message <em>from IRC</em>.
	 *  @param Nick The nick which sent the message.
	 *  @param sMessage The message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage);
	/** Called when we receive a channel message <em>from IRC</em>.
	 *  @param Nick The nick which sent the message.
	 *  @param Channel The channel to which the message was sent.
	 *  @param sMessage The message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage);
	/** Called when we receive a private notice.
	 *  @param Nick The nick which sent the notice.
	 *  @param sMessage The notice message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage);
	/** Called when we receive a channel notice.
	 *  @param Nick The nick which sent the notice.
	 *  @param Channel The channel to which the notice was sent.
	 *  @param sMessage The notice message.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage);
	/** Called when we receive a channel topic change <em>from IRC</em>.
	 *  @param Nick The nick which changed the topic.
	 *  @param Channel The channel whose topic was changed.
	 *  @param sTopic The new topic.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic);

	/** Called for every CAP received via CAP LS from server.
	 *  @param sCap capability supported by server.
	 *  @return true if your module supports this CAP and
	 *          needs to turn it on with CAP REQ.
	 */
	virtual bool OnServerCapAvailable(const CString& sCap);
	/** Called for every CAP accepted or rejected by server
	 *  (with CAP ACK or CAP NAK after our CAP REQ).
	 *  @param sCap capability accepted/rejected by server.
	 *  @param bSuccess true if capability was accepted, false if rejected.
	 */
	virtual void OnServerCapResult(const CString& sCap, bool bSuccess);

	/** This module hook is called just before ZNC tries to join a channel
	 *  by itself because it's in the config but wasn't joined yet.
	 *  @param Channel The channel which will be joined.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnTimerAutoJoin(CChan& Channel);

	ModHandle GetDLL() { return m_pDLL; }
	static double GetCoreVersion() { return VERSION; }

	/** This function sends a given raw IRC line to the IRC server, if we
	 *  are connected to one. Else this line is discarded.
	 *  @param sLine The line which should be sent.
	 *  @return true if the line was queued for sending.
	 */
	virtual bool PutIRC(const CString& sLine);
	/** This function sends a given raw IRC line to a client.
	 *  If we are in a module hook which is called for a specific client,
	 *  only that client will get the line, else all connected clients will
	 *  receive this line.
	 *  @param sLine The line which should be sent.
	 *  @return true if the line was sent to at least one client.
	 */
	virtual bool PutUser(const CString& sLine);
	/** This function generates a query from *status. If we are in a module
	 *  hook for a specific client, only that client gets this message, else
	 *  all connected clients will receive it.
	 *  @param sLine The message which should be sent from *status.
	 *  @return true if the line was sent to at least one client.
	 */
	virtual bool PutStatus(const CString& sLine);
	/** This function sends a query from your module nick. If we are in a
	 *  module hook for a specific client, only that client gets this
	 *  message, else all connected clients will receive it.
	 *  @param sLine The message which should be sent.
	 *  @return true if the line was sent to at least one client.
	 */
	virtual bool PutModule(const CString& sLine);
	/** This function calls CModule::PutModule(const CString&, const
	 *  CString&, const CString&) for each line in the table.
	 *  @param table The table which should be send.
	 *  @return The number of lines sent.
	 */
	virtual unsigned int PutModule(const CTable& table);
	/** Send a notice from your module nick. If we are in a module hook for
	 *  a specific client, only that client gets this notice, else all
	 *  clients will receive it.
	 *  @param sLine The line which should be sent.
	 *  @return true if the line was sent to at least one client.
	 */
	virtual bool PutModNotice(const CString& sLine);

	/** @returns The name of the module. */
	const CString& GetModName() const { return m_sModName; }

	/** @returns The nick of the module. This is just the module name
	 *           prefixed by the status prefix.
	 */
	CString GetModNick() const;

	/** Get the module's data dir.
	 *  Modules can be accompanied by static data, e.g. skins for webadmin.
	 *  These function will return the path to that data.
	 */
	const CString& GetModDataDir() const { return m_sDataDir; }

	// Timer stuff
	bool AddTimer(CTimer* pTimer);
	bool AddTimer(FPTimer_t pFBCallback, const CString& sLabel, u_int uInterval, u_int uCycles = 0, const CString& sDescription = "");
	bool RemTimer(CTimer* pTimer);
	bool RemTimer(const CString& sLabel);
	bool UnlinkTimer(CTimer* pTimer);
	CTimer* FindTimer(const CString& sLabel);
	set<CTimer*>::const_iterator BeginTimers() const { return m_sTimers.begin(); }
	set<CTimer*>::const_iterator EndTimers() const { return m_sTimers.end(); }
	virtual void ListTimers();
	// !Timer stuff

	// Socket stuff
	bool AddSocket(CSocket* pSocket);
	bool RemSocket(CSocket* pSocket);
	bool RemSocket(const CString& sSockName);
	bool UnlinkSocket(CSocket* pSocket);
	CSocket* FindSocket(const CString& sSockName);
	set<CSocket*>::const_iterator BeginSockets() const { return m_sSockets.begin(); }
	set<CSocket*>::const_iterator EndSockets() const { return m_sSockets.end(); }
	virtual void ListSockets();
	// !Socket stuff

	// Command stuff
	/// Register the "Help" command.
	void AddHelpCommand();
	/// @return True if the command was successfully added.
	bool AddCommand(const CModCommand& Command);
	/// @return True if the command was successfully added.
	bool AddCommand(const CString& sCmd, CModCommand::ModCmdFunc func, const CString& sArgs = "", const CString& sDesc = "");
	/// @return True if the command was successfully removed.
	bool RemCommand(const CString& sCmd);
	/// @return The CModCommand instance or NULL if none was found.
	const CModCommand* FindCommand(const CString& sCmd) const;
	/** This function tries to dispatch the given command via the correct
	 * instance of CModCommand. Before this can be called, commands have to
	 * be added via AddCommand(). If no matching commands are found then
	 * OnUnknownModCommand will be called.
	 * @param sLine The command line to handle.
	 * @return True if something was done, else false.
	 */
	bool HandleCommand(const CString& sLine);
	/** Send a description of all registered commands via PutModule().
	 * @param sLine The help command that is being asked for.
	 */
	void HandleHelpCommand(const CString& sLine = "");
	// !Command stuff

	bool LoadRegistry();
	bool SaveRegistry() const;
	bool SetNV(const CString & sName, const CString & sValue, bool bWriteToDisk = true);
	CString GetNV(const CString & sName) const;
	bool DelNV(const CString & sName, bool bWriteToDisk = true);
	MCString::iterator FindNV(const CString & sName) { return m_mssRegistry.find(sName); }
	MCString::iterator EndNV() { return m_mssRegistry.end(); }
	MCString::iterator BeginNV() { return m_mssRegistry.begin(); }
	void DelNV(MCString::iterator it) { m_mssRegistry.erase(it); }
	bool ClearNV(bool bWriteToDisk = true);

	const CString& GetSavePath() const;

	// Setters
	void SetGlobal(bool b) { m_bGlobal = b; }
	void SetDescription(const CString& s) { m_sDescription = s; }
	void SetModPath(const CString& s) { m_sModPath = s; }
	void SetArgs(const CString& s) { m_sArgs = s; }
	// !Setters

	// Getters
	bool IsGlobal() const { return m_bGlobal; }
	const CString& GetDescription() const { return m_sDescription; }
	const CString& GetArgs() const { return m_sArgs; }
	const CString& GetModPath() const { return m_sModPath; }

	/** @returns For user modules this returns the user for which this
	 *           module was loaded. For global modules this returns NULL,
	 *           except when we are in a user-specific module hook in which
	 *           case this is the user pointer.
	 */
	CUser* GetUser() { return m_pUser; }
	/** @returns NULL except when we are in a client-specific module hook in
	 *           which case this is the client for which the hook is called.
	 */
	CClient* GetClient() { return m_pClient; }
	CSockManager* GetManager() { return m_pManager; }
	// !Getters

protected:
	bool               m_bGlobal;
	CString            m_sDescription;
	set<CTimer*>       m_sTimers;
	set<CSocket*>      m_sSockets;
	ModHandle          m_pDLL;
	CSockManager*      m_pManager;
	CUser*             m_pUser;
	CClient*           m_pClient;
	CString            m_sModName;
	CString            m_sDataDir;
	CString            m_sSavePath;
	CString            m_sArgs;
	CString            m_sModPath;
private:
	MCString           m_mssRegistry; //!< way to save name/value pairs. Note there is no encryption involved in this
	VWebSubPages       m_vSubPages;
	map<CString, CModCommand> m_mCommands;
};

class CModules : public vector<CModule*> {
public:
	CModules();
	~CModules();

	void SetUser(CUser* pUser) { m_pUser = pUser; }
	void SetClient(CClient* pClient) { m_pClient = pClient; }
	CUser* GetUser() { return m_pUser; }
	CClient* GetClient() { return m_pClient; }

	void UnloadAll();

	bool OnBoot();
	bool OnPreRehash();
	bool OnPostRehash();
	bool OnIRCDisconnected();
	bool OnIRCConnected();
	bool OnIRCConnecting(CIRCSock *pIRCSock);
	bool OnIRCConnectionError(CIRCSock *pIRCSock);
	bool OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName);
	bool OnBroadcast(CString& sMessage);

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
	bool OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage);

	bool OnChanBufferStarting(CChan& Chan, CClient& Client);
	bool OnChanBufferEnding(CChan& Chan, CClient& Client);
	bool OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine);
	bool OnPrivBufferPlayLine(CClient& Client, CString& sLine);

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

	bool OnServerCapAvailable(const CString& sCap);
	bool OnServerCapResult(const CString& sCap, bool bSuccess);

	CModule* FindModule(const CString& sModule) const;
	bool LoadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);
	bool UnloadModule(const CString& sModule);
	bool UnloadModule(const CString& sModule, CString& sRetMsg);
	bool ReloadModule(const CString& sModule, const CString& sArgs, CUser* pUser, CString& sRetMsg);

	static bool GetModInfo(CModInfo& ModInfo, const CString& sModule, CString &sRetMsg);
	static bool GetModPathInfo(CModInfo& ModInfo, const CString& sModule, const CString& sModPath, CString &sRetMsg);
	static void GetAvailableMods(set<CModInfo>& ssMods, bool bGlobal = false);

	// This returns the path to the .so and to the data dir
	// which is where static data (webadmin skins) are saved
	static bool FindModPath(const CString& sModule, CString& sModPath,
			CString& sDataPath);
	// Return a list of <module dir, data dir> pairs for directories in
	// which modules can be found.
	typedef std::queue<std::pair<CString, CString> > ModDirList;
	static ModDirList GetModDirs();

private:
	static ModHandle OpenModule(const CString& sModule, const CString& sModPath,
			bool &bVersionMismatch, CModInfo& Info, CString& sRetMsg);

protected:
	CUser*    m_pUser;
	CClient*  m_pClient;
};

/** Base class for global modules. If you want to write a global module, your
 *  module class has to derive from CGlobalModule instead of CModule.
 *
 *  All the module hooks from CModule work here, too. The difference is that
 *  they are now called for all users instead of just a specific one.
 *
 *  Instead of MODCONSTRUCTOR and MODULEDEFS, you will have to use
 *  GLOBALMODCONSTRUCTOR and GLOBALMODULEDEFS.
 */
class CGlobalModule : public CModule {
public:
	CGlobalModule(ModHandle pDLL, const CString& sModName,
			const CString &sDataDir) : CModule(pDLL, sModName, sDataDir) {}
	virtual ~CGlobalModule() {}

	/** This module hook is called when a user is being added.
	 * @param User The user which will be added.
	 * @param sErrorRet A message that may be displayed to the user if
	 *                  the module stops adding the user.
	 * @return See CModule::EModRet.
	 */
	virtual EModRet OnAddUser(CUser& User, CString& sErrorRet);
	/** This module hook is called when a user is deleted.
	 *  @param User The user which will be deleted.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnDeleteUser(CUser& User);
	/** This module hook is called when there is an incoming connection on
	 *  any of ZNC's listening sockets.
	 *  @param pSock The incoming client socket.
	 *  @param sHost The IP the client is connecting from.
	 *  @param uPort The port the client is connecting from.
	 */
	virtual void OnClientConnect(CZNCSock* pSock, const CString& sHost, unsigned short uPort);
	/** This module hook is called when a client tries to login. If your
	 *  module wants to handle the login attempt, it must return
	 *  CModule::EModRet::HALT;
	 *  @param Auth The necessary authentication info for this login attempt.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth);
	/** Called after a client login was rejected.
	 *  @param sUsername The username that tried to log in.
	 *  @param sRemoteIP The IP address from which the client tried to login.
	 */
	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP);
	/** This function behaves like CModule::OnRaw(), but is also called
	 *  before the client successfully logged in to ZNC. You should always
	 *  prefer to use CModule::OnRaw() if possible.
	 *  @param sLine The raw traffic line which the client sent.
	 *  @todo Why doesn't this use m_pUser and m_pClient?
	 *        (Well, ok, m_pUser isn't known yet...)
	 */
	virtual EModRet OnUnknownUserRaw(CString& sLine);

	/** Called when a client told us CAP LS. Use ssCaps.insert("cap-name")
	 *  for announcing capabilities which your module supports.
	 *  @param ssCaps set of caps which will be sent to client.
	 */
	virtual void OnClientCapLs(SCString& ssCaps);
	/** Called only to check if your module supports turning on/off named capability.
	 *  @param sCap name of capability.
	 *  @param bState On or off, depending on which case is interesting for client.
	 *  @return true if your module supports this capability in the specified state.
	 */
	virtual bool IsClientCapSupported(const CString& sCap, bool bState);
	/** Called when we actually need to turn a capability on or off for a client.
	 *  @param sCap name of wanted capability.
	 *  @param bState On or off, depending on which case client needs.
	 */
	virtual void OnClientCapRequest(const CString& sCap, bool bState);

	/** Called when a module is going to be loaded.
	 *  @param sModName name of the module.
	 *  @param sArgs arguments of the module.
	 *  @param[out] bSuccess the module was loaded successfully
	 *                       as result of this module hook?
	 *  @param[out] sRetMsg text about loading of the module.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnModuleLoading(const CString& sModName, const CString& sArgs,
			bool& bSuccess, CString& sRetMsg);
	/** Called when a module is going to be unloaded.
	 *  @param pModule the module.
	 *  @param[out] bSuccess the module was unloaded successfully
	 *                       as result of this module hook?
	 *  @param[out] sRetMsg text about unloading of the module.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg);
	/** Called when info about a module is needed.
	 *  @param[out] ModInfo put result here, if your module knows it.
	 *  @param sModule name of the module.
	 *  @param bSuccess this module provided info about the module.
	 *  @param sRetMsg text describing possible issues.
	 *  @return See CModule::EModRet.
	 */
	virtual EModRet OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg);
	/** Called when list of available mods is requested.
	 *  @param ssMods put new modules here.
	 *  @param bGlobal true if global modules are needed.
	 */
	virtual void OnGetAvailableMods(set<CModInfo>& ssMods, bool bGlobal);
private:
};

class CGlobalModules : public CModules {
public:
	CGlobalModules() : CModules() {}
	~CGlobalModules() {}

	bool OnAddUser(CUser& User, CString& sErrorRet);
	bool OnDeleteUser(CUser& User);
	bool OnClientConnect(CZNCSock* pSock, const CString& sHost, unsigned short uPort);
	bool OnLoginAttempt(CSmartPtr<CAuthBase> Auth);
	bool OnFailedLogin(const CString& sUsername, const CString& sRemoteIP);
	bool OnUnknownUserRaw(CString& sLine);
	bool OnClientCapLs(SCString& ssCaps);
	bool IsClientCapSupported(const CString& sCap, bool bState);
	bool OnClientCapRequest(const CString& sCap, bool bState);
	bool OnModuleLoading(const CString& sModName, const CString& sArgs,
			bool& bSuccess, CString& sRetMsg);
	bool OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg);
	bool OnGetModInfo(CModInfo& ModInfo, const CString& sModule,
			bool& bSuccess, CString& sRetMsg);
	bool OnGetAvailableMods(set<CModInfo>& ssMods, bool bGlobal);
private:
};

#endif // !_MODULES_H
