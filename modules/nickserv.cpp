/*
 * Copyright (C) 2004-2007  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>
#include <map>
#include <vector>

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
		else
			m_sPass = sArgs;
		
		return true;
	}

	virtual void OnModCommand( const CString& sCommand )
	{
		CString sCmdName = sCommand.Token(0).AsLower();
		if(sCmdName == "set") {
			CString sPass = sCommand.Token(1, true);
			m_sPass = sPass;
			PutModule("Password set");
		} else if(sCmdName == "show") {
			if (m_sPass.empty())
				PutModule("No password set");
			else
				PutModule("Current password: " + m_sPass);
		} else if(sCmdName == "save") {
			SetNV("Password", m_sPass);
			PutModule("Saved!");
		} else {
			PutModule("Commands: set <password>, show, save");
		}
	}

	void HandleMessage(CNick& Nick, const CString& sMessage)
	{
		if (!m_sPass.empty()
				&& Nick.GetNick().CaseCmp("NickServ") == 0
				&& sMessage.find("msg") != CString::npos
				&& sMessage.find("IDENTIFY") != CString::npos
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
	CString	m_sPass;
};

MODULEDEFS(CNickServ, "Auths you with NickServ")
