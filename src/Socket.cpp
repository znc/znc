/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

#include <znc/Socket.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/SSLVerifyHost.h>
#include <znc/znc.h>
#include <signal.h>

#ifdef HAVE_ICU
#include <unicode/ucnv_cb.h>
#endif

#ifdef HAVE_LIBSSL
// Copypasted from
// https://wiki.mozilla.org/Security/Server_Side_TLS#Intermediate_compatibility_.28default.29
// at 2024-02-08 (version 5.7)
static CString ZNC_DefaultCipher() {
    // This is TLS1.2 only, because TLS1.3 ciphers are probably not configurable here yet
    return "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
           "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
           "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
           "DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305";
}
#endif

CZNCSock::CZNCSock(int timeout)
    : Csock(timeout),
      m_sHostToVerifySSL(""),
      m_ssTrustedFingerprints(),
      m_ssCertVerificationErrors() {
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

CZNCSock::CZNCSock(const CString& sHost, u_short port, int timeout)
    : Csock(sHost, port, timeout),
      m_sHostToVerifySSL(""),
      m_ssTrustedFingerprints(),
      m_ssCertVerificationErrors() {
#ifdef HAVE_LIBSSL
    DisableSSLCompression();
    FollowSSLCipherServerPreference();
    DisableSSLProtocols(CZNC::Get().GetDisabledSSLProtocols());
#endif
}

unsigned int CSockManager::GetAnonConnectionCount(const CString& sIP) const {
    unsigned int ret = 0;

    for (const Csock* pSock : *this) {
        // Logged in CClients have "USR::<username>" as their sockname
        if (pSock->GetType() == Csock::INBOUND && pSock->GetRemoteIP() == sIP &&
            !pSock->GetSockName().StartsWith("USR::")) {
            ret++;
        }
    }

    DEBUG("There are [" << ret << "] clients from [" << sIP << "]");

    return ret;
}

int CZNCSock::ConvertAddress(const struct sockaddr_storage* pAddr,
                             socklen_t iAddrLen, CString& sIP,
                             u_short* piPort) const {
    int ret = Csock::ConvertAddress(pAddr, iAddrLen, sIP, piPort);
    if (ret == 0) sIP.TrimPrefix("::ffff:");
    return ret;
}

#ifdef HAVE_LIBSSL
int CZNCSock::VerifyPeerCertificate(int iPreVerify, X509_STORE_CTX* pStoreCTX) {
    if (iPreVerify == 0) {
        m_ssCertVerificationErrors.insert(
            X509_verify_cert_error_string(X509_STORE_CTX_get_error(pStoreCTX)));
    }
    return 1;
}

void CZNCSock::SSLHandShakeFinished() {
    X509* pCert = GetX509();
    if (!CheckSSLCert(pCert)) {
        Close();
    }
    X509_free(pCert);
}

bool CZNCSock::CheckSSLCert(X509* pCert) {
    if (GetType() != ETConn::OUTBOUND) {
        return true;
    }

    if (!pCert) {
        DEBUG(GetSockName() + ": No cert");
        CallSockError(errnoBadSSLCert, "Anonymous SSL cert is not allowed");
        return false;
    }
    if (GetTrustAllCerts()) {
        DEBUG(GetSockName() + ": Verification disabled, trusting all.");
        return true;
    }
    CString sHostVerifyError;
    if (!ZNC_SSLVerifyHost(m_sHostToVerifySSL, pCert, sHostVerifyError)) {
        m_ssCertVerificationErrors.insert(sHostVerifyError);
    }
    if (GetTrustPKI() && m_ssCertVerificationErrors.empty()) {
        DEBUG(GetSockName() + ": Good cert (PKI valid)");
        return true;
    }
    CString sFP = GetSSLPeerFingerprint(pCert);
    if (m_ssTrustedFingerprints.count(sFP) != 0) {
        DEBUG(GetSockName() + ": Cert explicitly trusted by user: " << sFP);
        return true;
    }
    DEBUG(GetSockName() + ": Bad cert");
    CString sErrorMsg = "Invalid SSL certificate: ";
    sErrorMsg += CString(", ").Join(begin(m_ssCertVerificationErrors),
                                    end(m_ssCertVerificationErrors));
    SSLCertError(pCert);
    CallSockError(errnoBadSSLCert, sErrorMsg);
    return false;
}

bool CZNCSock::SNIConfigureClient(CString& sHostname) {
    sHostname = m_sHostToVerifySSL;
    return true;
}

CString CZNCSock::GetSSLPeerFingerprint(X509* pCert) const {
    // Csocket's version returns insecure SHA-1
    // This one is SHA-256
    const EVP_MD* evp = EVP_sha256();
    bool bOwned = false;
    if (!pCert) {
        pCert = GetX509();
        bOwned = true;
    }
    if (!pCert) {
        DEBUG(GetSockName() + ": GetSSLPeerFingerprint: Anonymous cert");
        return "";
    }
    unsigned char buf[256 / 8];
    unsigned int _32 = 256 / 8;
    int iSuccess = X509_digest(pCert, evp, buf, &_32);
    if (bOwned) {
        X509_free(pCert);
    }
    if (!iSuccess) {
        DEBUG(GetSockName() + ": GetSSLPeerFingerprint: Couldn't find digest");
        return "";
    }
    return CString(reinterpret_cast<const char*>(buf), sizeof buf)
        .Escape_n(CString::EASCII, CString::EHEXCOLON);
}
#endif

void CZNCSock::SetEncoding(const CString& sEncoding) {
#ifdef HAVE_ICU
    Csock::SetEncoding(CZNC::Get().FixupEncoding(sEncoding));
#endif
}

#ifdef HAVE_PTHREAD
class CSockManager::CThreadMonitorFD : public CSMonitorFD {
  public:
    CThreadMonitorFD() { Add(CThreadPool::Get().getReadFD(), ECT_Read); }

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
        sleep(5);  // wait 5 seconds before next try
    }
}

