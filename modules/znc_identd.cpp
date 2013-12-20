/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#include <znc/ExecSock.h>
#include <znc/FileUtils.h>
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>

#include <map>
#include <algorithm>

using std::map;
using std::remove_if;

struct IdentPortInfo
{
	uint16_t local_port;
	uint16_t remote_port;
	CString ident_response;
};

class CNativeIdentdModule;

class IdentExecSock : public CExecSock
{
public:
	CNativeIdentdModule *module;

public:
	virtual ~IdentExecSock() {}

	IdentExecSock(CNativeIdentdModule *mod) : CExecSock(), module(mod)
	{
		EnableReadLine();
		SetTimeout(0);
	}

	virtual void ReadLine(const CString &sLine);
	virtual void Disconnected();
};

class CNativeIdentdModule : public CModule {

public:
	typedef enum
	{
		EIM_USERNAME = 0,	// use the znc username as ident string
		EIM_UNHASH = 1,		// use the hash of ZNC username as ident string
		EIM_RANDOM = 2,		// use random value as ident string
		EIM_RANDINT = 3		// use random integer value as ident string
	} EIdentMode;
	static const CString EIdentMode_names[4];

	EIdentMode 			m_eMode;
	IdentExecSock		       *m_RemoteIdentd;
	map<CString, IdentPortInfo>	m_PortsByUsername;
	CString				m_sIdentPrefix;

	bool 				m_bVerboseIdentd;		// if set, messages printed by the identd are broadcast to all admins
	bool 				m_bStopRegistrationOnFail;	// if set, users will fail to connect to irc if the identd is for some reason unavailable
	bool 				m_bForceUpdateIdent;	// if set, users ident strings will be forced to the ident string the identd will use
	bool 				m_bAllowAdminAnyIdent;	// if set, the ident strings provided by znc admins will be used verbatim

	void ResetTableCommand(const CString &line)
	{
		ResetIdentResponses();
		m_PortsByUsername.clear();
	}

	void DumpTableCommand(const CString &line)
	{
		CTable tIdentTable;
		tIdentTable.AddColumn("Username");
		tIdentTable.AddColumn("User Type");
		tIdentTable.AddColumn("Local Port");
		tIdentTable.AddColumn("Remote Port");
		tIdentTable.AddColumn("Ident Response");
		for (map<CString, IdentPortInfo>::iterator i = m_PortsByUsername.begin(); i != m_PortsByUsername.end(); ++i)
		{
			CUser *u = CZNC::Get().FindUser(i->first);
			tIdentTable.AddRow();
			tIdentTable.SetCell("Username", i->first);
			tIdentTable.SetCell("User Type", (u == NULL) ? "[deleted]" : (u->IsAdmin() ? "admin" : ""));
			tIdentTable.SetCell("Local Port", CString(i->second.local_port));
			tIdentTable.SetCell("Remote Port", CString(i->second.remote_port));
			tIdentTable.SetCell("Ident Response", i->second.ident_response);
		}

		int tabIndex = 0;
		CString l;
		while (tIdentTable.GetLine(tabIndex++, l)) PutModule(l);
	}

	void StopIdentdCommand(const CString &line)
	{
		if (m_RemoteIdentd == NULL)
		{
			PutModule("Remote identd is already stopped.");
			return;
		}
		ExitIdentd();
		m_PortsByUsername.clear();
	}

	void StartIdentdCommand(const CString &line)
	{
		if (m_RemoteIdentd != NULL)
		{
			PutModule("Remote identd is already running.");
			return;
		}
		if (!StartIdentd()) PutModule("Failed to start ident server!");
	}

	void RestartIdentdCommand(const CString &line)
	{
		StopIdentdCommand(line);
		m_RemoteIdentd = NULL;
		StartIdentdCommand(line);
	}

