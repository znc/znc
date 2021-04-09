/*
 * Copyright (C) 2004-2019 ZNC, see the NOTICE file for details.
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
/**
 * @file Csocket.cc
 * @author Jim Hull <csocket@jimloco.com>
 *
 *    Copyright (c) 1999-2012 Jim Hull <csocket@jimloco.com>
 *    All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. Redistributions in any
 * form must be accompanied by information on how to obtain complete source code
 * for this software and any accompanying software that uses this software. The
 * source code must either be included in the distribution or be available for
 * no more than the cost of distribution plus a nominal fee, and must be freely
 * redistributable under reasonable conditions. For an executable file, complete
 * source code means the source code for all modules it contains. It does not
 * include source code for modules or files that typically accompany the major
 * components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHORS OF THIS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// TODO: fix code style, verify events called, asio vs boost asio
// vs networking ts in cmake, execsock, bytes read/written, verify error
// messages
//
// TODO: move encoding to a separate layer

#include <znc/Asio.h>

#if ZNC_USE_BOOST_ASIO

#include <boost/asio.hpp>
#ifdef HAVE_LIBSSL
#include <boost/asio/ssl.hpp>
#endif /* HAVE_LIBSSL */
#define asio boost::asio
#define ASIO_INITFN_RESULT_TYPE BOOST_ASIO_INITFN_RESULT_TYPE

#else /* ZNC_USE_BOOST_ASIO */

#include "asio.hpp"
#ifdef HAVE_LIBSSL
#include "asio/ssl.hpp"
#endif /* HAVE_LIBSSL */

#endif /* ZNC_USE_BOOST_ASIO */

#ifdef HAVE_ICU
#include <unicode/errorcode.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ustring.h>
#endif /* HAVE_ICU */

// TODO: review whether these functions are used/needed
static CString empty_string;
std::vector<Csock*> CSocketManager::FindSocksByName(const CString& sName) {
    return {};
}

Csock::Csock(int iTimeout) : Csock("", 0, iTimeout) {}
bool Csock::StartTLS() { return true; }
#ifdef HAVE_LIBSSL
long Csock::GetPeerFingerprint(CString& sFP) const { return 0; }
void Csock::SetPemPass(const CString& sPassword) { m_sPemPass = sPassword; }
#endif
bool Csock::AcceptSSL() { return true; }
bool Csock::ConnectSSL() { return true; }
bool Csock::SSLClientSetup() { return true; }
bool Csock::SSLServerSetup() { return true; }
int Csock::GetPending() const { return 0; }
ssize_t Csock::Read(char*, size_t len) { return 0; }
void Csock::SetRate(uint32_t iBytes, uint64_t iMilliseconds) {}
Csock* Csock::GetSockObj(const CString& sHostname, uint16_t iPort) {
    return nullptr;
}
CString Csock::GetLocalIP() const { return ""; }
CString Csock::GetRemoteIP() const { return ""; }
void Csock::ReachedMaxBuffer() {}
bool Csock::CheckTimeout(time_t iNow) { return true; }
uint16_t Csock::GetRemotePort() const { return 0; }
uint16_t Csock::GetLocalPort() const { return 0; }
uint32_t Csock::GetMaxBufferThreshold() const { return 0; }
uint64_t Csock::GetBytesWritten() const { return 0; }
uint64_t Csock::GetBytesRead() const { return 0; }
void Csock::ResetBytesRead() {}
void Csock::ResetBytesWritten() {}
CString& Csock::GetInternalReadBuffer() { return empty_string; }
CString& Csock::GetInternalWriteBuffer() { return empty_string; }

////////////////////////////////////////////////////////////////////////

namespace znc_asio {

struct IoContext {
    asio::io_context context;
};

struct Timer {
    Timer(CCron* pCron, asio::io_context& ctx, CSockCommon* pSockCommon)
        : m_pCron(pCron), m_timer(ctx), m_pSockCommon(pSockCommon) {}

    void Schedule() {
        if (!m_pCron->isValid()) {
            m_pSockCommon->DelCronByAddr(m_pCron);
            return;
        }

        if (m_pCron->m_bRunOnNextCall) {
            m_pCron->m_bRunOnNextCall = false;
            if (m_pCron->m_iMaxCycles > 0) ++m_pCron->m_iCycles;
            m_pCron->RunJob();
        }

        if (m_pCron->m_iMaxCycles > 0 &&
            ++m_pCron->m_iCycles > m_pCron->m_iMaxCycles) {
            m_pSockCommon->DelCronByAddr(m_pCron);
            return;
        }
        timeval duration = m_pCron->GetInterval();
        m_timer.expires_after(std::chrono::seconds(duration.tv_sec) +
                              std::chrono::microseconds(duration.tv_usec));
        m_timer.async_wait([this](const std::error_code& e) {
            if (e) return;
            if (m_pCron->isValid()) {
                m_pCron->RunJob();
            }
            Schedule();
        });
    }

    CCron* m_pCron;
    asio::steady_timer m_timer;
    CSockCommon* m_pSockCommon;
};

struct FDMonitor {
    FDMonitor(CSMonitorFD* pMonitorFD, asio::io_context& ctx)
        : m_pMonitorFD(pMonitorFD) {
        m_descriptors.reserve(pMonitorFD->m_miiMonitorFDs.size());
        for (const auto& i : pMonitorFD->m_miiMonitorFDs) {
            auto fd = i.first;
            m_descriptors.emplace_back(ctx, fd);
            m_descriptors.back().async_wait(
                asio::posix::stream_descriptor::wait_read,
                [=](const std::error_code& e) {
                    if (e) return;
                    m_pMonitorFD->FDsThatTriggered({{fd, 1}});
                });
        }
    }

    CSMonitorFD* m_pMonitorFD;
    std::vector<asio::posix::stream_descriptor> m_descriptors;
};

template <typename Stream>
struct base_layer : private asio::noncopyable {
    using next_layer_type = typename std::remove_reference<Stream>::type;
    using lowest_layer_type = typename next_layer_type::lowest_layer_type;
    using executor_type = typename lowest_layer_type::executor_type;

    template <typename Arg>
    base_layer(Arg& arg) : m_next_layer(arg) {}

    next_layer_type& next_layer() { return m_next_layer; }
    lowest_layer_type& lowest_layer() { return m_next_layer.lowest_layer(); }
    const lowest_layer_type& lowest_layer() const {
        return m_next_layer.lowest_layer();
    }
    executor_type get_executor() ASIO_NOEXCEPT {
        return m_next_layer.lowest_layer().get_executor();
    }

