/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#define REQUIRESSL

#include "Modules.h"
#include "User.h"
#include "Listener.h"
#include "znc.h"

class CSSLClientCertMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CSSLClientCertMod) {
		AddHelpCommand();
		AddCommand("Add",  static_cast<CModCommand::ModCmdFunc>(&CSSLClientCertMod::HandleAddCommand),
			"[pubkey]", "If pubkey is not provided will use the current key");
		AddCommand("Del",  static_cast<CModCommand::ModCmdFunc>(&CSSLClientCertMod::HandleDelCommand),
			"id");
		AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CSSLClientCertMod::HandleListCommand));
		AddCommand("Show", static_cast<CModCommand::ModCmdFunc>(&CSSLClientCertMod::HandleShowCommand),
			"", "Print your current key");
	}

	virtual ~CSSLClientCertMod() {}

	virtual bool OnBoot() {
		const vector<CListener*>& vListeners = CZNC::Get().GetListeners();
		vector<CListener*>::const_iterator it;

		// We need the SSL_VERIFY_PEER flag on all listeners, or else
		// the client doesn't send a ssl cert
		for (it = vListeners.begin(); it != vListeners.end(); it++)
			(*it)->GetRealListener()->SetRequireClientCertFlags(SSL_VERIFY_PEER);

		MCString::iterator it1;
		for (it1 = BeginNV(); it1 != EndNV(); it1++) {
			VCString vsKeys;
			VCString::iterator it2;

			if (CZNC::Get().FindUser(it1->first) == NULL) {
				DEBUG("Unknown user in saved data [" + it1->first + "]");
				continue;
			}

			it1->second.Split(" ", vsKeys, false);
			for (it2 = vsKeys.begin(); it2 != vsKeys.end(); it2++) {
				m_PubKeys[it1->first].insert(*it2);
			}
		}

		return true;
	}

	virtual void OnPostRehash() {
		OnBoot();
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		OnBoot();

		return true;
	}

	bool Save() {
		MSCString::iterator it;

		ClearNV(false);
		for (it = m_PubKeys.begin(); it != m_PubKeys.end(); it++) {
			CString sVal;
			SCString::iterator it2;
			for (it2 = it->second.begin(); it2 != it->second.end(); it2++) {
				sVal += *it2 + " ";
			}

			if (!sVal.empty())
				SetNV(it->first, sVal, false);
		}

		return SaveRegistry();
	}

	bool AddKey(CUser *pUser, CString sKey) {
		pair<SCString::iterator, bool> pair = m_PubKeys[pUser->GetUserName()].insert(sKey);

		if (pair.second) {
			Save();
		}

		return pair.second;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		CString sUser = Auth->GetUsername();
		Csock *pSock = Auth->GetSocket();
		CUser *pUser = CZNC::Get().FindUser(sUser);

		if (pSock == NULL || pUser == NULL)
			return CONTINUE;

		CString sPubKey = GetKey(pSock);
		DEBUG("User: " << sUser << " Key: " << sPubKey);

		if (sPubKey.empty()) {
			DEBUG("Peer got no public key, ignoring");
			return CONTINUE;
		}

		MSCString::iterator it = m_PubKeys.find(sUser);
		if (it == m_PubKeys.end()) {
			DEBUG("No saved pubkeys for this client");
			return CONTINUE;
		}

		SCString::iterator it2 = it->second.find(sPubKey);
		if (it2 == it->second.end()) {
			DEBUG("Invalid pubkey");
			return CONTINUE;
		}

		// This client uses a valid pubkey for this user, let them in
		DEBUG("Accepted pubkey auth");
		Auth->AcceptLogin(*pUser);

		return HALT;
	}

	void HandleShowCommand(const CString& sLine) {
		CString sPubKey = GetKey(m_pClient);

		if (sPubKey.empty()) {
			PutModule("You are not connected with any valid public key");
		} else {
			PutModule("Your current public key is: " + sPubKey);
		}
	}

	void HandleAddCommand(const CString& sLine) {
		CString sPubKey = sLine.Token(1);

		if (sPubKey.empty()) {
			sPubKey = GetKey(m_pClient);
		}

		if (sPubKey.empty()) {
			PutModule("You did not supply a public key or connect with one.");
		} else {
			if (AddKey(m_pUser, sPubKey)) {
				PutModule("'" + sPubKey + "' added.");
			} else {
				PutModule("The key '" + sPubKey + "' is already added.");
			}
		}
	}

	void HandleListCommand(const CString& sLine) {
		CTable Table;

		Table.AddColumn("Id");
		Table.AddColumn("Key");

		MSCString::iterator it = m_PubKeys.find(m_pUser->GetUserName());
		if (it == m_PubKeys.end()) {
			PutModule("No keys set for your user");
			return;
		}

		SCString::iterator it2;
		unsigned int id = 1;
		for (it2 = it->second.begin(); it2 != it->second.end(); it2++) {
			Table.AddRow();
			Table.SetCell("Id", CString(id++));
			Table.SetCell("Key", *it2);
		}

		if (PutModule(Table) == 0) {
			// This double check is necessary, because the
			// set could be empty.
			PutModule("No keys set for your user");
		}
	}

	void HandleDelCommand(const CString& sLine) {
		unsigned int id = sLine.Token(1, true).ToUInt();
		MSCString::iterator it = m_PubKeys.find(m_pUser->GetUserName());

		if (it == m_PubKeys.end()) {
			PutModule("No keys set for your user");
			return;
		}

		if (id == 0 || id > it->second.size()) {
			PutModule("Invalid #, check \"list\"");
			return;
		}

		SCString::iterator it2 = it->second.begin();
		while (id > 1) {
			it2++;
			id--;
		}

		it->second.erase(it2);
		if (it->second.size() == 0)
			m_PubKeys.erase(it);
		PutModule("Removed");

		Save();
	}

	CString GetKey(Csock *pSock) {
		CString sRes;
		int res = pSock->GetPeerFingerprint(sRes);

		DEBUG("GetKey() returned status " << res << " with key " << sRes);

		// This is 'inspired' by charybdis' libratbox
		switch (res) {
		case X509_V_OK:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			return sRes;
		default:
			return "";
		}
	}

	virtual CString GetWebMenuTitle() { return "certauth"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		CUser *pUser = WebSock.GetSession()->GetUser();

		if (sPageName == "index") {
			MSCString::iterator it = m_PubKeys.find(pUser->GetUserName());
			if (it != m_PubKeys.end()) {
				SCString::iterator it2;

				for (it2 = it->second.begin(); it2 != it->second.end(); it2++) {
					CTemplate& row = Tmpl.AddRow("KeyLoop");
					row["Key"] = *it2;
				}
			}

			return true;
		} else if (sPageName == "add") {
			AddKey(pUser, WebSock.GetParam("key"));
			WebSock.Redirect("/mods/certauth/");
			return true;
		} else if (sPageName == "delete") {
			MSCString::iterator it = m_PubKeys.find(pUser->GetUserName());
			if (it != m_PubKeys.end()) {
				if (it->second.erase(WebSock.GetParam("key", false))) {
					if (it->second.size() == 0) {
						m_PubKeys.erase(it);
					}

					Save();
				}
			}

			WebSock.Redirect("/mods/certauth/");
			return true;
		}

		return false;
	}

private:
	// Maps user names to a list of allowed pubkeys
	typedef map<CString, set<CString> > MSCString;
	MSCString                           m_PubKeys;
};

template<> void TModInfo<CSSLClientCertMod>(CModInfo& Info) {
	Info.SetWikiPage("certauth");
}

GLOBALMODULEDEFS(CSSLClientCertMod, "Allow users to authenticate via SSL client certificates")