void CSockManager::CDNSJob::runMain() {
    if (0 != this->iRes) {
        DEBUG("Error in threaded DNS: " << gai_strerror(this->iRes) << " while trying to resolve " << this->sHostname);
        if (this->aiResult) {
            DEBUG("And aiResult is not nullptr...");
        }
        // just for case. Maybe to call freeaddrinfo()?
        this->aiResult = nullptr;
    }
    pManager->SetTDNSThreadFinished(this->task, this->bBind, this->aiResult);
}

void CSockManager::StartTDNSThread(TDNSTask* task, bool bBind) {
    CString sHostname = bBind ? task->sBindhost : task->sHostname;
    CDNSJob* arg = new CDNSJob;
    arg->sHostname = sHostname;
    arg->task = task;
    arg->bBind = bBind;
    arg->pManager = this;

    CThreadPool::Get().addJob(arg);
}

class CHappyEyeballs {
    class CConnectionAttempt;

  public:
    struct CAttempt {
        CString sHostname;
        CString sBindhost;
        CConnectionAttempt* pcSock = nullptr;
    };

    CHappyEyeballs(std::vector<CAttempt> vAttempts, int iPort, bool bSSL,
                   CString sSockName, int iTimeout, CZNCSock* pcSock,
                   CSockManager* pManager)
        : m_vAttempts(std::move(vAttempts)),
          m_iPort(iPort),
          m_bSSL(bSSL),
          m_sSockName(std::move(sSockName)),
          m_iTimeout(iTimeout),
          m_pcSock(pcSock),
          m_pManager(pManager),
          m_pTimer(new CConnectionAttemptor(this)),
          m_pShutdownWatcher(new CShutdownWatcher(this)) {
        pManager->AddCron(m_pTimer);
        pManager->AddCron(m_pShutdownWatcher);
    }

    ~CHappyEyeballs() {
        if (m_pTimer) {
            m_pTimer->Stop();
        }
        m_pShutdownWatcher->Stop();
        for (const CAttempt& at : m_vAttempts) {
            if (at.pcSock) {
                at.pcSock->Detach();
                at.pcSock->Close();
            }
        }
    }

