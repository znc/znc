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
 * @file Csocket.h
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

/*
 * NOTES ...
 * - You should always compile with -Woverloaded-virtual to detect callbacks
 * that may have been redefined since your last update
 * - If you want to use gethostbyname instead of getaddrinfo, the use
 * -DUSE_GETHOSTBYNAME when compiling
 * - To compile with win32 need to link to winsock2, using gcc its -lws2_32
 * - Code is formated with 'astyle --style=ansi -t4 --unpad-paren --pad-paren-in
 * --keep-one-line-blocks'
 */
#ifndef ZNC_ASIO_H
#define ZNC_ASIO_H

#include <memory>
#include <unordered_set>
#include "defines.h"  // require this as a general rule, most projects have a defines.h or the like

#ifdef HAVE_ICU
#include <unicode/ucnv.h>
#endif

#ifdef HAVE_LIBSSL
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#endif /* HAVE_LIBSSL */

#ifdef HAVE_UNIX_SOCKET
#include <sys/un.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef _WIN32
typedef SOCKET cs_sock_t;
#else
typedef int cs_sock_t;
#endif

namespace znc_asio {
// Pimpl, because asio is so slooow to compile
struct IoContext;
struct Pipeline;
struct Acceptor;
struct Timer;
struct FDMonitor;
}  // namespace znc_asio

/**
 * @class CSSockAddr
 * @brief sockaddr wrapper.
 */
class CSSockAddr {
  public:
    enum EAFRequire {
        RAF_ANY = PF_UNSPEC,
#ifdef HAVE_IPV6
        RAF_INET6 = AF_INET6,
#endif /* HAVE_IPV6 */
        RAF_INET = AF_INET
    };
};

class Csock;

/**
 * This does all the csocket initialized inclusing InitSSL() and win32 specific
 * initializations, only needs to be called once
 */
inline bool InitCsocket() { return true; }
/**
 * Shutdown and release global allocated memory
 */
inline void ShutdownCsocket() {}

/**
 * @class CCron
 * @brief this is the main cron job class
 *
 * You should derive from this class, and override RunJob() with your code
 * @author Jim Hull <csocket@jimloco.com>
 */
class CCron {
  public:
    CCron();
    virtual ~CCron();

    //! This is used by the Job Manager, and not you directly
    void run(timeval& tNow);

    /**
     * @param TimeSequence	how often to run in seconds
     * @param iMaxCycles		how many times to run, 0 makes it run forever
     */
    void StartMaxCycles(double dTimeSequence, uint32_t iMaxCycles);
    void StartMaxCycles(const timeval& tTimeSequence, uint32_t iMaxCycles);

    //! starts and runs infinity amount of times
    void Start(double dTimeSequence);
    void Start(const timeval& TimeSequence);

    //! call this to turn off your cron, it will be removed
    void Stop();

    //! pauses excution of your code in RunJob
    void Pause();

    //! removes the pause on RunJon
    void UnPause();

    //! reset the timer
    void Reset();

    timeval GetInterval() const;
    uint32_t GetMaxCycles() const;
    uint32_t GetCyclesLeft() const;

    //! returns true if cron is active
    bool isValid() const;

    const CString& GetName() const;
    void SetName(const CString& sName);

#ifndef SWIG
    /// Internal, don't call.
    void SetTimer(std::unique_ptr<znc_asio::Timer> timer);
#endif

  public:
    //! this is the method you should override
    virtual void RunJob();

  protected:
    bool
        m_bRunOnNextCall;  //!< if set to true, RunJob() gets called on next invocation of run() despite the timeout

  private:
    friend struct znc_asio::Timer;
    bool m_bActive, m_bPause;
    timeval m_tTimeSequence;
    uint32_t m_iMaxCycles, m_iCycles;
    CString m_sName;
    std::unique_ptr<znc_asio::Timer> m_timer;
};

/**
 * @class CSMonitorFD
 * @brief Class to tie sockets to for monitoring by Csocket at either the Csock
 * or TSockManager.
 * Currently only a very specific use case is supported.
 */
class CSMonitorFD {
  public:
    CSMonitorFD();
    //    CSMonitorFD() { /*m_bEnabled = true;*/
    //  }
    virtual ~CSMonitorFD();

#if 0
    /**
     * @brief called before select, typically you don't need to reimplement this
     * just add sockets via Add and let the default implementation have its way
     * @param miiReadyFds fill with fd's to monitor and the associated bit to
     * check them for (@see CSockManager::ECheckType)
     * @param iTimeoutMS the timeout to change to, setting this to -1 (the
     * default)
     * @return returning false will remove this from monitoring. The same effect
     * can be had by setting m_bEnabled to false as it is returned from this
     */
    virtual bool GatherFDsForSelect(std::map<cs_sock_t, short>& miiReadyFds,
                                    long& iTimeoutMS);
#endif
    /**
     * @brief called when there are fd's belonging to this class that have
     * triggered
     * @param miiReadyFds the map of fd's with the bits that triggered them
     * (@see CSockManager::ECheckType)
     * @return returning false will remove this from monitoring
     */
    virtual bool FDsThatTriggered(
        const std::map<cs_sock_t, short>& miiReadyFds) {
        return true;
    }
#if 0
    /**
     * @brief gets called to diff miiReadyFds with m_miiMonitorFDs, and calls
     * FDsThatTriggered when appropriate. Typically you don't need to
     * reimplement this.
     * @param miiReadyFds the map of all triggered fd's, not just the fd's from
     * this class
     * @return returning false will remove this from monitoring
     */
    virtual bool CheckFDs(const std::map<cs_sock_t, short>& miiReadyFds);
#endif
    /**
     * @brief adds a file descriptor to be monitored
     * @param iFD the file descriptor
     * @param iMonitorEvents bitset of events to monitor for (@see
     * CSockManager::ECheckType)
     */
    void Add(cs_sock_t iFD, short iMonitorEvents) {
        m_miiMonitorFDs[iFD] = iMonitorEvents;
    }
#if 0
    //! removes this fd from monitoring
    void Remove(cs_sock_t iFD) { m_miiMonitorFDs.erase(iFD); }
    //! causes this monitor to be removed
    void DisableMonitor() { m_bEnabled = false; }

