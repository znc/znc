/**
*
*    Copyright (c) 1999-2009 Jim Hull <imaginos@imaginos.net>
*    All rights reserved
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list
* of conditions and the following disclaimer in the documentation and/or other materials
*  provided with the distribution.
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
*
*
*/

// note to compile with win32 need to link to winsock2, using gcc its -lws2_32

#ifndef _HAS_CSOCKET_
#define _HAS_CSOCKET_
#include "defines.h" // require this as a general rule, most projects have a defines.h or the like

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
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
 * @brief ease of use self deleting char * class
 */
class CSCharBuffer
{
public:
	CSCharBuffer( size_t iSize )
	{
		m_pBuffer = (char *)malloc( iSize );
	}
	~CSCharBuffer()
	{
		free( m_pBuffer );
	}
	char * operator()() { return( m_pBuffer ); }

private:
	char *	m_pBuffer;
};

class CSSockAddr
{
public:
	CSSockAddr()
	{
		m_bIsIPv6 = false;
		memset( (struct sockaddr_in *)&m_saddr, '\0', sizeof( m_saddr ) );
#ifdef HAVE_IPV6
		memset( (struct sockaddr_in6 *)&m_saddr6, '\0', sizeof( m_saddr6 ) );
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

	void SinFamily()
	{
#ifdef HAVE_IPV6
		m_saddr6.sin6_family = PF_INET6;
#endif /* HAVE_IPV6 */
		m_saddr.sin_family = PF_INET;
	}
	void SinPort( u_short iPort )
	{
#ifdef HAVE_IPV6
		m_saddr6.sin6_port = htons( iPort );
#endif /* HAVE_IPV6 */
		m_saddr.sin_port = htons( iPort );
	}

	void SetIPv6( bool b )
	{
#ifndef HAVE_IPV6
		if( b )
		{
			CS_DEBUG( "-DHAVE_IPV6 must be set during compile time to enable this feature" );
			m_bIsIPv6 = false;
			return;
		}
#endif /* HAVE_IPV6 */
		m_bIsIPv6 = b;
		SinFamily();
	}
	bool GetIPv6() const { return( m_bIsIPv6 ); }


	socklen_t GetSockAddrLen() { return( sizeof( m_saddr ) ); }
	sockaddr_in * GetSockAddr() { return( &m_saddr ); }
	in_addr * GetAddr() { return( &(m_saddr.sin_addr) ); }
#ifdef HAVE_IPV6
	socklen_t GetSockAddrLen6() { return( sizeof( m_saddr6 ) ); }
	sockaddr_in6 * GetSockAddr6() { return( &m_saddr6 ); }
	in6_addr * GetAddr6() { return( &(m_saddr6.sin6_addr) ); }
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
 * @brief this function is a wrapper around gethostbyname and getaddrinfo (for ipv6)
 *
 * in the event this code is using ipv6, it calls getaddrinfo, and it tries to start the connection on each iteration
 * in the linked list returned by getaddrinfo. if pSock is not NULL the following behavior happens.
 * - if pSock is a listener, or if the connect state is in a bind vhost state (to be used with bind) AI_PASSIVE is sent to getaddrinfo
 * - if pSock is an outbound connection, AI_ADDRCONFIG and the connection is started from within this function.
 * getaddrinfo might return multiple (possibly invalid depending on system configuration) ip addresses, so connect needs to try them all.
 * A classic example of this is a hostname that resolves to both ipv4 and ipv6 ip's. You still need to call Connect (and ConnectSSL) to finish
 * up the connection state
 * - NOTE ... Once threading is reimplemented, this function will spin off a thread to resolve and return EAGAIN until its done.
 *
 * @param sHostname the host to resolve
 * @param pSock the sock being setup, this option can be NULL, if it is null csSockAddr is only setup
 * @param csSockAddr the struct that sockaddr data is being copied to
 * @return 0 on success, otherwise an error. EAGAIN if this needs to be called again at a later time, ETIMEDOUT if no host is found
 */
int GetAddrInfo( const CS_STRING & sHostname, Csock *pSock, CSSockAddr & csSockAddr );

//! used to retrieve the context position of the socket to its associated ssl connection. Setup once in InitSSL() via SSL_get_ex_new_index
int GetCsockClassIdx();

#ifdef HAVE_LIBSSL
//! returns the sock object associated to the particular context. returns NULL on failure or if not available
Csock *GetCsockFromCTX( X509_STORE_CTX *pCTX );
#endif /* HAVE_LIBSSL */


const u_int CS_BLOCKSIZE = 4096;
template <class T> inline void CS_Delete( T * & p ) { if( p ) { delete p; p = NULL; } }

#ifdef HAVE_LIBSSL
enum ECompType
{
	CT_NONE	= 0,
	CT_ZLIB	= 1,
	CT_RLE	= 2
};

void SSLErrors( const char *filename, u_int iLineNum );

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
	if ( FD_ISSET( iSock, set ) )
		return( true );

	return( false );
}

inline void TFD_CLR( cs_sock_t iSock, fd_set *set )
{
	FD_CLR( iSock, set );
}

void __Perror( const CS_STRING & s, const char *pszFile, unsigned int iLineNo );
unsigned long long millitime();


/**
* @class CCron
* @brief this is the main cron job class
*
* You should derive from this class, and override RunJob() with your code
* @author Jim Hull <imaginos@imaginos.net>
*/

class CCron
{
public:
	CCron();
	virtual ~CCron() {}

	//! This is used by the Job Manager, and not you directly
	void run( time_t & iNow );

	/**
	 * @param TimeSequence	how often to run in seconds
	 * @param iMaxCycles		how many times to run, 0 makes it run forever
	 */
	void StartMaxCycles( int TimeSequence, u_int iMaxCycles );

	//! starts and runs infinity amount of times
	void Start( int TimeSequence );

	//! call this to turn off your cron, it will be removed
	void Stop();

	//! pauses excution of your code in RunJob
	void Pause();

	//! removes the pause on RunJon
	void UnPause();

	int GetInterval() const;
	u_int GetMaxCycles() const;
	u_int GetCyclesLeft() const;

	//! returns true if cron is active
	bool isValid();

	const CS_STRING & GetName() const;
	void SetName( const CS_STRING & sName );

	//! returns the timestamp of the next estimated run time. Note that it may not run at this EXACT time, but it will run at least at this time or after
	time_t GetNextRun() const { return( m_iTime ); }

public:

	//! this is the method you should override
	virtual void RunJob();

protected:
	bool		m_bRunOnNextCall; //!< if set to true, RunJob() gets called on next invocation of run() despite the timeout

private:
	time_t		m_iTime;
	bool		m_bActive, m_bPause;
	int			m_iTimeSequence;
	u_int		m_iMaxCycles, m_iCycles;
	CS_STRING	m_sName;
};

#ifdef HAVE_LIBSSL
typedef int (*FPCertVerifyCB)( int, X509_STORE_CTX * );
#endif /* HAVE_LIBSSL */

/**
* @class Csock
* @brief Basic Socket Class
* The most basic level socket class
* You can use this class directly for quick things
* or use the socket manager
* @see TSocketManager
* @author Jim Hull <imaginos@imaginos.net>
*/
class Csock
{
public:
	//! default constructor, sets a timeout of 60 seconds
	Csock( int itimeout = 60 );
	/**
	* Advanced constructor, for creating a simple connection
	*
	* @param sHostname the hostname your are connecting to
	* @param uPort the port you are connectint to
	* @param itimeout how long to wait before ditching the connection, default is 60 seconds
	*/
	Csock( const CS_STRING & sHostname, u_short uPort, int itimeout = 60 );

