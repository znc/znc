/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Socket.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/znc.h>
#include <signal.h>

/* We should need 2 DNS threads (host, bindhost) per IRC connection */
static const size_t MAX_IDLE_THREADS = 2;

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

int CZNCSock::ConvertAddress(const struct sockaddr_storage * pAddr, socklen_t iAddrLen, CS_STRING & sIP, u_short * piPort) {
	int ret = Csock::ConvertAddress(pAddr, iAddrLen, sIP, piPort);
	if (ret == 0)
		sIP.TrimPrefix("::ffff:");
	return ret;
}

#ifdef HAVE_THREADED_DNS
class CSockManager::CTDNSMonitorFD : public CSMonitorFD {
	CSockManager* m_Manager;
public:
	CTDNSMonitorFD(CSockManager* mgr) {
		m_Manager = mgr;
		Add(mgr->m_iTDNSpipe[0], ECT_Read);
	}

	virtual bool FDsThatTriggered(const std::map<int, short>& miiReadyFds) {
		if (miiReadyFds.find(m_Manager->m_iTDNSpipe[0])->second) {
			m_Manager->RetrieveTDNSResult();
		}
		return true;
	}
};

bool CSockManager::ThreadNeeded(struct TDNSStatus* threadStatus)
{
	// We should keep a number of idle threads alive
	if (threadStatus->num_idle > MAX_IDLE_THREADS)
		return false;
	// If ZNC is shutting down, all threads should exit
	return !threadStatus->done;
}

void* CSockManager::TDNSThread(void* argument) {
	TDNSStatus *threadStatus = (TDNSStatus *) argument;

	pthread_mutex_lock(&threadStatus->mutex);
	threadStatus->num_threads++;
	threadStatus->num_idle++;
	while (true) {
		/* Wait for a DNS job for us to do. This is a while()-loop
		 * because POSIX allows spurious wakeups from pthread_cond_wait.
		 */
		while (threadStatus->jobs.empty()) {
			if (!ThreadNeeded(threadStatus))
				break;
			pthread_cond_wait(&threadStatus->cond, &threadStatus->mutex);
		}

		if (!ThreadNeeded(threadStatus))
			break;

		/* Figure out a DNS job to do */
		assert(threadStatus->num_idle > 0);
		TDNSArg *job = threadStatus->jobs.front();
		threadStatus->jobs.pop_front();
		threadStatus->num_idle--;
		pthread_mutex_unlock(&threadStatus->mutex);

		/* Now do the actual work */
		DoDNS(job);

		pthread_mutex_lock(&threadStatus->mutex);
		threadStatus->num_idle++;
	}
	assert(threadStatus->num_threads > 0 && threadStatus->num_idle > 0);
	threadStatus->num_threads--;
	threadStatus->num_idle--;
	pthread_mutex_unlock(&threadStatus->mutex);
	return NULL;
}

void CSockManager::DoDNS(TDNSArg *arg) {
	int iCount = 0;
	while (true) {
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_ADDRCONFIG;
		arg->iRes = getaddrinfo(arg->sHostname.c_str(), NULL, &hints, &arg->aiResult);
		if (EAGAIN != arg->iRes) {
			break;
		}

		iCount++;
		if (iCount > 5) {
			arg->iRes = ETIMEDOUT;
			break;
		}
		sleep(5); // wait 5 seconds before next try
	}

	int need = sizeof(TDNSArg*);
	char* x = (char*)&arg;
	// This write() must succeed because POSIX guarantees that writes of
	// less than PIPE_BUF are atomic (and PIPE_BUF is at least 512).
	int w = write(arg->fd, x, need);
	if (w != need) {
		DEBUG("Something bad happened during write() to a pipe from TDNSThread, wrote " << w << " bytes: " << strerror(errno));
		exit(1);
	}
}

