#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "znc.h"
#include "HTTPSock.h"
#include "Server.h"
#include <map>

using std::map;

class CIMAPAuthMod;

class CIMAPSock : public CSocket {
public:
	CIMAPSock(CIMAPAuthMod* pModule, CSmartPtr<CAuthBase> Auth)
		: CSocket((CModule*) pModule, "IMAPMod"), m_spAuth(Auth) {
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
	CIMAPAuthMod*			m_pIMAPMod;
	bool					m_bSentLogin;
	bool					m_bSentReply;
	CSmartPtr<CAuthBase>	m_spAuth;
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

	virtual bool OnLoad(const CString& sArgs) {
		if (sArgs.Trim_n().empty()) {
			return true;	// use defaults
		}

		m_sServer = sArgs.Token(0);
		CString sPort = sArgs.Token(1);

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

		if (pUser && m_Cache.HasItem(CString(Auth->GetUsername() + ":" + Auth->GetPassword()).MD5())) {
			DEBUG_ONLY(cerr << "+++ Found in cache" << endl);
			Auth->AcceptLogin(*pUser);
			return HALT;
		}

		CIMAPSock* pSock = new CIMAPSock(this, Auth);
		pSock->Connect(m_sServer, m_uPort, m_bSSL, 20);

		return HALT;
	}

	virtual void OnModCommand(const CString& sLine) {
		ListSockets();
	}

	void CacheLogin(const CString& sLogin) {
		m_Cache.AddItem(sLogin);
	}
private:
	// Settings
	CString				m_sServer;
	unsigned short		m_uPort;
	bool				m_bSSL;
	// !Settings

	TCacheMap<CString>	m_Cache;
};

void CIMAPSock::ReadLine(const CString& sLine) {
	if (!m_bSentLogin) {
		m_bSentLogin = true;

		Write("AUTH LOGIN " + m_spAuth->GetUsername() + " " + m_spAuth->GetPassword() + "\r\n");
	} else {
		CUser* pUser = CZNC::Get().FindUser(m_spAuth->GetUsername());

		if (pUser && sLine.Left(7).CaseCmp("AUTH OK") == 0) {
			m_spAuth->AcceptLogin(*pUser);
			m_pIMAPMod->CacheLogin(CString(m_spAuth->GetUsername() + ":" + m_spAuth->GetPassword()).MD5());		// Use MD5 so passes don't sit in memory in plain text
			DEBUG_ONLY(cerr << "+++ Successful IMAP lookup" << endl);
		} else {
			m_spAuth->RefuseLogin("Invalid Password");
			DEBUG_ONLY(cerr << "--- FAILED IMAP lookup" << endl);
		}

		m_bSentReply = true;
		Close();
	}
}

GLOBALMODULEDEFS(CIMAPAuthMod, "Allow users to authenticate via imap");
