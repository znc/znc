/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <list>

using std::list;

class CWatchSource {
public:
	CWatchSource(const CString& sSource, bool bNegated) {
		m_sSource = sSource;
		m_bNegated = bNegated;
	}
	virtual ~CWatchSource() {}

	// Getters
	const CString& GetSource() const { return m_sSource; }
	bool IsNegated() const { return m_bNegated; }
	// !Getters

	// Setters
	// !Setters
private:
protected:
	bool    m_bNegated;
	CString m_sSource;
};

class CWatchEntry {
public:
	CWatchEntry(const CString& sHostMask, const CString& sTarget, const CString& sPattern) {
		m_bDisabled = false;
		m_sPattern = (sPattern.size()) ? sPattern : "*";

		CNick Nick;
		Nick.Parse(sHostMask);

		m_sHostMask = (Nick.GetNick().size()) ? Nick.GetNick() : "*";
		m_sHostMask += "!";
		m_sHostMask += (Nick.GetIdent().size()) ? Nick.GetIdent() : "*";
		m_sHostMask += "@";
		m_sHostMask += (Nick.GetHost().size()) ? Nick.GetHost() : "*";

		if (sTarget.size()) {
			m_sTarget = sTarget;
		} else {
			m_sTarget = "$";
			m_sTarget += Nick.GetNick();
		}
	}
	virtual ~CWatchEntry() {}

	bool IsMatch(const CNick& Nick, const CString& sText, const CString& sSource, const CIRCNetwork* pNetwork) {
		if (IsDisabled()) {
			return false;
		}

		bool bGoodSource = true;

		if (sSource.size() && m_vsSources.size()) {
			bGoodSource = false;

			for (unsigned int a = 0; a < m_vsSources.size(); a++) {
				const CWatchSource& WatchSource = m_vsSources[a];

				if (sSource.AsLower().WildCmp(WatchSource.GetSource().AsLower())) {
					if (WatchSource.IsNegated()) {
						return false;
					} else {
						bGoodSource = true;
					}
				}
			}
		}

		if (!bGoodSource)
			return false;
		if (!Nick.GetHostMask().AsLower().WildCmp(m_sHostMask.AsLower()))
			return false;
		return (sText.AsLower().WildCmp(pNetwork->ExpandString(m_sPattern).AsLower()));
	}

	bool operator ==(const CWatchEntry& WatchEntry) {
		return (GetHostMask().Equals(WatchEntry.GetHostMask())
				&& GetTarget().Equals(WatchEntry.GetTarget())
				&& GetPattern().Equals(WatchEntry.GetPattern())
		);
	}

	// Getters
	const CString& GetHostMask() const { return m_sHostMask; }
	const CString& GetTarget() const { return m_sTarget; }
	const CString& GetPattern() const { return m_sPattern; }
	bool IsDisabled() const { return m_bDisabled; }
	const vector<CWatchSource>& GetSources() const { return m_vsSources; }
	CString GetSourcesStr() const {
		CString sRet;

		for (unsigned int a = 0; a < m_vsSources.size(); a++) {
			const CWatchSource& WatchSource = m_vsSources[a];

			if (a) {
				sRet += " ";
			}

			if (WatchSource.IsNegated()) {
				sRet += "!";
			}

			sRet += WatchSource.GetSource();
		}

		return sRet;
	}
	// !Getters

	// Setters
	void SetHostMask(const CString& s) { m_sHostMask = s; }
	void SetTarget(const CString& s) { m_sTarget = s; }
	void SetPattern(const CString& s) { m_sPattern = s; }
	void SetDisabled(bool b = true) { m_bDisabled = b; }
	void SetSources(const CString& sSources) {
		VCString vsSources;
		VCString::iterator it;
		sSources.Split(" ", vsSources, false);

		m_vsSources.clear();

		for (it = vsSources.begin(); it != vsSources.end(); ++it) {
			if (it->at(0) == '!' && it->size() > 1) {
				m_vsSources.push_back(CWatchSource(it->substr(1), true));
			} else {
				m_vsSources.push_back(CWatchSource(*it, false));
			}
		}
	}
	// !Setters
private:
protected:
	CString              m_sHostMask;
	CString              m_sTarget;
	CString              m_sPattern;
	bool                 m_bDisabled;
	vector<CWatchSource> m_vsSources;
};

class CWatcherMod : public CModule {
public:
	MODCONSTRUCTOR(CWatcherMod) {
		m_Buffer.SetLineCount(500);
		Load();
	}

