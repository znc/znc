/**
 * @file Csocket.h
 * @author Jim Hull <csocket@jimloco.com>
 *
 *    Copyright (c) 1999-2012 Jim Hull <csocket@jimloco.com>
 *    All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * Redistributions in any form must be accompanied by information on how to obtain
 * complete source code for this software and any accompanying software that uses this software.
 * The source code must either be included in the distribution or be available for no more than
 * the cost of distribution plus a nominal fee, and must be freely redistributable
 * under reasonable conditions. For an executable file, complete source code means the source
 * code for all modules it contains. It does not include source code for modules or files
 * that typically accompany the major components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
 * OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OF THIS SOFTWARE BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NOTES ...
 * - You should always compile with -Woverloaded-virtual to detect callbacks that may have been redefined since your last update
 * - If you want to use gethostbyname instead of getaddrinfo, the use -DUSE_GETHOSTBYNAME when compiling
 * - To compile with win32 need to link to winsock2, using gcc its -lws2_32
 * - Code is formated with 'astyle --style=ansi -t4 --unpad-paren --pad-paren-in --keep-one-line-blocks'
 */
#ifndef _HAS_CSOCKET_
#define _HAS_CSOCKET_

#include <znc/defines.h> // require this as a general rule, most projects have a defines.h or the like

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/file.h>

#ifndef _WIN32

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>

#else

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/timeb.h>
#define ECONNREFUSED WSAECONNREFUSED
#define EINPROGRESS WSAEINPROGRESS
#define ETIMEDOUT WSAETIMEDOUT
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define ECONNABORTED WSAECONNABORTED
#define ENETUNREACH WSAENETUNREACH

#endif /* _WIN32 */

#ifdef HAVE_C_ARES
#include <ares.h>
#endif /* HAVE_C_ARES */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif /* HAVE_LIBSSL */

#ifdef __sun
#include <strings.h>
#include <fcntl.h>
#endif /* __sun */

#include <vector>
#include <list>
#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <map>

#ifndef CS_STRING
#	ifdef _HAS_CSTRING_
#		define CS_STRING Cstring
#	else
#		define CS_STRING std::string
#	endif	/* _HAS_CSTRING_ */
#endif /* CS_STRING */

#ifndef CS_DEBUG
#ifdef __DEBUG__
#	define CS_DEBUG( f ) std::cerr << __FILE__ << ":" << __LINE__ << " " << f << std::endl
#else
#	define CS_DEBUG(f)	(void)0
#endif /* __DEBUG__ */
#endif /* CS_DEBUG */

#ifndef PERROR
#ifdef __DEBUG__
#	define PERROR( f ) __Perror( f, __FILE__, __LINE__ )
#else
#	define PERROR( f )	(void)0
#endif /* __DEBUG__ */
#endif /* PERROR */

#ifdef _WIN32
typedef SOCKET cs_sock_t;
#ifdef _WIN64
typedef signed __int64 cs_ssize_t;
#else
typedef signed int cs_ssize_t;
#endif  /* _WIN64 */
#define CS_INVALID_SOCK	INVALID_SOCKET
#else
typedef int cs_sock_t;
typedef ssize_t cs_ssize_t;
#define CS_INVALID_SOCK	-1
#endif /* _WIN32 */

#ifdef CSOCK_USE_POLL
#include <poll.h>
#endif /* CSOCK_USE_POLL */

#ifndef _NO_CSOCKET_NS // some people may not want to use a namespace
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */


/**
 * @class CSCharBuffer
 * @brief Ease of use self deleting char * class.
 */
class CSCharBuffer
{
public:
	CSCharBuffer( size_t iSize )
	{
		m_pBuffer = ( char * )malloc( iSize );
	}
	~CSCharBuffer()
	{
		free( m_pBuffer );
	}
	char * operator()() { return( m_pBuffer ); }

private:
	char *	m_pBuffer;
};


/**
 * @class CSSockAddr
 * @brief sockaddr wrapper.
 */
class CSSockAddr
{
public:
	CSSockAddr()
	{
		m_bIsIPv6 = false;
		memset( (struct sockaddr_in *) &m_saddr, '\0', sizeof( m_saddr ) );
#ifdef HAVE_IPV6
		memset( (struct sockaddr_in6 *) &m_saddr6, '\0', sizeof( m_saddr6 ) );
#endif /* HAVE_IPV6 */
		m_iAFRequire = RAF_ANY;
	}
	virtual ~CSSockAddr() {}


	enum EAFRequire
	{
		RAF_ANY		= PF_UNSPEC,
#ifdef HAVE_IPV6
		RAF_INET6	= AF_INET6,
#endif /* HAVE_IPV6 */
		RAF_INET	= AF_INET
	};

	void SinFamily();
	void SinPort( uint16_t iPort );
	void SetIPv6( bool b );
	bool GetIPv6() const { return( m_bIsIPv6 ); }

	socklen_t GetSockAddrLen() { return( sizeof( m_saddr ) ); }
	sockaddr_in * GetSockAddr() { return( &m_saddr ); }
	in_addr * GetAddr() { return( &( m_saddr.sin_addr ) ); }
#ifdef HAVE_IPV6
	socklen_t GetSockAddrLen6() { return( sizeof( m_saddr6 ) ); }
	sockaddr_in6 * GetSockAddr6() { return( &m_saddr6 ); }
	in6_addr * GetAddr6() { return( &( m_saddr6.sin6_addr ) ); }
#endif /* HAVE_IPV6 */

	void SetAFRequire( EAFRequire iWhich ) { m_iAFRequire = iWhich; }
	EAFRequire GetAFRequire() const { return( m_iAFRequire ); }

private:
	bool			m_bIsIPv6;
	sockaddr_in		m_saddr;
#ifdef HAVE_IPV6
	sockaddr_in6	m_saddr6;
#endif /* HAVE_IPV6 */
	EAFRequire		m_iAFRequire;
};


class Csock;


/**
 * @class CGetAddrInfo
 * @brief this function is a wrapper around getaddrinfo (for ipv6)
 *
 * in the event this code is using ipv6, it calls getaddrinfo, and it tries to start the connection on each iteration
 * in the linked list returned by getaddrinfo. if pSock is not NULL the following behavior happens.
 * - if pSock is a listener, or if the connect state is in a bind vhost state (to be used with bind) AI_PASSIVE is sent to getaddrinfo
 * - if pSock is an outbound connection, AI_ADDRCONFIG and the connection is started from within this function.
 * getaddrinfo might return multiple (possibly invalid depending on system configuration) ip addresses, so connect needs to try them all.
 * A classic example of this is a hostname that resolves to both ipv4 and ipv6 ip's. You still need to call Connect (and ConnectSSL) to finish
 * up the connection state
 *
 * Process can be called in a thread, but Init and Finish must only be called from the parent once the thread is complete
 */
