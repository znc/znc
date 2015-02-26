/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

class CAlias
{
private:
	CModule *parent;
	CString name;
	VCString alias_cmds;

public:
	// getters/setters
	const CString &GetName() const
	{
		return name;
	}

	// name should be a single, all uppercase word
	void SetName(const CString &newname)
	{
		name = newname.Token(0, false, " ");
		name.MakeUpper();
	}

	// combined getter/setter for command list
	VCString &AliasCmds()
	{
		return alias_cmds;
	}

	// check registry if alias exists
	static bool AliasExists(CModule *module, CString alias_name)
	{
		alias_name = alias_name.Token(0, false, " ").MakeUpper();
		return (module->FindNV(alias_name) != module->EndNV());
	}

	// populate alias from stored settings in registry, or return false if none exists
	static bool AliasGet(CAlias &alias, CModule *module, CString line)
	{
		line = line.Token(0, false, " ").MakeUpper();
		MCString::iterator i = module->FindNV(line);
		if (i == module->EndNV()) return false;
		alias.parent = module;
		alias.name = line;
		i->second.Split("\n", alias.alias_cmds, false);
		return true;
	}

	// constructors
	CAlias() : parent(NULL) {}
	CAlias(CModule *new_parent, const CString &new_name) : parent(new_parent) { SetName(new_name); }

	// produce a command string from this alias' command list
	CString GetCommands() const
	{
		return CString("\n").Join(alias_cmds.begin(), alias_cmds.end());
	}

	// write this alias to registry
	void Commit() const
	{
		if (!parent) return;
		parent->SetNV(name, GetCommands());
	}

	// delete this alias from regisrty
	void Delete() const
	{
		if (!parent) return;
		parent->DelNV(name);
	}

private:
	// this function helps imprint out.  it checks if there is a substitution token at 'caret' in 'alias_data'
	// and if it finds one, pulls the appropriate token out of 'line' and appends it to 'output', and updates 'caret'.
	// 'skip' is updated based on the logic that we should skip the % at the caret if we fail to parse the token.
	static void ParseToken(const CString &alias_data, const CString &line, CString &output, size_t &caret, size_t &skip)
	{
		bool optional = false;
		bool subsequent = false;
		size_t index = caret + 1;
		int token = -1;

		skip = 1;

		if (alias_data.length() > index && alias_data[index] == '?') { optional = true; ++index; }			// try to read optional flag
		if (alias_data.length() > index && CString(alias_data.substr(index)).Convert(&token))				// try to read integer
		{
			while(alias_data.length() > index && alias_data[index] >= '0' && alias_data[index] <= '9') ++index;	// skip any numeric digits in string
		}														// (supposed to fail if whitespace precedes integer)
		else return;													// token was malformed. leave caret unchanged, and flag first character for skipping
		if (alias_data.length() > index && alias_data[index] == '+') { subsequent = true; ++index; }			// try to read subsequent flag
		if (alias_data.length() > index && alias_data[index] == '%') { ++index; }					// try to read end-of-substitution marker
		else return;

		CString stok = line.Token(token, subsequent, " ");								// if we get here, we're definitely dealing with a token, so get the token's value
		if (stok.empty() && !optional)
			throw std::invalid_argument(CString("missing required parameter: ") + CString(token));			// blow up if token is required and also empty
		output.append(stok);												// write token value to output

		skip = 0;													// since we're moving the cursor after the end of the token, skip no characters
		caret = index;													// advance the cursor forward by the size of the token
	}

public:
	// read an IRC line and do token substitution
	// throws an exception if a required parameter is missing, and might also throw if you manage to make it bork
	CString Imprint(CString line) const
	{
		CString output;
		CString alias_data = GetCommands();
		alias_data = parent->ExpandString(alias_data);
		size_t lastfound = 0, skip = 0;

		// it would be very inefficient to attempt to blindly replace every possible token
		// so let's just parse the line and replace when we find them
		// token syntax:
		// %[?]n[+]%
		// adding ? makes the substitution optional (you'll get "" if there are insufficient tokens, otherwise the alias will fail)
		// adding + makes the substitution contain all tokens from the nth to the end of the line
		while (true)
		{
			// if (found >= (int) alias_data.length()) break; 		// shouldn't be possible.
			size_t found = alias_data.find("%", lastfound + skip);
			if (found == CString::npos) break; 				// if we found nothing, break
			output.append(alias_data.substr(lastfound, found - lastfound));	// capture everything between the last stopping point and here
			ParseToken(alias_data, line, output, found, skip);				// attempt to read a token, updates indices based on success/failure
			lastfound = found;
		}

		output += alias_data.substr(lastfound); // append from the final 
		return output;
	}
};

