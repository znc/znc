/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 * Copyright (C) 2008 by Stefan Rado
 * based on admin.cpp by Sebastian Ramacher
 * based on admin.cpp in crox branch
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "znc.h"
#include "User.h"
#include "Modules.h"
#include "Chan.h"
#include "IRCSock.h"

template<std::size_t N>
struct array_size_helper {
	char __place_holder[N];
};

template<class T, std::size_t N>
static array_size_helper<N> array_size(T (&)[N]) {
	return array_size_helper<N>();
}

#define ARRAY_SIZE(array) sizeof(array_size((array)))

class CAdminMod : public CModule {
	using CModule::PutModule;

	void PrintHelp(const CString&) {
		CTable CmdTable;
		CmdTable.AddColumn("Command");
		CmdTable.AddColumn("Arguments");
		CmdTable.AddColumn("Description");
		static const char* help[][3] = {
			{"Get",          "variable [username]",           "Prints the variable's value for the given or current user"},
			{"Set",          "variable username value",       "Sets the variable's value for the given user (use $me for the current user)"},
			{"GetChan",      "variable [username] chan",      "Prints the variable's value for the given channel"},
			{"SetChan",      "variable username chan value",  "Sets the variable's value for the given channel"},
			{"ListUsers",    "",                              "Lists users"},
			{"AddUser",      "username password [ircserver]", "Adds a new user"},
			{"DelUser",      "username",                      "Deletes a user"},
			{"CloneUser",    "oldusername newusername",       "Clones a user"},
			{"AddServer",    "[username] server",             "Adds a new IRC server for the given or current user"},
			{"Reconnect",    "username",                      "Cycles the user's IRC server connection"},
			{"Disconnect",   "username",                      "Disconnects the user from their IRC server"},
			{"LoadModule",   "username modulename",           "Loads a Module for a user"},
			{"UnLoadModule", "username modulename",           "Removes a Module of a user"},
			{"ListMods",     "username",                      "Get the list of modules for a user"}
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(help); ++i) {
			CmdTable.AddRow();
			CmdTable.SetCell("Command",     help[i][0]);
			CmdTable.SetCell("Arguments",   help[i][1]);
			CmdTable.SetCell("Description", help[i][2]);
		}
		PutModule(CmdTable);

		PutModule("The following variables are available when using the Set/Get commands:");

		CTable VarTable;
		VarTable.AddColumn("Variable");
		VarTable.AddColumn("Type");
		static const char* string = "String";
		static const char* boolean = "Boolean (true/false)";
		static const char* integer = "Integer";
		static const char* doublenum = "Double";
		static const char* vars[][2] = {
			{"Nick",             string},
			{"Altnick",          string},
			{"Ident",            string},
			{"RealName",         string},
			{"BindHost",         string},
			{"MultiClients",     boolean},
			{"BounceDCCs",       boolean},
			{"UseClientIP",      boolean},
			{"DenyLoadMod",      boolean},
			{"DenySetBindHost",  boolean},
			{"DefaultChanModes", string},
			{"QuitMsg",          string},
			{"BufferCount",      integer},
			{"KeepBuffer",       boolean},
			{"Password",         string},
			{"JoinTries",        integer},
			{"MaxJoins",         integer},
			{"TimezoneOffset",   doublenum},
			{"Admin",            boolean},
			{"AppendTimestamp",  boolean},
			{"PrependTimestamp", boolean},
			{"TimestampFormat",  string},
			{"DCCBindHost",      boolean},
			{"StatusPrefix",     string}
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(vars); ++i) {
			VarTable.AddRow();
			VarTable.SetCell("Variable", vars[i][0]);
			VarTable.SetCell("Type",     vars[i][1]);
		}
		PutModule(VarTable);

		PutModule("The following variables are available when using the SetChan/GetChan commands:");
		CTable CVarTable;
		CVarTable.AddColumn("Variable");
		CVarTable.AddColumn("Type");
		static const char* cvars[][2] = {
			{"DefModes",         string},
			{"Key",              string},
			{"Buffer",           integer},
			{"InConfig",         boolean},
			{"KeepBuffer",       boolean},
			{"Detached",         boolean}
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(cvars); ++i) {
			CVarTable.AddRow();
			CVarTable.SetCell("Variable", cvars[i][0]);
			CVarTable.SetCell("Type",     cvars[i][1]);
		}
		PutModule(CVarTable);

		PutModule("You can use $me as the user name for modifying your own user.");
	}