class CGetAddrInfo
{
public:
	/**
	 * @brief ctor
	 * @param sHostname the host to resolve
	 * @param pSock the sock being setup, this option can be NULL, if it is null csSockAddr is only setup
	 * @param csSockAddr the struct that sockaddr data is being copied to
	 */
	CGetAddrInfo( const CS_STRING & sHostname, Csock * pSock, CSSockAddr & csSockAddr );
	~CGetAddrInfo();

	//! simply sets up m_cHints for use in process
	void Init();
	//! the simplest part of the function, only calls getaddrinfo and uses only m_sHostname, m_pAddrRes, and m_cHints.
	int Process();
	//! finalizes and sets up csSockAddr (and pSock if not NULL), only needs to be called if Process returns 0, but can be called anyways if flow demands it
	int Finish();

private:
	CS_STRING m_sHostname;
	Csock * m_pSock;
	CSSockAddr & m_csSockAddr;
	struct addrinfo * m_pAddrRes;
	struct addrinfo m_cHints;
	int m_iRet;
};

//! backwards compatible wrapper around CGetAddrInfo and gethostbyname
int GetAddrInfo( const CS_STRING & sHostname, Csock * pSock, CSSockAddr & csSockAddr );

//! used to retrieve the context position of the socket to its associated ssl connection. Setup once in InitSSL() via SSL_get_ex_new_index
int GetCsockClassIdx();

#ifdef HAVE_LIBSSL
//! returns the sock object associated to the particular context. returns NULL on failure or if not available
Csock * GetCsockFromCTX( X509_STORE_CTX * pCTX );
#endif /* HAVE_LIBSSL */


const uint32_t CS_BLOCKSIZE = 4096;
template <class T> inline void CS_Delete( T * & p ) { if( p ) { delete p; p = NULL; } }

#ifdef HAVE_LIBSSL
enum ECompType
{
	CT_NONE	= 0,
	CT_ZLIB	= 1,
	CT_RLE	= 2
};

//! adjusts tv with a new timeout if iTimeoutMS is smaller
void CSAdjustTVTimeout( struct timeval & tv, long iTimeoutMS );

void SSLErrors( const char *filename, uint32_t iLineNum );

/**
 * @brief You HAVE to call this in order to use the SSL library, calling InitCsocket() also calls this
 * so unless you need to call InitSSL for a specific reason call InitCsocket()
 * @return true on success
 */
bool InitSSL( ECompType eCompressionType = CT_NONE );

#endif /* HAVE_LIBSSL */

/**
 * This does all the csocket initialized inclusing InitSSL() and win32 specific initializations, only needs to be called once
 */
bool InitCsocket();
/**
 * Shutdown and release global allocated memory
 */
void ShutdownCsocket();

//! @todo need to make this sock specific via getsockopt
inline int GetSockError()
{
#ifdef _WIN32
	return( WSAGetLastError() );
#else
	return( errno );
#endif /* _WIN32 */
}

//! wrappers for FD_SET and such to work in templates.
inline void TFD_ZERO( fd_set *set )
{
	FD_ZERO( set );
}

inline void TFD_SET( cs_sock_t iSock, fd_set *set )
{
	FD_SET( iSock, set );
}

inline bool TFD_ISSET( cs_sock_t iSock, fd_set *set )
{
	return( FD_ISSET( iSock, set ) );
}

inline void TFD_CLR( cs_sock_t iSock, fd_set *set )
{
	FD_CLR( iSock, set );
}

void __Perror( const CS_STRING & s, const char * pszFile, uint32_t iLineNo );
uint64_t millitime();


/**
 * @class CCron
 * @brief this is the main cron job class
 *
 * You should derive from this class, and override RunJob() with your code
 * @author Jim Hull <csocket@jimloco.com>
 */
class CCron
{
public:
	CCron();
	virtual ~CCron() {}

	//! This is used by the Job Manager, and not you directly
	void run( timeval & tNow );

	/**
	 * @param TimeSequence	how often to run in seconds
	 * @param iMaxCycles		how many times to run, 0 makes it run forever
	 */
	void StartMaxCycles( double dTimeSequence, uint32_t iMaxCycles );
	void StartMaxCycles( const timeval& tTimeSequence, uint32_t iMaxCycles );

	//! starts and runs infinity amount of times
	void Start( double dTimeSequence );
	void Start( const timeval& TimeSequence );

	//! call this to turn off your cron, it will be removed
	void Stop();

	//! pauses excution of your code in RunJob
	void Pause();

	//! removes the pause on RunJon
	void UnPause();

	timeval GetInterval() const;
	uint32_t GetMaxCycles() const;
	uint32_t GetCyclesLeft() const;

	//! returns true if cron is active
	bool isValid();

	const CS_STRING & GetName() const;
	void SetName( const CS_STRING & sName );

	//! returns the timestamp of the next estimated run time. Note that it may not run at this EXACT time, but it will run at least at this time or after
	timeval GetNextRun() const { return( m_tTime ); }

public:

	//! this is the method you should override
	virtual void RunJob();

protected:
	bool		m_bRunOnNextCall; //!< if set to true, RunJob() gets called on next invocation of run() despite the timeout

private:
	timeval		m_tTime;
	bool		m_bActive, m_bPause;
	timeval		m_tTimeSequence;
	uint32_t		m_iMaxCycles, m_iCycles;
	CS_STRING	m_sName;
};

/**
 * @class CSMonitorFD
 * @brief Class to tie sockets to for monitoring by Csocket at either the Csock or TSockManager.
 */
class CSMonitorFD
{
public:
	CSMonitorFD() { m_bEnabled = true; }
	virtual ~CSMonitorFD() {}

	/**
	 * @brief called before select, typically you don't need to reimplement this just add sockets via Add and let the default implementation have its way
	 * @param miiReadyFds fill with fd's to monitor and the associated bit to check them for (@see CSockManager::ECheckType)
	 * @param iTimeoutMS the timeout to change to, setting this to -1 (the default)
	 * @return returning false will remove this from monitoring. The same effect can be had by setting m_bEnabled to false as it is returned from this
	 */
	virtual bool GatherFDsForSelect( std::map< int, short > & miiReadyFds, long & iTimeoutMS );

	/**
	 * @brief called when there are fd's belonging to this class that have triggered
	 * @param miiReadyFds the map of fd's with the bits that triggered them (@see CSockManager::ECheckType)
	 * @return returning false will remove this from monitoring
	 */
	virtual bool FDsThatTriggered( const std::map< int, short > & miiReadyFds ) { return( true ); }