    bool IsEnabled() const { return m_bEnabled; }
#endif
#ifndef SWIG
    //! Private for CSocketManager usage. Don't call.
    void SetMonitor(std::unique_ptr<znc_asio::FDMonitor> pMonitor);
#endif

  protected:
    friend struct znc_asio::FDMonitor;
    std::map<cs_sock_t, short> m_miiMonitorFDs;
    std::unique_ptr<znc_asio::FDMonitor> m_monitor;
    //    bool m_bEnabled;
};

/**
 * @class CSockCommon
 * @brief simple class to share common code to both TSockManager and Csock
 */
class CSockCommon {
  public:
    CSockCommon() {}
    virtual ~CSockCommon();

    void CleanupCrons();
    void CleanupFDMonitors();

    //! returns a const reference to the crons associated to this socket
    const std::unordered_set<CCron*>& GetCrons() const { return m_vcCrons; }
    //! This has a garbage collecter, and is used internall to call the jobs
    //! insert a newly created cron
    virtual void AddCron(CCron* pcCron);
    /**
     * @brief deletes a cron by name
     * @param sName the name of the cron
     * @param bDeleteAll delete all crons that match sName
     * @param bCaseSensitive use strcmp or strcasecmp
     */
    //! delete cron by address
    virtual void DelCronByAddr(CCron* pCron);

    void CheckFDs(const std::map<cs_sock_t, short>& miiReadyFds);
    void AssignFDs(std::map<cs_sock_t, short>& miiReadyFds,
                   struct timeval* tvtimeout);

    //! add an FD set to monitor
    void MonitorFD(CSMonitorFD* pMonitorFD); /* {
         m_vcMonitorFD.push_back(pMonitorFD);
     }*/

  protected:
    friend class CSocketManager;
    std::unordered_set<CCron*> m_vcCrons;
    std::vector<CSMonitorFD*> m_vcMonitorFD;
    znc_asio::IoContext* m_pContext;
};

/**
 * @class Csock
 * @brief Basic socket class.
 *
 * The most basic level socket class.
 * You can use this class directly for quick things
 * or use the socket manager.
 * @see TSocketManager
 * @author Jim Hull <csocket@jimloco.com>
 */
class Csock : public CSockCommon {
  public:
    //! default constructor, sets a timeout of 60 seconds
    Csock(int iTimeout = 60);
    /**
     * @brief Advanced constructor, for creating a simple connection
     * @param sHostname the hostname your are connecting to
     * @param uPort the port you are connecting to
     * @param itimeout how long to wait before ditching the connection, default
     * is 60 seconds
     */
    Csock(const CString& sHostname, uint16_t uPort, int itimeout = 60);

    //! override this for accept sockets
    virtual Csock* GetSockObj(const CString& sHostname, uint16_t iPort);

    virtual ~Csock();

    //! use this to copy a sock from one to the other, override it if you have special needs in the event of a copy
    void MoveFrom(Csock& cCopy);

    enum ETConn {
        OUTBOUND = 0,  //!< outbound connection
        LISTENER = 1,  //!< a socket accepting connections
        INBOUND = 2    //!< an inbound connection, passed from LISTENER
    };

    enum EFRead {
        READ_EOF = 0,      //!< End Of File, done reading
        READ_ERR = -1,     //!< Error on the socket, socket closed, done reading
        READ_EAGAIN = -2,  //!< Try to get data again
        READ_CONNREFUSED = -3,  //!< Connection Refused
        READ_TIMEDOUT = -4      //!< Connection timed out
    };

    enum EFSelect {
        SEL_OK = 0,        //!< Select passed ok
        SEL_TIMEOUT = -1,  //!< Select timed out
        SEL_EAGAIN = -2,   //!< Select wants you to try again
        SEL_ERR = -3       //!< Select recieved an error
    };

    enum ESSLMethod {
        TLS = 0,
        SSL23 = TLS,
        SSL2 = 2,
        SSL3 = 3,
        TLS1 = 4,
        TLS11 = 5,
        TLS12 = 6
    };

    enum EDisableProtocol {
        EDP_None = 0,      //!< disable nothing
        EDP_SSLv2 = 1,     //!< disable SSL version 2
        EDP_SSLv3 = 2,     //!< disable SSL version 3
        EDP_TLSv1 = 4,     //!< disable TLS version 1
        EDP_TLSv1_1 = 8,   //!< disable TLS version 1.1
        EDP_TLSv1_2 = 16,  //!< disable TLS version 1.2
        EDP_SSL = (EDP_SSLv2 | EDP_SSLv3)
    };

    enum ECONState {
        CST_START = 0,
        CST_DNS = CST_START,
        CST_BINDVHOST = 1,
        CST_DESTDNS = 2,
        CST_CONNECT = 3,
        CST_CONNECTSSL = 4,
        CST_OK = 5
    };

    enum ECloseType {
        CLT_DONT = 0,        //!< don't close DER
        CLT_NOW = 1,         //!< close immediatly
        CLT_AFTERWRITE = 2,  //!< close after finishing writing the buffer
        CLT_DEREFERENCE =
            3  //!< used after copy in Csock::Dereference() to cleanup a sock thats being shutdown
    };

    /**
     * @brief Create the connection, this is used by the socket manager, and
     * shouldn't be called directly by the user
     * @return true on success
     */
    void Connect();

#ifdef HAVE_UNIX_SOCKET
    /**
     * @brief Connect to a UNIX socket.
     * @param sPath the path to the UNIX socket.
     */
    virtual bool ConnectUnix(const CString& sPath);

    /**
     * @brief Listens for connections on an UNIX socket
     * @param sBindFile the socket on which to listen
     * @param iMaxConns the maximum amount of pending connections to allow
     * @param iTimeout if no connections come in by this timeout, the listener
     * is closed
     */
    virtual bool ListenUnix(const CString& sBindFile, int iMaxConns = SOMAXCONN,
                            uint32_t iTimeout = 0);
#endif

