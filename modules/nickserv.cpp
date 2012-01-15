/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
		PutModule("IDENTIFY=" + GetNV("IdentifyCmd"));
		PutModule("GHOST=" + GetNV("GhostCmd"));
		PutModule("RECOVER=" + GetNV("RecoverCmd"));
		PutModule("RELEASE=" + GetNV("ReleaseCmd"));
		PutModule("GROUP=" + GetNV("GroupCmd"));
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
		if (!sArgs.empty()) {
			SetNV("Password", sArgs);
			SetArgs("");
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
		if (!GetNV("Password").empty()
				&& Nick.GetNick().Equals("NickServ")
				&& (sMessage.find("msg") != CString::npos
				 || sMessage.find("authenticate") != CString::npos
				 || sMessage.find("choose a different nickname") != CString::npos
				 || sMessage.find("If this is your nick, type") != CString::npos
				 || sMessage.find("type /NickServ IDENTIFY password") != CString::npos)
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
}

NETWORKMODULEDEFS(CNickServ, "Auths you with NickServ")
