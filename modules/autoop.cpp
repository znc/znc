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

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>

using std::map;
using std::set;
using std::vector;

class CAutoOpMod;

#define AUTOOP_CHALLENGE_LENGTH 32

class CAutoOpTimer : public CTimer {
  public:
    CAutoOpTimer(CAutoOpMod* pModule)
        : CTimer((CModule*)pModule, 20, 0, "AutoOpChecker",
                 "Check channels for auto op candidates") {
        m_pParent = pModule;
    }

    ~CAutoOpTimer() override {}

  private:
  protected:
    void RunJob() override;

    CAutoOpMod* m_pParent;
};

class CAutoOpUser {
  public:
    CAutoOpUser() {}

    CAutoOpUser(const CString& sLine) { FromString(sLine); }

    CAutoOpUser(const CString& sUsername, const CString& sUserKey,
                const CString& sHostmasks, const CString& sChannels)
        : m_sUsername(sUsername), m_sUserKey(sUserKey) {
        AddHostmasks(sHostmasks);
        AddChans(sChannels);
    }

    virtual ~CAutoOpUser() {}

    const CString& GetUsername() const { return m_sUsername; }
    const CString& GetUserKey() const { return m_sUserKey; }

    bool ChannelMatches(const CString& sChan) const {
        for (const CString& s : m_ssChans) {
            if (sChan.AsLower().WildCmp(s, CString::CaseInsensitive)) {
                return true;
            }
        }

        return false;
    }

