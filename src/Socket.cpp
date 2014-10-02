/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <signal.h>

unsigned int CSockManager::GetAnonConnectionCount(const CString &sIP) const {
	const_iterator it;
	unsigned int ret = 0;

	for (it = begin(); it != end(); ++it) {
		Csock *pSock = *it;
		// Logged in CClients have "USR::<username>" as their sockname
		if (pSock->GetType() == Csock::INBOUND && pSock->GetRemoteIP() == sIP
				&& pSock->GetSockName().Left(5) != "USR::") {
			ret++;
		}
	}

	DEBUG("There are [" << ret << "] clients from [" << sIP << "]");

	return ret;
}

int CZNCSock::ConvertAddress(const struct sockaddr_storage * pAddr, socklen_t iAddrLen, CS_STRING & sIP, u_short * piPort) const {
	int ret = Csock::ConvertAddress(pAddr, iAddrLen, sIP, piPort);
	if (ret == 0)
		sIP.TrimPrefix("::ffff:");
	return ret;
}

#ifdef HAVE_PTHREAD
class CSockManager::CTDNSMonitorFD : public CSMonitorFD {
public:
	CTDNSMonitorFD() {
		Add(CThreadPool::Get().getReadFD(), ECT_Read);
	}

	virtual bool FDsThatTriggered(const std::map<int, short>& miiReadyFds) {
		if (miiReadyFds.find(CThreadPool::Get().getReadFD())->second) {
			CThreadPool::Get().handlePipeReadable();
		}
		return true;
	}
};
#endif

#ifdef HAVE_THREADED_DNS
void CSockManager::CDNSJob::runThread() {
	int iCount = 0;
	while (true) {
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_ADDRCONFIG;
		iRes = getaddrinfo(sHostname.c_str(), NULL, &hints, &aiResult);
		if (EAGAIN != iRes) {
			break;
		}

		iCount++;
		if (iCount > 5) {
			iRes = ETIMEDOUT;
			break;
		}
		sleep(5); // wait 5 seconds before next try
	}
}

void CSockManager::CDNSJob::runMain() {
	if (0 != this->iRes) {
		DEBUG("Error in threaded DNS: " << gai_strerror(this->iRes));
		if (this->aiResult) {
			DEBUG("And aiResult is not NULL...");
		}
		this->aiResult = NULL; // just for case. Maybe to call freeaddrinfo()?
	}
	pManager->SetTDNSThreadFinished(this->task, this->bBind, this->aiResult);
}

void CSockManager::StartTDNSThread(TDNSTask* task, bool bBind) {
	CString sHostname = bBind ? task->sBindhost : task->sHostname;
	CDNSJob* arg = new CDNSJob;
	arg->sHostname = sHostname;
	arg->task      = task;
	arg->bBind     = bBind;
	arg->iRes      = 0;
	arg->aiResult  = NULL;
	arg->pManager  = this;

	CThreadPool::Get().addJob(arg);
}

void CSockManager::SetTDNSThreadFinished(TDNSTask* task, bool bBind, addrinfo* aiResult) {
	if (bBind) {
		task->aiBind = aiResult;
		task->bDoneBind = true;
	} else {
		task->aiTarget = aiResult;
		task->bDoneTarget = true;
	}

	// Now that something is done, check if everything we needed is done
	if (!task->bDoneBind || !task->bDoneTarget) {
		return;
	}

	// All needed DNS is done, now collect the results
	addrinfo* aiTarget = NULL;
	addrinfo* aiBind = NULL;

	try {
		addrinfo* aiTarget4 = task->aiTarget;
		addrinfo* aiBind4 = task->aiBind;
		while (aiTarget4 && aiTarget4->ai_family != AF_INET) aiTarget4 = aiTarget4->ai_next;
		while (aiBind4 && aiBind4->ai_family != AF_INET) aiBind4 = aiBind4->ai_next;

		addrinfo* aiTarget6 = NULL;
		addrinfo* aiBind6 = NULL;
#ifdef HAVE_IPV6
		aiTarget6 = task->aiTarget;
		aiBind6 = task->aiBind;
		while (aiTarget6 && aiTarget6->ai_family != AF_INET6) aiTarget6 = aiTarget6->ai_next;
		while (aiBind6 && aiBind6->ai_family != AF_INET6) aiBind6 = aiBind6->ai_next;
#endif

		if (!aiTarget4 && !aiTarget6) {
			throw "Can't resolve server hostname";
		} else if (task->sBindhost.empty()) {
#ifdef HAVE_IPV6
			aiTarget = task->aiTarget;
#else
			aiTarget = aiTarget4;
#endif
		} else if (!aiBind4 && !aiBind6) {
			throw "Can't resolve bind hostname. Try /znc clearbindhost and /znc clearuserbindhost";
		} else if (aiBind6 && aiTarget6) {
			aiTarget = aiTarget6;
			aiBind = aiBind6;
		} else if (aiBind4 && aiTarget4) {
			aiTarget = aiTarget4;
			aiBind = aiBind4;
		} else {
			throw "Address family of bindhost doesn't match address family of server";
		}

		CString sBindhost;
		CString sTargetHost;
		if (!task->sBindhost.empty()) {
			char s[INET6_ADDRSTRLEN] = {};
			getnameinfo(aiBind->ai_addr, aiBind->ai_addrlen, s, sizeof(s), NULL, 0, NI_NUMERICHOST);
			sBindhost = s;
		}
		char s[INET6_ADDRSTRLEN] = {};
		getnameinfo(aiTarget->ai_addr, aiTarget->ai_addrlen, s, sizeof(s), NULL, 0, NI_NUMERICHOST);
		sTargetHost = s;

		DEBUG("TDNS: " << task->sSockName << ", connecting to [" << sTargetHost << "] using bindhost [" << sBindhost << "]");

		FinishConnect(sTargetHost, task->iPort, task->sSockName, task->iTimeout, task->bSSL, sBindhost, task->pcSock);
	} catch (const char* s) {
		DEBUG(task->sSockName << ", dns resolving error: " << s);
		task->pcSock->SetSockName(task->sSockName);
		task->pcSock->SockError(-1, s);
		delete task->pcSock;
	}

	if (task->aiTarget) freeaddrinfo(task->aiTarget);
	if (task->aiBind) freeaddrinfo(task->aiBind);

	delete task;
}
#endif /* HAVE_THREADED_DNS */

