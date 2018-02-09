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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::list;
using std::vector;
using std::set;

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
    bool m_bNegated;
    CString m_sSource;
};

class CWatchEntry {
  public:
    CWatchEntry(const CString& sHostMask, const CString& sTarget,
                const CString& sPattern) {
        m_bDisabled = false;
        m_bDetachedClientOnly = false;
        m_bDetachedChannelOnly = false;
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

    bool IsMatch(const CNick& Nick, const CString& sText,
                 const CString& sSource, const CIRCNetwork* pNetwork) {
        if (IsDisabled()) {
            return false;
        }

        bool bGoodSource = true;

        if (!sSource.empty() && !m_vsSources.empty()) {
            bGoodSource = false;

            for (unsigned int a = 0; a < m_vsSources.size(); a++) {
                const CWatchSource& WatchSource = m_vsSources[a];

                if (sSource.WildCmp(WatchSource.GetSource(),
                                    CString::CaseInsensitive)) {
                    if (WatchSource.IsNegated()) {
                        return false;
                    } else {
                        bGoodSource = true;
                    }
                }
            }
        }

        if (!bGoodSource) return false;
        if (!Nick.GetHostMask().WildCmp(m_sHostMask, CString::CaseInsensitive))
            return false;
        return (sText.WildCmp(pNetwork->ExpandString(m_sPattern),
                              CString::CaseInsensitive));
    }

    bool operator==(const CWatchEntry& WatchEntry) {
        return (GetHostMask().Equals(WatchEntry.GetHostMask()) &&
                GetTarget().Equals(WatchEntry.GetTarget()) &&
                GetPattern().Equals(WatchEntry.GetPattern()));
    }

    // Getters
    const CString& GetHostMask() const { return m_sHostMask; }
    const CString& GetTarget() const { return m_sTarget; }
    const CString& GetPattern() const { return m_sPattern; }
    bool IsDisabled() const { return m_bDisabled; }
    bool IsDetachedClientOnly() const { return m_bDetachedClientOnly; }
    bool IsDetachedChannelOnly() const { return m_bDetachedChannelOnly; }
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
    void SetDetachedClientOnly(bool b = true) { m_bDetachedClientOnly = b; }
    void SetDetachedChannelOnly(bool b = true) { m_bDetachedChannelOnly = b; }
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
    CString m_sHostMask;
    CString m_sTarget;
    CString m_sPattern;
    bool m_bDisabled;
    bool m_bDetachedClientOnly;
    bool m_bDetachedChannelOnly;
    vector<CWatchSource> m_vsSources;
};

class CWatcherMod : public CModule {
  public:
    MODCONSTRUCTOR(CWatcherMod) {
        m_Buffer.SetLineCount(500);
        Load();
    }

    ~CWatcherMod() override {}

    void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes,
                   const CString& sArgs) override {
        Process(OpNick, "* " + OpNick.GetNick() + " sets mode: " + sModes +
                            " " + sArgs + " on " + Channel.GetName(),
                Channel.GetName());
    }

    void OnClientLogin() override {
        MCString msParams;
        msParams["target"] = GetNetwork()->GetCurNick();

        size_t uSize = m_Buffer.Size();
        for (unsigned int uIdx = 0; uIdx < uSize; uIdx++) {
            PutUser(m_Buffer.GetLine(uIdx, *GetClient(), msParams));
        }
        m_Buffer.Clear();
    }