    /**
     * @brief Listens for connections
     * @param iPort the port to listen on
     * @param iMaxConns the maximum amount of pending connections to allow
     * @param sBindHost the vhost on which to listen
     * @param iTimeout if no connections come in by this timeout, the listener
     * is closed
     * @param bDetach don't block waiting for port to come up, instead detach
     * and return immediately
     */
    bool Listen(uint16_t iPort, int iMaxConns = SOMAXCONN,
                const CString& sBindHost = "", uint32_t iTimeout = 0,
                bool bDetach = false);

    //! Accept an inbound connection, this is used internally
    void Accept();

    //! Accept an inbound SSL connection, this is used internally and called after Accept
    bool AcceptSSL();

    //! This sets up the SSL Client, this is used internally
    bool SSLClientSetup();

    //! This sets up the SSL Server, this is used internally
    bool SSLServerSetup();

    /**
     * @brief Create the SSL connection
     * @return true on success
     *
     * This is used by the socket manager, and shouldn't be called directly by
     * the user.
     */
    bool ConnectSSL();

    //! start a TLS connection on an existing plain connection
    bool StartTLS();

    /**
     * @brief Write data to the socket
     *
     * If not all of the data is sent, it will be stored on
     * an internal buffer, and tried again with next call to Write
     * if the socket is blocking, it will send everything, its ok to check ernno
     * after this (nothing else is processed)
     *
     * @param data the data to send
     * @param len the length of data
     */
    virtual bool Write(const char* data, size_t len);

    /**
     * @brief Write a text string to the socket
     *
     * Encoding is used, if set
     *
     * @param sData the string to send; if encoding is provided, sData should be
     * UTF-8 and will be encoded
     * @see Write( const char *, int )
     */
    virtual bool Write(const CString& sData);

    /**
     * Read from the socket
     * Just pass in a pointer, big enough to hold len bytes
     *
     * @param data the buffer to read into
     * @param len the size of the buffer
     *
     * @return
     * Returns READ_EOF for EOF
     * Returns READ_ERR for ERROR
     * Returns READ_EAGAIN for Try Again ( EAGAIN )
     * Returns READ_CONNREFUSED for connection refused
     * Returns READ_TIMEDOUT for a connection that timed out at the TCP level
     * Otherwise returns the bytes read into data
     */
    virtual ssize_t Read(char* data, size_t len);
    CString GetLocalIP() const;
    CString GetRemoteIP() const;

    //! Tells you if the socket is connected
    virtual bool IsConnected() const;
    //! Sets the sock, telling it its connected (internal use only)
    virtual void SetIsConnected(bool b);

    //! returns a reference to the sock
    cs_sock_t& GetRSock();
    const cs_sock_t& GetRSock() const;
    void SetRSock(cs_sock_t iSock);
    cs_sock_t& GetWSock();
    const cs_sock_t& GetWSock() const;
    void SetWSock(cs_sock_t iSock);

    void SetSock(cs_sock_t iSock);
    cs_sock_t& GetSock();
    const cs_sock_t& GetSock() const;

    /**
     * @brief calls SockError, if sDescription is not set, then strerror is used
     * to pull out a default description
     * @param iErrno the errno to send
     * @param sDescription the description of the error that occurred
     */
    void CallSockError(int iErrno, const CString& sDescription = "");

    //! will pause/unpause reading on this socket
    void PauseRead();
    void UnPauseRead();
    bool IsReadPaused() const;
    /**
     * this timeout isn't just connection timeout, but also timeout on
     * NOT recieving data, to disable this set it to 0
     * then the normal TCP timeout will apply (basically TCP will kill a dead
     * connection) Set the timeout, set to 0 to never timeout
     */
    enum {
        TMO_READ = 1,
        TMO_WRITE = 2,
        TMO_ACCEPT = 4,
        TMO_ALL = TMO_READ | TMO_WRITE | TMO_ACCEPT
    };

    //! Currently this uses the same value for all timeouts, and iTimeoutType merely states which event will be checked
    //! for timeouts
    void SetTimeout(int iTimeout, uint32_t iTimeoutType = TMO_ALL);
    int GetTimeout() const;

    //! returns true if the socket has timed out
    virtual bool CheckTimeout(time_t iNow);

    /**
     * pushes data up on the buffer, if a line is ready
     * it calls the ReadLine event
     */
    virtual void PushBuff(const char* data, size_t len,
                          bool bStartAtZero = false);

    //! This gives access to the internal read buffer, if your
    //! not going to use ReadLine(), then you may want to clear this out
    //! (if its binary data and not many '\\n')
    CString& GetInternalReadBuffer();

    //! This gives access to the internal write buffer.
    //! If you want to check if the send queue fills up, check here.
    CString& GetInternalWriteBuffer();

    //! sets the max buffered threshold when EnableReadLine() is enabled
    void SetMaxBufferThreshold(uint32_t iThreshold);
    uint32_t GetMaxBufferThreshold() const;

    //! Returns the connection type from enum eConnType
    ETConn GetType() const;
    void SetType(ETConn iType);

    //! Returns a reference to the socket name
    const CString& GetSockName() const;
    void SetSockName(const CString& sName);

    //! Returns a reference to the host name
    const CString& GetHostName() const;
    void SetHostName(const CString& sHostname);

    //! Gets the starting time of this socket
    uint64_t GetStartTime() const;
    //! Resets the start time
    void ResetStartTime();

    //! Gets the amount of data read during the existence of the socket
    uint64_t GetBytesRead() const;
    void ResetBytesRead();

    //! Gets the amount of data written during the existence of the socket
    uint64_t GetBytesWritten() const;
    void ResetBytesWritten();

    //! Get Avg Read Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
    double GetAvgRead(uint64_t iSample = 1000) const;

    //! Get Avg Write Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
    double GetAvgWrite(uint64_t iSample = 1000) const;