  private:
    class CConnectionAttemptor : public CCron {
      public:
        explicit CConnectionAttemptor(CHappyEyeballs* pEye) : m_pEye(pEye) {
            SetName("HappyEyeballsAttemptor::" + pEye->m_sSockName);
            RunJob();
            StartMaxCycles(1 /* seconds */, pEye->m_vAttempts.size() - 1);
        }

        ~CConnectionAttemptor() {
            if (m_pEye) {
                m_pEye->m_pTimer = nullptr;
            }
        }

        void Stop() {
            m_pEye = nullptr;
            CCron::Stop();
        }

      protected:
        void RunJob() override {
            if (m_pEye) {
                m_pEye->NewAttempt();
            }
        }

      private:
        CHappyEyeballs* m_pEye;
    };

    class CShutdownWatcher : public CCron {
        // Not really a cron, but allows to intercept CSockManager::Cleanup
      public:
        explicit CShutdownWatcher(CHappyEyeballs* pEye) : m_pEye(pEye) {
            SetName("HappyEyeballsShutdown::" + pEye->m_sSockName);
            Start(10000);
        }
        ~CShutdownWatcher() {
            if (m_pEye) {
                m_pEye->Shutdown();
            }
        }
        void Stop() {
            m_pEye = nullptr;
            CCron::Stop();
        }

      protected:
        void RunJob() override {}

      private:
        CHappyEyeballs* m_pEye;
    };

    class CConnectionAttempt : public CZNCSock {
      public:
        CConnectionAttempt(CHappyEyeballs* pEye, int iCurrentAttempt)
            : m_pEye(pEye), m_iCurrentAttempt(iCurrentAttempt) {}

        ~CConnectionAttempt() {
            if (m_pEye) {
                m_pEye->m_vAttempts[m_iCurrentAttempt].pcSock = nullptr;
            }
        }

        void Detach() { m_pEye = nullptr; }

      private:
        void Connected() override {
            if (m_pEye) {
                m_pEye->Success(m_iCurrentAttempt);
            }
        }

        void Timeout() override {
            if (m_pEye) {
                m_pEye->Error(m_iCurrentAttempt, -1, t_s("Connection timeout"));
            }
        }
        void SockError(int iErrno, const CString& sDescription) override {
            if (m_pEye) {
                m_pEye->Error(m_iCurrentAttempt, iErrno, sDescription);
            }
        }
        void ConnectionRefused() override {
            if (m_pEye) {
                m_pEye->Error(m_iCurrentAttempt, -1, t_s("Connection refused"));
            }
        }

        CHappyEyeballs* m_pEye;
        int m_iCurrentAttempt;
    };

    void NewAttempt() {
        CAttempt& at = m_vAttempts[m_iCurrentAttempt];
        at.pcSock = new CConnectionAttempt(this, m_iCurrentAttempt);
        CSConnection C(at.sHostname, m_iPort, m_iTimeout);
        C.SetSockName(m_sSockName + "::" + CString(m_iCurrentAttempt));
        C.SetIsSSL(false);
        C.SetBindHost(at.sBindhost);
        m_pManager->TSocketManager<CZNCSock>::Connect(C, at.pcSock);
        DEBUG("Connection attempt " << C.GetSockName() << " to "
                                    << at.sHostname);

        m_iCurrentAttempt++;
    }

    void Success(int iAttempt) {
        auto*& pGoodSocket = m_vAttempts[iAttempt].pcSock;
        DEBUG("Connection attempt " << pGoodSocket->GetSockName()
                                    << " succeeded.");
        m_pcSock->SetSock(pGoodSocket->GetSock());
        pGoodSocket->Detach();
        pGoodSocket->Dereference();
        pGoodSocket = nullptr;
        m_pcSock->SetSkipConnect(true);
        m_pManager->FinishConnect(m_vAttempts[iAttempt].sHostname, m_iPort,
                                  m_sSockName, m_iTimeout, m_bSSL,
                                  m_vAttempts[iAttempt].sBindhost, m_pcSock);
        delete this;
    }

