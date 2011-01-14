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

class CSSLClientCertMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CSSLClientCertMod) {}
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

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmd = sCommand.Token(0);

		if (sCmd.Equals("show")) {
			CString sPubKey = GetKey(m_pClient);
			if (sPubKey.empty())
				PutModule("You are not connected with any valid public key");
			else
				PutModule("Your current public key is: " + sPubKey);
		} else if (sCmd.Equals("add")) {
			CString sPubKey = GetKey(m_pClient);
			if (sPubKey.empty())
				PutModule("You are not connected with any valid public key");
			else {
				pair<SCString::iterator, bool> res = m_PubKeys[m_pUser->GetUserName()].insert(sPubKey);
				if (res.second) {
					PutModule("Added your current public key to the list");
					Save();
				} else
					PutModule("Your key was already added");
			}
		} else if (sCmd.Equals("list")) {
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

			if (PutModule(Table) == 0)
				// This double check is necessary, because the
				// set could be empty.
				PutModule("No keys set for your user");
		} else if (sCmd.Equals("del") || sCmd.Equals("remove")) {
			unsigned int id = sCommand.Token(1, true).ToUInt();
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
		} else {
			PutModule("Commands: show, list, add, del [no]");
		}
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

private:
	// Maps user names to a list of allowed pubkeys
	typedef map<CString, set<CString> > MSCString;
	MSCString                           m_PubKeys;
};

GLOBALMODULEDEFS(CSSLClientCertMod, "Allow users to authenticate via SSL client certificates")