	//! override this for accept sockets
	virtual Csock *GetSockObj( const CS_STRING & sHostname, u_short iPort );

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
		READ_EOF			= 0,			//!< End Of File, done reading
		READ_ERR			= -1,			//!< Error on the socket, socket closed, done reading
		READ_EAGAIN			= -2,			//!< Try to get data again
		READ_CONNREFUSED	= -3,			//!< Connection Refused
		READ_TIMEDOUT		= -4			//!< Connection timed out
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
		CLT_DONT			= 0, //! don't close DER
		CLT_NOW				= 1, //! close immediatly
		CLT_AFTERWRITE		= 2,  //! close after finishing writing the buffer
		CLT_DEREFERENCE		= 3	 //! used after copy in Csock::Dereference() to cleanup a sock thats being shutdown
	};

	Csock & operator<<( const CS_STRING & s );
	Csock & operator<<( std::ostream & ( *io )( std::ostream & ) );
	Csock & operator<<( int i );
	Csock & operator<<( unsigned int i );
	Csock & operator<<( long i );
	Csock & operator<<( unsigned long i );
	Csock & operator<<( unsigned long long i );
	Csock & operator<<( float i );
	Csock & operator<<( double i );

	/**
	* Create the connection
	*
	* @param sBindHost the ip you want to bind to locally
	* @param bSkipSetup if true, setting up the vhost etc is skipped
	* @return true on success
	*/
	virtual bool Connect( const CS_STRING & sBindHost = "", bool bSkipSetup = false );

	/**
	* WriteSelect on this socket
	* Only good if JUST using this socket, otherwise use the TSocketManager
	*/
	virtual int WriteSelect();

	/**
	* ReadSelect on this socket
	* Only good if JUST using this socket, otherwise use the TSocketManager
	*/
	virtual int ReadSelect();

	/**
	* Listens for connections
	*
	* @param iPort the port to listen on
	* @param iMaxConns the maximum amount of connections to allow
	* @param sBindHost the vhost on which to listen
	* @param iTimeout i dont know what this does :(
	*/
	virtual bool Listen( u_short iPort, int iMaxConns = SOMAXCONN, const CS_STRING & sBindHost = "", u_int iTimeout = 0 );

	//! Accept an inbound connection, this is used internally
	virtual cs_sock_t Accept( CS_STRING & sHost, u_short & iRPort );

	//! Accept an inbound SSL connection, this is used internally and called after Accept
	virtual bool AcceptSSL();

	//! This sets up the SSL Client, this is used internally
	virtual bool SSLClientSetup();

	//! This sets up the SSL Server, this is used internally
	virtual bool SSLServerSetup();

	/**
	* Create the SSL connection
	*
	* @param sBindhost the ip you want to bind to locally
	* @return true on success
	*/
	virtual bool ConnectSSL( const CS_STRING & sBindhost = "" );


	/**
	* Write data to the socket
	* if not all of the data is sent, it will be stored on
	* an internal buffer, and tried again with next call to Write
	* if the socket is blocking, it will send everything, its ok to check ernno after this (nothing else is processed)
	*
	* @param data the data to send
	* @param len the length of data
	*
	*/
	virtual bool Write( const char *data, size_t len );

	/**
	* convience function
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

	virtual CS_STRING ConvertAddress( void *addr, bool bIPv6 = false );

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
	void SetTimeout( int iTimeout, u_int iTimeoutType = TMO_ALL );
	void SetTimeoutType( u_int iTimeoutType );
	int GetTimeout() const;
	u_int GetTimeoutType() const;

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
	void SetMaxBufferThreshold( u_int iThreshold );
	u_int GetMaxBufferThreshold() const;

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
	unsigned long long GetStartTime() const;
	//! Resets the start time
	void ResetStartTime();

	//! Gets the amount of data read during the existence of the socket
	unsigned long long GetBytesRead() const;
	void ResetBytesRead();

	//! Gets the amount of data written during the existence of the socket
	unsigned long long GetBytesWritten() const;
	void ResetBytesWritten();

	//! Get Avg Read Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
	double GetAvgRead( unsigned long long iSample = 1000 );

	//! Get Avg Write Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
	double GetAvgWrite( unsigned long long iSample = 1000 );

	//! Returns the remote port
	u_short GetRemotePort();

	//! Returns the local port
	u_short GetLocalPort();

	//! Returns the port
	u_short GetPort();
	void SetPort( u_short iPort );

	//! just mark us as closed, the parent can pick it up
	void Close( ECloseType eCloseType = CLT_NOW );
	//! returns int of type to close @see ECloseType
	ECloseType GetCloseType() { return( m_eCloseType ); }
	bool IsClosed() { return( GetCloseType() != CLT_DONT ); }

	//! Set rather to NON Blocking IO on this socket, default is true
	void BlockIO( bool bBLOCK );

	//! Use this to change your fd's to blocking or none blocking
	void NonBlockingIO();

	//! if this connection type is ssl or not
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
	static int PemPassCB( char *buf, int size, int rwflag, void *pcSocket );
	static int CertVerifyCB( int preverify_ok, X509_STORE_CTX *x509_ctx );

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
	X509 *getX509();

	//! Returns the peer's public key
	CS_STRING GetPeerPubKey();
	//! Returns the peer's certificate finger print
	int GetPeerFingerprint( CS_STRING & sFP);

	unsigned int GetRequireClientCertFlags();
	//! legacy, deprecated @see SetRequireClientCertFlags
	void SetRequiresClientCert( bool bRequiresCert );
	//! bitwise flags, 0 means don't require cert, SSL_VERIFY_PEER verifies peers, SSL_VERIFY_FAIL_IF_NO_PEER_CERT will cause the connection to fail if no cert 
	void SetRequireClientCertFlags( unsigned int iRequireClientCertFlags ) { m_iRequireClientCertFlags = iRequireClientCertFlags; }

#endif /* HAVE_LIBSSL */

	//! Set The INBOUND Parent sockname
	virtual void SetParentSockName( const CS_STRING & sParentName );
	const CS_STRING & GetParentSockName();

	/**
	* sets the rate at which we can send data
	* @param iBytes the amount of bytes we can write
	* @param iMilliseconds the amount of time we have to rate to iBytes
	*/
	virtual void SetRate( u_int iBytes, unsigned long long iMilliseconds );

	u_int GetRateBytes();
	unsigned long long GetRateTime();
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
	virtual void DelCron( u_int iPos );
	//! delete cron by address
	virtual void DelCronByAddr( CCron *pcCron );

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
	* Override these functions for an easy interface when using the Socket Manager
	* Don't bother using these callbacks if you are using this class directly (without Socket Manager)
	* as the Socket Manager calls most of these callbacks
	*
	* A sock error occured event
	*/
	virtual void SockError( int iErrno ) {}
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
	virtual bool ConnectionFrom( const CS_STRING & sHost, u_short iPort ) { return( true ); }

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
	 * This gets called every iteration of TSocketManager::Select() if the socket is ReadPaused
	 */
	virtual void ReadPaused() {}

#ifdef HAVE_LIBSSL
	/**
	 * Gets called immediatly after the m_ssl member is setup and initialized, useful if you need to assign anything
	 * to this ssl session via SSL_set_ex_data
	 */
	virtual void SSLFinishSetup( SSL *pSSL ) {}
#endif /* HAVE_LIBSSL */


	//! return how long it has been (in seconds) since the last read or successful write
	time_t GetTimeSinceLastDataTransaction( time_t iNow = 0 )
	{
		if( m_iLastCheckTimeoutTime == 0 )
			return( 0 );
		return( ( iNow > 0 ? iNow : time( NULL ) ) - m_iLastCheckTimeoutTime );
	}
	time_t GetLastCheckTimeout() { return( m_iLastCheckTimeoutTime ); }

