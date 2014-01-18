/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
 * Copyright (C) 2008 by Stefan Rado
 * based on admin.cpp by Sebastian Ramacher
 * based on admin.cpp in crox branch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>

using std::map;
using std::vector;

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
		HandleHelpCommand();

		PutModule("The following variables are available when using the Set/Get commands:");

		CTable VarTable;
		VarTable.AddColumn("Variable");
		VarTable.AddColumn("Type");
		static const char* str = "String";
		static const char* boolean = "Boolean (true/false)";
		static const char* integer = "Integer";
		static const char* doublenum = "Double";
		static const char* vars[][2] = {
			{"Nick",                str},
			{"Altnick",             str},
			{"Ident",               str},
			{"RealName",            str},
			{"BindHost",            str},
			{"MultiClients",        boolean},
			{"DenyLoadMod",         boolean},
			{"DenySetBindHost",     boolean},
			{"DefaultChanModes",    str},
			{"QuitMsg",             str},
			{"BufferCount",         integer},
			{"AutoClearChanBuffer", boolean},
			{"Password",            str},
			{"JoinTries",           integer},
			{"MaxJoins",            integer},
			{"MaxNetworks",         integer},
			{"Timezone",            str},
			{"Admin",               boolean},
			{"AppendTimestamp",     boolean},
			{"PrependTimestamp",    boolean},
			{"TimestampFormat",     str},
			{"DCCBindHost",         str},
			{"StatusPrefix",        str}
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(vars); ++i) {
			VarTable.AddRow();
			VarTable.SetCell("Variable", vars[i][0]);
			VarTable.SetCell("Type",     vars[i][1]);
		}
		PutModule(VarTable);

		PutModule("The following variables are available when using the SetNetwork/GetNetwork commands:");

		CTable NVarTable;
		NVarTable.AddColumn("Variable");
		NVarTable.AddColumn("Type");
		static const char* nvars[][2] = {
			{"Nick",                str},
			{"Altnick",             str},
			{"Ident",               str},
			{"RealName",            str},
			{"FloodRate",           doublenum},
			{"FloodBurst",          integer},
		};
		for (unsigned int i = 0; i != ARRAY_SIZE(nvars); ++i) {
			NVarTable.AddRow();
			NVarTable.SetCell("Variable", nvars[i][0]);
			NVarTable.SetCell("Type",     nvars[i][1]);
		}
		PutModule(NVarTable);


		PutModule("The following variables are available when using the SetChan/GetChan commands:");
		CTable CVarTable;
		CVarTable.AddColumn("Variable");
		CVarTable.AddColumn("Type");
		static const char* cvars[][2] = {
			{"DefModes",            str},
			{"Key",                 str},
			{"Buffer",              integer},
			{"InConfig",            boolean},
			{"AutoClearChanBuffer", boolean},
			{"Detached",            boolean}
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
			PutModule("Error: User [" + sUsername + "] not found.");
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
			PutModule("KeepBuffer = " + CString(!pUser->AutoClearChanBuffer())); // XXX compatibility crap, added in 0.207
		else if (sVar == "autoclearchanbuffer")
			PutModule("AutoClearChanBuffer = " + CString(pUser->AutoClearChanBuffer()));
		else if (sVar == "maxjoins")
			PutModule("MaxJoins = " + CString(pUser->MaxJoins()));
		else if (sVar == "maxnetworks")
			PutModule("MaxNetworks = " + CString(pUser->MaxNetworks()));
		else if (sVar == "jointries")
			PutModule("JoinTries = " + CString(pUser->JoinTries()));
		else if (sVar == "timezone")
			PutModule("Timezone = " + pUser->GetTimezone());
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
			PutModule("StatusPrefix = " + pUser->GetStatusPrefix());
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
		else if (sVar == "keepbuffer") { // XXX compatibility crap, added in 0.207
			bool b = !sValue.ToBool();
			pUser->SetAutoClearChanBuffer(b);
			PutModule("AutoClearChanBuffer = " + CString(b));
		}
		else if (sVar == "autoclearchanbuffer") {
			bool b = sValue.ToBool();
			pUser->SetAutoClearChanBuffer(b);
			PutModule("AutoClearChanBuffer = " + CString(b));
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
		else if (sVar == "maxnetworks") {
			if(m_pUser->IsAdmin()) {
				unsigned int i = sValue.ToUInt();
				pUser->SetMaxNetworks(i);
				PutModule("MaxNetworks = " + sValue);
			} else {
				PutModule("Access denied!");
			}
		}
		else if (sVar == "jointries") {
			unsigned int i = sValue.ToUInt();
			pUser->SetJoinTries(i);
			PutModule("JoinTries = " + CString(pUser->JoinTries()));
		}
		else if (sVar == "timezone") {
			pUser->SetTimezone(sValue);
			PutModule("Timezone = " + pUser->GetTimezone());
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

	void GetNetwork(const CString& sLine) {
		const CString sVar = sLine.Token(1).AsLower();
		const CString sUsername = sLine.Token(2);
		const CString sNetwork = sLine.Token(3);

		CUser *pUser = NULL;
		CIRCNetwork *pNetwork = NULL;

		if (sUsername.empty()) {
			pUser = m_pUser;
			pNetwork = m_pNetwork;
		} else {
			pUser = GetUser(sUsername);
			if (!pUser) {
				return;
			}

			pNetwork = pUser->FindNetwork(sNetwork);
			if (!pNetwork && !sNetwork.empty()) {
				PutModule("Network [" + sNetwork + "] not found.");
				return;
			}
		}

		if (!pNetwork) {
			PutModule("Usage: GetNetwork <variable> <username> <network>");
			return;
		}

		if (sVar.Equals("nick")) {
			PutModule("Nick = " + pNetwork->GetNick());
		} else if (sVar.Equals("altnick")) {
			PutModule("AltNick = " + pNetwork->GetAltNick());
		} else if (sVar.Equals("ident")) {
			PutModule("Ident = " + pNetwork->GetIdent());
		} else if (sVar.Equals("realname")) {
			PutModule("RealName = " + pNetwork->GetRealName());
		} else if (sVar.Equals("floodrate")) {
			PutModule("FloodRate = " + CString(pNetwork->GetFloodRate()));
		} else if (sVar.Equals("floodburst")) {
			PutModule("FloodBurst = " + CString(pNetwork->GetFloodBurst()));
		} else {
			PutModule("Error: Unknown variable");
		}
	}

	void SetNetwork(const CString& sLine) {
		const CString sVar = sLine.Token(1).AsLower();
		const CString sUsername = sLine.Token(2);
		const CString sNetwork = sLine.Token(3);
		const CString sValue = sLine.Token(4, true);

		CUser *pUser = NULL;
		CIRCNetwork *pNetwork = NULL;

		if (sUsername.empty()) {
			pUser = m_pUser;
			pNetwork = m_pNetwork;
		} else {
			pUser = GetUser(sUsername);
			if (!pUser) {
				return;
			}

			pNetwork = pUser->FindNetwork(sNetwork);
			if (!pNetwork && !sNetwork.empty()) {
				PutModule("Network [" + sNetwork + "] not found.");
				return;
			}
		}

		if (!pNetwork) {
			PutModule("Usage: SetNetwork <variable> <username> <network> <value>");
			return;
		}

		if (sVar.Equals("nick")) {
			pNetwork->SetNick(sValue);
			PutModule("Nick = " + pNetwork->GetNick());
		} else if (sVar.Equals("altnick")) {
			pNetwork->SetAltNick(sValue);
			PutModule("AltNick = " + pNetwork->GetAltNick());
		} else if (sVar.Equals("ident")) {
			pNetwork->SetIdent(sValue);
			PutModule("Ident = " + pNetwork->GetIdent());
		} else if (sVar.Equals("realname")) {
			pNetwork->SetRealName(sValue);
			PutModule("RealName = " + pNetwork->GetRealName());
		} else if (sVar.Equals("floodrate")) {
			pNetwork->SetFloodRate(sValue.ToDouble());
			PutModule("FloodRate = " + CString(pNetwork->GetFloodRate()));
		} else if (sVar.Equals("floodburst")) {
			pNetwork->SetFloodBurst(sValue.ToUShort());
			PutModule("FloodBurst = " + CString(pNetwork->GetFloodBurst()));
		} else {
			PutModule("Error: Unknown variable");
		}
	}
	
	void AddChan(const CString& sLine) {
		const CString sUsername   = sLine.Token(1);
		const CString sNetwork    = sLine.Token(2);
		const CString sChan       = sLine.Token(3);
		
		if (sChan.empty()) {
			PutModule("Usage: addchan <username> <network> <channel>");
			return;
		}
		
		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;
				
		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("Error: [" + sUsername + "] does not have a network named [" + sNetwork + "].");
			return;
		}
		
		if (pNetwork->FindChan(sChan)) {
			PutModule("Error: [" + sUsername + "] already has a channel named [" + sChan + "].");
			return;
		}
		
		CChan* pChan = new CChan(sChan, pNetwork, true);
		pNetwork->AddChan(pChan);
		
		PutModule("Channel [" + sChan + "] for user [" + sUsername + "] added.");
	}
	
	void DelChan(const CString& sLine) {
		const CString sUsername   = sLine.Token(1);
		const CString sNetwork    = sLine.Token(2);
		const CString sChan       = sLine.Token(3);
		
		if (sChan.empty()) {
			PutModule("Usage: delchan <username> <network> <channel>");
			return;
		}
		
		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;
		
		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("Error: [" + sUsername + "] does not have a network named [" + sNetwork + "].");
			return;
		}
		
		CChan* pChan = pNetwork->FindChan(sChan);
		if (!pChan) {
			PutModule("Error: User [" + sUsername + "] does not have a channel named [" + sChan + "].");
			return;
		}
		
		pNetwork->DelChan(sChan);
		pNetwork->PutIRC("PART " + sChan);
		
		PutModule("Channel [" + sChan + "] for user [" + sUsername + "] deleted.");
	}

	void GetChan(const CString& sLine) {
		const CString sVar  = sLine.Token(1).AsLower();
		CString sUsername   = sLine.Token(2);
		CString sNetwork    = sLine.Token(3);
		CString sChan = sLine.Token(4, true);

		if (sChan.empty()) {
			PutModule("Usage: getchan <variable> <username> <network> <chan>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("[" + sUsername + "] does not have a network named [" + sNetwork + "]");
			return;
		}

		CChan* pChan = pNetwork->FindChan(sChan);
		if (!pChan) {
			PutModule("Error: Channel [" + sChan + "] not found.");
			return;
		}

		if (sVar == "defmodes")
			PutModule("DefModes = " + pChan->GetDefaultModes());
		else if (sVar == "buffer")
			PutModule("Buffer = " + CString(pChan->GetBufferCount()));
		else if (sVar == "inconfig")
			PutModule("InConfig = " + CString(pChan->InConfig()));
		else if (sVar == "keepbuffer")
			PutModule("KeepBuffer = " + CString(!pChan->AutoClearChanBuffer()));// XXX compatibility crap, added in 0.207
		else if (sVar == "autoclearchanbuffer")
			PutModule("AutoClearChanBuffer = " + CString(pChan->AutoClearChanBuffer()));
		else if (sVar == "detached")
			PutModule("Detached = " + CString(pChan->IsDetached()));
		else if (sVar == "key")
			PutModule("Key = " + pChan->GetKey());
		else
			PutModule("Error: Unknown variable");
	}

	void SetChan(const CString& sLine) {
		const CString sVar = sLine.Token(1).AsLower();
		CString sUsername  = sLine.Token(2);
		CString sNetwork   = sLine.Token(3);
		CString sChan      = sLine.Token(4);
		CString sValue     = sLine.Token(5, true);

		if (sValue.empty()) {
			PutModule("Usage: setchan <variable> <username> <network> <chan> <value>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("[" + sUsername + "] does not have a network named [" + sNetwork + "]");
			return;
		}

		CChan* pChan = pNetwork->FindChan(sChan);
		if (!pChan) {
			PutModule("Error: Channel [" + sChan + "] not found.");
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
		} else if (sVar == "keepbuffer") { // XXX compatibility crap, added in 0.207
			bool b = !sValue.ToBool();
			pChan->SetAutoClearChanBuffer(b);
			PutModule("AutoClearChanBuffer = " + CString(b));
		} else if (sVar == "autoclearchanbuffer") {
			bool b = sValue.ToBool();
			pChan->SetAutoClearChanBuffer(b);
			PutModule("AutoClearChanBuffer = " + CString(b));
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
			sPassword  = sLine.Token(2);
		if (sPassword.empty()) {
			PutModule("Usage: adduser <username> <password>");
			return;
		}

		if (CZNC::Get().FindUser(sUsername)) {
			PutModule("Error: User [" + sUsername + "] already exists!");
			return;
		}

		CUser* pNewUser = new CUser(sUsername);
		CString sSalt = CUtils::GetSalt();
		pNewUser->SetPass(CUser::SaltedHash(sPassword, sSalt), CUser::HASH_DEFAULT, sSalt);

		CString sErr;
		if (!CZNC::Get().AddUser(pNewUser, sErr)) {
			delete pNewUser;
			PutModule("Error: User not added! [" + sErr + "]");
			return;
		}

		PutModule("User [" + sUsername + "] added!");
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
			PutModule("Error: User [" + sUsername + "] does not exist!");
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

		CUser* pNewUser = new CUser(sNewUsername);
		CString sError;
		if (!pNewUser->Clone(*pOldUser, sError)) {
			delete pNewUser;
			PutModule("Error: Cloning failed! [" + sError + "]");
			return;
		}

		if (!CZNC::Get().AddUser(pNewUser, sError)) {
			delete pNewUser;
			PutModule("Error: User not added! [" + sError + "]");
			return;
		}

		PutModule("User [" + sNewUsername + "] added!");
		return;
	}

	void AddNetwork(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sNetwork = sLine.Token(2);
		CUser *pUser = m_pUser;

		if (sNetwork.empty()) {
			sNetwork = sUser;
		} else {
			pUser = GetUser(sUser);
			if (!pUser) {
				PutModule("User [" + sUser + "] not found");
				return;
			}
		}

		if (sNetwork.empty()) {
			PutModule("Usage: " + sLine.Token(0) + " [user] network");
			return;
		}

		if (!m_pUser->IsAdmin() && !pUser->HasSpaceForNewNetwork()) {
			PutStatus("Network number limit reached. Ask an admin to increase the limit for you, or delete unneeded networks using /znc DelNetwork <name>");
			return;
		}

		if (pUser->FindNetwork(sNetwork)) {
			PutModule("[" + pUser->GetUserName() + "] already has a network with the name [" + sNetwork + "]");
			return;
		}

		CString sNetworkAddError;
		if (pUser->AddNetwork(sNetwork, sNetworkAddError)) {
			PutModule("Network [" + sNetwork + "] added for user [" + pUser->GetUserName() + "].");
		} else {
			PutModule("Network [" + sNetwork + "] could not be added for user [" + pUser->GetUserName() + "]: " + sNetworkAddError);
		}
	}

	void DelNetwork(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CString sNetwork = sLine.Token(2);
		CUser *pUser = m_pUser;

		if (sNetwork.empty()) {
			sNetwork = sUser;
		} else {
			pUser = GetUser(sUser);
			if (!pUser) {
				return;
			}
		}

		if (sNetwork.empty()) {
			PutModule("Usage: " + sLine.Token(0) + " [user] network");
			return;
		}

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);

		if (!pNetwork) {
			PutModule("[" + pUser->GetUserName() + "] does not have a network with the name [" + sNetwork + "]");
			return;
		}

		if (pNetwork == m_pNetwork) {
			PutModule("The currently active network can be deleted via " + m_pUser->GetStatusPrefix() + "status");
			return;
		}

		if (pUser->DeleteNetwork(sNetwork)) {
			PutModule("Network [" + sNetwork + "] deleted on user [" + pUser->GetUserName() + "].");
		} else {
			PutModule("Network [" + sNetwork + "] could not be deleted for user [" + pUser->GetUserName() + "].");
		}
	}

	void ListNetworks(const CString& sLine) {
		CString sUser = sLine.Token(1);
		CUser *pUser = m_pUser;

		if (!sUser.empty()) {
			pUser = GetUser(sUser);
			if (!pUser) {
				return;
			}
		}

		const vector<CIRCNetwork*>& vNetworks = pUser->GetNetworks();

		CTable Table;
		Table.AddColumn("Network");
		Table.AddColumn("OnIRC");
		Table.AddColumn("IRC Server");
		Table.AddColumn("IRC User");
		Table.AddColumn("Channels");

		for (unsigned int a = 0; a < vNetworks.size(); a++) {
			CIRCNetwork* pNetwork = vNetworks[a];
			Table.AddRow();
			Table.SetCell("Network", pNetwork->GetName());
			if (pNetwork->IsIRCConnected()) {
				Table.SetCell("OnIRC", "Yes");
				Table.SetCell("IRC Server", pNetwork->GetIRCServer());
				Table.SetCell("IRC User", pNetwork->GetIRCNick().GetNickMask());
				Table.SetCell("Channels", CString(pNetwork->GetChans().size()));
			} else {
				Table.SetCell("OnIRC", "No");
			}
		}

		if (PutModule(Table) == 0) {
			PutModule("No networks");
		}
	}

	void AddServer(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sNetwork = sLine.Token(2);
		CString sServer = sLine.Token(3, true);

		if (sServer.empty()) {
			PutModule("Usage: addserver <username> <network> <server>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("[" + sUsername + "] does not have a network with the name [" + sNetwork + "]");
			return;
		}

		if (pNetwork->AddServer(sServer))
			PutModule("Added IRC Server [" + sServer + "] for network [" + sNetwork + "] for user [" + pUser->GetUserName() + "].");
		else
			PutModule("Could not add IRC server [" + sServer + "] for network [" + sNetwork + "] for user [" + pUser->GetUserName() + "].");
	}

	void ReconnectUser(const CString& sLine) {
		CString sUserName = sLine.Token(1);
		CString sNetwork = sLine.Token(2);

		if (sNetwork.empty()) {
			PutModule("Usage: Reconnect <username> <network>");
			return;
		}

		CUser* pUser = GetUser(sUserName);
		if (!pUser) {
			PutModule("User [" + sUserName + "] not found.");
			return;
		}

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("[" + sUserName + "] does not have a network with the name [" + sNetwork + "]");
			return;
		}

		CIRCSock *pIRCSock = pNetwork->GetIRCSock();
		// cancel connection attempt:
		if (pIRCSock && !pIRCSock->IsConnected()) {
			pIRCSock->Close();
		}
		// or close existing connection:
		else if(pIRCSock) {
			pIRCSock->Quit();
		}

		// then reconnect
		pNetwork->SetIRCConnectEnabled(true);

		PutModule("Queued network [" + sNetwork + "] for user [" + pUser->GetUserName() + "] for a reconnect.");
	}

	void DisconnectUser(const CString& sLine) {
		CString sUserName = sLine.Token(1);
		CString sNetwork = sLine.Token(2);

		if (sNetwork.empty()) {
			PutModule("Usage: Disconnect <username> <network>");
			return;
		}

		CUser* pUser = GetUser(sUserName);
		if (!pUser) {
			PutModule("User [" + sUserName + "] not found.");
			return;
		}

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("[" + sUserName + "] does not have a network with the name [" + sNetwork + "]");
			return;
		}

		pNetwork->SetIRCConnectEnabled(false);
		PutModule("Closed IRC connection for network [" + sNetwork + "] on user [" + sUserName + "].");
	}

	void ListCTCP(const CString& sLine) {
		CString sUserName = sLine.Token(1, true);

		if (sUserName.empty()) {
			sUserName = m_pUser->GetUserName();
		}
		CUser* pUser = GetUser(sUserName);
		if (!pUser)
			return;

		const MCString& msCTCPReplies = pUser->GetCTCPReplies();
		CTable Table;
		Table.AddColumn("Request");
		Table.AddColumn("Reply");
		for (MCString::const_iterator it = msCTCPReplies.begin(); it != msCTCPReplies.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Request", it->first);
			Table.SetCell("Reply", it->second);
		}

		if (Table.empty()) {
			PutModule("No CTCP replies for user [" + pUser->GetUserName() + "] configured!");
		} else {
			PutModule("CTCP replies for user [" + pUser->GetUserName() + "]:");
			PutModule(Table);
		}
	}

	void AddCTCP(const CString& sLine) {
		CString sUserName    = sLine.Token(1);
		CString sCTCPRequest = sLine.Token(2);
		CString sCTCPReply   = sLine.Token(3, true);

		if (sCTCPRequest.empty()) {
			sCTCPRequest = sUserName;
			sCTCPReply = sLine.Token(2, true);
			sUserName = m_pUser->GetUserName();
		}
		if (sCTCPRequest.empty()) {
			PutModule("Usage: AddCTCP [user] [request] [reply]");
			PutModule("This will cause ZNC to reply to the CTCP instead of forwarding it to clients.");
			PutModule("An empty reply will cause the CTCP request to be blocked.");
			return;
		}

		CUser* pUser = GetUser(sUserName);
		if (!pUser)
			return;

		if (pUser->AddCTCPReply(sCTCPRequest, sCTCPReply))
			PutModule("Added!");
		else
			PutModule("Error!");
	}

	void DelCTCP(const CString& sLine) {
		CString sUserName    = sLine.Token(1);
		CString sCTCPRequest = sLine.Token(2, true);

		if (sCTCPRequest.empty()) {
			sCTCPRequest = sUserName;
			sUserName = m_pUser->GetUserName();
		}
		CUser* pUser = GetUser(sUserName);
		if (!pUser)
			return;

		if (sCTCPRequest.empty()) {
			PutModule("Usage: DelCTCP [user] [request]");
			return;
		}

		if (pUser->DelCTCPReply(sCTCPRequest))
			PutModule("Successfully removed [" + sCTCPRequest + "] for user [" + pUser->GetUserName() + "].");
		else
			PutModule("Error: [" + sCTCPRequest + "] not found for user [" + pUser->GetUserName() + "]!");
	}

	void LoadModuleFor(CModules& Modules, const CString& sModName, const CString& sArgs, CModInfo::EModuleType eType, CUser* pUser, CIRCNetwork* pNetwork) {
		if (pUser->DenyLoadMod() && !m_pUser->IsAdmin()) {
			PutModule("Loading modules has been disabled.");
			return;
		}

		CString sModRet;
		CModule *pMod = Modules.FindModule(sModName);
		if (!pMod) {
			if (!Modules.LoadModule(sModName, sArgs, eType, pUser, pNetwork, sModRet)) {
				PutModule("Unable to load module [" + sModName + "] [" + sModRet + "]");
			} else {
				PutModule("Loaded module [" + sModName + "]");
			}
		} else if (pMod->GetArgs() != sArgs) {
			if (!Modules.ReloadModule(sModName, sArgs, pUser, pNetwork, sModRet)) {
				PutModule("Unable to reload module [" + sModName + "] [" + sModRet + "]");
			} else {
				PutModule("Reloaded module [" + sModName + "]");
			}
		} else {
			PutModule("Unable to load module [" + sModName + "] because it is already loaded");
		}
	}

	void LoadModuleForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sModName  = sLine.Token(2);
		CString sArgs     = sLine.Token(3, true);

		if (sModName.empty()) {
			PutModule("Usage: loadmodule <username> <modulename> [<args>]");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		LoadModuleFor(pUser->GetModules(), sModName, sArgs, CModInfo::UserModule, pUser, NULL);
	}

	void LoadModuleForNetwork(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sNetwork  = sLine.Token(2);
		CString sModName  = sLine.Token(3);
		CString sArgs     = sLine.Token(4, true);

		if (sModName.empty()) {
			PutModule("Usage: loadnetmodule <username> <network> <modulename> [<args>]");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("Network not found");
			return;
		}

		LoadModuleFor(pNetwork->GetModules(), sModName, sArgs, CModInfo::NetworkModule, pUser, pNetwork);
	}

	void UnLoadModuleFor(CModules& Modules, const CString& sModName, CUser* pUser) {
		if (pUser->DenyLoadMod() && !m_pUser->IsAdmin()) {
			PutModule("Loading modules has been disabled.");
			return;
		}

		if (Modules.FindModule(sModName) == this) {
			PutModule("Please use /znc unloadmod " + sModName);
			return;
		}

		CString sModRet;
		if (!Modules.UnloadModule(sModName, sModRet)) {
			PutModule("Unable to unload module [" + sModName + "] [" + sModRet + "]");
		} else {
			PutModule("Unloaded module [" + sModName + "]");
		}
	}

	void UnLoadModuleForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sModName  = sLine.Token(2);

		if (sModName.empty()) {
			PutModule("Usage: unloadmodule <username> <modulename>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		UnLoadModuleFor(pUser->GetModules(), sModName, pUser);
	}

	void UnLoadModuleForNetwork(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sNetwork  = sLine.Token(2);
		CString sModName  = sLine.Token(3);

		if (sModName.empty()) {
			PutModule("Usage: unloadnetmodule <username> <network> <modulename>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("Network not found");
			return;
		}

		UnLoadModuleFor(pNetwork->GetModules(), sModName, pUser);
	}

	void ListModulesFor(CModules& Modules, const CString& sWhere) {
		if (!Modules.size()) {
			PutModule(sWhere + " has no modules loaded.");
		} else {
			PutModule("Modules loaded for " + sWhere + ":");
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

	void ListModulesForUser(const CString& sLine) {
		CString sUsername = sLine.Token(1);

		if (sUsername.empty()) {
			PutModule("Usage: listmods <username>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		ListModulesFor(pUser->GetModules(), "User [" + pUser->GetUserName() + "]");
	}

	void ListModulesForNetwork(const CString& sLine) {
		CString sUsername = sLine.Token(1);
		CString sNetwork  = sLine.Token(2);

		if (sNetwork.empty()) {
			PutModule("Usage: listnetmods <username> <network>");
			return;
		}

		CUser* pUser = GetUser(sUsername);
		if (!pUser)
			return;

		CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
		if (!pNetwork) {
			PutModule("Network not found");
			return;
		}

		ListModulesFor(pNetwork->GetModules(), "Network [" + pNetwork->GetName() + "] of user [" + pUser->GetUserName() + "]");
	}

public:
	MODCONSTRUCTOR(CAdminMod) {
		AddCommand("Help",         static_cast<CModCommand::ModCmdFunc>(&CAdminMod::PrintHelp),
			"",                                     "Generates this output");
		AddCommand("Get",          static_cast<CModCommand::ModCmdFunc>(&CAdminMod::Get),
			"variable [username]",                  "Prints the variable's value for the given or current user");
		AddCommand("Set",          static_cast<CModCommand::ModCmdFunc>(&CAdminMod::Set),
			"variable username value",              "Sets the variable's value for the given user (use $me for the current user)");
		AddCommand("GetNetwork",   static_cast<CModCommand::ModCmdFunc>(&CAdminMod::GetNetwork),
			"variable [username network]",          "Prints the variable's value for the given network");
		AddCommand("SetNetwork",   static_cast<CModCommand::ModCmdFunc>(&CAdminMod::SetNetwork),
			"variable username network value",      "Sets the variable's value for the given network");
		AddCommand("GetChan",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::GetChan),
			"variable [username] network chan",     "Prints the variable's value for the given channel");
		AddCommand("SetChan",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::SetChan),
			"variable username network chan value", "Sets the variable's value for the given channel");
		AddCommand("AddChan",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::AddChan),
			"username network chan",                "Adds a new channel");
		AddCommand("DelChan",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::DelChan),
			"username network chan",                "Deletes a channel");
		AddCommand("ListUsers",    static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ListUsers),
			"",                                     "Lists users");
		AddCommand("AddUser",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::AddUser),
			"username password",                    "Adds a new user");
		AddCommand("DelUser",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::DelUser),
			"username",                             "Deletes a user");
		AddCommand("CloneUser",    static_cast<CModCommand::ModCmdFunc>(&CAdminMod::CloneUser),
			"oldusername newusername",              "Clones a user");
		AddCommand("AddServer",    static_cast<CModCommand::ModCmdFunc>(&CAdminMod::AddServer),
			"username network server",              "Adds a new IRC server for the given or current user");
		AddCommand("Reconnect",    static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ReconnectUser),
			"username network",                     "Cycles the user's IRC server connection");
		AddCommand("Disconnect",   static_cast<CModCommand::ModCmdFunc>(&CAdminMod::DisconnectUser),
			"username network",                     "Disconnects the user from their IRC server");
		AddCommand("LoadModule",   static_cast<CModCommand::ModCmdFunc>(&CAdminMod::LoadModuleForUser),
			"username modulename [args]",           "Loads a Module for a user");
		AddCommand("UnLoadModule", static_cast<CModCommand::ModCmdFunc>(&CAdminMod::UnLoadModuleForUser),
			"username modulename",                  "Removes a Module of a user");
		AddCommand("ListMods",     static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ListModulesForUser),
			"username",                             "Get the list of modules for a user");
		AddCommand("LoadNetModule",static_cast<CModCommand::ModCmdFunc>(&CAdminMod::LoadModuleForNetwork),
			"username network modulename [args]",   "Loads a Module for a network");
		AddCommand("UnLoadNetModule",static_cast<CModCommand::ModCmdFunc>(&CAdminMod::UnLoadModuleForNetwork),
			"username network modulename",          "Removes a Module of a network");
		AddCommand("ListNetMods",  static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ListModulesForNetwork),
			"username network",                     "Get the list of modules for a network");
		AddCommand("ListCTCPs",    static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ListCTCP),
			"username",                             "List the configured CTCP replies");
		AddCommand("AddCTCP",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::AddCTCP),
			"username ctcp [reply]",                "Configure a new CTCP reply");
		AddCommand("DelCTCP",      static_cast<CModCommand::ModCmdFunc>(&CAdminMod::DelCTCP),
			"username ctcp",                        "Remove a CTCP reply");

		// Network commands
		AddCommand("AddNetwork", static_cast<CModCommand::ModCmdFunc>(&CAdminMod::AddNetwork),
			"[username] network",                   "Add a network for a user");
		AddCommand("DelNetwork", static_cast<CModCommand::ModCmdFunc>(&CAdminMod::DelNetwork),
			"[username] network",                   "Delete a network for a user");
		AddCommand("ListNetworks", static_cast<CModCommand::ModCmdFunc>(&CAdminMod::ListNetworks),
			"[username]",                           "List all networks for a user");
	}

	virtual ~CAdminMod() {}
};

template<> void TModInfo<CAdminMod>(CModInfo& Info) {
	Info.SetWikiPage("controlpanel");
}

USERMODULEDEFS(CAdminMod, "Dynamic configuration through IRC. Allows editing only yourself if you're not ZNC admin.")
