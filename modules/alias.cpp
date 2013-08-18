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
	const CString &GetName()
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
	static bool AliasExists(CModule *mod, CString n)
	{
		n = n.Token(0, false, " ").MakeUpper();
		return (mod->FindNV(n) != mod->EndNV());
	}

	// populate alias from stored settings in registry, or return false if none exists
	static bool AliasGet(CAlias &a, CModule *mod, CString line)
	{
		line = line.Token(0, false, " ").MakeUpper();
		MCString::iterator i = mod->FindNV(line);
		if (i == mod->EndNV()) return false;
		a.parent = mod;
		a.name = line;
		i->second.Split("\n", a.alias_cmds, false);
		return true;
	}

	// string -> something else
	// why the CString::To* functions don't work like this is anybody's guess.
	// you can't use those to actually check of the conversion was successful,
	// since strtol and friends return the parsed value, and 0 if they fail.
	template <typename T> static bool ConvertString(const CString &s, T &t)
	{
		stringstream ss(s);
		ss >> t;
		return (bool) ss; // we don't care why it failed, only whether it failed
	}

	// constructors
	CAlias() : parent(NULL) {}
	CAlias(CModule *p, const CString &n) : parent(p) { SetName(n); }

	// produce a command string from this alias' command list.  This should also have a function in CString:
	// CString CString::Join(const VCString &strings), or even template <typename T> CString CString::Join(const T &strings)
	// look at how python's string join works.
	CString GetCommands() const
	{
		CString data = alias_cmds.size() > 0 ? alias_cmds[0] : "";
		for (unsigned int i = 1; i < alias_cmds.size(); ++i) data.append("\n").append(alias_cmds[i]);
		return data;
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

	// read an IRC line and do token substitution
	CString Imprint(CString line) const
	{
		CString output;
		CString data = GetCommands();
		data = parent->ExpandString(data);
		int found = -1, dummy = 0, lastfound = 0;

		// it would be very inefficient to attempt to blindly replace every possible token
		// so let's just parse the line and replace when we find them
		// token syntax:
		// %[?]n[+]%
		// adding ? makes the substitution optional (you'll get "" if there are insufficient tokens, otherwise the alias will fail)
		// adding + makes the substitution contain all tokens from the nth to the end of the line
		while (true)
		{
			lastfound = dummy;
			if (found >= (int) data.length()) break;
			found = data.find("%", found + 1);
			if (found == (int) CString::npos) break;
			dummy = found;
			output.append(data.substr(lastfound, found - lastfound));
			int index = found + 1;
			bool optional = false;
			bool subsequent = false;
			int token = -1;
			if ((int) data.length() > index && data[index] == '?') { optional = true; ++index; }
			if (ConvertString<int>(data.c_str() + index, token)) { index += CString(token).length(); }
			else continue;
			if ((int) data.length() > index && data[index] == '+') { subsequent = true; ++index; }
			if ((int) data.length() > index && data[index] == '%');
			else continue;
			
			CString stok = line.Token(token, subsequent, " ");
			if (stok.empty() && !optional) throw std::invalid_argument(CString("missing required parameter: ") + CString(token));
			output += stok;
			dummy = index + 1;
			found = index;
		}

		output += data.substr(lastfound);
		return output;
	}
};

class CAliasMod : public CModule {
public:
	using CModule::AddCommand;

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
		CAlias da;
		if (CAlias::AliasGet(da, this, name))
		{
			PutModule("Deleted alias: " + da.GetName());
			da.Delete();
		}
		else PutModule("Alias does not exist.");
	}

	void AddCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias a;
		if (CAlias::AliasGet(a, this, name))
		{
			a.AliasCmds().push_back(sLine.Token(2, true, " "));
			a.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void InsertCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias a;
		int i;
		if (CAlias::AliasGet(a, this, name))
		{	
			if (!CAlias::ConvertString<int>(sLine.Token(2, false, " "), i) || i < 0 || i > (int) a.AliasCmds().size())
			{
				PutModule("Invalid index.");
				return;
			}

			a.AliasCmds().insert(a.AliasCmds().begin() + i, sLine.Token(3, true, " "));
			a.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void RemoveCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias a;
		int i;
		if (CAlias::AliasGet(a, this, name))
		{	
			if (!CAlias::ConvertString<int>(sLine.Token(2, false, " "), i) || i < 0 || i > (int) a.AliasCmds().size() - 1)
			{
				PutModule("Invalid index.");
				return;
			}

			a.AliasCmds().erase(a.AliasCmds().begin() + i);
			a.Commit();
			PutModule("Modified alias.");
		}
		else PutModule("Alias does not exist.");
	}

	void ClearCommand(const CString& sLine)
	{
		CString name = sLine.Token(1, false, " ");
		CAlias a;
		if (CAlias::AliasGet(a, this, name))
		{
			a.AliasCmds().clear();
			a.Commit();
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
		CAlias a;
		if (CAlias::AliasGet(a, this, name))
		{
			PutModule("Actions for alias " + a.GetName() + ":");
			for (int i = 0; i < (int) a.AliasCmds().size(); ++i)
			{
				CString num(i);
				PutModule(CString(i) + ("    " + ((num.length() > 3) ? 3 : num.length())) + a.AliasCmds()[i]);
			}
			PutModule("End of actions for alias " + a.GetName() + ".");
		}
		else PutModule("Alias does not exist.");
	}

	MODCONSTRUCTOR(CAliasMod)
	{
		AddHelpCommand();
		AddCommand("Create", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::CreateCommand), "<name>", "Creates a new, blank alias called name.");
		AddCommand("Delete", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::DeleteCommand), "<name>", "Deletes an existing alias.");
		AddCommand("Add", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::AddCommand), "<name> <action ...>", "Adds a line to an existing alias.");
		AddCommand("Insert", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::InsertCommand), "<name> <pos> <action ...>", "Inserts a line into an existing alias.");
		AddCommand("Remove", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::RemoveCommand), "<name> <linenum>", "Removes a line from an existing alias.");
		AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::ClearCommand), "<name>", "Removes all line from an existing alias.");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::ListCommand), "", "Lists all aliases by name.");
		AddCommand("Info", static_cast<CModCommand::ModCmdFunc>(&CAliasMod::InfoCommand), "<name>", "Reports the actions performed by an alias.");
	}

	virtual EModRet OnUserRaw(CString& sLine)
	{
		CAlias a;
		try
		{
			if (sLine.Equals("ZNC-CLEAR-ALL-ALIASES!"))
			{
				ListCommand("");
				PutModule("Clearing all of them!");
				ClearNV();
				return HALT;
			}
			else if (CAlias::AliasGet(a, this, sLine))
			{
				PutIRC(a.Imprint(sLine));
				return HALT;
			}
		}
		catch (std::exception &e)
		{
			PutUser(CString(":znc.in 421 " + GetNetwork()->GetCurNick() + " " + a.GetName() + " :ZNC alias error: ") + e.what());
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

