/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
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
			{"Get",       "variable [username]",           "Prints the variable's value for the given or current user"},
			{"Set",       "variable [username] value",     "Sets the variable's value for the given or current user"},
			{"GetChan",   "variable [username] chan",      "Prints the variable's value for the given channel"},
			{"SetChan",   "variable [username] chan value", "Sets the variable's value for the given channel"},
			{"ListUsers", "",                              "Lists users"},
			{"AddUser",   "username password [ircserver]", "Adds a new user"},
			{"DelUser",   "username",                      "Deletes a user"},
			{"CloneUser", "oldusername newusername",       "Clones a user"},
			{"AddServer", "[username] server",             "Adds a new IRC server for the given or current user"}
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
		const char* string = "String";
		const char* boolean = "Boolean (true/false)";
		const char* integer = "Integer";
		static const char* vars[][2] = {
			{"Ident",            string},
			{"RealName",         string},
			{"VHost",            string},
			{"MultiClients",     boolean},
			{"BounceDCCs",       boolean},
			{"UseClientIP",      boolean},
			{"DenyLoadMod",      boolean},
			{"DefaultChanModes", string},
			{"QuitMsg",          string},
			{"BufferCount",      integer},
			{"KeepBuffer",       boolean},
			{"Password",         string}
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
			{"Buffer",           integer},
			{"InConfig",         boolean},
			{"KeepBuffer",       boolean},
			{"Detached",         boolean},
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(cvars); ++i) {
			CVarTable.AddRow();
			CVarTable.SetCell("Variable", cvars[i][0]);
			CVarTable.SetCell("Type",     cvars[i][1]);
		}
		PutModule(CVarTable);

		PutModule("You can use $me as the user name for modifying your own user.");
	}

	CUser* GetUser(const CString& username) {
		if (username.Equals("$me"))
			return m_pUser;
		return CZNC::Get().FindUser(username);
	}

	void Get(const CString& sLine) {
		const CString var      = sLine.Token(1).AsLower();
		CString username = sLine.Token(2, true);

		if (var.empty()) {
				PutModule("Usage: get <variable> [username]");
				return;
		}
		if (username.empty()) {
			username = m_pUser->GetUserName();
		}

		CUser* user = GetUser(username);
		if (!user) {
			PutModule("Error: User not found: " + username);
			return;
		}
		if (user != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return;
		}

		if (var == "nick")
			PutModule("Nick = " + user->GetNick());
		else if (var == "altnick")
			PutModule("AltNick = " + user->GetAltNick());
		else if (var == "ident")
			PutModule("Ident = " + user->GetIdent());
		else if (var == "realname")
			PutModule("RealName = " + user->GetRealName());
		else if (var == "vhost")
			PutModule("VHost = " + user->GetVHost());
		else if (var == "multiclients")
			PutModule("MultiClients = " + CString(user->MultiClients()));
		else if (var == "bouncedccs")
			PutModule("BounceDCCs = " + CString(user->BounceDCCs()));
		else if (var == "useclientip")
			PutModule("UseClientIP = " + CString(user->UseClientIP()));
		else if (var == "denyloadmod")
			PutModule("DenyLoadMod = " + CString(user->DenyLoadMod()));
		else if (var == "defaultchanmodes")
			PutModule("DefaultChanModes = " + user->GetDefaultChanModes());
		else if (var == "quitmsg")
			PutModule("QuitMsg = " + user->GetQuitMsg());
		else if (var == "buffercount")
			PutModule("BufferCount = " + CString(user->GetBufferCount()));
		else if (var == "keepbuffer")
			PutModule("KeepBuffer = " + CString(user->KeepBuffer()));
		else
			PutModule("Error: Unknown variable");
	}

	void Set(const CString& sLine) {
		const CString var = sLine.Token(1).AsLower();
		CString username  = sLine.Token(2);
		CString value     = sLine.Token(3, true);

		if (value.empty()) {
			if (!username.empty()) {
				value    = username;
				username = m_pUser->GetUserName();;
			} else {
				PutModule("Usage: set <variable> [username] <value>");
				return;
			}
		}

		CUser* user = GetUser(username);
		if (!user) {
			PutModule("Error: User not found: " + username);
			return;
		}
		if (user != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return;
		}

		if (var == "nick") {
			user->SetNick(value);
			PutModule("Nick = " + value);
		}
		else if (var == "altnick") {
			user->SetAltNick(value);
			PutModule("AltNick = " + value);
		}
		else if (var == "ident") {
			user->SetIdent(value);
			PutModule("Ident = " + value);
		}
		else if (var == "realname") {
			user->SetRealName(value);
			PutModule("RealName = " + value);
		}
		else if (var == "vhost") {
			user->SetVHost(value);
			PutModule("VHost = " + value);
		}
		else if (var == "multiclients") {
			bool b = value.ToBool();
			user->SetMultiClients(b);
			PutModule("MultiClients = " + CString(b));
		}
		else if (var == "bouncedccs") {
			bool b = value.ToBool();
			user->SetBounceDCCs(b);
			PutModule("BounceDCCs = " + CString(b));
		}
		else if (var == "useclientip") {
			bool b = value.ToBool();
			user->SetUseClientIP(b);
			PutModule("UseClientIP = " + CString(b));
		}
		else if (var == "denyloadmod") {
			bool b = value.ToBool();
			user->SetDenyLoadMod(b);
			PutModule("DenyLoadMod = " + CString(b));
		}
		else if (var == "defaultchanmodes") {
			user->SetDefaultChanModes(value);
			PutModule("DefaultChanModes = " + value);
		}
		else if (var == "quitmsg") {
			user->SetQuitMsg(value);
			PutModule("QuitMsg = " + value);
		}
		else if (var == "buffercount") {
			unsigned int i = value.ToUInt();
			user->SetBufferCount(i);
			PutModule("BufferCount = " + value);
		}
		else if (var == "keepbuffer") {
			bool b = value.ToBool();
			user->SetKeepBuffer(b);
			PutModule("KeepBuffer = " + CString(b));
		}
		else if (var == "password") {
			const CString sSalt = CUtils::GetSalt();
			const CString sHash = CUtils::SaltedHash(value, sSalt);
			user->SetPass(sHash, true, sSalt);
			PutModule("Password has been changed!!");
		}
		else
			PutModule("Error: Unknown variable");
	}

	void GetChan(const CString& sLine) {
		const CString var  = sLine.Token(1).AsLower();
		CString username   = sLine.Token(2);
		CString chan = sLine.Token(3, true);

		if (var.empty()) {
			PutModule("Usage: getchan <variable> [username] [chan]");
			return;
		}
		if (chan.empty()) {
			chan = username;
			username = "";
		}
		if (username.empty()) {
			username = m_pUser->GetUserName();
		}

		CUser* user = GetUser(username);
		if (!user) {
			PutModule("Error: User not found: " + username);
			return;
		}
		if (user != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return;
		}

		CChan* pChan = user->FindChan(chan);
		if (!pChan) {
			PutModule("Error: Channel not found: " + chan);
			return;
		}

		if (var == "defmodes")
			PutModule("DefModes = " + pChan->GetDefaultModes());
		else if (var == "buffer")
			PutModule("Buffer = " + CString(pChan->GetBufferCount()));
		else if (var == "inconfig")
			PutModule("InConfig = " + pChan->InConfig());
		else if (var == "keepbuffer")
			PutModule("KeepBuffer = " + pChan->KeepBuffer());
		else if (var == "detached")
			PutModule("Detached = " + pChan->IsDetached());
		else
			PutModule("Error: Unknown variable");
	}

	void SetChan(const CString& sLine) {
		const CString var = sLine.Token(1).AsLower();
		CString username  = sLine.Token(2);
		CString chan      = sLine.Token(3);
		CString value     = sLine.Token(4, true);

		if (value.empty()) {
			if (!username.empty() && !chan.empty()) {
				value    = chan;
				chan     = username;
				username = m_pUser->GetUserName();;
			} else {
				PutModule("Usage: set <variable> [username] <value>");
				return;
			}
		}

		CUser* user = GetUser(username);
		if (!user) {
			PutModule("Error: User not found: " + username);
			return;
		}
		if (user != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return;
		}

		CChan* pChan = user->FindChan(chan);
		if (!pChan) {
			PutModule("Error: Channel not found: " + chan);
			return;
		}

		if (var == "defmodes") {
			pChan->SetDefaultModes(value);
			PutModule("DefModes = " + value);
		} else if (var == "buffer") {
			unsigned int i = value.ToUInt();
			pChan->SetBufferCount(i);
			PutModule("Buffer = " + CString(i));
		} else if (var == "inconfig") {
			bool b = value.ToBool();
			pChan->SetInConfig(b);
			PutModule("InConfig = " + CString(b));
		} else if (var == "keepbuffer") {
			bool b = value.ToBool();
			pChan->SetKeepBuffer(b);
			PutModule("KeepBuffer = " + CString(b));
		} else if (var == "detached") {
			bool b = value.ToBool();
			if (pChan->IsDetached() != b) {
				if (b)
					pChan->DetachUser();
				else
					pChan->AttachUser();
			}
			PutModule("Detached = " + CString(b));
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
		Table.AddColumn("VHost");

		for (map<CString, CUser*>::const_iterator it = msUsers.begin(); it != msUsers.end(); it++) {
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
			Table.SetCell("VHost", it->second->GetVHost());
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
		pNewUser->SetPass(CUtils::SaltedHash(sPassword, sSalt), true, sSalt);
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

		if (!CZNC::Get().AddUser(pNewUser, sError)) {
			delete pNewUser;
			PutModule("Error: User not added! [" + sError + "]");
			return;
		}

		PutModule("User [" + sNewUsername + "] added!");
		return;
	}

	void AddServer(const CString& sLine) {
		CString username = sLine.Token(1);
		CString server   = sLine.Token(2, true);

		if (server.empty()) {
			if (!username.empty()) {
				server = username;
				username = "";
			} else {
				PutModule("Usage: addserver [username] <server>");
				return;
			}
		}

		CUser* user = GetUser(username);
		if (!user) {
			PutModule("Error: User not found: " + username);
			return;
		}
		if (user != m_pUser && !m_pUser->IsAdmin()) {
			PutModule("Error: You need to have admin rights to modify other users!");
			return;
		}

		user->AddServer(server);
		PutModule("Added IRC Server: " + server);
	}


	typedef void (CAdminMod::* fn)(const CString&);
	typedef std::map<CString, fn> function_map;
	function_map fnmap_;

public:
	MODCONSTRUCTOR(CAdminMod) {
		fnmap_["help"]      = &CAdminMod::PrintHelp;
		fnmap_["get"]       = &CAdminMod::Get;
		fnmap_["set"]       = &CAdminMod::Set;
		fnmap_["getchan"]   = &CAdminMod::GetChan;
		fnmap_["setchan"]   = &CAdminMod::SetChan;
		fnmap_["listusers"] = &CAdminMod::ListUsers;
		fnmap_["adduser"]   = &CAdminMod::AddUser;
		fnmap_["deluser"]   = &CAdminMod::DelUser;
		fnmap_["cloneuser"] = &CAdminMod::CloneUser;
		fnmap_["addserver"] = &CAdminMod::AddServer;
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

MODULEDEFS(CAdminMod, "Dynamic configuration of users/settings through irc")
