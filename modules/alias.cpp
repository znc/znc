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

#include <znc/Client.h>
#include <znc/Chan.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>

#include <sstream>
#include <stdexcept>

using std::vector;
using std::stringstream;

class CAlias {
  private:
    CModule* parent;
    CString name;
    VCString alias_cmds;

  public:
    // getters/setters
    const CString& GetName() const { return name; }

    // name should be a single, all uppercase word
    void SetName(const CString& newname) {
        name = newname.Token(0, false, " ");
        name.MakeUpper();
    }

    // combined getter/setter for command list
    VCString& AliasCmds() { return alias_cmds; }

    // check registry if alias exists
    static bool AliasExists(CModule* module, CString alias_name) {
        alias_name = alias_name.Token(0, false, " ").MakeUpper();
        return (module->FindNV(alias_name) != module->EndNV());
    }

    // populate alias from stored settings in registry, or return false if none
    // exists
    static bool AliasGet(CAlias& alias, CModule* module, CString line) {
        line = line.Token(0, false, " ").MakeUpper();
        MCString::iterator i = module->FindNV(line);
        if (i == module->EndNV()) return false;
        alias.parent = module;
        alias.name = line;
        i->second.Split("\n", alias.alias_cmds, false);
        return true;
    }

    // constructors
    CAlias() : parent(nullptr) {}
    CAlias(CModule* new_parent, const CString& new_name) : parent(new_parent) {
        SetName(new_name);
    }

    // produce a command string from this alias' command list
    CString GetCommands() const {
        return CString("\n").Join(alias_cmds.begin(), alias_cmds.end());
    }

    // write this alias to registry
    void Commit() const {
        if (!parent) return;
        parent->SetNV(name, GetCommands());
    }

    // delete this alias from regisrty
    void Delete() const {
        if (!parent) return;
        parent->DelNV(name);
    }

  private:
    // this function helps imprint out.  it checks if there is a substitution
    // token at 'caret' in 'alias_data'
    // and if it finds one, pulls the appropriate token out of 'line' and
    // appends it to 'output', and updates 'caret'.
    // 'skip' is updated based on the logic that we should skip the % at the
    // caret if we fail to parse the token.
    void ParseToken(const CString& alias_data, const CString& line,
                    CString& output, size_t& caret, size_t& skip) const {
        bool optional = false;
        bool subsequent = false;
        size_t index = caret + 1;
        int token = -1;

        skip = 1;

        // try to read optional flag
        if (alias_data.length() > index && alias_data[index] == '?') {
            optional = true;
            ++index;
        }
        // try to read integer
        if (alias_data.length() > index &&
            CString(alias_data.substr(index)).Convert(&token)) {
            // skip any numeric digits in string (supposed to fail if
            // whitespace precedes integer)
            while (alias_data.length() > index && alias_data[index] >= '0' &&
                   alias_data[index] <= '9')
                ++index;
        } else {
            // token was malformed. leave caret unchanged, and flag first
            // character for skipping
            return;
        }
        // try to read subsequent flag
        if (alias_data.length() > index && alias_data[index] == '+') {
            subsequent = true;
            ++index;
        }
        // try to read end-of-substitution marker
        if (alias_data.length() > index && alias_data[index] == '%') {
            ++index;
        }
        else
            return;

        // if we get here, we're definitely dealing with a token, so get the
        // token's value
        CString stok = line.Token(token, subsequent, " ");

        if (stok.empty() && !optional)
            // blow up if token is required and also empty
            throw std::invalid_argument(
                parent->t_f("missing required parameter: {1}")(CString(token)));
        // write token value to output
        output.append(stok);

        // since we're moving the cursor after the end of the token, skip no
        // characters
        skip = 0;
        // advance the cursor forward by the size of the token
        caret = index;
    }

