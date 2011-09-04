/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"

class CAutoCycleMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoCycleMod) {
		m_recentlyCycled.SetTTL(15 * 1000);
	}

	virtual ~CAutoCycleMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (VCString::const_iterator it = vsChans.begin(); it != vsChans.end(); ++it) {
			if (!Add(*it)) {
				PutModule("Unable to add [" + *it + "]");
			}
		}

		// Load our saved settings, ignore errors
		MCString::iterator it;
		for (it = BeginNV(); it != EndNV(); ++it) {
			Add(it->first);
		}

		// Default is auto cycle for all channels
		if (m_vsChans.empty())
			Add("*");

		return true;
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0);

		if (sCommand.Equals("ADD")) {
			CString sChan = sLine.Token(1);

			if (AlreadyAdded(sChan)) {
				PutModule(sChan + " is already added");
			} else if (Add(sChan)) {
				PutModule("Added " + sChan + " to list");
			} else {
				PutModule("Usage: Add [!]<#chan>");
			}
		} else if (sCommand.Equals("DEL")) {
			CString sChan = sLine.Token(1);

			if (Del(sChan))
				PutModule("Removed " + sChan + " from list");
			else
				PutModule("Usage: Del [!]<#chan>");
		} else if (sCommand.Equals("LIST")) {
			CTable Table;
			Table.AddColumn("Chan");

			for (unsigned int a = 0; a < m_vsChans.size(); a++) {
				Table.AddRow();
				Table.SetCell("Chan", m_vsChans[a]);
			}

			for (unsigned int b = 0; b < m_vsNegChans.size(); b++) {
				Table.AddRow();
				Table.SetCell("Chan", "!" + m_vsNegChans[b]);
			}

			if (Table.size()) {
				PutModule(Table);
			} else {
				PutModule("You have no entries.");
			}
		} else if (sCommand.Equals("HELP")) {
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Description");

			Table.AddRow();
			Table.SetCell("Command", "Add");
			Table.SetCell("Description", "Add an entry, use !#chan to negate and * for wildcards");

			Table.AddRow();
			Table.SetCell("Command", "Del");
			Table.SetCell("Description", "Remove an entry, needs to be an exact match");

			Table.AddRow();
			Table.SetCell("Command", "List");
			Table.SetCell("Description", "List all entries");

			if (Table.size()) {
				PutModule(Table);
			} else {
				PutModule("You have no entries.");
			}
		}
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		AutoCycle(Channel);
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		for (unsigned int i = 0; i < vChans.size(); i++)
			AutoCycle(*vChans[i]);
	}

	virtual void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage) {
		AutoCycle(Channel);
	}

protected:
	void AutoCycle(CChan& Channel) {
		if (!IsAutoCycle(Channel.GetName()))
			return;

		// Did we recently annoy opers via cycling of an empty channel?
		if (m_recentlyCycled.HasItem(Channel.GetName()))
			return;

		// Is there only one person left in the channel?
		if (Channel.GetNickCount() != 1)
			return;

		// Is that person us and we don't have op?
		const CNick& pNick = Channel.GetNicks().begin()->second;
		if (!pNick.HasPerm(CChan::Op) && pNick.GetNick().Equals(m_pUser->GetCurNick())) {
			Channel.Cycle();
			m_recentlyCycled.AddItem(Channel.GetName());
		}
	}

	bool AlreadyAdded(const CString& sInput) {
		vector<CString>::iterator it;

		if (sInput.Left(1) == "!") {
			CString sChan = sInput.substr(1);
			for (it = m_vsNegChans.begin(); it != m_vsNegChans.end();
					++it) {
				if (*it == sChan)
					return true;
			}
		} else {
			for (it = m_vsChans.begin(); it != m_vsChans.end(); ++it) {
				if (*it == sInput)
					return true;
			}
		}
		return false;
	}

	bool Add(const CString& sChan) {
		if (sChan.empty() || sChan == "!") {
			return false;
		}

		if (sChan.Left(1) == "!") {
			m_vsNegChans.push_back(sChan.substr(1));
		} else {
			m_vsChans.push_back(sChan);
		}

		// Also save it for next module load
		SetNV(sChan, "");

		return true;
	}

	bool Del(const CString& sChan) {
		vector<CString>::iterator it, end;

		if (sChan.empty() || sChan == "!")
			return false;

		if (sChan.Left(1) == "!") {
			CString sTmp = sChan.substr(1);
			it = m_vsNegChans.begin();
			end = m_vsNegChans.end();

			for (; it != end; ++it)
				if (*it == sTmp)
					break;

			if (it == end)
				return false;

			m_vsNegChans.erase(it);
		} else {
			it = m_vsChans.begin();
			end = m_vsChans.end();

			for (; it != end; ++it)
				if (*it == sChan)
					break;

			if (it == end)
				return false;

			m_vsChans.erase(it);
		}

		DelNV(sChan);

		return true;
	}

	bool IsAutoCycle(const CString& sChan) {
		for (unsigned int a = 0; a < m_vsNegChans.size(); a++) {
			if (sChan.WildCmp(m_vsNegChans[a])) {
				return false;
			}
		}

		for (unsigned int b = 0; b < m_vsChans.size(); b++) {
			if (sChan.WildCmp(m_vsChans[b])) {
				return true;
			}
		}

		return false;
	}

private:
	vector<CString> m_vsChans;
	vector<CString> m_vsNegChans;
	TCacheMap<CString> m_recentlyCycled;
};

template<> void TModInfo<CAutoCycleMod>(CModInfo& Info) {
	Info.SetWikiPage("autocycle");
}

MODULEDEFS(CAutoCycleMod, "Rejoins channels to gain Op if you're the only user left")