class CAliasMod : public CModule {
private:
	bool sending_lines;

public:
	void CreateCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		if (!CAlias::AliasExists(this, name))
		{
			CAlias na(this, name);
			na.Commit();
			PutModule("Created alias: " + na.GetName());
		}
		else PutModule("Alias already exists.");
	}

	void DeleteCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias delete_alias;
		if (CAlias::AliasGet(delete_alias, this, name))
		{
			PutModule("Deleted alias: " + delete_alias.GetName());
			delete_alias.Delete();
		}
		else PutModule("Alias does not exist.");
	}

	void AddCmd(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias add_alias;
		if (CAlias::AliasGet(add_alias, this, name))
		{
			add_alias.AliasCmds().push_back(sLine.Token(2, true, " "));
			add_alias.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void InsertCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias insert_alias;
		int index;
		if (CAlias::AliasGet(insert_alias, this, name))
		{
			// if Convert succeeds, then i has been successfully read from user input
			if (!sLine.Token(2, false, " ").Convert(&index) || index < 0 || index > (int) insert_alias.AliasCmds().size())
			{
				PutModule("Invalid index.");
				return;
			}

			insert_alias.AliasCmds().insert(insert_alias.AliasCmds().begin() + index, sLine.Token(3, true, " "));
			insert_alias.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void RemoveCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias remove_alias;
		int index;
		if (CAlias::AliasGet(remove_alias, this, name))
		{
			if (!sLine.Token(2, false, " ").Convert(&index) || index < 0 || index > (int) remove_alias.AliasCmds().size() - 1)
			{
				PutModule("Invalid index.");
				return;
			}

			remove_alias.AliasCmds().erase(remove_alias.AliasCmds().begin() + index);
			remove_alias.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void ClearCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias clear_alias;
		if (CAlias::AliasGet(clear_alias, this, name))
		{
			clear_alias.AliasCmds().clear();
			clear_alias.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void ListCommand(const CString& sLine)
	{
		CString output = "The following aliases exist:";
		MCString::iterator i = BeginNV();
		if (i == EndNV()) output += " [none]";
		for (; i != EndNV(); ++i)
		{
			output.append(" ");
			output.append(i->first);
		}
		PutModule(output);
	}

	void InfoCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias info_alias;
		if (CAlias::AliasGet(info_alias, this, name))
		{
			PutModule("Actions for alias " + info_alias.GetName() + ":");
			for (size_t i = 0; i < info_alias.AliasCmds().size(); ++i)
			{
				CString num(i);
				CString padding(4 - (num.length() > 3 ? 3 : num.length()), ' ');
				PutModule(num + padding + info_alias.AliasCmds()[i]);
			}
			PutModule("End of actions for alias " + info_alias.GetName() + ".");
		}
		else PutModule("Alias does not exist.");
	}

	MODCONSTRUCTOR(CAliasMod),
	sending_lines(false)
	{
		AddHelpCommand();
		AddCommand("Create", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::CreateCommand), "<name>", "Creates a new, blank alias called name.");
		AddCommand("Delete", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::DeleteCommand), "<name>", "Deletes an existing alias.");
		AddCommand("Add", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::AddCmd), "<name> <action ...>", "Adds a line to an existing alias.");
		AddCommand("Insert", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::InsertCommand), "<name> <pos> <action ...>", "Inserts a line into an existing alias.");
		AddCommand("Remove", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::RemoveCommand), "<name> <linenum>", "Removes a line from an existing alias.");
		AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::ClearCommand), "<name>", "Removes all line from an existing alias.");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::ListCommand), "", "Lists all aliases by name.");
		AddCommand("Info", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::InfoCommand), "<name>", "Reports the actions performed by an alias.");
	}

	EModRet OnUserRaw(CString& sLine) override
	{
		CAlias current_alias;

		if (sending_lines) return CONTINUE;

		try
		{
			if (sLine.Equals("ZNC-CLEAR-ALL-ALIASES!"))
			{
				ListCommand("");
				PutModule("Clearing all of them!");
				ClearNV();
				return HALT;
			}
			else if (CAlias::AliasGet(current_alias, this, sLine))
			{
				VCString rawLines;
				current_alias.Imprint(sLine).Split("\n", rawLines, false);
				sending_lines = true;

				for (size_t i = 0; i < rawLines.size(); ++i) {
					m_pClient->ReadLine(rawLines[i]);
				}

				sending_lines = false;
				return HALT;
			}
		}
		catch (std::exception &e)
		{
			CString my_nick = (GetNetwork() == NULL ? "" : GetNetwork()->GetCurNick());
			if (my_nick.empty()) my_nick = "*";
			PutUser(CString(":znc.in 461 " + my_nick + " " + current_alias.GetName() + " :ZNC alias error: ") + e.what());
			return HALTCORE;
		}

		return CONTINUE;
	}
};

template<> void TModInfo<CAliasMod>(CModInfo& Info) {
	Info.SetWikiPage("alias");
	Info.AddType(CModInfo::NetworkModule);
}

USERMODULEDEFS(CAliasMod, "Provides bouncer-side command alias support.")

