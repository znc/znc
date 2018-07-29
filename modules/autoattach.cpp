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

#include <string>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;
using std::to_string;

class CAttachMatch {
  public:
    CAttachMatch(CModule* pModule, const CString& sChannels,
                 const CString& sSearch, const CString& sHostmasks,
                 bool bNegated) {
        m_pModule = pModule;
        m_sChannelWildcard = sChannels;
        m_sSearchWildcard = sSearch;
        m_sHostmaskWildcard = sHostmasks;
        m_bNegated = bNegated;

        if (m_sChannelWildcard.empty()) m_sChannelWildcard = "*";
        if (m_sSearchWildcard.empty()) m_sSearchWildcard = "*";
        if (m_sHostmaskWildcard.empty()) m_sHostmaskWildcard = "*!*@*";
    }

    bool IsMatch(const CString& sChan, const CString& sHost,
                 const CString& sMessage) const {
        if (!sHost.WildCmp(m_sHostmaskWildcard, CString::CaseInsensitive))
            return false;
        if (!sChan.WildCmp(m_sChannelWildcard, CString::CaseInsensitive))
            return false;
        if (!sMessage.WildCmp(m_pModule->ExpandString(m_sSearchWildcard),
                              CString::CaseInsensitive))
            return false;
        return true;
    }

    bool IsNegated() const { return m_bNegated; }

    const CString& GetHostMask() const { return m_sHostmaskWildcard; }

    const CString& GetSearch() const { return m_sSearchWildcard; }

    const CString& GetChans() const { return m_sChannelWildcard; }

    CString ToString() {
        CString sRes;
        if (m_bNegated) sRes += "!";
        sRes += m_sChannelWildcard;
        sRes += " ";
        sRes += m_sSearchWildcard;
        sRes += " ";
        sRes += m_sHostmaskWildcard;
        return sRes;
    }

