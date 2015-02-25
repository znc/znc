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
#include <znc/Modules.h>

using std::vector;

class CAttachMatch {
public:
	CAttachMatch(CModule *pModule, const CString& sChannels, const CString& sSearch, const CString& sHostmasks, bool bNegated)
	{
		m_pModule = pModule;
		m_sChannelWildcard = sChannels;
		m_sSearchWildcard = sSearch;
		m_sHostmaskWildcard = sHostmasks;
		m_bNegated = bNegated;

		if (m_sChannelWildcard.empty())
			m_sChannelWildcard = "*";
		if (m_sSearchWildcard.empty())
			m_sSearchWildcard = "*";
		if (m_sHostmaskWildcard.empty())
			m_sHostmaskWildcard = "*!*@*";
	}

	bool IsMatch(const CString& sChan, const CString& sHost, const CString& sMessage) const {
		if (!sHost.WildCmp(m_sHostmaskWildcard, CString::CaseInsensitive))
			return false;
		if (!sChan.WildCmp(m_sChannelWildcard, CString::CaseInsensitive))
			return false;
		if (!sMessage.WildCmp(m_pModule->ExpandString(m_sSearchWildcard), CString::CaseInsensitive))
			return false;
		return true;
	}

	bool IsNegated() const {
		return m_bNegated;
	}

	const CString& GetHostMask() const {
		return m_sHostmaskWildcard;
	}

	const CString& GetSearch() const {
		return m_sSearchWildcard;
	}

	const CString& GetChans() const {
		return m_sChannelWildcard;
	}

	CString ToString() {
		CString sRes;
		if (m_bNegated)
			sRes += "!";
		sRes += m_sChannelWildcard;
		sRes += " ";
		sRes += m_sSearchWildcard;
		sRes += " ";
		sRes += m_sHostmaskWildcard;
		return sRes;
	}

private:
	bool m_bNegated;
	CModule *m_pModule;
	CString m_sChannelWildcard;
	CString m_sSearchWildcard;
	CString m_sHostmaskWildcard;
};

class CChanAttach : public CModule {
public:
	typedef vector<CAttachMatch> VAttachMatch;
	typedef VAttachMatch::iterator VAttachIter;

private:
	void HandleAdd(const CString& sLine) {
		CString sMsg = sLine.Token(1, true);
		bool bHelp = false;
		bool bNegated = sMsg.TrimPrefix("!");
		CString sChan = sMsg.Token(0);
		CString sSearch = sMsg.Token(1);
		CString sHost = sMsg.Token(2);

		if (sChan.empty()) {
			bHelp = true;
		} else if (Add(bNegated, sChan, sSearch, sHost)) {
			PutModule("Added to list");
		} else {
			PutModule(sLine.Token(1, true) + " is already added");
			bHelp = true;
		}
		if (bHelp) {
			PutModule("Usage: Add [!]<#chan> <search> <host>");
			PutModule("Wildcards are allowed");
		}
	}

	void HandleDel(const CString& sLine) {
		CString sMsg  = sLine.Token(1, true);
		bool bNegated = sMsg.TrimPrefix("!");
		CString sChan = sMsg.Token(0);
		CString sSearch = sMsg.Token(1);
		CString sHost = sMsg.Token(2);

		if (Del(bNegated, sChan, sSearch, sHost)) {
			PutModule("Removed " + sChan + " from list");
		} else {
			PutModule("Usage: Del [!]<#chan> <search> <host>");
		}
	}

	void HandleList(const CString& sLine) {
		CTable Table;
		Table.AddColumn("Neg");
		Table.AddColumn("Chan");
		Table.AddColumn("Search");
		Table.AddColumn("Host");

		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Neg", it->IsNegated() ? "!" : "");
			Table.SetCell("Chan", it->GetChans());
			Table.SetCell("Search", it->GetSearch());
			Table.SetCell("Host", it->GetHostMask());
		}

		if (Table.size()) {
			PutModule(Table);
		} else {
			PutModule("You have no entries.");
		}
	}

