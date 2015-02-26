/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#include <znc/znc.h>

using std::stringstream;

class CNotesMod : public CModule {
	bool bShowNotesOnLogin;

	void ListCommand(const CString &sLine) {
		ListNotes();
	}

	void AddNoteCommand(const CString &sLine) {
		CString sKey(sLine.Token(1));
		CString sValue(sLine.Token(2, true));

		if (!GetNV(sKey).empty()) {
			PutModule("That note already exists.  Use MOD <key> <note> to overwrite.");
		} else if (AddNote(sKey, sValue)) {
			PutModule("Added note [" + sKey + "]");
		} else {
			PutModule("Unable to add note [" + sKey + "]");
		}
	}

	void ModCommand(const CString &sLine) {
		CString sKey(sLine.Token(1));
		CString sValue(sLine.Token(2, true));

		if (AddNote(sKey, sValue)) {
			PutModule("Set note for [" + sKey + "]");
		} else {
			PutModule("Unable to add note [" + sKey + "]");
		}
	}

	void GetCommand(const CString &sLine) {
		CString sNote = GetNV(sLine.Token(1, true));

		if (sNote.empty()) {
			PutModule("This note doesn't exist.");
		} else {
			PutModule(sNote);
		}
	}

	void DelCommand(const CString &sLine) {
		CString sKey(sLine.Token(1));

		if (DelNote(sKey)) {
			PutModule("Deleted note [" + sKey + "]");
		} else {
			PutModule("Unable to delete note [" + sKey + "]");
		}
	}

public:
	MODCONSTRUCTOR(CNotesMod) {
		using std::placeholders::_1;
		AddHelpCommand();
		AddCommand("List",   static_cast<CModCommand::ModCmdFunc>(&CNotesMod::ListCommand));
		AddCommand("Add",    static_cast<CModCommand::ModCmdFunc>(&CNotesMod::AddNoteCommand),
			"<key> <note>");
		AddCommand("Del",    static_cast<CModCommand::ModCmdFunc>(&CNotesMod::DelCommand),
			"<key>",         "Delete a note");
		AddCommand("Mod", "<key> <note>", "Modify a note", std::bind(&CNotesMod::ModCommand, this, _1));
		AddCommand("Get", "<key>", "", [this](const CString& sLine){ GetCommand(sLine); });
	}

	virtual ~CNotesMod() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		bShowNotesOnLogin = !sArgs.Equals("-disableNotesOnLogin");
		return true;
	}

	CString GetWebMenuTitle() override { return "Notes"; }

	void OnClientLogin() override {
		if (bShowNotesOnLogin) {
			ListNotes(true);
		}
	}

	EModRet OnUserRaw(CString& sLine) override {
		if (sLine.Left(1) != "#") {
			return CONTINUE;
		}

		CString sKey;
		bool bOverwrite = false;

		if (sLine == "#?") {
			ListNotes(true);
			return HALT;
		} else if (sLine.Left(2) == "#-") {
			sKey = sLine.Token(0).LeftChomp_n(2);
			if (DelNote(sKey)) {
				PutModNotice("Deleted note [" + sKey + "]");
			} else {
				PutModNotice("Unable to delete note [" + sKey + "]");
			}
			return HALT;
		} else if (sLine.Left(2) == "#+") {
			sKey = sLine.Token(0).LeftChomp_n(2);
			bOverwrite = true;
		} else if (sLine.Left(1) == "#") {
			sKey = sLine.Token(0).LeftChomp_n(1);
		}

		CString sValue(sLine.Token(1, true));

		if (!sKey.empty()) {
			if (!bOverwrite && FindNV(sKey) != EndNV()) {
				PutModNotice("That note already exists.  Use /#+<key> <note> to overwrite.");
			} else if (AddNote(sKey, sValue)) {
				if (!bOverwrite) {
					PutModNotice("Added note [" + sKey + "]");
				} else {
					PutModNotice("Set note for [" + sKey + "]");
				}
			} else {
				PutModNotice("Unable to add note [" + sKey + "]");
			}
		}

		return HALT;
	}

	bool DelNote(const CString& sKey) {
		return DelNV(sKey);
	}

	bool AddNote(const CString& sKey, const CString& sNote) {
		if (sKey.empty()) {
			return false;
		}

		return SetNV(sKey, sNote);
	}

	void ListNotes(bool bNotice = false) {
		CClient* pClient = GetClient();

		if (pClient) {
			CTable Table;
			Table.AddColumn("Key");
			Table.AddColumn("Note");

			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				Table.AddRow();
				Table.SetCell("Key", it->first);
				Table.SetCell("Note", it->second);
			}

			if (Table.size()) {
				unsigned int idx = 0;
				CString sLine;
				while (Table.GetLine(idx++, sLine)) {
					if (bNotice) {
						pClient->PutModNotice(GetModName(), sLine);
					} else {
						pClient->PutModule(GetModName(), sLine);
					}
				}
			} else {
				if (bNotice) {
					PutModNotice("You have no entries.");
				} else {
					PutModule("You have no entries.");
				}
			}
		}
	}

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		if (sPageName == "index") {
			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				CTemplate& Row = Tmpl.AddRow("NotesLoop");

				Row["Key"] = it->first;
				Row["Note"] = it->second;
			}

			return true;
		} else if (sPageName == "delnote") {
			DelNote(WebSock.GetParam("key", false));
			WebSock.Redirect(GetWebPath());
			return true;
		} else if (sPageName == "addnote") {
			AddNote(WebSock.GetParam("key"), WebSock.GetParam("note"));
			WebSock.Redirect(GetWebPath());
			return true;
		}

		return false;
	}
};

template<> void TModInfo<CNotesMod>(CModInfo& Info) {
	Info.SetWikiPage("notes");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("This user module takes up to one arguments. It can be -disableNotesOnLogin not to show notes upon client login");
}

USERMODULEDEFS(CNotesMod, "Keep and replay notes")