    //! Returns the remote port
    uint16_t GetRemotePort() const;

    //! Returns the local port
    uint16_t GetLocalPort() const;

    //! Returns the port
    uint16_t GetPort() const;
    void SetPort(uint16_t iPort);

    //! just mark us as closed, the parent can pick it up
    void Close(ECloseType eCloseType = CLT_NOW);
    //! returns int of type to close @see ECloseType
    ECloseType GetCloseType() const { return m_eCloseType; }
    bool IsClosed() const { return GetCloseType() != CLT_DONT; }

    //! Use this to change your fd's to blocking or none blocking
    void NonBlockingIO();

    //! Return true if this socket is using ssl. Note this does not mean the SSL state is finished, but simply that its configured to use ssl
    bool GetSSL() const;
    void SetSSL(bool b);

#ifdef HAVE_LIBSSL
    //! bitwise setter, @see EDisableProtocol
    void DisableSSLProtocols(u_int uDisableOpts) {
        m_uDisableProtocols = uDisableOpts;
    }
    u_int DisabledSSLProtocols() const { return m_uDisableProtocols; }
    //! allow disabling compression
    void DisableSSLCompression() { m_bNoSSLCompression = true; }
    //! select the ciphers in server-preferred order
    void FollowSSLCipherServerPreference() {
        m_bSSLCipherServerPreference = true;
    }
    //! Set the cipher type ( openssl cipher [to see ciphers available] )
    void SetCipher(const CString& sCipher);
    const CString& GetCipher() const;

    //! Set the pem file location
    void SetDHParamLocation(const CString& sDHParamFile);
    const CString& GetDHParamLocation() const;
    void SetKeyLocation(const CString& sKeyFile);
    const CString& GetKeyLocation() const;
    void SetPemLocation(const CString& sPemFile);
    const CString& GetPemLocation() const;
    void SetPemPass(const CString& sPassword);
    const CString& GetPemPass() const;

    //! Set the SSL method type
    void SetSSLMethod(int iMethod);
    int GetSSLMethod() const;

    void SetSSLObject(SSL* ssl, bool bDeleteExisting = false);
    SSL* GetSSLObject() const;
    void SetCTXObject(SSL_CTX* sslCtx, bool bDeleteExisting = false);
    SSL_SESSION* GetSSLSession() const;

#endif /* HAVE_LIBSSL */

    //! Get the send buffer
    bool HasWriteBuffer() const;
    void ClearWriteBuffer();

    //! is SSL_accept finished ?
    //! is the ssl properly finished (from write no error)
    bool SslIsEstablished() const;

    //! Use this to bind this socket to inetd
    bool ConnectInetd(bool bIsSSL = false, const CString& sHostname = "");

    //! Tie this guy to an existing real file descriptor
    bool ConnectFD(int iReadFD, int iWriteFD, const CString& sName,
                   bool bIsSSL = false, ETConn eDirection = INBOUND);

    //! Get the peer's X509 cert
#ifdef HAVE_LIBSSL
    //! it is up to you, the caller to call X509_free() on this object
    X509* GetX509() const;

    //! Returns the peer's public key
    CString GetPeerPubKey() const;
    //! Returns the peer's certificate finger print
    long GetPeerFingerprint(CString& sFP) const;

    uint32_t GetRequireClientCertFlags() const;
    //! legacy, deprecated @see SetRequireClientCertFlags
    void SetRequiresClientCert(bool bRequiresCert);
    //! bitwise flags, 0 means don't require cert, SSL_VERIFY_PEER verifies peers, SSL_VERIFY_FAIL_IF_NO_PEER_CERT will cause the connection to fail if no cert
    void SetRequireClientCertFlags(uint32_t iRequireClientCertFlags) {
        m_iRequireClientCertFlags = iRequireClientCertFlags;
    }
#endif /* HAVE_LIBSSL */

    /**
     * sets the rate at which we can send data
     * @param iBytes the amount of bytes we can write
     * @param iMilliseconds the amount of time we have to rate to iBytes
     */
    virtual void SetRate(uint32_t iBytes, uint64_t iMilliseconds);

    uint32_t GetRateBytes() const;
    uint64_t GetRateTime() const;

    /**
     * Connected event
     */
    virtual void Connected() {}
    /**
     * Disconnected event
     */
    virtual void Disconnected() {}
    /**
     * Sock Timed out event
     */
    virtual void Timeout() {}
    /**
     * Ready to read data event
     */
    virtual void ReadData(const char* data, size_t len) {}
    /**
     *
     * Ready to Read a full line event. If encoding is provided, this is
     * guaranteed to be UTF-8
     */
    virtual void ReadLine(const CString& sLine) {}
    //! set the value of m_bEnableReadLine to true, we don't want to store a buffer for ReadLine, unless we want it
    void EnableReadLine();
    void DisableReadLine();
    //! returns the value of m_bEnableReadLine, if ReadLine is enabled
    bool HasReadLine() const { return m_bEnableReadLine; }

    /**
     * This WARNING event is called when your buffer for readline exceeds the
     * warning threshold and triggers this event. Either Override it and do
     * nothing, or SetMaxBufferThreshold() This event will only get called if
     * m_bEnableReadLine is enabled
     */
    virtual void ReachedMaxBuffer();
    /**
     * @brief A sock error occured event
     */
    virtual void SockError(int iErrno, const CString& sDescription) {}
    /**
     * Incoming Connection Event
     * return false and the connection will fail
     * default returns true
     */
    virtual bool ConnectionFrom(const CString& sHost, uint16_t iPort) {
        return true;
    }

    /**
     * @brief called when type is LISTENER and the listening port is up and
     * running
     * @param sBindIP the IP that is being bound to. Empty if no bind
     * restriction
     * @param uPort the listening port
     */
    virtual void Listening(const CString& sBindIP, uint16_t uPort) {}