	virtual ~CWatcherMod() {}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
		Process(OpNick, "* " + OpNick.GetNick() + " sets mode: " + sModes + " " +
			sArgs + " on " + Channel.GetName(), Channel.GetName());
	}

	virtual void OnClientLogin() {
		MCString msParams;
		msParams["target"] = m_pNetwork->GetCurNick();

		unsigned int uSize = m_Buffer.Size();
		for (unsigned int uIdx = 0; uIdx < uSize; uIdx++) {
			PutUser(m_Buffer.GetLine(uIdx, *GetClient(), msParams));
		}
		m_Buffer.Clear();
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		Process(OpNick, "* " + OpNick.GetNick() + " kicked " + sKickedNick + " from " +
			Channel.GetName() + " because [" + sMessage + "]", Channel.GetName());
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		Process(Nick, "* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") "
			"(" + sMessage + ")", "");
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") joins " +
			Channel.GetName(), Channel.GetName());
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") parts " +
			Channel.GetName() + "(" + sMessage + ")", Channel.GetName());
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		Process(OldNick, "* " + OldNick.GetNick() + " is now known as " + sNewNick, "");
	}

	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " reply [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "] to "
			"[" + Channel.GetName() + "]", Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + "- " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + ":" + Channel.GetName() + "- " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + "> " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + ":" + Channel.GetName() + "> " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0);
		if (sCmdName.Equals("ADD") || sCmdName.Equals("WATCH")) {
			Watch(sCommand.Token(1), sCommand.Token(2), sCommand.Token(3, true));
		} else if (sCmdName.Equals("HELP")) {
			Help();
		} else if (sCmdName.Equals("LIST")) {
			List();
		} else if (sCmdName.Equals("DUMP")) {
			Dump();
		} else if (sCmdName.Equals("ENABLE")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, false);
			} else {
				SetDisabled(sTok.ToUInt(), false);
			}
		} else if (sCmdName.Equals("DISABLE")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, true);
			} else {
				SetDisabled(sTok.ToUInt(), true);
			}
		} else if (sCmdName.Equals("SETSOURCES")) {
			SetSources(sCommand.Token(1).ToUInt(), sCommand.Token(2, true));
		} else if (sCmdName.Equals("CLEAR")) {
			m_lsWatchers.clear();
			PutModule("All entries cleared.");
			Save();
		} else if (sCmdName.Equals("BUFFER")) {
			CString sCount = sCommand.Token(1);

			if (sCount.size()) {
				m_Buffer.SetLineCount(sCount.ToUInt());
			}

			PutModule("Buffer count is set to [" + CString(m_Buffer.GetLineCount()) + "]");
		} else if (sCmdName.Equals("DEL")) {
			Remove(sCommand.Token(1).ToUInt());
		} else {
			PutModule("Unknown command: [" + sCmdName + "]");
		}
	}

