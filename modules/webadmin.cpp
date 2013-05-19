/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Chan.h>
#include <znc/Server.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

using std::stringstream;
using std::make_pair;
using std::set;
using std::vector;
using std::map;

/* Stuff to be able to write this:
   // i will be name of local variable, see below
   // pUser can be NULL if only global modules are needed
   FOR_EACH_MODULE(i, pUser) {
       // i is local variable of type CModules::iterator,
	   // so *i has type CModule*
   }
*/
struct FOR_EACH_MODULE_Type {
	enum {
		AtGlobal,
		AtUser,
		AtNetwork,
	} where;
	CModules CMtemp;
	CModules& CMuser;
	CModules& CMnet;
	FOR_EACH_MODULE_Type(CUser* pUser) : CMuser(pUser ? pUser->GetModules() : CMtemp), CMnet(CMtemp) {
		where = AtGlobal;
	}
	FOR_EACH_MODULE_Type(CIRCNetwork* pNetwork) : CMuser(pNetwork ? pNetwork->GetUser()->GetModules() : CMtemp), CMnet(pNetwork ? pNetwork->GetModules() : CMtemp) {
		where = AtGlobal;
	}
	FOR_EACH_MODULE_Type(std::pair<CUser*, CIRCNetwork*> arg) : CMuser(arg.first ? arg.first->GetModules() : CMtemp), CMnet(arg.second ? arg.second->GetModules() : CMtemp) {
		where = AtGlobal;
	}
	operator bool() { return false; }
};

inline bool FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Type& state, CModules::iterator& i) {
	if (state.where == FOR_EACH_MODULE_Type::AtGlobal && i == CZNC::Get().GetModules().end()) {
		i = state.CMuser.begin();
		state.where = FOR_EACH_MODULE_Type::AtUser;
	}
	if (state.where == FOR_EACH_MODULE_Type::AtUser && i == state.CMuser.end()) {
		i = state.CMnet.begin();
		state.where = FOR_EACH_MODULE_Type::AtNetwork;
	}
	return !(state.where == FOR_EACH_MODULE_Type::AtNetwork && i == state.CMnet.end());
}

#define FOR_EACH_MODULE(I, pUserOrNetwork)\
	if (FOR_EACH_MODULE_Type FOR_EACH_MODULE_Var = pUserOrNetwork) {} else\
	for (CModules::iterator I = CZNC::Get().GetModules().begin(); FOR_EACH_MODULE_CanContinue(FOR_EACH_MODULE_Var, I); ++I)

class CWebAdminMod : public CModule {
public:
	MODCONSTRUCTOR(CWebAdminMod) {
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
		if (sArgStr.empty() || CModInfo::GlobalModule != GetType())
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
		unsigned int a = 0;

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

			const VCString& vsHosts = CZNC::Get().GetBindHosts();
			if (!spSession->IsAdmin() && !vsHosts.empty()) {
				VCString::const_iterator it;
				bool bFound = false;
				bool bFoundDCC = false;

				for (it = vsHosts.begin(); it != vsHosts.end(); ++it) {
					if (sArg.Equals(*it)) {
						bFound = true;
					}
					if (sArg2.Equals(*it)) {
						bFoundDCC = true;
					}
				}

				if (!bFound) {
					pNewUser->SetBindHost(pUser ? pUser->GetBindHost() : "");
				}
				if (!bFoundDCC) {
					pNewUser->SetDCCBindHost(pUser ? pUser->GetDCCBindHost() : "");
				}
			}
		} else if (pUser){
			pNewUser->SetBindHost(pUser->GetBindHost());
			pNewUser->SetDCCBindHost(pUser->GetDCCBindHost());
		}

		sArg = WebSock.GetParam("bufsize"); if (!sArg.empty()) pNewUser->SetBufferCount(sArg.ToUInt(), spSession->IsAdmin());
		if (!sArg.empty()) {
			// First apply the old limit in case the new one is too high
			if (pUser)
				pNewUser->SetBufferCount(pUser->GetBufferCount(), true);
			pNewUser->SetBufferCount(sArg.ToUInt(), spSession->IsAdmin());
		}

		pNewUser->SetSkinName(WebSock.GetParam("skin"));
		pNewUser->SetAutoClearChanBuffer(WebSock.GetParam("autoclearchanbuffer").ToBool());
		pNewUser->SetMultiClients(WebSock.GetParam("multiclients").ToBool());
		pNewUser->SetTimestampAppend(WebSock.GetParam("appendtimestamp").ToBool());
		pNewUser->SetTimestampPrepend(WebSock.GetParam("prependtimestamp").ToBool());
		pNewUser->SetTimezone(WebSock.GetParam("timezone"));
		pNewUser->SetJoinTries(WebSock.GetParam("jointries").ToUInt());