	//! Returns the time when CheckTimeout() should be called next
	time_t GetNextCheckTimeout( time_t iNow = 0 ) 
	{
		if( iNow == 0 )
			iNow = time( NULL );
		time_t itimeout = m_itimeout;
		time_t iDiff = iNow - m_iLastCheckTimeoutTime;
		/* CheckTimeout() wants to be called after half the timeout */
		if( m_iTcount == 0 )
			itimeout /= 2;
		if( iDiff > itimeout )
			itimeout = 0;
		else
			itimeout -= iDiff;
		return( iNow + itimeout );
	}

	//! return the data imediatly ready for read
	virtual int GetPending();

	//////////////////////////
	// Connection State Stuff
	//! returns the current connection state
	ECONState GetConState() const { return( m_eConState ); }
	//! sets the connection state to eState
	void SetConState( ECONState eState ) { m_eConState = eState; }

	//! grabs fd's for the sockets
	bool CreateSocksFD()
	{
		if( m_iReadSock != CS_INVALID_SOCK )
			return( true );

		m_iReadSock = m_iWriteSock = CreateSocket();
		if ( m_iReadSock == CS_INVALID_SOCK )
			return( false );

		m_address.SinFamily();
		m_address.SinPort( m_uPort );

		return( true );
	}

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
	bool AllowWrite( unsigned long long & iNOW ) const;

	//! returns a const reference to the crons associated to this socket
	const std::vector<CCron *> & GetCrons() const { return( m_vcCrons ); }

	void SetSkipConnect( bool b ) { m_bSkipConnect = b; }

	/**
	 * @brief override this call with your own DNS lookup method if you have one. By default this function is blocking
	 * @param sHostname the hostname to resolve
	 * @param csSockAddr the destination sock address info @see CSSockAddr
	 * @return 0 on success, ETIMEDOUT if no lookup was found, EAGAIN if you should check again later for an answer
	 */
	virtual int GetAddrInfo( const CS_STRING & sHostname, CSSockAddr & csSockAddr );

#ifdef HAVE_C_ARES
	CSSockAddr * GetCurrentAddr() const { return( m_pCurrAddr ); }
	void SetAresFinished( int status ) { m_pCurrAddr = NULL; m_iARESStatus = status; }
	ares_channel GetAresChannel() { return( m_pARESChannel ); }
#endif /* HAVE_C_ARES */

private:
	//! making private for safety
	Csock( const Csock & cCopy ) {}

	// NOTE! if you add any new members, be sure to add them to Copy()
	u_short		m_uPort, m_iRemotePort, m_iLocalPort;
	cs_sock_t	m_iReadSock, m_iWriteSock;
	int m_itimeout, m_iConnType, m_iMethod, m_iTcount;
	bool		m_bssl, m_bIsConnected, m_bBLOCK;
	bool		m_bsslEstablished, m_bEnableReadLine, m_bPauseRead;
	CS_STRING	m_shostname, m_sbuffer, m_sSockName, m_sPemFile, m_sCipherType, m_sParentName;
	CS_STRING	m_sSend, m_sPemPass, m_sLocalIP, m_sRemoteIP;
	ECloseType	m_eCloseType;

	unsigned long long	m_iMaxMilliSeconds, m_iLastSendTime, m_iBytesRead, m_iBytesWritten, m_iStartTime;
	unsigned int		m_iMaxBytes, m_iMaxStoredBufferLength, m_iTimeoutType;
	size_t				m_iLastSend;

	CSSockAddr 		m_address, m_bindhost;
	bool			m_bIsIPv6, m_bSkipConnect;
	time_t			m_iLastCheckTimeoutTime;

#ifdef HAVE_LIBSSL
	CS_STRING			m_sSSLBuffer;
	SSL 				*m_ssl;
	SSL_CTX				*m_ssl_ctx;
	unsigned int		m_iRequireClientCertFlags;

	FPCertVerifyCB		m_pCerVerifyCB;

	void FREE_SSL();
	void FREE_CTX();

#endif /* HAVE_LIBSSL */

	std::vector<CCron *>		m_vcCrons;

	//! Create the socket
	cs_sock_t CreateSocket( bool bListen = false );
	void Init( const CS_STRING & sHostname, u_short uPort, int itimeout = 60 );


	// Connection State Info
	ECONState		m_eConState;
	CS_STRING		m_sBindHost;
	u_int			m_iCurBindCount, m_iDNSTryCount;
#ifdef HAVE_C_ARES
	void FreeAres();
	ares_channel	m_pARESChannel;
	CSSockAddr		*m_pCurrAddr;
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
	CSConnection( const CS_STRING & sHostname, u_short iPort, int iTimeout = 60 )
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
	u_short GetPort() const { return( m_iPort ); }
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
	void SetPort( u_short i ) { m_iPort = i; }
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
	u_short		m_iPort;
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
	CSSSLConnection( const CS_STRING & sHostname, u_short iPort, int iTimeout = 60 ) :
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
	 */
	CSListener( u_short iPort, const CS_STRING & sBindHost = "" )
	{
		m_iPort = iPort;
		m_sBindHost = sBindHost;
		m_bIsSSL = false;
		m_iMaxConns = SOMAXCONN;
		m_iTimeout = 0;
		m_iAFrequire = CSSockAddr::RAF_ANY;
#ifdef HAVE_LIBSSL
		m_sCipher = "HIGH";
		m_iRequireCertFlags = 0;
#endif /* HAVE_LIBSSL */
	}
	virtual ~CSListener() {}

	u_short GetPort() const { return( m_iPort ); }
	const CS_STRING & GetSockName() const { return( m_sSockName ); }
	const CS_STRING & GetBindHost() const { return( m_sBindHost ); }
	bool GetIsSSL() const { return( m_bIsSSL ); }
	int GetMaxConns() const { return( m_iMaxConns ); }
	u_int GetTimeout() const { return( m_iTimeout ); }
	CSSockAddr::EAFRequire GetAFRequire() const { return( m_iAFrequire ); }
#ifdef HAVE_LIBSSL
	const CS_STRING & GetCipher() const { return( m_sCipher ); }
	const CS_STRING & GetPemLocation() const { return( m_sPemLocation ); }
	const CS_STRING & GetPemPass() const { return( m_sPemPass ); }
	unsigned int GetRequireClientCertFlags() const { return( m_iRequireCertFlags ); }
#endif /* HAVE_LIBSSL */

	//! sets the port to listen on. Set to 0 to listen on a random port
	void SetPort( u_short iPort ) { m_iPort = iPort; }
	//! sets the sock name for later reference (ie FindSockByName)
	void SetSockName( const CS_STRING & sSockName ) { m_sSockName = sSockName; }
	//! sets the host to bind to
	void SetBindHost( const CS_STRING & sBindHost ) { m_sBindHost = sBindHost; }
	//! set to true to enable SSL
	void SetIsSSL( bool b ) { m_bIsSSL = b; }
	//! set max connections as called by accept()
	void SetMaxConns( int i ) { m_iMaxConns = i; }
	//! sets the listen timeout. The listener class will close after timeout has been reached if not 0
	void SetTimeout( u_int i ) { m_iTimeout = i; }
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
	u_short		m_iPort;
	CS_STRING	m_sSockName, m_sBindHost;
	bool		m_bIsSSL;
	int			m_iMaxConns;
	u_int		m_iTimeout;
	CSSockAddr::EAFRequire	m_iAFrequire;

#ifdef HAVE_LIBSSL
	CS_STRING	m_sPemLocation, m_sPemPass, m_sCipher;
	unsigned int		m_iRequireCertFlags;
#endif /* HAVE_LIBSSL */
};