	void SetVerboseCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true).MakeLower());
		m_bVerboseIdentd = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
		PutModule(CString("Verbose identd ") + (m_bVerboseIdentd ? "enabled." : "disabled."));
		SetNV("verbose", m_bVerboseIdentd ? "1" : "0");
	}

	void SetSOFCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true).MakeLower());
		m_bStopRegistrationOnFail = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
		PutModule(CString("Registration halt on identd failure ") + (m_bStopRegistrationOnFail ? "enabled." : "disabled."));
		SetNV("SOF", m_bStopRegistrationOnFail ? "1" : "0");
	}

	void SetForceIdentCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true).MakeLower());
		m_bForceUpdateIdent = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
		PutModule(CString("Ident force rewrite ") + (m_bForceUpdateIdent ? "enabled." : "disabled."));
		SetNV("force", m_bVerboseIdentd ? "1" : "0");
	}

	void SetOverrideCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true).MakeLower());
		m_bAllowAdminAnyIdent = (sArg.Equals("1") || sArg.Equals("on") || sArg.Equals("true"));
		PutModule(CString("Admin override ") + (m_bAllowAdminAnyIdent ? "enabled." : "disabled."));
		SetNV("override", m_bVerboseIdentd ? "1" : "0");
	}

	void SetModeCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true).MakeLower());
		EIdentMode newmode = m_eMode;
		if (sArg == "0" || sArg == EIdentMode_names[0]) newmode = EIM_USERNAME;
		if (sArg == "1" || sArg == EIdentMode_names[1]) newmode = EIM_UNHASH;
		if (sArg == "2" || sArg == EIdentMode_names[2]) newmode = EIM_RANDOM;
		if (sArg == "3" || sArg == EIdentMode_names[3]) newmode = EIM_RANDINT;
		PutModule(CString("Ident mode ") + (m_eMode == newmode ? CString("unchanged.") : (CString("set to ") + EIdentMode_names[newmode])));
		m_eMode = newmode;
		SetNV("mode", CString((int) m_eMode));
	}

	void SetPrefixCommand(const CString &line)
	{
		const CString sArg(line.Token(1, true));
		m_sIdentPrefix = sArg;
		PutModule(CString("Ident prefix set to '") + m_sIdentPrefix + "'.");
		SetNV("prefix", m_sIdentPrefix);
	}

	void ShowSettingsCommand(const CString &line)
	{
		PutModule("Current configuration:");
		PutModule("Verbose: " + CString(m_bVerboseIdentd));
		PutModule("Stop on fail: " + CString(m_bStopRegistrationOnFail));
		PutModule("Force ident: " + CString(m_bForceUpdateIdent));
		PutModule("Admin Override: " + CString(m_bAllowAdminAnyIdent));
		PutModule("Ident prefix: '" + m_sIdentPrefix + "'");
		PutModule("Mode: " + EIdentMode_names[m_eMode]);
	}
	
	MODCONSTRUCTOR(CNativeIdentdModule) {
		AddHelpCommand();
		// refer to these help messages when wondering what the ...Command() functions above do
		AddCommand("ResetTable", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::ResetTableCommand), "", "Clears the table of identd responses.");
		AddCommand("DumpTable", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::DumpTableCommand), "", "Prints the table of identd responses.");
		AddCommand("StopIdentd", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::StopIdentdCommand), "", "Exits the existing identd (if one exists).");
		AddCommand("StartIdentd", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::StartIdentdCommand), "", "Attempts to spawn another identd (if one does not exist).");
		AddCommand("RestartIdentd", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::RestartIdentdCommand), "", "Equivalent to StopIdentd followed by StartIdentd.");
		AddCommand("SetVerbose", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetVerboseCommand), "BOOLEAN", "If set, broadcasts all messages produced by the identd to admins.");
		AddCommand("SetStopOnFail", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetSOFCommand), "BOOLEAN", "If set, stops clients from connecting to servers if the identd is down.");
		AddCommand("SetForceIdent", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetForceIdentCommand), "BOOLEAN", "If set, forces the ident sent by clients to the server to the 'official' one.");
		AddCommand("SetAdminOverride", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetOverrideCommand), "BOOLEAN", "If set, the identd will validate any ident string used by a ZNC admin.");
		AddCommand("SetMode", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetModeCommand), "username | hash | random | randint", "Sets the ident response method for all future connections.");
		AddCommand("SetPrefix", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::SetPrefixCommand), "STRING", "Assigns a static prefix to every ident (except idents of admins if Admin Override is true).");
		AddCommand("ShowSettings", static_cast<CModCommand::ModCmdFunc>(&CNativeIdentdModule::ShowSettingsCommand), "", "Displays all settings.");

		m_RemoteIdentd = NULL;

		// reasonable defaults for settings that are initialized later
		m_bVerboseIdentd = false;
		m_bStopRegistrationOnFail = false;
		m_bForceUpdateIdent = true;
		m_bAllowAdminAnyIdent = false;
		m_sIdentPrefix = "";
		m_eMode = EIM_USERNAME;
	}

	bool StartIdentd()
	{
		m_RemoteIdentd = new IdentExecSock(this);
		if (m_RemoteIdentd->Execute("znc-identd") < 0) // if there's an error starting it
		{
			delete m_RemoteIdentd;
			return false;
		}

		// otherwise notify that it's started
		CZNC::Get().Broadcast("Identd process has started (PID=" + CString(m_RemoteIdentd->GetPID()) + ").", true);
		m_pManager->AddSock(m_RemoteIdentd, "znc-identd");
		return true;
	}

	virtual bool OnLoad(const CString &sArgs, CString &sMessage)
	{
		// only one identd can be running systemwide, so force this to run as a global module
		if (GetType() != CModInfo::GlobalModule)
		{
			sMessage = "This module must be loaded globally.";
			return false;
		}

		// load persistent settings
		m_bVerboseIdentd = (GetNV("verbose") == "1") ? true : false;
		m_bStopRegistrationOnFail = (GetNV("SOF") == "1") ? true : false;
		m_bForceUpdateIdent = (GetNV("force") == "0") ? false : true;
		m_bAllowAdminAnyIdent = (GetNV("override") == "1") ? true : false;
		m_sIdentPrefix = GetNV("prefix");

		int value = 0;
		GetNV("mode").Convert(&value);
		m_eMode = (EIdentMode) value;

		// launch identd process
		if (!StartIdentd()) sMessage = "Failed to start ident server!";
		return true;
	}

	virtual ~CNativeIdentdModule() {
		// if it's already dead, do nothing
		if (m_RemoteIdentd == NULL) return;

		// kill the identd and delete the socket (this must happen manually otherwise 
		// the socket will persist after the module is unloaded which will cause segment faults
		// when we try to call into the various members of IdentExecSock after those members
		// are deallocated)
		ExitIdentd();
		m_pManager->DelSockByAddr(m_RemoteIdentd);
	}

	void RegisterIdentResponse(int local_port, int remote_port, const CString &response)
	{
		if (m_RemoteIdentd == NULL) return;
		*m_RemoteIdentd << local_port << " " << remote_port << " " << response << "\n";
		// 12346 6667 my.ident.response[ENDL]
	}

	void UnregisterIdentResponse(int local_port, int remote_port)
	{
		if (m_RemoteIdentd == NULL) return;
		*m_RemoteIdentd << local_port << " " << remote_port << "\n";
		// 12346 6667[ENDL]
	}

	void ResetIdentResponses()
	{
		if (m_RemoteIdentd == NULL) return;
		*m_RemoteIdentd << "reset" << "\n";
		m_PortsByUsername.clear();
		// reset[ENDL]
	}

	void ExitIdentd()
	{
		if (m_RemoteIdentd == NULL) return;
		*m_RemoteIdentd << "exit" << "\n";
		m_PortsByUsername.clear();
		// exit[ENDL]
	}

	static bool NotAllowed(char c)
	{
		// used to filter out characters that are not allowed in ident responses

		// easy contiguous cases
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;

		// uglier sparse cases
		switch (c)
		{
		case '-':
		case '.':
		case '[':
		case '\\':
		case ']':
		case '^':
		case '_':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
			return true;
		default:
			return false;
		}
	}

	CString GetIdentString(CUser *user, const CString &origIdent)
	{
		CString sResponse;

		// override mode
		if (m_bAllowAdminAnyIdent && user->IsAdmin()) sResponse = origIdent;
		else
		{
			switch (m_eMode)
			{
			case EIM_USERNAME:
				sResponse = m_sIdentPrefix + user->GetUserName(); break; // just the username
			case EIM_UNHASH:
				sResponse = m_sIdentPrefix + user->GetUserName().SHA256(); break; // sha256 hash of username
			case EIM_RANDOM:
				sResponse = m_sIdentPrefix + CString::RandomString(16); break; // a random 16 char string
			case EIM_RANDINT:
				sResponse = m_sIdentPrefix + CString((rand() ^ (rand() << 16)) % 100000000); break; // a roughly random integer between 0 and 999999999
			}
		}

		// get rid of any characters that aren't valid ident characters
		sResponse.erase(std::remove_if(sResponse.begin(), sResponse.end(), NotAllowed), sResponse.end());
		return sResponse;
	}

	void OnModCommand(const CString& sCommand) {
		// only admins may command this module
		if (m_pUser->IsAdmin()) {
			HandleCommand(sCommand);
		} else {
			PutModule("Access denied");
		}
	}

	virtual EModRet OnIRCRegistration(CString& sPass, CString& sNick, CString& sIdent, CString& sRealName)
	// this hook is called by CIRCSock's Connected() hook, which makes it exactly what we want (though we don't use it exactly the way it's named)
	{
		// abort if stop-on-fail is set and something is missing or wrong
		if (m_pNetwork == NULL || m_pNetwork->GetIRCSock() == NULL || m_pUser == NULL || m_RemoteIdentd == NULL) return m_bStopRegistrationOnFail ? HALT : CONTINUE;

		// get endpoint information
		CIRCSock *pIRCSock = m_pNetwork->GetIRCSock();
		int local_port = pIRCSock->GetLocalPort(), remote_port = pIRCSock->GetRemotePort();
		CString username = m_pUser->GetUserName();

		// map the ident response into the table
		IdentPortInfo &ipi = m_PortsByUsername[username];
		ipi.local_port = local_port;
		ipi.remote_port = remote_port;
		ipi.ident_response = GetIdentString(m_pUser, sIdent);

		// push the ident response to the daemon
		RegisterIdentResponse(local_port, remote_port, ipi.ident_response);

		// if force-update is set, change the ident sent by the client to the one the identd expects
		if (m_bForceUpdateIdent) sIdent = ipi.ident_response;
		return CONTINUE;
	}

	virtual void OnIRCConnectionError(CIRCSock *pIRCSock) {
		// forward
		OnIRCDisconnected();
	}

	virtual void OnIRCDisconnected() {
                if (m_pUser == NULL)
                {
			// this should probably never happen
                        CZNC::Get().Broadcast("znc-identd encountered a bizarre error in OnIRCDisconnected(): required state was null!");
			return;
                }

		// try to lookup the ident response for this port pair (and do nothing if the lookup fails)
                CString username = m_pUser->GetUserName();
		map<CString, IdentPortInfo>::iterator i = m_PortsByUsername.find(username);
		if (i == m_PortsByUsername.end()) return;
                IdentPortInfo &ipi = i->second;

		// push the ident cancellation to the daemon, erase from the table
                UnregisterIdentResponse(ipi.local_port, ipi.remote_port);
                m_PortsByUsername.erase(username);
	}
};

// names for the EIdentModes (indexed by mode)
const CString CNativeIdentdModule::EIdentMode_names[4] = {"username", "hash", "random", "randint"};

void IdentExecSock::Disconnected()
{
	CZNC::Get().Broadcast("Identd process has shut down.", true);

	// we only want to remove the ident sock from the module if it equals the ident module that's being disconnected
	// otherwise we might accidentally remove the one that replaced us (e.g. if we were restarting)
	if (module->m_RemoteIdentd == this) module->m_RemoteIdentd = NULL;
}

void IdentExecSock::ReadLine(const CString &sLine)
{
	// if verbose mode, broadcast the line to admins
	if (module->m_bVerboseIdentd)
		CZNC::Get().Broadcast("znc-identd: " + sLine, true);
}


template<> void TModInfo<CNativeIdentdModule>(CModInfo& Info) {
	Info.SetWikiPage("znc-identd");
}

GLOBALMODULEDEFS(CNativeIdentdModule, "native identd for znc.")