    /**
     * Connection Refused Event
     *
     */
    virtual void ConnectionRefused() {}
    /**
     * This gets called every iteration of CSocketManager::Select() if the
     * socket is ReadPaused
     */
    virtual void ReadPaused() {}

#ifdef HAVE_LIBSSL
    /**
     * Gets called immediatly after the m_ssl member is setup and initialized,
     * useful if you need to assign anything to this ssl session via
     * SSL_set_ex_data
     */
    virtual void SSLFinishSetup(SSL* pSSL) {}
    /**
     * @brief gets called when a SNI request is sent, and used to configure a
     * SNI session
     * @param sHostname the hostname sent from the client
     * @param sPemFile fill this with the location to the pemfile
     * @param sPemPass fill this with the pemfile password if there is one
     * @return return true to proceed with the SNI server configuration
     */
    virtual bool SNIConfigureServer(const CString& sHostname, CString& sPemFile,
                                    CString& sPemPass) {
        return false;
    }
    /**
     * @brief called to configure the SNI client
     * @param sHostname, the hostname to configure SNI with, you can fill this
     * with GetHostname() if its a valid hostname and not an OP
     * @return returning true causes a call to configure SNI with the hostname
     * returned
     */
    virtual bool SNIConfigureClient(CString& sHostname);
    //! creates a new SSL_CTX based on the setup of this sock
    SSL_CTX* SetupServerCTX();

    /**
     * @brief called once the SSL handshake is complete, this is triggered via
     * SSL_CB_HANDSHAKE_DONE in SSL_set_info_callback()
     *
     * This is a spot where you can look at the finished peer certifificate ...
     * IE <pre> X509 * pCert = GetX509(); char szName[256]; memset( szName,
     * '\0', 256 ); X509_NAME_get_text_by_NID ( X509_get_subject_name( pCert ),
     * NID_commonName, szName, 255 ); cerr << "Name! " << szName << endl;
     * X509_free( pCert );
     * </pre>
     */
    virtual void SSLHandShakeFinished() {}
    /**
     * @brief this is hooked in via SSL_set_verify, and be default it just
     * returns 1 meaning success
     * @param iPreVerify the pre-verification status as determined by openssl
     * internally
     * @param pStoreCTX the X509_STORE_CTX containing the certificate
     * @return 1 to continue, 0 to abort
     *
     * This may get called multiple times, for example with a chain certificate
     * which is fairly typical with certificates from godaddy, freessl, etc.
     * Additionally, openssl does not do any host verification, they leave that
     * up to the you. One easy way to deal with this is to wait for
     * SSLHandShakeFinished() and examine the peer certificate @see
     * SSLHandShakeFinished
     */
    virtual int VerifyPeerCertificate(int iPreVerify,
                                      X509_STORE_CTX* pStoreCTX) {
        return 1;
    }
#endif /* HAVE_LIBSSL */

    //! return how long it has been (in seconds) since the last read or successful write
    // TODO: return steady timer duration instead
    int GetTimeSinceLastDataTransaction() const;

    time_t GetLastCheckTimeout() const { return m_iLastCheckTimeoutTime; }
    void SetLastCheckTimeout(time_t t) { m_iLastCheckTimeoutTime = t; }

    //! Returns the time when CheckTimeout() should be called next
    time_t GetNextCheckTimeout(time_t iNow = 0) const;

    //! return the data imediatly ready for read
    virtual int GetPending() const;

    //////////////////////////
    // Connection State Stuff
    //! returns the current connection state
    ECONState GetConState() const { return m_eConState; }
    //! sets the connection state to eState
    void SetConState(ECONState eState) { m_eConState = eState; }

    //! grabs fd's for the sockets
    bool CreateSocksFD();

    //! puts the socks back to the state they were prior to calling CreateSocksFD
    void CloseSocksFD();

    const CString& GetBindHost() const { return m_sBindHost; }
    void SetBindHost(const CString& sBindHost) { m_sBindHost = sBindHost; }

    //! this is only used on outbound connections, listeners bind in a different spot
    bool SetupVHost();

    bool GetIPv6() const { return m_bIsIPv6; }
    void SetIPv6(bool b) { m_bIsIPv6 = b; }

    CSSockAddr::EAFRequire GetAFRequire() const { return m_iAFRequire; }
    void SetAFRequire(CSSockAddr::EAFRequire iAFRequire) {
        m_iAFRequire = iAFRequire;
    }

    //! returns true if this socket can write its data, primarily used with rate shaping, initialize iNOW to 0 and it sets it on the first call
    bool AllowWrite(uint64_t& iNOW) const;

    void SetSkipConnect(bool b) { m_bSkipConnect = b; }

    //! returns the number of max pending connections when type is LISTENER
    int GetMaxConns() const { return m_iMaxConns; }

#ifdef HAVE_ICU
    void SetEncoding(const CString& sEncoding);
    CString GetEncoding() const { return m_sEncoding; }
    virtual void IcuExtToUCallback(UConverterToUnicodeArgs* toArgs,
                                   const char* codeUnits, int32_t length,
                                   UConverterCallbackReason reason,
                                   UErrorCode* err);
    virtual void IcuExtFromUCallback(UConverterFromUnicodeArgs* fromArgs,
                                     const UChar* codeUnits, int32_t length,
                                     UChar32 codePoint,
                                     UConverterCallbackReason reason,
                                     UErrorCode* err);
#endif /* HAVE_ICU */

#ifndef SWIG
    //! Private for CSocketManager usage. Don't call.
    void SetPipeline(std::shared_ptr<znc_asio::Pipeline>);
    void SetAcceptor(std::unique_ptr<znc_asio::Acceptor>);
#endif

  private:
    //! making private for safety
    Csock(const Csock& cCopy) = delete;
    //! shrink sendbuff by removing m_uSendBufferPos bytes from m_sSend
    void ShrinkSendBuff();
    void IncBuffPos(size_t uBytes);
    //! checks for configured protocol disabling

