/**
*
*    Copyright (c) 1999-2005 Jim Hull <imaginos@imaginos.net>
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
* complete source code for the DB software and any accompanying software that uses the DB software.
* The source code must either be included in the distribution or be available for no more than
* the cost of distribution plus a nominal fee, and must be freely redistributable
* under reasonable conditions. For an executable file, complete source code means the source
* code for all modules it contains. It does not include source code for modules or files
* that typically accompany the major components of the operating system on which the executable file runs.
*
* THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
* BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
* OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT SHALL SLEEPYCAT SOFTWARE BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
* $Revision$
*/

// note to compile with win32 need to link to winsock2, using gcc its -lws2_32

#ifndef _HAS_CSOCKET_
#define _HAS_CSOCKET_
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

#endif /* _WIN32 */

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
#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <map>

#include "main.h" // require this as a general rule, most projects have a defines.h or the like

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

#ifdef __DEBUG__
#	define PERROR( f ) __Perror( f )
#else
#	define PERROR( f )	(void)0
#endif /* __DEBUG__ */

#ifndef _NO_CSOCKET_NS // some people may not want to use a namespace
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */

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
		RAF_INET	= AF_INET,
#ifdef HAVE_IPV6
		RAF_INET6	= AF_INET6
#endif /* HAVE_IPV6 */
	};

	void SinFamily()
	{
#ifdef HAVE_IPV6
		if( m_bIsIPv6 )
		{
			m_saddr6.sin6_family = PF_INET6;
			return;
		}
#endif /* HAVE_IPV6 */
		m_saddr.sin_family = PF_INET;
	}
	void SinPort( u_short iPort )
	{
#ifdef HAVE_IPV6
		if( m_bIsIPv6 )
		{
			m_saddr6.sin6_port = htons( iPort );
			return;
		}
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
int GetAddrInfo( const CS_STRING & sHostname, Csock *pSock, CSSockAddr & csSockAddr );

#if defined( _REENTRANT ) && defined( _USE_THREADED_DNS )
#define ___DO_THREADS
#include <pthread.h>

#ifndef PTHREAD_MUTEX_FAST_NP 
#define PTHREAD_MUTEX_FAST_NP PTHREAD_MUTEX_NORMAL
#endif /* PTHREAD_MUTEX_FAST_NP */

class CSMutex
{
public:
	CSMutex ();
	virtual ~CSMutex();
	int lock() { return( pthread_mutex_lock( &m_mutex ) ); }
	int unlock() { return( pthread_mutex_unlock( &m_mutex ) ); }
	int trylock() { return( pthread_mutex_trylock( &m_mutex ) ); }
private:
	pthread_mutex_t			m_mutex;
	pthread_mutexattr_t		m_mattrib;
	
};

class CSThread
{
public:
	CSThread() { m_eStatus = WAITING; }
	virtual ~CSThread() {}
	
	enum EStatus
	{
		WAITING = 1,
		RUNNING = 2,
		FINISHED = 3
	};

	bool start();
	void wait();
	virtual void run() = 0;
	static void *start_thread( void *args );
	EStatus Status() { return( m_eStatus ); }
	void SetStatus( EStatus e ) { m_eStatus = e; }
	int lock() { return( m_mutex.lock() ); }
	int unlock() { return( m_mutex.unlock() ); }
	int cancel() { return( pthread_cancel( m_ppth ) ); }

private:
	pthread_t	m_ppth;
	EStatus m_eStatus;
	CSMutex  m_mutex;
};

class CDNSResolver : public CSThread
{
public:
	CDNSResolver() : CSThread() { m_bSuccess = false; }
	virtual ~CDNSResolver() {}
	//! returns imediatly, from here out check if IsCompleted() returns true before looking at ANY of the data
	void Lookup( const CS_STRING & sHostname );

	virtual void run();

	//! true if dns entry was successfuly found
	bool Suceeded() const { return( m_bSuccess ); }

	//! true if task is finished, this function is thread safe
	bool IsCompleted();

	CSSockAddr * GetSockAddr() { return( &m_cSockAddr ); }

private:
	bool		m_bSuccess;
	CS_STRING	m_sHostname;
	CSSockAddr	m_cSockAddr;
};


#endif /* ___DO_THREADS */

const u_int CS_BLOCKSIZE = 4096;
template <class T> inline void CS_Delete( T * & p ) { if( p ) { delete p; p = NULL; } }

#ifdef HAVE_LIBSSL
enum ECompType
{
	CT_NONE	= 0,
	CT_ZLIB	= 1,
	CT_RLE	= 2
};

/**
 * @brief You HAVE to call this in order to use the SSL library
 * @return true on success
 */

bool InitSSL( ECompType eCompressionType = CT_NONE );
void SSLErrors( const char *filename, u_int iLineNum );
#endif /* HAVE_LIBSSL */

// TODO need to make this sock specific via getsockopt
inline int GetSockError()
{
#ifdef _WIN32
	return( WSAGetLastError() );
#else
	return( errno );
#endif /* _WIN32 */
}

#ifdef _WIN32
inline bool InitWin32()
{
	WSADATA wsaData;
	int iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if( iResult != NO_ERROR )
		return( false );

	return( true );
}
inline void ShutdownWin32()
{
	WSACleanup();
}
#endif /* _WIN32 */

// wrappers for FD_SET and such to work in templates
inline void TFD_ZERO( fd_set *set )
{
	FD_ZERO( set );
}

inline void TFD_SET( u_int iSock, fd_set *set )
{
	FD_SET( iSock, set );
}

inline bool TFD_ISSET( u_int iSock, fd_set *set )
{
	if ( FD_ISSET( iSock, set ) )
		return( true );

	return( false );
}

inline void TFD_CLR( u_int iSock, fd_set *set )
{
	FD_CLR( iSock, set );
}

void __Perror( const CS_STRING & s );
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
	void run();

	/**
	 * @TimeSequence	how often to run in seconds
	 * @iMaxCycles		how many times to run, 0 makes it run forever
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

public:

	//! this is the method you should override
	virtual void RunJob();

private:
	time_t		m_iTime;
	bool		m_bActive, m_bPause;
	int			m_iTimeSequence;
	u_int		m_iMaxCycles, m_iCycles;
	CS_STRING	m_sName;
};

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
	* @param iport the port you are connectint to
	* @param itimeout how long to wait before ditching the connection, default is 60 seconds
	*/
	Csock( const CS_STRING & sHostname, u_short iport, int itimeout = 60 );

	// override this for accept sockets
	virtual Csock *GetSockObj( const CS_STRING & sHostname, u_short iPort );

	virtual ~Csock();

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
		SSL3				= 3
	};

	enum ECONState
	{
		CST_START		= 0,
		CST_DNS			= CST_START,
		CST_VHOSTDNS	= 1,
		CST_BINDVHOST	= 2,
		CST_CONNECT		= 3,
		CST_OK			= 4
	};

	enum ECloseType
	{
		CLT_DONT			= 0, //! don't close DER
		CLT_NOW				= 1, //! close immediatly
		CLT_AFTERWRITE		= 2  //! close after finishing writing the buffer
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
	*/
	virtual bool Listen( u_short iPort, int iMaxConns = SOMAXCONN, const CS_STRING & sBindHost = "", u_int iTimeout = 0 );

	//! Accept an inbound connection, this is used internally
	virtual int Accept( CS_STRING & sHost, u_short & iRPort );

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
	virtual bool Write( const char *data, int len );

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
	virtual int Read( char *data, int len );
	CS_STRING GetLocalIP();
	CS_STRING GetRemoteIP();

	//! Tells you if the socket is connected
	virtual bool IsConnected();
	//! Sets the sock, telling it its connected (internal use only)
	virtual void SetIsConnected( bool b );

	//! returns a reference to the sock
	int & GetRSock();
	void SetRSock( int iSock );
	int & GetWSock();
	void SetWSock( int iSock );

	void SetSock( int iSock );
	int & GetSock();

	//! resets the time counter
	void ResetTimer();

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
	virtual bool CheckTimeout();

	/**
	* pushes data up on the buffer, if a line is ready
	* it calls the ReadLine event
	*/
	virtual void PushBuff( const char *data, int len );

	//! This gives access to the internal buffer, if your
	//! not going to use GetLine(), then you may want to clear this out
	//! (if its binary data and not many '\n'
	CS_STRING & GetInternalBuffer();
	//! sets the max buffered threshold when enablereadline() is enabled
	void SetMaxBufferThreshold( u_int iThreshold );
	u_int GetMaxBufferThreshold();

	//! Returns the connection type from enum eConnType
	int GetType();
	void SetType( int iType );

	//! Returns a reference to the socket name
	const CS_STRING & GetSockName();
	void SetSockName( const CS_STRING & sName );

	//! Returns a reference to the host name
	const CS_STRING & GetHostName();
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
	void SetFullSSLAccept();
	SSL_SESSION * GetSSLSession();
#endif /* HAVE_LIBSSL */

	//! Get the send buffer
	const CS_STRING & GetWriteBuffer();
	void ClearWriteBuffer();

	//! is SSL_accept finished ?
	bool FullSSLAccept();
	//! is the ssl properly finished (from write no error)
	bool SslIsEstablished();

	//! Use this to bind this socket to inetd
	bool ConnectInetd( bool bIsSSL = false, const CS_STRING & sHostname = "" );

	//! Tie this guy to an existing real file descriptor
	bool ConnectFD( int iReadFD, int iWriteFD, const CS_STRING & sName, bool bIsSSL = false, ETConn eDirection = INBOUND );

	//! Get the peer's X509 cert
#ifdef HAVE_LIBSSL
	X509 *getX509();

	//!
	//! Returns The Peers Public Key
	//!
	CS_STRING GetPeerPubKey();
	bool RequiresClientCert();
	void SetRequiresClientCert( bool bRequiresCert );

#endif /* HAVE_LIBSSL */

	//! Set The INBOUND Parent sockname
	virtual void SetParentSockName( const CS_STRING & sParentName );
	const CS_STRING & GetParentSockName();

	/*
	* sets the rate at which we can send data
	* \param iBytes the amount of bytes we can write
	* \param iMilliseconds the amount of time we have to rate to iBytes
	*/
	virtual void SetRate( u_int iBytes, unsigned long long iMilliseconds );

	u_int GetRateBytes();
	unsigned long long GetRateTime();
	//! This has a garbage collecter, and is used internall to call the jobs
	virtual void Cron();

	//! insert a newly created cron
	virtual void AddCron( CCron * pcCron );
	//! delete cron(s) by name
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
	virtual void ReadData( const char *data, int len ) {}
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
	 * and triggers this event. Either Override it and do nothing, or @SetMaxBufferThreshold( int )
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
	 * This gets called every iteration of Select() if the socket is ReadPaused
	 */
	virtual void ReadPaused() {}

	//! return the duration since last write
	int GetLastWriteDuration() { return m_iTcount; }

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
		if( m_iReadSock != -1 )
			return( true );
			
		m_iReadSock = m_iWriteSock = SOCKET();
		if ( m_iReadSock == -1 )
			return( false );

		m_address.SinFamily();
		m_address.SinPort( m_iport );

		return( true );
	}

	const CS_STRING & GetBindHost() const { return( m_sBindHost ); }
	void SetBindHost( const CS_STRING & sBindHost ) { m_sBindHost = sBindHost; }
		
	enum EDNSLType
	{
		DNS_VHOST,
		DNS_DEST
	};
	
	/**
	 * DNSLookup nonblocking dns lookup (when -pthread is set to compile
	 * 
	 * @return 0 for success, EAGAIN to check back again (same arguments as before, ETIMEDOUT on failure
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

private:
	u_short		m_iport, m_iRemotePort, m_iLocalPort;
	int			m_iReadSock, m_iWriteSock, m_itimeout, m_iConnType, m_iTcount, m_iMethod;
	bool		m_bssl, m_bIsConnected, m_bBLOCK, m_bFullsslAccept;
	bool		m_bsslEstablished, m_bEnableReadLine, m_bRequireClientCert, m_bPauseRead;
	CS_STRING	m_shostname, m_sbuffer, m_sSockName, m_sPemFile, m_sCipherType, m_sParentName;
	CS_STRING	m_sSend, m_sSSLBuffer, m_sPemPass, m_sLocalIP, m_sRemoteIP;
	ECloseType	m_eCloseType;

	unsigned long long	m_iMaxMilliSeconds, m_iLastSendTime, m_iBytesRead, m_iBytesWritten, m_iStartTime;
	unsigned int		m_iMaxBytes, m_iLastSend, m_iMaxStoredBufferLength, m_iTimeoutType;

	CSSockAddr 		m_address, m_bindhost;
	bool			m_bIsIPv6;

#ifdef HAVE_LIBSSL
	SSL 				*m_ssl;
	SSL_CTX				*m_ssl_ctx;
	SSL_METHOD			*m_ssl_method;

	virtual void FREE_SSL();
	virtual void FREE_CTX();

#endif /* HAVE_LIBSSL */

	std::vector<CCron *>		m_vcCrons;

	//! Create the socket
	virtual int SOCKET( bool bListen = false );
	virtual void Init( const CS_STRING & sHostname, u_short iport, int itimeout = 60 );


	// Connection State Info
	ECONState		m_eConState;
	CS_STRING		m_sBindHost;
	u_int			m_iCurBindCount, m_iDNSTryCount;

