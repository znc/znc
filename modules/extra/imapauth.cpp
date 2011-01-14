/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "znc.h"

using std::map;

class CIMAPAuthMod;

class CIMAPSock : public CSocket {
public:
	CIMAPSock(CIMAPAuthMod* pModule, CSmartPtr<CAuthBase> Auth)
		: CSocket((CModule*) pModule), m_spAuth(Auth) {
			m_pIMAPMod = pModule;
			m_bSentReply = false;
			m_bSentLogin = false;
			EnableReadLine();
	}

	virtual ~CIMAPSock() {
		if (!m_bSentReply) {
			m_spAuth->RefuseLogin("IMAP server is down, please try again later");
		}
	}

	virtual void ReadLine(const CString& sLine);
private:
protected:
	CIMAPAuthMod*        m_pIMAPMod;
	bool                 m_bSentLogin;
	bool                 m_bSentReply;
	CSmartPtr<CAuthBase> m_spAuth;
};


class CIMAPAuthMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CIMAPAuthMod) {
		m_Cache.SetTTL(60000);
		m_sServer = "localhost";
		m_uPort = 143;
		m_bSSL = false;
	}

	virtual ~CIMAPAuthMod() {}

	virtual bool OnBoot() {
		return true;
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (sArgs.Trim_n().empty()) {
			return true; // use defaults
		}

		m_sServer = sArgs.Token(0);
		CString sPort = sArgs.Token(1);
		m_sUserFormat = sArgs.Token(2);

		if (sPort.Left(1) == "+") {
			m_bSSL = true;
			sPort.LeftChomp();
		}

		unsigned short uPort = sPort.ToUShort();

		if (uPort) {
			m_uPort = uPort;
		}

		return true;
	}

	virtual EModRet OnLoginAttempt(CSmartPtr<CAuthBase> Auth) {
		CUser* pUser = CZNC::Get().FindUser(Auth->GetUsername());

		if (!pUser) { // @todo Will want to do some sort of && !m_bAllowCreate in the future
			Auth->RefuseLogin("Invalid User - Halting IMAP Lookup");
			return HALT;
		}

		if (pUser && m_Cache.HasItem(CString(Auth->GetUsername() + ":" + Auth->GetPassword()).MD5())) {
			DEBUG("+++ Found in cache");
			Auth->AcceptLogin(*pUser);
			return HALT;
		}

		CIMAPSock* pSock = new CIMAPSock(this, Auth);
		pSock->Connect(m_sServer, m_uPort, m_bSSL, 20);

		return HALT;
	}

	virtual void OnModCommand(const CString& sLine) {
	}

	void CacheLogin(const CString& sLogin) {
		m_Cache.AddItem(sLogin);
	}

	// Getters
	const CString& GetUserFormat() const { return m_sUserFormat; }
	// !Getters
private:
	// Settings
	CString         m_sServer;
	unsigned short  m_uPort;
	bool            m_bSSL;
	CString         m_sUserFormat;
	// !Settings

	TCacheMap<CString> m_Cache;
};

void CIMAPSock::ReadLine(const CString& sLine) {
	if (!m_bSentLogin) {
		CString sUsername = m_spAuth->GetUsername();
		m_bSentLogin = true;

		const CString& sFormat = m_pIMAPMod->GetUserFormat();

		if (!sFormat.empty()) {
			if (sFormat.find('%') != CString::npos) {
				sUsername = sFormat.Replace_n("%", sUsername);
			} else {
				sUsername += sFormat;
			}
		}

		Write("AUTH LOGIN " + sUsername + " " + m_spAuth->GetPassword() + "\r\n");
	} else {
		CUser* pUser = CZNC::Get().FindUser(m_spAuth->GetUsername());

		if (pUser && sLine.Equals("AUTH OK", false, 7)) {
			m_spAuth->AcceptLogin(*pUser);
			m_pIMAPMod->CacheLogin(CString(m_spAuth->GetUsername() + ":" + m_spAuth->GetPassword()).MD5()); // Use MD5 so passes don't sit in memory in plain text
			DEBUG("+++ Successful IMAP lookup");
		} else {
			m_spAuth->RefuseLogin("Invalid Password");
			DEBUG("--- FAILED IMAP lookup");
		}

		m_bSentReply = true;
		Close();
	}
}

GLOBALMODULEDEFS(CIMAPAuthMod, "Allow users to authenticate via imap")