    void close(std::error_code& ec) { m_next_layer.close(ec); }

    template <typename ConstBufferSequence, typename WriteHandler>
    ASIO_INITFN_RESULT_TYPE(WriteHandler, void(std::error_code, std::size_t))
    async_write_some(const ConstBufferSequence& buffers,
                     WriteHandler&& handler) {
        return m_next_layer.async_write_some(
            buffers, std::forward<WriteHandler>(handler));
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    ASIO_INITFN_RESULT_TYPE(ReadHandler, void(std::error_code, std::size_t))
    async_read_some(const MutableBufferSequence& buffers,
                    ReadHandler&& handler) {
        return m_next_layer.async_read_some(buffers,
                                            std::forward<ReadHandler>(handler));
    }

    template <typename Endpoint, typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void(std::error_code))
    async_connect(Endpoint&& ep, Handler&& handler) {
        return m_next_layer.async_connect(std::forward<Endpoint>(ep),
                                          std::forward<Handler>(handler));
    }

    template <typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void(std::error_code))
    async_accept(Handler&& handler) {
        handler(std::error_code());
    }

  private:
    Stream m_next_layer;
};

template <typename Stream>
struct noop_layer : base_layer<Stream> {};

template <typename Stream, typename Layer>
struct optional_layer : base_layer<Stream> {
    template <typename Arg>
    optional_layer(Arg& arg) : base_layer<Stream>(arg) {}

    template <typename... Arg>
    void enable(Arg&&... arg) {
        m_layer.reset(new Layer(base_layer<Stream>::next_layer(),
                                std::forward<Arg>(arg)...));
    }

    template <typename ConstBufferSequence, typename WriteHandler>
    ASIO_INITFN_RESULT_TYPE(WriteHandler, void(std::error_code, std::size_t))
    async_write_some(const ConstBufferSequence& buffers,
                     WriteHandler&& handler) {
        if (m_layer) {
            return m_layer->async_write_some(
                buffers, std::forward<WriteHandler>(handler));
        } else {
            return base_layer<Stream>::async_write_some(
                buffers, std::forward<WriteHandler>(handler));
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    ASIO_INITFN_RESULT_TYPE(ReadHandler, void(std::error_code, std::size_t))
    async_read_some(const MutableBufferSequence& buffers,
                    ReadHandler&& handler) {
        if (m_layer) {
            return m_layer->async_read_some(buffers,
                                            std::forward<ReadHandler>(handler));
        } else {
            return base_layer<Stream>::async_read_some(
                buffers, std::forward<ReadHandler>(handler));
        }
    }

    template <typename Endpoint, typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void(std::error_code))
    async_connect(Endpoint&& ep, Handler&& handler) {
        if (m_layer) {
            return m_layer->async_connect(std::forward<Endpoint>(ep),
                                          std::forward<Handler>(handler));
        } else {
            return base_layer<Stream>::async_connect(
                std::forward<Endpoint>(ep), std::forward<Handler>(handler));
        }
    }

    //  private:
    std::unique_ptr<Layer> m_layer;
};

#ifdef HAVE_LIBSSL
static void ConfigureCTXOptions(const Csock* pSock, asio::ssl::context* ctx,
                                std::error_code& e) {
    if (SSL_CTX_set_cipher_list(ctx->native_handle(),
                                pSock->GetCipher().c_str()) <= 0) {
        CS_DEBUG("Could not assign cipher [" << pSock->GetCipher() << "]");
        e = asio::error::invalid_argument;
        return;
    }
    asio::ssl::context::options options =
        asio::ssl::context::default_workarounds;
    if (Csock::EDP_SSLv2 & pSock->DisabledSSLProtocols())
        options |= asio::ssl::context::no_sslv2;
    if (Csock::EDP_SSLv3 & pSock->DisabledSSLProtocols())
        options |= asio::ssl::context::no_sslv3;
    if (Csock::EDP_TLSv1 & pSock->DisabledSSLProtocols())
        options |= asio::ssl::context::no_tlsv1;
    if (Csock::EDP_TLSv1_1 & pSock->DisabledSSLProtocols())
        options |= asio::ssl::context::no_tlsv1_1;
    if (Csock::EDP_TLSv1_2 & pSock->DisabledSSLProtocols())
        options |= asio::ssl::context::no_tlsv1_2;
    ctx->set_options(options, e);
}

template <typename Stream>
struct optional_ssl_layer : optional_layer<Stream, asio::ssl::stream<Stream>> {
    template <typename Arg>
    optional_ssl_layer(Arg& arg)
        : optional_layer<Stream, asio::ssl::stream<Stream>>(arg) {}

    template <typename Endpoint, typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void(std::error_code))
    async_connect(Endpoint&& ep, Handler handler) {
        return base_layer<Stream>::async_connect(
            std::forward<Endpoint>(ep), [=](const std::error_code& e) {
                if (e) {
                    handler(e);
                    return;
                }
                if (this->m_layer) {
                    this->m_layer->async_handshake(
                        asio::ssl::stream<Stream>::client, handler);
                } else {
                    handler(std::error_code());
                }
            });
    }

    template <typename Handler>
    ASIO_INITFN_RESULT_TYPE(Handler, void(std::error_code))
    async_accept(Handler&& handler) {
        if (this->m_layer) {
            this->m_layer->async_handshake(asio::ssl::stream<Stream>::server,
                                           std::forward<Handler>(handler));
        } else {
            handler(std::error_code());
        }
    }

    std::shared_ptr<asio::ssl::context> m_sslctx;
};
#else
template <typename Stream>
struct optional_ssl_layer : base_layer<Stream> {
    template <typename Arg>
    optional_ssl_layer(Arg& arg) : base_layer<Stream>(arg) {}
};
#endif

struct Pipeline : std::enable_shared_from_this<Pipeline> {
    template <typename SocketSource>
    Pipeline(asio::io_context& ctx, SocketSource&& s0, CSocketManager* pManager,
             Csock* pSock)
        : m_resolver(ctx),
          m_s1(std::forward<SocketSource>(s0)),
          m_s2(m_s1),
          m_pSock(pSock),
          m_pManager(pManager),
          m_timer(ctx) {}

    void ResetTimeoutTimer() {
        m_pSock->SetLastCheckTimeout(time(0));
        m_timer.expires_after(std::chrono::seconds(m_pSock->GetTimeout()));
        m_timer.async_wait([this](const std::error_code& e) {
            if (e) return;
            m_pSock->Timeout();
            m_pManager->Dereference(m_pSock);
        });
    }