    void Error(int iAttempt, int iErrno, const CString& sDescription) {
        DEBUG("Connection attempt "
              << m_vAttempts[iAttempt].pcSock->GetSockName() << " failed "
              << iErrno << " " << sDescription);
        m_siFailedAttempts.insert(iAttempt);
        // TODO aggregate errors from other attempts
        if (m_siFailedAttempts.size() == m_vAttempts.size()) {
            m_pcSock->SetSockName(m_sSockName);
            m_pcSock->SockError(iErrno, sDescription);
            delete m_pcSock;
            delete this;
        }
    }

    void Shutdown() {
        m_pcSock->SetSockName(m_sSockName);
        m_pcSock->SockError(-1, "Shutdown");
        delete m_pcSock;
        delete this;
    }

    std::vector<CAttempt> m_vAttempts;
    int m_iCurrentAttempt = 0;
    int m_iPort;
    bool m_bSSL;
    CString m_sSockName;
    int m_iTimeout;
    CZNCSock* m_pcSock;
    CSockManager* m_pManager;
    CConnectionAttemptor* m_pTimer;
    CShutdownWatcher* m_pShutdownWatcher;
    std::set<int> m_siFailedAttempts;
};

static CString RandomFromSet(const SCString& sSet,
                             std::default_random_engine& gen) {
    std::uniform_int_distribution<> distr(0, sSet.size() - 1);
    auto it = sSet.cbegin();
    std::advance(it, distr(gen));
    return *it;
}