  public:
    // read an IRC line and do token substitution
    // throws an exception if a required parameter is missing, and might also
    // throw if you manage to make it bork
    CString Imprint(CString line) const {
        CString output;
        CString alias_data = GetCommands();
        alias_data = parent->ExpandString(alias_data);
        size_t lastfound = 0, skip = 0;

        // it would be very inefficient to attempt to blindly replace every
        // possible token
        // so let's just parse the line and replace when we find them
        // token syntax:
        // %[?]n[+]%
        // adding ? makes the substitution optional (you'll get "" if there are
        // insufficient tokens, otherwise the alias will fail)
        // adding + makes the substitution contain all tokens from the nth to
        // the end of the line
        while (true) {
            // if (found >= (int) alias_data.length()) break;
            // ^ shouldn't be possible.
            size_t found = alias_data.find("%", lastfound + skip);
            // if we found nothing, break
            if (found == CString::npos) break;
            // capture everything between the last stopping point and here
            output.append(alias_data.substr(lastfound, found - lastfound));
            // attempt to read a token, updates indices based on
            // success/failure
            ParseToken(alias_data, line, output, found, skip);
            lastfound = found;
        }

        // append from the final
        output += alias_data.substr(lastfound);
        return output;
    }
};

class CAliasMod : public CModule {
  private:
    bool sending_lines;