#ifdef ___DO_THREADS
	CDNSResolver	m_cResolver;
#endif /* ___DO_THREADS */

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
		m_bRequiresClientCert = false;
#endif /* HAVE_LIBSSL */
	}
	virtual ~CSListener() {}

	const u_short GetPort() const { return( m_iPort ); }
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
	bool GetRequiresClientCert() const { return( m_bRequiresClientCert ); }
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
	//! set to true if require a client certificate
	void SetRequiresClientCert( bool b ) { m_bRequiresClientCert = b; }
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
	bool		m_bRequiresClientCert;
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
		SUCCESS			= 0,	//! Select returned more then 1 fd ready for action
		SELECT_ERROR	= -1,	//! An Error Happened, Probably dead socket. That socket is returned if available
		SELECT_TIMEOUT	= -2,	//! Select Timeout
		SELECT_TRYAGAIN	= -3	//! Select calls for you to try again
	};

	/**
	* Create a connection
	*
	* \param sHostname the destination
	* \param iPort the destination port
	* \param sSockName the Socket Name ( should be unique )
	* \param iTimeout the amount of time to try to connect
	* \param isSSL does the connection require a SSL layer
	* \param sBindHost the host to bind too
	* \param bIsIPv6 set to true to connect to an ipv6 host, requires HAVE_IPV6 at compile time to work
	* \return true on success
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
#ifdef HAVE_LIBSSL
		pcSock->SetSSL( cListen.GetIsSSL() );
		if( ( cListen.GetIsSSL() ) && ( !cListen.GetPemLocation().empty() ) )
		{
			pcSock->SetPemLocation( cListen.GetPemLocation() );
			pcSock->SetPemPass( cListen.GetPemPass() );
			pcSock->SetCipher( cListen.GetCipher() );
			pcSock->SetRequiresClientCert( cListen.GetRequiresClientCert() );
		}
#endif /* HAVE_LIBSSL */

		if( piRandPort )
			*piRandPort = 0;

		if ( pcSock->Listen( cListen.GetPort(), cListen.GetMaxConns(), cListen.GetBindHost(), cListen.GetTimeout() ) )
		{
			AddSock( pcSock, cListen.GetSockName() );
			if( ( piRandPort ) && ( cListen.GetPort() == 0 ) )
			{
				int iSock = pcSock->GetSock();

				if ( iSock < 0 )
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

	/*
	* Best place to call this class for running, all the call backs are called
	* You should through this in your main while loop (long as its not blocking)
	* all the events are called as needed
	*/
	virtual void Loop ()
	{
		for( u_int a = 0; a < this->size(); a++ )
		{
			T *pcSock = (*this)[a];

			if ( ( pcSock->GetType() != T::OUTBOUND ) || ( pcSock->GetConState() == T::CST_OK ) )
				continue;

			if ( pcSock->GetConState() == T::CST_DNS )
			{
				if ( pcSock->DNSLookup( T::DNS_DEST ) == ETIMEDOUT )
				{
					pcSock->SockError( EDOM );
					DelSock( a-- );
					continue;
				}
			}

			if ( pcSock->GetConState() == T::CST_VHOSTDNS )
			{
				if ( pcSock->DNSLookup( T::DNS_VHOST ) == ETIMEDOUT )
				{
					pcSock->SockError( EADDRNOTAVAIL );
					DelSock( a-- );
					continue;
				}
			}

			if ( pcSock->GetConState() == T::CST_BINDVHOST )
			{
				if ( !pcSock->SetupVHost() )
				{
					pcSock->SockError( errno );
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
						pcSock->SockError( ECONNABORTED );

					DelSock( a-- );
					continue;
				}

#ifdef HAVE_LIBSSL
				if ( pcSock->GetSSL() )
				{
					if ( !pcSock->ConnectSSL() )
					{
						if ( GetSockError() == ECONNREFUSED )
							pcSock->ConnectionRefused();
						else
							pcSock->SockError( ECONNABORTED );

						DelSock( a-- );
						continue;
					}
				}
#endif /* HAVE_LIBSSL */
			}
		}

		std::map<T *, EMessages> mpeSocks;
		Select( mpeSocks );
		std::set<T *> spReadySocks;

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
						char *buff;
						int iLen = 0;

						if ( pcSock->GetSSL() )
							iLen = pcSock->GetPending();

						if ( iLen > 0 )
						{
							buff = (char *)malloc( iLen );
						} else
						{
							iLen = CS_BLOCKSIZE;
							buff = (char *)malloc( CS_BLOCKSIZE );
						}

						int bytes = pcSock->Read( buff, iLen );

						if ( ( bytes != T::READ_TIMEDOUT ) && ( bytes != T::READ_CONNREFUSED )
							&& ( !pcSock->IsConnected() ) )
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

								pcSock->ReadData( buff, bytes );	// Call ReadData() before PushBuff() so that it is called before the ReadLine() event - LD  07/18/05
								pcSock->PushBuff( buff, bytes );
								break;
							}
						}

						// free up the buff
						free( buff );
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
			case SELECT_ERROR:
			default	:
				break;
		}

		unsigned long long iMilliNow = millitime();
		if ( ( iMilliNow - m_iCallTimeouts ) > 1000 )
		{
			m_iCallTimeouts = iMilliNow;
			// call timeout on all the sockets that recieved no data
			for( unsigned int i = 0; i < this->size(); i++ )
			{
				if ( (*this)[i]->GetConState() != T::CST_OK )
					continue;

				if ( (*this)[i]->CheckTimeout() )
					DelSock( i-- );
			}
		}
		// run any Manager Crons we may have
		Cron();
	}

	/*
	* Make this method virtual, so you can override it when a socket is added
	* Assuming you might want to do some extra stuff
	*/
	virtual void AddSock( T *pcSock, const CS_STRING & sSockName )
	{
		pcSock->SetSockName( sSockName );
		push_back( pcSock );
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
		for( unsigned int i = 0; i < this->size(); i++ )
			if ( (*this)[i]->GetSockName() == sName )
				return( (*this)[i] );

		return( NULL );
	}

	//! returns a pointer to the FIRST sock found by filedescriptor or NULL on no match
	virtual T * FindSockByFD( int iFD )
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

	//! delete cron(s) by name
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
	u_int GetSelectTimeout() { return( m_iSelectWait ); }
	//! Set the Select Timeout in MICROSECODS ( 1000 == 1 millisecond )
	//! Setting this to 0 will cause no timeout to happen, select will return instantly
	void  SetSelectTimeout( u_int iTimeout ) { m_iSelectWait = iTimeout; }

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

		Csock * pSock = (*this)[iPos];

		if ( pSock->IsConnected() )
			pSock->Disconnected(); // only call disconnected event if connected event was called (IE IsConnected was set)

		m_iBytesRead += pSock->GetBytesRead();
		m_iBytesWritten += pSock->GetBytesWritten();

		CS_Delete( pSock );
		this->erase( this->begin() + iPos );
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
		fd_set rfds, wfds;

		tv.tv_sec = 0;
		tv.tv_usec = m_iSelectWait;

		u_int iQuickReset = 100;
		if ( m_iSelectWait == 0 )
			iQuickReset = 0;

		TFD_ZERO( &rfds );
		TFD_ZERO( &wfds );

		// before we go any further, Process work needing to be done on the job
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			if ( ( (*this)[i]->GetCloseType() == T::CLT_NOW ) || ( ( (*this)[i]->GetCloseType() == T::CLT_AFTERWRITE ) && ( (*this)[i]->GetWriteBuffer().empty() ) ) )
				DelSock( i-- ); // close any socks that have requested it
			else
				(*this)[i]->Cron(); // call the Cron handler here
		}

		bool bHasWriteable = false;
		bool bHasAvailSocks = false;

		for( unsigned int i = 0; i < this->size(); i++ )
		{
			T *pcSock = (*this)[i];

			if ( pcSock->GetConState() != T::CST_OK )
				continue;

			bHasAvailSocks = true;

			int & iRSock = pcSock->GetRSock();
			int & iWSock = pcSock->GetWSock();
			bool bIsReadPaused = pcSock->IsReadPaused();
			if ( bIsReadPaused )
			{
				pcSock->ReadPaused();
				bIsReadPaused = pcSock->IsReadPaused(); // re-read it again, incase it changed status)
			}
			if ( ( iRSock < 0 ) || ( iWSock < 0 ) )
			{
				SelectSock( mpeSocks, SUCCESS, pcSock );
				continue;	// invalid sock fd
			}

			if ( pcSock->GetType() != T::LISTENER )
			{
				if ( ( pcSock->GetSSL() ) && ( pcSock->GetType() == T::INBOUND ) && ( !pcSock->FullSSLAccept() ) )
				{
					tv.tv_usec = iQuickReset;	// just make sure this returns quick incase we still need pending
					// try accept on this socket again
					if ( !pcSock->AcceptSSL() )
						pcSock->Close();

				} else if ( ( pcSock->IsConnected() ) && ( pcSock->GetWriteBuffer().empty() ) )
				{
					if ( !bIsReadPaused )
						TFD_SET( iRSock, &rfds );

				} else if ( ( pcSock->GetSSL() ) && ( !pcSock->SslIsEstablished() ) && ( !pcSock->GetWriteBuffer().empty() ) )
				{
					// do this here, cause otherwise ssl will cause a small
					// cpu spike waiting for the handshake to finish
					TFD_SET( iRSock, &rfds );
					// resend this data
					if ( !pcSock->Write( "" ) )
					{
						pcSock->Close();
					}
				} else
				{
					if ( !bIsReadPaused )
						TFD_SET( iRSock, &rfds );

					TFD_SET( iWSock, &wfds );
					bHasWriteable = true;
				}

			} else
				TFD_SET( iRSock, &rfds );
		}

		// first check to see if any ssl sockets are ready for immediate read
		// a mini select() type deal for ssl
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			T *pcSock = (*this)[i];

			if ( pcSock->GetConState() != T::CST_OK )
				continue;

			if ( ( pcSock->GetSSL() ) && ( pcSock->GetType() != Csock::LISTENER ) )
			{
				if ( ( pcSock->GetPending() > 0 ) && ( !pcSock->IsReadPaused() ) )
					SelectSock( mpeSocks, SUCCESS, pcSock );
			}
		}

		// old fashion select, go fer it
		int iSel;

		if ( !mpeSocks.empty() ) // .1 ms pause to see if anything else is ready (IE if there is SSL data pending, don't wait too long)
			tv.tv_usec = iQuickReset;
		else if ( ( !this->empty() ) && ( !bHasAvailSocks ) )
			tv.tv_usec = iQuickReset;


		if ( bHasWriteable )
			iSel = select(FD_SETSIZE, &rfds, &wfds, NULL, &tv);
		else
			iSel = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

		if ( iSel == 0 )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_TIMEOUT;
			else
				m_errno = SUCCESS;

			return;
		}

		if ( ( iSel == -1 ) && ( errno == EINTR ) )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_TRYAGAIN;
			else
				m_errno = SUCCESS;

			return;
		} else if ( iSel == -1 )
		{
			if ( mpeSocks.empty() )
				m_errno = SELECT_ERROR;
			else
				m_errno = SUCCESS;

			return;
		} else
		{
			m_errno = SUCCESS;
		}

		// find out wich one is ready
		for( unsigned int i = 0; i < this->size(); i++ )
		{
			T *pcSock = (*this)[i];

			if ( pcSock->GetConState() != T::CST_OK )
				continue;

			int & iRSock = pcSock->GetRSock();
			int & iWSock = pcSock->GetWSock();
			EMessages iErrno = SUCCESS;

			if ( ( iRSock < 0 ) || ( iWSock < 0 ) )
			{
				// trigger a success so it goes through the normal motions
				// and an error is produced
				SelectSock( mpeSocks, SUCCESS, pcSock );
				continue; // watch for invalid socks
			}

			if ( TFD_ISSET( iWSock, &wfds ) )
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

			} else if ( TFD_ISSET( iRSock, &rfds ) )
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
					int inSock = pcSock->Accept( sHost, port );

					if ( inSock != -1 )
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
							NewpcSock->SetRequiresClientCert( pcSock->RequiresClientCert() );
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
				}
			}
		}
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
		for( unsigned int a = 0; a < m_vcCrons.size(); a++ )
		{
			CCron *pcCron = m_vcCrons[a];

			if ( !pcCron->isValid() )
			{
				CS_Delete( pcCron );
				m_vcCrons.erase( m_vcCrons.begin() + a-- );
			} else
				pcCron->run();
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
	u_int					m_iSelectWait;
};

//! basic socket class
typedef TSocketManager<Csock> CSocketManager;

#ifndef _NO_CSOCKET_NS
}
#endif /* _NO_CSOCKET_NS */

#endif /* _HAS_CSOCKET_ */