void CSockManager::SetTDNSThreadFinished(TDNSTask* task, bool bBind,
                                         addrinfo* aiResult) {
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
    VCString vsTargets4;
    VCString vsTargets6;
    for (addrinfo* ai = task->aiTarget; ai; ai = ai->ai_next) {
        char s[INET6_ADDRSTRLEN] = {};
        getnameinfo(ai->ai_addr, ai->ai_addrlen, s, sizeof(s), nullptr, 0,
                    NI_NUMERICHOST);
        switch (ai->ai_family) {
            case AF_INET:
                vsTargets4.push_back(s);
                break;
#ifdef HAVE_IPV6
            case AF_INET6:
                vsTargets6.push_back(s);
                break;
#endif
        }
    }
    SCString ssBinds4;
    SCString ssBinds6;
    for (addrinfo* ai = task->aiBind; ai; ai = ai->ai_next) {
        char s[INET6_ADDRSTRLEN] = {};
        getnameinfo(ai->ai_addr, ai->ai_addrlen, s, sizeof(s), nullptr, 0,
                    NI_NUMERICHOST);
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

    std::vector<CHappyEyeballs::CAttempt> vAttempts;
    vAttempts.reserve(vsTargets4.size() + vsTargets6.size());
    std::random_device rd;
    std::default_random_engine gen(rd());

    try {
        if (vsTargets4.empty() && vsTargets6.empty()) {
            throw t_s("Can't resolve server hostname");
        } else if (task->sBindhost.empty()) {
            int i = 0;
            for (; i < std::min(vsTargets4.size(), vsTargets6.size()); ++i) {
                CHappyEyeballs::CAttempt at6;
                at6.sHostname = vsTargets6[i];
                vAttempts.push_back(at6);
                CHappyEyeballs::CAttempt at4;
                at4.sHostname = vsTargets4[i];
                vAttempts.push_back(at4);
            }
            VCString& vsRest =
                vsTargets4.size() < vsTargets6.size() ? vsTargets6 : vsTargets4;
            for (; i < vsRest.size(); ++i) {
                CHappyEyeballs::CAttempt at;
                at.sHostname = vsRest[i];
                vAttempts.push_back(at);
            }
        } else if (ssBinds4.empty() && ssBinds6.empty()) {
            throw t_s(
                "Can't resolve bind hostname. Try /znc ClearBindHost and /znc "
                "ClearUserBindHost");
        } else if (ssBinds4.empty()) {
            if (vsTargets6.empty()) {
                throw t_s(
                    "Server address is IPv4-only, but bindhost is IPv6-only");
            } else {
                for (const CString& sTarget : vsTargets6) {
                    CHappyEyeballs::CAttempt at;
                    at.sHostname = sTarget;
                    at.sBindhost = RandomFromSet(ssBinds6, gen);
                    vAttempts.push_back(at);
                }
            }
        } else if (ssBinds6.empty()) {
            if (vsTargets4.empty()) {
                throw t_s(
                    "Server address is IPv6-only, but bindhost is IPv4-only");
            } else {
                for (const CString& sTarget : vsTargets4) {
                    CHappyEyeballs::CAttempt at;
                    at.sHostname = sTarget;
                    at.sBindhost = RandomFromSet(ssBinds4, gen);
                    vAttempts.push_back(at);
                }
            }
        } else {
            int i = 0;
            for (; i < std::min(vsTargets4.size(), vsTargets6.size()); ++i) {
                CHappyEyeballs::CAttempt at6;
                at6.sHostname = vsTargets6[i];
                at6.sBindhost = RandomFromSet(ssBinds6, gen);
                vAttempts.push_back(at6);
                CHappyEyeballs::CAttempt at4;
                at4.sHostname = vsTargets4[i];
                at4.sBindhost = RandomFromSet(ssBinds4, gen);
                vAttempts.push_back(at4);
            }
            VCString& vsRest =
                vsTargets4.size() < vsTargets6.size() ? vsTargets6 : vsTargets4;
            SCString& ssRestBinds =
                vsTargets4.size() < vsTargets6.size() ? ssBinds6 : ssBinds4;
            for (; i < vsRest.size(); ++i) {
                CHappyEyeballs::CAttempt at;
                at.sHostname = vsRest[i];
                at.sBindhost = RandomFromSet(ssRestBinds, gen);
                vAttempts.push_back(at);
            }
        }
    } catch (const CString& s) {
        DEBUG(task->sSockName << ", dns resolving error: " << s);
        task->pcSock->SetSockName(task->sSockName);
        task->pcSock->SockError(-1, s);
        delete task->pcSock;
        delete task;
        return;
    }

    if (vAttempts.size() > 5) {
        vAttempts.resize(5);
    }

    std::stringstream ss;
    for (const auto& at : vAttempts) {
        ss << " (" << at.sHostname << " from " << at.sBindhost << ")";
    }
    DEBUG("Resolved DNS for " << task->sSockName << ":" << ss.str());

    new CHappyEyeballs(std::move(vAttempts), task->iPort, task->bSSL,
                       task->sSockName, task->iTimeout, task->pcSock, this);
    delete task;
}
#endif /* HAVE_THREADED_DNS */

CSockManager::CSockManager() {
#ifdef HAVE_PTHREAD
    MonitorFD(new CThreadMonitorFD());
#endif
}

CSockManager::~CSockManager() {}

void CSockManager::Connect(const CString& sHostname, u_short iPort,
                           const CString& sSockName, int iTimeout, bool bSSL,
                           const CString& sBindHost, CZNCSock* pcSock) {
    m_InFlightDnsSockets[pcSock] = false;
    if (pcSock) {
        pcSock->SetHostToVerifySSL(sHostname);
    }
#ifdef HAVE_THREADED_DNS
    DEBUG("Initiating DNS resolving of [" << sHostname << "] and bindhost ["
                                          << sBindHost << "]");
    TDNSTask* task = new TDNSTask;
    task->sHostname = sHostname;
    task->iPort = iPort;
    task->sSockName = sSockName;
    task->iTimeout = iTimeout;
    task->bSSL = bSSL;
    task->sBindhost = sBindHost;
    task->pcSock = pcSock;
    // TODO asynchronously lookup A and AAAA like RFC 8305 advises.
    if (sBindHost.empty()) {
        task->bDoneBind = true;
    } else {
        StartTDNSThread(task, true);
    }
    StartTDNSThread(task, false);
#else /* HAVE_THREADED_DNS */
    // Just let Csocket handle DNS itself
    FinishConnect(sHostname, iPort, sSockName, iTimeout, bSSL, sBindHost,
                  pcSock);
#endif
}

void CSockManager::FinishConnect(const CString& sHostname, u_short iPort,
                                 const CString& sSockName, int iTimeout,
                                 bool bSSL, const CString& sBindHost,
                                 CZNCSock* pcSock) {
    auto it = m_InFlightDnsSockets.find(pcSock);
    if (it != m_InFlightDnsSockets.end()) {
        bool bSocketDeletedAlready = it->second;
        m_InFlightDnsSockets.erase(it);
        if (bSocketDeletedAlready) {
            DEBUG("TDNS: Socket ["
                  << sSockName
                  << "] is deleted already, not proceeding with connection");
            return;
        }
    } else {
        // impossible
    }

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

void CSockManager::DelSockByAddr(Csock* pcSock) {
    auto it = m_InFlightDnsSockets.find(pcSock);
    if (it != m_InFlightDnsSockets.end()) {
        // The socket is resolving its DNS. When that finishes, let it silently
        // die without crash.
        it->second = true;
    }
    TSocketManager<CZNCSock>::DelSockByAddr(pcSock);
}

/////////////////// CSocket ///////////////////
CSocket::CSocket(CModule* pModule) : CZNCSock(), m_pModule(pModule) {
    if (m_pModule) m_pModule->AddSocket(this);
    EnableReadLine();
    SetMaxBufferThreshold(10240);
}

CSocket::CSocket(CModule* pModule, const CString& sHostname,
                 unsigned short uPort, int iTimeout)
    : CZNCSock(sHostname, uPort, iTimeout), m_pModule(pModule) {
    if (m_pModule) m_pModule->AddSocket(this);
    EnableReadLine();
    SetMaxBufferThreshold(10240);
}

CSocket::~CSocket() {
    CUser* pUser = nullptr;
    CIRCNetwork* pNetwork = nullptr;

    // CWebSock could cause us to have a nullptr pointer here
    if (m_pModule) {
        pUser = m_pModule->GetUser();
        pNetwork = m_pModule->GetNetwork();
        m_pModule->UnlinkSocket(this);
    }

    if (pNetwork && m_pModule &&
        (m_pModule->GetType() == CModInfo::NetworkModule)) {
        pNetwork->AddBytesWritten(GetBytesWritten());
        pNetwork->AddBytesRead(GetBytesRead());
    } else if (pUser && m_pModule &&
               (m_pModule->GetType() == CModInfo::UserModule)) {
        pUser->AddBytesWritten(GetBytesWritten());
        pUser->AddBytesRead(GetBytesRead());
    } else {
        CZNC::Get().AddBytesWritten(GetBytesWritten());
        CZNC::Get().AddBytesRead(GetBytesRead());
    }
}

void CSocket::ReachedMaxBuffer() {
    DEBUG(GetSockName() << " == ReachedMaxBuffer()");
    if (m_pModule)
        m_pModule->PutModule(
            t_s("Some socket reached its max buffer limit and was closed!"));
    Close();
}

void CSocket::SockError(int iErrno, const CString& sDescription) {
    DEBUG(GetSockName() << " == SockError(" << sDescription << ", "
                        << strerror(iErrno) << ")");
    if (iErrno == EMFILE) {
        // We have too many open fds, this can cause a busy loop.
        Close();
    }
}

bool CSocket::ConnectionFrom(const CString& sHost, unsigned short uPort) {
    return CZNC::Get().AllowConnectionFrom(sHost);
}

bool CSocket::Connect(const CString& sHostname, unsigned short uPort, bool bSSL,
                      unsigned int uTimeout) {
    if (!m_pModule) {
        DEBUG(
            "ERROR: CSocket::Connect called on instance without m_pModule "
            "handle!");
        return false;
    }

    CUser* pUser = m_pModule->GetUser();
    CString sBindHost;

    if (pUser) {
        sBindHost = pUser->GetBindHost();
        CIRCNetwork* pNetwork = m_pModule->GetNetwork();
        if (pNetwork) {
            sBindHost = pNetwork->GetBindHost();
        }
    }

    CString sSockName = ConstructSockName("C");
    m_pModule->GetManager()->Connect(sHostname, uPort, sSockName, uTimeout,
                                     bSSL, sBindHost, this);
    return true;
}

bool CSocket::Listen(unsigned short uPort, bool bSSL, unsigned int uTimeout) {
    if (!m_pModule) {
        DEBUG(
            "ERROR: CSocket::Listen called on instance without m_pModule "
            "handle!");
        return false;
    }

    CString sSockName = ConstructSockName("L");
    return m_pModule->GetManager()->ListenAll(uPort, sSockName, bSSL, SOMAXCONN,
                                              this);
}

bool CSocket::ListenUnix(const CString& sPath) {
    if (!m_pModule) {
        DEBUG(
            "ERROR: CSocket::Listen called on instance without m_pModule "
            "handle!");
        return false;
    }

    CString sSockName = ConstructSockName("LU");
    return m_pModule->GetManager()->ListenUnix(sSockName, sPath, this);
}

bool CSocket::ConnectUnix(const CString& sPath) {
    if (!m_pModule) {
        DEBUG(
            "ERROR: CSocket::Listen called on instance without m_pModule "
            "handle!");
        return false;
    }

    CString sSockName = ConstructSockName("CU");
    return m_pModule->GetManager()->ConnectUnix(sSockName, sPath, this);
}

CString CSocket::ConstructSockName(const CString& sPart) const {
    CString sSockName = GetSockName();
    if (!sSockName.empty()) return sSockName;

    sSockName = "MOD::" + sPart + "::" + m_pModule->GetModName();

    if (CUser* pUser = m_pModule->GetUser()) {
        sSockName += "::" + pUser->GetUsername();
        if (CIRCNetwork* pNetwork = m_pModule->GetNetwork()) {
            sSockName += "::" + pNetwork->GetName();
        }
    }
    return sSockName;
}

CModule* CSocket::GetModule() const { return m_pModule; }
/////////////////// !CSocket ///////////////////

#ifdef HAVE_ICU
void CIRCSocket::IcuExtToUCallback(UConverterToUnicodeArgs* toArgs,
                                   const char* codeUnits, int32_t length,
                                   UConverterCallbackReason reason,
                                   UErrorCode* err) {
    // From http://www.mirc.com/colors.html
    // The Control+O key combination in mIRC inserts ascii character 15,
    // which turns off all previous attributes, including color, bold,
    // underline, and italics.
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
    static const std::set<char> scAllowedChars = {
        '\x02', '\x03', '\x04', '\x0F', '\x12', '\x16', '\x1D', '\x1F'};
    if (reason == UCNV_ILLEGAL && length == 1 &&
        scAllowedChars.count(*codeUnits)) {
        *err = U_ZERO_ERROR;
        UChar c = *codeUnits;
        ucnv_cbToUWriteUChars(toArgs, &c, 1, 0, err);
        return;
    }
    Csock::IcuExtToUCallback(toArgs, codeUnits, length, reason, err);
}

void CIRCSocket::IcuExtFromUCallback(UConverterFromUnicodeArgs* fromArgs,
                                     const UChar* codeUnits, int32_t length,
                                     UChar32 codePoint,
                                     UConverterCallbackReason reason,
                                     UErrorCode* err) {
    // See comment in CIRCSocket::IcuExtToUCallback
    static const std::set<UChar32> scAllowedChars = {0x02, 0x03, 0x04, 0x0F,
                                                     0x12, 0x16, 0x1D, 0x1F};
    if (reason == UCNV_ILLEGAL && scAllowedChars.count(codePoint)) {
        *err = U_ZERO_ERROR;
        char c = codePoint;
        ucnv_cbFromUWriteBytes(fromArgs, &c, 1, 0, err);
        return;
    }
    Csock::IcuExtFromUCallback(fromArgs, codeUnits, length, codePoint, reason,
                               err);
}
#endif


CString CSocket::t_s(const CString& sEnglish, const CString& sContext) const {
    return GetModule()->t_s(sEnglish, sContext);
}

CInlineFormatMessage CSocket::t_f(const CString& sEnglish,
                                  const CString& sContext) const {
    return GetModule()->t_f(sEnglish, sContext);
}

CInlineFormatMessage CSocket::t_p(const CString& sEnglish,
                                  const CString& sEnglishes, int iNum,
                                  const CString& sContext) const {
    return GetModule()->t_p(sEnglish, sEnglishes, iNum, sContext);
}

CDelayedTranslation CSocket::t_d(const CString& sEnglish,
                                 const CString& sContext) const {
    return GetModule()->t_d(sEnglish, sContext);
}