public:
	MODCONSTRUCTOR(CChanAttach) {
		AddHelpCommand();
		AddCommand("Add",    static_cast<CModCommand::ModCmdFunc>(&CChanAttach::HandleAdd),
			"[!]<#chan> <search> <host>", "Add an entry, use !#chan to negate and * for wildcards");
		AddCommand("Del",    static_cast<CModCommand::ModCmdFunc>(&CChanAttach::HandleDel),
			"[!]<#chan> <search> <host>", "Remove an entry, needs to be an exact match");
		AddCommand("List",    static_cast<CModCommand::ModCmdFunc>(&CChanAttach::HandleList),
			"",           "List all entries");
	}

	virtual ~CChanAttach() {
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (VCString::const_iterator it = vsChans.begin(); it != vsChans.end(); ++it) {
			CString sAdd = *it;
			bool bNegated = sAdd.TrimPrefix("!");
			CString sChan = sAdd.Token(0);
			CString sSearch = sAdd.Token(1);
			CString sHost = sAdd.Token(2, true);

			if (!Add(bNegated, sChan, sSearch, sHost)) {
				PutModule("Unable to add [" + *it + "]");
			}
		}

		// Load our saved settings, ignore errors
		MCString::iterator it;
		for (it = BeginNV(); it != EndNV(); ++it) {
			CString sAdd = it->first;
			bool bNegated = sAdd.TrimPrefix("!");
			CString sChan = sAdd.Token(0);
			CString sSearch = sAdd.Token(1);
			CString sHost = sAdd.Token(2, true);

			Add(bNegated, sChan, sSearch, sHost);
		}

		return true;
	}

	void TryAttach(const CNick& Nick, CChan& Channel, CString& Message) {
		const CString& sChan = Channel.GetName();
		const CString& sHost = Nick.GetHostMask();
		const CString& sMessage = Message;
		VAttachIter it;

		if (!Channel.IsDetached())
			return;

		// Any negated match?
		for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
			if (it->IsNegated() && it->IsMatch(sChan, sHost, sMessage))
				return;
		}

		// Now check for a positive match
		for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
			if (!it->IsNegated() && it->IsMatch(sChan, sHost, sMessage)) {
				Channel.AttachUser();
				return;
			}
		}
	}

	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override {
		TryAttach(Nick, Channel, sMessage);
		return CONTINUE;
	}

	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
		TryAttach(Nick, Channel, sMessage);
		return CONTINUE;
	}

	EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override {
		TryAttach(Nick, Channel, sMessage);
		return CONTINUE;
	}

	VAttachIter FindEntry(const CString& sChan, const CString& sSearch, const CString& sHost) {
		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			if (sHost.empty() || it->GetHostMask() != sHost)
				continue;
			if (sSearch.empty() || it->GetSearch() != sSearch)
				continue;
			if (sChan.empty() || it->GetChans() != sChan)
				continue;
			return it;
		}
		return m_vMatches.end();
	}

	bool Add(bool bNegated, const CString& sChan, const CString& sSearch, const CString& sHost) {
		CAttachMatch attach(this, sChan, sSearch, sHost, bNegated);

		// Check for duplicates
		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			if (it->GetHostMask() == attach.GetHostMask()
					&& it->GetChans() == attach.GetChans()
					&& it->GetSearch() == attach.GetSearch())
				return false;
		}

		m_vMatches.push_back(attach);

		// Also save it for next module load
		SetNV(attach.ToString(), "");

		return true;
	}

	bool Del(bool bNegated, const CString& sChan, const CString& sSearch, const CString& sHost) {
		VAttachIter it = FindEntry(sChan, sSearch, sHost);
		if (it == m_vMatches.end() || it->IsNegated() != bNegated)
			return false;

		DelNV(it->ToString());
		m_vMatches.erase(it);

		return true;
	}
private:
	VAttachMatch m_vMatches;
};

template<> void TModInfo<CChanAttach>(CModInfo& Info) {
	Info.AddType(CModInfo::UserModule);
	Info.SetWikiPage("autoattach");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("List of channel masks and channel masks with ! before them.");
}

NETWORKMODULEDEFS(CChanAttach, "Reattaches you to channels on activity.")
