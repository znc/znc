#include "main.h"

class CKeepNickTimer : public CCron {
public:
	CKeepNickTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		Start(5);
	}
	virtual ~CKeepNickTimer() {}

private:
protected:
	virtual void RunJob() {
		CIRCSock* pSock = m_pUser->GetIRCSock();
		if (pSock) {
			pSock->KeepNick();
		}
	}

	CUser*	m_pUser;
};

class CJoinTimer : public CCron {
public:
	CJoinTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		Start(20);
	}
	virtual ~CJoinTimer() {}

private:
protected:
	virtual void RunJob() {
		CIRCSock* pSock = m_pUser->GetIRCSock();

		if (pSock) {
			const vector<CChan*>& vChans = m_pUser->GetChans();

			for (unsigned int a = 0; a < vChans.size(); a++) {
				if (!vChans[a]->IsOn()) {
					pSock->PutServ("JOIN " + vChans[a]->GetName() + " " + vChans[a]->GetKey());
				}
			}
		}
	}

	CUser*	m_pUser;
};

class CAwayNickTimer : public CCron {
public:

	CAwayNickTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		Start(30);
	}
	virtual ~CAwayNickTimer() {}

private:
protected:
	virtual void RunJob() {
		if (!m_pUser->IsUserAttached()) {
			CIRCSock* pSock = m_pUser->GetIRCSock();
			if (pSock) {
				const CString& sSuffix = m_pUser->GetAwaySuffix();

				if (!sSuffix.empty()) {
					CString sNewNick = CNick::Concat(m_pUser->GetNick(), sSuffix, pSock->GetMaxNickLen());
					pSock->PutServ("NICK " + sNewNick);
				}
			}
		}

		m_pUser->GetZNC()->GetManager().DelCronByAddr(this);
	}

	CUser* m_pUser;
};