    void Close() {
        std::error_code e;
        m_s2.close(e);
    }

    void Write(std::string s) {
        m_sWriteBuffer[!m_iLockedWriteBuffer] += std::move(s);
        TryStartWrite();
    }
    void TryStartWrite() {
        if (!m_bReadyToWrite) return;
        if (m_bWriting) return;
        if (m_sWriteBuffer[!m_iLockedWriteBuffer].empty()) {
            if (m_bClosing) {
                auto self = shared_from_this();
                asio::defer(m_pManager->GetContext()->context, [this, self]() {
                    m_pManager->DelSockByAddr(m_pSock);
                });
            }
            return;
        }
        m_bWriting = true;
        m_iLockedWriteBuffer = !m_iLockedWriteBuffer;
        auto self = shared_from_this();
        asio::async_write(m_s2,
                          asio::buffer(m_sWriteBuffer[m_iLockedWriteBuffer]),
                          [this, self](const std::error_code& e, size_t bytes) {
                              if (e) {
                                  if (e == asio::error::operation_aborted)
                                      return;
                                  m_pSock->SockError(e.value(), e.message());
                                  m_pManager->DelSockByAddr(m_pSock);
                                  return;
                              }
                              m_sWriteBuffer[m_iLockedWriteBuffer].clear();
                              m_bWriting = false;
                              TryStartWrite();
                          });
    }
    void Connect() {
        DEBUG("asio connect: " << m_pSock->GetSockName() << " "
                               << m_pSock->GetHostName() << " "
                               << m_pSock->GetPort());
        ResetTimeoutTimer();
#ifdef HAVE_LIBSSL
        if (m_pSock->GetSSL()) {
            m_s2.m_sslctx = std::make_shared<asio::ssl::context>(
                asio::ssl::context::sslv23);
            std::error_code e;
            m_s2.m_sslctx->set_default_verify_paths(e);
            if (!e) {
                m_s2.m_sslctx->set_verify_mode(asio::ssl::verify_peer, e);
            }
            if (!e) {
                m_s2.m_sslctx->set_verify_callback(
                    [=](bool bPreverified, asio::ssl::verify_context& ctx) {
                        bool bSuccess = m_pSock->VerifyPeerCertificate(
                            bPreverified, ctx.native_handle());
                        if (!bSuccess) {
                            m_bFailedSSL = true;
                        }
                        return bSuccess;
                    },
                    e);
            }
            if (!e) {
                ConfigureCTXOptions(m_pSock, m_s2.m_sslctx.get(), e);
            }
            if (e) {
                m_pSock->SockError(e.value(), e.message());
                m_pManager->DelSockByAddr(m_pSock);
                return;
            }
            m_s2.enable(*m_s2.m_sslctx);
#if defined(SSL_set_tlsext_host_name)
            CString sSNIHostname;
            if (m_pSock->SNIConfigureClient(sSNIHostname)) {
                SSL_set_tlsext_host_name(m_s2.m_layer->native_handle(),
                                         sSNIHostname.c_str());
            }
#endif /* SSL_set_tlsext_host_name */
        }
#endif /* HAVE_LIBSSL */
        // TODO: use std::future with when_all() from Concurrency TS instead
        struct DNSTask {
            asio::ip::tcp::resolver::results_type bind_results;
            asio::ip::tcp::resolver::results_type target_results;
            bool bind_done = false;
            bool target_done = false;
            std::error_code bind_error;
            std::error_code target_error;
        };
        DNSTask* task = new DNSTask;
        auto try_finish_task = [=]() {
            if (!task->bind_done || !task->target_done) {
                return;
            }
            std::unique_ptr<DNSTask> task_deleter(task);
            if (task->target_error) {
                if (task->target_error == asio::error::operation_aborted)
                    return;
                m_pSock->SockError(task->target_error.value(),
                                   task->target_error.message());
                m_pManager->DelSockByAddr(m_pSock);
                return;
            }
            if (task->bind_error) {
                if (task->bind_error == asio::error::operation_aborted) return;
                m_pSock->SockError(task->bind_error.value(),
                                   task->bind_error.message());
                m_pManager->DelSockByAddr(m_pSock);
                return;
            }
            // TODO: happy eyeballs
            if (task->bind_results.empty()) {
                DEBUG("asio resolved: " << m_pSock->GetSockName() << " to "
                                        << task->target_results->endpoint());
            } else {
                DEBUG("asio resolved: " << m_pSock->GetSockName() << " from "
                                        << task->bind_results->endpoint()
                                        << " to "
                                        << task->target_results->endpoint());
                std::error_code e;
                m_s1.bind(task->bind_results->endpoint(), e);
                if (e) {
                    if (e == asio::error::operation_aborted) return;
                    m_pSock->SockError(e.value(), e.message());
                    m_pManager->DelSockByAddr(m_pSock);
                    return;
                }
            }
            ResetTimeoutTimer();
            m_s2.async_connect(task->target_results->endpoint(),
                               [this](const std::error_code& e) {
                                   if (e) {
                                       if (e == asio::error::operation_aborted)
                                           return;
                                       if (!m_bFailedSSL) {
                                           // The error was reported already.
                                           m_pSock->SockError(e.value(),
                                                              e.message());
                                       }
                                       m_pManager->DelSockByAddr(m_pSock);
                                       return;
                                   }
                                   m_pSock->SetIsConnected(true);
                                   m_pSock->Connected();
                                   ConnectionEstablished();
                               });
        };
        if (m_pSock->GetBindHost().empty()) {
            task->bind_done = true;
        } else {
            m_resolver.async_resolve(
                m_pSock->GetBindHost(), "",
                asio::ip::tcp::resolver::address_configured |
                    asio::ip::tcp::resolver::passive,
                [=](const std::error_code& err,
                    const asio::ip::tcp::resolver::results_type& res) {
                    task->bind_results = res;
                    task->bind_error = err;
                    task->bind_done = true;
                    try_finish_task();
                });
        }
        m_resolver.async_resolve(
            m_pSock->GetHostName(), CString(m_pSock->GetPort()),
            asio::ip::tcp::resolver::address_configured,
            [=](const std::error_code& err,
                const asio::ip::tcp::resolver::results_type& res) {
                task->target_results = res;
                task->target_error = err;
                task->target_done = true;
                try_finish_task();
            });
    }

    void ConnectionEstablished() {
        m_bReadyToWrite = true;
        TryStartWrite();
        ContinueReading();
    }