void CSockManager::StartTDNSThread(TDNSTask* task, bool bBind) {
	CString sHostname = bBind ? task->sBindhost : task->sHostname;
	TDNSArg* arg = new TDNSArg;
	arg->sHostname = sHostname;
	arg->task      = task;
	arg->fd        = m_iTDNSpipe[1];
	arg->bBind     = bBind;
	arg->iRes      = 0;
	arg->aiResult  = NULL;

	pthread_mutex_lock(&m_threadStatus.mutex);
	m_threadStatus.jobs.push_back(arg);
	/* Do we need a new DNS thread? */
	if (m_threadStatus.num_idle > 0) {
		/* Nope, there is one waiting for a job */
		pthread_cond_signal(&m_threadStatus.cond);
		pthread_mutex_unlock(&m_threadStatus.mutex);
		return;
	}
	pthread_mutex_unlock(&m_threadStatus.mutex);

	pthread_attr_t attr;
	if (pthread_attr_init(&attr)) {
		CString sError = "Couldn't init pthread_attr for " + sHostname;
		DEBUG(sError);
		CZNC::Get().Broadcast(sError, /* bAdminOnly = */ true);
		SetTDNSThreadFinished(task, bBind, NULL);
		return;
	}
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thr;
	sigset_t old_sigmask;
	sigset_t sigmask;
	sigfillset(&sigmask);
	/* Block all signals. The thread will inherit our signal mask and thus
	 * won't ever try to handle any signals.
	 */
	if (pthread_sigmask(SIG_SETMASK, &sigmask, &old_sigmask)) {
		CString sError = "Couldn't block signals";
		DEBUG(sError);
		CZNC::Get().Broadcast(sError, /* bAdminOnly = */ true);
		delete arg;
		pthread_attr_destroy(&attr);
		SetTDNSThreadFinished(task, bBind, NULL);
		return;
	}
	if (pthread_create(&thr, &attr, TDNSThread, &m_threadStatus)) {
		CString sError = "Couldn't create thread for " + sHostname;
		DEBUG(sError);
		CZNC::Get().Broadcast(sError, /* bAdminOnly = */ true);
		delete arg;
		pthread_attr_destroy(&attr);
		SetTDNSThreadFinished(task, bBind, NULL);
		return;
	}
	if (pthread_sigmask(SIG_SETMASK, &old_sigmask, NULL)) {
		CString sError = "Couldn't unblock signals";
		DEBUG(sError);
		CZNC::Get().Broadcast(sError, /* bAdminOnly = */ true);
		delete arg;
		pthread_attr_destroy(&attr);
		SetTDNSThreadFinished(task, bBind, NULL);
		return;
	}
	pthread_attr_destroy(&attr);
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
			throw "Can't resolve bind hostname. Try /znc clearbindhost";
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
			char s[40] = {}; // 40 is enough for both ipv4 and ipv6 addresses, including 0 terminator.
			getnameinfo(aiBind->ai_addr, aiBind->ai_addrlen, s, sizeof(s), NULL, 0, NI_NUMERICHOST);
			sBindhost = s;
		}
		char s[40] = {};
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

void CSockManager::RetrieveTDNSResult() {
	TDNSArg* a = NULL;
	int readed = 0;
	int need = sizeof(TDNSArg*);
	char* x = (char*)&a;
	while (readed < need) {
		int r = read(m_iTDNSpipe[0], x, need - readed);
		if (-1 == r) {
			DEBUG("Something bad happened during read() from a pipe when getting result of TDNSThread: " << strerror(errno));
			exit(1);
		}
		readed += r;
		x += r;
	}
	TDNSTask* task = a->task;
	if (0 != a->iRes) {
		DEBUG("Error in threaded DNS: " << gai_strerror(a->iRes));
		if (a->aiResult) {
			DEBUG("And aiResult is not NULL...");
		}
		a->aiResult = NULL; // just for case. Maybe to call freeaddrinfo()?
	}
	SetTDNSThreadFinished(task, a->bBind, a->aiResult);
	delete a;
}
#endif /* HAVE_THREADED_DNS */

CSockManager::CSockManager() {
#ifdef HAVE_THREADED_DNS
	int m = pthread_mutex_init(&m_threadStatus.mutex, NULL);
	if (m) {
		CUtils::PrintError("Can't initialize mutex: " + CString(strerror(errno)));
		exit(1);
	}
	m = pthread_cond_init(&m_threadStatus.cond, NULL);
	if (m) {
		CUtils::PrintError("Can't initialize condition variable: " + CString(strerror(errno)));
		exit(1);
	}

	m_threadStatus.num_threads = 0;
	m_threadStatus.num_idle = 0;
	m_threadStatus.done = false;

	if (pipe(m_iTDNSpipe)) {
		DEBUG("Ouch, can't open pipe for threaded DNS resolving: " << strerror(errno));
		exit(1);
	}

	MonitorFD(new CTDNSMonitorFD(this));
#endif
}

CSockManager::~CSockManager() {
#ifdef HAVE_THREADED_DNS
	/* Anyone has an idea how this can be done less ugly? */
	pthread_mutex_lock(&m_threadStatus.mutex);
	m_threadStatus.done = true;
	while (m_threadStatus.num_threads > 0) {
		pthread_cond_broadcast(&m_threadStatus.cond);
		pthread_mutex_unlock(&m_threadStatus.mutex);
		usleep(100);
		pthread_mutex_lock(&m_threadStatus.mutex);
	}
	pthread_mutex_unlock(&m_threadStatus.mutex);
	pthread_cond_destroy(&m_threadStatus.cond);
	pthread_mutex_destroy(&m_threadStatus.mutex);
#endif
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
		sBindHost = m_pModule->GetUser()->GetBindHost();
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