	/**
	 * @brief gets called to diff miiReadyFds with m_miiMonitorFDs, and calls FDsThatTriggered when appropriate. Typically you don't need to reimplement this.
	 * @param miiReadyFds the map of all triggered fd's, not just the fd's from this class
	 * @return returning false will remove this from monitoring
	 */
	virtual bool CheckFDs( const std::map< int, short > & miiReadyFds );

	/**
	 * @brief adds a file descriptor to be monitored
	 * @param iFD the file descriptor
	 * @param iMonitorEvents bitset of events to monitor for (@see CSockManager::ECheckType)
	 */
	void Add( int iFD, short iMonitorEvents ) { m_miiMonitorFDs[iFD] = iMonitorEvents; }
	//! removes this fd from monitoring
	void Remove( int iFD ) { m_miiMonitorFDs.erase( iFD ); }
	//! causes this monitor to be removed
	void DisableMonitor() { m_bEnabled = false; }

	bool IsEnabled() const { return( m_bEnabled ); }

protected:
	std::map< int, short > m_miiMonitorFDs;
	bool m_bEnabled;
};


/**
 * @class CSockCommon
 * @brief simple class to share common code to both TSockManager and Csock
 */
class CSockCommon
{
public:
	CSockCommon() {}
	virtual ~CSockCommon();

	void CleanupCrons();
	void CleanupFDMonitors();

	//! returns a const reference to the crons associated to this socket
	const std::vector<CCron *> & GetCrons() const { return( m_vcCrons ); }
	//! This has a garbage collecter, and is used internall to call the jobs
	virtual void Cron();

	//! insert a newly created cron
	virtual void AddCron( CCron * pcCron );
	/**
	 * @brief deletes a cron by name
	 * @param sName the name of the cron
	 * @param bDeleteAll delete all crons that match sName
	 * @param bCaseSensitive use strcmp or strcasecmp
	 */
	virtual void DelCron( const CS_STRING & sName, bool bDeleteAll = true, bool bCaseSensitive = true );
	//! delete cron by idx
	virtual void DelCron( uint32_t iPos );
	//! delete cron by address
	virtual void DelCronByAddr( CCron * pcCron );

	void CheckFDs( const std::map< int, short > & miiReadyFds );
	void AssignFDs( std::map< int, short > & miiReadyFds, struct timeval * tvtimeout );

	//! add an FD set to monitor
	void MonitorFD( CSMonitorFD * pMonitorFD ) { m_vcMonitorFD.push_back( pMonitorFD ); }

protected:
	std::vector<CCron *>		m_vcCrons;
	std::vector<CSMonitorFD *>	m_vcMonitorFD;
};


#ifdef HAVE_LIBSSL
typedef int ( *FPCertVerifyCB )( int, X509_STORE_CTX * );
#endif /* HAVE_LIBSSL */


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
class Csock : public CSockCommon
{
public:
	//! default constructor, sets a timeout of 60 seconds
	Csock( int iTimeout = 60 );
	/**
	 * @brief Advanced constructor, for creating a simple connection
	 * @param sHostname the hostname your are connecting to
	 * @param uPort the port you are connecting to
	 * @param itimeout how long to wait before ditching the connection, default is 60 seconds
	 */
	Csock( const CS_STRING & sHostname, uint16_t uPort, int itimeout = 60 );

	//! override this for accept sockets
	virtual Csock *GetSockObj( const CS_STRING & sHostname, uint16_t iPort );

	virtual ~Csock();

	/**
	 * @brief in the event you pass this class to Copy(), you MUST call this function or
	 * on the original Csock other wise bad side effects will happen (double deletes, weird sock closures, etc)
	 * if you call this function and have not handled the internal pointers, other bad things can happend (memory leaks, fd leaks, etc)
	 * the whole point of this function is to allow this class to go away without shutting down
	 */
	virtual void Dereference();
	//! use this to copy a sock from one to the other, override it if you have special needs in the event of a copy
	virtual void Copy( const Csock & cCopy );

	enum ETConn
	{
		OUTBOUND			= 0,		//!< outbound connection
		LISTENER			= 1,		//!< a socket accepting connections
		INBOUND				= 2			//!< an inbound connection, passed from LISTENER
	};

	enum EFRead
	{
		READ_EOF			= 0,		//!< End Of File, done reading
		READ_ERR			= -1,		//!< Error on the socket, socket closed, done reading
		READ_EAGAIN			= -2,		//!< Try to get data again
		READ_CONNREFUSED	= -3,		//!< Connection Refused
		READ_TIMEDOUT		= -4		//!< Connection timed out
	};

	enum EFSelect
	{
		SEL_OK				= 0,		//!< Select passed ok
		SEL_TIMEOUT			= -1,		//!< Select timed out
		SEL_EAGAIN			= -2,		//!< Select wants you to try again
		SEL_ERR				= -3		//!< Select recieved an error
	};

	enum ESSLMethod
	{
		SSL23				= 0,
		SSL2				= 2,
		SSL3				= 3,
		TLS1				= 4
	};

	enum ECONState
	{
		CST_START		= 0,
		CST_DNS			= CST_START,
		CST_BINDVHOST	= 1,
		CST_DESTDNS		= 2,
		CST_CONNECT		= 3,
		CST_CONNECTSSL	= 4,
		CST_OK			= 5
	};

	enum ECloseType
	{
		CLT_DONT			= 0, //!< don't close DER
		CLT_NOW				= 1, //!< close immediatly
		CLT_AFTERWRITE		= 2, //!< close after finishing writing the buffer
		CLT_DEREFERENCE		= 3	 //!< used after copy in Csock::Dereference() to cleanup a sock thats being shutdown
	};

	Csock & operator<<( const CS_STRING & s );
	Csock & operator<<( std::ostream & ( *io )( std::ostream & ) );
	Csock & operator<<( int32_t i );
	Csock & operator<<( uint32_t i );
	Csock & operator<<( int64_t i );
	Csock & operator<<( uint64_t i );
	Csock & operator<<( float i );
	Csock & operator<<( double i );

	/**
	 * @brief Create the connection, this is used by the socket manager, and shouldn't be called directly by the user
	 * @return true on success
	 */
	virtual bool Connect();

	/**
	 * @brief Listens for connections
	 * @param iPort the port to listen on
	 * @param iMaxConns the maximum amount of pending connections to allow
	 * @param sBindHost the vhost on which to listen
	 * @param iTimeout if no connections come in by this timeout, the listener is closed
	 * @param bDetach don't block waiting for port to come up, instead detach and return immediately
	 */
	virtual bool Listen( uint16_t iPort, int iMaxConns = SOMAXCONN, const CS_STRING & sBindHost = "", uint32_t iTimeout = 0, bool bDetach = false );

