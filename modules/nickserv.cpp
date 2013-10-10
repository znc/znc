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

#include <znc/Modules.h>
#include <znc/User.h>

class CNickServ : public CModule {
	void DoNickCommand(const CString& sCmd, const CString& sNick) {
		MCString msValues;
		msValues["nickname"] = sNick;
		msValues["password"] = GetNV("Password");
		PutIRC(CString::NamedFormat(GetNV(sCmd), msValues));
	}
public:
	void SetCommand(const CString& sLine) {
		SetNV("Password", sLine.Token(1, true));
		PutModule("Password set");
	}

	void ClearCommand(const CString& sLine) {
		DelNV("Password");
	}

	void SetNSNameCommand(const CString& sLine) {
		SetNV("NickServName", sLine.Token(1, true));
		PutModule("NickServ name set");
	}

	void ClearNSNameCommand(const CString& sLine) {
		DelNV("NickServName");
	}

	void GhostCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: ghost <nickname>");
		} else {
			DoNickCommand("GhostCmd", sLine.Token(1));
		}
	}

	void RecoverCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: recover <nickname>");
		} else {
			DoNickCommand("RecoverCmd", sLine.Token(1));
		}
	}

	void ReleaseCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: release <nickname>");
		} else {
			DoNickCommand("ReleaseCmd", sLine.Token(1));
		}
	}

	void GroupCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: group <nickname>");
		} else {
			DoNickCommand("GroupCmd", sLine.Token(1));
		}
	}

	void ViewCommandsCommand(const CString& sLine) {
		PutModule("IDENTIFY " + GetNV("IdentifyCmd"));
		PutModule("GHOST " + GetNV("GhostCmd"));
		PutModule("RECOVER " + GetNV("RecoverCmd"));
		PutModule("RELEASE " + GetNV("ReleaseCmd"));
		PutModule("GROUP " + GetNV("GroupCmd"));
	}

	void SetCommandCommand(const CString& sLine) {
		CString sCmd = sLine.Token(1);
		CString sNewCmd = sLine.Token(2, true);
		if (sCmd.Equals("IDENTIFY")) {
			SetNV("IdentifyCmd", sNewCmd);
		} else if (sCmd.Equals("GHOST")) {
			SetNV("GhostCmd", sNewCmd);
		} else if (sCmd.Equals("RECOVER")) {
			SetNV("RecoverCmd", sNewCmd);
		} else if (sCmd.Equals("RELEASE")) {
			SetNV("ReleaseCmd", sNewCmd);
		} else if (sCmd.Equals("GROUP")) {
			SetNV("GroupCmd", sNewCmd);
		} else {
			PutModule("No such editable command. See ViewCommands for list.");
			return;
		}
		PutModule("Ok");
	}

	MODCONSTRUCTOR(CNickServ) {
		AddHelpCommand();
		AddCommand("Set", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommand),
			"password");
		AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ClearCommand),
			"", "Clear your nickserv password");
		AddCommand("SetNSName", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetNSNameCommand),
			"nickname", "Set NickServ name (Useful on networks like EpiKnet, where NickServ is named Themis)");
		AddCommand("ClearNSName", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ClearNSNameCommand),
			"", "Reset NickServ name to default (NickServ)");
		AddCommand("Ghost", static_cast<CModCommand::ModCmdFunc>(&CNickServ::GhostCommand),
			"nickname", "GHOST disconnects an old user session, or somebody attempting to use your nickname without authorization.");
		AddCommand("Recover", static_cast<CModCommand::ModCmdFunc>(&CNickServ::RecoverCommand),
			"nickname");
		AddCommand("Release", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ReleaseCommand),
			"nickname");
		AddCommand("Group", static_cast<CModCommand::ModCmdFunc>(&CNickServ::GroupCommand),
			"nickname");
		AddCommand("ViewCommands", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ViewCommandsCommand),
			"", "Show patterns for lines, which are being sent to NickServ");
		AddCommand("SetCommand", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommandCommand),
			"cmd new-pattern", "Set pattern for commands");
	}

	virtual ~CNickServ() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty() && sArgs != "<hidden>") {
			SetNV("Password", sArgs);
			SetArgs("<hidden>");
		}

		if (GetNV("IdentifyCmd").empty()) {
			SetNV("IdentifyCmd", "PRIVMSG NickServ :IDENTIFY {password}");
		}
		if (GetNV("GhostCmd").empty()) {
			SetNV("GhostCmd", "PRIVMSG NickServ :GHOST {nickname} {password}");
		}
		if (GetNV("RecoverCmd").empty()) {
			SetNV("RecoverCmd", "PRIVMSG NickServ :RECOVER {nickname} {password}");
		}
		if (GetNV("ReleaseCmd").empty()) {
			SetNV("ReleaseCmd", "PRIVMSG NickServ :RELEASE {nickname} {password}");
		}
		if (GetNV("GroupCmd").empty()) {
			SetNV("GroupCmd", "PRIVMSG NickServ :GROUP {nickname} {password}");
		}

		return true;
	}

	void HandleMessage(CNick& Nick, const CString& sMessage) {
		CString sNickServName = (!GetNV("NickServName").empty()) ? GetNV("NickServName") : "NickServ";
		if (!GetNV("Password").empty()
				&& Nick.NickEquals(sNickServName)
				&& (sMessage.find("msg") != CString::npos
				 || sMessage.find("authenticate") != CString::npos
				 || sMessage.find("choose a different nickname") != CString::npos
				 || sMessage.find("If this is your nick, identify yourself with") != CString::npos
				 || sMessage.find("If this is your nick, type") != CString::npos
				 || sMessage.StripControls_n().find("type /NickServ IDENTIFY password") != CString::npos)
				&& sMessage.AsUpper().find("IDENTIFY") != CString::npos
				&& sMessage.find("help") == CString::npos) {
			MCString msValues;
			msValues["password"] = GetNV("Password");
			PutIRC(CString::NamedFormat(GetNV("IdentifyCmd"), msValues));
		}
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		HandleMessage(Nick, sMessage);
		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
		HandleMessage(Nick, sMessage);
		return CONTINUE;
	}
};

template<> void TModInfo<CNickServ>(CModInfo& Info) {
	Info.SetWikiPage("nickserv");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Please enter your nickserv password.");
}

NETWORKMODULEDEFS(CNickServ, "Auths you with NickServ")
