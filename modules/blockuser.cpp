/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

using std::vector;

#define MESSAGE \
    t_s("Your account has been disabled. Contact your administrator.")

class CBlockUser : public CModule {
  public:
    MODCONSTRUCTOR(CBlockUser) {
        AddHelpCommand();
        AddCommand("List", "", t_d("List blocked users"),
                   [this](const CString& sLine) { OnListCommand(sLine); });
        AddCommand("Block", t_d("<user>"), t_d("Block a user"),
                   [this](const CString& sLine) { OnBlockCommand(sLine); });
        AddCommand("Unblock", t_d("<user>"), t_d("Unblock a user"),
                   [this](const CString& sLine) { OnUnblockCommand(sLine); });
    }

    ~CBlockUser() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        VCString vArgs;
        VCString::iterator it;
        MCString::iterator it2;

        // Load saved settings
        for (it2 = BeginNV(); it2 != EndNV(); ++it2) {
            // Ignore errors
            Block(it2->first);
        }

        // Parse arguments, each argument is a user name to block
        sArgs.Split(" ", vArgs, false);

        for (it = vArgs.begin(); it != vArgs.end(); ++it) {
            if (!Block(*it)) {
                sMessage = t_f("Could not block {1}")(*it);
                return false;
            }
        }

        return true;
    }

    /* If a user is on the blocked list and tries to log in, displays - MESSAGE 
    and stops their log in attempt.*/
    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
        if (IsBlocked(Auth->GetUsername())) {
            Auth->RefuseLogin(MESSAGE);
            return HALT;
        }

        return CONTINUE;
    }

    void OnModCommand(const CString& sCommand) override {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
        } else {
            HandleCommand(sCommand);
        }
    }

    // Displays all blocked users as a list.
    void OnListCommand(const CString& sCommand) {
        if (BeginNV() == EndNV()) {
            PutModule(t_s("No users are blocked"));
            return;
        }
        PutModule(t_s("Blocked users:"));
        for (auto it = BeginNV(); it != EndNV(); ++it) {
            PutModule(it->first);
        }
    }

    /* Blocks a user if possible(ie not self, not already blocked).
    Displays an error message if not possible. */
    void OnBlockCommand(const CString& sCommand) {
        CString sUser = sCommand.Token(1, true);

        if (sUser.empty()) {
            PutModule(t_s("Usage: Block <user>"));
            return;
        }

        if (GetUser()->GetUserName().Equals(sUser)) {
            PutModule(t_s("You can't block yourself"));
            return;
        }

        if (Block(sUser))
            PutModule(t_f("Blocked {1}")(sUser));
        else
            PutModule(t_f("Could not block {1} (misspelled?)")(sUser));
    }

    // Unblocks a user from the blocked list.
    void OnUnblockCommand(const CString& sCommand) {
        CString sUser = sCommand.Token(1, true);

        if (sUser.empty()) {
            PutModule(t_s("Usage: Unblock <user>"));
            return;
        }

        if (DelNV(sUser))
            PutModule(t_f("Unblocked {1}")(sUser));
        else
            PutModule(t_s("This user is not blocked"));
    }

    // Provides GUI to configure this module by adding a widget to user page in webadmin.
    bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName,
                              CTemplate& Tmpl) override {
        if (sPageName == "webadmin/user" && WebSock.GetSession()->IsAdmin()) {
            CString sAction = Tmpl["WebadminAction"];
            if (sAction == "display") {
                Tmpl["Blocked"] = CString(IsBlocked(Tmpl["Username"]));
                Tmpl["Self"] = CString(Tmpl["Username"].Equals(
                    WebSock.GetSession()->GetUser()->GetUserName()));
                return true;
            }
            if (sAction == "change" &&
                WebSock.GetParam("embed_blockuser_presented").ToBool()) {
                if (Tmpl["Username"].Equals(
                        WebSock.GetSession()->GetUser()->GetUserName()) &&
                    WebSock.GetParam("embed_blockuser_block").ToBool()) {
                    WebSock.GetSession()->AddError(
                        t_s("You can't block yourself"));
                } else if (WebSock.GetParam("embed_blockuser_block").ToBool()) {
                    if (!WebSock.GetParam("embed_blockuser_old").ToBool()) {
                        if (Block(Tmpl["Username"])) {
                            WebSock.GetSession()->AddSuccess(
                                t_f("Blocked {1}")(Tmpl["Username"]));
                        } else {
                            WebSock.GetSession()->AddError(
                                t_f("Couldn't block {1}")(Tmpl["Username"]));
                        }
                    }
                } else if (WebSock.GetParam("embed_blockuser_old").ToBool()) {
                    if (DelNV(Tmpl["Username"])) {
                        WebSock.GetSession()->AddSuccess(
                            t_f("Unblocked {1}")(Tmpl["Username"]));
                    } else {
                        WebSock.GetSession()->AddError(
                            t_f("User {1} is not blocked")(Tmpl["Username"]));
                    }
                }
                return true;
            }
        }
        return false;
    }

  private:
    /* Iterates through all blocked users and returns true if the specified user (sUser)
    is blocked, else returns false.*/
    bool IsBlocked(const CString& sUser) {
        MCString::iterator it;
        for (it = BeginNV(); it != EndNV(); ++it) {
            if (sUser == it->first) {
                return true;
            }
        }
        return false;
    }

    bool Block(const CString& sUser) {
        CUser* pUser = CZNC::Get().FindUser(sUser);

        if (!pUser) return false;

        // Disconnect all clients
        vector<CClient*> vpClients = pUser->GetAllClients();
        vector<CClient*>::iterator it;
        for (it = vpClients.begin(); it != vpClients.end(); ++it) {
            (*it)->PutStatusNotice(MESSAGE);
            (*it)->Close(Csock::CLT_AFTERWRITE);
        }

        // Disconnect all networks from irc
        vector<CIRCNetwork*> vNetworks = pUser->GetNetworks();
        for (vector<CIRCNetwork*>::iterator it2 = vNetworks.begin();
             it2 != vNetworks.end(); ++it2) {
            (*it2)->SetIRCConnectEnabled(false);
        }

        SetNV(pUser->GetUserName(), "");
        return true;
    }
};

template <>
void TModInfo<CBlockUser>(CModInfo& Info) {
    Info.SetWikiPage("blockuser");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("Enter one or more user names. Separate them by spaces."));
}

GLOBALMODULEDEFS(CBlockUser, t_s("Block certain users from logging in."))
