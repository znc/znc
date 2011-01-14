/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "HTTPSock.h"
#include "Server.h"
#include "Template.h"
#include "User.h"
#include "znc.h"
#include "WebModules.h"
#include <sstream>

using std::stringstream;

class CNotesMod : public CModule {
public:
	MODCONSTRUCTOR(CNotesMod) {
	}

	virtual ~CNotesMod() {
	}

	virtual bool OnLoad(const CString& sArgStr, CString& sMessage) {
		return true;
	}

	virtual CString GetWebMenuTitle() { return "Notes"; }

	virtual void OnClientLogin() {
		ListNotes(true);
	}

	virtual EModRet OnUserRaw(CString& sLine) {
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

	virtual void OnModCommand(const CString& sLine) {
		CString sCmd(sLine.Token(0));

		if (sLine.Equals("LIST")) {
			ListNotes();
		} else if (sCmd.Equals("ADD")) {
			CString sKey(sLine.Token(1));
			CString sValue(sLine.Token(2, true));

			if (!GetNV(sKey).empty()) {
				PutModule("That note already exists.  Use MOD <key> <note> to overwrite.");
			} else if (AddNote(sKey, sValue)) {
				PutModule("Added note [" + sKey + "]");
			} else {
				PutModule("Unable to add note [" + sKey + "]");
			}
		} else if (sCmd.Equals("MOD")) {
			CString sKey(sLine.Token(1));
			CString sValue(sLine.Token(2, true));

			if (AddNote(sKey, sValue)) {
				PutModule("Set note for [" + sKey + "]");
			} else {
				PutModule("Unable to add note [" + sKey + "]");
			}
		} else if (sCmd.Equals("DEL")) {
			CString sKey(sLine.Token(1));

			if (DelNote(sKey)) {
				PutModule("Deleted note [" + sKey + "]");
			} else {
				PutModule("Unable to delete note [" + sKey + "]");
			}
		} else {
			PutModule("Commands are: Help, List, Add <key> <note>, Del <key>, Mod <key> <note>");
		}
	}

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
				CTemplate& Row = Tmpl.AddRow("NotesLoop");

				Row["Key"] = it->first;
				Row["Note"] = it->second;
			}

			return true;
		} else if (sPageName == "delnote") {
			DelNote(WebSock.GetParam("key", false));
			WebSock.Redirect("/mods/notes/");
			return true;
		} else if (sPageName == "addnote") {
			AddNote(WebSock.GetParam("key"), WebSock.GetParam("note"));
			WebSock.Redirect("/mods/notes/");
			return true;
		}

		return false;
	}
};

MODULEDEFS(CNotesMod, "Keep and replay notes")
