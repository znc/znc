/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"

class CNickServ : public CModule
{
public:
	MODCONSTRUCTOR(CNickServ)
	{
	}

	virtual ~CNickServ()
	{
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		if (sArgs.empty())
			m_sPass = GetNV("Password");
		else {
			m_sPass = sArgs;
			SetNV("Password", m_sPass);
			SetArgs("");
		}

		return true;
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString sCmdName = sCommand.Token(0).AsLower();
		if (sCmdName == "set") {
			CString sPass = sCommand.Token(1, true);
			m_sPass = sPass;
			SetNV("Password", m_sPass);
			PutModule("Password set");
		} else if (sCmdName == "clear") {
			m_sPass = "";
			DelNV("Password");
		} else {
			PutModule("Commands: set <password>, clear");
		}
	}

	void HandleMessage(CNick& Nick, const CString& sMessage)
	{
		if (!m_sPass.empty()
				&& Nick.GetNick().Equals("NickServ")
				&& (sMessage.find("msg") != CString::npos
				 || sMessage.find("authenticate") != CString::npos)
				&& sMessage.AsUpper().find("IDENTIFY") != CString::npos
				&& sMessage.find("help") == CString::npos) {
			PutIRC("PRIVMSG NickServ :IDENTIFY " + m_sPass);
		}
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage)
	{
		HandleMessage(Nick, sMessage);
		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage)
	{
		HandleMessage(Nick, sMessage);
		return CONTINUE;
	}

private:
	CString m_sPass;
};

template<> void TModInfo<CNickServ>(CModInfo& Info) {
	Info.SetWikiPage("nickserv");
}

MODULEDEFS(CNickServ, "Auths you with NickServ")