	//! Accept an inbound connection, this is used internally
	virtual cs_sock_t Accept( CS_STRING & sHost, uint16_t & iRPort );

	//! Accept an inbound SSL connection, this is used internally and called after Accept
	virtual bool AcceptSSL();

	//! This sets up the SSL Client, this is used internally
	virtual bool SSLClientSetup();

	//! This sets up the SSL Server, this is used internally
	virtual bool SSLServerSetup();

	/**
	 * @brief Create the SSL connection
	 * @return true on success
	 *
	 * This is used by the socket manager, and shouldn't be called directly by the user.
	 */
	virtual bool ConnectSSL();

	//! start a TLS connection on an existing plain connection
	bool StartTLS();

	/**
	 * @brief Write data to the socket
	 *
	 * If not all of the data is sent, it will be stored on
	 * an internal buffer, and tried again with next call to Write
	 * if the socket is blocking, it will send everything, its ok to check ernno after this (nothing else is processed)
	 *
	 * @param data the data to send
	 * @param len the length of data
	 */
	virtual bool Write( const char *data, size_t len );

	/**
	 * convenience function
	 * @see Write( const char *, int )
	 */
	virtual bool Write( const CS_STRING & sData );

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
	virtual cs_ssize_t Read( char *data, size_t len );
	CS_STRING GetLocalIP();
	CS_STRING GetRemoteIP();

	//! Tells you if the socket is connected
	virtual bool IsConnected() const;
	//! Sets the sock, telling it its connected (internal use only)
	virtual void SetIsConnected( bool b );

	//! returns a reference to the sock
	cs_sock_t & GetRSock();
	void SetRSock( cs_sock_t iSock );
	cs_sock_t & GetWSock();
	void SetWSock( cs_sock_t iSock );

	void SetSock( cs_sock_t iSock );
	cs_sock_t & GetSock();

	/**
	 * @brief calls SockError, if sDescription is not set, then strerror is used to pull out a default description
	 * @param iErrno the errno to send
	 * @param sDescription the description of the error that occurred
	 */
	void CallSockError( int iErrno, const CS_STRING & sDescription = "" );
	//! resets the time counter, this is virtual in the event you need an event on the timer being Reset
	virtual void ResetTimer();

	//! will pause/unpause reading on this socket
	void PauseRead();
	void UnPauseRead();
	bool IsReadPaused();
	/**
	 * this timeout isn't just connection timeout, but also timeout on
	 * NOT recieving data, to disable this set it to 0
	 * then the normal TCP timeout will apply (basically TCP will kill a dead connection)
	 * Set the timeout, set to 0 to never timeout
	 */
	enum
	{
		TMO_READ	= 1,
		TMO_WRITE	= 2,
		TMO_ACCEPT	= 4,
		TMO_ALL		= TMO_READ|TMO_WRITE|TMO_ACCEPT
	};

	//! Currently this uses the same value for all timeouts, and iTimeoutType merely states which event will be checked
	//! for timeouts
	void SetTimeout( int iTimeout, uint32_t iTimeoutType = TMO_ALL );
	void SetTimeoutType( uint32_t iTimeoutType );
	int GetTimeout() const;
	uint32_t GetTimeoutType() const;

	//! returns true if the socket has timed out
	virtual bool CheckTimeout( time_t iNow );

	/**
	* pushes data up on the buffer, if a line is ready
	* it calls the ReadLine event
	*/
	virtual void PushBuff( const char *data, size_t len, bool bStartAtZero = false );

	//! This gives access to the internal read buffer, if your
	//! not going to use ReadLine(), then you may want to clear this out
	//! (if its binary data and not many '\\n')
	CS_STRING & GetInternalReadBuffer();

	//! This gives access to the internal write buffer.
	//! If you want to check if the send queue fills up, check here.
	CS_STRING & GetInternalWriteBuffer();

	//! sets the max buffered threshold when EnableReadLine() is enabled
	void SetMaxBufferThreshold( uint32_t iThreshold );
	uint32_t GetMaxBufferThreshold() const;

	//! Returns the connection type from enum eConnType
	int GetType() const;
	void SetType( int iType );

	//! Returns a reference to the socket name
	const CS_STRING & GetSockName() const;
	void SetSockName( const CS_STRING & sName );

	//! Returns a reference to the host name
	const CS_STRING & GetHostName() const;
	void SetHostName( const CS_STRING & sHostname );


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
	double GetAvgRead( uint64_t iSample = 1000 );

	//! Get Avg Write Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
	double GetAvgWrite( uint64_t iSample = 1000 );

	//! Returns the remote port
	uint16_t GetRemotePort();

	//! Returns the local port
	uint16_t GetLocalPort();

	//! Returns the port
	uint16_t GetPort();
	void SetPort( uint16_t iPort );

	//! just mark us as closed, the parent can pick it up
	void Close( ECloseType eCloseType = CLT_NOW );
	//! returns int of type to close @see ECloseType
	ECloseType GetCloseType() { return( m_eCloseType ); }
	bool IsClosed() { return( GetCloseType() != CLT_DONT ); }

	//! Use this to change your fd's to blocking or none blocking
	void NonBlockingIO();

	//! Return true if this socket is using ssl. Note this does not mean the SSL state is finished, but simply that its configured to use ssl
	bool GetSSL();
	void SetSSL( bool b );

#ifdef HAVE_LIBSSL
	//! Set the cipher type ( openssl cipher [to see ciphers available] )
	void SetCipher( const CS_STRING & sCipher );
	const CS_STRING & GetCipher();

	//! Set the pem file location
	void SetPemLocation( const CS_STRING & sPemFile );
	const CS_STRING & GetPemLocation();
	void SetPemPass( const CS_STRING & sPassword );
	const CS_STRING & GetPemPass() const;

	//! Set the SSL method type
	void SetSSLMethod( int iMethod );
	int GetSSLMethod();

	void SetSSLObject( SSL *ssl );
	void SetCTXObject( SSL_CTX *sslCtx );
	SSL_SESSION * GetSSLSession();

	void SetCertVerifyCB( FPCertVerifyCB pFP ) { m_pCerVerifyCB = pFP; }
#endif /* HAVE_LIBSSL */

	//! Get the send buffer
	const CS_STRING & GetWriteBuffer();
	void ClearWriteBuffer();

	//! is SSL_accept finished ?
	//! is the ssl properly finished (from write no error)
	bool SslIsEstablished();

