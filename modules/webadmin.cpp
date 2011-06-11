/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "HTTPSock.h"
#include "Server.h"
#include "Template.h"
#include "User.h"
#include "znc.h"
#include "WebModules.h"
#include "ZNCString.h"
#include "Listener.h"
#include <sstream>
#include <utility>

using std::stringstream;
using std::make_pair;

/* Stuff to be able to write this:
   // i will be name of local variable, see below
   // pUser can be NULL if only global modules are needed
   FOR_EACH_MODULE(i, pUser) {
       // i is local variable of type CModules::iterator,
	   // so *i has type CModule*
   }
*/
struct FOR_EACH_MODULE_Type {
	bool bOnCMuser;
	CModules CMtemp;
	CModules& CMuser;
	FOR_EACH_MODULE_Type(CUser* pUser) : CMuser(pUser ? pUser->GetModules() : CMtemp) {
		bOnCMuser = false;
	}
	operator bool() { return false; }
};

inline bool FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Type& state, CModules::iterator& i) {
	if (!state.bOnCMuser && i == CZNC::Get().GetModules().end()) {
		i = state.CMuser.begin();
		state.bOnCMuser = true;
	}
	return !(state.bOnCMuser && i == state.CMuser.end());
}

#define FOR_EACH_MODULE(I, pUser)\
	if (FOR_EACH_MODULE_Type FOR_EACH_MODULE_Var = pUser) {} else\
	for (CModules::iterator I = CZNC::Get().GetModules().begin(); FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Var, I); ++I)

class CWebAdminMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CWebAdminMod) {
		VPair vParams;
		vParams.push_back(make_pair("user", ""));
		AddSubPage(new CWebSubPage("settings", "Global Settings", CWebSubPage::F_ADMIN));
		AddSubPage(new CWebSubPage("edituser", "Your Settings", vParams));
		AddSubPage(new CWebSubPage("traffic", "Traffic Info", CWebSubPage::F_ADMIN));
		AddSubPage(new CWebSubPage("listusers", "List Users", CWebSubPage::F_ADMIN));
		AddSubPage(new CWebSubPage("adduser", "Add User", CWebSubPage::F_ADMIN));
	}

	virtual ~CWebAdminMod() {
	}

	virtual bool OnLoad(const CString& sArgStr, CString& sMessage) {
		if (sArgStr.empty())
			return true;

		// We don't accept any arguments, but for backwards
		// compatibility we have to do some magic here.
		sMessage = "Arguments converted to new syntax";

		bool bSSL = false;
		bool bIPv6 = false;
		bool bShareIRCPorts = true;
		unsigned short uPort = 8080;
		CString sArgs(sArgStr);
		CString sPort;
		CString sListenHost;

		while (sArgs.Left(1) == "-") {
			CString sOpt = sArgs.Token(0);
			sArgs = sArgs.Token(1, true);

			if (sOpt.Equals("-IPV6")) {
				bIPv6 = true;
			} else if (sOpt.Equals("-IPV4")) {
				bIPv6 = false;
			} else if (sOpt.Equals("-noircport")) {
				bShareIRCPorts = false;
			} else {
				// Uhm... Unknown option? Let's just ignore all
				// arguments, older versions would have returned
				// an error and denied loading
				return true;
			}
		}

		// No arguments left: Only port sharing
		if (sArgs.empty() && bShareIRCPorts)
			return true;

		if (sArgs.find(" ") != CString::npos) {
			sListenHost = sArgs.Token(0);
			sPort = sArgs.Token(1, true);
		} else {
			sPort = sArgs;
		}

		if (sPort.Left(1) == "+") {
			sPort.TrimLeft("+");
			bSSL = true;
		}

		if (!sPort.empty()) {
			uPort = sPort.ToUShort();
		}

		if (!bShareIRCPorts) {
			// Make all existing listeners IRC-only
			const vector<CListener*>& vListeners = CZNC::Get().GetListeners();
			vector<CListener*>::const_iterator it;
			for (it = vListeners.begin(); it != vListeners.end(); ++it) {
				(*it)->SetAcceptType(CListener::ACCEPT_IRC);
			}
		}

		// Now turn that into a listener instance
		CListener *pListener = new CListener(uPort, sListenHost, bSSL,
				(!bIPv6 ? ADDR_IPV4ONLY : ADDR_ALL), CListener::ACCEPT_HTTP);

		if (!pListener->Listen()) {
			sMessage = "Failed to add backwards-compatible listener";
			return false;
		}
		CZNC::Get().AddListener(pListener);

		SetArgs("");
		return true;
	}

	CString GetModArgs(CUser* pUser, const CString& sModName, bool bGlobal = false) {
		if (!bGlobal && !pUser) {
			return "";
		}

		CModules& Modules = (bGlobal) ? CZNC::Get().GetModules() : pUser->GetModules();

		for (unsigned int a = 0; a < Modules.size(); a++) {
			CModule* pModule = Modules[a];

			if (pModule->GetModName() == sModName) {
				return pModule->GetArgs();
			}
		}

		return "";
	}

	CUser* GetNewUser(CWebSock& WebSock, CUser* pUser) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		CString sUsername = WebSock.GetParam("newuser");

		if (sUsername.empty()) {
			sUsername = WebSock.GetParam("user");
		}

		if (sUsername.empty()) {
			WebSock.PrintErrorPage("Invalid Submission [Username is required]");
			return NULL;
		}

		if (pUser) {
			/* If we are editing a user we must not change the user name */
			sUsername = pUser->GetUserName();
		}

		CString sArg = WebSock.GetParam("password");

		if (sArg != WebSock.GetParam("password2")) {
			WebSock.PrintErrorPage("Invalid Submission [Passwords do not match]");
			return NULL;
		}

		CUser* pNewUser = new CUser(sUsername);

		if (!sArg.empty()) {
			CString sSalt = CUtils::GetSalt();
			CString sHash = CUser::SaltedHash(sArg, sSalt);
			pNewUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
		}

		VCString vsArgs;
		WebSock.GetRawParam("servers").Split("\n", vsArgs);
		unsigned int a = 0;

		for (a = 0; a < vsArgs.size(); a++) {
			pNewUser->AddServer(vsArgs[a].Trim_n());
		}

		WebSock.GetRawParam("allowedips").Split("\n", vsArgs);
		if (vsArgs.size()) {
			for (a = 0; a < vsArgs.size(); a++) {
				pNewUser->AddAllowedHost(vsArgs[a].Trim_n());
			}
		} else {
			pNewUser->AddAllowedHost("*");
		}

		WebSock.GetRawParam("ctcpreplies").Split("\n", vsArgs);
		for (a = 0; a < vsArgs.size(); a++) {
			CString sReply = vsArgs[a].TrimRight_n("\r");
			pNewUser->AddCTCPReply(sReply.Token(0).Trim_n(), sReply.Token(1, true).Trim_n());
		}

		sArg = WebSock.GetParam("nick"); if (!sArg.empty()) { pNewUser->SetNick(sArg); }
		sArg = WebSock.GetParam("altnick"); if (!sArg.empty()) { pNewUser->SetAltNick(sArg); }
		sArg = WebSock.GetParam("statusprefix"); if (!sArg.empty()) { pNewUser->SetStatusPrefix(sArg); }
		sArg = WebSock.GetParam("ident"); if (!sArg.empty()) { pNewUser->SetIdent(sArg); }
		sArg = WebSock.GetParam("skin"); if (!sArg.empty()) { pNewUser->SetSkinName(sArg); }
		sArg = WebSock.GetParam("realname"); if (!sArg.empty()) { pNewUser->SetRealName(sArg); }
		sArg = WebSock.GetParam("quitmsg"); if (!sArg.empty()) { pNewUser->SetQuitMsg(sArg); }
		sArg = WebSock.GetParam("chanmodes"); if (!sArg.empty()) { pNewUser->SetDefaultChanModes(sArg); }
		sArg = WebSock.GetParam("timestampformat"); if (!sArg.empty()) { pNewUser->SetTimestampFormat(sArg); }

		sArg = WebSock.GetParam("bindhost");
		// To change BindHosts be admin or don't have DenySetBindHost
		if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
			CString sArg2 = WebSock.GetParam("dccbindhost");
			if (!sArg.empty()) {
				pNewUser->SetBindHost(sArg);
			}
			if (!sArg2.empty()) {
				pNewUser->SetDCCBindHost(sArg2);
			}
		} else if (pUser){
			pNewUser->SetBindHost(pUser->GetBindHost());
			pNewUser->SetDCCBindHost(pUser->GetDCCBindHost());
		}

		// First apply the old limit in case the new one is too high
		if (pUser)
			pNewUser->SetBufferCount(pUser->GetBufferCount(), true);
		pNewUser->SetBufferCount(WebSock.GetParam("bufsize").ToUInt(), spSession->IsAdmin());
		pNewUser->SetSkinName(WebSock.GetParam("skin"));
		pNewUser->SetKeepBuffer(WebSock.GetParam("keepbuffer").ToBool());
		pNewUser->SetMultiClients(WebSock.GetParam("multiclients").ToBool());
		pNewUser->SetTimestampAppend(WebSock.GetParam("appendtimestamp").ToBool());
		pNewUser->SetTimestampPrepend(WebSock.GetParam("prependtimestamp").ToBool());
		pNewUser->SetTimezoneOffset(WebSock.GetParam("timezoneoffset").ToDouble());
		pNewUser->SetJoinTries(WebSock.GetParam("jointries").ToUInt());
		pNewUser->SetMaxJoins(WebSock.GetParam("maxjoins").ToUInt());
		pNewUser->SetIRCConnectEnabled(WebSock.GetParam("doconnect").ToBool());

		if (spSession->IsAdmin()) {
			pNewUser->SetDenyLoadMod(WebSock.GetParam("denyloadmod").ToBool());
			pNewUser->SetDenySetBindHost(WebSock.GetParam("denysetbindhost").ToBool());
		} else if (pUser) {
			pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
			pNewUser->SetDenySetBindHost(pUser->DenySetBindHost());
		}

		// If pUser is not NULL, we are editing an existing user.
		// Users must not be able to change their own admin flag.
		if (pUser != CZNC::Get().FindUser(WebSock.GetUser())) {
			pNewUser->SetAdmin(WebSock.GetParam("isadmin").ToBool());
		} else if (pUser) {
			pNewUser->SetAdmin(pUser->IsAdmin());
		}

		WebSock.GetParamValues("channel", vsArgs);
		for (a = 0; a < vsArgs.size(); a++) {
			const CString& sChan = vsArgs[a];
			pNewUser->AddChan(sChan.TrimRight_n("\r"), WebSock.GetParam("save_" + sChan).ToBool());
		}

		if (spSession->IsAdmin() || (pUser && !pUser->DenyLoadMod())) {
			WebSock.GetParamValues("loadmod", vsArgs);

			for (a = 0; a < vsArgs.size(); a++) {
				CString sModRet;
				CString sModName = vsArgs[a].TrimRight_n("\r");
				CString sModLoadError;

				if (!sModName.empty()) {
					CString sArgs = WebSock.GetParam("modargs_" + sModName);

					try {
						if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet)) {
							sModLoadError = "Unable to load module [" + sModName + "] [" + sModRet + "]";
						}
					} catch (...) {
						sModLoadError = "Unable to load module [" + sModName + "] [" + sArgs + "]";
					}

					if (!sModLoadError.empty()) {
						DEBUG(sModLoadError);
						spSession->AddError(sModLoadError);
					}
				}
			}
		} else if (pUser) {
			CModules& Modules = pUser->GetModules();

			for (a = 0; a < Modules.size(); a++) {
				CString sModName = Modules[a]->GetModName();
				CString sArgs = Modules[a]->GetArgs();
				CString sModRet;
				CString sModLoadError;

				try {
					if (!pNewUser->GetModules().LoadModule(sModName, sArgs, pNewUser, sModRet)) {
						sModLoadError = "Unable to load module [" + sModName + "] [" + sModRet + "]";
					}
				} catch (...) {
					sModLoadError = "Unable to load module [" + sModName + "]";
				}

				if (!sModLoadError.empty()) {
					DEBUG(sModLoadError);
					spSession->AddError(sModLoadError);
				}
			}
		}

		return pNewUser;
	}

	CString SafeGetUserNameParam(CWebSock& WebSock) {
		CString sUserName = WebSock.GetParam("user"); // check for POST param
		if(sUserName.empty() && !WebSock.IsPost()) {
			// if no POST param named user has been given and we are not
			// saving this form, fall back to using the GET parameter.
			sUserName = WebSock.GetParam("user", false);
		}
		return sUserName;
	}

	CUser* SafeGetUserFromParam(CWebSock& WebSock) {
		return CZNC::Get().FindUser(SafeGetUserNameParam(WebSock));
	}

	virtual CString GetWebMenuTitle() { return "webadmin"; }
	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();

		if (sPageName == "settings") {
			// Admin Check
			if (!spSession->IsAdmin()) {
				return false;
			}

			return SettingsPage(WebSock, Tmpl);
		} else if (sPageName == "adduser") {
			// Admin Check
			if (!spSession->IsAdmin()) {
				return false;
			}

			return UserPage(WebSock, Tmpl);
		} else if (sPageName == "editchan") {
			CUser* pUser = SafeGetUserFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			if (!pUser) {
				WebSock.PrintErrorPage("No such username");
				return true;
			}

			CString sChan = WebSock.GetParam("name");
			if(sChan.empty() && !WebSock.IsPost()) {
				sChan = WebSock.GetParam("name", false);
			}
			CChan* pChan = pUser->FindChan(sChan);
			if (!pChan) {
				WebSock.PrintErrorPage("No such channel");
				return true;
			}

			return ChanPage(WebSock, Tmpl, pUser, pChan);
		} else if (sPageName == "addchan") {
			CUser* pUser = SafeGetUserFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			if (pUser) {
				return ChanPage(WebSock, Tmpl, pUser);
			}

			WebSock.PrintErrorPage("No such username");
			return true;
		} else if (sPageName == "delchan") {
			CUser* pUser = CZNC::Get().FindUser(WebSock.GetParam("user", false));

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			if (pUser) {
				return DelChan(WebSock, pUser);
			}

			WebSock.PrintErrorPage("No such username");
			return true;
		} else if (sPageName == "deluser") {
			if (!spSession->IsAdmin()) {
				return false;
			}

			if (!WebSock.IsPost()) {
				// Show the "Are you sure?" page:

				CString sUser = WebSock.GetParam("user", false);
				CUser* pUser = CZNC::Get().FindUser(sUser);

				if (!pUser) {
					WebSock.PrintErrorPage("No such username");
					return true;
				}

				Tmpl.SetFile("del_user.tmpl");
				Tmpl["Username"] = sUser;
				return true;
			}

			// The "Are you sure?" page has been submitted with "Yes",
			// so we actually delete the user now:

			CString sUser = WebSock.GetParam("user");
			CUser* pUser = CZNC::Get().FindUser(sUser);

			if (pUser && pUser == spSession->GetUser()) {
				WebSock.PrintErrorPage("Please don't delete yourself, suicide is not the answer!");
				return true;
			} else if (CZNC::Get().DeleteUser(sUser)) {
				WebSock.Redirect("listusers");
				return true;
			}

			WebSock.PrintErrorPage("No such username");
			return true;
		} else if (sPageName == "edituser") {
			CString sUserName = SafeGetUserNameParam(WebSock);
			CUser* pUser = CZNC::Get().FindUser(sUserName);

			if(!pUser) {
				if(sUserName.empty()) {
					pUser = spSession->GetUser();
				} // else: the "no such user" message will be printed.
			}

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			if (pUser) {
				return UserPage(WebSock, Tmpl, pUser);
			}

			WebSock.PrintErrorPage("No such username");
			return true;
		} else if (sPageName == "listusers" && spSession->IsAdmin()) {
			return ListUsersPage(WebSock, Tmpl);
		} else if (sPageName == "traffic" && spSession->IsAdmin()) {
			return TrafficPage(WebSock, Tmpl);
		} else if (sPageName == "index") {
			return true;
		}

		return false;
	}

	bool ChanPage(CWebSock& WebSock, CTemplate& Tmpl, CUser* pUser, CChan* pChan = NULL) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl.SetFile("add_edit_chan.tmpl");

		if (!pUser) {
			WebSock.PrintErrorPage("That user doesn't exist");
			return true;
		}

		if (!WebSock.GetParam("submitted").ToUInt()) {
			Tmpl["User"] = pUser->GetUserName();

			if (pChan) {
				Tmpl["Action"] = "editchan";
				Tmpl["Edit"] = "true";
				Tmpl["Title"] = "Edit Channel" + CString(" [" + pChan->GetName() + "]");
				Tmpl["ChanName"] = pChan->GetName();
				Tmpl["BufferCount"] = CString(pChan->GetBufferCount());
				Tmpl["DefModes"] = pChan->GetDefaultModes();
				Tmpl["Key"] = pChan->GetKey();

				if (pChan->InConfig()) {
					Tmpl["InConfig"] = "true";
				}
			} else {
				Tmpl["Action"] = "addchan";
				Tmpl["Title"] = "Add Channel" + CString(" for User [" + pUser->GetUserName() + "]");
				Tmpl["BufferCount"] = CString(pUser->GetBufferCount());
				Tmpl["DefModes"] = CString(pUser->GetDefaultChanModes());
				Tmpl["InConfig"] = "true";
			}

			// o1 used to be AutoCycle which was removed

			CTemplate& o2 = Tmpl.AddRow("OptionLoop");
			o2["Name"] = "keepbuffer";
			o2["DisplayName"] = "Keep Buffer";
			if ((pChan && pChan->KeepBuffer()) || (!pChan && pUser->KeepBuffer())) { o2["Checked"] = "true"; }

			CTemplate& o3 = Tmpl.AddRow("OptionLoop");
			o3["Name"] = "detached";
			o3["DisplayName"] = "Detached";
			if (pChan && pChan->IsDetached()) { o3["Checked"] = "true"; }

			FOR_EACH_MODULE(i, pUser) {
				CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
				mod.insert(Tmpl.begin(), Tmpl.end());
				mod["WebadminAction"] = "display";
				if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/channel", mod)) {
					mod["Embed"] = WebSock.FindTmpl(*i, "WebadminChan.tmpl");
					mod["ModName"] = (*i)->GetModName();
				}
			}

			return true;
		}

		CString sChanName = WebSock.GetParam("name").Trim_n();

		if (!pChan) {
			if (sChanName.empty()) {
				WebSock.PrintErrorPage("Channel name is a required argument");
				return true;
			}

			if (pUser->FindChan(sChanName.Token(0))) {
				WebSock.PrintErrorPage("Channel [" + sChanName.Token(0) + "] already exists");
				return true;
			}

			pChan = new CChan(sChanName, pUser, true);
			pUser->AddChan(pChan);
		}

		pChan->SetBufferCount(WebSock.GetParam("buffercount").ToUInt(), spSession->IsAdmin());
		pChan->SetDefaultModes(WebSock.GetParam("defmodes"));
		pChan->SetInConfig(WebSock.GetParam("save").ToBool());
		pChan->SetKeepBuffer(WebSock.GetParam("keepbuffer").ToBool());
		pChan->SetKey(WebSock.GetParam("key"));

		bool bDetached = WebSock.GetParam("detached").ToBool();

		if (pChan->IsDetached() != bDetached) {
			if (bDetached) {
				pChan->DetachUser();
			} else {
				pChan->AttachUser();
			}
		}

		CTemplate TmplMod;
		TmplMod["User"] = pUser->GetUserName();
		TmplMod["ChanName"] = sChanName;
		TmplMod["WebadminAction"] = "change";
		FOR_EACH_MODULE(it, pUser) {
			(*it)->OnEmbeddedWebRequest(WebSock, "webadmin/channel", TmplMod);
		}

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Channel added/modified, but config was not written");
			return true;
		}

		WebSock.Redirect("edituser?user=" + pUser->GetUserName().Escape_n(CString::EURL));
		return true;
	}

	bool DelChan(CWebSock& WebSock, CUser* pUser) {
		CString sChan = WebSock.GetParam("name", false);

		if (!pUser) {
			WebSock.PrintErrorPage("That user doesn't exist");
			return true;
		}

		if (sChan.empty()) {
			WebSock.PrintErrorPage("That channel doesn't exist for this user");
			return true;
		}

		pUser->DelChan(sChan);
		pUser->PutIRC("PART " + sChan);

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Channel deleted, but config was not written");
			return true;
		}

		WebSock.Redirect("edituser?user=" + pUser->GetUserName().Escape_n(CString::EURL));
		return false;
	}

	bool UserPage(CWebSock& WebSock, CTemplate& Tmpl, CUser* pUser = NULL) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl.SetFile("add_edit_user.tmpl");

		if (!WebSock.GetParam("submitted").ToUInt()) {
			CString sAllowedHosts, sServers, sChans, sCTCPReplies;

			if (pUser) {
				Tmpl["Action"] = "edituser";
				Tmpl["Title"] = "Edit User [" + pUser->GetUserName() + "]";
				Tmpl["Edit"] = "true";
				Tmpl["Username"] = pUser->GetUserName();
				Tmpl["Nick"] = pUser->GetNick();
				Tmpl["AltNick"] = pUser->GetAltNick();
				Tmpl["StatusPrefix"] = pUser->GetStatusPrefix();
				Tmpl["Ident"] = pUser->GetIdent();
				Tmpl["RealName"] = pUser->GetRealName();
				Tmpl["QuitMsg"] = pUser->GetQuitMsg();
				Tmpl["DefaultChanModes"] = pUser->GetDefaultChanModes();
				Tmpl["BufferCount"] = CString(pUser->GetBufferCount());
				Tmpl["TimestampFormat"] = pUser->GetTimestampFormat();
				Tmpl["TimezoneOffset"] = CString(pUser->GetTimezoneOffset());
				Tmpl["JoinTries"] = CString(pUser->JoinTries());
				Tmpl["MaxJoins"] = CString(pUser->MaxJoins());
				Tmpl["IRCConnectEnabled"] = CString(pUser->GetIRCConnectEnabled());

				const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
				for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); ++it) {
					CTemplate& l = Tmpl.AddRow("AllowedHostLoop");
					l["Host"] = *it;
				}

				const vector<CServer*>& vServers = pUser->GetServers();
				for (unsigned int a = 0; a < vServers.size(); a++) {
					CTemplate& l = Tmpl.AddRow("ServerLoop");
					l["Server"] = vServers[a]->GetString();
				}

				const MCString& msCTCPReplies = pUser->GetCTCPReplies();
				for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); it2++) {
					CTemplate& l = Tmpl.AddRow("CTCPLoop");
					l["CTCP"] = it2->first + " " + it2->second;
				}

				const vector<CChan*>& Channels = pUser->GetChans();
				for (unsigned int c = 0; c < Channels.size(); c++) {
					CChan* pChan = Channels[c];
					CTemplate& l = Tmpl.AddRow("ChannelLoop");

					l["Username"] = pUser->GetUserName();
					l["Name"] = pChan->GetName();
					l["Perms"] = pChan->GetPermStr();
					l["CurModes"] = pChan->GetModeString();
					l["DefModes"] = pChan->GetDefaultModes();
					l["BufferCount"] = CString(pChan->GetBufferCount());
					l["Options"] = pChan->GetOptions();

					if (pChan->InConfig()) {
						l["InConfig"] = "true";
					}
				}
			} else {
				Tmpl["Action"] = "adduser";
				Tmpl["Title"] = "Add User";
				Tmpl["StatusPrefix"] = "*";
				Tmpl["IRCConnectEnabled"] = "true";
			}

			// To change BindHosts be admin or don't have DenySetBindHost
			if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
				const VCString& vsBindHosts = CZNC::Get().GetBindHosts();
				bool bFoundBindHost = false;
				bool bFoundDCCBindHost = false;
				for (unsigned int b = 0; b < vsBindHosts.size(); b++) {
					const CString& sBindHost = vsBindHosts[b];
					CTemplate& l = Tmpl.AddRow("BindHostLoop");
					CTemplate& k = Tmpl.AddRow("DCCBindHostLoop");

					l["BindHost"] = sBindHost;
					k["BindHost"] = sBindHost;

					if (pUser && pUser->GetBindHost() == sBindHost) {
						l["Checked"] = "true";
						bFoundBindHost = true;
					}

					if (pUser && pUser->GetDCCBindHost() == sBindHost) {
						k["Checked"] = "true";
						bFoundDCCBindHost = true;
					}
				}

				// If our current bindhost is not in the global list...
				if (pUser && !bFoundBindHost && !pUser->GetBindHost().empty()) {
					CTemplate& l = Tmpl.AddRow("BindHostLoop");

					l["BindHost"] = pUser->GetBindHost();
					l["Checked"] = "true";
				}
				if (pUser && !bFoundDCCBindHost && !pUser->GetDCCBindHost().empty()) {
					CTemplate& l = Tmpl.AddRow("DCCBindHostLoop");

					l["BindHost"] = pUser->GetDCCBindHost();
					l["Checked"] = "true";
				}
			}

			vector<CString> vDirs;
			WebSock.GetAvailSkins(vDirs);

			for (unsigned int d = 0; d < vDirs.size(); d++) {
				const CString& SubDir = vDirs[d];
				CTemplate& l = Tmpl.AddRow("SkinLoop");
				l["Name"] = SubDir;

				if (pUser && SubDir == pUser->GetSkinName()) {
					l["Checked"] = "true";
				}
			}

			set<CModInfo> ssUserMods;
			CZNC::Get().GetModules().GetAvailableMods(ssUserMods);

			for (set<CModInfo>::iterator it = ssUserMods.begin(); it != ssUserMods.end(); ++it) {
				const CModInfo& Info = *it;
				CTemplate& l = Tmpl.AddRow("ModuleLoop");

				l["Name"] = Info.GetName();
				l["Description"] = Info.GetDescription();
				l["Args"] = GetModArgs(pUser, Info.GetName());
				l["Wiki"] = Info.GetWikiPage();

				if (pUser && pUser->GetModules().FindModule(Info.GetName())) {
					l["Checked"] = "true";
				}

				if (!spSession->IsAdmin() && pUser && pUser->DenyLoadMod()) {
					l["Disabled"] = "true";
				}
			}

			CTemplate& o1 = Tmpl.AddRow("OptionLoop");
			o1["Name"] = "keepbuffer";
			o1["DisplayName"] = "Keep Buffer";
			if (!pUser || pUser->KeepBuffer()) { o1["Checked"] = "true"; }

			/* o2 used to be auto cycle which was removed */

			CTemplate& o4 = Tmpl.AddRow("OptionLoop");
			o4["Name"] = "multiclients";
			o4["DisplayName"] = "Multi Clients";
			if (!pUser || pUser->MultiClients()) { o4["Checked"] = "true"; }

			CTemplate& o7 = Tmpl.AddRow("OptionLoop");
			o7["Name"] = "appendtimestamp";
			o7["DisplayName"] = "Append Timestamps";
			if (pUser && pUser->GetTimestampAppend()) { o7["Checked"] = "true"; }

			CTemplate& o8 = Tmpl.AddRow("OptionLoop");
			o8["Name"] = "prependtimestamp";
			o8["DisplayName"] = "Prepend Timestamps";
			if (pUser && pUser->GetTimestampPrepend()) { o8["Checked"] = "true"; }

			if (spSession->IsAdmin()) {
				CTemplate& o9 = Tmpl.AddRow("OptionLoop");
				o9["Name"] = "denyloadmod";
				o9["DisplayName"] = "Deny LoadMod";
				if (pUser && pUser->DenyLoadMod()) { o9["Checked"] = "true"; }

				CTemplate& o10 = Tmpl.AddRow("OptionLoop");
				o10["Name"] = "isadmin";
				o10["DisplayName"] = "Admin";
				if (pUser && pUser->IsAdmin()) { o10["Checked"] = "true"; }
				if (pUser && pUser == CZNC::Get().FindUser(WebSock.GetUser())) { o10["Disabled"] = "true"; }

				CTemplate& o11 = Tmpl.AddRow("OptionLoop");
				o11["Name"] = "denysetbindhost";
				o11["DisplayName"] = "Deny SetBindHost";
				if (pUser && pUser->DenySetBindHost()) { o11["Checked"] = "true"; }
			}

			FOR_EACH_MODULE(i, pUser) {
				CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
				mod.insert(Tmpl.begin(), Tmpl.end());
				mod["WebadminAction"] = "display";
				if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/user", mod)) {
					mod["Embed"] = WebSock.FindTmpl(*i, "WebadminUser.tmpl");
					mod["ModName"] = (*i)->GetModName();
				}
			}

			return true;
		}

		/* If pUser is NULL, we are adding a user, else we are editing this one */

		CString sUsername = WebSock.GetParam("user");
		if (!pUser && CZNC::Get().FindUser(sUsername)) {
			WebSock.PrintErrorPage("Invalid Submission [User " + sUsername + " already exists]");
			return true;
		}

		CUser* pNewUser = GetNewUser(WebSock, pUser);
		if (!pNewUser) {
			return true;
		}

		CString sErr;
		CString sAction;

		if (!pUser) {
			// Add User Submission
			if (!CZNC::Get().AddUser(pNewUser, sErr)) {
				delete pNewUser;
				WebSock.PrintErrorPage("Invalid submission [" + sErr + "]");
				return true;
			}

			pUser = pNewUser;
			sAction = "added";
		} else {
			// Edit User Submission
			if (!pUser->Clone(*pNewUser, sErr, false)) {
				delete pNewUser;
				WebSock.PrintErrorPage("Invalid Submission [" + sErr + "]");
				return true;
			}

			delete pNewUser;
			sAction = "edited";
		}

		CTemplate TmplMod;
		TmplMod["Username"] = sUsername;
		TmplMod["WebadminAction"] = "change";
		FOR_EACH_MODULE(it, pUser) {
			(*it)->OnEmbeddedWebRequest(WebSock, "webadmin/user", TmplMod);
		}

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("User " + sAction + ", but config was not written");
			return true;
		}

		if (!spSession->IsAdmin()) {
			WebSock.Redirect("edituser");
		} else {
			WebSock.Redirect("listusers");
		}

		/* we don't want the template to be printed while we redirect */
		return false;
	}

	bool ListUsersPage(CWebSock& WebSock, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
		Tmpl["Title"] = "List Users";
		Tmpl["Action"] = "listusers";

		unsigned int a = 0;

		for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it, a++) {
			CServer* pServer = it->second->GetCurrentServer();
			CTemplate& l = Tmpl.AddRow("UserLoop");
			CUser& User = *it->second;

			l["Username"] = User.GetUserName();
			l["Clients"] = CString(User.GetClients().size());
			l["IRCNick"] = User.GetIRCNick().GetNick();

			if (&User == spSession->GetUser()) {
				l["IsSelf"] = "true";
			}

			if (pServer) {
				l["Server"] = pServer->GetName();
			}
		}

		return true;
	}

	bool TrafficPage(CWebSock& WebSock, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl["Uptime"] = CZNC::Get().GetUptime();

		const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
		Tmpl["TotalUsers"] = CString(msUsers.size());

		unsigned int uiAttached = 0, uiClients = 0, uiServers = 0;

		for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			CUser& User = *it->second;
			if (User.IsUserAttached()) {
				uiAttached++;
			}
			if (User.IsIRCConnected()) {
				uiServers++;
			}
			uiClients += User.GetClients().size();
		}

		Tmpl["AttachedUsers"] = CString(uiAttached);
		Tmpl["TotalCConnections"] = CString(uiClients);
		Tmpl["TotalIRCConnections"] = CString(uiServers);

		CZNC::TrafficStatsPair Users, ZNC, Total;
		CZNC::TrafficStatsMap traffic = CZNC::Get().GetTrafficStats(Users, ZNC, Total);
		CZNC::TrafficStatsMap::const_iterator it;

		for (it = traffic.begin(); it != traffic.end(); ++it) {
			CTemplate& l = Tmpl.AddRow("TrafficLoop");

			l["Username"] = it->first;
			l["In"] = CString::ToByteStr(it->second.first);
			l["Out"] = CString::ToByteStr(it->second.second);
			l["Total"] = CString::ToByteStr(it->second.first + it->second.second);
		}

		Tmpl["UserIn"] = CString::ToByteStr(Users.first);
		Tmpl["UserOut"] = CString::ToByteStr(Users.second);
		Tmpl["UserTotal"] = CString::ToByteStr(Users.first + Users.second);

		Tmpl["ZNCIn"] = CString::ToByteStr(ZNC.first);
		Tmpl["ZNCOut"] = CString::ToByteStr(ZNC.second);
		Tmpl["ZNCTotal"] = CString::ToByteStr(ZNC.first + ZNC.second);

		Tmpl["AllIn"] = CString::ToByteStr(Total.first);
		Tmpl["AllOut"] = CString::ToByteStr(Total.second);
		Tmpl["AllTotal"] = CString::ToByteStr(Total.first + Total.second);

		return true;
	}

	bool SettingsPage(CWebSock& WebSock, CTemplate& Tmpl) {
		if (!WebSock.GetParam("submitted").ToUInt()) {
			CString sBindHosts, sMotd;
			Tmpl["Action"] = "settings";
			Tmpl["Title"] = "Settings";
			Tmpl["StatusPrefix"] = CZNC::Get().GetStatusPrefix();
			Tmpl["MaxBufferSize"] = CString(CZNC::Get().GetMaxBufferSize());
			Tmpl["ConnectDelay"] = CString(CZNC::Get().GetConnectDelay());
			Tmpl["ServerThrottle"] = CString(CZNC::Get().GetServerThrottle());
			Tmpl["AnonIPLimit"] = CString(CZNC::Get().GetAnonIPLimit());
			Tmpl["ProtectWebSessions"] = CString(CZNC::Get().GetProtectWebSessions());

			const VCString& vsBindHosts = CZNC::Get().GetBindHosts();
			for (unsigned int a = 0; a < vsBindHosts.size(); a++) {
				CTemplate& l = Tmpl.AddRow("BindHostLoop");
				l["BindHost"] = vsBindHosts[a];
			}

			const VCString& vsMotd = CZNC::Get().GetMotd();
			for (unsigned int b = 0; b < vsMotd.size(); b++) {
				CTemplate& l = Tmpl.AddRow("MOTDLoop");
				l["Line"] = vsMotd[b];
			}

			const vector<CListener*>& vpListeners = CZNC::Get().GetListeners();
			for (unsigned int c = 0; c < vpListeners.size(); c++) {
				CListener* pListener = vpListeners[c];
				CTemplate& l = Tmpl.AddRow("ListenLoop");

				l["Port"] = CString(pListener->GetPort());
				l["BindHost"] = pListener->GetBindHost();

				l["IsWeb"] = CString(pListener->GetAcceptType() != CListener::ACCEPT_IRC);
				l["IsIRC"] = CString(pListener->GetAcceptType() != CListener::ACCEPT_HTTP);

#ifdef HAVE_LIBSSL
				if (pListener->IsSSL()) {
					l["IsSSL"] = "true";
				}
#endif

#ifdef HAVE_IPV6
				switch (pListener->GetAddrType()) {
					case ADDR_IPV4ONLY:
						l["IsIPV4"] = "true";
						break;
					case ADDR_IPV6ONLY:
						l["IsIPV6"] = "true";
						break;
					case ADDR_ALL:
						l["IsIPV4"] = "true";
						l["IsIPV6"] = "true";
						break;
				}
#else
				l["IsIPV4"] = "true";
#endif
			}

			vector<CString> vDirs;
			WebSock.GetAvailSkins(vDirs);

			for (unsigned int d = 0; d < vDirs.size(); d++) {
				const CString& SubDir = vDirs[d];
				CTemplate& l = Tmpl.AddRow("SkinLoop");
				l["Name"] = SubDir;

				if (SubDir == CZNC::Get().GetSkinName()) {
					l["Checked"] = "true";
				}
			}

			set<CModInfo> ssGlobalMods;
			CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, true);

			for (set<CModInfo>::iterator it = ssGlobalMods.begin(); it != ssGlobalMods.end(); ++it) {
				const CModInfo& Info = *it;
				CTemplate& l = Tmpl.AddRow("ModuleLoop");

				if (CZNC::Get().GetModules().FindModule(Info.GetName())) {
					l["Checked"] = "true";
				}

				if (Info.GetName() == GetModName()) {
					l["Disabled"] = "true";
				}

				l["Name"] = Info.GetName();
				l["Description"] = Info.GetDescription();
				l["Args"] = GetModArgs(NULL, Info.GetName(), true);
				l["Wiki"] = Info.GetWikiPage();
			}

			return true;
		}

		CString sArg;
		sArg = WebSock.GetParam("statusprefix"); CZNC::Get().SetStatusPrefix(sArg);
		sArg = WebSock.GetParam("maxbufsize"); CZNC::Get().SetMaxBufferSize(sArg.ToUInt());
		sArg = WebSock.GetParam("connectdelay"); CZNC::Get().SetConnectDelay(sArg.ToUInt());
		sArg = WebSock.GetParam("serverthrottle"); CZNC::Get().SetServerThrottle(sArg.ToUInt());
		sArg = WebSock.GetParam("anoniplimit"); CZNC::Get().SetAnonIPLimit(sArg.ToUInt());
		sArg = WebSock.GetParam("protectwebsessions"); CZNC::Get().SetProtectWebSessions(sArg.ToBool());

		VCString vsArgs;
		WebSock.GetRawParam("motd").Split("\n", vsArgs);
		CZNC::Get().ClearMotd();

		unsigned int a = 0;
		for (a = 0; a < vsArgs.size(); a++) {
			CZNC::Get().AddMotd(vsArgs[a].TrimRight_n());
		}

		WebSock.GetRawParam("bindhosts").Split("\n", vsArgs);
		CZNC::Get().ClearBindHosts();

		for (a = 0; a < vsArgs.size(); a++) {
			CZNC::Get().AddBindHost(vsArgs[a].Trim_n());
		}

		CZNC::Get().SetSkinName(WebSock.GetParam("skin"));

		set<CString> ssArgs;
		WebSock.GetParamValues("loadmod", ssArgs);

		for (set<CString>::iterator it = ssArgs.begin(); it != ssArgs.end(); ++it) {
			CString sModRet;
			CString sModName = (*it).TrimRight_n("\r");
			CString sModLoadError;

			if (!sModName.empty()) {
				CString sArgs = WebSock.GetParam("modargs_" + sModName);

				CModule *pMod = CZNC::Get().GetModules().FindModule(sModName);
				if (!pMod) {
					if (!CZNC::Get().GetModules().LoadModule(sModName, sArgs, NULL, sModRet)) {
						sModLoadError = "Unable to load module [" + sModName + "] [" + sModRet + "]";
					}
				} else if (pMod->GetArgs() != sArgs) {
					if (!CZNC::Get().GetModules().ReloadModule(sModName, sArgs, NULL, sModRet)) {
						sModLoadError = "Unable to reload module [" + sModName + "] [" + sModRet + "]";
					}
				}

				if (!sModLoadError.empty()) {
					DEBUG(sModLoadError);
					WebSock.GetSession()->AddError(sModLoadError);
				}
			}
		}

		const CModules& vCurMods = CZNC::Get().GetModules();
		set<CString> ssUnloadMods;

		for (a = 0; a < vCurMods.size(); a++) {
			CModule* pCurMod = vCurMods[a];

			if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() && pCurMod->GetModName() != GetModName()) {
				ssUnloadMods.insert(pCurMod->GetModName());
			}
		}

		for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); it2++) {
			CZNC::Get().GetModules().UnloadModule(*it2);
		}

		if (!CZNC::Get().WriteConfig()) {
			WebSock.GetSession()->AddError("Settings changed, but config was not written");
		}

		WebSock.Redirect("settings");
		/* we don't want the template to be printed while we redirect */
		return false;
	}
};

template<> void TModInfo<CWebAdminMod>(CModInfo& Info) {
	Info.SetWikiPage("webadmin");
}

GLOBALMODULEDEFS(CWebAdminMod, "Web based administration module")
