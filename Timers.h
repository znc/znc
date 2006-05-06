#include "main.h"

class CKeepNickTimer : public CCron {
public:
	CKeepNickTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		m_uTrys = 0;
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

class CMiscTimer : public CCron {
public:
	CMiscTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		Start(30);
	}
	virtual ~CMiscTimer() {}

private:
protected:
	virtual void RunJob() {
		vector<CClient*>& vClients = m_pUser->GetClients();
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock && pIRCSock->GetTimeSinceLastWrite() >= 270) {
			pIRCSock->PutIRC("PING :ZNC");
		}

		for (size_t a = 0; a < vClients.size(); a++) {
			CClient* pClient = vClients[a];

			if (pClient->GetTimeSinceLastWrite() >= 470) {
				pClient->PutClient("PING :ZNC");
			}
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
		if (m_pUser->IsIRCConnected()) {
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
		if (m_pUser->IsUserAttached() && m_pUser->IsIRCConnected()) {
			CIRCSock* pIRCSock = m_pUser->GetIRCSock();

			if (pIRCSock) {
				CString sConfNick = m_pUser->GetNick();

				if (pIRCSock->GetNick().CaseCmp(CNick::Concat(sConfNick, m_pUser->GetAwaySuffix(), pIRCSock->GetMaxNickLen())) == 0) {
					pIRCSock->PutIRC("NICK " + sConfNick);
				}
			}
		}

		m_pUser->DelBackNickTimer();
		CZNC::Get().GetManager().DelCronByAddr(this);
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
		if (!m_pUser->IsUserAttached() && m_pUser->IsIRCConnected()) {
			CIRCSock* pIRCSock = m_pUser->GetIRCSock();

			if (pIRCSock) {
				const CString& sSuffix = m_pUser->GetAwaySuffix();

				if (!sSuffix.empty()) {
					CString sAwayNick = CNick::Concat(m_pUser->GetNick(), sSuffix, pIRCSock->GetMaxNickLen());
					pIRCSock->PutIRC("NICK " + sAwayNick);
				}
			}
		}

		m_pUser->DelAwayNickTimer();
		CZNC::Get().GetManager().DelCronByAddr(this);
	}

	CUser* m_pUser;
};
