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
			PutIRC("PRIVMSG NickServ :GHOST " + sLine.Token(1) + " " + GetNV("Password"));
		}
	}

	void RecoverCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: recover <nickname>");
		} else {
			PutIRC("PRIVMSG NickServ :RECOVER " + sLine.Token(1) + " " + GetNV("Password"));
		}
	}

	void ReleaseCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: release <nickname>");
		} else {
			PutIRC("PRIVMSG NickServ :RELEASE " + sLine.Token(1) + " " + GetNV("Password"));
		}
	}

	void GroupCommand(const CString& sLine) {
		if (sLine.Token(1).empty()) {
			PutModule("Syntax: group <nickname>");
		} else {
			PutIRC("PRIVMSG NickServ :GROUP " + sLine.Token(1) + " " + GetNV("Password"));
		}
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

	}

	virtual ~CNickServ() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty()) {
			SetNV("Password", sArgs);
			SetArgs("");
		}

		return true;
	}

	void HandleMessage(CNick& Nick, const CString& sMessage) {
		if (!GetNV("Password").empty()
				&& Nick.GetNick().Equals("NickServ")
				&& (sMessage.find("msg") != CString::npos
				 || sMessage.find("authenticate") != CString::npos)
				&& sMessage.AsUpper().find("IDENTIFY") != CString::npos
				&& sMessage.find("help") == CString::npos) {
			PutIRC("PRIVMSG NickServ :IDENTIFY " + GetNV("Password"));
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