	CUser* GetUser(const CString& sUsername) {
		if (sUsername.Equals("$me"))
			return m_pUser;
		CUser *pUser = CZNC::Get().FindUser(sUsername);
		if (!pUser) {
			PutModule("Error: User not found: " + sUsername);
			return NULL;
		}
		if (pUser != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return NULL;
		}
		return pUser;
	}

	void Get(const CString& sLine) {
		const CString sVar = sLine.Token(1).AsLower();
		CString sUsername  = sLine.Token(2, true);
		CUser* pUser;

		if (sVar.empty()) {
				PutModule("Usage: get <variable> [username]");
				return;
		}

		if (sUsername.empty()) {
			pUser = m_pUser;
		} else {
			pUser = GetUser(sUsername);
		}

		if (!pUser)
			return;

		if (sVar == "nick")
			PutModule("Nick = " + pUser->GetNick());
		else if (sVar == "altnick")
			PutModule("AltNick = " + pUser->GetAltNick());
		else if (sVar == "ident")
			PutModule("Ident = " + pUser->GetIdent());
		else if (sVar == "realname")
			PutModule("RealName = " + pUser->GetRealName());
		else if (sVar == "bindhost")
			PutModule("BindHost = " + pUser->GetBindHost());
		else if (sVar == "multiclients")
			PutModule("MultiClients = " + CString(pUser->MultiClients()));
		else if (sVar == "bouncedccs")
			PutModule("BounceDCCs = " + CString(pUser->BounceDCCs()));
		else if (sVar == "useclientip")
			PutModule("UseClientIP = " + CString(pUser->UseClientIP()));
		else if (sVar == "denyloadmod")
			PutModule("DenyLoadMod = " + CString(pUser->DenyLoadMod()));
		else if (sVar == "denysetbindhost")
			PutModule("DenySetBindHost = " + CString(pUser->DenySetBindHost()));
		else if (sVar == "defaultchanmodes")
			PutModule("DefaultChanModes = " + pUser->GetDefaultChanModes());
		else if (sVar == "quitmsg")
			PutModule("QuitMsg = " + pUser->GetQuitMsg());
		else if (sVar == "buffercount")
			PutModule("BufferCount = " + CString(pUser->GetBufferCount()));
		else if (sVar == "keepbuffer")
			PutModule("KeepBuffer = " + CString(pUser->KeepBuffer()));
		else if (sVar == "maxjoins")
			PutModule("MaxJoins = " + CString(pUser->MaxJoins()));
		else if (sVar == "jointries")
			PutModule("JoinTries = " + CString(pUser->JoinTries()));
		else if (sVar == "timezoneoffset")
			PutModule("TimezoneOffset = " + CString(pUser->GetTimezoneOffset()));
		else if (sVar == "appendtimestamp")
			PutModule("AppendTimestamp = " + CString(pUser->GetTimestampAppend()));
		else if (sVar == "prependtimestamp")
			PutModule("PrependTimestamp = " + CString(pUser->GetTimestampPrepend()));
		else if (sVar == "timestampformat")
			PutModule("TimestampFormat = " + pUser->GetTimestampFormat());
		else if (sVar == "dccbindhost")
			PutModule("DCCBindHost = " + CString(pUser->GetDCCBindHost()));
		else if (sVar == "admin")
			PutModule("Admin = " + CString(pUser->IsAdmin()));
		else if (sVar == "statusprefix")
			PutModule("StatuxPrefix = " + pUser->GetStatusPrefix());
		else
			PutModule("Error: Unknown variable");
	}