#ifdef HAVE_LIBSSL
class CSSSListener : public CSListener
{
public:
	CSSSListener( u_short iPort, const CS_STRING & sBindHost = "" ) :
		CSListener( iPort, sBindHost )
	{
		SetIsSSL( true );
	}
};
#endif /* HAVE_LIBSSL */

/**
* @class TSocketManager
* @brief Best class to use to interact with the sockets
*
* handles SSL and NON Blocking IO
* Its a template class since Csock derives need to be new'd correctly
* Makes it easier to use overall
* Rather then use it directly, you'll probably get more use deriving from it
* Another thing to note, is that all sockets are deleted implicitly, so obviously you
* cant pass in Csock classes created on the stack. For those of you who don't
* know STL very well, the reason I did this is because whenever you add to certain stl containers
* (ie vector, or map), its completely rebuilt using the copy constructor on each element.
* That then means the constructor and destructor are called on every item in the container.
* Not only is this more overhead then just moving pointers around, its dangerous as if you have
* an object that is newed and deleted in the destructor the value of its pointer is copied in the
* default copy constructor. This means everyone has to know better and create a copy constructor,
* or I just make everyone new their object :)
*
* class CBlahSock : public TSocketManager<SomeSock>
*
* @author Jim Hull <imaginos@imaginos.net>
*/

template<class T>
class TSocketManager : public std::vector<T *>
{
public:
	TSocketManager() : std::vector<T *>()
	{
		m_errno = SUCCESS;
		m_iCallTimeouts = millitime();
		m_iSelectWait = 100000; // Default of 100 milliseconds
		m_iBytesRead = 0;
		m_iBytesWritten = 0;
	}

	virtual ~TSocketManager()
	{
		Cleanup();
	}

	void clear()
	{
		while ( this->size() )
			DelSock( 0 );
	}

	virtual void Cleanup()
	{
		for( u_int a = 0; a < m_vcCrons.size(); a++ )
			CS_Delete( m_vcCrons[a] );

		m_vcCrons.clear();
		clear();
	}

	enum EMessages
	{
		SUCCESS			= 0,	//! Select returned at least 1 fd ready for action
		SELECT_ERROR	= -1,	//! An Error Happened, Probably dead socket. That socket is returned if available
		SELECT_TIMEOUT	= -2,	//! Select Timeout
		SELECT_TRYAGAIN	= -3	//! Select calls for you to try again
	};

	/**
	* Create a connection
	*
	* @param cCon the connection which should be established
	* @param pcSock the socket used for the connectiong, can be NULL
	* @return true on success
	*/
	bool Connect( const CSConnection & cCon, T * pcSock = NULL )
	{
		// create the new object
		if ( !pcSock )
			pcSock = new T( cCon.GetHostname(), cCon.GetPort(), cCon.GetTimeout() );
		else
		{
			pcSock->SetHostName( cCon.GetHostname() );
			pcSock->SetPort( cCon.GetPort() );
			pcSock->SetTimeout( cCon.GetTimeout() );
		}

		if( cCon.GetAFRequire() != CSSockAddr::RAF_ANY )
			pcSock->SetAFRequire( cCon.GetAFRequire() );

		// make it NON-Blocking IO
		pcSock->BlockIO( false );

		// bind the vhost
		pcSock->SetBindHost( cCon.GetBindHost() );

#ifdef HAVE_LIBSSL
		pcSock->SetSSL( cCon.GetIsSSL() );
		if( cCon.GetIsSSL() )
		{
			if( !cCon.GetPemLocation().empty() )
			{
				pcSock->SetPemLocation( cCon.GetPemLocation() );
				pcSock->SetPemPass( cCon.GetPemPass() );
			}
			if( !cCon.GetCipher().empty() )
				pcSock->SetCipher( cCon.GetCipher() );
		}
#endif /* HAVE_LIBSSL */

		pcSock->SetType( T::OUTBOUND );

		pcSock->SetConState( T::CST_START );
		AddSock( pcSock, cCon.GetSockName() );
		return( true );
	}

	virtual bool Listen( const CSListener & cListen, T * pcSock = NULL, u_short *piRandPort = NULL )
	{
		if ( !pcSock )
			pcSock = new T();

		pcSock->BlockIO( false );
		if( cListen.GetAFRequire() != CSSockAddr::RAF_ANY )
		{
			pcSock->SetAFRequire( cListen.GetAFRequire() );
#ifdef HAVE_IPV6
			if( cListen.GetAFRequire() == CSSockAddr::RAF_INET6 )
				pcSock->SetIPv6( true );
#endif /* HAVE_IPV6 */
		}
#ifdef HAVE_IPV6
		else
		{
				pcSock->SetIPv6( true );
		}
#endif /* HAVE_IPV6 */
#ifdef HAVE_LIBSSL
		pcSock->SetSSL( cListen.GetIsSSL() );
		if( ( cListen.GetIsSSL() ) && ( !cListen.GetPemLocation().empty() ) )
		{
			pcSock->SetPemLocation( cListen.GetPemLocation() );
			pcSock->SetPemPass( cListen.GetPemPass() );
			pcSock->SetCipher( cListen.GetCipher() );
			pcSock->SetRequireClientCertFlags( cListen.GetRequireClientCertFlags() );
		}
#endif /* HAVE_LIBSSL */

		if( piRandPort )
			*piRandPort = 0;

		if ( pcSock->Listen( cListen.GetPort(), cListen.GetMaxConns(), cListen.GetBindHost(), cListen.GetTimeout() ) )
		{
			AddSock( pcSock, cListen.GetSockName() );
			if( ( piRandPort ) && ( cListen.GetPort() == 0 ) )
			{
				cs_sock_t iSock = pcSock->GetSock();

				if ( iSock == CS_INVALID_SOCK )
				{
					CS_DEBUG( "Failed to attain a valid file descriptor" );
					pcSock->Close();
					return( false );
				}
				struct sockaddr_in mLocalAddr;
				socklen_t mLocalLen = sizeof( mLocalAddr );
				getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen );
				*piRandPort = ntohs( mLocalAddr.sin_port );
			}
			return( true );
		}