    void ContinueReading() {
        ResetTimeoutTimer();
        auto self = shared_from_this();
        m_s2.async_read_some(
            asio::buffer(m_sReadBuf),
            [this, self](const std::error_code& e, std::size_t bytes) {
                if (e == asio::error::operation_aborted) return;
                if (e) {
                    if (e != asio::error::eof)
                        m_pSock->SockError(e.value(), e.message());
                    m_pManager->DelSockByAddr(m_pSock);
                    return;
                }
                if (bytes) {
                    m_pSock->ReadData(m_sReadBuf, bytes);
                    m_pSock->PushBuff(m_sReadBuf, bytes);
                    ContinueReading();
                }
            });
    }

    void Accept() {
#ifdef HAVE_LIBSSL
        if (m_pSock->GetSSL()) {
            m_s2.enable(*m_s2.m_sslctx);
        }
#endif
        m_s2.async_accept([=](const std::error_code& e) {
            if (e) {
                if (e == asio::error::operation_aborted) return;
                m_pSock->SockError(e.value(), e.message());
                m_pManager->DelSockByAddr(m_pSock);
                return;
            }
            ConnectionEstablished();
        });
    }

    char m_sReadBuf[1000] = {};
    asio::ip::tcp::resolver m_resolver;
    asio::ip::tcp::socket m_s1;
    optional_ssl_layer<asio::ip::tcp::socket&> m_s2;
    bool m_bWriting = false;
    bool m_bReadyToWrite = false;
    bool m_bClosing = false;
    int m_iLockedWriteBuffer = 0;
    CString m_sWriteBuffer[2];
    Csock* m_pSock;
    CSocketManager* m_pManager;
    asio::steady_timer m_timer;
    bool m_bFailedSSL = false;
};

struct Acceptor {
    Acceptor(asio::io_context& ctx, CSocketManager* pManager, Csock* pSock)
        : m_resolver(ctx),
          m_acceptor(ctx),
#ifdef HAVE_LIBSSL
          m_sslctx(
              std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23)),
#endif
          m_pSock(pSock),
          m_pManager(pManager) {
    }

    void StartListening(const asio::ip::tcp::endpoint& ep,
                        std::error_code& ec) {
        m_acceptor.open(ep.protocol(), ec);
        if (ec) return;
        m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
        if (ec) return;
#ifdef HAVE_IPV6
        if (m_pSock->GetIPv6()) {
            m_acceptor.set_option(asio::ip::v6_only(m_pSock->GetAFRequire() ==
                                                    CSSockAddr::RAF_INET6),
                                  ec);
        }
#endif
        m_acceptor.bind(ep, ec);
        if (ec) return;
        m_acceptor.listen(asio::ip::tcp::acceptor::max_listen_connections, ec);
        if (ec) return;
#ifdef HAVE_LIBSSL
        if (m_pSock->GetSSL()) {
            m_sslctx->use_private_key_file(m_pSock->GetKeyLocation(),
                                           asio::ssl::context::pem, ec);
            if (ec) return;
            m_sslctx->use_certificate_chain_file(m_pSock->GetPemLocation(), ec);
            if (ec) return;
            m_sslctx->use_tmp_dh_file(m_pSock->GetDHParamLocation(), ec);
            if (ec) return;
            ConfigureCTXOptions(m_pSock, m_sslctx.get(), ec);
            if (ec) return;
        }
#endif
        InitiateAccept();
    }

    void InitiateAccept() {
        m_acceptor.async_accept(
            [this](const std::error_code& e, asio::ip::tcp::socket peer) {
                if (e) {
                    if (e == asio::error::operation_aborted) return;
                    m_pSock->SockError(e.value(), e.message());
                    m_pManager->DelSockByAddr(m_pSock);
                    return;
                }
                InitiateAccept();

                const auto ep = peer.remote_endpoint();
                CString sIP = ep.address().to_string();
                sIP.TrimPrefix("::ffff:");
                if (!m_pSock->ConnectionFrom(sIP, ep.port())) return;

                Csock* pNewSock = m_pSock->GetSockObj(sIP, ep.port());
                pNewSock->SetType(Csock::INBOUND);
#ifdef HAVE_ICU
                pNewSock->SetEncoding(m_pSock->GetEncoding());
#endif
                pNewSock->SetSSL(m_pSock->GetSSL());
                auto pipe = std::make_shared<znc_asio::Pipeline>(
                    m_pManager->GetContext()->context, std::move(peer),
                    m_pManager, pNewSock);
                pNewSock->SetPipeline(pipe);
#ifdef HAVE_LIBSSL
                pipe->m_s2.m_sslctx = m_sslctx;
#endif
                pNewSock->SetIsConnected(true);
                if (pNewSock->GetSockName().empty()) {
                    std::stringstream s;
                    s << sIP << ":" << ep.port();
                    m_pManager->AddSock(pNewSock, s.str());
                } else {
                    m_pManager->AddSock(pNewSock, pNewSock->GetSockName());
                }
            });
    }

    asio::ip::tcp::resolver m_resolver;
    asio::ip::tcp::acceptor m_acceptor;
#ifdef HAVE_LIBSSL
    std::shared_ptr<asio::ssl::context> m_sslctx;
#endif
    Csock* m_pSock;
    CSocketManager* m_pManager;
};
}  // namespace znc_asio

#ifdef HAVE_ICU
inline bool icuConv(const CString& src, CString& tgt, UConverter* cnv_in,
                    UConverter* cnv_out) {
    const char* indata = src.c_str();
    const char* indataend = indata + src.length();
    tgt.clear();
    char buf[100];
    UChar pivotStart[100];
    UChar* pivotSource = pivotStart;
    UChar* pivotTarget = pivotStart;
    UChar* pivotLimit = pivotStart + sizeof pivotStart / sizeof pivotStart[0];
    const char* outdataend = buf + sizeof buf;
    bool reset = true;
    while (true) {
        char* outdata = buf;
        UErrorCode e = U_ZERO_ERROR;
        ucnv_convertEx(cnv_out, cnv_in, &outdata, outdataend, &indata,
                       indataend, pivotStart, &pivotSource, &pivotTarget,
                       pivotLimit, reset, true, &e);
        reset = false;
        if (U_SUCCESS(e)) {
            if (e != U_ZERO_ERROR) {
                CS_DEBUG("Warning during converting string encoding: "
                         << u_errorName(e));
            }
            tgt.append(buf, outdata - buf);
            break;
        }
        if (e == U_BUFFER_OVERFLOW_ERROR) {
            tgt.append(buf, outdata - buf);
            continue;
        }
        CS_DEBUG("Error during converting string encoding: " << u_errorName(e));
        return false;
    }
    return true;
}