	//! Use this to bind this socket to inetd
	bool ConnectInetd( bool bIsSSL = false, const CS_STRING & sHostname = "" );

	//! Tie this guy to an existing real file descriptor
	bool ConnectFD( int iReadFD, int iWriteFD, const CS_STRING & sName, bool bIsSSL = false, ETConn eDirection = INBOUND );

	//! Get the peer's X509 cert
#ifdef HAVE_LIBSSL
	X509 *GetX509();

	//! Returns the peer's public key
	CS_STRING GetPeerPubKey();
	//! Returns the peer's certificate finger print
	long GetPeerFingerprint( CS_STRING & sFP );

	uint32_t GetRequireClientCertFlags();
	//! legacy, deprecated @see SetRequireClientCertFlags
	void SetRequiresClientCert( bool bRequiresCert );
	//! bitwise flags, 0 means don't require cert, SSL_VERIFY_PEER verifies peers, SSL_VERIFY_FAIL_IF_NO_PEER_CERT will cause the connection to fail if no cert 
	void SetRequireClientCertFlags( uint32_t iRequireClientCertFlags ) { m_iRequireClientCertFlags = iRequireClientCertFlags; }
#endif /* HAVE_LIBSSL */

	//! Set The INBOUND Parent sockname
	virtual void SetParentSockName( const CS_STRING & sParentName );
	const CS_STRING & GetParentSockName();

	/**
	 * sets the rate at which we can send data
	 * @param iBytes the amount of bytes we can write
	 * @param iMilliseconds the amount of time we have to rate to iBytes
	 */
	virtual void SetRate( uint32_t iBytes, uint64_t iMilliseconds );

	uint32_t GetRateBytes();
	uint64_t GetRateTime();

	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 * Connected event
	 */
	virtual void Connected() {}
	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 * Disconnected event
	 */
	virtual void Disconnected() {}
	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 * Sock Timed out event
	 */
	virtual void Timeout() {}
	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 * Ready to read data event
	 */
	virtual void ReadData( const char *data, size_t len ) {}
	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks.
	 *  With ReadLine, if your not going to use it IE a data stream, @see EnableReadLine()
	 *
	 * Ready to Read a full line event
	 */
	virtual void ReadLine( const CS_STRING & sLine ) {}
	//! set the value of m_bEnableReadLine to true, we don't want to store a buffer for ReadLine, unless we want it
	void EnableReadLine();
	void DisableReadLine();
	//! returns the value of m_bEnableReadLine, if ReadLine is enabled
	bool HasReadLine() const { return( m_bEnableReadLine ); }

	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 * This WARNING event is called when your buffer for readline exceeds the warning threshold
	 * and triggers this event. Either Override it and do nothing, or SetMaxBufferThreshold()
	 * This event will only get called if m_bEnableReadLine is enabled
	 */
	virtual void ReachedMaxBuffer();
	/**
	 * @brief A sock error occured event
	 *
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 */
	virtual void SockError( int iErrno, const CS_STRING & sDescription ) {}
	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 *
	 * Incoming Connection Event
	 * return false and the connection will fail
	 * default returns true
	 */
	virtual bool ConnectionFrom( const CS_STRING & sHost, uint16_t iPort ) { return( true ); }

	/**
	 * @brief called when type is LISTENER and the listening port is up and running
	 * @param sBindIP the IP that is being bound to. Empty if no bind restriction
	 * @param uPort the listening port
	 */
	virtual void Listening( const CS_STRING & sBindIP, uint16_t uPort ) {}

	/**
	 * Override these functions for an easy interface when using the Socket Manager
	 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	 * as the Socket Manager calls most of these callbacks
	 *
	 * Connection Refused Event
	 *
	 */
	virtual void ConnectionRefused() {}
	/**
	 * This gets called every iteration of CSocketManager::Select() if the socket is ReadPaused
	 */
	virtual void ReadPaused() {}

#ifdef HAVE_LIBSSL
	/**
	 * Gets called immediatly after the m_ssl member is setup and initialized, useful if you need to assign anything
	 * to this ssl session via SSL_set_ex_data
	 */
	virtual void SSLFinishSetup( SSL * pSSL ) {}
#endif /* HAVE_LIBSSL */


	//! return how long it has been (in seconds) since the last read or successful write
	time_t GetTimeSinceLastDataTransaction( time_t iNow = 0 );

	time_t GetLastCheckTimeout() { return( m_iLastCheckTimeoutTime ); }

	//! Returns the time when CheckTimeout() should be called next
	time_t GetNextCheckTimeout( time_t iNow = 0 );

	//! return the data imediatly ready for read
	virtual int GetPending();

	//////////////////////////
	// Connection State Stuff
	//! returns the current connection state
	ECONState GetConState() const { return( m_eConState ); }
	//! sets the connection state to eState
	void SetConState( ECONState eState ) { m_eConState = eState; }

	//! grabs fd's for the sockets
	bool CreateSocksFD();

	//! puts the socks back to the state they were prior to calling CreateSocksFD
	void CloseSocksFD();

	const CS_STRING & GetBindHost() const { return( m_sBindHost ); }
	void SetBindHost( const CS_STRING & sBindHost ) { m_sBindHost = sBindHost; }

	enum EDNSLType
	{
		DNS_VHOST, //!< this lookup is for the vhost bind
		DNS_DEST //!< this lookup is for the destination address
	};

	/**
	 * dns lookup @see EDNSLType
	 * @return 0 for success, EAGAIN to check back again (same arguments as before), ETIMEDOUT on failure
	 */
	int DNSLookup( EDNSLType eDNSLType );

	//! this is only used on outbound connections, listeners bind in a different spot
	bool SetupVHost();

	bool GetIPv6() const { return( m_bIsIPv6 ); }
	void SetIPv6( bool b )
	{
		m_bIsIPv6 = b;
		m_address.SetIPv6( b );
		m_bindhost.SetIPv6( b );
	}

	void SetAFRequire( CSSockAddr::EAFRequire iAFRequire )
	{
		m_address.SetAFRequire( iAFRequire );
		m_bindhost.SetAFRequire( iAFRequire );
	}

	//! returns true if this socket can write its data, primarily used with rate shaping, initialize iNOW to 0 and it sets it on the first call
	bool AllowWrite( uint64_t & iNOW ) const;


	void SetSkipConnect( bool b ) { m_bSkipConnect = b; }