		CS_Delete( pcSock );
		return( false );
	}


	/**
	* Best place to call this class for running, all the call backs are called.
	* You should through this in your main while loop (long as its not blocking)
	* all the events are called as needed.
	*/
	virtual void Loop()
	{
		for( u_int a = 0; a < this->size(); a++ )
		{
			T *pcSock = (*this)[a];

			if ( ( pcSock->GetType() != T::OUTBOUND ) || ( pcSock->GetConState() == T::CST_OK ) )
				continue;
			if ( pcSock->GetConState() == T::CST_DNS )
			{
				if ( pcSock->DNSLookup( T::DNS_VHOST ) == ETIMEDOUT )
				{
					pcSock->SockError( EDOM );
					DelSock( a-- );
					continue;
				}
			}

			if ( pcSock->GetConState() == T::CST_BINDVHOST )
			{
				if ( !pcSock->SetupVHost() )
				{
					pcSock->SockError( GetSockError() );
					DelSock( a-- );
					continue;
				}
			}

			if ( pcSock->GetConState() == T::CST_DESTDNS )
			{
				if ( pcSock->DNSLookup( T::DNS_DEST ) == ETIMEDOUT )
				{
					pcSock->SockError( EADDRNOTAVAIL );
					DelSock( a-- );
					continue;
				}
			}
			if ( pcSock->GetConState() == T::CST_CONNECT )
			{
				if ( !pcSock->Connect( pcSock->GetBindHost(), true ) )
				{
					if ( GetSockError() == ECONNREFUSED )
						pcSock->ConnectionRefused();
					else
						pcSock->SockError( GetSockError() );

					DelSock( a-- );
					continue;
				}
			}
#ifdef HAVE_LIBSSL
			if( pcSock->GetConState() == T::CST_CONNECTSSL )
			{
				if ( pcSock->GetSSL() )
				{
					if ( !pcSock->ConnectSSL() )
					{
						if ( GetSockError() == ECONNREFUSED )
							pcSock->ConnectionRefused();
						else
							pcSock->SockError( GetSockError() == 0 ? ECONNABORTED : GetSockError() );

						DelSock( a-- );
						continue;
					}
				}
			}
#endif /* HAVE_LIBSSL */
		}

		std::map<T *, EMessages> mpeSocks;
		Select( mpeSocks );

		switch( m_errno )
		{
			case SUCCESS:
			{
				for( typename std::map<T *, EMessages>::iterator itSock = mpeSocks.begin(); itSock != mpeSocks.end(); itSock++ )
				{
					T * pcSock = itSock->first;
					EMessages iErrno = itSock->second;

					if ( iErrno == SUCCESS )
					{
						// read in data
						// if this is a
						int iLen = 0;

						if ( pcSock->GetSSL() )
							iLen = pcSock->GetPending();

						if ( iLen <= 0 )
							iLen = CS_BLOCKSIZE;

						CSCharBuffer cBuff( iLen );

						cs_ssize_t bytes = pcSock->Read( cBuff(), iLen );

						if ( bytes != T::READ_TIMEDOUT && bytes != T::READ_CONNREFUSED && bytes != T::READ_ERR && !pcSock->IsConnected() )
						{
							pcSock->SetIsConnected( true );
							pcSock->Connected();
						}

						switch( bytes )
						{
							case T::READ_EOF:
							{
								DelSockByAddr( pcSock );
								break;
							}

							case T::READ_ERR:
							{
								pcSock->SockError( GetSockError() );
								DelSockByAddr( pcSock );
								break;
							}

							case T::READ_EAGAIN:
								break;

							case T::READ_CONNREFUSED:
								pcSock->ConnectionRefused();
								DelSockByAddr( pcSock );
								break;

							case T::READ_TIMEDOUT:
								pcSock->Timeout();
								DelSockByAddr( pcSock );
								break;

							default:
							{
								if ( T::TMO_READ & pcSock->GetTimeoutType() )
									pcSock->ResetTimer();	// reset the timeout timer

								pcSock->ReadData( cBuff(), bytes );	// Call ReadData() before PushBuff() so that it is called before the ReadLine() event - LD  07/18/05
								pcSock->PushBuff( cBuff(), bytes );
								break;
							}
						}

					} else if ( iErrno == SELECT_ERROR )
					{
						// a socket came back with an error
						// usually means it was closed
						DelSockByAddr( pcSock );
					}
				}
				break;
			}

			case SELECT_TIMEOUT:
			case SELECT_TRYAGAIN:
			case SELECT_ERROR:
			default	:
				break;
		}

		unsigned long long iMilliNow = millitime();
		if ( ( iMilliNow - m_iCallTimeouts ) >= 1000 )
		{
			m_iCallTimeouts = iMilliNow;
			// call timeout on all the sockets that recieved no data
			for( unsigned int i = 0; i < this->size(); i++ )
			{
				if ( (*this)[i]->GetConState() != T::CST_OK )
					continue;

				if ( (*this)[i]->CheckTimeout( iMilliNow / 1000 ) )
					DelSock( i-- );
			}
		}
		// run any Manager Crons we may have
		Cron();
	}

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
	void DynamicSelectLoop( u_long iLowerBounds, u_long iUpperBounds, time_t iMaxResolution = 3600 )
	{
		SetSelectTimeout( iLowerBounds );
		if( m_errno == SELECT_TIMEOUT )
		{ // only do this if the previous call to select was a timeout
			time_t iNow = time( NULL );
			u_long iSelectTimeout = GetDynamicSleepTime( iNow, iMaxResolution );
			iSelectTimeout *= 1000000;
			iSelectTimeout = std::max( iLowerBounds, iSelectTimeout );
			iSelectTimeout = std::min( iSelectTimeout, iUpperBounds );
			if( iLowerBounds != iSelectTimeout )
				SetSelectTimeout( iSelectTimeout );
		}
		Loop();
	}

	/**
	* Make this method virtual, so you can override it when a socket is added.
	* Assuming you might want to do some extra stuff
	*/
	virtual void AddSock( T *pcSock, const CS_STRING & sSockName )
	{
		pcSock->SetSockName( sSockName );
		this->push_back( pcSock );
	}

	//! returns a pointer to the FIRST sock found by port or NULL on no match
	virtual T * FindSockByRemotePort( u_short iPort )
	{
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			if ( (*this)[i]->GetRemotePort() == iPort )
				return( (*this)[i] );
		}

		return( NULL );
	}

	//! returns a pointer to the FIRST sock found by port or NULL on no match
	virtual T * FindSockByLocalPort( u_short iPort )
	{
		for( unsigned int i = 0; i < this->size(); i++ )
			if ( (*this)[i]->GetLocalPort() == iPort )
				return( (*this)[i] );

		return( NULL );
	}

	//! returns a pointer to the FIRST sock found by name or NULL on no match
	virtual T * FindSockByName( const CS_STRING & sName )
	{
		typename std::vector<T *>::iterator it;
		typename std::vector<T *>::iterator it_end = this->end();
		for( it = this->begin(); it != it_end; it++ )
			if ( (*it)->GetSockName() == sName )
				return( *it );

		return( NULL );
	}

	//! returns a pointer to the FIRST sock found by filedescriptor or NULL on no match
	virtual T * FindSockByFD( cs_sock_t iFD )
	{
		for( unsigned int i = 0; i < this->size(); i++ )
			if ( ( (*this)[i]->GetRSock() == iFD ) || ( (*this)[i]->GetWSock() == iFD ) )
				return( (*this)[i] );

		return( NULL );
	}

	virtual std::vector<T *> FindSocksByName( const CS_STRING & sName )
	{
		std::vector<T *> vpSocks;

		for( unsigned int i = 0; i < this->size(); i++ )
			if ( (*this)[i]->GetSockName() == sName )
				vpSocks.push_back( (*this)[i] );

		return( vpSocks );
	}

	//! returns a vector of pointers to socks with sHostname as being connected
	virtual std::vector<T *> FindSocksByRemoteHost( const CS_STRING & sHostname )
	{
		std::vector<T *> vpSocks;

		for( unsigned int i = 0; i < this->size(); i++ )
			if ( (*this)[i]->GetHostName() == sHostname )
				vpSocks.push_back( (*this)[i] );

		return( vpSocks );
	}

	//! return the last known error as set by this class
	int GetErrno() { return( m_errno ); }

	//! add a cronjob at the manager level
	virtual void AddCron( CCron *pcCron )
	{
		m_vcCrons.push_back( pcCron );
	}

	/**
	 * @brief deletes a cron by name
	 * @param sName the name of the cron
	 * @param bDeleteAll delete all crons that match sName
	 * @param bCaseSensitive use strcmp or strcasecmp
	 */
	virtual void DelCron( const CS_STRING & sName, bool bDeleteAll = true, bool bCaseSensitive = true )
	{
		for( u_int a = 0; a < m_vcCrons.size(); a++ )
		{
			int (*Cmp)(const char *, const char *) = ( bCaseSensitive ? strcmp : strcasecmp );
			if ( Cmp( m_vcCrons[a]->GetName().c_str(), sName.c_str() ) == 0 )
			{
				m_vcCrons[a]->Stop();
				CS_Delete( m_vcCrons[a] );
				m_vcCrons.erase( m_vcCrons.begin() + a-- );
				if( !bDeleteAll )
					break;
			}
		}
	}

	//! delete cron by idx
	virtual void DelCron( u_int iPos )
	{
		if ( iPos < m_vcCrons.size() )
		{
			m_vcCrons[iPos]->Stop();
			CS_Delete( m_vcCrons[iPos] );
			m_vcCrons.erase( m_vcCrons.begin() + iPos );
		}
	}
	//! delete cron by address
	virtual void DelCronByAddr( CCron *pcCron )
	{
		for( u_int a = 0; a < m_vcCrons.size(); a++ )
		{
			if ( m_vcCrons[a] == pcCron )
			{
				m_vcCrons[a]->Stop();
				CS_Delete( m_vcCrons[a] );
				m_vcCrons.erase( m_vcCrons.begin() + a );
				return;
			}
		}
	}

	//! Get the Select Timeout in MICROSECONDS ( 1000 == 1 millisecond )
	u_long GetSelectTimeout() { return( m_iSelectWait ); }
	//! Set the Select Timeout in MICROSECONDS ( 1000 == 1 millisecond )
	//! Setting this to 0 will cause no timeout to happen, Select() will return instantly
	void  SetSelectTimeout( u_long iTimeout ) { m_iSelectWait = iTimeout; }

	std::vector<CCron *> & GetCrons() { return( m_vcCrons ); }

	//! Delete a sock by addr
	//! its position is looked up
	//! the socket is deleted, the appropriate call backs are peformed
	//! and its instance is removed from the manager
	virtual void DelSockByAddr( T *pcSock )
	{
		for( u_int a = 0; a < this->size(); a++ )
		{
			if ( pcSock == (*this)[a] )
			{
				DelSock( a );
				return;
			}
		}
	}
	//! Delete a sock by position in the vector
	//! the socket is deleted, the appropriate call backs are peformed
	//! and its instance is removed from the manager
	//! deleting in a loop can be tricky, be sure you watch your position.
	//! ie for( u_int a = 0; a < size(); a++ ) DelSock( a-- );
	virtual void DelSock( u_int iPos )
	{
		if ( iPos >= this->size() )
		{
			CS_DEBUG( "Invalid Sock Position Requested! [" << iPos << "]" );
			return;
		}

		T * pSock = (*this)[iPos];

		if( pSock->GetCloseType() != T::CLT_DEREFERENCE )
		{
			if ( pSock->IsConnected() )
				pSock->Disconnected(); // only call disconnected event if connected event was called (IE IsConnected was set)

			m_iBytesRead += pSock->GetBytesRead();
			m_iBytesWritten += pSock->GetBytesWritten();
		}

		CS_Delete( pSock );
		this->erase( this->begin() + iPos );
	}

	/**
	 * @brief swaps out a sock with a copy of the original sock
	 * @param pNewSock the new sock to change out with. (this should be constructed by you with the default ctor)
	 * @param iOrginalSockIdx the position in this sockmanager of the original sock
	 * @return true on success
	 */
	virtual bool SwapSockByIdx( Csock *pNewSock, u_long iOrginalSockIdx )
	{
		if( iOrginalSockIdx >= this->size() )
		{
			CS_DEBUG( "Invalid Sock Position Requested! [" << iOrginalSockIdx << "]" );
			return( false );
		}

		Csock *pSock = (*this)[iOrginalSockIdx];
		pNewSock->Copy( *pSock );
		pSock->Dereference();
		(*this)[iOrginalSockIdx] = (T *)pNewSock;
		this->push_back( (T *)pSock ); // this allows it to get cleaned up
		return( true );
	}

	/**
	 * @brief swaps out a sock with a copy of the original sock
	 * @param pNewSock the new sock to change out with. (this should be constructed by you with the default ctor)
	 * @param pOrigSock the address of the original socket
	 * @return true on success
	 */
	virtual bool SwapSockByAddr( Csock *pNewSock, Csock *pOrigSock )
	{
		for( u_long a = 0; a < this->size(); a++ )
		{
			if( (*this)[a] == pOrigSock )
				return( SwapSockByIdx( pNewSock, a ) );
		}
		return( false );
	}

	//! Get the bytes read from all sockets current and past
	unsigned long long GetBytesRead() const
	{
		// Start with the total bytes read from destroyed sockets
		unsigned long long iRet = m_iBytesRead;

		// Add in the outstanding bytes read from active sockets
		for( u_int a = 0; a < this->size(); a++ )
			iRet += (*this)[a]->GetBytesRead();

		return( iRet );
	}

	//! Get the bytes written to all sockets current and past
	unsigned long long GetBytesWritten() const
	{
		// Start with the total bytes written to destroyed sockets
		unsigned long long iRet = m_iBytesWritten;

		// Add in the outstanding bytes written to active sockets
		for( u_int a = 0; a < this->size(); a++ )
			iRet += (*this)[a]->GetBytesWritten();

		return( iRet );
	}