    bool HostMatches(const CString& sHostmask) {
        for (const CString& s : m_ssHostmasks) {
            if (sHostmask.WildCmp(s, CString::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    CString GetHostmasks() const {
        return CString(",").Join(m_ssHostmasks.begin(), m_ssHostmasks.end());
    }

    CString GetChannels() const {
        return CString(" ").Join(m_ssChans.begin(), m_ssChans.end());
    }

    bool DelHostmasks(const CString& sHostmasks) {
        VCString vsHostmasks;
        sHostmasks.Split(",", vsHostmasks);

        for (const CString& s : vsHostmasks) {
            m_ssHostmasks.erase(s);
        }

        return m_ssHostmasks.empty();
    }

    void AddHostmasks(const CString& sHostmasks) {
        VCString vsHostmasks;
        sHostmasks.Split(",", vsHostmasks);

        for (const CString& s : vsHostmasks) {
            m_ssHostmasks.insert(s);
        }
    }

    void DelChans(const CString& sChans) {
        VCString vsChans;
        sChans.Split(" ", vsChans);

        for (const CString& sChan : vsChans) {
            m_ssChans.erase(sChan.AsLower());
        }
    }

    void AddChans(const CString& sChans) {
        VCString vsChans;
        sChans.Split(" ", vsChans);

        for (const CString& sChan : vsChans) {
            m_ssChans.insert(sChan.AsLower());
        }
    }

    CString ToString() const {
        return m_sUsername + "\t" + GetHostmasks() + "\t" + m_sUserKey + "\t" +
               GetChannels();
    }

    bool FromString(const CString& sLine) {
        m_sUsername = sLine.Token(0, false, "\t");
        sLine.Token(1, false, "\t").Split(",", m_ssHostmasks);
        m_sUserKey = sLine.Token(2, false, "\t");
        sLine.Token(3, false, "\t").Split(" ", m_ssChans);

        return !m_sUserKey.empty();
    }

  private:
  protected:
    CString m_sUsername;
    CString m_sUserKey;
    set<CString> m_ssHostmasks;
    set<CString> m_ssChans;
};

class CAutoOpMod : public CModule {
  public:
    MODCONSTRUCTOR(CAutoOpMod) {
        AddHelpCommand();
        AddCommand("ListUsers", "", t_d("List all users"),
                   [=](const CString& sLine) { OnListUsersCommand(sLine); });
        AddCommand("AddChans", t_d("<user> <channel> [channel] ..."),
                   t_d("Adds channels to a user"),
                   [=](const CString& sLine) { OnAddChansCommand(sLine); });
        AddCommand("DelChans", t_d("<user> <channel> [channel] ..."),
                   t_d("Removes channels from a user"),
                   [=](const CString& sLine) { OnDelChansCommand(sLine); });
        AddCommand("AddMasks", t_d("<user> <mask>,[mask] ..."),
                   t_d("Adds masks to a user"),
                   [=](const CString& sLine) { OnAddMasksCommand(sLine); });
        AddCommand("DelMasks", t_d("<user> <mask>,[mask] ..."),
                   t_d("Removes masks from a user"),
                   [=](const CString& sLine) { OnDelMasksCommand(sLine); });
        AddCommand("AddUser",
                   t_d("<user> <hostmask>[,<hostmasks>...] <key> [channels]"),
                   t_d("Adds a user"),
                   [=](const CString& sLine) { OnAddUserCommand(sLine); });
        AddCommand("DelUser", t_d("<user>"), t_d("Removes a user"),
                   [=](const CString& sLine) { OnDelUserCommand(sLine); });
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        AddTimer(new CAutoOpTimer(this));

        // Load the users
        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            const CString& sLine = it->second;
            CAutoOpUser* pUser = new CAutoOpUser;

            if (!pUser->FromString(sLine) ||
                FindUser(pUser->GetUsername().AsLower())) {
                delete pUser;
            } else {
                m_msUsers[pUser->GetUsername().AsLower()] = pUser;
            }
        }

        return true;
    }

    ~CAutoOpMod() override {
        for (const auto& it : m_msUsers) {
            delete it.second;
        }
        m_msUsers.clear();
    }

    void OnJoin(const CNick& Nick, CChan& Channel) override {
        // If we have ops in this chan
        if (Channel.HasPerm(CChan::Op)) {
            CheckAutoOp(Nick, Channel);
        }
    }

    void OnQuit(const CNick& Nick, const CString& sMessage,
                const vector<CChan*>& vChans) override {
        MCString::iterator it = m_msQueue.find(Nick.GetNick().AsLower());

        if (it != m_msQueue.end()) {
            m_msQueue.erase(it);
        }
    }

    void OnNick(const CNick& OldNick, const CString& sNewNick,
                const vector<CChan*>& vChans) override {
        // Update the queue with nick changes
        MCString::iterator it = m_msQueue.find(OldNick.GetNick().AsLower());

        if (it != m_msQueue.end()) {
            m_msQueue[sNewNick.AsLower()] = it->second;
            m_msQueue.erase(it);
        }
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        if (!sMessage.Token(0).Equals("!ZNCAO")) {
            return CONTINUE;
        }

        CString sCommand = sMessage.Token(1);

        if (sCommand.Equals("CHALLENGE")) {
            ChallengeRespond(Nick, sMessage.Token(2));
        } else if (sCommand.Equals("RESPONSE")) {
            VerifyResponse(Nick, sMessage.Token(2));
        }

        return HALTCORE;
    }

    void OnOp2(const CNick* pOpNick, const CNick& Nick, CChan& Channel,
               bool bNoChange) override {
        if (Nick.GetNick() == GetNetwork()->GetIRCNick().GetNick()) {
            const map<CString, CNick>& msNicks = Channel.GetNicks();

            for (const auto& it : msNicks) {
                if (!it.second.HasPerm(CChan::Op)) {
                    CheckAutoOp(it.second, Channel);
                }
            }
        }
    }

    void OnModCommand(const CString& sLine) override {
        CString sCommand = sLine.Token(0).AsUpper();
        if (sCommand.Equals("TIMERS")) {
            // for testing purposes - hidden from help
            ListTimers();
        } else {
            HandleCommand(sLine);
        }
    }

    void OnAddUserCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHost = sLine.Token(2);
        CString sKey = sLine.Token(3);

        if (sHost.empty()) {
            PutModule(
                t_s("Usage: AddUser <user> <hostmask>[,<hostmasks>...] <key> "
                    "[channels]"));
        } else {
            CAutoOpUser* pUser =
                AddUser(sUser, sKey, sHost, sLine.Token(4, true));

            if (pUser) {
                SetNV(sUser, pUser->ToString());
            }
        }
    }

    void OnDelUserCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);

        if (sUser.empty()) {
            PutModule(t_s("Usage: DelUser <user>"));
        } else {
            DelUser(sUser);
            DelNV(sUser);
        }
    }

    void OnListUsersCommand(const CString& sLine) {
        if (m_msUsers.empty()) {
            PutModule(t_s("There are no users defined"));
            return;
        }

        CTable Table;

        Table.AddColumn(t_s("User"));
        Table.AddColumn(t_s("Hostmasks"));
        Table.AddColumn(t_s("Key"));
        Table.AddColumn(t_s("Channels"));

        for (const auto& it : m_msUsers) {
            VCString vsHostmasks;
            it.second->GetHostmasks().Split(",", vsHostmasks);
            for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
                Table.AddRow();
                if (a == 0) {
                    Table.SetCell(t_s("User"), it.second->GetUsername());
                    Table.SetCell(t_s("Key"), it.second->GetUserKey());
                    Table.SetCell(t_s("Channels"), it.second->GetChannels());
                } else if (a == vsHostmasks.size() - 1) {
                    Table.SetCell(t_s("User"), "`-");
                } else {
                    Table.SetCell(t_s("User"), "|-");
                }
                Table.SetCell(t_s("Hostmasks"), vsHostmasks[a]);
            }
        }

