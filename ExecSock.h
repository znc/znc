/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef EXEC_SOCK_H
#define EXEC_SOCK_H

#include "zncconfig.h"
#include "Socket.h"
#include <signal.h>

//! @author imaginos@imaginos.net
class CExecSock : public CZNCSock {
public:
	CExecSock() : CZNCSock() {
		m_iPid = -1;
	}

	int Execute(const CString & sExec) {
		int iReadFD, iWriteFD;
		m_iPid = popen2(iReadFD, iWriteFD, sExec);
		if (m_iPid != -1) {
			ConnectFD(iReadFD, iWriteFD, "0.0.0.0:0");
		}
		return(m_iPid);
	}
	void Kill(int iSignal)
	{
		kill(m_iPid, iSignal);
		Close();
	}
	virtual ~CExecSock() {
		close2(m_iPid, GetRSock(), GetWSock());
		SetRSock(-1);
		SetWSock(-1);
	}

	int popen2(int & iReadFD, int & iWriteFD, const CString & sCommand);
	void close2(int iPid, int iReadFD, int iWriteFD);

private:
	int  m_iPid;
};

#endif // !EXEC_SOCK_H
