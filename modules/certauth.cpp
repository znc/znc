/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#define REQUIRESSL

#include <znc/User.h>
#include <znc/znc.h>

using std::map;
using std::vector;
using std::set;
using std::pair;

class CSSLClientCertMod : public CModule {
public:
	MODCONSTRUCTOR(CSSLClientCertMod) {
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

	bool OnBoot() override {
		const vector<CListener*>& vListeners = CZNC::Get().GetListeners();
		vector<CListener*>::const_iterator it;

		// We need the SSL_VERIFY_PEER flag on all listeners, or else
		// the client doesn't send a ssl cert
		for (it = vListeners.begin(); it != vListeners.end(); ++it)
			(*it)->GetRealListener()->SetRequireClientCertFlags(SSL_VERIFY_PEER);

		for (MCString::const_iterator it1 = BeginNV(); it1 != EndNV(); ++it1) {
			VCString vsKeys;

			if (CZNC::Get().FindUser(it1->first) == NULL) {
				DEBUG("Unknown user in saved data [" + it1->first + "]");
				continue;
			}

			it1->second.Split(" ", vsKeys, false);
			for (VCString::const_iterator it2 = vsKeys.begin(); it2 != vsKeys.end(); ++it2) {
				m_PubKeys[it1->first].insert(it2->AsLower());
			}
		}

		return true;
	}

	void OnPostRehash() override {
		OnBoot();
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		OnBoot();

		return true;
	}

	bool Save() {
		ClearNV(false);
		for (MSCString::const_iterator it = m_PubKeys.begin(); it != m_PubKeys.end(); ++it) {
			CString sVal;
			for (SCString::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
				sVal += *it2 + " ";
			}

			if (!sVal.empty())
				SetNV(it->first, sVal, false);
		}

		return SaveRegistry();
	}

	bool AddKey(CUser *pUser, const CString& sKey) {
		const pair<SCString::const_iterator, bool> pair
			= m_PubKeys[pUser->GetUserName()].insert(sKey.AsLower());

		if (pair.second) {
			Save();
		}

		return pair.second;
	}

	EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
		const CString sUser = Auth->GetUsername();
		Csock *pSock = Auth->GetSocket();
		CUser *pUser = CZNC::Get().FindUser(sUser);

		if (pSock == NULL || pUser == NULL)
			return CONTINUE;

		const CString sPubKey = GetKey(pSock);
		DEBUG("User: " << sUser << " Key: " << sPubKey);

		if (sPubKey.empty()) {
			DEBUG("Peer got no public key, ignoring");
			return CONTINUE;
		}

		MSCString::const_iterator it = m_PubKeys.find(sUser);
		if (it == m_PubKeys.end()) {
			DEBUG("No saved pubkeys for this client");
			return CONTINUE;
		}

		SCString::const_iterator it2 = it->second.find(sPubKey);
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
		const CString sPubKey = GetKey(GetClient());

		if (sPubKey.empty()) {
			PutModule("You are not connected with any valid public key");
		} else {
			PutModule("Your current public key is: " + sPubKey);
		}
	}

	void HandleAddCommand(const CString& sLine) {
		CString sPubKey = sLine.Token(1);

		if (sPubKey.empty()) {
			sPubKey = GetKey(GetClient());
		}

		if (sPubKey.empty()) {
			PutModule("You did not supply a public key or connect with one.");
		} else {
			if (AddKey(GetUser(), sPubKey)) {
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

		MSCString::const_iterator it = m_PubKeys.find(GetUser()->GetUserName());
		if (it == m_PubKeys.end()) {
			PutModule("No keys set for your user");
			return;
		}

		unsigned int id = 1;
		for (SCString::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
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
		MSCString::iterator it = m_PubKeys.find(GetUser()->GetUserName());

		if (it == m_PubKeys.end()) {
			PutModule("No keys set for your user");
			return;
		}

		if (id == 0 || id > it->second.size()) {
			PutModule("Invalid #, check \"list\"");
			return;
		}

		SCString::const_iterator it2 = it->second.begin();
		while (id > 1) {
			++it2;
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
		long int res = pSock->GetPeerFingerprint(sRes);

		DEBUG("GetKey() returned status " << res << " with key " << sRes);

		// This is 'inspired' by charybdis' libratbox
		switch (res) {
		case X509_V_OK:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			return sRes.AsLower();
		default:
			return "";
		}
	}

	CString GetWebMenuTitle() override { return "certauth"; }

	bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) override {
		CUser *pUser = WebSock.GetSession()->GetUser();

		if (sPageName == "index") {
			MSCString::const_iterator it = m_PubKeys.find(pUser->GetUserName());
			if (it != m_PubKeys.end()) {
				for (SCString::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
					CTemplate& row = Tmpl.AddRow("KeyLoop");
					row["Key"] = *it2;
				}
			}

			return true;
		} else if (sPageName == "add") {
			AddKey(pUser, WebSock.GetParam("key"));
			WebSock.Redirect(GetWebPath());
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

			WebSock.Redirect(GetWebPath());
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

GLOBALMODULEDEFS(CSSLClientCertMod, "Allow users to authenticate via SSL client certificates.")
