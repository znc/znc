/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
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

	CString ParsePerform(const CString& sArg) const {
		CString sPerf = sArg;

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

		return sPerf;
	}

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

			m_vPerform.push_back(ParsePerform(sPerf));
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
			for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++, i++) {
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
		for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it) {
			PutIRC(GetUser()->ExpandString(*it));
		}
	}

	virtual CString GetWebMenuTitle() { return "Perform"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName != "index") {
			// only accept requests to /mods/perform/
			return false;
		}

		if (WebSock.IsPost()) {
			VCString vsPerf;
			WebSock.GetRawParam("perform", true).Split("\n", vsPerf, false);
			m_vPerform.clear();

			for (VCString::const_iterator it = vsPerf.begin(); it != vsPerf.end(); ++it)
				m_vPerform.push_back(ParsePerform(*it));

			Save();
		}

		for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it) {
			CTemplate& Row = Tmpl.AddRow("PerformLoop");
			Row["Perform"] = *it;
		}

		return true;
	}

private:
	void Save() {
		CString sBuffer = "";

		for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it) {
			sBuffer += *it + "\n";
		}
		SetNV("Perform", sBuffer);
	}

	VCString m_vPerform;
};

MODULEDEFS(CPerform, "Keeps a list of commands to be executed when ZNC connects to IRC.")