static bool isUTF8(const CString& src, CString& target) {
    UErrorCode e = U_ZERO_ERROR;
    // Convert to UTF-16 without actually converting. This will still count
    // the number of output characters, so we either get
    // U_BUFFER_OVERFLOW_ERROR or U_INVALID_CHAR_FOUND.
    u_strFromUTF8(NULL, 0, NULL, src.c_str(), (int32_t)src.length(), &e);
    if (e != U_BUFFER_OVERFLOW_ERROR) return false;
    target = src;
    return true;
}
#endif /* HAVE_ICU */

void CSocketManager::Connect(const CSConnection& cCon, Csock* pcSock) {
    pcSock->SetHostName(cCon.GetHostname());
    pcSock->SetPort(cCon.GetPort());
    pcSock->SetTimeout(cCon.GetTimeout());

    if (cCon.GetAFRequire() != CSSockAddr::RAF_ANY)
        pcSock->SetAFRequire(cCon.GetAFRequire());

    // bind the vhost
    pcSock->SetBindHost(cCon.GetBindHost());

#ifdef HAVE_LIBSSL
    pcSock->SetSSL(cCon.GetIsSSL());
    if (cCon.GetIsSSL()) {
        if (!cCon.GetPemLocation().empty()) {
            pcSock->SetDHParamLocation(cCon.GetDHParamLocation());
            pcSock->SetKeyLocation(cCon.GetKeyLocation());
            pcSock->SetPemLocation(cCon.GetPemLocation());
            pcSock->SetPemPass(cCon.GetPemPass());
        }
        if (!cCon.GetCipher().empty()) pcSock->SetCipher(cCon.GetCipher());
    }
#endif /* HAVE_LIBSSL */

    pcSock->SetType(Csock::OUTBOUND);

    pcSock->SetConState(Csock::CST_START);
    pcSock->SetPipeline(std::make_shared<znc_asio::Pipeline>(
        m_iocontext->context, m_iocontext->context, this, pcSock));
    AddSock(pcSock, cCon.GetSockName());
}
bool CSocketManager::Listen(const CSListener& cListen, Csock* pcSock,
                            uint16_t* piRandPort) {
    if (cListen.GetAFRequire() != CSSockAddr::RAF_ANY) {
        pcSock->SetAFRequire(cListen.GetAFRequire());
#ifdef HAVE_IPV6
        if (cListen.GetAFRequire() == CSSockAddr::RAF_INET6)
            pcSock->SetIPv6(true);
#endif /* HAVE_IPV6 */
    }
#ifdef HAVE_IPV6
    else {
        pcSock->SetIPv6(true);
    }
#endif /* HAVE_IPV6 */
#ifdef HAVE_LIBSSL
    pcSock->SetSSL(cListen.GetIsSSL());
    if (cListen.GetIsSSL() && !cListen.GetPemLocation().empty()) {
        pcSock->SetDHParamLocation(cListen.GetDHParamLocation());
        pcSock->SetKeyLocation(cListen.GetKeyLocation());
        pcSock->SetPemLocation(cListen.GetPemLocation());
        pcSock->SetPemPass(cListen.GetPemPass());
        pcSock->SetCipher(cListen.GetCipher());
        pcSock->SetRequireClientCertFlags(cListen.GetRequireClientCertFlags());
    }
#endif /* HAVE_LIBSSL */

    if (piRandPort) *piRandPort = 0;

    bool bDetach = (cListen.GetDetach() &&
                    !piRandPort);  // can't detach if we're waiting for the port
                                   // to come up right now
    pcSock->SetType(Csock::LISTENER);
    pcSock->SetAcceptor(std::unique_ptr<znc_asio::Acceptor>(
        new znc_asio::Acceptor(m_iocontext->context, this, pcSock)));
    if (pcSock->Listen(cListen.GetPort(), cListen.GetMaxConns(),
                       cListen.GetBindHost(), cListen.GetTimeout(), bDetach)) {
        AddSock(pcSock, cListen.GetSockName());
        if (piRandPort && cListen.GetPort() == 0) {
            // TODO: implement
        }
        return true;
    }

    delete pcSock;
    return false;
}
const CString& Csock::GetSockName() const { return m_sSockName; }
void Csock::SetSockName(const CString& sName) { m_sSockName = sName; }
const CString& Csock::GetHostName() const { return m_shostname; }
void Csock::SetHostName(const CString& sHostname) { m_shostname = sHostname; }
void Csock::SetPort(uint16_t iPort) { m_uPort = iPort; }
uint16_t Csock::GetPort() const { return m_uPort; }
bool Csock::IsConnected() const { return m_bIsConnected; }
void Csock::SetIsConnected(bool b) { m_bIsConnected = b; }
bool Csock::GetSSL() const { return m_bUseSSL; }
void Csock::SetSSL(bool b) { m_bUseSSL = b; }
void Csock::SetPipeline(std::shared_ptr<znc_asio::Pipeline> pipeline) {
    m_pipeline = std::move(pipeline);
}
void Csock::SetAcceptor(std::unique_ptr<znc_asio::Acceptor> acceptor) {
    m_acceptor = std::move(acceptor);
}
void CCron::SetTimer(std::unique_ptr<znc_asio::Timer> timer) {
    m_timer = std::move(timer);
    asio::defer(m_timer->m_timer.get_executor(),
                [=]() { m_timer->Schedule(); });
}
CSocketManager::CSocketManager() : m_iocontext(new znc_asio::IoContext) {
    m_pContext = m_iocontext.get();
}
void CSocketManager::DynamicSelectLoop(uint64_t iLowerBounds,
                                       uint64_t iUpperBounds,
                                       time_t iMaxResolution) {
    Loop();
}
void CSocketManager::Loop() {
    m_iocontext->context.restart();
    m_iocontext->context.run_one();
}
void CSocketManager::AddSock(Csock* pcSock, const CString& sSockName) {
    pcSock->m_pContext = m_pContext;
    for (CCron* pcCron : pcSock->m_vcCrons) {
        pcCron->SetTimer(std::unique_ptr<znc_asio::Timer>(
            new znc_asio::Timer(pcCron, m_pContext->context, pcSock)));
    }

    pcSock->SetSockName(sSockName);
    m_sockets.insert(pcSock);
    switch (pcSock->GetType()) {
        case Csock::OUTBOUND:
            pcSock->Connect();
            break;
        case Csock::INBOUND:
            pcSock->Accept();
            break;
        case Csock::LISTENER:
            break;
    }
}