    // NOTE! if you add any new members, be sure to add them to Copy()
    uint16_t m_uPort;
    cs_sock_t m_iReadSock, m_iWriteSock;
    int m_iTimeout, m_iMethod, m_iTcount, m_iMaxConns;
    ETConn m_iConnType;
    bool m_bUseSSL, m_bIsConnected;
    bool m_bsslEstablished, m_bEnableReadLine, m_bPauseRead;
    CString m_shostname, m_sbuffer, m_sSockName, m_sDHParamFile, m_sKeyFile,
        m_sPemFile, m_sCipherType;
    CString m_sSend, m_sPemPass;
    ECloseType m_eCloseType;

    // initialized lazily
    mutable uint16_t m_iRemotePort, m_iLocalPort;
    mutable CString m_sLocalIP, m_sRemoteIP;

    uint64_t m_iMaxMilliSeconds, m_iLastSendTime, m_iBytesRead, m_iBytesWritten,
        m_iStartTime;
    uint32_t m_iMaxBytes, m_iMaxStoredBufferLength;
    size_t m_iLastSend, m_uSendBufferPos;

    CSSockAddr m_address, m_bindhost;
    bool m_bIsIPv6, m_bSkipConnect;
    time_t m_iLastCheckTimeoutTime;

#ifdef HAVE_LIBSSL
    CString m_sSSLBuffer;
    SSL* m_ssl;
    SSL_CTX* m_ssl_ctx;
    uint32_t m_iRequireClientCertFlags;
    u_int m_uDisableProtocols;
    bool m_bNoSSLCompression;
    bool m_bSSLCipherServerPreference;

#endif /* HAVE_LIBSSL */

    //! Create the socket
    cs_sock_t CreateSocket(bool bListen = false, bool bUnix = false);
    void Init(const CString& sHostname, uint16_t uPort, int iTimeout = 60);

    // Connection State Info
    ECONState m_eConState;
    CString m_sBindHost;

#ifdef HAVE_ICU
    UConverter* m_cnvInt;
    UConverter* m_cnvExt;
    bool m_cnvTryUTF8;
    bool m_cnvSendUTF8;
    CString m_sEncoding;
#endif

    std::shared_ptr<znc_asio::Pipeline> m_pipeline;
    std::unique_ptr<znc_asio::Acceptor> m_acceptor;
    CSSockAddr::EAFRequire m_iAFRequire;
};

/**
 * @class CSConnection
 * @brief options for creating a connection
 */
class CSConnection {
  public:
    /**
     * @param sHostname hostname to connect to
     * @param iPort port to connect to
     * @param iTimeout connection timeout
     */
    CSConnection(const CString& sHostname, uint16_t iPort, int iTimeout = 60) {
        m_sHostname = sHostname;
        m_iPort = iPort;
        m_iTimeout = iTimeout;
        m_bIsSSL = false;
#ifdef HAVE_LIBSSL
        m_sCipher = "HIGH";
#endif /* HAVE_LIBSSL */
        m_iAFrequire = CSSockAddr::RAF_ANY;
    }
    virtual ~CSConnection() {}

    const CString& GetHostname() const { return m_sHostname; }
    const CString& GetSockName() const { return m_sSockName; }
    const CString& GetBindHost() const { return m_sBindHost; }
    uint16_t GetPort() const { return m_iPort; }
    int GetTimeout() const { return m_iTimeout; }
    bool GetIsSSL() const { return m_bIsSSL; }
    CSSockAddr::EAFRequire GetAFRequire() const { return m_iAFrequire; }

#ifdef HAVE_LIBSSL
    const CString& GetCipher() const { return m_sCipher; }
    const CString& GetPemLocation() const { return m_sPemLocation; }
    const CString& GetKeyLocation() const { return m_sKeyLocation; }
    const CString& GetDHParamLocation() const { return m_sDHParamLocation; }
    const CString& GetPemPass() const { return m_sPemPass; }
#endif /* HAVE_LIBSSL */

    //! sets the hostname to connect to
    void SetHostname(const CString& s) { m_sHostname = s; }
    //! sets the name of the socket, used for reference, ie in FindSockByName()
    void SetSockName(const CString& s) { m_sSockName = s; }
    //! sets the hostname to bind to (vhost support)
    void SetBindHost(const CString& s) { m_sBindHost = s; }
    //! sets the port to connect to
    void SetPort(uint16_t i) { m_iPort = i; }
    //! sets the connection timeout
    void SetTimeout(int i) { m_iTimeout = i; }
    //! set to true to enable SSL
    void SetIsSSL(bool b) { m_bIsSSL = b; }
    //! sets the AF family type required
    void SetAFRequire(CSSockAddr::EAFRequire iAFRequire) {
        m_iAFrequire = iAFRequire;
    }

#ifdef HAVE_LIBSSL
    //! set the cipher strength to use, default is HIGH
    void SetCipher(const CString& s) { m_sCipher = s; }
    //! set the location of the pemfile
    void SetPemLocation(const CString& s) { m_sPemLocation = s; }
    //! set the pemfile pass
    void SetPemPass(const CString& s) { m_sPemPass = s; }
#endif /* HAVE_LIBSSL */

  protected:
    CString m_sHostname, m_sSockName, m_sBindHost;
    uint16_t m_iPort;
    int m_iTimeout;
    bool m_bIsSSL;
    CSSockAddr::EAFRequire m_iAFrequire;
#ifdef HAVE_LIBSSL
    CString m_sDHParamLocation, m_sKeyLocation, m_sPemLocation, m_sPemPass,
        m_sCipher;
#endif /* HAVE_LIBSSL */
};

class CSSSLConnection : public CSConnection {
  public:
    CSSSLConnection(const CString& sHostname, uint16_t iPort, int iTimeout = 60)
        : CSConnection(sHostname, iPort, iTimeout) {
        SetIsSSL(true);
    }
};

/**
 * @class CSListener
 * @brief options container to create a listener
 */
