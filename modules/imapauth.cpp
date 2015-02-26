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

#include <znc/znc.h>

using std::map;

class CIMAPAuthMod;

class CIMAPSock : public CSocket {
public:
	CIMAPSock(CIMAPAuthMod* pModule, std::shared_ptr<CAuthBase> Auth)
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

	void ReadLine(const CString& sLine) override;
private:
protected:
	CIMAPAuthMod*              m_pIMAPMod;
	bool                       m_bSentLogin;
	bool                       m_bSentReply;
	std::shared_ptr<CAuthBase> m_spAuth;
};


class CIMAPAuthMod : public CModule {
public:
	MODCONSTRUCTOR(CIMAPAuthMod) {
		m_Cache.SetTTL(60000);
		m_sServer = "localhost";
		m_uPort = 143;
		m_bSSL = false;
	}

	virtual ~CIMAPAuthMod() {}

	bool OnBoot() override {
		return true;
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
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

	EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
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

	void OnModCommand(const CString& sLine) override {
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
	} else if (sLine.Left(5) == "AUTH ") {
		CUser* pUser = CZNC::Get().FindUser(m_spAuth->GetUsername());

		if (pUser && sLine.StartsWith("AUTH OK")) {
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

template<> void TModInfo<CIMAPAuthMod>(CModInfo& Info) {
	Info.SetWikiPage("imapauth");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("[ server [+]port [ UserFormatString ] ]");
}

GLOBALMODULEDEFS(CIMAPAuthMod, "Allow users to authenticate via IMAP.")