Csock::Csock(const CString& sHostname, uint16_t uPort, int iTimeout)
    : m_bIsIPv6(false),
      m_bUseSSL(false),
      m_eCloseType(Csock::CLT_DONT),
      m_bPauseRead(false),
      m_bIsConnected(false),
      m_iLastCheckTimeoutTime(time(0)),
      m_iTimeout(iTimeout),
      m_sCipherType("ALL") {
#ifdef HAVE_ICU
    m_cnvTryUTF8 = false;
    m_cnvSendUTF8 = false;
    UErrorCode e = U_ZERO_ERROR;
    m_cnvExt = nullptr;
    m_cnvInt = ucnv_open("UTF-8", &e);
#endif /* HAVE_ICU */
}

bool Csock::Listen(uint16_t iPort, int iMaxConns, const CString& sBindHost,
                   uint32_t iTimeout, bool bDetach) {
    std::error_code e;
    if (!sBindHost.empty()) {
        if (bDetach) {
            m_acceptor->m_resolver.async_resolve(
                sBindHost, CString(iPort),
                asio::ip::tcp::resolver::address_configured |
                    asio::ip::tcp::resolver::passive,
                [this](const std::error_code& e,
                       asio::ip::tcp::resolver::results_type results) {
                    if (e) {
                        if (e == asio::error::operation_aborted) return;
                        SockError(e.value(), e.message());
                        m_acceptor->m_pManager->DelSockByAddr(this);
                        return;
                    }
                    // TODO: select result from the list of results based on
                    // GetAFRequire()
                    std::error_code ee;
                    m_acceptor->StartListening(*results, ee);
                    if (ee) {
                        if (ee == asio::error::operation_aborted) return;
                        SockError(ee.value(), ee.message());
                        m_acceptor->m_pManager->DelSockByAddr(this);
                    }
                });
        } else {
            auto results = m_acceptor->m_resolver.resolve(
                sBindHost, CString(iPort),
                asio::ip::tcp::resolver::address_configured |
                    asio::ip::tcp::resolver::passive,
                e);
            if (e) {
                SockError(e.value(), e.message());
                return false;
            }
            // TODO: select result from the list of results based on
            // GetAFRequire()
            m_acceptor->StartListening(*results, e);
            if (e) {
                SockError(e.value(), e.message());
                return false;
            }
        }
        return true;
    }
    if (!sBindHost.empty()) {
    } else {
        const auto protocol =
#ifdef HAVE_IPV6
            GetIPv6() ? asio::ip::tcp::v6() :
#endif
                      asio::ip::tcp::v4();
        m_acceptor->StartListening(asio::ip::tcp::endpoint(protocol, iPort), e);
        if (e) {
            SockError(e.value(), e.message());
            return false;
        }
    }
    return true;
}
void Csock::Accept() { m_pipeline->Accept(); }
void Csock::Connect() { m_pipeline->Connect(); }
void Csock::CallSockError(int iErrno, const CString& s) {
    SockError(iErrno, s);
}
void Csock::PushBuff(const char* data, size_t len, bool bStartAtZero) {
    if (!m_bEnableReadLine)
        return;  // If the ReadLine event is disabled, just ditch here

    size_t iStartPos =
        (m_sbuffer.empty() || bStartAtZero ? 0 : m_sbuffer.length() - 1);

    if (data) m_sbuffer.append(data, len);

    while (!m_bPauseRead && GetCloseType() == CLT_DONT) {
        CString::size_type iFind = m_sbuffer.find("\n", iStartPos);

        if (iFind != CString::npos) {
            CString sBuff = m_sbuffer.substr(
                0, iFind + 1);              // read up to(including) the newline
            m_sbuffer.erase(0, iFind + 1);  // erase past the newline
#ifdef HAVE_ICU
            if (m_cnvExt) {
                CString sUTF8;
                if ((m_cnvTryUTF8 &&
                     isUTF8(sBuff, sUTF8))  // maybe it's already UTF-8?
                    || icuConv(sBuff, sUTF8, m_cnvExt, m_cnvInt)) {
                    ReadLine(sUTF8);
                } else {
                    CS_DEBUG("Can't convert received line to UTF-8");
                }
            } else
#endif /* HAVE_ICU */
            {
                ReadLine(sBuff);
            }
            iStartPos = 0;  // reset this back to 0, since we need to look for
                            // the next newline here.
        } else {
            break;
        }
    }

    if (m_iMaxStoredBufferLength > 0 &&
        m_sbuffer.length() > m_iMaxStoredBufferLength)
        ReachedMaxBuffer();  // call the max read buffer event
}
void Csock::EnableReadLine() { m_bEnableReadLine = true; }
void Csock::DisableReadLine() {
    m_bEnableReadLine = false;
    m_sbuffer.clear();
}
bool Csock::Write(const CString& sData) {
#ifdef HAVE_ICU
    if (m_cnvExt && !m_cnvSendUTF8) {
        CString sBinary;
        if (icuConv(sData, sBinary, m_cnvInt, m_cnvExt)) {
            return Write(sBinary.c_str(), sBinary.length());
        }
    }
    // can't convert our UTF-8 string to that encoding, just put it as is...
#endif /* HAVE_ICU */
    return Write(sData.c_str(), sData.length());
}

