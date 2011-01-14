/**
 * Copyright (C) 2008 Heiko Hund <heiko@ist.eigentlich.net>
 * Copyright (C) 2008-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * @class CSASLAuthMod
 * @author Heiko Hund <heiko@ist.eigentlich.net>
 * @brief SASL authentication module for znc.
 */

#include "Modules.h"
#include "znc.h"
#include <sasl/sasl.h>

class CSASLAuthMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CSASLAuthMod) {
		m_Cache.SetTTL(60000/*ms*/);
	}
	virtual ~CSASLAuthMod() {}

	virtual bool OnBoot() {
		return true;
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		VCString vsChans;
		VCString::const_iterator it;
		sArgs.Split(" ", vsChans, false);

		for (it = vsChans.begin(); it != vsChans.end(); ++it) {
			if (it->StrCmp("saslauthd") || it->StrCmp("auxprop")) {
				method += *it + " ";
			}
			else {
				CUtils::PrintError("Ignoring invalid SASL pwcheck method: " + *it);
				sMessage = "Ignored invalid SASL pwcheck method";
			}
		}
		method.TrimRight();

		if (method.empty()) {
			sMessage = "Need a pwcheck method as argument (saslauthd, auxprop)";
			return false;
		}

		if (sasl_server_init(NULL, NULL) != SASL_OK) {
			sMessage = "SASL Could Not Be Initialized - Halting Startup";
			return false;
		}

		return true;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		CString const user(Auth->GetUsername());
		CString const pass(Auth->GetPassword());
		CUser* pUser(CZNC::Get().FindUser(user));
		sasl_conn_t *sasl_conn(0);

		if (!pUser) { // @todo Will want to do some sort of && !m_bAllowCreate in the future
			Auth->RefuseLogin("Invalid User - Halting SASL Authentication");
			return HALT;
		}

		CString const key(CString(user + ":" + pass).MD5());
		if (m_Cache.HasItem(key)) {
			Auth->AcceptLogin(*pUser);
			DEBUG("+++ Found in cache");
		}
		else if (sasl_server_new("znc", NULL, NULL, NULL, NULL, cbs, 0, &sasl_conn) == SASL_OK &&
		         sasl_checkpass(sasl_conn, user.c_str(), user.size(), pass.c_str(), pass.size()) == SASL_OK) {
			Auth->AcceptLogin(*pUser);
			m_Cache.AddItem(key);
			DEBUG("+++ Successful SASL password check");
		}
		else {
			Auth->RefuseLogin("SASL Authentication failed");
			DEBUG("--- FAILED SASL password check");
		}

		sasl_dispose(&sasl_conn);
		return HALT;
	}

private:
	TCacheMap<CString>     m_Cache;

	static sasl_callback_t cbs[];
	static CString         method;

	static int getopt(void *context, const char *plugin_name, const char *option, const char **result, unsigned *len) {
		if (!method.empty() && strcmp(option, "pwcheck_method") == 0) {
			*result = method.c_str();
			return SASL_OK;
		}
		return SASL_CONTINUE;
	}
};

sasl_callback_t CSASLAuthMod::cbs[] = {
	{ SASL_CB_GETOPT, reinterpret_cast<int(*)()>(CSASLAuthMod::getopt), NULL },
	{ SASL_CB_LIST_END, NULL, NULL }
};

CString CSASLAuthMod::method;

GLOBALMODULEDEFS(CSASLAuthMod, "Allow users to authenticate via SASL password verification method")
