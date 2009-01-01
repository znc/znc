/**
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
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
		int i(0);
		CString arg(sArgs.Token(i));

		while (!arg.empty()) {
			if (arg.StrCmp("saslauthd") || arg.StrCmp("auxprop")) {
				method += arg + " ";
			}
			else {
				CUtils::PrintError("Ignoring invalid SASL pwcheck method: " + arg);
			}
			arg = sArgs.Token(++i);
		}
		method.TrimRight();

		if (sasl_server_init(NULL, NULL) != SASL_OK){
			CUtils::PrintError("SASL Could Not Be Initialized - Halting Startup");
			return false;
		}

		return true;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		CString const user(Auth->GetUsername());
		CString const pass(Auth->GetPassword());
		CUser* pUser(CZNC::Get().FindUser(user));
		sasl_conn_t *sasl_conn(0);

		if (!pUser) {	// @todo Will want to do some sort of && !m_bAllowCreate in the future
			Auth->RefuseLogin("Invalid User - Halting SASL Authentication");
			return HALT;
		}

		CString const key(CString(user + ":" + pass).MD5());
		if (m_Cache.HasItem(key)) {
			Auth->AcceptLogin(*pUser);
			DEBUG_ONLY(cerr << "+++ Found in cache" << endl);
		}
		else if (sasl_server_new("znc", NULL, NULL, NULL, NULL, cbs, 0, &sasl_conn) == SASL_OK &&
		         sasl_checkpass(sasl_conn, user.c_str(), user.size(), pass.c_str(), pass.size()) == SASL_OK) {
			Auth->AcceptLogin(*pUser);
			m_Cache.AddItem(key);
			DEBUG_ONLY(cerr << "+++ Successful SASL password check" << endl);
		}
		else {
			Auth->RefuseLogin("SASL Authentication failed");
			DEBUG_ONLY(cerr << "--- FAILED SASL password check" << endl);
		}

		sasl_dispose(&sasl_conn);
		return HALT;
	}

private:
	TCacheMap<CString>	m_Cache;

	static sasl_callback_t	cbs[];
	static CString		method;

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