protected:
	//! this is a strict wrapper around C-api select(). Added in the event you need to do special work here
	enum ECheckType
	{
		eCheckRead = 1,
		eCheckWrite = 2
	};

	void FDSetCheck( int iFd, std::map< int, short > & miiReadyFds, ECheckType eType )
	{
		std::map< int, short >::iterator it = miiReadyFds.find( iFd );
		if( it != miiReadyFds.end() )
			it->second |= eType;
		else
			miiReadyFds[iFd] = eType;
	}
	bool FDHasCheck( int iFd, std::map< int, short > & miiReadyFds, ECheckType eType )
	{
		std::map< int, short >::iterator it = miiReadyFds.find( iFd );
		if( it != miiReadyFds.end() )
			return( (it->second & eType) );
		return( false );
	}
	virtual int Select( std::map< int, short > & miiReadyFds, struct timeval *tvtimeout)
	{
#ifdef CSOCK_USE_POLL
		if( miiReadyFds.empty() )
			return( select( 0, NULL, NULL, NULL, tvtimeout ) );

		struct pollfd * pFDs = (struct pollfd *)malloc( sizeof( struct pollfd ) * miiReadyFds.size() );
		size_t uCurrPoll = 0;
		for( std::map< int, short >::iterator it = miiReadyFds.begin(); it != miiReadyFds.end(); ++it, ++uCurrPoll )
		{
			short iEvents = 0;
			if( it->second & eCheckRead )
				iEvents |= POLLIN;
			if( it->second & eCheckWrite )
				iEvents |= POLLOUT;
			pFDs[uCurrPoll].fd = it->first;
			pFDs[uCurrPoll].events = iEvents;
			pFDs[uCurrPoll].revents = 0;
		}
		int iTimeout = (int)(tvtimeout->tv_usec / 1000);
		iTimeout += (int)(tvtimeout->tv_sec * 1000);
		size_t uMaxFD = miiReadyFds.size();
		int iRet = poll( pFDs, uMaxFD, iTimeout );
		miiReadyFds.clear();
		for( uCurrPoll = 0; uCurrPoll < uMaxFD; ++uCurrPoll )
		{
			short iEvents = 0;
			if( (pFDs[uCurrPoll].revents & (POLLIN|POLLERR|POLLHUP|POLLNVAL) ) )
				iEvents |= eCheckRead;
			if( (pFDs[uCurrPoll].revents & POLLOUT ) )
				iEvents |= eCheckWrite;
			std::map< int, short >::iterator it = miiReadyFds.find( pFDs[uCurrPoll].fd );
			if( it != miiReadyFds.end() )
				it->second |= iEvents;
			else
				miiReadyFds[pFDs[uCurrPoll].fd] = iEvents;
		}
		free( pFDs );
#else
		fd_set rfds, wfds;
		TFD_ZERO( &rfds );
		TFD_ZERO( &wfds );
		bool bHasWrite = false;
		int iHighestFD = 0;
		for( std::map< int, short >::iterator it = miiReadyFds.begin(); it != miiReadyFds.end(); ++it )
		{
			iHighestFD = std::max( it->first, iHighestFD );
			if( it->second & eCheckRead )
			{
				TFD_SET( it->first, &rfds );
			}
			if( it->second & eCheckWrite )
			{
				bHasWrite = true;
				TFD_SET( it->first, &wfds );
			}
		}

		int iRet = select( iHighestFD + 1, &rfds, ( bHasWrite ? &wfds : NULL ), NULL, tvtimeout );
		if( iRet <= 0 )
			miiReadyFds.clear();
		else
		{
			for( std::map< int, short >::iterator it = miiReadyFds.begin(); it != miiReadyFds.end(); ++it )
			{
				if( (it->second & eCheckRead) && !TFD_ISSET( it->first, &rfds ) )
					it->second &= ~eCheckRead;
				if( (it->second & eCheckWrite) && !TFD_ISSET( it->first, &wfds ) )
					it->second &= ~eCheckWrite;
			}
		}
#endif /* CSOCK_USE_POLL */

		return( iRet );
	}
