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
			if (m_uTrys++ >= 40) {
				pSock->SetOrigNickPending(false);
				m_uTrys = 0;
			}

			pSock->KeepNick();
		}
	}

	CUser*			m_pUser;
	unsigned int	m_uTrys;
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
			m_pUser->JoinChans();
		}
	}

	CUser*	m_pUser;
};

class CBackNickTimer : public CCron {
public:
	CBackNickTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		Start(3);
	}
	virtual ~CBackNickTimer() {}

private:
protected:
	virtual void RunJob() {
		if (m_pUser->IsUserAttached()) {
			CIRCSock* pSock = m_pUser->GetIRCSock();

			if (pSock) {
				CString sConfNick = m_pUser->GetNick();

				if (pSock->GetNick().CaseCmp(CNick::Concat(sConfNick, m_pUser->GetAwaySuffix(), pSock->GetMaxNickLen())) == 0) {
					pSock->PutServ("NICK " + sConfNick);
				}
			}
		}

		CZNC::Get().GetManager().DelCronByAddr(this);
		m_pUser->DelBackNickTimer();
	}

	CUser* m_pUser;
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
					CString sAwayNick = CNick::Concat(m_pUser->GetNick(), sSuffix, pSock->GetMaxNickLen());
					pSock->PutServ("NICK " + sAwayNick);
				}
			}
		}

		CZNC::Get().GetManager().DelCronByAddr(this);
		m_pUser->DelAwayNickTimer();
	}

	CUser* m_pUser;
};
