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

#include <random>

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/SSLVerifyHost.h>
#include <znc/znc.h>
#include <signal.h>

#ifdef HAVE_ICU
#include <unicode/ucnv_cb.h>
#endif

// Copypasted from https://wiki.mozilla.org/Security/Server_Side_TLS#Intermediate_compatibility_.28default.29 at 22 Dec 2014
static CString ZNC_DefaultCipher() {
    return
        "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"
        "DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:"
        "ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:"
        "ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"
        "DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:"
        "AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
}

CZNCSock::CZNCSock(int timeout) : Csock(timeout) {
#ifdef HAVE_LIBSSL
	DisableSSLCompression();
	FollowSSLCipherServerPreference();
	DisableSSLProtocols(CZNC::Get().GetDisabledSSLProtocols());
	CString sCipher = CZNC::Get().GetSSLCiphers();
	if (sCipher.empty()) {
		sCipher = ZNC_DefaultCipher();
	}
	SetCipher(sCipher);
#endif
}

CZNCSock::CZNCSock(const CString& sHost, u_short port, int timeout) : Csock(sHost, port, timeout) {
#ifdef HAVE_LIBSSL
	DisableSSLCompression();
	FollowSSLCipherServerPreference();
	DisableSSLProtocols(CZNC::Get().GetDisabledSSLProtocols());
#endif
}

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

#ifdef HAVE_LIBSSL
int CZNCSock::VerifyPeerCertificate(int iPreVerify, X509_STORE_CTX * pStoreCTX) {
	if (iPreVerify == 0) {
		m_ssCertVerificationErrors.insert(X509_verify_cert_error_string(X509_STORE_CTX_get_error(pStoreCTX)));
	}
	return 1;
}

void CZNCSock::SSLHandShakeFinished() {
	if (GetType() != ETConn::OUTBOUND) {
		return;
	}

	X509* pCert = GetX509();
	if (!pCert) {
		DEBUG(GetSockName() + ": No cert");
		CallSockError(errnoBadSSLCert, "Anonymous SSL cert is not allowed");
		Close();
		return;
	}
	CString sHostVerifyError;
	if (!ZNC_SSLVerifyHost(m_HostToVerifySSL, pCert, sHostVerifyError)) {
		m_ssCertVerificationErrors.insert(sHostVerifyError);
	}
	X509_free(pCert);
	if (m_ssCertVerificationErrors.empty()) {
		DEBUG(GetSockName() + ": Good cert");
		return;
	}
	CString sFP = GetSSLPeerFingerprint();
	if (m_ssTrustedFingerprints.count(sFP) != 0) {
		DEBUG(GetSockName() + ": Cert explicitly trusted by user: " << sFP);
		return;
	}
	DEBUG(GetSockName() + ": Bad cert");
	CString sErrorMsg = "Invalid SSL certificate: ";
	sErrorMsg += CString(", ").Join(begin(m_ssCertVerificationErrors), end(m_ssCertVerificationErrors));
	CallSockError(errnoBadSSLCert, sErrorMsg);
	Close();
}
#endif

CString CZNCSock::GetSSLPeerFingerprint() const {
#ifdef HAVE_LIBSSL
	// Csocket's version returns insecure SHA-1
	// This one is SHA-256
	const EVP_MD* evp = EVP_sha256();
	X509* pCert = GetX509();
	if (!pCert) {
		DEBUG(GetSockName() + ": GetSSLPeerFingerprint: Anonymous cert");
		return "";
	}
	unsigned char buf[256/8];
	unsigned int _32 = 256/8;
	int iSuccess = X509_digest(pCert, evp, buf, &_32);
	X509_free(pCert);
	if (!iSuccess) {
		DEBUG(GetSockName() + ": GetSSLPeerFingerprint: Couldn't find digest");
		return "";
	}
	return CString(reinterpret_cast<const char*>(buf), sizeof buf).Escape_n(CString::EASCII, CString::EHEXCOLON);
#else
	return "";
#endif
}