        PutModule(Table);
    }

    void OnAddChansCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sChans = sLine.Token(2, true);

        if (sChans.empty()) {
            PutModule(t_s("Usage: AddChans <user> <channel> [channel] ..."));
            return;
        }

        CAutoOpUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule(t_s("No such user"));
            return;
        }

        pUser->AddChans(sChans);
        PutModule(t_f("Channel(s) added to user {1}")(pUser->GetUsername()));
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    void OnDelChansCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sChans = sLine.Token(2, true);

        if (sChans.empty()) {
            PutModule(t_s("Usage: DelChans <user> <channel> [channel] ..."));
            return;
        }

        CAutoOpUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule(t_s("No such user"));
            return;
        }

        pUser->DelChans(sChans);
        PutModule(
            t_f("Channel(s) Removed from user {1}")(pUser->GetUsername()));
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    void OnAddMasksCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHostmasks = sLine.Token(2, true);

        if (sHostmasks.empty()) {
            PutModule(t_s("Usage: AddMasks <user> <mask>,[mask] ..."));
            return;
        }

        CAutoOpUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule(t_s("No such user"));
            return;
        }

        pUser->AddHostmasks(sHostmasks);
        PutModule(t_f("Hostmasks(s) added to user {1}")(pUser->GetUsername()));
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    void OnDelMasksCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHostmasks = sLine.Token(2, true);

        if (sHostmasks.empty()) {
            PutModule(t_s("Usage: DelMasks <user> <mask>,[mask] ..."));
            return;
        }

        CAutoOpUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule(t_s("No such user"));
            return;
        }

        if (pUser->DelHostmasks(sHostmasks)) {
            PutModule(t_f("Removed user {1} with key {2} and channels {3}")(
                pUser->GetUsername(), pUser->GetUserKey(),
                pUser->GetChannels()));
            DelUser(sUser);
            DelNV(sUser);
        } else {
            PutModule(t_f("Hostmasks(s) Removed from user {1}")(
                pUser->GetUsername()));
            SetNV(pUser->GetUsername(), pUser->ToString());
        }
    }

    CAutoOpUser* FindUser(const CString& sUser) {
        map<CString, CAutoOpUser*>::iterator it =
            m_msUsers.find(sUser.AsLower());

        return (it != m_msUsers.end()) ? it->second : nullptr;
    }

    CAutoOpUser* FindUserByHost(const CString& sHostmask,
                                const CString& sChannel = "") {
        for (const auto& it : m_msUsers) {
            CAutoOpUser* pUser = it.second;

            if (pUser->HostMatches(sHostmask) &&
                (sChannel.empty() || pUser->ChannelMatches(sChannel))) {
                return pUser;
            }
        }

        return nullptr;
    }

    bool CheckAutoOp(const CNick& Nick, CChan& Channel) {
        CAutoOpUser* pUser =
            FindUserByHost(Nick.GetHostMask(), Channel.GetName());

        if (!pUser) {
            return false;
        }

        if (pUser->GetUserKey().Equals("__NOKEY__")) {
            PutIRC("MODE " + Channel.GetName() + " +o " + Nick.GetNick());
        } else {
            // then insert this nick into the queue, the timer does the rest
            CString sNick = Nick.GetNick().AsLower();
            if (m_msQueue.find(sNick) == m_msQueue.end()) {
                m_msQueue[sNick] = "";
            }
        }

        return true;
    }

    void DelUser(const CString& sUser) {
        map<CString, CAutoOpUser*>::iterator it =
            m_msUsers.find(sUser.AsLower());

        if (it == m_msUsers.end()) {
            PutModule(t_s("No such user"));
            return;
        }

        delete it->second;
        m_msUsers.erase(it);
        PutModule(t_f("User {1} removed")(sUser));
    }

    CAutoOpUser* AddUser(const CString& sUser, const CString& sKey,
                         const CString& sHosts, const CString& sChans) {
        if (m_msUsers.find(sUser) != m_msUsers.end()) {
            PutModule(t_s("That user already exists"));
            return nullptr;
        }

        CAutoOpUser* pUser = new CAutoOpUser(sUser, sKey, sHosts, sChans);
        m_msUsers[sUser.AsLower()] = pUser;
        PutModule(t_f("User {1} added with hostmask(s) {2}")(sUser, sHosts));
        return pUser;
    }

    bool ChallengeRespond(const CNick& Nick, const CString& sChallenge) {
        // Validate before responding - don't blindly trust everyone
        bool bValid = false;
        bool bMatchedHost = false;
        CAutoOpUser* pUser = nullptr;

        for (const auto& it : m_msUsers) {
            pUser = it.second;

            // First verify that the person who challenged us matches a user's
            // host
            if (pUser->HostMatches(Nick.GetHostMask())) {
                const vector<CChan*>& Chans = GetNetwork()->GetChans();
                bMatchedHost = true;

                // Also verify that they are opped in at least one of the user's
                // chans
                for (CChan* pChan : Chans) {
                    const CNick* pNick = pChan->FindNick(Nick.GetNick());

                    if (pNick) {
                        if (pNick->HasPerm(CChan::Op) &&
                            pUser->ChannelMatches(pChan->GetName())) {
                            bValid = true;
                            break;
                        }
                    }
                }

                if (bValid) {
                    break;
                }
            }
        }

        if (!bValid) {
            if (bMatchedHost) {
                PutModule(t_f(
                    "[{1}] sent us a challenge but they are not opped in any "
                    "defined channels.")(Nick.GetHostMask()));
            } else {
                PutModule(
                    t_f("[{1}] sent us a challenge but they do not match a "
                        "defined user.")(Nick.GetHostMask()));
            }

            return false;
        }

        if (sChallenge.length() != AUTOOP_CHALLENGE_LENGTH) {
            PutModule(t_f("WARNING! [{1}] sent an invalid challenge.")(
                Nick.GetHostMask()));
            return false;
        }

        CString sResponse = pUser->GetUserKey() + "::" + sChallenge;
        PutIRC("NOTICE " + Nick.GetNick() + " :!ZNCAO RESPONSE " +
               sResponse.MD5());
        return false;
    }

    bool VerifyResponse(const CNick& Nick, const CString& sResponse) {
        MCString::iterator itQueue = m_msQueue.find(Nick.GetNick().AsLower());

        if (itQueue == m_msQueue.end()) {
            PutModule(
                t_f("[{1}] sent an unchallenged response.  This could be due "
                    "to lag.")(Nick.GetHostMask()));
            return false;
        }

        CString sChallenge = itQueue->second;
        m_msQueue.erase(itQueue);

        for (const auto& it : m_msUsers) {
            if (it.second->HostMatches(Nick.GetHostMask())) {
                if (sResponse ==
                    CString(it.second->GetUserKey() + "::" + sChallenge)
                        .MD5()) {
                    OpUser(Nick, *it.second);
                    return true;
                } else {
                    PutModule(
                        t_f("WARNING! [{1}] sent a bad response.  Please "
                            "verify that you have their correct password.")(
                            Nick.GetHostMask()));
                    return false;
                }
            }
        }

        PutModule(
            t_f("WARNING! [{1}] sent a response but did not match any defined "
                "users.")(Nick.GetHostMask()));
        return false;
    }

    void ProcessQueue() {
        bool bRemoved = true;

        // First remove any stale challenges

        while (bRemoved) {
            bRemoved = false;

            for (MCString::iterator it = m_msQueue.begin();
                 it != m_msQueue.end(); ++it) {
                if (!it->second.empty()) {
                    m_msQueue.erase(it);
                    bRemoved = true;
                    break;
                }
            }
        }

        // Now issue challenges for the new users in the queue
        for (auto& it : m_msQueue) {
            it.second = CString::RandomString(AUTOOP_CHALLENGE_LENGTH);
            PutIRC("NOTICE " + it.first + " :!ZNCAO CHALLENGE " + it.second);
        }
    }

    void OpUser(const CNick& Nick, const CAutoOpUser& User) {
        const vector<CChan*>& Chans = GetNetwork()->GetChans();

        for (CChan* pChan : Chans) {
            if (pChan->HasPerm(CChan::Op) &&
                User.ChannelMatches(pChan->GetName())) {
                const CNick* pNick = pChan->FindNick(Nick.GetNick());

                if (pNick && !pNick->HasPerm(CChan::Op)) {
                    PutIRC("MODE " + pChan->GetName() + " +o " +
                           Nick.GetNick());
                }
            }
        }
    }

  private:
    map<CString, CAutoOpUser*> m_msUsers;
    MCString m_msQueue;
};

void CAutoOpTimer::RunJob() { m_pParent->ProcessQueue(); }

template <>
void TModInfo<CAutoOpMod>(CModInfo& Info) {
    Info.SetWikiPage("autoop");
}

NETWORKMODULEDEFS(CAutoOpMod, t_s("Auto op the good people"))