	/**
	 * @brief override this call with your own DNS lookup method if you have one. By default this function is blocking
	 * @param sHostname the hostname to resolve
	 * @param csSockAddr the destination sock address info @see CSSockAddr
	 * @return 0 on success, ETIMEDOUT if no lookup was found, EAGAIN if you should check again later for an answer
	 */
	virtual int GetAddrInfo( const CS_STRING & sHostname, CSSockAddr & csSockAddr );

	/**
	 * @brief retrieve name info (numeric only) for a given sockaddr_storage
	 * @param pAddr the sockaddr_storage
	 * @param iAddrLen the length
	 * @param sIP filled with the IP from getnameinfo
	 * @param piPort if not null, filled with the port
	 * @return 0 on success.
	 *
	 * In the event you want to do additional work before or after getnameinfo is called, you can override this and do just that.
	 * One example is in the event that an ipv6 ip is a mapped ipv4 mapped, you can check like so.
	 * - if( pAddr->ss_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED( &(((const struct sockaddr_in6 *)pAddr)->sin6_addr ) )
	 */
	virtual int ConvertAddress( const struct sockaddr_storage * pAddr, socklen_t iAddrLen, CS_STRING & sIP, uint16_t * piPort );

#ifdef HAVE_C_ARES
	CSSockAddr * GetCurrentAddr() const { return( m_pCurrAddr ); }
	void SetAresFinished( int status ) { m_pCurrAddr = NULL; m_iARESStatus = status; }
	ares_channel GetAresChannel() { return( m_pARESChannel ); }
#endif /* HAVE_C_ARES */

	//! returns the number of max pending connections when type is LISTENER
	int GetMaxConns() const { return( m_iMaxConns ); }

private:
	//! making private for safety
	Csock( const Csock & cCopy ) : CSockCommon() {}

	// NOTE! if you add any new members, be sure to add them to Copy()
	uint16_t	m_uPort, m_iRemotePort, m_iLocalPort;
	cs_sock_t	m_iReadSock, m_iWriteSock;
	int 		m_iTimeout, m_iConnType, m_iMethod, m_iTcount, m_iMaxConns;
	bool		m_bUseSSL, m_bIsConnected, m_bBLOCK;
	bool		m_bsslEstablished, m_bEnableReadLine, m_bPauseRead;
	CS_STRING	m_shostname, m_sbuffer, m_sSockName, m_sPemFile, m_sCipherType, m_sParentName;
	CS_STRING	m_sSend, m_sPemPass, m_sLocalIP, m_sRemoteIP;
	ECloseType	m_eCloseType;

	uint64_t	m_iMaxMilliSeconds, m_iLastSendTime, m_iBytesRead, m_iBytesWritten, m_iStartTime;
	uint32_t	m_iMaxBytes, m_iMaxStoredBufferLength, m_iTimeoutType;
	size_t		m_iLastSend;

	CSSockAddr 	m_address, m_bindhost;
	bool		m_bIsIPv6, m_bSkipConnect;
	time_t		m_iLastCheckTimeoutTime;

#ifdef HAVE_LIBSSL
	CS_STRING	m_sSSLBuffer;
	SSL	*		m_ssl;
	SSL_CTX	*	m_ssl_ctx;
	uint32_t	m_iRequireClientCertFlags;

	FPCertVerifyCB		m_pCerVerifyCB;

	void FREE_SSL();
	void FREE_CTX();

#endif /* HAVE_LIBSSL */

	//! Create the socket
	cs_sock_t CreateSocket( bool bListen = false );
	void Init( const CS_STRING & sHostname, uint16_t uPort, int iTimeout = 60 );

	// Connection State Info
	ECONState		m_eConState;
	CS_STRING		m_sBindHost;
	uint32_t		m_iCurBindCount, m_iDNSTryCount;
#ifdef HAVE_C_ARES
	void FreeAres();
	ares_channel	m_pARESChannel;
	CSSockAddr	*	m_pCurrAddr;
	int				m_iARESStatus;
#endif /* HAVE_C_ARES */

};

/**
 * @class CSConnection
 * @brief options for creating a connection
 */
