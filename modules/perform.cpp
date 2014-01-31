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

#include <znc/IRCNetwork.h>

class CPerform : public CModule {
	void Add(const CString& sCommand) {
		CString sPerf = sCommand.Token(1, true);

		if (sPerf.empty()) {
			PutModule("Usage: add <command>");
			return;
		}

		m_vPerform.push_back(ParsePerform(sPerf));
		PutModule("Added!");
		Save();
	}

	void Del(const CString& sCommand) {
		u_int iNum = sCommand.Token(1, true).ToUInt();

		if (iNum > m_vPerform.size() || iNum <= 0) {
			PutModule("Illegal # Requested");
			return;
		} else {
			m_vPerform.erase(m_vPerform.begin() + iNum - 1);
			PutModule("Command Erased.");
		}
		Save();
	}

	void List(const CString& sCommand) {
		CTable Table;
		unsigned int index = 1;

		Table.AddColumn("Id");
		Table.AddColumn("Perform");
		Table.AddColumn("Expanded");

		for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it, index++) {
			Table.AddRow();
			Table.SetCell("Id", CString(index));
			Table.SetCell("Perform", *it);

			CString sExpanded = ExpandString(*it);

			if (sExpanded != *it) {
				Table.SetCell("Expanded", sExpanded);
			}
		}

		if (PutModule(Table) == 0) {
			PutModule("No commands in your perform list.");
		}
	}

	void Execute(const CString& sCommand) {
		OnIRCConnected();
		PutModule("perform commands sent");
	}

	void Swap(const CString& sCommand) {
		u_int iNumA = sCommand.Token(1).ToUInt();
		u_int iNumB = sCommand.Token(2).ToUInt();

		if (iNumA > m_vPerform.size() || iNumA <= 0 || iNumB > m_vPerform.size() || iNumB <= 0) {
			PutModule("Illegal # Requested");
		} else {
			std::iter_swap(m_vPerform.begin() + (iNumA - 1), m_vPerform.begin() + (iNumB - 1));
			PutModule("Commands Swapped.");
			Save();
		}
	}

public:
	MODCONSTRUCTOR(CPerform) {
		AddHelpCommand();
		AddCommand("Add",     static_cast<CModCommand::ModCmdFunc>(&CPerform::Add),
			"<command>");
		AddCommand("Del",     static_cast<CModCommand::ModCmdFunc>(&CPerform::Del),
			"<nr>");
		AddCommand("List",    static_cast<CModCommand::ModCmdFunc>(&CPerform::List));
		AddCommand("Execute", static_cast<CModCommand::ModCmdFunc>(&CPerform::Execute));
		AddCommand("Swap",    static_cast<CModCommand::ModCmdFunc>(&CPerform::Swap),
			"<nr> <nr>");
	}

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

	virtual void OnIRCConnected() {
		for (VCString::const_iterator it = m_vPerform.begin(); it != m_vPerform.end(); ++it) {
			PutIRC(ExpandString(*it));
		}
	}

	virtual CString GetWebMenuTitle() { return "Perform"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName != "index") {
			// only accept requests to index
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

template<> void TModInfo<CPerform>(CModInfo& Info) {
	Info.AddType(CModInfo::UserModule);
	Info.SetWikiPage("perform");
}

NETWORKMODULEDEFS(CPerform, "Keeps a list of commands to be executed when ZNC connects to IRC.")
