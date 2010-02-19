/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include <algorithm>

class CPerform : public CModule {
public:
	MODCONSTRUCTOR(CPerform) {}

	virtual ~CPerform() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		GetNV("Perform").Split("\n", m_vPerform, false);

		return true;
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0).AsLower();
		if (sCmdName == "add") {
			CString sPerf = sCommand.Token(1, true);

			if (sPerf.empty()) {
				PutModule("Usage: add <command>");
				return;
			}

			if (sPerf.Left(1) == "/")
				sPerf.LeftChomp();

			if (sPerf.Token(0).Equals("MSG")) {
				sPerf = "PRIVMSG " + sPerf.Token(1, true);
			}

			if ((sPerf.Token(0).Equals("PRIVMSG") ||
				sPerf.Token(0).Equals("NOTICE")) &&
				sPerf.Token(2).Left(1) != ":") {
				sPerf = sPerf.Token(0) + " " + sPerf.Token(1)
					+ " :" + sPerf.Token(2, true);
			}
			m_vPerform.push_back(sPerf);
			PutModule("Added!");
			Save();
		} else if (sCmdName == "del") {
			u_int iNum = sCommand.Token(1, true).ToUInt();
			if (iNum > m_vPerform.size() || iNum <= 0) {
				PutModule("Illegal # Requested");
				return;
			} else {
				m_vPerform.erase(m_vPerform.begin() + iNum - 1);
				PutModule("Command Erased.");
			}
			Save();
		} else if (sCmdName == "list") {
			int i = 1;
			CString sExpanded;
			for (VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++, i++) {
				sExpanded = GetUser()->ExpandString(*it);
				if (sExpanded != *it)
					PutModule(CString(i) + ": " + *it + " (" + sExpanded + ")");
				else
					PutModule(CString(i) + ": " + *it);
			}
			PutModule(" -- End of List");
		} else if (sCmdName == "execute") {
			OnIRCConnected();
			PutModule("perform commands sent");
		} else if (sCmdName == "swap") {
			u_int iNumA = sCommand.Token(1).ToUInt();
			u_int iNumB = sCommand.Token(2).ToUInt();

			if (iNumA > m_vPerform.size() || iNumA <= 0 || iNumB > m_vPerform.size() || iNumB <= 0) {
				PutModule("Illegal # Requested");
			} else {
				std::iter_swap(m_vPerform.begin() + (iNumA - 1), m_vPerform.begin() + (iNumB - 1));
				PutModule("Commands Swapped.");
				Save();
			}
		} else {
			PutModule("Commands: add <command>, del <nr>, list, execute, swap <nr> <nr>");
		}
	}

	virtual void OnIRCConnected() {
		for (VCString::iterator it = m_vPerform.begin();
			it != m_vPerform.end();  ++it) {
			PutIRC(GetUser()->ExpandString(*it));
		}
	}

private:
	bool Save() {
		CString sBuffer = "";

		for (VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it) {
			sBuffer += *it + "\n";
		}
		SetNV("Perform", sBuffer);

		return true;
	}

	VCString	m_vPerform;
};

MODULEDEFS(CPerform, "Adds perform capabilities")