class CSConnection
{
public:
	/**
	 * @param sHostname hostname to connect to
	 * @param iPort port to connect to
	 * @param iTimeout connection timeout
	 */
	CSConnection( const CS_STRING & sHostname, uint16_t iPort, int iTimeout = 60 )
	{
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

	const CS_STRING & GetHostname() const { return( m_sHostname ); }
	const CS_STRING & GetSockName() const { return( m_sSockName ); }
	const CS_STRING & GetBindHost() const { return( m_sBindHost ); }
	uint16_t GetPort() const { return( m_iPort ); }
	int GetTimeout() const { return( m_iTimeout ); }
	bool GetIsSSL() const { return( m_bIsSSL ); }
	CSSockAddr::EAFRequire GetAFRequire() const { return( m_iAFrequire ); }

#ifdef HAVE_LIBSSL
	const CS_STRING & GetCipher() const { return( m_sCipher ); }
	const CS_STRING & GetPemLocation() const { return( m_sPemLocation ); }
	const CS_STRING & GetPemPass() const { return( m_sPemPass ); }
#endif /* HAVE_LIBSSL */

	//! sets the hostname to connect to
	void SetHostname( const CS_STRING & s ) { m_sHostname = s; }
	//! sets the name of the socket, used for reference, ie in FindSockByName()
	void SetSockName( const CS_STRING & s ) { m_sSockName = s; }
	//! sets the hostname to bind to (vhost support)
	void SetBindHost( const CS_STRING & s ) { m_sBindHost = s; }
	//! sets the port to connect to
	void SetPort( uint16_t i ) { m_iPort = i; }
	//! sets the connection timeout
	void SetTimeout( int i ) { m_iTimeout = i; }
	//! set to true to enable SSL
	void SetIsSSL( bool b ) { m_bIsSSL = b; }
	//! sets the AF family type required
	void SetAFRequire( CSSockAddr::EAFRequire iAFRequire ) { m_iAFrequire = iAFRequire; }

#ifdef HAVE_LIBSSL
	//! set the cipher strength to use, default is HIGH
	void SetCipher( const CS_STRING & s ) { m_sCipher = s; }
	//! set the location of the pemfile
	void SetPemLocation( const CS_STRING & s ) { m_sPemLocation = s; }
	//! set the pemfile pass
	void SetPemPass( const CS_STRING & s ) { m_sPemPass = s; }
#endif /* HAVE_LIBSSL */

protected:
	CS_STRING	m_sHostname, m_sSockName, m_sBindHost;
	uint16_t	m_iPort;
	int			m_iTimeout;
	bool		m_bIsSSL;
	CSSockAddr::EAFRequire	m_iAFrequire;
#ifdef HAVE_LIBSSL
	CS_STRING	m_sPemLocation, m_sPemPass, m_sCipher;
#endif /* HAVE_LIBSSL */
};

class CSSSLConnection : public CSConnection
{
public:
	CSSSLConnection( const CS_STRING & sHostname, uint16_t iPort, int iTimeout = 60 ) :
		CSConnection( sHostname, iPort, iTimeout )
	{
		SetIsSSL( true );
	}
};


/**
 * @class CSListener
 * @brief options container to create a listener
 */
class CSListener
{
public:
	/**
	 * @param iPort port to listen on. Set to 0 to listen on a random port
	 * @param sBindHost host to bind to
	 * @param bDetach don't block while waiting for the port to come up, instead detach and return immediately
	 */
	CSListener( uint16_t iPort, const CS_STRING & sBindHost = "", bool bDetach = false )
	{
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

	void SetDetach( bool b ) { m_bDetach = b; }
	bool GetDetach() const { return( m_bDetach ); }
	uint16_t GetPort() const { return( m_iPort ); }
	const CS_STRING & GetSockName() const { return( m_sSockName ); }
	const CS_STRING & GetBindHost() const { return( m_sBindHost ); }
	bool GetIsSSL() const { return( m_bIsSSL ); }
	int GetMaxConns() const { return( m_iMaxConns ); }
	uint32_t GetTimeout() const { return( m_iTimeout ); }
	CSSockAddr::EAFRequire GetAFRequire() const { return( m_iAFrequire ); }
#ifdef HAVE_LIBSSL
	const CS_STRING & GetCipher() const { return( m_sCipher ); }
	const CS_STRING & GetPemLocation() const { return( m_sPemLocation ); }
	const CS_STRING & GetPemPass() const { return( m_sPemPass ); }
	uint32_t GetRequireClientCertFlags() const { return( m_iRequireCertFlags ); }
#endif /* HAVE_LIBSSL */

	//! sets the port to listen on. Set to 0 to listen on a random port
	void SetPort( uint16_t iPort ) { m_iPort = iPort; }
	//! sets the sock name for later reference (ie FindSockByName)
	void SetSockName( const CS_STRING & sSockName ) { m_sSockName = sSockName; }
	//! sets the host to bind to
	void SetBindHost( const CS_STRING & sBindHost ) { m_sBindHost = sBindHost; }
	//! set to true to enable SSL
	void SetIsSSL( bool b ) { m_bIsSSL = b; }
	//! set max connections as called by accept()
	void SetMaxConns( int i ) { m_iMaxConns = i; }
	//! sets the listen timeout. The listener class will close after timeout has been reached if not 0
	void SetTimeout( uint32_t i ) { m_iTimeout = i; }
	//! sets the AF family type required
	void SetAFRequire( CSSockAddr::EAFRequire iAFRequire ) { m_iAFrequire = iAFRequire; }

#ifdef HAVE_LIBSSL
	//! set the cipher strength to use, default is HIGH
	void SetCipher( const CS_STRING & s ) { m_sCipher = s; }
	//! set the location of the pemfile
	void SetPemLocation( const CS_STRING & s ) { m_sPemLocation = s; }
	//! set the pemfile pass
	void SetPemPass( const CS_STRING & s ) { m_sPemPass = s; }
	//! set to true if require a client certificate (deprecated @see SetRequireClientCertFlags)
	void SetRequiresClientCert( bool b ) { m_iRequireCertFlags = ( b ? SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0 ); }
	//! bitwise flags, 0 means don't require cert, SSL_VERIFY_PEER verifies peers, SSL_VERIFY_FAIL_IF_NO_PEER_CERT will cause the connection to fail if no cert
	void SetRequireClientCertFlags( unsigned int iRequireCertFlags ) { m_iRequireCertFlags = iRequireCertFlags; }
#endif /* HAVE_LIBSSL */
private:
	uint16_t	m_iPort;
	CS_STRING	m_sSockName, m_sBindHost;
	bool		m_bIsSSL;
	bool		m_bDetach;
	int			m_iMaxConns;
	uint32_t	m_iTimeout;
	CSSockAddr::EAFRequire	m_iAFrequire;

#ifdef HAVE_LIBSSL
	CS_STRING	m_sPemLocation, m_sPemPass, m_sCipher;
	uint32_t		m_iRequireCertFlags;
#endif /* HAVE_LIBSSL */
};

#ifdef HAVE_LIBSSL
class CSSSListener : public CSListener
{
public:
	CSSSListener( uint16_t iPort, const CS_STRING & sBindHost = "" ) :
		CSListener( iPort, sBindHost )
	{
		SetIsSSL( true );
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
 * Another thing to note, is that all sockets are deleted implicitly, so obviously you
 * can't pass in Csock classes created on the stack. For those of you who don't
 * know STL very well, the reason I did this is because whenever you add to certain stl containers
 * (e.g. vector, or map), it is completely rebuilt using the copy constructor on each element.
 * That then means the constructor and destructor are called on every item in the container.
 * Not only is this more overhead then just moving pointers around, its dangerous as if you have
 * an object that is newed and deleted in the destructor the value of its pointer is copied in the
 * default copy constructor. This means everyone has to know better and create a copy constructor,
 * or I just make everyone new their object :).
 *
 * @see TSocketManager
 * @author Jim Hull <csocket@jimloco.com>
 */
class CSocketManager : public std::vector<Csock *>, public CSockCommon
{
public:
	CSocketManager();
	virtual ~CSocketManager();
	virtual void clear();
	virtual void Cleanup();

	virtual Csock * GetSockObj( const CS_STRING & sHostname, uint16_t uPort, int iTimeout = 60 );

	enum EMessages
	{
		SUCCESS			= 0,	//!< Select returned at least 1 fd ready for action
		SELECT_ERROR	= -1,	//!< An Error Happened, Probably dead socket. That socket is returned if available
		SELECT_TIMEOUT	= -2,	//!< Select Timeout
		SELECT_TRYAGAIN	= -3	//!< Select calls for you to try again
	};

	/**
	 * @brief Create a connection
	 * @param cCon the connection which should be established
	 * @param pcSock the socket used for the connection, can be NULL
	 */
	void Connect( const CSConnection & cCon, Csock * pcSock = NULL );

	/**
	 * @brief Sets up a listening socket
	 * @param cListen the listener configuration
	 * @param pcSock preconfigured sock to use
	 * @param piRandPort if listener is set to port 0, then a random port is used and this is assigned.
	 *
	 * IF you provide piRandPort to be assigned, AND you set bDetach to true, then Listen() still blocks. If you don't want this
	 * behavior, then look for the port assignment to be called in Csock::Listening
	 */
	virtual bool Listen( const CSListener & cListen, Csock * pcSock = NULL, uint16_t * piRandPort = NULL );