#ifdef HAVE_PTHREAD
class CSockManager::CTDNSMonitorFD : public CSMonitorFD {
public:
	CTDNSMonitorFD() {
		Add(CThreadPool::Get().getReadFD(), ECT_Read);
	}

	bool FDsThatTriggered(const std::map<int, short>& miiReadyFds) override {
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
		iRes = getaddrinfo(sHostname.c_str(), nullptr, &hints, &aiResult);
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
		this->aiResult = nullptr; // just for case. Maybe to call freeaddrinfo()?
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
	arg->aiResult  = nullptr;
	arg->pManager  = this;

	CThreadPool::Get().addJob(arg);
}

static CString RandomFromSet(const SCString& sSet, std::default_random_engine& gen) {
	std::uniform_int_distribution<> distr(0, sSet.size() - 1);
	auto it = sSet.cbegin();
	std::advance(it, distr(gen));
	return *it;
}

static std::tuple<CString, bool> RandomFrom2SetsWithBias(const SCString& ss4, const SCString& ss6, std::default_random_engine& gen) {
	// It's not quite what RFC says how to choose between IPv4 and IPv6, but proper way is harder to implement.
	// It would require to maintain some state between Csock objects.
	bool bUseIPv6;
	if (ss4.empty()) {
		bUseIPv6 = true;
	} else if (ss6.empty()) {
		bUseIPv6 = false;
	} else {
		// Let's prefer IPv6 :)
		std::discrete_distribution<> d({2, 3});
		bUseIPv6 = d(gen);
	}
	const SCString& sSet = bUseIPv6 ? ss6 : ss4;
	return std::make_tuple(RandomFromSet(sSet, gen), bUseIPv6);
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
	SCString ssTargets4;
	SCString ssTargets6;
	for (addrinfo* ai = task->aiTarget; ai; ai = ai->ai_next) {
		char s[INET6_ADDRSTRLEN] = {};
		getnameinfo(ai->ai_addr, ai->ai_addrlen, s, sizeof(s), nullptr, 0, NI_NUMERICHOST);
		switch (ai->ai_family) {
			case AF_INET:
				ssTargets4.insert(s);
				break;
#ifdef HAVE_IPV6
			case AF_INET6:
				ssTargets6.insert(s);
				break;
#endif
		}
	}
	SCString ssBinds4;
	SCString ssBinds6;
	for (addrinfo* ai = task->aiBind; ai; ai = ai->ai_next) {
		char s[INET6_ADDRSTRLEN] = {};
		getnameinfo(ai->ai_addr, ai->ai_addrlen, s, sizeof(s), nullptr, 0, NI_NUMERICHOST);
		switch (ai->ai_family) {
			case AF_INET:
				ssBinds4.insert(s);
				break;
#ifdef HAVE_IPV6
			case AF_INET6:
				ssBinds6.insert(s);
				break;
#endif
		}
	}
	if (task->aiTarget) freeaddrinfo(task->aiTarget);
	if (task->aiBind) freeaddrinfo(task->aiBind);

	CString sBindhost;
	CString sTargetHost;
	std::random_device rd;
	std::default_random_engine gen(rd());

	try {
		if (ssTargets4.empty() && ssTargets6.empty()) {
			throw "Can't resolve server hostname";
		} else if (task->sBindhost.empty()) {
			// Choose random target
			std::tie(sTargetHost, std::ignore) = RandomFrom2SetsWithBias(ssTargets4, ssTargets6, gen);
		} else if (ssBinds4.empty() && ssBinds6.empty()) {
			throw "Can't resolve bind hostname. Try /znc ClearBindHost and /znc ClearUserBindHost";
		} else if (ssBinds4.empty()) {
			if (ssTargets6.empty()) {
				throw "Server address is IPv4-only, but bindhost is IPv6-only";
			} else {
				// Choose random target and bindhost from IPv6-only sets
				sTargetHost = RandomFromSet(ssTargets6, gen);
				sBindhost = RandomFromSet(ssBinds6, gen);
			}
		} else if (ssBinds6.empty()) {
			if (ssTargets4.empty()) {
				throw "Server address is IPv6-only, but bindhost is IPv4-only";
			} else {
				// Choose random target and bindhost from IPv4-only sets
				sTargetHost = RandomFromSet(ssTargets4, gen);
				sBindhost = RandomFromSet(ssBinds4, gen);
			}
		} else {
			// Choose random target
			bool bUseIPv6;
			std::tie(sTargetHost, bUseIPv6) = RandomFrom2SetsWithBias(ssTargets4, ssTargets6, gen);
			// Choose random bindhost matching chosen target
			const SCString& ssBinds = bUseIPv6 ? ssBinds6 : ssBinds4;
			sBindhost = RandomFromSet(ssBinds, gen);
		}

		DEBUG("TDNS: " << task->sSockName << ", connecting to [" << sTargetHost << "] using bindhost [" << sBindhost << "]");
		FinishConnect(sTargetHost, task->iPort, task->sSockName, task->iTimeout, task->bSSL, sBindhost, task->pcSock);
	} catch (const char* s) {
		DEBUG(task->sSockName << ", dns resolving error: " << s);
		task->pcSock->SetSockName(task->sSockName);
		task->pcSock->SockError(-1, s);
		delete task->pcSock;
	}

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
	if (pcSock) {
		pcSock->SetHostToVerifySSL(sHostname);
	}
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
	task->aiTarget    = nullptr;
	task->aiBind      = nullptr;
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
#ifdef HAVE_LIBSSL
	CString sCipher = CZNC::Get().GetSSLCiphers();
	if (sCipher.empty()) {
		sCipher = ZNC_DefaultCipher();
	}
	C.SetCipher(sCipher);
#endif

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
	CUser *pUser = nullptr;

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

#ifdef HAVE_ICU
void CIRCSocket::IcuExtToUCallback(
		UConverterToUnicodeArgs* toArgs,
		const char* codeUnits,
		int32_t length,
		UConverterCallbackReason reason,
		UErrorCode* err) {
	// From http://www.mirc.com/colors.html
	// The Control+O key combination in mIRC inserts ascii character 15,
	// which turns off all previous attributes, including color, bold, underline, and italics.
	//
	// \x02 bold
	// \x03 mIRC-compatible color
	// \x04 RRGGBB color
	// \x0F normal/reset (turn off bold, colors, etc.)
	// \x12 reverse (weechat)
	// \x16 reverse (mirc, kvirc)
	// \x1D italic
	// \x1F underline
	// Also see http://www.visualirc.net/tech-attrs.php
	//
	// Keep in sync with CUser::AddTimestamp and CIRCSocket::IcuExtFromUCallback
	static const std::set<char> scAllowedChars = {'\x02', '\x03', '\x04', '\x0F', '\x12', '\x16', '\x1D', '\x1F'};
	if (reason == UCNV_ILLEGAL && length == 1 && scAllowedChars.count(*codeUnits)) {
		*err = U_ZERO_ERROR;
		UChar c = *codeUnits;
		ucnv_cbToUWriteUChars(toArgs, &c, 1, 0, err);
		return;
	}
	Csock::IcuExtToUCallback(toArgs, codeUnits, length, reason, err);
}

void CIRCSocket::IcuExtFromUCallback(
		UConverterFromUnicodeArgs* fromArgs,
		const UChar* codeUnits,
		int32_t length,
		UChar32 codePoint,
		UConverterCallbackReason reason,
		UErrorCode* err) {
	// See comment in CIRCSocket::IcuExtToUCallback
	static const std::set<UChar32> scAllowedChars = {0x02, 0x03, 0x04, 0x0F, 0x12, 0x16, 0x1D, 0x1F};
	if (reason == UCNV_ILLEGAL && scAllowedChars.count(codePoint)) {
		*err = U_ZERO_ERROR;
		char c = codePoint;
		ucnv_cbFromUWriteBytes(fromArgs, &c, 1, 0, err);
		return;
	}
	Csock::IcuExtFromUCallback(fromArgs, codeUnits, length, codePoint, reason, err);
}
#endif