private:
	/**
	* fills a map of socks to a message for check
	* map is empty if none are ready, check GetErrno() for the error, if not SUCCESS Select() failed
	* each struct contains the socks error
	* @see GetErrno()
	*/
	virtual void Select( std::map<T *, EMessages> & mpeSocks )
	{
		mpeSocks.clear();
		struct timeval tv;

		std::map< int, short > miiReadyFds;
		tv.tv_sec = m_iSelectWait / 1000000;
		tv.tv_usec = m_iSelectWait % 1000000;
		u_int iQuickReset = 1000;
		if ( m_iSelectWait == 0 )
			iQuickReset = 0;

		bool bHasAvailSocks = false;
		unsigned long long iNOW = 0;
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			T *pcSock = (*this)[i];

			Csock::ECloseType eCloseType = pcSock->GetCloseType();

			if( eCloseType == T::CLT_NOW || eCloseType == T::CLT_DEREFERENCE || ( eCloseType == T::CLT_AFTERWRITE && pcSock->GetWriteBuffer().empty() ) )
			{
				DelSock( i-- ); // close any socks that have requested it
				continue;
			}
			else
				pcSock->Cron(); // call the Cron handler here

			cs_sock_t & iRSock = pcSock->GetRSock();
			cs_sock_t & iWSock = pcSock->GetWSock();
#ifndef CSOCK_USE_POLL
			if( iRSock > FD_SETSIZE || iWSock > FD_SETSIZE )
			{
				CS_DEBUG( "FD is larger than select() can handle" );
				DelSock( i-- );
				continue;
			}
#endif /* CSOCK_USE_POLL */
		
#ifdef HAVE_C_ARES
			ares_channel pChannel = pcSock->GetAresChannel();
			if( pChannel )
			{
				ares_socket_t aiAresSocks[1];
				aiAresSocks[0] = ARES_SOCKET_BAD;
				int iSockMask = ares_getsock( pChannel, aiAresSocks, 1 );
				if( ARES_GETSOCK_READABLE( iSockMask, 0 ) )
					FDSetCheck( aiAresSocks[0], miiReadyFds, eCheckRead );
				if( ARES_GETSOCK_WRITABLE( iSockMask, 0 ) )
					FDSetCheck( aiAresSocks[0], miiReadyFds, eCheckWrite );
				// let ares drop the timeout if it has something timing out sooner then whats in tv currently
				ares_timeout( pChannel, &tv, &tv );
			}
#endif /* HAVE_C_ARES */

			if ( pcSock->GetConState() != T::CST_OK )
				continue;

			bHasAvailSocks = true;

			bool bIsReadPaused = pcSock->IsReadPaused();
			if ( bIsReadPaused )
			{
				pcSock->ReadPaused();
				bIsReadPaused = pcSock->IsReadPaused(); // re-read it again, incase it changed status)
			}
			if ( iRSock == CS_INVALID_SOCK || iWSock == CS_INVALID_SOCK )
			{
				SelectSock( mpeSocks, SUCCESS, pcSock );
				continue;	// invalid sock fd
			}

			if( pcSock->GetType() != T::LISTENER )
			{
				bool bHasWriteBuffer = !pcSock->GetWriteBuffer().empty();

				if ( !bIsReadPaused )
					FDSetCheck( iRSock, miiReadyFds, eCheckRead );

				if( pcSock->AllowWrite( iNOW ) && ( !pcSock->IsConnected() || bHasWriteBuffer ) )
				{ 
					if( !pcSock->IsConnected() )
					{ // set the write bit if not connected yet
						FDSetCheck( iWSock, miiReadyFds, eCheckWrite );
					}
					else if( bHasWriteBuffer && !pcSock->GetSSL() )
					{ // always set the write bit if there is data to send when NOT ssl
						FDSetCheck( iWSock, miiReadyFds, eCheckWrite );
					}
					else if( bHasWriteBuffer && pcSock->GetSSL() && pcSock->SslIsEstablished() )
					{ // ONLY set the write bit if there is data to send and the SSL handshake is finished
						FDSetCheck( iWSock, miiReadyFds, eCheckWrite );
					}
				}

				if( pcSock->GetSSL() && !pcSock->SslIsEstablished() && bHasWriteBuffer )
				{ // if this is an unestabled SSL session with data to send ... try sending it
					// do this here, cause otherwise ssl will cause a small
					// cpu spike waiting for the handshake to finish
					// resend this data
					if ( !pcSock->Write( "" ) )
					{
						pcSock->Close();
					}
					// warning ... setting write bit in here causes massive CPU spinning on invalid SSL servers
					// http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=631590
					// however, we can set the select WAY down and it will retry quickly, but keep it from spinning at 100%
					tv.tv_usec = iQuickReset;
					tv.tv_sec = 0;
				} 
			} 
			else
			{
				FDSetCheck( iRSock, miiReadyFds, eCheckRead );
			}
			
			if( pcSock->GetSSL() && pcSock->GetType() != Csock::LISTENER )
			{
				if ( ( pcSock->GetPending() > 0 ) && ( !pcSock->IsReadPaused() ) )
					SelectSock( mpeSocks, SUCCESS, pcSock );
			}
		}

		// old fashion select, go fer it
		int iSel;

		if( !mpeSocks.empty() ) // .1 ms pause to see if anything else is ready (IE if there is SSL data pending, don't wait too long)
		{
			tv.tv_usec = iQuickReset;
			tv.tv_sec = 0;
		}
		else if ( !this->empty() && !bHasAvailSocks )
		{
			tv.tv_usec = iQuickReset;
			tv.tv_sec = 0;
		}

		iSel = Select( miiReadyFds, &tv );
		if ( iSel == 0 )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_TIMEOUT;
			else
				m_errno = SUCCESS;
#ifdef HAVE_C_ARES
			// run through ares channels and process timeouts
			for( u_long uSock = 0; uSock < this->size(); ++uSock )
			{
				T *pcSock = this->at( uSock );
				ares_channel pChannel = pcSock->GetAresChannel();
				if( pChannel )
					ares_process_fd( pChannel, ARES_SOCKET_BAD, ARES_SOCKET_BAD );
			}
