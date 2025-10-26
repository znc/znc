/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

using std::map;
using std::set;
using std::vector;

class CAutoModeMod;

constexpr static int AUTOMODE_CHALLENGE_LENGTH = 32;

class CAutoModeTimer : public CTimer {
  public:
    CAutoModeTimer(CAutoModeMod* pModule)
        : CTimer((CModule*)pModule, 20, 0, "AutoModeChecker",
                 "Check channels for auto mode candidates") {
        m_pParent = pModule;
    }

    ~CAutoModeTimer() override {}

  private:
  protected:
    void RunJob() override;

    CAutoModeMod* m_pParent;
};
class CAutoModeUser {
  public:
    CAutoModeUser() {}

    CAutoModeUser(const CString& sLine) { FromString(sLine); }

    CAutoModeUser(const CString& sUsername, const CString& sMode,
                  const CString& sUserKey, const CString& sHostmasks,
                  const CString& sChannels)
        : m_sUsername(sUsername), m_sUserKey(sUserKey), m_sMode(sMode) {
        AddHostmasks(sHostmasks);
        AddChans(sChannels);
        SetMode(sMode);
    }

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

    bool HostMatches(const CString& sHostmask) const {
        for (const CString& s : m_ssHostmasks) {
            if (sHostmask.WildCmp(s, CString::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    CString GetMode() const { return m_sMode; }

    CString GetHostmasks() const {
        return CString(",").Join(m_ssHostmasks.begin(), m_ssHostmasks.end());
    }

    CString GetChannels() const {
        return CString(" ").Join(m_ssChans.begin(), m_ssChans.end());
    }

    void SetMode(const CString& sMode) { m_sMode = sMode; }

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
        return m_sUsername + "\t" + GetHostmasks() + "\t" + m_sMode + "\t" +
               m_sUserKey + "\t" + GetChannels();
    }

    bool FromString(const CString& sLine) {
        m_sUsername = sLine.Token(0, false, "\t");
        // Bug from autoop module.
        // Trim because there was a bug which caused spaces in the hostname
        sLine.Token(1, false, "\t").Trim_n().Split(",", m_ssHostmasks);
        m_sMode = sLine.Token(2, false, "\t");
        m_sUserKey = sLine.Token(3, false, "\t");
        sLine.Token(4, false, "\t").Split(" ", m_ssChans);
        return !m_sUserKey.empty();
    }

  private:
  protected:
    CString m_sUsername;
    CString m_sMode;
    CString m_sUserKey;
    set<CString> m_ssHostmasks;
    set<CString> m_ssChans;
};

class CAutoModeMod : public CModule {
  public:
    MODCONSTRUCTOR(CAutoModeMod) {
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
        AddCommand("AddModes", t_d("<user> <mode>"),
                   t_d("Add modes to an existing user"),
                   [=](const CString& sLine) { OnAddModesCommand(sLine); });
        AddCommand("DelModes", t_d("<user> <mode>"),
                   t_d("Delete modes from an existing user"),
                   [=](const CString& sLine) { OnDelModesCommand(sLine); });
        AddCommand("AddUser",
                   t_d("<user> <hostmask>[,<hostmasks>...] <mode> "
                       "<key>/<__NOKEY__> [channels]"),
                   t_d("Adds a user"),
                   [=](const CString& sLine) { OnAddUserCommand(sLine); });
        AddCommand("DelUser", t_d("<user>"), t_d("Removes a user"),
                   [=](const CString& sLine) { OnDelUserCommand(sLine); });
        AddCommand("Dump", t_d(""),
                   t_d("Dump a list of all current entries to be used later."),
                   [=](const CString& sLine) { OnDumpCommand(); });
        AddCommand("Clear", t_d(""), t_d("Delete all entries."),
                   [=](const CString& sLine) { OnClearCommand(); });
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        AddTimer(new CAutoModeTimer(this));

        // Load the users
        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            const CString& sLine = it->second;
            CAutoModeUser user;

            if (user.FromString(sLine) &&
                !FindUser(user.GetUsername().AsLower())) {
                m_msUsers[user.GetUsername().AsLower()] = user;
            }
        }

        return true;
    }
    void OnDumpCommand() {
        if (m_msUsers.empty()) {
            PutModule(t_s("You have no entries."));
            return;
        }
        PutModule("---------------");
        PutModule("/msg " + GetModNick() + " CLEAR");
        for (const auto& [_, pUser] : m_msUsers) {
            VCString vsHostmasks;
            pUser.GetHostmasks().Split(",", vsHostmasks);
            PutModule("/msg " + GetModNick() + " ADDUSER " +
                      pUser.GetUsername() + " " + pUser.GetHostmasks() + " " +
                      pUser.GetMode() + " " + pUser.GetUserKey() + " " +
                      pUser.GetChannels());
        }
        PutModule("---------------");
    }
    void OnClearCommand() {
        m_msUsers.clear();
        PutModule(t_s("All entries cleared."));
        ClearNV(true);
    }

    ~CAutoModeMod() override { m_msUsers.clear(); }

    void OnJoinMessage(CJoinMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        if (Channel.HasPerm(CChan::Op)) {
            CheckAutoMode(Nick, Channel);
        }
    }

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        MCString::iterator it = m_msQueue.find(Nick.GetNick().AsLower());

        if (it != m_msQueue.end()) {
            m_msQueue.erase(it);
        }
    }

    void OnNickMessage(CNickMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& OldNick = Message.GetNick();
        const CString sNewNick = Message.GetNewNick();
        MCString::iterator it = m_msQueue.find(OldNick.GetNick().AsLower());

        if (it != m_msQueue.end()) {
            m_msQueue[sNewNick.AsLower()] = it->second;
            m_msQueue.erase(it);
        }
    }

    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override {
        CString sMessage = Message.GetText();
        CNick& Nick = Message.GetNick();

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
                    CheckAutoMode(it.second, Channel);
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
        CString sMode = sLine.Token(3);
        CString sKey = sLine.Token(4);

        if (sHost.empty()) {
            PutModule(
                t_s("Usage: AddUser <user> <hostmask>[,<hostmasks>...] <mode> "
                    "<key>/<__NOKEY__> [channels]"));
        } else {
            CAutoModeUser* pUser =
                AddUser(sUser, sMode, sKey, sHost, sLine.Token(5, true));

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
        Table.AddColumn(t_s("Mode"));
        Table.AddColumn(t_s("Key"));
        Table.AddColumn(t_s("Channels"));

        for (const auto& [_, pUser] : m_msUsers) {
            VCString vsHostmasks;
            pUser.GetHostmasks().Split(",", vsHostmasks);
            for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
                Table.AddRow();
                if (a == 0) {
                    Table.SetCell(t_s("User"), pUser.GetUsername());
                    Table.SetCell(t_s("Mode"), pUser.GetMode());
                    Table.SetCell(t_s("Key"), pUser.GetUserKey());
                    Table.SetCell(t_s("Channels"), pUser.GetChannels());
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

        CAutoModeUser* pUser = FindUser(sUser);

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

        CAutoModeUser* pUser = FindUser(sUser);

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
        CString sHostmasks = sLine.Token(2);

        if (sHostmasks.empty()) {
            PutModule(t_s("Usage: AddMasks <user> <mask>,[mask] ..."));
            return;
        }

        CAutoModeUser* pUser = FindUser(sUser);

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
        CString sHostmasks = sLine.Token(2);

        if (sHostmasks.empty()) {
            PutModule(t_s("Usage: DelMasks <user> <mask>,[mask] ..."));
            return;
        }

        CAutoModeUser* pUser = FindUser(sUser);

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

    void OnAddModesCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sAddModes = sLine.Token(2);

        if (sUser.empty() || sAddModes.empty()) {
            PutModule(t_s("Usage: ADDMODES <user> <modes>"));
            return;
        }

        auto it = m_msUsers.find(sUser);
        if (it == m_msUsers.end()) {
            PutModule(t_s("User not found: " + sUser));
            return;
        }

        CAutoModeUser* pUser = FindUser(sUser);

        CString sCurrentModes = pUser->GetMode();

        for (char c : sAddModes) {
            CString cs(c);  // Convert char to CString
            if (!sCurrentModes.Contains(cs)) {
                sCurrentModes += c;
            }
        }

        pUser->SetMode(sCurrentModes);
        PutModule(t_s("Updated modes for [" + sUser + "]: " + sCurrentModes));
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    void OnDelModesCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sDelModes = sLine.Token(2);

        if (sUser.empty() || sDelModes.empty()) {
            PutModule(t_s("Usage: DELMODES <user> <modes>"));
            return;
        }

        auto it = m_msUsers.find(sUser);
        if (it == m_msUsers.end()) {
            PutModule(t_s("User not found: " + sUser));
            return;
        }

        CAutoModeUser* pUser = FindUser(sUser);

        CString sCurrentModes = pUser->GetMode();
        CString sNewModes;

        for (char c : sCurrentModes) {
            if (!sDelModes.Contains(CString(c))) {
                sNewModes += c;
            }
        }

        pUser->SetMode(sNewModes);
        PutModule(t_s("Updated modes for [" + sUser + "]: " + sNewModes));
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    CAutoModeUser* FindUser(const CString& sUser) {
        auto it = m_msUsers.find(sUser.AsLower());
        return (it != m_msUsers.end()) ? &it->second : nullptr;
    }

    CAutoModeUser* FindUserByHost(const CString& sHostmask,
                                  const CString& sChannel = "") {
        for (const auto& it : m_msUsers) {
            const CAutoModeUser* pUser = &it.second;

            if (pUser->HostMatches(sHostmask) &&
                (sChannel.empty() || pUser->ChannelMatches(sChannel))) {
                return const_cast<CAutoModeUser*>(pUser);
            }
        }
        return nullptr;
    }

    bool CheckAutoMode(const CNick& Nick, CChan& Channel) {
        CAutoModeUser* pUser =
            FindUserByHost(Nick.GetHostMask(), Channel.GetName());

        if (!pUser) {
            return false;
        }

        if (pUser->GetUserKey().Equals("__NOKEY__")) {
            CString sModes = pUser->GetMode();
            CString sNickArgs;
            for (unsigned int i = 0; i < sModes.length(); ++i) {
                sNickArgs += Nick.GetNick() + " ";
            }
            sNickArgs.TrimRight();
            PutIRC("MODE " + Channel.GetName() + " +" + sModes + " " +
                   sNickArgs);
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
        auto it = m_msUsers.find(sUser.AsLower());

        if (it == m_msUsers.end()) {
            PutModule(t_s("No such user"));
            return;
        }

        m_msUsers.erase(it);
        PutModule(t_f("User {1} removed")(sUser));
    }

    CAutoModeUser* AddUser(const CString& sUser, const CString& sMode,
                           const CString& sKey, const CString& sHosts,
                           const CString& sChans) {
        if (m_msUsers.find(sUser) != m_msUsers.end()) {
            PutModule(t_s("That user already exists"));
            return nullptr;
        }

        CAutoModeUser user(sUser, sMode, sKey, sHosts, sChans);
        m_msUsers[sUser.AsLower()] = user;
        PutModule(t_f("User {1} added with hostmask(s) {2}")(sUser, sHosts));
        return &m_msUsers[sUser.AsLower()];
    }

    bool ChallengeRespond(const CNick& Nick, const CString& sChallenge) {
        // Validate before responding - don't blindly trust everyone
        bool bValid = false;
        bool bMatchedHost = false;
        CAutoModeUser* pUser = nullptr;

        for (const auto& [_, pUser] : m_msUsers) {
            // First verify that the person who challenged us matches a user's
            // host
            if (pUser.HostMatches(Nick.GetHostMask())) {
                const vector<CChan*>& Chans = GetNetwork()->GetChans();
                bMatchedHost = true;

                // Also verify that they are opped in at least one of the user's
                // chans
                for (CChan* pChan : Chans) {
                    const CNick* pNick = pChan->FindNick(Nick.GetNick());

                    if (pNick) {
                        if (pNick->HasPerm(CChan::Op) &&
                            pUser.ChannelMatches(pChan->GetName())) {
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

        if (sChallenge.length() != AUTOMODE_CHALLENGE_LENGTH) {
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
            if (it.second.HostMatches(Nick.GetHostMask())) {
                if (sResponse ==
                    CString(it.second.GetUserKey() + "::" + sChallenge).MD5()) {
                    ModeUser(Nick, it.second);
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
            it.second = CString::RandomString(AUTOMODE_CHALLENGE_LENGTH);
            PutIRC("NOTICE " + it.first + " :!ZNCAO CHALLENGE " + it.second);
        }
    }

    void ModeUser(const CNick& Nick, const CAutoModeUser& User) {
        const vector<CChan*>& Chans = GetNetwork()->GetChans();

        for (CChan* pChan : Chans) {
            if (pChan->HasPerm(CChan::Op) &&
                User.ChannelMatches(pChan->GetName())) {
                const CNick* pNick = pChan->FindNick(Nick.GetNick());

                if (pNick && !pNick->HasPerm(CChan::Op)) {
                    CString sModes = User.GetMode();
                    if (sModes.empty()) continue;

                    CString sNickArgs;
                    for (unsigned int i = 0; i < sModes.size(); ++i)
                        sNickArgs += Nick.GetNick() + " ";

                    sNickArgs.TrimRight();
                    PutIRC("MODE " + pChan->GetName() + " +" + sModes + " " +
                           sNickArgs);
                }
            }
        }
    }

  private:
    map<CString, CAutoModeUser> m_msUsers;
    MCString m_msQueue;
};

void CAutoModeTimer::RunJob() { m_pParent->ProcessQueue(); }

template <>
void TModInfo<CAutoModeMod>(CModInfo& Info) {
    Info.SetWikiPage("automode");
}

NETWORKMODULEDEFS(
    CAutoModeMod,
    t_s("Automatically owner/admin/op/halfop/voice the good people"))
