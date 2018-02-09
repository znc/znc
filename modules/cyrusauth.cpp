/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 * Copyright (C) 2008 Heiko Hund <heiko@ist.eigentlich.net>
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

/**
 * @class CSASLAuthMod
 * @author Heiko Hund <heiko@ist.eigentlich.net>
 * @brief SASL authentication module for znc.
 */

#include <znc/znc.h>
#include <znc/User.h>

#include <sasl/sasl.h>

class CSASLAuthMod : public CModule {
  public:
    MODCONSTRUCTOR(CSASLAuthMod) {
        m_Cache.SetTTL(60000 /*ms*/);

        m_cbs[0].id = SASL_CB_GETOPT;
        m_cbs[0].proc = reinterpret_cast<int (*)()>(CSASLAuthMod::getopt);
        m_cbs[0].context = this;
        m_cbs[1].id = SASL_CB_LIST_END;
        m_cbs[1].proc = nullptr;
        m_cbs[1].context = nullptr;

        AddHelpCommand();
        AddCommand("Show", "", t_d("Shows current settings"),
                   [=](const CString& sLine) { ShowCommand(sLine); });
        AddCommand("CreateUsers", t_d("yes|clone <username>|no"),
                   t_d("Create ZNC users upon first successful login, "
                       "optionally from a template"),
                   [=](const CString& sLine) { CreateUsersCommand(sLine); });
    }

    ~CSASLAuthMod() override { sasl_done(); }

    void OnModCommand(const CString& sCommand) override {
        if (GetUser()->IsAdmin()) {
            HandleCommand(sCommand);
        } else {
            PutModule(t_s("Access denied"));
        }
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        VCString vsArgs;
        VCString::const_iterator it;
        sArgs.Split(" ", vsArgs, false);

        for (it = vsArgs.begin(); it != vsArgs.end(); ++it) {
            if (it->Equals("saslauthd") || it->Equals("auxprop")) {
                m_sMethod += *it + " ";
            } else {
                CUtils::PrintError(
                    t_f("Ignoring invalid SASL pwcheck method: {1}")(*it));
                sMessage = t_s("Ignored invalid SASL pwcheck method");
            }
        }

        m_sMethod.TrimRight();

        if (m_sMethod.empty()) {
            sMessage =
                t_s("Need a pwcheck method as argument (saslauthd, auxprop)");
            return false;
        }

        if (sasl_server_init(nullptr, nullptr) != SASL_OK) {
            sMessage = t_s("SASL Could Not Be Initialized - Halting Startup");
            return false;
        }

        return true;
    }

    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
        const CString& sUsername = Auth->GetUsername();
        const CString& sPassword = Auth->GetPassword();
        CUser* pUser(CZNC::Get().FindUser(sUsername));
        sasl_conn_t* sasl_conn(nullptr);
        bool bSuccess = false;

        if (!pUser && !CreateUser()) {
            return CONTINUE;
        }

        const CString sCacheKey(CString(sUsername + ":" + sPassword).MD5());
        if (m_Cache.HasItem(sCacheKey)) {
            bSuccess = true;
            DEBUG("saslauth: Found [" + sUsername + "] in cache");
        } else if (sasl_server_new("znc", nullptr, nullptr, nullptr, nullptr,
                                   m_cbs, 0, &sasl_conn) == SASL_OK &&
                   sasl_checkpass(sasl_conn, sUsername.c_str(),
                                  sUsername.size(), sPassword.c_str(),
                                  sPassword.size()) == SASL_OK) {
            m_Cache.AddItem(sCacheKey);

            DEBUG("saslauth: Successful SASL authentication [" + sUsername +
                  "]");

            bSuccess = true;
        }

        sasl_dispose(&sasl_conn);

        if (bSuccess) {
            if (!pUser) {
                CString sErr;
                pUser = new CUser(sUsername);

                if (ShouldCloneUser()) {
                    CUser* pBaseUser = CZNC::Get().FindUser(CloneUser());

                    if (!pBaseUser) {
                        DEBUG("saslauth: Clone User [" << CloneUser()
                                                       << "] User not found");
                        delete pUser;
                        pUser = nullptr;
                    }

                    if (pUser && !pUser->Clone(*pBaseUser, sErr)) {
                        DEBUG("saslauth: Clone User [" << CloneUser()
                                                       << "] failed: " << sErr);
                        delete pUser;
                        pUser = nullptr;
                    }
                }

                if (pUser) {
                    // "::" is an invalid MD5 hash, so user won't be able to
                    // login by usual method
                    pUser->SetPass("::", CUser::HASH_MD5, "::");
                }

                if (pUser && !CZNC::Get().AddUser(pUser, sErr)) {
                    DEBUG("saslauth: Add user [" << sUsername
                                                 << "] failed: " << sErr);
                    delete pUser;
                    pUser = nullptr;
                }
            }

            if (pUser) {
                Auth->AcceptLogin(*pUser);
                return HALT;
            }
        }

        return CONTINUE;
    }

    const CString& GetMethod() const { return m_sMethod; }

    void ShowCommand(const CString& sLine) {
        if (!CreateUser()) {
            PutModule(t_s("We will not create users on their first login"));
        } else if (ShouldCloneUser()) {
            PutModule(
                t_f("We will create users on their first login, using user "
                    "[{1}] as a template")(CloneUser()));
        } else {
            PutModule(t_s("We will create users on their first login"));
        }
    }

    void CreateUsersCommand(const CString& sLine) {
        CString sCreate = sLine.Token(1);
        if (sCreate == "no") {
            DelNV("CloneUser");
            SetNV("CreateUser", CString(false));
            PutModule(t_s("We will not create users on their first login"));
        } else if (sCreate == "yes") {
            DelNV("CloneUser");
            SetNV("CreateUser", CString(true));
            PutModule(t_s("We will create users on their first login"));
        } else if (sCreate == "clone" && !sLine.Token(2).empty()) {
            SetNV("CloneUser", sLine.Token(2));
            SetNV("CreateUser", CString(true));
            PutModule(
                t_f("We will create users on their first login, using user "
                    "[{1}] as a template")(sLine.Token(2)));
        } else {
            PutModule(
                t_s("Usage: CreateUsers yes, CreateUsers no, or CreateUsers "
                    "clone <username>"));
        }
    }

    bool CreateUser() const { return GetNV("CreateUser").ToBool(); }

    CString CloneUser() const { return GetNV("CloneUser"); }

    bool ShouldCloneUser() { return !GetNV("CloneUser").empty(); }

  protected:
    TCacheMap<CString> m_Cache;

    sasl_callback_t m_cbs[2];
    CString m_sMethod;

    static int getopt(void* context, const char* plugin_name,
                      const char* option, const char** result, unsigned* len) {
        if (CString(option).Equals("pwcheck_method")) {
            *result = ((CSASLAuthMod*)context)->GetMethod().c_str();
            return SASL_OK;
        }

        return SASL_CONTINUE;
    }
};

template <>
void TModInfo<CSASLAuthMod>(CModInfo& Info) {
    Info.SetWikiPage("cyrusauth");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s(
        "This global module takes up to two arguments - the methods of "
        "authentication - auxprop and saslauthd"));
}

GLOBALMODULEDEFS(
    CSASLAuthMod,
    t_s("Allow users to authenticate via SASL password verification method"))
