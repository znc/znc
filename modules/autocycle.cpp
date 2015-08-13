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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::vector;

class CAutoCycleMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoCycleMod) {
		AddHelpCommand();
		AddCommand("Add", static_cast<CModCommand::ModCmdFunc>(&CAutoCycleMod::OnAddCommand), "[!]<#chan>", "Add an entry, use !#chan to negate and * for wildcards");
		AddCommand("Del", static_cast<CModCommand::ModCmdFunc>(&CAutoCycleMod::OnDelCommand), "[!]<#chan>", "Remove an entry, needs to be an exact match");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CAutoCycleMod::OnListCommand), "", "List all entries");
		m_recentlyCycled.SetTTL(15 * 1000);
	}

	virtual ~CAutoCycleMod() {}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (const CString& sChan : vsChans) {
			if (!Add(sChan)) {
				PutModule("Unable to add [" + sChan + "]");
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

	void OnAddCommand(const CString& sLine) {
		CString sChan = sLine.Token(1);

		if (AlreadyAdded(sChan)) {
			PutModule(sChan + " is already added");
		} else if (Add(sChan)) {
			PutModule("Added " + sChan + " to list");
		} else {
			PutModule("Usage: Add [!]<#chan>");
		}
	}

	void OnDelCommand(const CString& sLine) {
		CString sChan = sLine.Token(1);

		if (Del(sChan))
			PutModule("Removed " + sChan + " from list");
		else
			PutModule("Usage: Del [!]<#chan>");
	}

	void OnListCommand(const CString& sLine) {
		CTable Table;
		Table.AddColumn("Chan");

		for (const CString& sChan : m_vsChans) {
			Table.AddRow();
			Table.SetCell("Chan", sChan);
		}

		for (const CString& sChan : m_vsNegChans) {
			Table.AddRow();
			Table.SetCell("Chan", "!" + sChan);
		}

		if (Table.size()) {
			PutModule(Table);
		} else {
			PutModule("You have no entries.");
		}
	}

	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override {
		AutoCycle(Channel);
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		for (CChan* pChan : vChans)
			AutoCycle(*pChan);
	}

	void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage) override {
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
		if (!pNick.HasPerm(CChan::Op) && pNick.NickEquals(GetNetwork()->GetCurNick())) {
			Channel.Cycle();
			m_recentlyCycled.AddItem(Channel.GetName());
		}
	}

	bool AlreadyAdded(const CString& sInput) {
		CString sChan = sInput;
		if (sChan.TrimPrefix("!")) {
			for (const CString& s : m_vsNegChans) {
				if (s.Equals(sChan))
					return true;
			}
		} else {
			for (const CString& s : m_vsChans) {
				if (s.Equals(sChan))
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
		for (const CString& s : m_vsNegChans) {
			if (sChan.WildCmp(s, CString::CaseInsensitive)) {
				return false;
			}
		}

		for (const CString& s : m_vsChans) {
			if (sChan.WildCmp(s, CString::CaseInsensitive)) {
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
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("List of channel masks and channel masks with ! before them.");
}

NETWORKMODULEDEFS(CAutoCycleMod, "Rejoins channels to gain Op if you're the only user left")