class CSListener {
  public:
    /**
     * @param iPort port to listen on. Set to 0 to listen on a random port
     * @param sBindHost host to bind to
     * @param bDetach don't block while waiting for the port to come up, instead
     * detach and return immediately
     */
    CSListener(uint16_t iPort, const CString& sBindHost = "",
               bool bDetach = false) {
        m_iPort = iPort;
        m_sBindHost = sBindHost;
        m_bIsSSL = false;
        m_iMaxConns = SOMAXCONN;
        m_iTimeout = 0;
        m_iAFrequire = CSSockAddr::RAF_ANY;
        m_bDetach = bDetach;
#ifdef HAVE_LIBSSL
        m_sCipher = "HIGH";
        m_iRequireCertFlags = 0;
#endif /* HAVE_LIBSSL */
    }
    virtual ~CSListener() {}

    void SetDetach(bool b) { m_bDetach = b; }
    bool GetDetach() const { return m_bDetach; }
    uint16_t GetPort() const { return m_iPort; }
    const CString& GetSockName() const { return m_sSockName; }
    const CString& GetBindHost() const { return m_sBindHost; }
    bool GetIsSSL() const { return m_bIsSSL; }
    int GetMaxConns() const { return m_iMaxConns; }
    uint32_t GetTimeout() const { return m_iTimeout; }
    CSSockAddr::EAFRequire GetAFRequire() const { return m_iAFrequire; }
#ifdef HAVE_LIBSSL
    const CString& GetCipher() const { return m_sCipher; }
    const CString& GetDHParamLocation() const { return m_sDHParamLocation; }
    const CString& GetKeyLocation() const { return m_sKeyLocation; }
    const CString& GetPemLocation() const { return m_sPemLocation; }
    const CString& GetPemPass() const { return m_sPemPass; }
    uint32_t GetRequireClientCertFlags() const { return m_iRequireCertFlags; }
#endif /* HAVE_LIBSSL */

    //! sets the port to listen on. Set to 0 to listen on a random port
    void SetPort(uint16_t iPort) { m_iPort = iPort; }
    //! sets the sock name for later reference (ie FindSockByName)
    void SetSockName(const CString& sSockName) { m_sSockName = sSockName; }
    //! sets the host to bind to
    void SetBindHost(const CString& sBindHost) { m_sBindHost = sBindHost; }
    //! set to true to enable SSL
    void SetIsSSL(bool b) { m_bIsSSL = b; }
    //! set max connections as called by accept()
    void SetMaxConns(int i) { m_iMaxConns = i; }
    //! sets the listen timeout. The listener class will close after timeout has been reached if not 0
    void SetTimeout(uint32_t i) { m_iTimeout = i; }
    //! sets the AF family type required
    void SetAFRequire(CSSockAddr::EAFRequire iAFRequire) {
        m_iAFrequire = iAFRequire;
    }

#ifdef HAVE_LIBSSL
    //! set the cipher strength to use, default is HIGH
    void SetCipher(const CString& s) { m_sCipher = s; }
    //! set the location of the pemfile
    void SetPemLocation(const CString& s) { m_sPemLocation = s; }
    //! set the location of the keyfile
    void SetKeyLocation(const CString& s) { m_sKeyLocation = s; }
    //! set the location of the dhparamfile
    void SetDHParamLocation(const CString& s) { m_sDHParamLocation = s; }
    //! set the pemfile pass
    void SetPemPass(const CString& s) { m_sPemPass = s; }
    //! set to true if require a client certificate (deprecated @see SetRequireClientCertFlags)
    void SetRequiresClientCert(bool b) {
        m_iRequireCertFlags =
            (b ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0);
    }
    //! bitwise flags, 0 means don't require cert, SSL_VERIFY_PEER verifies peers, SSL_VERIFY_FAIL_IF_NO_PEER_CERT will cause the connection to fail if no cert
    void SetRequireClientCertFlags(unsigned int iRequireCertFlags) {
        m_iRequireCertFlags = iRequireCertFlags;
    }
#endif /* HAVE_LIBSSL */
  private:
    uint16_t m_iPort;
    CString m_sSockName, m_sBindHost;
    bool m_bIsSSL;
    bool m_bDetach;
    int m_iMaxConns;
    uint32_t m_iTimeout;
    CSSockAddr::EAFRequire m_iAFrequire;

#ifdef HAVE_LIBSSL
    CString m_sDHParamLocation, m_sKeyLocation, m_sPemLocation, m_sPemPass,
        m_sCipher;
    uint32_t m_iRequireCertFlags;
#endif /* HAVE_LIBSSL */
};

#ifdef HAVE_LIBSSL
class CSSSListener : public CSListener {
  public:
    CSSSListener(uint16_t iPort, const CString& sBindHost = "")
        : CSListener(iPort, sBindHost) {
        SetIsSSL(true);
    }
};
#endif /* HAVE_LIBSSL */

/**
 * @class CSocketManager
 * @brief Best class to use to interact with the sockets
 *
 * Handles SSL and NON Blocking IO.
 * Rather then use it directly, you'll probably get more use deriving from it.
 * Override GetSockObj since Csock derivatives need to be new'd correctly.
 * Makes it easier to use overall.
 * Another thing to note, is that all sockets are deleted implicitly, so
 * obviously you can't pass in Csock classes created on the stack. For those of
 * you who don't know STL very well, the reason I did this is because whenever
 * you add to certain stl containers (e.g. vector, or map), it is completely
 * rebuilt using the copy constructor on each element. That then means the
 * constructor and destructor are called on every item in the container. Not
 * only is this more overhead then just moving pointers around, its dangerous as
 * if you have an object that is newed and deleted in the destructor the value
 * of its pointer is copied in the default copy constructor. This means everyone
 * has to know better and create a copy constructor, or I just make everyone new
 * their object :).
 *
 * @see TSocketManager
 * @author Jim Hull <csocket@jimloco.com>
 */
class CSocketManager : public CSockCommon {
  public:
    CSocketManager();
    virtual ~CSocketManager();
    virtual void Cleanup();

    /**
     * @brief Create a connection
     * @param cCon the connection which should be established
     * @param pcSock the socket used for the connection
     */
    void Connect(const CSConnection& cCon, Csock* pcSock);

