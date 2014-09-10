/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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
	void DoCommand(const CString& sCmd) {
		MCString msValues;
		msValues["password"] = GetNV("Password");
		PutIRC(CString::NamedFormat(GetNV(sCmd), msValues));
	}

	void DoNickCommand(const CString& sCmd, const CString& sNick) {
		MCString msValues;
		msValues["nickname"] = sNick;
		msValues["password"] = GetNV("Password");
		PutIRC(CString::NamedFormat(GetNV(sCmd), msValues));
	}
public:
	void SetPasswordCommand(const CString& sLine) {
		SetNV("Password", sLine.Token(1, true));
		PutModule("Password has been set");
	}

	void ViewPasswordCommand(const CString& sLine) {
		CString sPassword = GetNV("Password");

		if (sPassword.empty()) {
			PutModule("Password has not been set");
		} else {
			PutModule("Password is " + sPassword);
		}
	}

	void ClearPasswordCommand(const CString& sLine) {
		DelNV("Password");
		PutModule("Password has been cleared");
	}

	void SetNickServNameCommand(const CString& sLine) {
		SetNV("NickServName", sLine.Token(1, true));
		PutModule("NickServ name has been set");
	}

	void ViewNickServNameCommand(const CString& sLine) {
		CString sNickServName = GetNV("NickServName");

		if (sNickServName.empty()) {
			PutModule("NickServ name has not been set");
		} else {
			PutModule("NickServ name is " + sNickServName);
		}
	}

	void ClearNickServNameCommand(const CString& sLine) {
		DelNV("NickServName");
		PutModule("NickServ name has been cleared");
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

		PutModule("Command " + sCmd + " has been set");
	}

	void SetCommandsForDALnetCommand(const CString& sLine) {
		SetNV("IdentifyCmd", "PRIVMSG NickServ@services.dal.net :IDENTIFY {password}");
		SetNV("GhostCmd", "PRIVMSG NickServ@services.dal.net :GHOST {nickname} {password}");
		SetNV("RecoverCmd", "PRIVMSG NickServ@services.dal.net :RECOVER {nickname} {password}");
		SetNV("ReleaseCmd", "PRIVMSG NickServ@services.dal.net :RELEASE {nickname} {password}");
		SetNV("GroupCmd", "PRIVMSG NickServ@services.dal.net :GROUP {nickname} {password}");
		PutModule("Commands have been set for the DALnet network");
	}

	void SetCommandsForDefaultCommand(const CString& sLine) {
		SetNV("IdentifyCmd", "PRIVMSG NickServ :IDENTIFY {password}");
		SetNV("GhostCmd", "PRIVMSG NickServ :GHOST {nickname} {password}");
		SetNV("RecoverCmd", "PRIVMSG NickServ :RECOVER {nickname} {password}");
		SetNV("ReleaseCmd", "PRIVMSG NickServ :RELEASE {nickname} {password}");
		SetNV("GroupCmd", "PRIVMSG NickServ :GROUP {nickname} {password}");
		PutModule("Commands have been set for other networks");
	}

	void ViewCommandsCommand(const CString& sLine) {
		PutModule("IDENTIFY " + GetNV("IdentifyCmd"));
		PutModule("GHOST " + GetNV("GhostCmd"));
		PutModule("RECOVER " + GetNV("RecoverCmd"));
		PutModule("RELEASE " + GetNV("ReleaseCmd"));
		PutModule("GROUP " + GetNV("GroupCmd"));
	}

	void IdentifyCommand(const CString& sLine) {
		if (!sLine.Token(1).empty()) {
			PutModule("Syntax: identify");
		} else {
			DoCommand("IdentifyCmd");
		}
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

	MODCONSTRUCTOR(CNickServ) {
		AddHelpCommand();
		AddCommand("SetPassword", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetPasswordCommand),
			"password", "Set password");
		AddCommand("ViewPassword", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ViewPasswordCommand),
			"", "View password");
		AddCommand("ClearPassword", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ClearPasswordCommand),
			"", "Clear password");
		AddCommand("SetNickServName", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetNickServNameCommand),
			"nickname", "Set NickServ name; can be useful on networks like EpiKnet, where NickServ is named Themis");
		AddCommand("ViewNickServName", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ViewNickServNameCommand),
			"", "View NickServ name");
		AddCommand("ClearNickServName", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ClearNickServNameCommand),
			"", "Reset NickServ name to default, i.e., NickServ");
		AddCommand("SetCommandsForDALnet", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommandsForDALnetCommand),
			"", "Set command patterns for the DALnet network");
		AddCommand("SetCommandsForDefault", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommandsForDefaultCommand),
			"", "Set command patterns for other networks");
		AddCommand("SetCommand", static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommandCommand),
			"cmd new-pattern", "Set command pattern for specific commands");
		AddCommand("ViewCommands", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ViewCommandsCommand),
			"", "View patterns for lines, which are being sent to NickServ");
		AddCommand("Identify", static_cast<CModCommand::ModCmdFunc>(&CNickServ::IdentifyCommand),
			"", "Identify to NickServ");
		AddCommand("Ghost", static_cast<CModCommand::ModCmdFunc>(&CNickServ::GhostCommand),
			"nickname", "Disconnect an old user session, or somebody attempting to use your nickname without authorization.");
		AddCommand("Recover", static_cast<CModCommand::ModCmdFunc>(&CNickServ::RecoverCommand),
			"nickname", "Stop another user from using your nickname");
		AddCommand("Release", static_cast<CModCommand::ModCmdFunc>(&CNickServ::ReleaseCommand),
			"nickname", "Ask NickServ to stop holding on to your nickname");
		AddCommand("Group", static_cast<CModCommand::ModCmdFunc>(&CNickServ::GroupCommand),
			"nickname");
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
				 || sMessage.find("please choose a different nick") != CString::npos
				 || sMessage.find("If this is your nick, identify yourself with") != CString::npos
				 || sMessage.find("If this is your nick, type") != CString::npos
				 || sMessage.find("This is a registered nickname, please identify") != CString::npos
				 || sMessage.StripControls_n().find("type /NickServ IDENTIFY password") != CString::npos
				 || sMessage.StripControls_n().find("type /msg NickServ IDENTIFY password") != CString::npos)
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