  private:
    bool m_bNegated;
    CModule* m_pModule;
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
            PutModule(t_s("Added to list"));
        } else {
            PutModule(t_f("{1} is already added")(sLine.Token(1, true)));
            bHelp = true;
        }
        if (bHelp) {
            PutModule(t_s("Usage: Add [!]<#chan> <search> <host>"));
            PutModule(t_s("Wildcards are allowed"));
        }
    }

    void HandleDel(const CString& sLine) {
        CString sMsg = sLine.Token(1, true);
        bool bNegated = sMsg.TrimPrefix("!");
        CString sChan = sMsg.Token(0);
        CString sSearch = sMsg.Token(1);
        CString sHost = sMsg.Token(2);

        if (Del(bNegated, sChan, sSearch, sHost)) {
            PutModule(t_f("Removed {1} from list")(sChan));
        } else {
            PutModule(t_s("Usage: Del [!]<#chan> <search> <host>"));
        }
    }

    void HandleSwap(const CString& sLine) {
        u_int iA = sLine.Token(1).ToUInt();
        u_int iB = sLine.Token(2).ToUInt();

        Swap(iA, iB);
    }

    void HandleList(const CString& sLine) {
        int iIndex;
        CTable Table;
        Table.AddColumn(t_s("Index"));
        Table.AddColumn(t_s("Neg"));
        Table.AddColumn(t_s("Chan"));
        Table.AddColumn(t_s("Search"));
        Table.AddColumn(t_s("Host"));

        iIndex = 1;
        VAttachIter it = m_vMatches.begin();
        for (; it != m_vMatches.end(); ++it) {
            Table.AddRow();
            Table.SetCell(t_s("Index"), to_string(iIndex));
            Table.SetCell(t_s("Neg"), it->IsNegated() ? "!" : "");
            Table.SetCell(t_s("Chan"), it->GetChans());
            Table.SetCell(t_s("Search"), it->GetSearch());
            Table.SetCell(t_s("Host"), it->GetHostMask());
            iIndex += 1;
        }

        if (Table.size()) {
            PutModule(Table);
        } else {
            PutModule(t_s("You have no entries."));
        }
    }

  public:
    MODCONSTRUCTOR(CChanAttach) {
        AddHelpCommand();
        AddCommand(
            "Add", t_d("[!]<#chan> <search> <host>"),
            t_d("Add an entry, use !#chan to negate and * for wildcards"),
            [=](const CString& sLine) { HandleAdd(sLine); });
        AddCommand("Del", t_d("[!]<#chan> <search> <host>"),
                   t_d("Remove an entry, needs to be an exact match"),
                   [=](const CString& sLine) { HandleDel(sLine); });
        AddCommand("List", "", t_d("List all entries"),
                   [=](const CString& sLine) { HandleList(sLine); });
        AddCommand("Swap", t_d("<a> <b>"), t_d("Swap a and b entries in the list."),
                   [=](const CString& sLine) { HandleSwap(sLine); });
    }

    ~CChanAttach() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        CString migrated = GetNV("migrated");

        if (migrated != "1") {
            MigrateFromOldNV();
        }

        VCString vsChans;

        GetNV("Autoattach").Split("\n", vsChans, false);

        for (const CString& sRule : vsChans) {
            if (!AddFromString(sRule)) {
                PutModule(t_f("Unable to add [{1}]")(sRule));
            }
        }

        return true;
    }

    void TryAttach(const CNick& Nick, CChan& Channel, CString& Message) {
        const CString& sChan = Channel.GetName();
        const CString& sHost = Nick.GetHostMask();
        const CString& sMessage = Message;
        VAttachIter it;

        if (!Channel.IsDetached()) return;

        // Any negated match?
        for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
            if (it->IsNegated() && it->IsMatch(sChan, sHost, sMessage)) return;
        }

        // Now check for a positive match
        for (it = m_vMatches.begin(); it != m_vMatches.end(); ++it) {
            if (!it->IsNegated() && it->IsMatch(sChan, sHost, sMessage)) {
                Channel.AttachUser();
                return;
            }
        }
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        TryAttach(Nick, Channel, sMessage);
        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        TryAttach(Nick, Channel, sMessage);
        return CONTINUE;
    }

    EModRet OnChanAction(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        TryAttach(Nick, Channel, sMessage);
        return CONTINUE;
    }

    VAttachIter FindEntry(const CString& sChan, const CString& sSearch,
                          const CString& sHost) {
        VAttachIter it = m_vMatches.begin();
        for (; it != m_vMatches.end(); ++it) {
            if (sHost.empty() || it->GetHostMask() != sHost) continue;
            if (sSearch.empty() || it->GetSearch() != sSearch) continue;
            if (sChan.empty() || it->GetChans() != sChan) continue;
            return it;
        }
        return m_vMatches.end();
    }

    bool AddFromString(const CString& sRule) {
        CString sAdd = sRule;
        bool bNegated = sAdd.TrimPrefix("!");
        CString sChan = sAdd.Token(0);
        CString sSearch = sAdd.Token(1);
        CString sHost = sAdd.Token(2, true);

        return Add(bNegated, sChan, sSearch, sHost);
    }

    bool Add(bool bNegated, const CString& sChan, const CString& sSearch,
             const CString& sHost) {
        CAttachMatch attach(this, sChan, sSearch, sHost, bNegated);

        // Check for duplicates
        VAttachIter it = m_vMatches.begin();
        for (; it != m_vMatches.end(); ++it) {
            if (it->GetHostMask() == attach.GetHostMask() &&
                it->GetChans() == attach.GetChans() &&
                it->GetSearch() == attach.GetSearch())
                return false;
        }

        m_vMatches.push_back(attach);

        Save();
        return true;
    }

    bool Del(bool bNegated, const CString& sChan, const CString& sSearch,
             const CString& sHost) {
        VAttachIter it = FindEntry(sChan, sSearch, sHost);
        if (it == m_vMatches.end() || it->IsNegated() != bNegated) return false;

        m_vMatches.erase(it);

        Save();

        return true;
    }

    bool Swap(int Start, int End) {
        // CAttachMatch
        if (Start > m_vMatches.size() || Start <= 0 ||
            End > m_vMatches.size() || End <= 0) {
            PutModule(t_s("Illegal # Requested"));
            return false;
        } else {
            std::iter_swap(m_vMatches.begin() + (Start - 1),
                           m_vMatches.begin() + (End - 1));
            PutModule(t_s("Rules Swapped."));
            Save();
            return true;
        }
    }

  private:
    void Save() {
        CString sBuffer = "";

        for (CAttachMatch& cRule : m_vMatches) {
            sBuffer += cRule.ToString() + "\n";
        }

        SetNV("Autoattach", sBuffer);
    }

    void MigrateFromOldNV() {
        VCString rules;
        // Load our saved settings, ignore errors
        MCString::iterator it;
        for (it = BeginNV(); it != EndNV(); ++it) {
            CString sAdd = it->first;

            rules.push_back(sAdd);
        }

        for (const CString& sRule : rules) {
            AddFromString(sRule);
            DelNV(sRule);
        }

        SetNV("migrated", "1");

        Save();
    }

    VAttachMatch m_vMatches;
};

template <>
void TModInfo<CChanAttach>(CModInfo& Info) {
    Info.AddType(CModInfo::UserModule);
    Info.SetWikiPage("autoattach");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s(
        "List of channel masks and channel masks with ! before them."));
}

NETWORKMODULEDEFS(CChanAttach, t_s("Reattaches you to channels on activity."))