    void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel,
                const CString& sMessage) override {
        Process(OpNick,
                "* " + OpNick.GetNick() + " kicked " + sKickedNick + " from " +
                    Channel.GetName() + " because [" + sMessage + "]",
                Channel.GetName());
    }

    void OnQuit(const CNick& Nick, const CString& sMessage,
                const vector<CChan*>& vChans) override {
        Process(Nick, "* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() +
                          "@" + Nick.GetHost() +
                          ") "
                          "(" +
                          sMessage + ")",
                "");
    }

    void OnJoin(const CNick& Nick, CChan& Channel) override {
        Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
                          Nick.GetHost() + ") joins " + Channel.GetName(),
                Channel.GetName());
    }

    void OnPart(const CNick& Nick, CChan& Channel,
                const CString& sMessage) override {
        Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
                          Nick.GetHost() + ") parts " + Channel.GetName() +
                          "(" + sMessage + ")",
                Channel.GetName());
    }

    void OnNick(const CNick& OldNick, const CString& sNewNick,
                const vector<CChan*>& vChans) override {
        Process(OldNick,
                "* " + OldNick.GetNick() + " is now known as " + sNewNick, "");
    }

    EModRet OnCTCPReply(CNick& Nick, CString& sMessage) override {
        Process(Nick, "* CTCP: " + Nick.GetNick() + " reply [" + sMessage + "]",
                "priv");
        return CONTINUE;
    }

    EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
        Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "]",
                "priv");
        return CONTINUE;
    }

    EModRet OnChanCTCP(CNick& Nick, CChan& Channel,
                       CString& sMessage) override {
        Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage +
                          "] to "
                          "[" +
                          Channel.GetName() + "]",
                Channel.GetName());
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        Process(Nick, "-" + Nick.GetNick() + "- " + sMessage, "priv");
        return CONTINUE;
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        Process(Nick, "-" + Nick.GetNick() + ":" + Channel.GetName() + "- " +
                          sMessage,
                Channel.GetName());
        return CONTINUE;
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        Process(Nick, "<" + Nick.GetNick() + "> " + sMessage, "priv");
        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        Process(Nick, "<" + Nick.GetNick() + ":" + Channel.GetName() + "> " +
                          sMessage,
                Channel.GetName());
        return CONTINUE;
    }

    void OnModCommand(const CString& sCommand) override {
        CString sCmdName = sCommand.Token(0);
        if (sCmdName.Equals("ADD") || sCmdName.Equals("WATCH")) {
            Watch(sCommand.Token(1), sCommand.Token(2),
                  sCommand.Token(3, true));
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
        } else if (sCmdName.Equals("SETDETACHEDCLIENTONLY")) {
            CString sTok = sCommand.Token(1);
            bool bDetachedClientOnly = sCommand.Token(2).ToBool();

            if (sTok == "*") {
                SetDetachedClientOnly(~0, bDetachedClientOnly);
            } else {
                SetDetachedClientOnly(sTok.ToUInt(), bDetachedClientOnly);
            }
        } else if (sCmdName.Equals("SETDETACHEDCHANNELONLY")) {
            CString sTok = sCommand.Token(1);
            bool bDetachedchannelOnly = sCommand.Token(2).ToBool();

            if (sTok == "*") {
                SetDetachedChannelOnly(~0, bDetachedchannelOnly);
            } else {
                SetDetachedChannelOnly(sTok.ToUInt(), bDetachedchannelOnly);
            }
        } else if (sCmdName.Equals("SETSOURCES")) {
            SetSources(sCommand.Token(1).ToUInt(), sCommand.Token(2, true));
        } else if (sCmdName.Equals("CLEAR")) {
            m_lsWatchers.clear();
            PutModule(t_s("All entries cleared."));
            Save();
        } else if (sCmdName.Equals("BUFFER")) {
            CString sCount = sCommand.Token(1);

            if (sCount.size()) {
                m_Buffer.SetLineCount(sCount.ToUInt());
            }

            PutModule(
                t_f("Buffer count is set to {1}")(m_Buffer.GetLineCount()));
        } else if (sCmdName.Equals("DEL")) {
            Remove(sCommand.Token(1).ToUInt());
        } else {
            PutModule(t_f("Unknown command: {1}")(sCmdName));
        }
    }

  private:
    void Process(const CNick& Nick, const CString& sMessage,
                 const CString& sSource) {
        set<CString> sHandledTargets;
        CIRCNetwork* pNetwork = GetNetwork();
        CChan* pChannel = pNetwork->FindChan(sSource);

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it) {
            CWatchEntry& WatchEntry = *it;

            if (pNetwork->IsUserAttached() &&
                WatchEntry.IsDetachedClientOnly()) {
                continue;
            }

            if (pChannel && !pChannel->IsDetached() &&
                WatchEntry.IsDetachedChannelOnly()) {
                continue;
            }

            if (WatchEntry.IsMatch(Nick, sMessage, sSource, pNetwork) &&
                sHandledTargets.count(WatchEntry.GetTarget()) < 1) {
                if (pNetwork->IsUserAttached()) {
                    pNetwork->PutUser(":" + WatchEntry.GetTarget() +
                                      "!watch@znc.in PRIVMSG " +
                                      pNetwork->GetCurNick() + " :" + sMessage);
                } else {
                    m_Buffer.AddLine(
                        ":" + _NAMEDFMT(WatchEntry.GetTarget()) +
                            "!watch@znc.in PRIVMSG {target} :{text}",
                        sMessage);
                }
                sHandledTargets.insert(WatchEntry.GetTarget());
            }
        }
    }

    void SetDisabled(unsigned int uIdx, bool bDisabled) {
        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDisabled(bDisabled);
            }

            PutModule(bDisabled ? t_s("Disabled all entries.")
                                : t_s("Enabled all entries."));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDisabled(bDisabled);
        if (bDisabled)
            PutModule(t_f("Id {1} disabled")(uIdx + 1));
        else
            PutModule(t_f("Id {1} enabled")(uIdx + 1));
        Save();
    }

    void SetDetachedClientOnly(unsigned int uIdx, bool bDetachedClientOnly) {
        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDetachedClientOnly(bDetachedClientOnly);
            }

            if (bDetachedClientOnly)
                PutModule(t_s("Set DetachedClientOnly for all entries to Yes"));
            else
                PutModule(t_s("Set DetachedClientOnly for all entries to No"));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDetachedClientOnly(bDetachedClientOnly);
        if (bDetachedClientOnly)
            PutModule(t_f("Id {1} set to Yes")(uIdx + 1));
        else
            PutModule(t_f("Id {1} set to No")(uIdx + 1));
        Save();
    }

    void SetDetachedChannelOnly(unsigned int uIdx, bool bDetachedChannelOnly) {
        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDetachedChannelOnly(bDetachedChannelOnly);
            }

            if (bDetachedChannelOnly)
                PutModule(
                    t_s("Set DetachedChannelOnly for all entries to Yes"));
            else
                PutModule(t_s("Set DetachedChannelOnly for all entries to No"));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDetachedChannelOnly(bDetachedChannelOnly);
        if (bDetachedChannelOnly)
            PutModule(t_f("Id {1} set to Yes")(uIdx + 1));
        else
            PutModule(t_f("Id {1} set to No")(uIdx + 1));
        Save();
    }

    void List() {
        CTable Table;
        Table.AddColumn(t_s("Id"));
        Table.AddColumn(t_s("HostMask"));
        Table.AddColumn(t_s("Target"));
        Table.AddColumn(t_s("Pattern"));
        Table.AddColumn(t_s("Sources"));
        Table.AddColumn(t_s("Off"));
        Table.AddColumn(t_s("DetachedClientOnly"));
        Table.AddColumn(t_s("DetachedChannelOnly"));

        unsigned int uIdx = 1;

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it, uIdx++) {
            CWatchEntry& WatchEntry = *it;

            Table.AddRow();
            Table.SetCell(t_s("Id"), CString(uIdx));
            Table.SetCell(t_s("HostMask"), WatchEntry.GetHostMask());
            Table.SetCell(t_s("Target"), WatchEntry.GetTarget());
            Table.SetCell(t_s("Pattern"), WatchEntry.GetPattern());
            Table.SetCell(t_s("Sources"), WatchEntry.GetSourcesStr());
            Table.SetCell(t_s("Off"),
                          (WatchEntry.IsDisabled()) ? t_s("Off") : "");
            Table.SetCell(
                t_s("DetachedClientOnly"),
                WatchEntry.IsDetachedClientOnly() ? t_s("Yes") : t_s("No"));
            Table.SetCell(
                t_s("DetachedChannelOnly"),
                WatchEntry.IsDetachedChannelOnly() ? t_s("Yes") : t_s("No"));
        }

        if (Table.size()) {
            PutModule(Table);
        } else {
            PutModule(t_s("You have no entries."));
        }
    }

    void Dump() {
        if (m_lsWatchers.empty()) {
            PutModule(t_s("You have no entries."));
            return;
        }

        PutModule("---------------");
        PutModule("/msg " + GetModNick() + " CLEAR");

        unsigned int uIdx = 1;

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it, uIdx++) {
            CWatchEntry& WatchEntry = *it;

            PutModule("/msg " + GetModNick() + " ADD " +
                      WatchEntry.GetHostMask() + " " + WatchEntry.GetTarget() +
                      " " + WatchEntry.GetPattern());

            if (WatchEntry.GetSourcesStr().size()) {
                PutModule("/msg " + GetModNick() + " SETSOURCES " +
                          CString(uIdx) + " " + WatchEntry.GetSourcesStr());
            }

            if (WatchEntry.IsDisabled()) {
                PutModule("/msg " + GetModNick() + " DISABLE " + CString(uIdx));
            }

            if (WatchEntry.IsDetachedClientOnly()) {
                PutModule("/msg " + GetModNick() + " SETDETACHEDCLIENTONLY " +
                          CString(uIdx) + " TRUE");
            }

            if (WatchEntry.IsDetachedChannelOnly()) {
                PutModule("/msg " + GetModNick() + " SETDETACHEDCHANNELONLY " +
                          CString(uIdx) + " TRUE");
            }
        }

        PutModule("---------------");
    }

    void SetSources(unsigned int uIdx, const CString& sSources) {
        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetSources(sSources);
        PutModule(t_f("Sources set for Id {1}.")(uIdx + 1));
        Save();
    }

    void Remove(unsigned int uIdx) {
        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        m_lsWatchers.erase(it);
        PutModule(t_f("Id {1} removed.")(uIdx + 1));
        Save();
    }

    void Help() {
        CTable Table;

        Table.AddColumn(t_s("Command"));
        Table.AddColumn(t_s("Description"));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Add <HostMask> [Target] [Pattern]"));
        Table.SetCell(t_s("Description"),
                      t_s("Used to add an entry to watch for."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("List"));
        Table.SetCell(t_s("Description"),
                      t_s("List all entries being watched."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Dump"));
        Table.SetCell(
            t_s("Description"),
            t_s("Dump a list of all current entries to be used later."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Del <Id>"));
        Table.SetCell(t_s("Description"),
                      t_s("Deletes Id from the list of watched entries."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Clear"));
        Table.SetCell(t_s("Description"), t_s("Delete all entries."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Enable <Id | *>"));
        Table.SetCell(t_s("Description"), t_s("Enable a disabled entry."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Disable <Id | *>"));
        Table.SetCell(t_s("Description"),
                      t_s("Disable (but don't delete) an entry."));

        Table.AddRow();
        Table.SetCell(t_s("Command"),
                      t_s("SetDetachedClientOnly <Id | *> <True | False>"));
        Table.SetCell(
            t_s("Description"),
            t_s("Enable or disable detached client only for an entry."));

        Table.AddRow();
        Table.SetCell(t_s("Command"),
                      t_s("SetDetachedChannelOnly <Id | *> <True | False>"));
        Table.SetCell(
            t_s("Description"),
            t_s("Enable or disable detached channel only for an entry."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Buffer [Count]"));
        Table.SetCell(
            t_s("Description"),
            t_s("Show/Set the amount of buffered lines while detached."));

        Table.AddRow();
        Table.SetCell(t_s("Command"),
                      t_s("SetSources <Id> [#chan priv #foo* !#bar]"));
        Table.SetCell(t_s("Description"),
                      t_s("Set the source channels that you care about."));

        Table.AddRow();
        Table.SetCell(t_s("Command"), t_s("Help"));
        Table.SetCell(t_s("Description"), t_s("This help."));

        PutModule(Table);
    }

    void Watch(const CString& sHostMask, const CString& sTarget,
               const CString& sPattern, bool bNotice = false) {
        CString sMessage;

        if (sHostMask.size()) {
            CWatchEntry WatchEntry(sHostMask, sTarget, sPattern);

            bool bExists = false;
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                if (*it == WatchEntry) {
                    sMessage = t_f("Entry for {1} already exists.")(
                        WatchEntry.GetHostMask());
                    bExists = true;
                    break;
                }
            }

            if (!bExists) {
                sMessage = t_f("Adding entry: {1} watching for [{2}] -> {3}")(
                    WatchEntry.GetHostMask(), WatchEntry.GetPattern(),
                    WatchEntry.GetTarget());
                m_lsWatchers.push_back(WatchEntry);
            }
        } else {
            sMessage = t_s("Watch: Not enough arguments.  Try Help");
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
        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it) {
            CWatchEntry& WatchEntry = *it;
            CString sSave;

            sSave = WatchEntry.GetHostMask() + "\n";
            sSave += WatchEntry.GetTarget() + "\n";
            sSave += WatchEntry.GetPattern() + "\n";
            sSave += (WatchEntry.IsDisabled() ? "disabled\n" : "enabled\n");
            sSave += CString(WatchEntry.IsDetachedClientOnly()) + "\n";
            sSave += CString(WatchEntry.IsDetachedChannelOnly()) + "\n";
            sSave += WatchEntry.GetSourcesStr();
            // Without this, loading fails if GetSourcesStr()
            // returns an empty string
            sSave += " ";

            SetNV(sSave, "", false);
        }

        SaveRegistry();
    }

    void Load() {
        // Just to make sure we don't mess up badly
        m_lsWatchers.clear();

        bool bWarn = false;

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            VCString vList;
            it->first.Split("\n", vList);

            // Backwards compatibility with the old save format
            if (vList.size() != 5 && vList.size() != 7) {
                bWarn = true;
                continue;
            }

            CWatchEntry WatchEntry(vList[0], vList[1], vList[2]);
            if (vList[3].Equals("disabled"))
                WatchEntry.SetDisabled(true);
            else
                WatchEntry.SetDisabled(false);

            // Backwards compatibility with the old save format
            if (vList.size() == 5) {
                WatchEntry.SetSources(vList[4]);
            } else {
                WatchEntry.SetDetachedClientOnly(vList[4].ToBool());
                WatchEntry.SetDetachedChannelOnly(vList[5].ToBool());
                WatchEntry.SetSources(vList[6]);
            }
            m_lsWatchers.push_back(WatchEntry);
        }

        if (bWarn)
            PutModule(t_s("WARNING: malformed entry found while loading"));
    }

    list<CWatchEntry> m_lsWatchers;
    CBuffer m_Buffer;
};

template <>
void TModInfo<CWatcherMod>(CModInfo& Info) {
    Info.SetWikiPage("watch");
}

NETWORKMODULEDEFS(
    CWatcherMod,
    t_s("Copy activity from a specific user into a separate window"))
