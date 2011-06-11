/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "Modules.h"

class CAttachMatch {
public:
	CAttachMatch(const CString& sChannels, const CString& sHostmasks, bool bNegated)
	{
		m_sChannelWildcard = sChannels;
		m_sHostmaskWildcard = sHostmasks;
		m_bNegated = bNegated;

		if (m_sChannelWildcard.empty())
			m_sChannelWildcard = "*";
		if (m_sHostmaskWildcard.empty())
			m_sHostmaskWildcard = "*!*@*";
	}

	bool IsMatch(const CString& sChan, const CString& sHost) const {
		if (!sHost.WildCmp(m_sHostmaskWildcard))
			return false;
		if (!sChan.WildCmp(m_sChannelWildcard))
			return false;
		return true;
	}

	bool IsNegated() const {
		return m_bNegated;
	}

	const CString& GetHostMask() const {
		return m_sHostmaskWildcard;
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
		sRes += m_sHostmaskWildcard;
		return sRes;
	}

private:
	bool m_bNegated;
	CString m_sChannelWildcard;
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
		CString sHost = sMsg.Token(1, true);

		if (sChan.empty()) {
			bHelp = true;
		} else if (Add(bNegated, sChan, sHost)) {
			PutModule("Added to list");
		} else {
			PutModule(sLine.Token(1, true) + " is already added");
			bHelp = true;
		}
		if (bHelp) {
			PutModule("Usage: Add [!]<#chan> <host>");
			PutModule("Wildcards are allowed");
		}
	}

	void HandleDel(const CString& sLine) {
		CString sMsg  = sLine.Token(1, true);
		bool bNegated = sMsg.TrimPrefix("!");
		CString sChan = sMsg.Token(0);
		CString sHost = sMsg.Token(1, true);

		if (Del(bNegated, sChan, sHost)) {
			PutModule("Removed " + sChan + " from list");
		} else {
			PutModule("Usage: Del [!]<#chan> <host>");
		}
	}

	void HandleList(const CString& sLine) {
		CTable Table;
		Table.AddColumn("Neg");
		Table.AddColumn("Chan");
		Table.AddColumn("Host");

		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			Table.AddRow();
			Table.SetCell("Neg", it->IsNegated() ? "!" : "");
			Table.SetCell("Chan", it->GetChans());
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
			"[!]<#chan> <host>", "Add an entry, use !#chan to negate and * for wildcards");
		AddCommand("Del",    static_cast<CModCommand::ModCmdFunc>(&CChanAttach::HandleDel),
			"[!]<#chan> <host>", "Remove an entry, needs to be an exact match");
		AddCommand("List",    static_cast<CModCommand::ModCmdFunc>(&CChanAttach::HandleList),
			"",           "List all entries");
	}

	virtual ~CChanAttach() {
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		VCString vsChans;
		sArgs.Split(" ", vsChans, false);

		for (VCString::const_iterator it = vsChans.begin(); it != vsChans.end(); ++it) {
			CString sAdd = *it;
			bool bNegated = sAdd.TrimPrefix("!");
			CString sChan = sAdd.Token(0);
			CString sHost = sAdd.Token(1, true);

			if (!Add(bNegated, sChan, sHost)) {
				PutModule("Unable to add [" + *it + "]");
			}
		}

		// Load our saved settings, ignore errors
		MCString::iterator it;
		for (it = BeginNV(); it != EndNV(); ++it) {
			CString sAdd = it->first;
			bool bNegated = sAdd.TrimPrefix("!");
			CString sChan = sAdd.Token(0);
			CString sHost = sAdd.Token(1, true);

			Add(bNegated, sChan, sHost);
		}

		return true;
	}

	void TryAttach(const CNick& Nick, CChan& Channel) {
		const CString& sChan = Channel.GetName();
		const CString& sHost = Nick.GetHostMask();
		VAttachIter it;

		if (!Channel.IsDetached())
			return;

		// Any negated match?
		for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
			if (it->IsNegated() && it->IsMatch(sChan, sHost))
				return;
		}

		// Now check for a positive match
		for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
			if (!it->IsNegated() && it->IsMatch(sChan, sHost)) {
				Channel.JoinUser();
				return;
			}
		}
	}

	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
		TryAttach(Nick, Channel);
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		TryAttach(Nick, Channel);
		return CONTINUE;
	}

	virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) {
		TryAttach(Nick, Channel);
		return CONTINUE;
	}

	VAttachIter FindEntry(const CString& sChan, const CString& sHost) {
		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			if (sHost.empty() || it->GetHostMask() != sHost)
				continue;
			if (sChan.empty() || it->GetChans() != sChan)
				continue;
			return it;
		}
		return m_vMatches.end();
	}

	bool Add(bool bNegated, const CString& sChan, const CString& sHost) {
		CAttachMatch attach(sChan, sHost, bNegated);

		// Check for duplicates
		VAttachIter it = m_vMatches.begin();
		for (; it != m_vMatches.end(); ++it) {
			if (it->GetHostMask() == attach.GetHostMask()
					&& it->GetChans() == attach.GetChans())
				return false;
		}

		m_vMatches.push_back(attach);

		// Also save it for next module load
		SetNV(attach.ToString(), "");

		return true;
	}

	bool Del(bool bNegated, const CString& sChan, const CString& sHost) {
		VAttachIter it = FindEntry(sChan, sHost);
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
	Info.SetWikiPage("autoattach");
}

MODULEDEFS(CChanAttach, "Reattaches you to channels on activity.")