	//! simple method to see if there are file descriptors being processed, useful to know if all the work is done in the manager
	bool HasFDs() const;

	/**
	 * Best place to call this class for running, all the call backs are called.
	 * You should through this in your main while loop (long as its not blocking)
	 * all the events are called as needed.
	 */
	virtual void Loop();

	/**
	 * @brief this is similar to loop, except that it dynamically adjusts the select time based on jobs and timeouts in sockets
	 *
	 *	- This type of behavior only works well in a scenario where there is low traffic. If you use this then its because you
	 *	- are trying to spare yourself some of those idle loops where nothing is done. If you try to use this code where you have lots of
	 *	- connections and/or lots of traffic you might end up causing more CPU usage than just a plain Loop() with a static sleep of 500ms
	 *	- its a trade off at some point, and you'll probably find out that the vast majority of the time and in most cases Loop() works fine
	 *	- by itself. I've tried to mitigate that as much as possible by not having it change the select if the previous call to select
	 *	- was not a timeout. Anyways .... Caveat Emptor.
	 *	- Sample useage is cFoo.DynamicSelectLoop( 500000, 5000000 ); which basically says min of 500ms and max of 5s
	 *
	 * @param iLowerBounds the lower bounds to use in MICROSECONDS
	 * @param iUpperBounds the upper bounds to use in MICROSECONDS
	 * @param iMaxResolution the maximum time to calculate overall in seconds
	 */
	void DynamicSelectLoop( uint64_t iLowerBounds, uint64_t iUpperBounds, time_t iMaxResolution = 3600 );

	/**
	 * Make this method virtual, so you can override it when a socket is added.
	 * Assuming you might want to do some extra stuff
	 */
	virtual void AddSock( Csock * pcSock, const CS_STRING & sSockName );

	//! returns a pointer to the FIRST sock found by port or NULL on no match
	virtual Csock * FindSockByRemotePort( uint16_t iPort );

	//! returns a pointer to the FIRST sock found by port or NULL on no match
	virtual Csock * FindSockByLocalPort( uint16_t iPort );

	//! returns a pointer to the FIRST sock found by name or NULL on no match
	virtual Csock * FindSockByName( const CS_STRING & sName );

	//! returns a pointer to the FIRST sock found by filedescriptor or NULL on no match
	virtual Csock * FindSockByFD( cs_sock_t iFD );

	virtual std::vector<Csock *> FindSocksByName( const CS_STRING & sName );

	//! returns a vector of pointers to socks with sHostname as being connected
	virtual std::vector<Csock *> FindSocksByRemoteHost( const CS_STRING & sHostname );

	//! return the last known error as set by this class
	int GetErrno() { return( m_errno ); }

	//! Get the Select Timeout in MICROSECONDS ( 1000 == 1 millisecond )
	uint64_t GetSelectTimeout() { return( m_iSelectWait ); }
	//! Set the Select Timeout in MICROSECONDS ( 1000 == 1 millisecond )
	//! Setting this to 0 will cause no timeout to happen, Select() will return instantly
	void  SetSelectTimeout( uint64_t iTimeout ) { m_iSelectWait = iTimeout; }

	//! Delete a sock by addr
	//! its position is looked up
	//! the socket is deleted, the appropriate call backs are peformed
	//! and its instance is removed from the manager
	virtual void DelSockByAddr( Csock * pcSock );

	//! Delete a sock by position in the vector
	//! the socket is deleted, the appropriate call backs are peformed
	//! and its instance is removed from the manager
	//! deleting in a loop can be tricky, be sure you watch your position.
	//! ie for( uint32_t a = 0; a < size(); a++ ) DelSock( a-- );
	virtual void DelSock( size_t iPos );

	/**
	 * @brief swaps out a sock with a copy of the original sock
	 * @param pNewSock the new sock to change out with. (this should be constructed by you with the default ctor)
	 * @param iOrginalSockIdx the position in this sockmanager of the original sock
	 * @return true on success
	 */
	virtual bool SwapSockByIdx( Csock * pNewSock, size_t iOrginalSockIdx );

	/**
	 * @brief swaps out a sock with a copy of the original sock
	 * @param pNewSock the new sock to change out with. (this should be constructed by you with the default ctor)
	 * @param pOrigSock the address of the original socket
	 * @return true on success
	 */
	virtual bool SwapSockByAddr( Csock * pNewSock, Csock * pOrigSock );

	//! Get the bytes read from all sockets current and past
	uint64_t GetBytesRead() const;

	//! Get the bytes written to all sockets current and past
	uint64_t GetBytesWritten() const;

	//! this is a strict wrapper around C-api select(). Added in the event you need to do special work here
	enum ECheckType
	{
		ECT_Read = 1,
		ECT_Write = 2
	};

	void FDSetCheck( int iFd, std::map< int, short > & miiReadyFds, ECheckType eType );
	bool FDHasCheck( int iFd, std::map< int, short > & miiReadyFds, ECheckType eType );

protected:

	virtual int Select( std::map< int, short > & miiReadyFds, struct timeval *tvtimeout );

private:
	/**
	 * @brief fills a map of socks to a message for check
	 * map is empty if none are ready, check GetErrno() for the error, if not SUCCESS Select() failed
	 * each struct contains the socks error
	 * @see GetErrno()
	 */
	void Select( std::map<Csock *, EMessages> & mpeSocks );

	timeval GetDynamicSleepTime( const timeval& tNow, const timeval& tMaxResolution ) const;

	//! internal use only
	virtual void SelectSock( std::map<Csock *, EMessages> & mpeSocks, EMessages eErrno, Csock * pcSock );

	////////
	// Connection State Functions

	///////////
	// members
	EMessages		m_errno;
	uint64_t		m_iCallTimeouts;
	uint64_t		m_iBytesRead;
	uint64_t		m_iBytesWritten;
	uint64_t		m_iSelectWait;
};


/**
 * @class TSocketManager
 * @brief Ease of use templated socket manager
 *
 * class CBlahSock : public TSocketManager<SomeSock>
 */
template<class T>
class TSocketManager : public CSocketManager
{
public:
	TSocketManager() : CSocketManager() {}
	virtual ~TSocketManager() {}
	virtual T * GetSockObj( const CS_STRING & sHostname, uint16_t uPort, int iTimeout = 60 )
	{
		return( new T( sHostname, uPort, iTimeout ) );
	}
};

#ifndef _NO_CSOCKET_NS
}
#endif /* _NO_CSOCKET_NS */

#endif /* _HAS_CSOCKET_ */