private:
	void Process(const CNick& Nick, const CString& sMessage, const CString& sSource) {
		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
			CWatchEntry& WatchEntry = *it;

			if (WatchEntry.IsMatch(Nick, sMessage, sSource, m_pNetwork)) {
				if (m_pNetwork->IsUserAttached()) {
					m_pNetwork->PutUser(":" + WatchEntry.GetTarget() + "!watch@znc.in PRIVMSG " + m_pNetwork->GetCurNick() + " :" + sMessage);
				} else {
					m_Buffer.AddLine(":" + _NAMEDFMT(WatchEntry.GetTarget()) + "!watch@znc.in PRIVMSG {target} :{text}", sMessage);
				}
			}
		}
	}

	void SetDisabled(unsigned int uIdx, bool bDisabled) {
		if (uIdx == (unsigned int) ~0) {
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
				(*it).SetDisabled(bDisabled);
			}

			PutModule(((bDisabled) ? "Disabled all entries." : "Enabled all entries."));
			Save();
			return;
		}

		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			it++;

		(*it).SetDisabled(bDisabled);
		PutModule("Id " + CString(uIdx +1) + ((bDisabled) ? " Disabled" : " Enabled"));
		Save();
	}

	void List() {
		CTable Table;
		Table.AddColumn("Id");
		Table.AddColumn("HostMask");
		Table.AddColumn("Target");
		Table.AddColumn("Pattern");
		Table.AddColumn("Sources");
		Table.AddColumn("Off");

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			Table.AddRow();
			Table.SetCell("Id", CString(uIdx));
			Table.SetCell("HostMask", WatchEntry.GetHostMask());
			Table.SetCell("Target", WatchEntry.GetTarget());
			Table.SetCell("Pattern", WatchEntry.GetPattern());
			Table.SetCell("Sources", WatchEntry.GetSourcesStr());
			Table.SetCell("Off", (WatchEntry.IsDisabled()) ? "Off" : "");
		}

		if (Table.size()) {
			PutModule(Table);
		} else {
			PutModule("You have no entries.");
		}
	}

	void Dump() {
		if (m_lsWatchers.empty()) {
			PutModule("You have no entries.");
			return;
		}

		PutModule("---------------");
		PutModule("/msg " + GetModNick() + " CLEAR");

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			PutModule("/msg " + GetModNick() + " ADD " + WatchEntry.GetHostMask() + " " +
				WatchEntry.GetTarget() + " " + WatchEntry.GetPattern());

			if (WatchEntry.GetSourcesStr().size()) {
				PutModule("/msg " + GetModNick() + " SETSOURCES " + CString(uIdx) + " " +
					WatchEntry.GetSourcesStr());
			}

			if (WatchEntry.IsDisabled()) {
				PutModule("/msg " + GetModNick() + " DISABLE " + CString(uIdx));
			}
		}

		PutModule("---------------");
	}

	void SetSources(unsigned int uIdx, const CString& sSources) {
		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			it++;

		(*it).SetSources(sSources);
		PutModule("Sources set for Id " + CString(uIdx +1) + ".");
		Save();
	}

	void Remove(unsigned int uIdx) {
		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			it++;

		m_lsWatchers.erase(it);
		PutModule("Id " + CString(uIdx +1) + " Removed.");
		Save();
	}

	void Help() {
		CTable Table;

		Table.AddColumn("Command");
		Table.AddColumn("Description");

		Table.AddRow();
		Table.SetCell("Command", "Add <HostMask> [Target] [Pattern]");
		Table.SetCell("Description", "Used to add an entry to watch for.");

		Table.AddRow();
		Table.SetCell("Command", "List");
		Table.SetCell("Description", "List all entries being watched.");

		Table.AddRow();
		Table.SetCell("Command", "Dump");
		Table.SetCell("Description", "Dump a list of all current entries to be used later.");

		Table.AddRow();
		Table.SetCell("Command", "Del <Id>");
		Table.SetCell("Description", "Deletes Id from the list of watched entries.");

		Table.AddRow();
		Table.SetCell("Command", "Clear");
		Table.SetCell("Description", "Delete all entries.");

		Table.AddRow();
		Table.SetCell("Command", "Enable <Id | *>");
		Table.SetCell("Description", "Enable a disabled entry.");

		Table.AddRow();
		Table.SetCell("Command", "Disable <Id | *>");
		Table.SetCell("Description", "Disable (but don't delete) an entry.");

		Table.AddRow();
		Table.SetCell("Command", "Buffer [Count]");
		Table.SetCell("Description", "Show/Set the amount of buffered lines while detached.");

		Table.AddRow();
		Table.SetCell("Command", "SetSources <Id> [#chan priv #foo* !#bar]");
		Table.SetCell("Description", "Set the source channels that you care about.");

		Table.AddRow();
		Table.SetCell("Command", "Help");
		Table.SetCell("Description", "This help.");

		PutModule(Table);
	}

	void Watch(const CString& sHostMask, const CString& sTarget, const CString& sPattern, bool bNotice = false) {
		CString sMessage;

		if (sHostMask.size()) {
			CWatchEntry WatchEntry(sHostMask, sTarget, sPattern);

			bool bExists = false;
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
				if (*it == WatchEntry) {
					sMessage = "Entry for [" + WatchEntry.GetHostMask() + "] already exists.";
					bExists = true;
					break;
				}
			}

			if (!bExists) {
				sMessage = "Adding entry: [" + WatchEntry.GetHostMask() + "] watching for "
					"[" + WatchEntry.GetPattern() + "] -> [" + WatchEntry.GetTarget() + "]";
				m_lsWatchers.push_back(WatchEntry);
			}
		} else {
			sMessage = "Watch: Not enough arguments.  Try Help";
		}

		if (bNotice) {
			PutModNotice(sMessage);
		} else {
			PutModule(sMessage);
		}
		Save();
	}

	void Save() {
		ClearNV(false);
		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
			CWatchEntry& WatchEntry = *it;
			CString sSave;

			sSave  = WatchEntry.GetHostMask() + "\n";
			sSave += WatchEntry.GetTarget() + "\n";
			sSave += WatchEntry.GetPattern() + "\n";
			sSave += (WatchEntry.IsDisabled() ? "disabled\n" : "enabled\n");
			sSave += WatchEntry.GetSourcesStr();
			// Without this, loading fails if GetSourcesStr()
			// returns an empty string
			sSave += " ";

			SetNV(sSave, "", false);
		}

		SaveRegistry();
	}

	void Load() {
		// Just to make sure we dont mess up badly
		m_lsWatchers.clear();

		bool bWarn = false;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			VCString vList;
			it->first.Split("\n", vList);

			if (vList.size() != 5) {
				bWarn = true;
				continue;
			}

			CWatchEntry WatchEntry(vList[0], vList[1], vList[2]);
			if (vList[3].Equals("disabled"))
				WatchEntry.SetDisabled(true);
			else
				WatchEntry.SetDisabled(false);
			WatchEntry.SetSources(vList[4]);
			m_lsWatchers.push_back(WatchEntry);
		}

		if (bWarn)
			PutModule("WARNING: malformed entry found while loading");
	}

	list<CWatchEntry>  m_lsWatchers;
	CBuffer            m_Buffer;
};

template<> void TModInfo<CWatcherMod>(CModInfo& Info) {
	Info.SetWikiPage("watch");
}

NETWORKMODULEDEFS(CWatcherMod, "Copy activity from a specific user into a separate window")