CSockManager::CSockManager() {
#ifdef HAVE_PTHREAD
	MonitorFD(new CTDNSMonitorFD());
#endif
}

CSockManager::~CSockManager() {
}

void CSockManager::Connect(const CString& sHostname, u_short iPort, const CString& sSockName, int iTimeout, bool bSSL, const CString& sBindHost, CZNCSock *pcSock) {
#ifdef HAVE_THREADED_DNS
	DEBUG("TDNS: initiating resolving of [" << sHostname << "] and bindhost [" << sBindHost << "]");
	TDNSTask* task = new TDNSTask;
	task->sHostname   = sHostname;
	task->iPort       = iPort;
	task->sSockName   = sSockName;
	task->iTimeout    = iTimeout;
	task->bSSL        = bSSL;
	task->sBindhost   = sBindHost;
	task->pcSock      = pcSock;
	task->aiTarget    = NULL;
	task->aiBind      = NULL;
	task->bDoneTarget = false;
	if (sBindHost.empty()) {
		task->bDoneBind = true;
	} else {
		task->bDoneBind = false;
		StartTDNSThread(task, true);
	}
	StartTDNSThread(task, false);
#else /* HAVE_THREADED_DNS */
	// Just let Csocket handle DNS itself
	FinishConnect(sHostname, iPort, sSockName, iTimeout, bSSL, sBindHost, pcSock);
#endif
}

void CSockManager::FinishConnect(const CString& sHostname, u_short iPort, const CString& sSockName, int iTimeout, bool bSSL, const CString& sBindHost, CZNCSock *pcSock) {
	CSConnection C(sHostname, iPort, iTimeout);

	C.SetSockName(sSockName);
	C.SetIsSSL(bSSL);
	C.SetBindHost(sBindHost);

	TSocketManager<CZNCSock>::Connect(C, pcSock);
}


/////////////////// CSocket ///////////////////
CSocket::CSocket(CModule* pModule) : CZNCSock() {
	m_pModule = pModule;
	if (m_pModule) m_pModule->AddSocket(this);
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CSocket::CSocket(CModule* pModule, const CString& sHostname, unsigned short uPort, int iTimeout) : CZNCSock(sHostname, uPort, iTimeout) {
	m_pModule = pModule;
	if (m_pModule) m_pModule->AddSocket(this);
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CSocket::~CSocket() {
	CUser *pUser = NULL;

	// CWebSock could cause us to have a NULL pointer here
	if (m_pModule) {
		pUser = m_pModule->GetUser();
		m_pModule->UnlinkSocket(this);
	}

	if (pUser && m_pModule && (m_pModule->GetType() != CModInfo::GlobalModule)) {
		pUser->AddBytesWritten(GetBytesWritten());
		pUser->AddBytesRead(GetBytesRead());
	} else {
		CZNC::Get().AddBytesWritten(GetBytesWritten());
		CZNC::Get().AddBytesRead(GetBytesRead());
	}
}

void CSocket::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	if (m_pModule) m_pModule->PutModule("Some socket reached its max buffer limit and was closed!");
	Close();
}

void CSocket::SockError(int iErrno, const CString& sDescription) {
	DEBUG(GetSockName() << " == SockError(" << sDescription << ", " << strerror(iErrno) << ")");
	if (iErrno == EMFILE) {
		// We have too many open fds, this can cause a busy loop.
		Close();
	}
}

bool CSocket::ConnectionFrom(const CString& sHost, unsigned short uPort) {
	return CZNC::Get().AllowConnectionFrom(sHost);
}

bool CSocket::Connect(const CString& sHostname, unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	if (!m_pModule) {
		DEBUG("ERROR: CSocket::Connect called on instance without m_pModule handle!");
		return false;
	}

	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::C::" + m_pModule->GetModName();
	CString sBindHost;

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
		sBindHost = pUser->GetBindHost();
		CIRCNetwork* pNetwork = m_pModule->GetNetwork();
		if (pNetwork) {
			sSockName += "::" + pNetwork->GetName();
			sBindHost = pNetwork->GetBindHost();
		}
	}

	// Don't overwrite the socket name if one is already set
	if (!GetSockName().empty()) {
		sSockName = GetSockName();
	}

	m_pModule->GetManager()->Connect(sHostname, uPort, sSockName, uTimeout, bSSL, sBindHost, this);
	return true;
}

bool CSocket::Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout) {
	if (!m_pModule) {
		DEBUG("ERROR: CSocket::Listen called on instance without m_pModule handle!");
		return false;
	}

	CUser* pUser = m_pModule->GetUser();
	CString sSockName = "MOD::L::" + m_pModule->GetModName();

	if (pUser) {
		sSockName += "::" + pUser->GetUserName();
	}
	// Don't overwrite the socket name if one is already set
	if (!GetSockName().empty()) {
		sSockName = GetSockName();
	}

	return m_pModule->GetManager()->ListenAll(uPort, sSockName, bSSL, SOMAXCONN, this);
}

CModule* CSocket::GetModule() const { return m_pModule; }
/////////////////// !CSocket ///////////////////
