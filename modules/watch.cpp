#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Buffer.h"
#include "Utils.h"

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
	bool	m_bNegated;
	CString	m_sSource;
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

	bool IsMatch(const CNick& Nick, const CString& sText, const CString& sSource) {
		if (IsDisabled()) {
			return false;
		}

		bool bGoodSource = true;

		if (sSource.size() && m_vsSources.size()) {
			bGoodSource = false;

			for (unsigned int a = 0; a < m_vsSources.size(); a++) {
				const CWatchSource& WatchSource = m_vsSources[a];

				if (sSource.AsLower().WildCmp(Lower(WatchSource.GetSource()))) {
					if (WatchSource.IsNegated()) {
						return false;
					} else {
						bGoodSource = true;
					}
				}
			}
		}

		return (bGoodSource && Nick.GetHostMask().AsLower().WildCmp(Lower(m_sHostMask))) && sText.AsLower().WildCmp(Lower(m_sPattern));
	}

	bool operator ==(const CWatchEntry& WatchEntry) {
		return (strcasecmp(GetHostMask().c_str(), WatchEntry.GetHostMask().c_str()) == 0
			   	&& strcasecmp(GetTarget().c_str(), WatchEntry.GetTarget().c_str()) == 0
		   		&& strcasecmp(GetPattern().c_str(), WatchEntry.GetPattern().c_str()) == 0
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
		unsigned int uIdx = 1;
		CString sSrc = sSources.Token(0);

		m_vsSources.clear();

		while (sSrc.size()) {
			if (sSrc[0] == '!') {
				if (sSrc.size() > 1) {
					m_vsSources.push_back(CWatchSource(sSrc.substr(1), true));
				}
			} else {
				m_vsSources.push_back(CWatchSource(sSrc, false));
			}

			sSrc = sSources.Token(uIdx++);
		}
	}
	// !Setters
private:
protected:
	CString		m_sHostMask;
	CString		m_sTarget;
	CString		m_sPattern;
	bool		m_bDisabled;
	vector<CWatchSource>	m_vsSources;
};

class CWatcherMod : public CModule {
public:
	MODCONSTRUCTOR(CWatcherMod) {
		m_Buffer.SetLineCount(500);
	}

	virtual bool OnLoad(const CString& sArgs) {
		return true;
	}

	virtual ~CWatcherMod() {
	}

	virtual CString GetDescription() {
		return "Copy activity from a specific user into a separate window.";
	}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
		Process(OpNick, "* " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs + " on " + Channel.GetName(), Channel.GetName());
	}

	virtual void OnUserAttached() {
		CString sBufLine;
		while (m_Buffer.GetNextLine(m_pUser->GetCurNick(), sBufLine)) {
			PutUser(sBufLine);
		}

		m_Buffer.Clear();
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		if (strncasecmp(sLine.c_str(), "WATCH ", 6) == 0) {
			Watch(sLine.Token(1), sLine.Token(2), sLine.Token(3, true), true);
			return HALT;
		}

		return CONTINUE;
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		Process(OpNick, "* " + OpNick.GetNick() + " kicked " + sKickedNick + " from " + Channel.GetName() + " because [" + sMessage + "]", Channel.GetName());
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		Process(Nick, "* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") (" + sMessage + ")", "");
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") joins " + Channel.GetName(), Channel.GetName());
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") parts " + Channel.GetName(), Channel.GetName());
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		Process(OldNick, "* " + OldNick.GetNick() + " is now known as " + sNewNick, "");
	}

	virtual EModRet OnCTCPReply(const CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " reply [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnPrivCTCP(const CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanCTCP(const CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "] to [" + Channel.GetName() + "]", Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(const CNick& Nick, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + "- " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanNotice(const CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + ":" + Channel.GetName() + "- " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(const CNick& Nick, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + "> " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(const CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + ":" + Channel.GetName() + "> " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0);
		if (strcasecmp(sCmdName.c_str(), "ADD") == 0 || strcasecmp(sCmdName.c_str(), "WATCH") == 0) {
			Watch(sCommand.Token(1), sCommand.Token(2), sCommand.Token(3, true));
		} else if (strcasecmp(sCmdName.c_str(), "HELP") == 0) {
			Help();
		} else if (strcasecmp(sCmdName.c_str(), "LIST") == 0) {
			List();
		} else if (strcasecmp(sCmdName.c_str(), "DUMP") == 0) {
			Dump();
		} else if (strcasecmp(sCmdName.c_str(), "ENABLE") == 0) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, false);
			} else {
				SetDisabled(atoi(sTok.c_str()), false);
			}
		} else if (strcasecmp(sCmdName.c_str(), "DISABLE") == 0) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, true);
			} else {
				SetDisabled(atoi(sTok.c_str()), true);
			}
		} else if (strcasecmp(sCmdName.c_str(), "SETSOURCES") == 0) {
			SetSources(atoi(sCommand.Token(1).c_str()), sCommand.Token(2, true));
		} else if (strcasecmp(sCmdName.c_str(), "CLEAR") == 0) {
			m_lsWatchers.clear();
			PutModule("All entries cleared.");
		} else if (strcasecmp(sCmdName.c_str(), "BUFFER") == 0) {
			CString sCount = sCommand.Token(1);

			if (sCount.size()) {
				m_Buffer.SetLineCount(atoi(sCount.c_str()));
			}

			PutModule("Buffer count is set to [" + CString::ToString(m_Buffer.GetLineCount()) + "]");
		} else if (strcasecmp(sCmdName.c_str(), "DEL") == 0) {
			Remove(atoi(sCommand.Token(1).c_str()));
		} else {
			PutModule("Unknown command: [" + sCommand.Token(0) + "]");
		}
	}