bool Csock::Write(const char* data, size_t len) {
    m_pipeline->Write(CString(data, len));
    return true;
}
Csock::ETConn Csock::GetType() const { return m_iConnType; }
void Csock::SetType(Csock::ETConn iType) { m_iConnType = iType; }
bool CSocketManager::SwapSockByAddr(Csock* pNewSock, Csock* pOrigSock) {
    if (m_sockets.count(pOrigSock)) {
        pNewSock->MoveFrom(*pOrigSock);
        Dereference(pOrigSock);
        m_sockets.insert(pNewSock);
        return true;
    }
    return false;
}
void Csock::MoveFrom(Csock& other) {
    m_pipeline = std::move(other.m_pipeline);
    m_pipeline->m_pSock = this;

    m_iTcount = other.m_iTcount;
    m_iLastCheckTimeoutTime = other.m_iLastCheckTimeoutTime;
    m_uPort = other.m_uPort;
    m_iRemotePort = other.m_iRemotePort;
    m_iLocalPort = other.m_iLocalPort;
    m_iReadSock = other.m_iReadSock;
    m_iWriteSock = other.m_iWriteSock;
    m_iTimeout = other.m_iTimeout;
    m_iMaxConns = other.m_iMaxConns;
    m_iConnType = other.m_iConnType;
    m_iMethod = other.m_iMethod;
    m_bUseSSL = other.m_bUseSSL;
    m_bIsConnected = other.m_bIsConnected;
    m_bsslEstablished = other.m_bsslEstablished;
    m_bEnableReadLine = other.m_bEnableReadLine;
    m_bPauseRead = other.m_bPauseRead;
    m_shostname = other.m_shostname;
    m_sbuffer = other.m_sbuffer;
    m_sSockName = other.m_sSockName;
    m_sKeyFile = other.m_sKeyFile;
    m_sDHParamFile = other.m_sDHParamFile;
    m_sPemFile = other.m_sPemFile;
    m_sCipherType = other.m_sCipherType;
    m_sSend = other.m_sSend;
    m_sPemPass = other.m_sPemPass;
    m_sLocalIP = other.m_sLocalIP;
    m_sRemoteIP = other.m_sRemoteIP;
    m_eCloseType = other.m_eCloseType;

    m_iMaxMilliSeconds = other.m_iMaxMilliSeconds;
    m_iLastSendTime = other.m_iLastSendTime;
    m_iBytesRead = other.m_iBytesRead;
    m_iBytesWritten = other.m_iBytesWritten;
    m_iStartTime = other.m_iStartTime;
    m_iMaxBytes = other.m_iMaxBytes;
    m_iLastSend = other.m_iLastSend;
    m_uSendBufferPos = other.m_uSendBufferPos;
    m_iMaxStoredBufferLength = other.m_iMaxStoredBufferLength;

    m_address = other.m_address;
    m_bindhost = other.m_bindhost;
    m_bIsIPv6 = other.m_bIsIPv6;
    m_bSkipConnect = other.m_bSkipConnect;
#ifdef HAVE_LIBSSL
    m_bNoSSLCompression = other.m_bNoSSLCompression;
    m_bSSLCipherServerPreference = other.m_bSSLCipherServerPreference;
    m_uDisableProtocols = other.m_uDisableProtocols;
    m_iRequireClientCertFlags = other.m_iRequireClientCertFlags;
    m_sSSLBuffer = other.m_sSSLBuffer;
#endif /* HAVE_LIBSSL */

#ifdef HAVE_ICU
    SetEncoding(other.m_sEncoding);
#endif

    CleanupCrons();
    m_vcCrons = other.m_vcCrons;
    m_vcMonitorFD = other.m_vcMonitorFD;

    m_eConState = other.m_eConState;
    m_sBindHost = other.m_sBindHost;
}
void CSockCommon::AddCron(CCron* pcCron) {
    m_vcCrons.insert(pcCron);
    if (m_pContext) {
        pcCron->SetTimer(std::unique_ptr<znc_asio::Timer>(
            new znc_asio::Timer(pcCron, m_pContext->context, this)));
    }
}
void CSockCommon::MonitorFD(CSMonitorFD* pMonitorFD) {
    m_vcMonitorFD.push_back(pMonitorFD);
    pMonitorFD->SetMonitor(std::unique_ptr<znc_asio::FDMonitor>(
        new znc_asio::FDMonitor(pMonitorFD, m_pContext->context)));
}
CCron::CCron()
    : m_iCycles(0),
      m_iMaxCycles(0),
      m_bActive(true),
      m_bPause(false),
      m_bRunOnNextCall(false) {}
CCron::~CCron() {}
void CCron::StartMaxCycles(double dTimeSequence, u_int iMaxCycles) {
    m_tTimeSequence.tv_sec = (time_t)dTimeSequence;
    // this could be done with modf(), but we're avoiding bringing in libm just
    // for the one function.
    m_tTimeSequence.tv_usec = (suseconds_t)(
        (dTimeSequence - (double)((time_t)dTimeSequence)) * 1000000);
    m_iMaxCycles = iMaxCycles;
    m_bActive = true;
}

void CCron::StartMaxCycles(const timeval& tTimeSequence, u_int iMaxCycles) {
    m_tTimeSequence = tTimeSequence;
    m_iMaxCycles = iMaxCycles;
    m_bActive = true;
}

void CCron::Start(double dTimeSequence) { StartMaxCycles(dTimeSequence, 0); }

void CCron::Start(const timeval& tTimeSequence) {
    StartMaxCycles(tTimeSequence, 0);
}

void CCron::Stop() { m_bActive = false; }

void CCron::Pause() {
    if (m_bPause) return;
    m_bPause = true;
    m_timer->m_timer.cancel();
}

void CCron::UnPause() {
    if (!m_bPause) return;
    m_bPause = false;
    asio::defer(m_timer->m_timer.get_executor(),
                [=]() { m_timer->Schedule(); });
}

void CCron::Reset() {
    Stop();
    Start(m_tTimeSequence);
}

timeval CCron::GetInterval() const { return m_tTimeSequence; }
u_int CCron::GetMaxCycles() const { return m_iMaxCycles; }
u_int CCron::GetCyclesLeft() const {
    return m_iMaxCycles > m_iCycles ? (m_iMaxCycles - m_iCycles) : 0;
}

bool CCron::isValid() const { return m_bActive; }
const CString& CCron::GetName() const { return m_sName; }
void CCron::SetName(const CString& sName) { m_sName = sName; }
void CCron::RunJob() { CS_DEBUG("This should be overridden"); }
CSockCommon::~CSockCommon() {
    CleanupCrons();
    CleanupFDMonitors();
}
void CSockCommon::CleanupCrons() {
    while (!m_vcCrons.empty()) {
        CCron* pCron = *m_vcCrons.begin();
        m_vcCrons.erase(pCron);
        delete pCron;
    }
}
void CSockCommon::CleanupFDMonitors() {
    for (size_t a = 0; a < m_vcMonitorFD.size(); ++a) delete m_vcMonitorFD[a];
    m_vcMonitorFD.clear();
}
void CSockCommon::DelCronByAddr(CCron* pCron) {
    if (m_vcCrons.count(pCron)) {
        m_vcCrons.erase(pCron);
        delete pCron;
    }
}
void CSocketManager::Dereference(Csock* pSock) {
    pSock->Close(Csock::CLT_DEREFERENCE);
    asio::defer(m_iocontext->context, [=]() { DelSockByAddr(pSock); });
}
void CSocketManager::DelSockByAddr(Csock* pSock) {
    if (m_sockets.count(pSock)) {
        m_sockets.erase(pSock);
        // only call disconnected event if connected event was called (i.e.
        // IsConnected was set)
        if (pSock->IsConnected()) {
            pSock->SetIsConnected(false);
            pSock->Disconnected();
        }
        delete pSock;
    }
}
#ifdef HAVE_ICU
void Csock::IcuExtToUCallback(UConverterToUnicodeArgs* toArgs,
                              const char* codeUnits, int32_t length,
                              UConverterCallbackReason reason,
                              UErrorCode* err) {
    if (reason <= UCNV_IRREGULAR) {
        *err = U_ZERO_ERROR;
        ucnv_cbToUWriteSub(toArgs, 0, err);
    }
}