    /**
     * @brief Sets up a listening socket
     * @param cListen the listener configuration
     * @param pcSock preconfigured sock to use
     * @param piRandPort if listener is set to port 0, then a random port is
     * used and this is assigned.
     *
     * IF you provide piRandPort to be assigned, AND you set bDetach to true,
     * then Listen() still blocks. If you don't want this behavior, then look
     * for the port assignment to be called in Csock::Listening
     */
    bool Listen(const CSListener& cListen, Csock* pcSock,
                uint16_t* piRandPort = NULL);

    //! simple method to see if there are file descriptors being processed, useful to know if all the work is done in the manager
    bool HasFDs() const;

    /**
     * Best place to call this class for running, all the call backs are called.
     * You should through this in your main while loop (long as its not
     * blocking) all the events are called as needed.
     */
    void Loop();

    /**
     * @brief this is similar to loop, except that it dynamically adjusts the
     *select time based on jobs and timeouts in sockets
     *
     *	- This type of behavior only works well in a scenario where there is low
     *traffic. If you use this then its because you
     *	- are trying to spare yourself some of those idle loops where nothing is
     *done. If you try to use this code where you have lots of
     *	- connections and/or lots of traffic you might end up causing more CPU
     *usage than just a plain Loop() with a static sleep of 500ms
     *	- its a trade off at some point, and you'll probably find out that the
     *vast majority of the time and in most cases Loop() works fine
     *	- by itself. I've tried to mitigate that as much as possible by not
     *having it change the select if the previous call to select
     *	- was not a timeout. Anyways .... Caveat Emptor.
     *	- Sample useage is cFoo.DynamicSelectLoop( 500000, 5000000 ); which
     *basically says min of 500ms and max of 5s
     *
     * @param iLowerBounds the lower bounds to use in MICROSECONDS
     * @param iUpperBounds the upper bounds to use in MICROSECONDS
     * @param iMaxResolution the maximum time to calculate overall in seconds
     */
    void DynamicSelectLoop(uint64_t iLowerBounds, uint64_t iUpperBounds,
                           time_t iMaxResolution = 3600);

    /**
     * Make this method virtual, so you can override it when a socket is added.
     * Assuming you might want to do some extra stuff
     */
    void AddSock(Csock* pcSock, const CString& sSockName);

    std::vector<Csock*> FindSocksByName(const CString& sName);

    //! Delete a sock by addr
    //! its position is looked up
    //! the socket is deleted, the appropriate call backs are peformed
    //! and its instance is removed from the manager
    virtual void DelSockByAddr(Csock* pcSock);

    /**
     * @brief swaps out a sock with a copy of the original sock
     * @param pNewSock the new sock to change out with. (this should be
     * constructed by you with the default ctor)
     * @param pOrigSock the address of the original socket
     * @return true on success
     */
    bool SwapSockByAddr(Csock* pNewSock, Csock* pOrigSock);

    /**
     * @brief in the event you pass this class to Copy(), you MUST call this
     * function or on the original Csock other wise bad side effects will happen
     * (double deletes, weird sock closures, etc) if you call this function and
     * have not handled the internal pointers, other bad things can happend
     * (memory leaks, fd leaks, etc) the whole point of this function is to
     * allow this class to go away without shutting down
     */
    void Dereference(Csock* pSock);

    //! Get the bytes read from all sockets current and past
    uint64_t GetBytesRead() const;

    //! Get the bytes written to all sockets current and past
    uint64_t GetBytesWritten() const;

    //! this is a strict wrapper around C-api select(). Added in the event you need to do special work here
    enum ECheckType { ECT_Read = 1, ECT_Write = 2 };

    void FDSetCheck(cs_sock_t iFd, std::map<cs_sock_t, short>& miiReadyFds,
                    ECheckType eType);
    bool FDHasCheck(cs_sock_t iFd, std::map<cs_sock_t, short>& miiReadyFds,
                    ECheckType eType);

    /// Internal, don't call.
    znc_asio::IoContext* GetContext() { return m_iocontext.get(); }

    /// Support for accessing sockets via foreach loop.
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Csock;
        using pointer = Csock*;
        using reference = Csock&;
        using difference_type = void;

        iterator() = default;
        iterator(std::unordered_set<Csock*>::iterator it) : m_it(it) {}

        Csock* operator*() const { return *m_it; }
        void operator++() { ++m_it; }
        bool operator!=(const iterator& other) { return m_it != other.m_it; }

      private:
        std::unordered_set<Csock*>::iterator m_it;
    };
    struct const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Csock;
        using pointer = const Csock*;
        using reference = const Csock&;
        using difference_type = void;

        const_iterator() = default;
        const_iterator(std::unordered_set<Csock*>::const_iterator it)
            : m_it(it) {}

        const Csock* operator*() const { return *m_it; }
        void operator++() { ++m_it; }
        bool operator!=(const const_iterator& other) {
            return m_it != other.m_it;
        }

      private:
        std::unordered_set<Csock*>::const_iterator m_it;
    };
    iterator begin() { return iterator(m_sockets.begin()); }
    iterator end() { return iterator(m_sockets.end()); }
    const_iterator begin() const { return const_iterator(m_sockets.begin()); }
    const_iterator end() const { return const_iterator(m_sockets.end()); }
    bool empty() const { return m_sockets.empty(); }

  private:
    ////////
    // Connection State Functions

    ///////////
    // members
    uint64_t m_iBytesRead;
    uint64_t m_iBytesWritten;

    std::unique_ptr<znc_asio::IoContext> m_iocontext;
    std::unordered_set<Csock*> m_sockets;
};

/**
 * @class TSocketManager
 * @brief Ease of use templated socket manager
 *
 * class CBlahSock : public TSocketManager<SomeSock>
 */
template <class T>
class TSocketManager : public CSocketManager {
  public:
    TSocketManager() : CSocketManager() {}
    virtual ~TSocketManager() {}
};

#endif /* ZNC_ASIO_H */
