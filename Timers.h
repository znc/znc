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