	void Set(const CString& sLine) {
		const CString sVar  = sLine.Token(1).AsLower();
		CString sUserName   = sLine.Token(2);
		CString sValue      = sLine.Token(3, true);

		if (sValue.empty()) {
			PutModule("Usage: set <variable> <username> <value>");
			return;
		}

		CUser* pUser = GetUser(sUserName);
		if (!pUser)
			return;

		if (sVar == "nick") {
			pUser->SetNick(sValue);
			PutModule("Nick = " + sValue);
		}
		else if (sVar == "altnick") {
			pUser->SetAltNick(sValue);
			PutModule("AltNick = " + sValue);
		}
		else if (sVar == "ident") {
			pUser->SetIdent(sValue);
			PutModule("Ident = " + sValue);
		}
		else if (sVar == "realname") {
			pUser->SetRealName(sValue);
			PutModule("RealName = " + sValue);
		}
		else if (sVar == "bindhost") {
			if(!pUser->DenySetBindHost() || m_pUser->IsAdmin()) {
				pUser->SetBindHost(sValue);
				PutModule("BindHost = " + sValue);
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "multiclients") {
			bool b = sValue.ToBool();
			pUser->SetMultiClients(b);
			PutModule("MultiClients = " + CString(b));
		}
		else if (sVar == "bouncedccs") {
			bool b = sValue.ToBool();
			pUser->SetBounceDCCs(b);
			PutModule("BounceDCCs = " + CString(b));
		}
		else if (sVar == "useclientip") {
			bool b = sValue.ToBool();
			pUser->SetUseClientIP(b);
			PutModule("UseClientIP = " + CString(b));
		}
		else if (sVar == "denyloadmod") {
			if(m_pUser->IsAdmin()) {
				bool b = sValue.ToBool();
				pUser->SetDenyLoadMod(b);
				PutModule("DenyLoadMod = " + CString(b));
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "denysetbindhost") {
			if(m_pUser->IsAdmin()) {
				bool b = sValue.ToBool();
				pUser->SetDenySetBindHost(b);
				PutModule("DenySetBindHost = " + CString(b));
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "defaultchanmodes") {
			pUser->SetDefaultChanModes(sValue);
			PutModule("DefaultChanModes = " + sValue);
		}
		else if (sVar == "quitmsg") {
			pUser->SetQuitMsg(sValue);
			PutModule("QuitMsg = " + sValue);
		}
		else if (sVar == "buffercount") {
			unsigned int i = sValue.ToUInt();
			// Admins don't have to honour the buffer limit
			if (pUser->SetBufferCount(i, m_pUser->IsAdmin())) {
				PutModule("BufferCount = " + sValue);
			} else {
				PutModule("Setting failed, limit is " +
						CString(CZNC::Get().GetMaxBufferSize()));
			}
		}
		else if (sVar == "keepbuffer") {
			bool b = sValue.ToBool();
			pUser->SetKeepBuffer(b);
			PutModule("KeepBuffer = " + CString(b));
		}
		else if (sVar == "password") {
			const CString sSalt = CUtils::GetSalt();
			const CString sHash = CUser::SaltedHash(sValue, sSalt);
			pUser->SetPass(sHash, CUser::HASH_DEFAULT, sSalt);
			PutModule("Password has been changed!");
		}
		else if (sVar == "maxjoins") {
			unsigned int i = sValue.ToUInt();
			pUser->SetMaxJoins(i);
			PutModule("MaxJoins = " + CString(pUser->MaxJoins()));
		}
		else if (sVar == "jointries") {
			unsigned int i = sValue.ToUInt();
			pUser->SetJoinTries(i);
			PutModule("JoinTries = " + CString(pUser->JoinTries()));
		}
		else if (sVar == "timezoneoffset") {
			double d = sValue.ToDouble();
			pUser->SetTimezoneOffset(d);
			PutModule("TimezoneOffset = " + CString(pUser->GetTimezoneOffset()));
		}
		else if (sVar == "admin") {
			if(m_pUser->IsAdmin() && pUser != m_pUser) {
				bool b = sValue.ToBool();
				pUser->SetAdmin(b);
				PutModule("Admin = " + CString(pUser->IsAdmin()));
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "prependtimestamp") {
			bool b = sValue.ToBool();
			pUser->SetTimestampPrepend(b);
			PutModule("PrependTimestamp = " + CString(b));
		}
		else if (sVar == "appendtimestamp") {
			bool b = sValue.ToBool();
			pUser->SetTimestampAppend(b);
			PutModule("AppendTimestamp = " + CString(b));
		}
		else if (sVar == "timestampformat") {
			pUser->SetTimestampFormat(sValue);
			PutModule("TimestampFormat = " + sValue);
		}
		else if (sVar == "dccbindhost") {
			if(!pUser->DenySetBindHost() || m_pUser->IsAdmin()) {
				pUser->SetDCCBindHost(sValue);
				PutModule("DCCBindHost = " + sValue);
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "statusprefix") {
			if (sVar.find_first_of(" \t\n") == CString::npos) {
				pUser->SetStatusPrefix(sValue);
				PutModule("StatusPrefix = " + sValue);
			} else {
				PutModule("That would be a bad idea!");
			}
		}
		else
			PutModule("Error: Unknown variable");
	}

	void GetChan(const CString& sLine) {
		const CString sVar  = sLine.Token(1).AsLower();
		CString sUsername   = sLine.Token(2);
		CString sChan = sLine.Token(3, true);

		if (sVar.empty()) {
			PutModule("Usage: getchan <variable> [username] <chan>");
			return;
		}
		if (sChan.empty()) {
			sChan = sUsername;
			sUsername = "";
		}
		if (sUsername.empty()) {
			sUsername = m_pUser->GetUserName();
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CChan* pChan = pUser->FindChan(sChan);
		if (!pChan) {
			PutModule("Error: Channel not found: " + sChan);
			return;
		}

		if (sVar == "defmodes")
			PutModule("DefModes = " + pChan->GetDefaultModes());
		else if (sVar == "buffer")
			PutModule("Buffer = " + CString(pChan->GetBufferCount()));
		else if (sVar == "inconfig")
			PutModule("InConfig = " + pChan->InConfig());
		else if (sVar == "keepbuffer")
			PutModule("KeepBuffer = " + pChan->KeepBuffer());
		else if (sVar == "detached")
			PutModule("Detached = " + pChan->IsDetached());
		else if (sVar == "key")
			PutModule("Key = " + pChan->GetKey());
		else
			PutModule("Error: Unknown variable");
	}

	void SetChan(const CString& sLine) {
		const CString sVar = sLine.Token(1).AsLower();
		CString sUsername  = sLine.Token(2);
		CString sChan      = sLine.Token(3);
		CString sValue     = sLine.Token(4, true);

		if (sValue.empty()) {
			PutModule("Usage: setchan <variable> <username> <chan> <value>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CChan* pChan = pUser->FindChan(sChan);
		if (!pChan) {
			PutModule("Error: Channel not found: " + sChan);
			return;
		}

		if (sVar == "defmodes") {
			pChan->SetDefaultModes(sValue);
			PutModule("DefModes = " + sValue);
		} else if (sVar == "buffer") {
			unsigned int i = sValue.ToUInt();
			// Admins don't have to honour the buffer limit
			if (pChan->SetBufferCount(i, m_pUser->IsAdmin())) {
				PutModule("Buffer = " + sValue);
			} else {
				PutModule("Setting failed, limit is " +
						CString(CZNC::Get().GetMaxBufferSize()));
			}
		} else if (sVar == "inconfig") {
			bool b = sValue.ToBool();
			pChan->SetInConfig(b);
			PutModule("InConfig = " + CString(b));
		} else if (sVar == "keepbuffer") {
			bool b = sValue.ToBool();
			pChan->SetKeepBuffer(b);
			PutModule("KeepBuffer = " + CString(b));
		} else if (sVar == "detached") {
			bool b = sValue.ToBool();
			if (pChan->IsDetached() != b) {
				if (b)
					pChan->DetachUser();
				else
					pChan->AttachUser();
			}
			PutModule("Detached = " + CString(b));
		} else if (sVar == "key") {
			pChan->SetKey(sValue);
			PutModule("Key = " + sValue);
		} else
			PutModule("Error: Unknown variable");
	}

	void ListUsers(const CString&) {
		if (!m_pUser->IsAdmin())
			return;

		const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
		CTable Table;
		Table.AddColumn("Username");
		Table.AddColumn("Realname");
		Table.AddColumn("IsAdmin");
		Table.AddColumn("Nick");
		Table.AddColumn("AltNick");
		Table.AddColumn("Ident");
		Table.AddColumn("BindHost");

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Username", it->first);
			Table.SetCell("Realname", it->second->GetRealName());
			if (!it->second->IsAdmin())
				Table.SetCell("IsAdmin", "No");
			else
				Table.SetCell("IsAdmin", "Yes");
			Table.SetCell("Nick", it->second->GetNick());
			Table.SetCell("AltNick", it->second->GetAltNick());
			Table.SetCell("Ident", it->second->GetIdent());
			Table.SetCell("BindHost", it->second->GetBindHost());
		}

		PutModule(Table);
	}

	void AddUser(const CString& sLine) {
		if (!m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to add new users!");
			return;
		}

		const CString
			sUsername  = sLine.Token(1),
			sPassword  = sLine.Token(2),
			sIRCServer = sLine.Token(3, true);
		if (sUsername.empty() || sPassword.empty()) {
			PutModule("Usage: adduser <username> <password> [ircserver]");
			return;
		}

		if (CZNC::Get().FindUser(sUsername)) {
			PutModule("Error: User " + sUsername + " already exists!");
			return;
		}

		CUser* pNewUser = new CUser(sUsername);
		CString sSalt = CUtils::GetSalt();
		pNewUser->SetPass(CUser::SaltedHash(sPassword, sSalt), CUser::HASH_DEFAULT, sSalt);
		if (sIRCServer.size())
			pNewUser->AddServer(sIRCServer);

		CString sErr;
		if (!CZNC::Get().AddUser(pNewUser, sErr)) {
			delete pNewUser;
			PutModule("Error: User not added! [" + sErr + "]");
			return;
		}

		PutModule("User " + sUsername + " added!");
		return;
	}

	void DelUser(const CString& sLine) {
		if (!m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to delete users!");
			return;
		}

		const CString sUsername  = sLine.Token(1, true);
		if (sUsername.empty()) {
			PutModule("Usage: deluser <username>");
			return;
		}

		CUser *pUser = CZNC::Get().FindUser(sUsername);

		if (!pUser) {
			PutModule("Error: User " + sUsername + " does not exist!");
			return;
		}

		if (pUser == m_pUser) {
			PutModule("Error: You can't delete yourself!");
			return;
		}

		if (!CZNC::Get().DeleteUser(pUser->GetUserName())) {
			// This can't happen, because we got the user from FindUser()
			PutModule("Error: Internal error!");
			return;
		}

		PutModule("User " + sUsername + " deleted!");
		return;
	}

	void CloneUser(const CString& sLine) {
		if (!m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to add new users!");
			return;
		}

		const CString
			sOldUsername = sLine.Token(1),
			sNewUsername = sLine.Token(2, true);

		if (sOldUsername.empty() || sNewUsername.empty()) {
			PutModule("Usage: cloneuser <oldusername> <newusername>");
			return;
		}

		CUser *pOldUser = CZNC::Get().FindUser(sOldUsername);

		if (!pOldUser) {
			PutModule("Error: User [" + sOldUsername + "] not found!");
			return;
		}

		CUser* pNewUser = new CUser(sOldUsername);
		CString sError;
		if (!pNewUser->Clone(*pOldUser, sError)) {
			delete pNewUser;
			PutModule("Error: Cloning failed! [" + sError + "]");
			return;
		}
		pNewUser->SetUserName(sNewUsername);
		pNewUser->SetIRCConnectEnabled(false);

		if (!CZNC::Get().AddUser(pNewUser, sError)) {
			delete pNewUser;
			PutModule("Error: User not added! [" + sError + "]");
			return;
		}

		PutModule("User [" + sNewUsername + "] added!");
		return;
	}

	void AddServer(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		const CString sServer = sLine.Token(2, true);

		if (sServer.empty()) {
			PutModule("Usage: addserver <username> <server>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		if (pUser->AddServer(sServer))
			PutModule("Added IRC Server: " + sServer);
		else
			PutModule("Could not add IRC server");
	}

	void ReconnectUser(const CString& sLine) {
		const CString sUsername = sLine.Token(1);

		CUser* pUser = GetUser(sUsername);
		if (!pUser) {
			PutModule("User not found.");
			return;
		}

		CIRCSock *pIRCSock = pUser->GetIRCSock();
		// cancel connection attempt:
		if (pIRCSock && !pIRCSock->IsConnected()) {
			pIRCSock->Close();
		}
		// or close existing connection:
		else if(pIRCSock) {
			pIRCSock->Quit();
		}

		// then reconnect
		pUser->SetIRCConnectEnabled(true);
		pUser->CheckIRCConnect();

		PutModule("Queued user for a reconnect.");
	}

	void DisconnectUser(const CString& sLine) {
		const CString sUsername = sLine.Token(1);

		CUser* pUser = GetUser(sUsername);
		if (!pUser) {
			PutModule("User not found.");
			return;
		}

		CIRCSock *pIRCSock = pUser->GetIRCSock();
		if (pIRCSock && !pIRCSock->IsConnected())
			pIRCSock->Close();
		else if(pIRCSock)
			pIRCSock->Quit();

		pUser->SetIRCConnectEnabled(false);

		PutModule("Closed user's IRC connection.");
	}

	void LoadModuleForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sModName  = sLine.Token(2);
		CString sArgs     = sLine.Token(3, true);
		CString sModRet;

		if (sModName.empty()) {
			PutModule("Usage: loadmodule <username> <modulename>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		if (pUser->DenyLoadMod() && !m_pUser->IsAdmin()) {
			PutModule("Loading modules has been denied");
			return;
		}

		CModule *pMod = (pUser)->GetModules().FindModule(sModName);
		if (!pMod) {
			if (!(pUser)->GetModules().LoadModule(sModName, sArgs, pUser, sModRet)) {
				PutModule("Unable to load module [" + sModName + "] [" + sModRet + "]");
			} else {
				PutModule("Loaded module [" + sModName + "]");
			}
		} else if (pMod->GetArgs() != sArgs) {
			if (!(pUser)->GetModules().ReloadModule(sModName, sArgs, pUser, sModRet)) {
				PutModule("Unable to reload module [" + sModName + "] [" + sModRet + "]");
			} else {
				PutModule("Reloaded module [" + sModName + "]");
			}
		} else {
			PutModule("Unable to load module [" + sModName + "] because it is already loaded");
		}
	}

	void UnLoadModuleForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sModName  = sLine.Token(2);
		CString sArgs     = sLine.Token(3, true);
		CString sModRet;

		if (sModName.empty()) {
			PutModule("Usage: unloadmodule <username> <modulename>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		if (pUser->DenyLoadMod() && !m_pUser->IsAdmin()) {
			PutModule("Loading modules has been denied");
			return;
		}

		if (!(pUser)->GetModules().UnloadModule(sModName, sModRet)) {
			PutModule("Unable to unload module [" + sModName + "] [" + sModRet + "]");
		} else {
			PutModule("Unloaded module [" + sModName + "] [" + sModRet + "]");
		}
	}

	void ListModuleForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1, true);

		CUser* pUser = GetUser(sUsername);
		if (!pUser || (pUser != m_pUser && !m_pUser->IsAdmin())) {
			PutModule("Usage: listmods <username of other user>");
			return;
		}

		CModules& Modules = pUser->GetModules();

		if (!Modules.size()) {
			PutModule("This user has no modules loaded.");
		} else {
			PutModule("User modules:");
			CTable Table;
			Table.AddColumn("Name");
			Table.AddColumn("Arguments");

			for (unsigned int b = 0; b < Modules.size(); b++) {
				Table.AddRow();
				Table.SetCell("Name", Modules[b]->GetModName());
				Table.SetCell("Arguments", Modules[b]->GetArgs());
			}

			PutModule(Table);
		}

	}

	typedef void (CAdminMod::* fn)(const CString&);
	typedef std::map<CString, fn> function_map;
	function_map fnmap_;

public:
	MODCONSTRUCTOR(CAdminMod) {
		fnmap_["help"]         = &CAdminMod::PrintHelp;
		fnmap_["get"]          = &CAdminMod::Get;
		fnmap_["set"]          = &CAdminMod::Set;
		fnmap_["getchan"]      = &CAdminMod::GetChan;
		fnmap_["setchan"]      = &CAdminMod::SetChan;
		fnmap_["listusers"]    = &CAdminMod::ListUsers;
		fnmap_["adduser"]      = &CAdminMod::AddUser;
		fnmap_["deluser"]      = &CAdminMod::DelUser;
		fnmap_["cloneuser"]    = &CAdminMod::CloneUser;
		fnmap_["addserver"]    = &CAdminMod::AddServer;
		fnmap_["reconnect"]    = &CAdminMod::ReconnectUser;
		fnmap_["disconnect"]   = &CAdminMod::DisconnectUser;
		fnmap_["loadmodule"]   = &CAdminMod::LoadModuleForUser;
		fnmap_["unloadmodule"] = &CAdminMod::UnLoadModuleForUser;
		fnmap_["listmods"]     = &CAdminMod::ListModuleForUser;
	}

	virtual ~CAdminMod() {}

	virtual void OnModCommand(const CString& sLine) {
		if (!m_pUser)
			return;

		const CString cmd = sLine.Token(0).AsLower();
		function_map::iterator it = fnmap_.find(cmd);
		if (it != fnmap_.end())
			(this->*it->second)(sLine);
		else
			PutModule("Unknown command");
	}
};

MODULEDEFS(CAdminMod, "Dynamic configuration of users/settings through IRC")