		if (spSession->IsAdmin()) {
			pNewUser->SetDenyLoadMod(WebSock.GetParam("denyloadmod").ToBool());
			pNewUser->SetDenySetBindHost(WebSock.GetParam("denysetbindhost").ToBool());
			sArg = WebSock.GetParam("maxnetworks"); if (!sArg.empty()) pNewUser->SetMaxNetworks(sArg.ToUInt());
		} else if (pUser) {
			pNewUser->SetDenyLoadMod(pUser->DenyLoadMod());
			pNewUser->SetDenySetBindHost(pUser->DenySetBindHost());
			pNewUser->SetMaxNetworks(pUser->MaxNetworks());
		}

		// If pUser is not NULL, we are editing an existing user.
		// Users must not be able to change their own admin flag.
		if (pUser != CZNC::Get().FindUser(WebSock.GetUser())) {
			pNewUser->SetAdmin(WebSock.GetParam("isadmin").ToBool());
		} else if (pUser) {
			pNewUser->SetAdmin(pUser->IsAdmin());
		}

		if (spSession->IsAdmin() || (pUser && !pUser->DenyLoadMod())) {
			WebSock.GetParamValues("loadmod", vsArgs);

			// disallow unload webadmin from itself
			if (CModInfo::UserModule == GetType() && pUser == CZNC::Get().FindUser(WebSock.GetUser())) {
				bool bLoadedWebadmin = false;
				for (a = 0; a < vsArgs.size(); ++a) {
					CString sModName = vsArgs[a].TrimRight_n("\r");
					if (sModName == GetModName()) {
						bLoadedWebadmin = true;
						break;
					}
				}
				if (!bLoadedWebadmin) {
					vsArgs.push_back(GetModName());
				}
			}

			for (a = 0; a < vsArgs.size(); a++) {
				CString sModRet;
				CString sModName = vsArgs[a].TrimRight_n("\r");
				CString sModLoadError;

				if (!sModName.empty()) {
					CString sArgs = WebSock.GetParam("modargs_" + sModName);

					try {
						if (!pNewUser->GetModules().LoadModule(sModName, sArgs, CModInfo::UserModule, pNewUser, NULL, sModRet)) {
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
					if (!pNewUser->GetModules().LoadModule(sModName, sArgs, CModInfo::UserModule, pNewUser, NULL, sModRet)) {
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

	CString SafeGetNetworkParam(CWebSock& WebSock) {
		CString sNetwork = WebSock.GetParam("network"); // check for POST param
		if(sNetwork.empty() && !WebSock.IsPost()) {
			// if no POST param named user has been given and we are not
			// saving this form, fall back to using the GET parameter.
			sNetwork = WebSock.GetParam("network", false);
		}
		return sNetwork;
	}

	CUser* SafeGetUserFromParam(CWebSock& WebSock) {
		return CZNC::Get().FindUser(SafeGetUserNameParam(WebSock));
	}

	CIRCNetwork* SafeGetNetworkFromParam(CWebSock& WebSock) {
		CUser* pUser = CZNC::Get().FindUser(SafeGetUserNameParam(WebSock));
		CIRCNetwork* pNetwork = NULL;

		if (pUser) {
			pNetwork = pUser->FindNetwork(SafeGetNetworkParam(WebSock));
		}

		return pNetwork;
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
		} else if (sPageName == "addnetwork") {
			CUser* pUser = SafeGetUserFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			if (pUser) {
				return NetworkPage(WebSock, Tmpl, pUser);
			}

			WebSock.PrintErrorPage("No such username");
			return true;
		} else if (sPageName == "editnetwork") {
			CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pNetwork->GetUser())) {
				return false;
			}

			if (!pNetwork) {
				WebSock.PrintErrorPage("No such username or network");
				return true;
			}

			return NetworkPage(WebSock, Tmpl, pNetwork->GetUser(), pNetwork);

		} else if (sPageName == "delnetwork") {
			CString sUser = WebSock.GetParam("user");
			if (sUser.empty() && !WebSock.IsPost()) {
				sUser = WebSock.GetParam("user", false);
			}

			CUser* pUser = CZNC::Get().FindUser(sUser);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pUser)) {
				return false;
			}

			return DelNetwork(WebSock, pUser, Tmpl);
		} else if (sPageName == "editchan") {
			CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pNetwork->GetUser())) {
				return false;
			}

			if (!pNetwork) {
				WebSock.PrintErrorPage("No such username or network");
				return true;
			}

			CString sChan = WebSock.GetParam("name");
			if(sChan.empty() && !WebSock.IsPost()) {
				sChan = WebSock.GetParam("name", false);
			}
			CChan* pChan = pNetwork->FindChan(sChan);
			if (!pChan) {
				WebSock.PrintErrorPage("No such channel");
				return true;
			}

			return ChanPage(WebSock, Tmpl, pNetwork, pChan);
		} else if (sPageName == "addchan") {
			CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pNetwork->GetUser())) {
				return false;
			}

			if (pNetwork) {
				return ChanPage(WebSock, Tmpl, pNetwork);
			}

			WebSock.PrintErrorPage("No such username or network");
			return true;
		} else if (sPageName == "delchan") {
			CIRCNetwork* pNetwork = SafeGetNetworkFromParam(WebSock);

			// Admin||Self Check
			if (!spSession->IsAdmin() && (!spSession->GetUser() || spSession->GetUser() != pNetwork->GetUser())) {
				return false;
			}

			if (pNetwork) {
				return DelChan(WebSock, pNetwork);
			}

			WebSock.PrintErrorPage("No such username or network");
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
		} else if (sPageName == "add_listener") {
			// Admin Check
			if (!spSession->IsAdmin()) {
				return false;
			}

			return AddListener(WebSock, Tmpl);
		} else if (sPageName == "del_listener") {
			// Admin Check
			if (!spSession->IsAdmin()) {
				return false;
			}

			return DelListener(WebSock, Tmpl);
		}

		return false;
	}

	bool ChanPage(CWebSock& WebSock, CTemplate& Tmpl, CIRCNetwork* pNetwork, CChan* pChan = NULL) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl.SetFile("add_edit_chan.tmpl");
		CUser* pUser = pNetwork->GetUser();

		if (!pUser) {
			WebSock.PrintErrorPage("That user doesn't exist");
			return true;
		}

		if (!WebSock.GetParam("submitted").ToUInt()) {
			Tmpl["User"] = pUser->GetUserName();
			Tmpl["Network"] = pNetwork->GetName();

			if (pChan) {
				Tmpl["Action"] = "editchan";
				Tmpl["Edit"] = "true";
				Tmpl["Title"] = "Edit Channel" + CString(" [" + pChan->GetName() + "]") + " of Network [" + pNetwork->GetName() + "] of User [" + pNetwork->GetUser()->GetUserName() + "]";
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
			o2["Name"] = "autoclearchanbuffer";
			o2["DisplayName"] = "Auto Clear Chan Buffer";
			o2["Tooltip"] = "Automatically Clear Channel Buffer After Playback";
			if ((pChan && pChan->AutoClearChanBuffer()) || (!pChan && pUser->AutoClearChanBuffer())) { o2["Checked"] = "true"; }

			CTemplate& o3 = Tmpl.AddRow("OptionLoop");
			o3["Name"] = "detached";
			o3["DisplayName"] = "Detached";
			if (pChan && pChan->IsDetached()) { o3["Checked"] = "true"; }

			CTemplate& o4 = Tmpl.AddRow("OptionLoop");
			o4["Name"] = "disabled";
			o4["DisplayName"] = "Disabled";
			if (pChan && pChan->IsDisabled()) { o4["Checked"] = "true"; }

			FOR_EACH_MODULE(i, pNetwork) {
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

			if (pNetwork->FindChan(sChanName.Token(0))) {
				WebSock.PrintErrorPage("Channel [" + sChanName.Token(0) + "] already exists");
				return true;
			}

			pChan = new CChan(sChanName, pNetwork, true);
			pNetwork->AddChan(pChan);
		}

		pChan->SetBufferCount(WebSock.GetParam("buffercount").ToUInt(), spSession->IsAdmin());
		pChan->SetDefaultModes(WebSock.GetParam("defmodes"));
		pChan->SetInConfig(WebSock.GetParam("save").ToBool());
		pChan->SetAutoClearChanBuffer(WebSock.GetParam("autoclearchanbuffer").ToBool());
		pChan->SetKey(WebSock.GetParam("key"));

		bool bDetached = WebSock.GetParam("detached").ToBool();
		if (pChan->IsDetached() != bDetached) {
			if (bDetached) {
				pChan->DetachUser();
			} else {
				pChan->AttachUser();
			}
		}

		bool bDisabled = WebSock.GetParam("disabled").ToBool();
		if (bDisabled)
			pChan->Disable();
		else
			pChan->Enable();

		CTemplate TmplMod;
		TmplMod["User"] = pUser->GetUserName();
		TmplMod["ChanName"] = sChanName;
		TmplMod["WebadminAction"] = "change";
		FOR_EACH_MODULE(it, pNetwork) {
			(*it)->OnEmbeddedWebRequest(WebSock, "webadmin/channel", TmplMod);
		}

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Channel added/modified, but config was not written");
			return true;
		}

		WebSock.Redirect("editnetwork?user=" + pUser->GetUserName().Escape_n(CString::EURL) + "&network=" + pNetwork->GetName().Escape_n(CString::EURL));
		return true;
	}

	bool NetworkPage(CWebSock& WebSock, CTemplate& Tmpl, CUser* pUser, CIRCNetwork* pNetwork = NULL) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl.SetFile("add_edit_network.tmpl");

		if (!WebSock.GetParam("submitted").ToUInt()) {
			Tmpl["Username"] = pUser->GetUserName();

			set<CModInfo> ssNetworkMods;
			CZNC::Get().GetModules().GetAvailableMods(ssNetworkMods, CModInfo::NetworkModule);
			for (set<CModInfo>::iterator it = ssNetworkMods.begin(); it != ssNetworkMods.end(); ++it) {
				const CModInfo& Info = *it;
				CTemplate& l = Tmpl.AddRow("ModuleLoop");

				l["Name"] = Info.GetName();
				l["Description"] = Info.GetDescription();
				l["Wiki"] = Info.GetWikiPage();
				l["HasArgs"] = CString(Info.GetHasArgs());
				l["ArgsHelpText"] = Info.GetArgsHelpText();

				if (pNetwork) {
					CModule *pModule = pNetwork->GetModules().FindModule(Info.GetName());
					if (pModule) {
						l["Checked"] = "true";
						l["Args"] = pModule->GetArgs();
					}
				}

				if (!spSession->IsAdmin() && pUser->DenyLoadMod()) {
					l["Disabled"] = "true";
				}
			}

			// To change BindHosts be admin or don't have DenySetBindHost
			if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
				Tmpl["BindHostEdit"] = "true";
				const VCString& vsBindHosts = CZNC::Get().GetBindHosts();
				if (vsBindHosts.empty()) {
					if (pNetwork) {
						Tmpl["BindHost"] = pNetwork->GetBindHost();
					}
				} else {
					bool bFoundBindHost = false;
					for (unsigned int b = 0; b < vsBindHosts.size(); b++) {
						const CString& sBindHost = vsBindHosts[b];
						CTemplate& l = Tmpl.AddRow("BindHostLoop");

						l["BindHost"] = sBindHost;

						if (pNetwork && pNetwork->GetBindHost() == sBindHost) {
							l["Checked"] = "true";
							bFoundBindHost = true;
						}
					}

					// If our current bindhost is not in the global list...
					if (pNetwork && !bFoundBindHost && !pNetwork->GetBindHost().empty()) {
						CTemplate& l = Tmpl.AddRow("BindHostLoop");

						l["BindHost"] = pNetwork->GetBindHost();
						l["Checked"] = "true";
					}
				}
			}

			if (pNetwork) {
				Tmpl["Action"] = "editnetwork";
				Tmpl["Edit"] = "true";
				Tmpl["Title"] = "Edit Network" + CString(" [" + pNetwork->GetName() + "]") + " of User [" + pUser->GetUserName() + "]";
				Tmpl["Name"] = pNetwork->GetName();

				Tmpl["Nick"] = pNetwork->GetNick();
				Tmpl["AltNick"] = pNetwork->GetAltNick();
				Tmpl["Ident"] = pNetwork->GetIdent();
				Tmpl["RealName"] = pNetwork->GetRealName();

				Tmpl["FloodProtection"] = CString(CIRCSock::IsFloodProtected(pNetwork->GetFloodRate()));
				Tmpl["FloodRate"] = CString(pNetwork->GetFloodRate());
				Tmpl["FloodBurst"] = CString(pNetwork->GetFloodBurst());

				Tmpl["IRCConnectEnabled"] = CString(pNetwork->GetIRCConnectEnabled());

				const vector<CServer*>& vServers = pNetwork->GetServers();
				for (unsigned int a = 0; a < vServers.size(); a++) {
					CTemplate& l = Tmpl.AddRow("ServerLoop");
					l["Server"] = vServers[a]->GetString();
				}

				const vector<CChan*>& Channels = pNetwork->GetChans();
				for (unsigned int c = 0; c < Channels.size(); c++) {
					CChan* pChan = Channels[c];
					CTemplate& l = Tmpl.AddRow("ChannelLoop");

					l["Network"] = pNetwork->GetName();
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
				if (!spSession->IsAdmin() && !pUser->HasSpaceForNewNetwork()) {
					WebSock.PrintErrorPage("Network number limit reached. Ask an admin to increase the limit for you, or delete few old ones from Your Settings");
					return true;
				}

				Tmpl["Action"] = "addnetwork";
				Tmpl["Title"] = "Add Network for User [" + pUser->GetUserName() + "]";
				Tmpl["IRCConnectEnabled"] = "true";
				Tmpl["FloodProtection"] = "true";
				Tmpl["FloodRate"] = "1.0";
				Tmpl["FloodBurst"] = "4";
			}

			FOR_EACH_MODULE(i, make_pair(pUser, pNetwork)) {
				CTemplate& mod = Tmpl.AddRow("EmbeddedModuleLoop");
				mod.insert(Tmpl.begin(), Tmpl.end());
				mod["WebadminAction"] = "display";
				if ((*i)->OnEmbeddedWebRequest(WebSock, "webadmin/network", mod)) {
					mod["Embed"] = WebSock.FindTmpl(*i, "WebadminNetwork.tmpl");
					mod["ModName"] = (*i)->GetModName();
				}
			}

			return true;
		}

		CString sName = WebSock.GetParam("network").Trim_n();
		if (sName.empty()) {
			WebSock.PrintErrorPage("Network name is a required argument");
			return true;
		}

		if (!pNetwork) {
			if (!spSession->IsAdmin() && !pUser->HasSpaceForNewNetwork()) {
				WebSock.PrintErrorPage("Network number limit reached. Ask an admin to increase the limit for you, or delete few old ones from Your Settings");
				return true;
			}
			if (!CIRCNetwork::IsValidNetwork(sName)) {
				WebSock.PrintErrorPage("Network name should be alphanumeric");
				return true;
			}
			pNetwork = pUser->AddNetwork(sName);
			if (!pNetwork) {
				WebSock.PrintErrorPage("Network [" + sName.Token(0) + "] already exists");
				return true;
			}
		}

		CString sArg;

		pNetwork->SetNick(WebSock.GetParam("nick"));
		pNetwork->SetAltNick(WebSock.GetParam("altnick"));
		pNetwork->SetIdent(WebSock.GetParam("ident"));
		pNetwork->SetRealName(WebSock.GetParam("realname"));

		pNetwork->SetIRCConnectEnabled(WebSock.GetParam("doconnect").ToBool());

		sArg = WebSock.GetParam("bindhost");
		// To change BindHosts be admin or don't have DenySetBindHost
		if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
			CString sHost = WebSock.GetParam("bindhost");
			const VCString& vsHosts = CZNC::Get().GetBindHosts();
			if (!spSession->IsAdmin() && !vsHosts.empty()) {
				VCString::const_iterator it;
				bool bFound = false;

				for (it = vsHosts.begin(); it != vsHosts.end(); ++it) {
					if (sHost.Equals(*it)) {
						bFound = true;
						break;
					}
				}

				if (!bFound) {
					sHost = pNetwork->GetBindHost();
				}
			}
			pNetwork->SetBindHost(sHost);
		}

		if (WebSock.GetParam("floodprotection").ToBool()) {
			pNetwork->SetFloodRate(WebSock.GetParam("floodrate").ToDouble());
			pNetwork->SetFloodBurst(WebSock.GetParam("floodburst").ToUShort());
		} else {
			pNetwork->SetFloodRate(-1);
		}

		VCString vsArgs;

		pNetwork->DelServers();
		WebSock.GetRawParam("servers").Split("\n", vsArgs);
		for (unsigned int a = 0; a < vsArgs.size(); a++) {
			pNetwork->AddServer(vsArgs[a].Trim_n());
		}

		WebSock.GetParamValues("channel", vsArgs);
		for (unsigned int a = 0; a < vsArgs.size(); a++) {
			const CString& sChan = vsArgs[a];
			CChan *pChan = pNetwork->FindChan(sChan.TrimRight_n("\r"));
			if (pChan) {
				pChan->SetInConfig(WebSock.GetParam("save_" + sChan).ToBool());
			}
		}

		set<CString> ssArgs;
		WebSock.GetParamValues("loadmod", ssArgs);
		if (spSession->IsAdmin() || !pUser->DenyLoadMod()) {
			for (set<CString>::iterator it = ssArgs.begin(); it != ssArgs.end(); ++it) {
				CString sModRet;
				CString sModName = (*it).TrimRight_n("\r");
				CString sModLoadError;

				if (!sModName.empty()) {
					CString sArgs = WebSock.GetParam("modargs_" + sModName);

					CModule *pMod = pNetwork->GetModules().FindModule(sModName);

					if (!pMod) {
						if (!pNetwork->GetModules().LoadModule(sModName, sArgs, CModInfo::NetworkModule, pUser, pNetwork, sModRet)) {
							sModLoadError = "Unable to load module [" + sModName + "] [" + sModRet + "]";
						}
					} else if (pMod->GetArgs() != sArgs) {
						if (!pNetwork->GetModules().ReloadModule(sModName, sArgs, pUser, pNetwork, sModRet)) {
							sModLoadError = "Unable to reload module [" + sModName + "] [" + sModRet + "]";
						}
					}

					if (!sModLoadError.empty()) {
						DEBUG(sModLoadError);
						WebSock.GetSession()->AddError(sModLoadError);
					}
				}
			}
		}

		const CModules& vCurMods = pNetwork->GetModules();
		set<CString> ssUnloadMods;

		for (unsigned int a = 0; a < vCurMods.size(); a++) {
			CModule* pCurMod = vCurMods[a];

			if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() && pCurMod->GetModName() != GetModName()) {
				ssUnloadMods.insert(pCurMod->GetModName());
			}
		}

		for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); ++it2) {
			pNetwork->GetModules().UnloadModule(*it2);
		}

		CTemplate TmplMod;
		TmplMod["Username"] = pUser->GetUserName();
		TmplMod["Name"] = pNetwork->GetName();
		TmplMod["WebadminAction"] = "change";
		FOR_EACH_MODULE(it, make_pair(pUser, pNetwork)) {
			(*it)->OnEmbeddedWebRequest(WebSock, "webadmin/network", TmplMod);
		}

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Network added/modified, but config was not written");
			return true;
		}

		WebSock.Redirect("edituser?user=" + pUser->GetUserName().Escape_n(CString::EURL));
		return true;
	}

	bool DelNetwork(CWebSock& WebSock, CUser* pUser, CTemplate& Tmpl) {
		CString sNetwork = WebSock.GetParam("name");
		if (sNetwork.empty() && !WebSock.IsPost()) {
			sNetwork = WebSock.GetParam("name", false);
		}

		if (!pUser) {
			WebSock.PrintErrorPage("That user doesn't exist");
			return true;
		}

		if (sNetwork.empty()) {
			WebSock.PrintErrorPage("That network doesn't exist for this user");
			return true;
		}

		if (!WebSock.IsPost()) {
			// Show the "Are you sure?" page:

			Tmpl.SetFile("del_network.tmpl");
			Tmpl["Username"] = pUser->GetUserName();
			Tmpl["Network"] = sNetwork;
			return true;
		}

		pUser->DeleteNetwork(sNetwork);

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Network deleted, but config was not written");
			return true;
		}

		WebSock.Redirect("edituser?user=" + pUser->GetUserName().Escape_n(CString::EURL));
		return false;
	}

	bool DelChan(CWebSock& WebSock, CIRCNetwork* pNetwork) {
		CString sChan = WebSock.GetParam("name", false);

		if (sChan.empty()) {
			WebSock.PrintErrorPage("That channel doesn't exist for this user");
			return true;
		}

		pNetwork->DelChan(sChan);
		pNetwork->PutIRC("PART " + sChan);

		if (!CZNC::Get().WriteConfig()) {
			WebSock.PrintErrorPage("Channel deleted, but config was not written");
			return true;
		}

		WebSock.Redirect("editnetwork?user=" + pNetwork->GetUser()->GetUserName().Escape_n(CString::EURL) + "&network=" + pNetwork->GetName().Escape_n(CString::EURL));
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
			} else {
				CString sUsername = WebSock.GetParam("clone", false);
				pUser = CZNC::Get().FindUser(sUsername);

				if (pUser) {
					Tmpl["Title"] = "Clone User [" + pUser->GetUserName() + "]";
					Tmpl["Clone"] = "true";
					Tmpl["CloneUsername"] = pUser->GetUserName();
				}
			}

			Tmpl["ImAdmin"] = CString(spSession->IsAdmin());

			if (pUser) {
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
				Tmpl["Timezone"] = pUser->GetTimezone();
				Tmpl["JoinTries"] = CString(pUser->JoinTries());
				Tmpl["MaxNetworks"] = CString(pUser->MaxNetworks());

				const set<CString>& ssAllowedHosts = pUser->GetAllowedHosts();
				for (set<CString>::const_iterator it = ssAllowedHosts.begin(); it != ssAllowedHosts.end(); ++it) {
					CTemplate& l = Tmpl.AddRow("AllowedHostLoop");
					l["Host"] = *it;
				}

				const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();
				for (unsigned int a = 0; a < vNetworks.size(); a++) {
					CTemplate& l = Tmpl.AddRow("NetworkLoop");
					l["Name"] = vNetworks[a]->GetName();
					l["Username"] = pUser->GetUserName();
					l["Clients"] = CString(vNetworks[a]->GetClients().size());
					l["IRCNick"] = vNetworks[a]->GetIRCNick().GetNick();
					CServer* pServer = vNetworks[a]->GetCurrentServer();
					if (pServer) {
						l["Server"] = pServer->GetName();
					}
				}

				const MCString& msCTCPReplies = pUser->GetCTCPReplies();
				for (MCString::const_iterator it2 = msCTCPReplies.begin(); it2 != msCTCPReplies.end(); ++it2) {
					CTemplate& l = Tmpl.AddRow("CTCPLoop");
					l["CTCP"] = it2->first + " " + it2->second;
				}
			} else {
				Tmpl["Action"] = "adduser";
				Tmpl["Title"] = "Add User";
				Tmpl["StatusPrefix"] = "*";
			}

			SCString ssTimezones = CUtils::GetTimezones();
			for (SCString::iterator i = ssTimezones.begin(); i != ssTimezones.end(); ++i) {
				CTemplate& l = Tmpl.AddRow("TZLoop");
				l["TZ"] = *i;
			}

			// To change BindHosts be admin or don't have DenySetBindHost
			if (spSession->IsAdmin() || !spSession->GetUser()->DenySetBindHost()) {
				Tmpl["BindHostEdit"] = "true";
				const VCString& vsBindHosts = CZNC::Get().GetBindHosts();
				if (vsBindHosts.empty()) {
					if (pUser) {
						Tmpl["BindHost"] = pUser->GetBindHost();
						Tmpl["DCCBindHost"] = pUser->GetDCCBindHost();
					}
				} else {
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
				l["Wiki"] = Info.GetWikiPage();
				l["HasArgs"] = CString(Info.GetHasArgs());
				l["ArgsHelpText"] = Info.GetArgsHelpText();

				CModule *pModule = NULL;
				if (pUser)
					pModule = pUser->GetModules().FindModule(Info.GetName());
				if (pModule) {
					l["Checked"] = "true";
					l["Args"] = pModule->GetArgs();
					if (CModInfo::UserModule == GetType() && Info.GetName() == GetModName()) {
						l["Disabled"] = "true";
					}
				}

				if (!spSession->IsAdmin() && pUser && pUser->DenyLoadMod()) {
					l["Disabled"] = "true";
				}
			}

			CTemplate& o1 = Tmpl.AddRow("OptionLoop");
			o1["Name"] = "autoclearchanbuffer";
			o1["DisplayName"] = "Auto Clear Chan Buffer";
			o1["Tooltip"] = "Automatically Clear Channel Buffer After Playback (the default value for new channels)";
			if (!pUser || pUser->AutoClearChanBuffer()) { o1["Checked"] = "true"; }

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
			WebSock.PrintErrorPage("Invalid user settings");
			return true;
		}

		CString sErr;
		CString sAction;

		if (!pUser) {
			CString sClone = WebSock.GetParam("clone");
			if (CUser *pCloneUser = CZNC::Get().FindUser(sClone)) {
				pNewUser->CloneNetworks(*pCloneUser);
			}

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
			CTemplate& l = Tmpl.AddRow("UserLoop");
			CUser& User = *it->second;

			l["Username"] = User.GetUserName();
			l["Clients"] = CString(User.GetAllClients().size());
			l["Networks"] = CString(User.GetNetworks().size());

			if (&User == spSession->GetUser()) {
				l["IsSelf"] = "true";
			}
		}

		return true;
	}

	bool TrafficPage(CWebSock& WebSock, CTemplate& Tmpl) {
		CSmartPtr<CWebSession> spSession = WebSock.GetSession();
		Tmpl["Uptime"] = CZNC::Get().GetUptime();

		const map<CString,CUser*>& msUsers = CZNC::Get().GetUserMap();
		Tmpl["TotalUsers"] = CString(msUsers.size());

		size_t uiNetworks = 0, uiAttached = 0, uiClients = 0, uiServers = 0;

		for (map<CString,CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			CUser& User = *it->second;
			vector<CIRCNetwork*> vNetworks = User.GetNetworks();

			for (vector<CIRCNetwork*>::const_iterator it2 = vNetworks.begin(); it2 != vNetworks.end(); ++it2) {
				CIRCNetwork *pNetwork = *it2;
				uiNetworks++;

				if (pNetwork->IsIRCConnected()) {
					uiServers++;
				}

				if (pNetwork->IsNetworkAttached()) {
					uiAttached++;
				}

				uiClients += pNetwork->GetClients().size();
			}

			uiClients += User.GetUserClients().size();
		}

		Tmpl["TotalNetworks"] = CString(uiNetworks);
		Tmpl["AttachedNetworks"] = CString(uiAttached);
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

	bool AddListener(CWebSock& WebSock, CTemplate& Tmpl) {
		unsigned short uPort = WebSock.GetParam("port").ToUShort();
		CString sHost = WebSock.GetParam("host");
		if (sHost == "*") sHost = "";
		bool bSSL = WebSock.GetParam("ssl").ToBool();
		bool bIPv4 = WebSock.GetParam("ipv4").ToBool();
		bool bIPv6 = WebSock.GetParam("ipv6").ToBool();
		bool bIRC = WebSock.GetParam("irc").ToBool();
		bool bWeb = WebSock.GetParam("web").ToBool();

		EAddrType eAddr = ADDR_ALL;
		if (bIPv4) {
			if (bIPv6) {
				eAddr = ADDR_ALL;
			} else {
				eAddr = ADDR_IPV4ONLY;
			}
		} else {
			if (bIPv6) {
				eAddr = ADDR_IPV6ONLY;
			} else {
				WebSock.GetSession()->AddError("Choose either IPv4 or IPv6 or both.");
				return SettingsPage(WebSock, Tmpl);
			}
		}

		CListener::EAcceptType eAccept;
		if (bIRC) {
			if (bWeb) {
				eAccept = CListener::ACCEPT_ALL;
			} else {
				eAccept = CListener::ACCEPT_IRC;
			}
		} else {
			if (bWeb) {
				eAccept = CListener::ACCEPT_HTTP;
			} else {
				WebSock.GetSession()->AddError("Choose either IRC or Web or both.");
				return SettingsPage(WebSock, Tmpl);
			}
		}

		CString sMessage;
		if (CZNC::Get().AddListener(uPort, sHost, bSSL, eAddr, eAccept, sMessage)) {
			if (!sMessage.empty()) {
				WebSock.GetSession()->AddSuccess(sMessage);
			}
			if (!CZNC::Get().WriteConfig()) {
				WebSock.GetSession()->AddError("Port changed, but config was not written");
			}
		} else {
			WebSock.GetSession()->AddError(sMessage);
		}

		return SettingsPage(WebSock, Tmpl);
	}

	bool DelListener(CWebSock& WebSock, CTemplate& Tmpl) {
		unsigned short uPort = WebSock.GetParam("port").ToUShort();
		CString sHost = WebSock.GetParam("host");
		bool bIPv4 = WebSock.GetParam("ipv4").ToBool();
		bool bIPv6 = WebSock.GetParam("ipv6").ToBool();

		EAddrType eAddr = ADDR_ALL;
		if (bIPv4) {
			if (bIPv6) {
				eAddr = ADDR_ALL;
			} else {
				eAddr = ADDR_IPV4ONLY;
			}
		} else {
			if (bIPv6) {
				eAddr = ADDR_IPV6ONLY;
			} else {
				WebSock.GetSession()->AddError("Invalid request.");
				return SettingsPage(WebSock, Tmpl);
			}
		}

		CListener* pListener = CZNC::Get().FindListener(uPort, sHost, eAddr);
		if (pListener) {
			CZNC::Get().DelListener(pListener);
			if (!CZNC::Get().WriteConfig()) {
				WebSock.GetSession()->AddError("Port changed, but config was not written");
			}
		} else {
			WebSock.GetSession()->AddError("The specified listener was not found.");
		}

		return SettingsPage(WebSock, Tmpl);
	}

	bool SettingsPage(CWebSock& WebSock, CTemplate& Tmpl) {
		Tmpl.SetFile("settings.tmpl");
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

				// simple protection for user from shooting his own foot
				// TODO check also for hosts/families
				// such check is only here, user still can forge HTTP request to delete web port
				l["SuggestDeletion"] = CString(pListener->GetPort() != WebSock.GetLocalPort());

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
			CZNC::Get().GetModules().GetAvailableMods(ssGlobalMods, CModInfo::GlobalModule);

			for (set<CModInfo>::iterator it = ssGlobalMods.begin(); it != ssGlobalMods.end(); ++it) {
				const CModInfo& Info = *it;
				CTemplate& l = Tmpl.AddRow("ModuleLoop");

				CModule *pModule = CZNC::Get().GetModules().FindModule(Info.GetName());
				if (pModule) {
					l["Checked"] = "true";
					l["Args"] = pModule->GetArgs();
					if (CModInfo::GlobalModule == GetType() && Info.GetName() == GetModName()) {
						l["Disabled"] = "true";
					}
				}

				l["Name"] = Info.GetName();
				l["Description"] = Info.GetDescription();
				l["Wiki"] = Info.GetWikiPage();
				l["HasArgs"] = CString(Info.GetHasArgs());
				l["ArgsHelpText"] = Info.GetArgsHelpText();
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
					if (!CZNC::Get().GetModules().LoadModule(sModName, sArgs, CModInfo::GlobalModule, NULL, NULL, sModRet)) {
						sModLoadError = "Unable to load module [" + sModName + "] [" + sModRet + "]";
					}
				} else if (pMod->GetArgs() != sArgs) {
					if (!CZNC::Get().GetModules().ReloadModule(sModName, sArgs, NULL, NULL, sModRet)) {
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

			if (ssArgs.find(pCurMod->GetModName()) == ssArgs.end() &&
					(CModInfo::GlobalModule != GetType() || pCurMod->GetModName() != GetModName())) {
				ssUnloadMods.insert(pCurMod->GetModName());
			}
		}

		for (set<CString>::iterator it2 = ssUnloadMods.begin(); it2 != ssUnloadMods.end(); ++it2) {
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
	Info.AddType(CModInfo::UserModule);
	Info.SetWikiPage("webadmin");
}

GLOBALMODULEDEFS(CWebAdminMod, "Web based administration module")
