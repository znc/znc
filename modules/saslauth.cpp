/**
 * Copyright (C) 2008 Heiko Hund <heiko@ist.eigentlich.net>
 * Copyright (C) 2008-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * @class CSASLAuthMod
 * @author Heiko Hund <heiko@ist.eigentlich.net>
 * @brief SASL authentication module for znc.
 */

#include <znc/Modules.h>
#include <znc/znc.h>
#include <znc/User.h>

#include <sasl/sasl.h>

class CSASLAuthMod : public CModule {
public:
	MODCONSTRUCTOR(CSASLAuthMod) {
		m_Cache.SetTTL(60000/*ms*/);

		AddHelpCommand();
		AddCommand("CreateUser",       static_cast<CModCommand::ModCmdFunc>(&CSASLAuthMod::CreateUser),
			"[yes|no]");
		AddCommand("CloneUser",        static_cast<CModCommand::ModCmdFunc>(&CSASLAuthMod::CloneUser),
			"[username]");
		AddCommand("DisableCloneUser", static_cast<CModCommand::ModCmdFunc>(&CSASLAuthMod::DisableCloneUser));
	}

	virtual ~CSASLAuthMod() {
		sasl_done();
	}

	void OnModCommand(const CString& sCommand) {
		if (m_pUser->IsAdmin()) {
			HandleCommand(sCommand);
		} else {
			PutModule("Access denied");
		}
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		VCString vsArgs;
		VCString::const_iterator it;
		sArgs.Split(" ", vsArgs, false);

		for (it = vsArgs.begin(); it != vsArgs.end(); ++it) {
			if (it->Equals("saslauthd") || it->Equals("auxprop")) {
				m_sMethod += *it + " ";
			} else {
				CUtils::PrintError("Ignoring invalid SASL pwcheck method: " + *it);
				sMessage = "Ignored invalid SASL pwcheck method";
			}
		}

		m_sMethod.TrimRight();

		if (m_sMethod.empty()) {
			sMessage = "Need a pwcheck method as argument (saslauthd, auxprop)";
			return false;
		}

		if (sasl_server_init(NULL, NULL) != SASL_OK) {
			sMessage = "SASL Could Not Be Initialized - Halting Startup";
			return false;
		}

		m_cbs[0].id = SASL_CB_GETOPT;
		m_cbs[0].proc = reinterpret_cast<int(*)()>(CSASLAuthMod::getopt);
		m_cbs[0].context = this;
		m_cbs[1].id = SASL_CB_LIST_END;
		m_cbs[1].proc = NULL;
		m_cbs[1].context = NULL;

		return true;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		const CString sUsername(Auth->GetUsername());
		const CString sPassword(Auth->GetPassword());
		CUser *pUser(CZNC::Get().FindUser(sUsername));
		sasl_conn_t *sasl_conn(NULL);
		bool bSuccess = false;

		if (!pUser && !CreateUser()) {
			return CONTINUE;
		}

		const CString sCacheKey(CString(sUsername + ":" + sPassword).MD5());
		if (m_Cache.HasItem(sCacheKey)) {
			bSuccess = true;
			DEBUG("saslauth: Found [" + sUsername + "] in cache");
		} else if (sasl_server_new("znc", NULL, NULL, NULL, NULL, m_cbs, 0, &sasl_conn) == SASL_OK &&
				sasl_checkpass(sasl_conn, sUsername.c_str(), sUsername.size(), sPassword.c_str(), sPassword.size()) == SASL_OK) {
			m_Cache.AddItem(sCacheKey);

			DEBUG("saslauth: Successful SASL authentication [" + sUsername + "]");

			bSuccess = true;
		}

		sasl_dispose(&sasl_conn);

		if (bSuccess) {
			if (!pUser) {
				CString sErr;
				pUser = new CUser(sUsername);

				if (ShouldCloneUser()) {
					CUser *pBaseUser = CZNC::Get().FindUser(CloneUser());

					if (!pBaseUser) {
						DEBUG("saslauth: Clone User [" << CloneUser() << "] User not found");
						delete pUser;
						pUser = NULL;
					}

					if (!pUser->Clone(*pBaseUser, sErr)) {
						DEBUG("saslauth: Clone User [" << CloneUser() << "] " << sErr);
						delete pUser;
						pUser = NULL;
					}
				}

				if (pUser && !CZNC::Get().AddUser(pUser, sErr)) {
					DEBUG("saslauth: Add user [" << sUsername << "] " << sErr);
					delete pUser;
					pUser = NULL;
				}

				if (pUser) {
					pUser->SetPass("::", CUser::HASH_MD5, "::");
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

	void CreateUser(const CString &sLine) {
		CString sCreate = sLine.Token(1);

		if (!sCreate.empty()) {
			SetNV("CreateUser", sCreate);
		}

		if (CreateUser()) {
			PutModule("We will create users on their first login");
		} else {
			PutModule("We will not create users on their first login");
		}
	}

	void CloneUser(const CString &sLine) {
		CString sUsername = sLine.Token(1);

		if (!sUsername.empty()) {
			SetNV("CloneUser", sUsername);
		}

		if (ShouldCloneUser()) {
			PutModule("We will clone [" + CloneUser() + "]");
		} else {
			PutModule("We will not clone a user");
		}
	}

	void DisableCloneUser(const CString &sLine) {
		DelNV("CloneUser");
		PutModule("Clone user disabled");
	}

	bool CreateUser() const {
		return GetNV("CreateUser").ToBool();
	}

	CString CloneUser() const {
		return GetNV("CloneUser");
	}

	bool ShouldCloneUser() {
		return !GetNV("CloneUser").empty();
	}

protected:
	TCacheMap<CString>     m_Cache;

	sasl_callback_t m_cbs[2];
	CString m_sMethod;

	static int getopt(void *context, const char *plugin_name,
			const char *option, const char **result, unsigned *len) {
		if (CString(option).Equals("pwcheck_method")) {
			*result = ((CSASLAuthMod*)context)->GetMethod().c_str();
			return SASL_OK;
		}

		return SASL_CONTINUE;
	}
};

template<> void TModInfo<CSASLAuthMod>(CModInfo& Info) {
	Info.SetWikiPage("saslauth");
}

GLOBALMODULEDEFS(CSASLAuthMod, "Allow users to authenticate via SASL password verification method")