void Csock::IcuExtFromUCallback(UConverterFromUnicodeArgs* fromArgs,
                                const UChar* codeUnits, int32_t length,
                                UChar32 codePoint,
                                UConverterCallbackReason reason,
                                UErrorCode* err) {
    if (reason <= UCNV_IRREGULAR) {
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteSub(fromArgs, 0, err);
    }
}

static void icuExtToUCallback(const void* context,
                              UConverterToUnicodeArgs* toArgs,
                              const char* codeUnits, int32_t length,
                              UConverterCallbackReason reason,
                              UErrorCode* err) {
    Csock* pcSock = (Csock*)context;
    pcSock->IcuExtToUCallback(toArgs, codeUnits, length, reason, err);
}

static void icuExtFromUCallback(const void* context,
                                UConverterFromUnicodeArgs* fromArgs,
                                const UChar* codeUnits, int32_t length,
                                UChar32 codePoint,
                                UConverterCallbackReason reason,
                                UErrorCode* err) {
    Csock* pcSock = (Csock*)context;
    pcSock->IcuExtFromUCallback(fromArgs, codeUnits, length, codePoint, reason,
                                err);
}

void Csock::SetEncoding(const CString& sEncoding) {
    if (m_cnvExt) ucnv_close(m_cnvExt);
    m_cnvExt = NULL;
    m_sEncoding = sEncoding;
    if (!sEncoding.empty()) {
        m_cnvTryUTF8 = sEncoding[0] == '*' || sEncoding[0] == '^';
        m_cnvSendUTF8 = sEncoding[0] == '^';
        const char* sEncodingName = sEncoding.c_str();
        if (m_cnvTryUTF8) sEncodingName++;
        UErrorCode e = U_ZERO_ERROR;
        m_cnvExt = ucnv_open(sEncodingName, &e);
        if (U_FAILURE(e)) {
            CS_DEBUG("Can't set encoding to " << sEncoding << ": "
                                              << u_errorName(e));
        }
        if (m_cnvExt) {
            ucnv_setToUCallBack(m_cnvExt, icuExtToUCallback, this, NULL, NULL,
                                &e);
            ucnv_setFromUCallBack(m_cnvExt, icuExtFromUCallback, this, NULL,
                                  NULL, &e);
        }
    }
}
#endif /* HAVE_ICU */
Csock::~Csock() {
    if (m_pipeline) {
        m_pipeline->m_bClosing = true;
        m_pipeline->Close();
    }
#ifdef HAVE_ICU
    if (m_cnvExt) ucnv_close(m_cnvExt);
    if (m_cnvInt) ucnv_close(m_cnvInt);
#endif
}
void Csock::Close(Csock::ECloseType eCloseType) {
    m_eCloseType = eCloseType;
    if (m_acceptor) {
        m_acceptor->m_pManager->DelSockByAddr(this);
        return;
    }
    if (m_pipeline) {
        if (eCloseType == CLT_NOW) {
            m_pipeline->m_bClosing = true;
            m_pipeline->m_pManager->DelSockByAddr(this);
            return;
        }
        if (eCloseType == CLT_AFTERWRITE) {
            m_pipeline->m_bClosing = true;
            m_pipeline->TryStartWrite();
            return;
        }
    }
}
CSocketManager::~CSocketManager() { Cleanup(); }
void CSocketManager::Cleanup() {
    CleanupCrons();
    CleanupFDMonitors();
    while (!m_sockets.empty()) {
        DelSockByAddr(*m_sockets.begin());
    }
}
void Csock::SetTimeout(int iTimeout, uint32_t) {
    m_iTimeout = iTimeout;
    if (m_pipeline) {
        m_pipeline->ResetTimeoutTimer();
    }
}
int Csock::GetTimeout() const { return m_iTimeout; }
void Csock::PauseRead() { m_bPauseRead = true; }
bool Csock::IsReadPaused() const { return m_bPauseRead; }

void Csock::UnPauseRead() {
    m_bPauseRead = false;
    if (m_pipeline) m_pipeline->ResetTimeoutTimer();
    PushBuff("", 0, true);
}
int Csock::GetTimeSinceLastDataTransaction() const {
    return time(0) - m_iLastCheckTimeoutTime;
}
#ifdef HAVE_LIBSSL
void Csock::SetPemLocation(const CString& sPemFile) { m_sPemFile = sPemFile; }
void Csock::SetKeyLocation(const CString& sKeyFile) { m_sKeyFile = sKeyFile; }
void Csock::SetDHParamLocation(const CString& sDHParamFile) {
    m_sDHParamFile = sDHParamFile;
}
const CString& Csock::GetPemLocation() const { return m_sPemFile; }
const CString& Csock::GetKeyLocation() const { return m_sKeyFile; }
const CString& Csock::GetDHParamLocation() const { return m_sDHParamFile; }
X509* Csock::GetX509() const {
    if (!m_pipeline) return nullptr;
    auto& layer = m_pipeline->m_s2.m_layer;
    if (!layer) return nullptr;
    auto* handle = layer->native_handle();
    if (!handle) return nullptr;
    return SSL_get_peer_certificate(handle);
}
bool Csock::SNIConfigureClient(CString& sHostname) { return false; }
void Csock::SetCipher(const CString& sCipher) { m_sCipherType = sCipher; }
const CString& Csock::GetCipher() const { return m_sCipherType; }
#endif
void Csock::SetMaxBufferThreshold(u_int iThreshold) {
    m_iMaxStoredBufferLength = iThreshold;
}
CSMonitorFD::CSMonitorFD() {}
CSMonitorFD::~CSMonitorFD() {}
void CSMonitorFD::SetMonitor(std::unique_ptr<znc_asio::FDMonitor> pMonitor) {
    m_monitor = std::move(pMonitor);
}