  public:
    void CreateCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        if (!CAlias::AliasExists(this, name)) {
            CAlias na(this, name);
            na.Commit();
            PutModule(t_f("Created alias: {1}")(na.GetName()));
        } else
            PutModule(t_s("Alias already exists."));
    }

    void DeleteCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias delete_alias;
        if (CAlias::AliasGet(delete_alias, this, name)) {
            PutModule(t_f("Deleted alias: {1}")(delete_alias.GetName()));
            delete_alias.Delete();
        } else
            PutModule(t_s("Alias does not exist."));
    }

    void AddCmd(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias add_alias;
        if (CAlias::AliasGet(add_alias, this, name)) {
            add_alias.AliasCmds().push_back(sLine.Token(2, true, " "));
            add_alias.Commit();
            PutModule(t_s("Modified alias."));
        } else
            PutModule(t_s("Alias does not exist."));
    }

    void InsertCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias insert_alias;
        int index;
        if (CAlias::AliasGet(insert_alias, this, name)) {
            // if Convert succeeds, then i has been successfully read from user
            // input
            if (!sLine.Token(2, false, " ").Convert(&index) || index < 0 ||
                index > (int)insert_alias.AliasCmds().size()) {
                PutModule(t_s("Invalid index."));
                return;
            }

            insert_alias.AliasCmds().insert(
                insert_alias.AliasCmds().begin() + index,
                sLine.Token(3, true, " "));
            insert_alias.Commit();
            PutModule(t_s("Modified alias."));
        } else
            PutModule(t_s("Alias does not exist."));
    }

    void RemoveCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias remove_alias;
        int index;
        if (CAlias::AliasGet(remove_alias, this, name)) {
            if (!sLine.Token(2, false, " ").Convert(&index) || index < 0 ||
                index > (int)remove_alias.AliasCmds().size() - 1) {
                PutModule(t_s("Invalid index."));
                return;
            }

            remove_alias.AliasCmds().erase(remove_alias.AliasCmds().begin() +
                                           index);
            remove_alias.Commit();
            PutModule(t_s("Modified alias."));
        } else
            PutModule(t_s("Alias does not exist."));
    }

    void ClearCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias clear_alias;
        if (CAlias::AliasGet(clear_alias, this, name)) {
            clear_alias.AliasCmds().clear();
            clear_alias.Commit();
            PutModule(t_s("Modified alias."));
        } else
            PutModule(t_s("Alias does not exist."));
    }

    void ListCommand(const CString& sLine) {
        MCString::iterator i = BeginNV();
        if (i == EndNV()) {
            PutModule(t_s("There are no aliases."));
            return;
        }
        VCString vsAliases;
        for (; i != EndNV(); ++i) {
            vsAliases.push_back(i->first);
        }
        PutModule(t_f("The following aliases exist: {1}")(
            CString(t_s(", ", "list|separator"))
                .Join(vsAliases.begin(), vsAliases.end())));
    }

    void DumpCommand(const CString& sLine) {
        MCString::iterator i = BeginNV();

        if (i == EndNV()) {
            PutModule(t_s("There are no aliases."));
            return;
        }

        PutModule("-----------------------");
        PutModule("/ZNC-CLEAR-ALL-ALIASES!");
        for (; i != EndNV(); ++i) {
            PutModule("/msg " + GetModNick() + " Create " + i->first);
            if (!i->second.empty()) {
                VCString it;
                uint idx;
                i->second.Split("\n", it);

                for (idx = 0; idx < it.size(); ++idx) {
                    PutModule("/msg " + GetModNick() + " Add " + i->first +
                              " " + it[idx]);
                }
            }
        }
        PutModule("-----------------------");
    }

    void InfoCommand(const CString& sLine) {
        CString name = sLine.Token(1, false, " ");
        CAlias info_alias;
        if (CAlias::AliasGet(info_alias, this, name)) {
            PutModule(t_f("Actions for alias {1}:")(info_alias.GetName()));
            for (size_t i = 0; i < info_alias.AliasCmds().size(); ++i) {
                CString num(i);
                CString padding(4 - (num.length() > 3 ? 3 : num.length()), ' ');
                PutModule(num + padding + info_alias.AliasCmds()[i]);
            }
            PutModule(
                t_f("End of actions for alias {1}.")(info_alias.GetName()));
        } else
            PutModule(t_s("Alias does not exist."));
    }

    MODCONSTRUCTOR(CAliasMod), sending_lines(false) {
        AddHelpCommand();
        AddCommand("Create", t_d("<name>"),
                   t_d("Creates a new, blank alias called name."),
                   [=](const CString& sLine) { CreateCommand(sLine); });
        AddCommand("Delete", t_d("<name>"), t_d("Deletes an existing alias."),
                   [=](const CString& sLine) { DeleteCommand(sLine); });
        AddCommand("Add", t_d("<name> <action ...>"),
                   t_d("Adds a line to an existing alias."),
                   [=](const CString& sLine) { AddCmd(sLine); });
        AddCommand("Insert", t_d("<name> <pos> <action ...>"),
                   t_d("Inserts a line into an existing alias."),
                   [=](const CString& sLine) { InsertCommand(sLine); });
        AddCommand("Remove", t_d("<name> <pos>"),
                   t_d("Removes a line from an existing alias."),
                   [=](const CString& sLine) { RemoveCommand(sLine); });
        AddCommand("Clear", t_d("<name>"),
                   t_d("Removes all lines from an existing alias."),
                   [=](const CString& sLine) { ClearCommand(sLine); });
        AddCommand("List", "", t_d("Lists all aliases by name."),
                   [=](const CString& sLine) { ListCommand(sLine); });
        AddCommand("Info", t_d("<name>"),
                   t_d("Reports the actions performed by an alias."),
                   [=](const CString& sLine) { InfoCommand(sLine); });
        AddCommand(
            "Dump", "",
            t_d("Generate a list of commands to copy your alias config."),
            [=](const CString& sLine) { DumpCommand(sLine); });
    }

    EModRet OnUserRaw(CString& sLine) override {
        CAlias current_alias;

        if (sending_lines) return CONTINUE;

        try {
            if (sLine.Equals("ZNC-CLEAR-ALL-ALIASES!")) {
                ListCommand("");
                PutModule(t_s("Clearing all of them!"));
                ClearNV();
                return HALT;
            } else if (CAlias::AliasGet(current_alias, this, sLine)) {
                VCString rawLines;
                current_alias.Imprint(sLine).Split("\n", rawLines, false);
                sending_lines = true;

                for (size_t i = 0; i < rawLines.size(); ++i) {
                    m_pClient->ReadLine(rawLines[i]);
                }

                sending_lines = false;
                return HALT;
            }
        } catch (std::exception& e) {
            CString my_nick =
                (GetNetwork() == nullptr ? "" : GetNetwork()->GetCurNick());
            if (my_nick.empty()) my_nick = "*";
            PutUser(CString(":znc.in 461 " + my_nick + " " +
                            current_alias.GetName() + " :ZNC alias error: ") +
                    e.what());
            return HALTCORE;
        }

        return CONTINUE;
    }
};

template <>
void TModInfo<CAliasMod>(CModInfo& Info) {
    Info.SetWikiPage("alias");
    Info.AddType(CModInfo::NetworkModule);
}

USERMODULEDEFS(CAliasMod, t_s("Provides bouncer-side command alias support."))