#endif /* HAVE_C_ARES */

			return;
		}

		if ( ( iSel == -1 ) && ( errno == EINTR ) )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_TRYAGAIN;
			else
				m_errno = SUCCESS;

			return;
		} 
		else if ( iSel == -1 )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_ERROR;
			else
				m_errno = SUCCESS;

			return;
		} 
		else
		{
			m_errno = SUCCESS;
		}

		// find out wich one is ready
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			T *pcSock = (*this)[i];

#ifdef HAVE_C_ARES
			ares_channel pChannel = pcSock->GetAresChannel();
			if( pChannel )
			{
				ares_socket_t aiAresSocks[1];
				aiAresSocks[0] = ARES_SOCKET_BAD;
				ares_getsock( pChannel, aiAresSocks, 1 );
				if( FDHasCheck( aiAresSocks[0], miiReadyFds, eCheckRead ) || FDHasCheck( aiAresSocks[0], miiReadyFds, eCheckWrite ) )
					ares_process_fd( pChannel, aiAresSocks[0], aiAresSocks[0] );
			}
#endif /* HAVE_C_ARES */

			if ( pcSock->GetConState() != T::CST_OK )
				continue;

			cs_sock_t & iRSock = pcSock->GetRSock();
			cs_sock_t & iWSock = pcSock->GetWSock();
			EMessages iErrno = SUCCESS;

			if ( iRSock == CS_INVALID_SOCK || iWSock == CS_INVALID_SOCK )
			{
				// trigger a success so it goes through the normal motions
				// and an error is produced
				SelectSock( mpeSocks, SUCCESS, pcSock );
				continue; // watch for invalid socks
			}

			if ( FDHasCheck( iWSock, miiReadyFds, eCheckWrite ) )
			{
				if ( iSel > 0 )
				{
					iErrno = SUCCESS;
					if ( ( !pcSock->GetWriteBuffer().empty() ) && ( pcSock->IsConnected() ) )
					{ // write whats in the socks send buffer
						if ( !pcSock->Write( "" ) )
						{
							// write failed, sock died :(
							iErrno = SELECT_ERROR;
						}
					}
				} else
					iErrno = SELECT_ERROR;

				SelectSock( mpeSocks, iErrno, pcSock );

			} 
			else if ( FDHasCheck( iRSock, miiReadyFds, eCheckRead ) )
			{
				if ( iSel > 0 )
					iErrno = SUCCESS;
				else
					iErrno = SELECT_ERROR;

				if ( pcSock->GetType() != T::LISTENER )
					SelectSock( mpeSocks, iErrno, pcSock );
				else // someone is coming in!
				{
					CS_STRING sHost;
					u_short port;
					cs_sock_t inSock = pcSock->Accept( sHost, port );

					if ( inSock != CS_INVALID_SOCK )
					{
						if ( T::TMO_ACCEPT & pcSock->GetTimeoutType() )
							pcSock->ResetTimer();	// let them now it got dinged

						// if we have a new sock, then add it
						T *NewpcSock = (T *)pcSock->GetSockObj( sHost, port );

						if ( !NewpcSock )
							NewpcSock = new T( sHost, port );

						NewpcSock->BlockIO( false );
						NewpcSock->SetType( T::INBOUND );
						NewpcSock->SetRSock( inSock );
						NewpcSock->SetWSock( inSock );
						NewpcSock->SetIPv6( pcSock->GetIPv6() );

						bool bAddSock = true;
#ifdef HAVE_LIBSSL
						//
						// is this ssl ?
						if ( pcSock->GetSSL() )
						{
							NewpcSock->SetCipher( pcSock->GetCipher() );
							NewpcSock->SetPemLocation( pcSock->GetPemLocation() );
							NewpcSock->SetPemPass( pcSock->GetPemPass() );
							NewpcSock->SetRequireClientCertFlags( pcSock->GetRequireClientCertFlags() );
							bAddSock = NewpcSock->AcceptSSL();
						}

#endif /* HAVE_LIBSSL */
						if ( bAddSock )
						{
							// set the name of the listener
							NewpcSock->SetParentSockName( pcSock->GetSockName() );
							NewpcSock->SetRate( pcSock->GetRateBytes(), pcSock->GetRateTime() );
							if ( NewpcSock->GetSockName().empty() )
							{
								std::stringstream s;
								s << sHost << ":" << port;
								AddSock( NewpcSock,  s.str() );

							} else
								AddSock( NewpcSock, NewpcSock->GetSockName() );
						} else
							CS_Delete( NewpcSock );
					}
#ifdef _WIN32
					else if( GetSockError() != WSAEWOULDBLOCK )
#else /* _WIN32 */
					else if( GetSockError() != EAGAIN )
#endif /* _WIN32 */
					{
						pcSock->SockError( GetSockError() );
					}
				}
			}
		}
	}

	time_t GetDynamicSleepTime( time_t iNow, time_t iMaxResolution = 3600 ) const
	{
		time_t iNextRunTime = iNow + iMaxResolution;
		typename std::vector<T *>::const_iterator it;
		// This is safe, because we don't modify the vector.
		typename std::vector<T *>::const_iterator it_end = this->end();

		for (it = this->begin(); it != it_end; it++)
		{
			T* pSock = *it;

			if( pSock->GetConState() != T::CST_OK )
				iNextRunTime = iNow; // this is in a nebulous state, need to let it proceed like normal

			time_t iTimeoutInSeconds = pSock->GetTimeout();
			if( iTimeoutInSeconds > 0 )
			{
				time_t iNextTimeout = pSock->GetNextCheckTimeout( iNow );
				iNextRunTime = std::min( iNextRunTime, iNextTimeout );
			}

			const std::vector<CCron *> & vCrons = pSock->GetCrons();
			std::vector<CCron *>::const_iterator cit;
			std::vector<CCron *>::const_iterator cit_end = vCrons.end();
			for (cit = vCrons.begin(); cit != cit_end; cit++)
				iNextRunTime = std::min( iNextRunTime, (*cit)->GetNextRun() );
		}
		std::vector<CCron *>::const_iterator cit;
		std::vector<CCron *>::const_iterator cit_end = m_vcCrons.end();
		for (cit = m_vcCrons.begin(); cit != cit_end; cit++)
			iNextRunTime = std::min( iNextRunTime, (*cit)->GetNextRun() );

		if( iNextRunTime < iNow )
			return( 0 ); // smallest unit possible
		return( std::min( iNextRunTime - iNow, iMaxResolution ) );
	}

	//! internal use only
	virtual void SelectSock( std::map<T *, EMessages> & mpeSocks, EMessages eErrno, T * pcSock )
	{
		if ( mpeSocks.find( pcSock ) != mpeSocks.end() )
			return;

		mpeSocks[pcSock] = eErrno;
	}

	//! these crons get ran and checked in Loop()
	virtual void Cron()
	{
		time_t iNow = 0;
		for( unsigned int a = 0; a < m_vcCrons.size(); a++ )
		{
			CCron *pcCron = m_vcCrons[a];

			if ( !pcCron->isValid() )
			{
				CS_Delete( pcCron );
				m_vcCrons.erase( m_vcCrons.begin() + a-- );
			} else
				pcCron->run( iNow );
		}
	}

	////////
	// Connection State Functions

	///////////
	// members
	EMessages				m_errno;
	std::vector<CCron *>	m_vcCrons;
	unsigned long long		m_iCallTimeouts;
	unsigned long long		m_iBytesRead;
	unsigned long long		m_iBytesWritten;
	u_long					m_iSelectWait;
};

//! basic socket class
typedef TSocketManager<Csock> CSocketManager;

#ifndef _NO_CSOCKET_NS
}
#endif /* _NO_CSOCKET_NS */

#endif /* _HAS_CSOCKET_ */

