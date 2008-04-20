/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"

class CPerform : public CModule
{
public:
	MODCONSTRUCTOR(CPerform)
	{
	}

	virtual ~CPerform()
	{
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		GetNV("Perform").Split("\n", m_vPerform, false);

		return true;
	}

	virtual void OnModCommand(const CString& sCommand)
	{
		CString sCmdName = sCommand.Token(0).AsLower();
		if (sCmdName == "add")
		{
			CString sPerf = sCommand.Token(1, true);
			if (sPerf.Left(1) == "/")
				sPerf.LeftChomp();

			if (sPerf.Token(0).CaseCmp("MSG") == 0) {
				sPerf = "PRIVMSG " + sPerf.Token(1, true);
			}

			if ((sPerf.Token(0).CaseCmp("PRIVMSG") == 0 ||
				sPerf.Token(0).CaseCmp("NOTICE") == 0) &&
				sPerf.Token(2).Left(1) != ":") {
				sPerf = sPerf.Token(0) + " " + sPerf.Token(1)
					+ " :" + sPerf.Token(2, true);
			}
			m_vPerform.push_back(sPerf);
			PutModule("Added!");
			Save();
		} else if (sCmdName == "del")
		{
			u_int iNum = atoi(sCommand.Token(1, true).c_str());
			if (iNum > m_vPerform.size() || iNum <= 0)
			{
				PutModule("Illegal # Requested");
				return;
			}
			else
			{
				m_vPerform.erase(m_vPerform.begin() + iNum - 1);
				PutModule("Command Erased.");
			}
			Save();
		} else if (sCmdName == "list")
		{
			int i = 1;
			CString sExpanded;
			for (VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++, i++)
			{
				sExpanded = GetUser()->ExpandString(*it);
				if (sExpanded != *it)
					PutModule(CString(i) + ": " + *it + " (" + sExpanded + ")");
				else
					PutModule(CString(i) + ": " + *it);
			}
			PutModule(" -- End of List");
		}else
		{
			PutModule("Commands: add <command>, del <nr>, list");
		}
	}

	virtual void OnIRCConnected()
	{
		for (VCString::iterator it = m_vPerform.begin();
			it != m_vPerform.end();  it++)
		{
			PutIRC(GetUser()->ExpandString(*it));
		}
	}

private:
	bool Save()
	{
		CString sBuffer = "";

		for (VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++)
		{
			sBuffer += *it + "\n";
		}
		SetNV("Perform", sBuffer);

		return true;
	}

	VCString	m_vPerform;
};

MODULEDEFS(CPerform, "Adds perform capabilities")
