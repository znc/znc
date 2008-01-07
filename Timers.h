/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _TIMERS_H
#define _TIMERS_H

#include "main.h"

class CKeepNickTimer : public CCron {
public:
	CKeepNickTimer(CUser* pUser) : CCron() {
		m_pUser = pUser;
		m_uTrys = 0;
		SetName("CKeepNickTimer::" + m_pUser->GetUserName());
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
		SetName("CMiscTimer::" + m_pUser->GetUserName());
		Start(30);
	}
	virtual ~CMiscTimer() {}

private:
protected:
	virtual void RunJob() {
		vector<CClient*>& vClients = m_pUser->GetClients();
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock && pIRCSock->GetTimeSinceLastDataTransaction() >= 270) {
			pIRCSock->PutIRC("PING :ZNC");
		}

		for (size_t a = 0; a < vClients.size(); a++) {
			CClient* pClient = vClients[a];

			if (pClient->GetTimeSinceLastDataTransaction() >= 470) {
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
		SetName("CJoinTimer::" + m_pUser->GetUserName());
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

#endif // !_TIMERS_H