private:
	void Process(const CNick& Nick, const CString& sMessage, const CString& sSource) {
		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++) {
			CWatchEntry& WatchEntry = *it;

			if (WatchEntry.IsMatch(Nick, sMessage, sSource)) {
				if (m_pUser->IsUserAttached()) {
					m_pUser->PutUser(":" + WatchEntry.GetTarget() + "!watch@znc.com PRIVMSG " + m_pUser->GetCurNick() + " :" + sMessage);
				} else {
					m_Buffer.AddLine(":" + WatchEntry.GetTarget() + "!watch@znc.com PRIVMSG ", " :" + sMessage);
				}
			}
		}
	}

	void SetDisabled(unsigned int uIdx, bool bDisabled) {
		if (uIdx == (unsigned int) ~0) {
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++) {
				(*it).SetDisabled(bDisabled);
			}

			PutModule(((bDisabled) ? "Disabled all entries." : "Enabled all entries."));
			return;
		}

		uIdx--;	// "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++, it++);

		(*it).SetDisabled(bDisabled);
		PutModule("Id " + CString::ToString(uIdx +1) + ((bDisabled) ? " Disabled" : " Enabled"));
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
			Table.SetCell("Id", CString::ToString(uIdx));
			Table.SetCell("HostMask", WatchEntry.GetHostMask());
			Table.SetCell("Target", WatchEntry.GetTarget());
			Table.SetCell("Pattern", WatchEntry.GetPattern());
			Table.SetCell("Sources", WatchEntry.GetSourcesStr());
			Table.SetCell("Off", (WatchEntry.IsDisabled()) ? "Off" : "");
		}

		if (Table.size()) {
			unsigned int uTableIdx = 0;
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutModule(sLine);
			}
		} else {
			PutModule("You have no entries.");
		}
	}

	void Dump() {
		if (!m_lsWatchers.size()) {
			PutModule("You have no entries.");
			return;
		}

		PutModule("---------------");
		PutModule("/msg " + GetModNick() + " CLEAR");

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			PutModule("/msg " + GetModNick() + " ADD " + WatchEntry.GetHostMask() + " " + WatchEntry.GetTarget() + " " + WatchEntry.GetPattern());

			if (WatchEntry.GetSourcesStr().size()) {
				PutModule("/msg " + GetModNick() + " SETSOURCES " + CString::ToString(uIdx) + " " + WatchEntry.GetSourcesStr());
			}

			if (WatchEntry.IsDisabled()) {
				PutModule("/msg " + GetModNick() + " DISABLE " + CString::ToString(uIdx));
			}
		}

		PutModule("---------------");
	}

	void SetSources(unsigned int uIdx, const CString& sSources) {
		uIdx--;	// "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++, it++);

		(*it).SetSources(sSources);
		PutModule("Sources set for Id " + CString::ToString(uIdx +1) + ".");
	}

	void Remove(unsigned int uIdx) {
		uIdx--;	// "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++, it++);

		m_lsWatchers.erase(it);
		PutModule("Id " + CString::ToString(uIdx +1) + " Removed.");
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

		if (Table.size()) {
			unsigned int uTableIdx = 0;
			CString sLine;

			while (Table.GetLine(uTableIdx++, sLine)) {
				PutModule(sLine);
			}
		}
	}

	void Watch(const CString& sHostMask, const CString& sTarget, const CString& sPattern, bool bNotice = false) {
		CString sMessage;

		if (sHostMask.size()) {
			CWatchEntry WatchEntry(sHostMask, sTarget, sPattern);

			bool bExists = false;
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); it++) {
				if (*it == WatchEntry) {
					sMessage = "Entry for [" + WatchEntry.GetHostMask() + "] already exists.";
					bExists = true;
					break;
				}
			}

			if (!bExists) {
				sMessage = "Adding entry: [" + WatchEntry.GetHostMask() + "] watching for [" + WatchEntry.GetPattern() + "] -> [" + WatchEntry.GetTarget() + "]";
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
	}

	list<CWatchEntry>	m_lsWatchers;
	CBuffer				m_Buffer;
};

MODULEDEFS(CWatcherMod)
