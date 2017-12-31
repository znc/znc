/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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
    bool m_bShowNotesOnLogin{};

    void ListCommand(const CString& sLine) { ListNotes(); }

    void AddNoteCommand(const CString& sLine) {
        CString sKey(sLine.Token(1));
        CString sValue(sLine.Token(2, true));

        if (!GetNV(sKey).empty()) {
            PutModule(
                t_s("That note already exists.  Use MOD <key> <note> to "
                    "overwrite."));
        } else if (AddNote(sKey, sValue)) {
            PutModule(t_f("Added note {1}")(sKey));
        } else {
            PutModule(t_f("Unable to add note {1}")(sKey));
        }
    }

    void ModCommand(const CString& sLine) {
        CString sKey(sLine.Token(1));
        CString sValue(sLine.Token(2, true));

        if (AddNote(sKey, sValue)) {
            PutModule(t_f("Set note for {1}")(sKey));
        } else {
            PutModule(t_f("Unable to add note {1}")(sKey));
        }
    }

    void GetCommand(const CString& sLine) {
        CString sNote = GetNV(sLine.Token(1, true));

        if (sNote.empty()) {
            PutModule(t_s("This note doesn't exist."));
        } else {
            PutModule(sNote);
        }
    }

    void DelCommand(const CString& sLine) {
        CString sKey(sLine.Token(1));

        if (DelNote(sKey)) {
            PutModule(t_f("Deleted note {1}")(sKey));
        } else {
            PutModule(t_f("Unable to delete note {1}")(sKey));
        }
    }

  public:
    MODCONSTRUCTOR(CNotesMod) {
        AddHelpCommand();
        AddCommand("List", "", t_d("List notes"),
                   [=](const CString& sLine) { ListCommand(sLine); });
        AddCommand("Add", t_d("<key> <note>"), t_d("Add a note"),
                   [=](const CString& sLine) { AddNoteCommand(sLine); });
        AddCommand("Del", t_d("<key>"), t_d("Delete a note"),
                   [=](const CString& sLine) { DelCommand(sLine); });
        AddCommand("Mod", t_d("<key> <note>"), t_d("Modify a note"),
                   [=](const CString& sLine) { ModCommand(sLine); });
        AddCommand("Get", t_d("<key>"), "",
                   [=](const CString& sLine) { GetCommand(sLine); });
    }

    ~CNotesMod() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_bShowNotesOnLogin = !sArgs.Equals("-disableNotesOnLogin");
        return true;
    }

    CString GetWebMenuTitle() override { return t_s("Notes"); }

    void OnClientLogin() override {
        if (m_bShowNotesOnLogin) {
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
                PutModNotice(t_f("Deleted note {1}")(sKey));
            } else {
                PutModNotice(t_f("Unable to delete note {1}")(sKey));
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
                PutModNotice(
                    t_s("That note already exists.  Use /#+<key> <note> to "
                        "overwrite."));
            } else if (AddNote(sKey, sValue)) {
                if (!bOverwrite) {
                    PutModNotice(t_f("Added note {1}")(sKey));
                } else {
                    PutModNotice(t_f("Set note for {1}")(sKey));
                }
            } else {
                PutModNotice(t_f("Unable to add note {1}")(sKey));
            }
        }

        return HALT;
    }

    bool DelNote(const CString& sKey) { return DelNV(sKey); }

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
            Table.AddColumn(t_s("Key"));
            Table.AddColumn(t_s("Note"));

            for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
                Table.AddRow();
                Table.SetCell(t_s("Key"), it->first);
                Table.SetCell(t_s("Note"), it->second);
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
                    PutModNotice(t_s("You have no entries."));
                } else {
                    PutModule(t_s("You have no entries."));
                }
            }
        }
    }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
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

template <>
void TModInfo<CNotesMod>(CModInfo& Info) {
    Info.SetWikiPage("notes");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("This user module takes up to one arguments. It can be "
                 "-disableNotesOnLogin not to show notes upon client login"));
}

USERMODULEDEFS(CNotesMod, t_s("Keep and replay notes"))
