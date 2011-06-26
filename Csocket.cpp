/** @file
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

#include "Csocket.h"
#ifdef __NetBSD__
#include <sys/param.h>
#endif /* __NetBSD__ */

#ifdef HAVE_LIBSSL
#include <stdio.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#endif /* HAVE_LIBSSL */

#include <list>

#define CS_SRANDBUFFER 128

using namespace std;

#define CREATE_ARES_VER( a, b, c ) ((a<<16)|(b<<8)|c)

#ifndef _NO_CSOCKET_NS // some people may not want to use a namespace
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */

static int g_iCsockSSLIdx = 0; //!< this get setup once in InitSSL
int GetCsockClassIdx()
{
	return( g_iCsockSSLIdx );
}

#ifdef _WIN32
static const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
	if( af == AF_INET )
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy( &in.sin_addr, src, sizeof(struct in_addr) );
		getnameinfo( (struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST );
		return dst;
	}
	else if( af == AF_INET6 )
	{
		struct sockaddr_in6 in;
		memset( &in, 0, sizeof(in) );
		in.sin6_family = AF_INET6;
		memcpy( &in.sin6_addr, src, sizeof(struct in_addr6) );
		getnameinfo( (struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST );
		return dst;
	}
	return( NULL );
}

#if defined(_WIN32) && (!defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600))
//! thanks to KiNgMaR @ #znc for this wrapper
static int inet_pton( int af, const char *src, void *dst )
{
	sockaddr_storage aAddress;
	int iAddrLen = sizeof( sockaddr_storage );
	memset( &aAddress, 0, iAddrLen );
	char *pTmp = strdup( src );
	aAddress.ss_family = af; // this is important:
	// The function fails if the sin_family member of the SOCKADDR_IN structure is not set to AF_INET or AF_INET6.
	int iRet = WSAStringToAddressA( pTmp, af, NULL, (sockaddr *)&aAddress, &iAddrLen );
	free( pTmp );
	if( iRet == 0 )
	{
		if( af == AF_INET6 )
			 memcpy(dst, &((sockaddr_in6 *)&aAddress)->sin6_addr, sizeof(in6_addr));
		else
			 memcpy(dst, &((sockaddr_in *)&aAddress)->sin_addr, sizeof(in_addr));
		return( 1 );
	}
	return( -1 );
}
#endif

static inline void set_non_blocking(cs_sock_t fd)
{
	u_long iOpts = 1;
	ioctlsocket( fd, FIONBIO, &iOpts );
}

static inline void set_blocking(cs_sock_t fd)
{
	u_long iOpts = 0;
	ioctlsocket( fd, FIONBIO, &iOpts );
}

static inline void set_close_on_exec(cs_sock_t fd)
{
	// TODO add this for windows
	// see http://gcc.gnu.org/ml/java-patches/2002-q1/msg00696.html
	// for infos on how to do this
}
#else
static inline void set_non_blocking(cs_sock_t fd)
{
	int fdflags = fcntl(fd, F_GETFL, 0);
	if ( fdflags < 0 )
		return; // Ignore errors
	fcntl( fd, F_SETFL, fdflags|O_NONBLOCK );
}

static inline void set_blocking(cs_sock_t fd)
{
	int fdflags = fcntl(fd, F_GETFL, 0);
	if ( fdflags < 0 )
		return; // Ignore errors
	fdflags &= ~O_NONBLOCK;
	fcntl( fd, F_SETFL, fdflags );
}

static inline void set_close_on_exec(cs_sock_t fd)
{
	int fdflags = fcntl(fd, F_GETFD, 0);
	if ( fdflags < 0 )
		return; // Ignore errors
	fcntl( fd, F_SETFD, fdflags|FD_CLOEXEC);
}
#endif /* _WIN32 */

#ifdef HAVE_LIBSSL
Csock *GetCsockFromCTX( X509_STORE_CTX *pCTX )
{
	Csock *pSock = NULL;
	SSL *pSSL = (SSL *)X509_STORE_CTX_get_ex_data( pCTX, SSL_get_ex_data_X509_STORE_CTX_idx() );
	if( pSSL )
		pSock = (Csock *)SSL_get_ex_data( pSSL, GetCsockClassIdx() );
	return( pSock );
}
#endif /* HAVE_LIBSSL */


#ifndef HAVE_IPV6

// this issue here is getaddrinfo has a significant behavior difference when dealing with round robin dns on an
// ipv4 network. This is not desirable IMHO. so when this is compiled without ipv6 support backwards compatibility
// is maintained.

static int __GetHostByName( const CS_STRING & sHostName, struct in_addr *paddr, u_int iNumRetries )
{
	int iReturn = HOST_NOT_FOUND;
	struct hostent *hent = NULL;
#ifdef __linux__
	char hbuff[2048];
	struct hostent hentbuff;

	int err;
	for( u_int a = 0; a < iNumRetries; a++ )
	{
		memset( (char *)hbuff, '\0', 2048 );
		iReturn = gethostbyname_r( sHostName.c_str(), &hentbuff, hbuff, 2048, &hent, &err );

		if ( iReturn == 0 )
			break;

		if ( iReturn != TRY_AGAIN )
		{
			CS_DEBUG( "gethostyname_r: " << hstrerror( h_errno ) );
			break;
		}

	}
	if ( ( !hent ) && ( iReturn == 0 ) )
		iReturn = HOST_NOT_FOUND;
#else
	for( u_int a = 0; a < iNumRetries; a++ )
	{
		iReturn = HOST_NOT_FOUND;
		hent = gethostbyname( sHostName.c_str() );

		if ( hent )
		{
			iReturn = 0;
			break;
		}

		if( h_errno != TRY_AGAIN )
		{
#ifndef _WIN32
			CS_DEBUG( "gethostyname: " << hstrerror( h_errno ) );
#endif /* _WIN32 */
			break;
		}
	}

#endif /* __linux__ */

	if ( iReturn == 0 )
		memcpy( &paddr->s_addr, hent->h_addr_list[0], sizeof( paddr->s_addr ) );

	return( iReturn == TRY_AGAIN ? EAGAIN : iReturn );
}
#endif /* !HAVE_IPV6 */

#ifdef HAVE_C_ARES
void Csock::FreeAres()
{
	if( m_pARESChannel )
	{
		ares_destroy( m_pARESChannel );
		m_pARESChannel = NULL;
	}
}
#endif /* HAVE_C_ARES */

#ifdef HAVE_C_ARES
static void AresHostCallback( void *pArg, int status, int timeouts, struct hostent *hent )
{
	Csock *pSock = (Csock *)pArg;
	if( status == ARES_SUCCESS && hent && hent->h_addr_list[0] != NULL )
	{
		CSSockAddr *pSockAddr = pSock->GetCurrentAddr();
		if( hent->h_addrtype == AF_INET )
		{
			pSock->SetIPv6( false );
			memcpy( pSockAddr->GetAddr(), hent->h_addr_list[0], sizeof( *(pSockAddr->GetAddr()) ) );
		}
#ifdef HAVE_IPV6
		else if( hent->h_addrtype == AF_INET6 )
		{
			pSock->SetIPv6( true );
			memcpy( pSockAddr->GetAddr6(), hent->h_addr_list[0], sizeof( *(pSockAddr->GetAddr6()) ) );
		}
#endif /* HAVE_IPV6 */
		else
		{
			status = ARES_ENOTFOUND;
		}
	}
	else
	{
		CS_DEBUG( ares_strerror( status ) );
		if( status == ARES_SUCCESS )
		{
			CS_DEBUG("Received ARES_SUCCESS without any useful reply, using NODATA instead");
			status = ARES_ENODATA;
		}
	}
	pSock->SetAresFinished( status );
}
#endif /* HAVE_C_ARES */

int GetAddrInfo( const CS_STRING & sHostname, Csock *pSock, CSSockAddr & csSockAddr )
{
#ifndef HAVE_IPV6
	// if ipv6 is not enabled, then simply use gethostbyname, nothing special outside of this is done
	if( pSock )
		pSock->SetIPv6( false );
	csSockAddr.SetIPv6( false );
	if( __GetHostByName( sHostname, csSockAddr.GetAddr(), 3 ) == 0 )
		return( 0 );

#else /* HAVE_IPV6 */
	struct addrinfo *res = NULL;
	struct addrinfo hints;
	memset( (struct addrinfo *)&hints, '\0', sizeof( hints ) );
	hints.ai_family = csSockAddr.GetAFRequire();

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
#ifdef AI_ADDRCONFIG
	// this is suppose to eliminate host from appearing that this system can not support
	hints.ai_flags = AI_ADDRCONFIG;
#endif /* AI_ADDRCONFIG */

	if( pSock && ( pSock->GetType() == Csock::LISTENER || pSock->GetConState() == Csock::CST_BINDVHOST ) )
	{ // when doing a dns for bind only, set the AI_PASSIVE flag as suggested by the man page
		hints.ai_flags |= AI_PASSIVE;
	}

	int iRet = getaddrinfo( sHostname.c_str(), NULL, &hints, &res );
	if( iRet == EAI_AGAIN )
		return( EAGAIN ); // need to return telling the user to try again
	else if( ( iRet == 0 ) && ( res ) )
	{ 
		std::list<struct addrinfo *> lpTryAddrs;
		bool bFound = false;
		for( struct addrinfo *pRes = res; pRes; pRes = pRes->ai_next )
		{ // pass through the list building out a lean list of candidates to try. AI_CONFIGADDR doesn't always seem to work
#ifdef __sun
			if( ( pRes->ai_socktype != SOCK_STREAM ) || ( pRes->ai_protocol != IPPROTO_TCP && pRes->ai_protocol != IPPROTO_IP ) )
#else
			if( ( pRes->ai_socktype != SOCK_STREAM ) || ( pRes->ai_protocol != IPPROTO_TCP ) )
#endif /* __sun work around broken impl of getaddrinfo */
				continue;
			
			if( ( csSockAddr.GetAFRequire() != CSSockAddr::RAF_ANY ) && ( pRes->ai_family != csSockAddr.GetAFRequire() ) )
				continue; // they requested a special type, so be certain we woop past anything unwanted
			lpTryAddrs.push_back( pRes );
		}
		for( std::list<struct addrinfo *>::iterator it = lpTryAddrs.begin(); it != lpTryAddrs.end();  )
		{ // cycle through these, leaving the last iterator for the outside caller to call, so if there is an error it can call the events
			struct addrinfo * pRes = *it;
			bool bTryConnect = false;
			if( pRes->ai_family == AF_INET )
			{
				if( pSock )
					pSock->SetIPv6( false );
				csSockAddr.SetIPv6( false );
				struct sockaddr_in *pTmp = (struct sockaddr_in *)pRes->ai_addr;
				memcpy( csSockAddr.GetAddr(), &(pTmp->sin_addr), sizeof( *(csSockAddr.GetAddr()) ) );
				if( pSock && pSock->GetConState() == Csock::CST_DESTDNS && pSock->GetType() == Csock::OUTBOUND )
				{
					bTryConnect = true;
				}
				else
				{
					bFound = true;
					break;
				}
			}
			else if( pRes->ai_family == AF_INET6 )
			{
				if( pSock )
					pSock->SetIPv6( true );
				csSockAddr.SetIPv6( true );
				struct sockaddr_in6 *pTmp = (struct sockaddr_in6 *)pRes->ai_addr;
				memcpy( csSockAddr.GetAddr6(), &(pTmp->sin6_addr), sizeof( *(csSockAddr.GetAddr6()) ) );
				if( pSock && pSock->GetConState() == Csock::CST_DESTDNS && pSock->GetType() == Csock::OUTBOUND )
				{
					bTryConnect = true;
				}
				else
				{
					bFound = true;
					break;
				}
			}

			it++; // increment the iterator her so we know if its the last element or not

			if( bTryConnect && it != lpTryAddrs.end() )
			{ // save the last attempt for the outer loop, the issue then becomes that the error is thrown on the last failure
				if( pSock->CreateSocksFD() && pSock->Connect( pSock->GetBindHost(), true ) )
				{
					pSock->SetSkipConnect( true ); // this tells the socket that the connection state has been started
					bFound = true;
					break;
				}
				pSock->CloseSocksFD();
			}
			else if( bTryConnect )
			{
				bFound = true;
			}
		}

		freeaddrinfo( res );
		if( bFound ) // the data pointed to here is invalid now, but the pointer itself is a good test
		{
			return( 0 );
		}
	}
#endif /* ! HAVE_IPV6 */
	return( ETIMEDOUT );
}

bool InitCsocket()
{
#ifdef _WIN32
	WSADATA wsaData;
	int iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if( iResult != NO_ERROR )
		return( false );
#endif /* _WIN32 */
#ifdef HAVE_C_ARES
#if ARES_VERSION >= CREATE_ARES_VER( 1, 6, 1 )
	if( ares_library_init( ARES_LIB_INIT_ALL ) != 0 )
		return( false );
#endif /* ARES_VERSION >= CREATE_ARES_VER( 1, 6, 1 ) */
#endif /* HAVE_C_ARES */
#ifdef HAVE_LIBSSL
	if( !InitSSL() )
		return( false );
#endif /* HAVE_LIBSSL */
	return( true );
}

void ShutdownCsocket()
{
#ifdef HAVE_LIBSSL
	ERR_remove_state(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
#endif /* HAVE_LIBSSL */
#ifdef HAVE_C_ARES
#if ARES_VERSION >= CREATE_ARES_VER( 1, 6, 1 )
	ares_library_cleanup();
#endif /* ARES_VERSION >= CREATE_ARES_VER( 1, 6, 1 ) */
#endif /* HAVE_C_ARES */
#ifdef _WIN32
	WSACleanup();
#endif /* _WIN32 */
}

#ifdef HAVE_LIBSSL
bool InitSSL( ECompType eCompressionType )
{
	SSL_load_error_strings();
	if ( SSL_library_init() != 1 )
	{
		CS_DEBUG( "SSL_library_init() failed!" );
		return( false );
	}

#ifndef _WIN32
	if ( access( "/dev/urandom", R_OK ) == 0 )
		RAND_load_file( "/dev/urandom", 1024 );
	else if( access( "/dev/random", R_OK ) == 0 )
		RAND_load_file( "/dev/random", 1024 );
	else
	{
		CS_DEBUG( "Unable to locate entropy location! Tried /dev/urandom and /dev/random" );
		return( false );
	}
#endif /* _WIN32 */

	COMP_METHOD *cm = NULL;

	if ( CT_ZLIB & eCompressionType )
	{
		cm = COMP_zlib();
		if ( cm )
			SSL_COMP_add_compression_method( CT_ZLIB, cm );
	}

	if ( CT_RLE & eCompressionType )
	{
		cm = COMP_rle();
		if ( cm )
			SSL_COMP_add_compression_method( CT_RLE, cm );
	}

	// setting this up once in the begining
	g_iCsockSSLIdx = SSL_get_ex_new_index( 0, (void *)"CsockGlobalIndex", NULL, NULL, NULL);

	return( true );
}

void SSLErrors( const char *filename, u_int iLineNum )
{
	unsigned long iSSLError = 0;
	while( ( iSSLError = ERR_get_error() ) != 0 )
	{
		CS_DEBUG( "at " << filename << ":" << iLineNum );
		char szError[512];
		memset( (char *) szError, '\0', 512 );
		ERR_error_string_n( iSSLError, szError, 511 );
		if ( *szError )
			CS_DEBUG( szError );
	}
}
#endif /* HAVE_LIBSSL */

void __Perror( const CS_STRING & s, const char *pszFile, unsigned int iLineNo )
{
#if defined( sgi ) || defined(__sun) || defined(_WIN32) || (defined(__NetBSD_Version__) && __NetBSD_Version__ < 4000000000)
	std::cerr << s << "(" << pszFile << ":" << iLineNo << "): " << strerror( GetSockError() ) << endl;
#else
	char buff[512];
	memset( (char *)buff, '\0', 512 );
	if ( strerror_r( GetSockError(), buff, 511 ) == 0 )
		std::cerr << s << "(" << pszFile << ":" << iLineNo << "): " << buff << endl;
	else
		std::cerr << s << "(" << pszFile << ":" << iLineNo << "): Unknown Error Occured " << endl;
#endif /* __sun */
}

unsigned long long millitime()
{
	unsigned long long iTime = 0;
#ifdef _WIN32
	struct timeb tm;
	ftime( &tm );
	iTime = tm.time * 1000;
	iTime += tm.millitm;
#else
	struct timeval tv;
	gettimeofday( &tv, NULL );
	iTime = (unsigned long long )tv.tv_sec * 1000;
	iTime += ( (unsigned long long)tv.tv_usec / 1000 );
#endif /* _WIN32 */
	return( iTime );
}

#ifndef _NO_CSOCKET_NS // some people may not want to use a namespace
}
using namespace Csocket;
#endif /* _NO_CSOCKET_NS */

CCron::CCron()
{
	m_iCycles = 0;
	m_iMaxCycles = 0;
	m_bActive = true;
	m_iTime	= 0;
	m_iTimeSequence = 60;
	m_bPause = false;
	m_bRunOnNextCall = false;
}

void CCron::run( time_t & iNow )
{
	if ( m_bPause )
		return;

	if( iNow == 0 )
		iNow = time( NULL );

	if ( ( m_bActive ) && ( iNow >= m_iTime || m_bRunOnNextCall ) )
	{
		m_bRunOnNextCall = false; // Setting this here because RunJob() could set it back to true
		RunJob();

		if ( ( m_iMaxCycles > 0 ) && ( ++m_iCycles >= m_iMaxCycles  ) )
			m_bActive = false;
		else
			m_iTime = iNow + m_iTimeSequence;
	}
}

void CCron::StartMaxCycles( int TimeSequence, u_int iMaxCycles )
{
	m_iTimeSequence = TimeSequence;
	m_iTime = time( NULL ) + m_iTimeSequence;
	m_iMaxCycles = iMaxCycles;
}

void CCron::Start( int TimeSequence )
{
	m_iTimeSequence = TimeSequence;
	m_iTime = time( NULL ) + m_iTimeSequence;
	m_iMaxCycles = 0;
}

void CCron::Stop()
{
	m_bActive = false;
}

void CCron::Pause()
{
	m_bPause = true;
}

void CCron::UnPause()
{
	m_bPause = false;
}

int CCron::GetInterval() const { return( m_iTimeSequence ); }
u_int CCron::GetMaxCycles() const { return( m_iMaxCycles ); }
u_int CCron::GetCyclesLeft() const { return( ( m_iMaxCycles > m_iCycles ? ( m_iMaxCycles - m_iCycles ) : 0 ) ); }

bool CCron::isValid() { return( m_bActive ); }
const CS_STRING & CCron::GetName() const { return( m_sName ); }
void CCron::SetName( const CS_STRING & sName ) { m_sName = sName; }
void CCron::RunJob() { CS_DEBUG( "This should be overridden" ); }

Csock::Csock( int itimeout )
{
#ifdef HAVE_LIBSSL
	m_pCerVerifyCB = NULL;
#endif /* HAVE_LIBSSL */
	Init( "", 0, itimeout );
}

Csock::Csock( const CS_STRING & sHostname, u_short iport, int itimeout )
{
#ifdef HAVE_LIBSSL
	m_pCerVerifyCB = NULL;
#endif /* HAVE_LIBSSL */
	Init( sHostname, iport, itimeout );
}

// override this for accept sockets
Csock *Csock::GetSockObj( const CS_STRING & sHostname, u_short iPort )
{
	return( NULL );
}

#ifdef _WIN32
#define CS_CLOSE closesocket
#else
#define CS_CLOSE close
#endif /* _WIN32 */

Csock::~Csock()
{
#ifdef HAVE_C_ARES
	if( m_pARESChannel )
		ares_cancel( m_pARESChannel );
	FreeAres();
#endif /* HAVE_C_ARES */

#ifdef HAVE_LIBSSL
	FREE_SSL();
	FREE_CTX();
#endif /* HAVE_LIBSSL */

	CloseSocksFD();

	// delete any left over crons
	for( vector<CCron *>::size_type i = 0; i < m_vcCrons.size(); i++ )
		CS_Delete( m_vcCrons[i] );
}

void Csock::CloseSocksFD()
{
	if ( m_iReadSock != m_iWriteSock )
	{
		if( m_iReadSock != CS_INVALID_SOCK )
			CS_CLOSE( m_iReadSock );
		if( m_iWriteSock != CS_INVALID_SOCK )
			CS_CLOSE( m_iWriteSock );
	} else if( m_iReadSock != CS_INVALID_SOCK )
		CS_CLOSE( m_iReadSock );

	m_iReadSock = CS_INVALID_SOCK;
	m_iWriteSock = CS_INVALID_SOCK;
}


void Csock::Dereference()
{
	m_iWriteSock = m_iReadSock = CS_INVALID_SOCK;

#ifdef HAVE_LIBSSL
	m_ssl = NULL;
	m_ssl_ctx = NULL;
#endif /* HAVE_LIBSSL */

	m_vcCrons.clear();
	Close( CLT_DEREFERENCE );
}

void Csock::Copy( const Csock & cCopy )
{
	m_iTcount		= cCopy.m_iTcount;
	m_iLastCheckTimeoutTime	=	cCopy.m_iLastCheckTimeoutTime;
	m_uPort 		= cCopy.m_uPort;
	m_iRemotePort	= cCopy.m_iRemotePort;
	m_iLocalPort	= cCopy.m_iLocalPort;
	m_iReadSock		= cCopy.m_iReadSock;
	m_iWriteSock	= cCopy.m_iWriteSock;
	m_itimeout		= cCopy.m_itimeout;
	m_iConnType		= cCopy.m_iConnType;
	m_iMethod		= cCopy.m_iMethod;
	m_bssl			= cCopy.m_bssl;
	m_bIsConnected	= cCopy.m_bIsConnected;
	m_bBLOCK		= cCopy.m_bBLOCK;
	m_bsslEstablished	= cCopy.m_bsslEstablished;
	m_bEnableReadLine	= cCopy.m_bEnableReadLine;
	m_bPauseRead		= cCopy.m_bPauseRead;
	m_shostname		= cCopy.m_shostname;
	m_sbuffer		= cCopy.m_sbuffer;
	m_sSockName		= cCopy.m_sSockName;
	m_sPemFile		= cCopy.m_sPemFile;
	m_sCipherType	= cCopy.m_sCipherType;
	m_sParentName	= cCopy.m_sParentName;
	m_sSend			= cCopy.m_sSend;
	m_sPemPass		= cCopy.m_sPemPass;
	m_sLocalIP		= cCopy.m_sLocalIP;
	m_sRemoteIP		= cCopy.m_sRemoteIP;
	m_eCloseType	= cCopy.m_eCloseType;

	m_iMaxMilliSeconds	= cCopy.m_iMaxMilliSeconds;
	m_iLastSendTime		= cCopy.m_iLastSendTime;
	m_iBytesRead		= cCopy.m_iBytesRead;
	m_iBytesWritten		= cCopy.m_iBytesWritten;
	m_iStartTime		= cCopy.m_iStartTime;
	m_iMaxBytes			= cCopy.m_iMaxBytes;
	m_iLastSend			= cCopy.m_iLastSend;
	m_iMaxStoredBufferLength	= cCopy.m_iMaxStoredBufferLength;
	m_iTimeoutType		= cCopy.m_iTimeoutType;

	m_address			= cCopy.m_address;
	m_bindhost			= cCopy.m_bindhost;
	m_bIsIPv6			= cCopy.m_bIsIPv6;
	m_bSkipConnect		= cCopy.m_bSkipConnect;
#ifdef HAVE_C_ARES
	FreeAres(); // Not copying this state, but making sure its nulled out
	m_iARESStatus = -1; // set it to unitialized
	m_pCurrAddr = NULL;
#endif /* HAVE_C_ARES */

#ifdef HAVE_LIBSSL
	m_iRequireClientCertFlags = cCopy.m_iRequireClientCertFlags;
	m_sSSLBuffer	= cCopy.m_sSSLBuffer;

	FREE_SSL();
	FREE_CTX(); // be sure to remove anything that was already here
	m_ssl				= cCopy.m_ssl;
	m_ssl_ctx			= cCopy.m_ssl_ctx;

	m_pCerVerifyCB		= cCopy.m_pCerVerifyCB;

#endif /* HAVE_LIBSSL */

	if( !m_vcCrons.empty() )
	{
		for( u_long a = 0; a < m_vcCrons.size(); a++ )
		{
			CS_Delete( m_vcCrons[a] );
		}
		m_vcCrons.clear();
	}
	m_vcCrons			= cCopy.m_vcCrons;

	m_eConState			= cCopy.m_eConState;
	m_sBindHost			= cCopy.m_sBindHost;
	m_iCurBindCount		= cCopy.m_iCurBindCount;
	m_iDNSTryCount		= cCopy.m_iDNSTryCount;

}

Csock & Csock::operator<<( const CS_STRING & s )
{
	Write( s );
	return( *this );
}

Csock & Csock::operator<<( ostream & ( *io )( ostream & ) )
{
	Write( "\r\n" );
	return( *this );
}

Csock & Csock::operator<<( int i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( unsigned int i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( long i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( unsigned long i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( unsigned long long i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( float i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}
Csock & Csock::operator<<( double i )
{
	stringstream s;
	s << i;
	Write( s.str() );
	return( *this );
}

bool Csock::Connect( const CS_STRING & sBindHost, bool bSkipSetup )
{
	if( m_bSkipConnect )
	{ // this was already called, so skipping now. this is to allow easy pass through
		if ( m_eConState != CST_OK )
		{
			m_eConState = ( GetSSL() ? CST_CONNECTSSL : CST_OK );
		}
		return( true );
	}
	// bind to a hostname if requested
	m_sBindHost = sBindHost;
	if ( !bSkipSetup )
	{
		if ( !sBindHost.empty() )
		{
			// try to bind 3 times, otherwise exit failure
			bool bBound = false;
			for( int a = 0; a < 3 && !bBound; a++ )
			{
				if ( SetupVHost() )
					bBound = true;
#ifdef _WIN32
				Sleep( 5000 );
#else
				usleep( 5000 );	// quick pause, common lets BIND!)(!*!
#endif /* _WIN32 */
			}

			if ( !bBound )
			{
				CS_DEBUG( "Failure to bind to " << sBindHost );
				return( false );
			}
		}

		int iDNSRet = ETIMEDOUT;
		while( true )
		{
			iDNSRet = DNSLookup( DNS_VHOST );
			if ( iDNSRet == EAGAIN )
				continue;

			break;
		}
		if ( iDNSRet != 0 )
			return( false );

	}

	// set it none blocking
	set_non_blocking( m_iReadSock );

	m_iConnType = OUTBOUND;

	int ret = -1;
	if( !GetIPv6() )
		ret = connect( m_iReadSock, (struct sockaddr *)m_address.GetSockAddr(), m_address.GetSockAddrLen() );
#ifdef HAVE_IPV6
	else
		ret = connect( m_iReadSock, (struct sockaddr *)m_address.GetSockAddr6(), m_address.GetSockAddrLen6() );
#endif /* HAVE_IPV6 */
#ifndef _WIN32
	if ( ( ret == -1 ) && ( GetSockError() != EINPROGRESS ) )
#else
	if ( ( ret == -1 ) && ( GetSockError() != EINPROGRESS ) && ( GetSockError() != WSAEWOULDBLOCK ) )
#endif /* _WIN32 */

	{
		CS_DEBUG( "Connect Failed. ERRNO [" << GetSockError() << "] FD [" << m_iReadSock << "]" );
		return( false );
	}

	if ( m_bBLOCK )
	{
		set_blocking( m_iReadSock );
	}

	if ( m_eConState != CST_OK )
	{
		m_eConState = ( GetSSL() ? CST_CONNECTSSL : CST_OK );
	}

	return( true );
}

int Csock::WriteSelect()
{
	if ( m_iWriteSock == CS_INVALID_SOCK )
		return( SEL_ERR );

	struct timeval tv;
	fd_set wfds;

	TFD_ZERO( &wfds );
	TFD_SET( m_iWriteSock, &wfds );

	tv.tv_sec = m_itimeout;
	tv.tv_usec = 0;

	int ret = select( FD_SETSIZE, NULL, &wfds, NULL, &tv );

	if ( ret == 0 )
		return( SEL_TIMEOUT );

	if ( ret == -1 )
	{
		if ( GetSockError() == EINTR )
			return( SEL_EAGAIN );
		else
			return( SEL_ERR );
	}

	return( SEL_OK );
}

int Csock::ReadSelect()
{
	if ( m_iReadSock == CS_INVALID_SOCK )
		return( SEL_ERR );

	struct timeval tv;
	fd_set rfds;

	TFD_ZERO( &rfds );
	TFD_SET( m_iReadSock, &rfds );

	tv.tv_sec = m_itimeout;
	tv.tv_usec = 0;

	int ret = select( FD_SETSIZE, &rfds, NULL, NULL, &tv );

	if ( ret == 0 )
		return( SEL_TIMEOUT );

	if ( ret == -1 )
	{
		if ( GetSockError() == EINTR )
			return( SEL_EAGAIN );
		else
			return( SEL_ERR );
	}

	return( SEL_OK );
}

bool Csock::Listen( u_short iPort, int iMaxConns, const CS_STRING & sBindHost, u_int iTimeout )
{
	m_iConnType = LISTENER;
	m_itimeout = iTimeout;

	m_sBindHost = sBindHost;
	if ( !sBindHost.empty() )
	{
		// forcing this to block regardless of resolver overloading, because listen is not currently setup to
		// to handle nonblocking operations. This is used to resolve local ip's for binding anyways and should be instant
		if( ::GetAddrInfo( sBindHost, this, m_address ) != 0 )
			return( false );
	}

	m_iReadSock = m_iWriteSock = CreateSocket( true );

	if ( m_iReadSock == CS_INVALID_SOCK )
		return( false );

#ifdef HAVE_IPV6
// there's no IPPROTO_IPV6 below Win XP. - KiNgMaR
#if (!defined(_WIN32) && defined(IPV6_V6ONLY)) || (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0501)
	if( GetIPv6() )
	{
		// per RFC3493#5.3
		const int on = ( m_address.GetAFRequire() == CSSockAddr::RAF_INET6 ? 1 : 0 );
		if( setsockopt( m_iReadSock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof( on ) ) != 0 )
			PERROR( "IPV6_V6ONLY" );
	}
#endif /* IPV6_V6ONLY */
#endif /* HAVE_IPV6 */

	m_address.SinFamily();
	m_address.SinPort( iPort );
	if( !GetIPv6() )
	{
		if ( bind( m_iReadSock, (struct sockaddr *) m_address.GetSockAddr(), m_address.GetSockAddrLen() ) == -1 )
			return( false );
	}
#ifdef HAVE_IPV6
	else
	{
		if ( bind( m_iReadSock, (struct sockaddr *) m_address.GetSockAddr6(), m_address.GetSockAddrLen6() ) == -1 )
			return( false );
	}
#endif /* HAVE_IPV6 */

	if ( listen( m_iReadSock, iMaxConns ) == -1 )
		return( false );

	if ( !m_bBLOCK )
	{
		// set it none blocking
		set_non_blocking( m_iReadSock );
	}

	return( true );
}

cs_sock_t Csock::Accept( CS_STRING & sHost, u_short & iRPort )
{
	cs_sock_t iSock = CS_INVALID_SOCK;
	if( !GetIPv6() )
	{
		struct sockaddr_in client;
		socklen_t clen = sizeof( client );
		iSock = accept( m_iReadSock, (struct sockaddr *) &client, &clen );
		if( iSock != CS_INVALID_SOCK )
		{
			getpeername( iSock, (struct sockaddr *) &client, &clen );
			sHost = ConvertAddress( &client.sin_addr, false );
			iRPort = ntohs( client.sin_port );
		}
	}
#ifdef HAVE_IPV6
	else
	{
		struct sockaddr_in6 client;
		socklen_t clen = sizeof( client );
		iSock = accept( m_iReadSock, (struct sockaddr *) &client, &clen );
		if( iSock != CS_INVALID_SOCK )
		{
			getpeername( iSock, (struct sockaddr *) &client, &clen );
			sHost = ConvertAddress( &client.sin6_addr, true );
			iRPort = ntohs( client.sin6_port );
		}
	}
#endif /* HAVE_IPV6 */

	if ( iSock != CS_INVALID_SOCK )
	{
		// Make it close-on-exec
		set_close_on_exec( iSock );

		if ( !m_bBLOCK )
		{
			// make it none blocking 
			set_non_blocking( iSock );
		}

		if ( !ConnectionFrom( sHost, iRPort ) )
		{
			CS_CLOSE( iSock );
			iSock = CS_INVALID_SOCK;
		}

	}

	return( iSock );
}

bool Csock::AcceptSSL()
{
#ifdef HAVE_LIBSSL
	if ( !m_ssl )
		if ( !SSLServerSetup() )
			return( false );

	int err = SSL_accept( m_ssl );

	if ( err == 1 )
	{
		return( true );
	}

	int sslErr = SSL_get_error( m_ssl, err );

	if ( ( sslErr == SSL_ERROR_WANT_READ ) || ( sslErr == SSL_ERROR_WANT_WRITE ) )
		return( true );

	SSLErrors( __FILE__, __LINE__ );

#endif /* HAVE_LIBSSL */

	return( false );
}

bool Csock::SSLClientSetup()
{
#ifdef HAVE_LIBSSL
	m_bssl = true;
	FREE_SSL();
	FREE_CTX();

#ifdef _WIN64
	if( m_iReadSock != (int)m_iReadSock || m_iWriteSock != (int)m_iWriteSock )
	{
		// sanity check the FD to be sure its compatible with openssl
		CS_DEBUG( "ERROR: sockfd larger than OpenSSL can handle" );
		return( false );
	}
#endif /* _WIN64 */

	switch( m_iMethod )
	{
		case SSL3:
			m_ssl_ctx = SSL_CTX_new ( SSLv3_client_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv3_client_method failed!" );
				return( false );
			}
			break;
		case TLS1:
			m_ssl_ctx = SSL_CTX_new ( TLSv1_client_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... TLSv1_client_method failed!" );
				return( false );
			}
			break;
		case SSL2:
#ifndef OPENSSL_NO_SSL2
			m_ssl_ctx = SSL_CTX_new ( SSLv2_client_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv2_client_method failed!" );
				return( false );
			}
			break;
#endif
			/* Fall through if SSL2 is disabled */
		case SSL23:
		default:
			m_ssl_ctx = SSL_CTX_new ( SSLv23_client_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv23_client_method failed!" );
				return( false );
			}
			break;
	}


	SSL_CTX_set_default_verify_paths( m_ssl_ctx );

	if ( !m_sPemFile.empty() )
	{	// are we sending a client cerificate ?
		SSL_CTX_set_default_passwd_cb( m_ssl_ctx, PemPassCB );
		SSL_CTX_set_default_passwd_cb_userdata( m_ssl_ctx, (void *)this );

		//
		// set up the CTX
		if ( SSL_CTX_use_certificate_file( m_ssl_ctx, m_sPemFile.c_str() , SSL_FILETYPE_PEM ) <= 0 )
		{
			CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
			SSLErrors( __FILE__, __LINE__ );
		}

		if ( SSL_CTX_use_PrivateKey_file( m_ssl_ctx, m_sPemFile.c_str(), SSL_FILETYPE_PEM ) <= 0 )
		{
			CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
			SSLErrors( __FILE__, __LINE__ );
		}
	}

	m_ssl = SSL_new ( m_ssl_ctx );
	if ( !m_ssl )
		return( false );

	SSL_set_rfd( m_ssl, (int)m_iReadSock );
	SSL_set_wfd( m_ssl, (int)m_iWriteSock );
	SSL_set_verify( m_ssl, SSL_VERIFY_PEER, ( m_pCerVerifyCB ? m_pCerVerifyCB : CertVerifyCB ) );
	SSL_set_ex_data( m_ssl, GetCsockClassIdx(), this );

	SSLFinishSetup( m_ssl );
	return( true );
#else
	return( false );

#endif /* HAVE_LIBSSL */
}

bool Csock::SSLServerSetup()
{
#ifdef HAVE_LIBSSL
	m_bssl = true;
	FREE_SSL();
	FREE_CTX();

#ifdef _WIN64
	if( m_iReadSock != (int)m_iReadSock || m_iWriteSock != (int)m_iWriteSock )
	{
		// sanity check the FD to be sure its compatible with openssl
		CS_DEBUG( "ERROR: sockfd larger than OpenSSL can handle" );
		return( false );
	}
#endif /* _WIN64 */


	switch( m_iMethod )
	{
		case SSL3:
			m_ssl_ctx = SSL_CTX_new ( SSLv3_server_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv3_server_method failed!" );
				return( false );
			}
			break;

		case TLS1:
			m_ssl_ctx = SSL_CTX_new ( TLSv1_server_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... TLSv1_server_method failed!" );
				return( false );
			}
			break;
#ifndef OPENSSL_NO_SSL2
		case SSL2:
			m_ssl_ctx = SSL_CTX_new ( SSLv2_server_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv2_server_method failed!" );
				return( false );
			}
			break;
#endif
			/* Fall through if SSL2 is disabled */
		case SSL23:
		default:
			m_ssl_ctx = SSL_CTX_new ( SSLv23_server_method() );
			if ( !m_ssl_ctx )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv23_server_method failed!" );
				return( false );
			}
			break;
	}

	SSL_CTX_set_default_verify_paths( m_ssl_ctx );

	// set the pemfile password
	SSL_CTX_set_default_passwd_cb( m_ssl_ctx, PemPassCB );
	SSL_CTX_set_default_passwd_cb_userdata( m_ssl_ctx, (void *)this );

	if ( ( m_sPemFile.empty() ) || ( access( m_sPemFile.c_str(), R_OK ) != 0 ) )
	{
		CS_DEBUG( "There is a problem with [" << m_sPemFile << "]" );
		return( false );
	}

	//
	// set up the CTX
	if( SSL_CTX_use_certificate_chain_file( m_ssl_ctx, m_sPemFile.c_str() ) <= 0 )
	{
		CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
		SSLErrors( __FILE__, __LINE__ );
		return( false );
	}

	if( SSL_CTX_use_PrivateKey_file( m_ssl_ctx, m_sPemFile.c_str(), SSL_FILETYPE_PEM ) <= 0 )
	{
		CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
		SSLErrors( __FILE__, __LINE__ );
		return( false );
	}

	// check to see if this pem file contains a DH structure for use with DH key exchange
	// https://github.com/znc/znc/pull/46
	FILE *dhParamsFile = fopen( m_sPemFile.c_str(), "r" );
	if( !dhParamsFile )
	{
		CS_DEBUG( "There is a problem with [" << m_sPemFile << "]" );
		return( false );
	}

	DH * dhParams = PEM_read_DHparams( dhParamsFile, NULL, NULL, NULL );
	fclose( dhParamsFile );
	if( dhParams )
	{
		SSL_CTX_set_options( m_ssl_ctx, SSL_OP_SINGLE_DH_USE );
		if( !SSL_CTX_set_tmp_dh( m_ssl_ctx, dhParams ) )
		{
			CS_DEBUG( "Error setting ephemeral DH parameters from [" << m_sPemFile << "]" );
			SSLErrors( __FILE__, __LINE__ );
			DH_free( dhParams );
			return( false );
		}
		DH_free( dhParams );
	}
	else
	{ // Presumably PEM_read_DHparams failed, as there was no DH structure. Clearing those errors here so they are removed off the stack
		ERR_clear_error();
	}

	if( SSL_CTX_set_cipher_list( m_ssl_ctx, m_sCipherType.c_str() ) <= 0 )
	{
		CS_DEBUG( "Could not assign cipher [" << m_sCipherType << "]" );
		return( false );
	}

	//
	// setup the SSL
	m_ssl = SSL_new( m_ssl_ctx );
	if ( !m_ssl )
		return( false );

	// Call for client Verification
	SSL_set_rfd( m_ssl, (int)m_iReadSock );
	SSL_set_wfd( m_ssl, (int)m_iWriteSock );
	SSL_set_accept_state( m_ssl );
	if ( m_iRequireClientCertFlags )
	{
		SSL_set_verify( m_ssl, m_iRequireClientCertFlags, ( m_pCerVerifyCB ? m_pCerVerifyCB : CertVerifyCB ) );
		SSL_set_ex_data( m_ssl, GetCsockClassIdx(), this );
	}

	SSLFinishSetup( m_ssl );
	return( true );
#else
	return( false );
#endif /* HAVE_LIBSSL */
}

bool Csock::ConnectSSL( const CS_STRING & sBindhost )
{
#ifdef HAVE_LIBSSL
	if ( m_iReadSock == CS_INVALID_SOCK )
		if ( !Connect( sBindhost ) )
			return( false );
	if ( !m_ssl )
		if ( !SSLClientSetup() )
			return( false );

	bool bPass = true;

	if ( m_bBLOCK )
	{
		set_non_blocking( m_iReadSock );
	}

	int iErr = SSL_connect( m_ssl );
	if ( iErr != 1 )
	{
		int sslErr = SSL_get_error( m_ssl, iErr );
		bPass = false;
		if( sslErr == SSL_ERROR_WANT_READ || sslErr == SSL_ERROR_WANT_WRITE )
			bPass = true;
#ifdef _WIN32
		else if( sslErr == SSL_ERROR_SYSCALL && iErr < 0 && GetLastError() == WSAENOTCONN )
		{ 
			// this seems to be an issue with win32 only. I've seen it happen on slow connections
			// the issue is calling this before select(), which isn't a problem on unix. Allowing this
			// to pass in this case is fine because subsequent ssl transactions will occur and the handshake
			// will finish. At this point, its just instantiating the handshake.
			bPass = true;
		}
#endif /* _WIN32 */
	} else
		bPass = true;

	if ( m_bBLOCK )
	{
		// unset the flags afterwords, rather then have connect block
		set_blocking( m_iReadSock );
	}

	if ( m_eConState != CST_OK )
		m_eConState = CST_OK;
	return( bPass );
#else
	return( false );
#endif /* HAVE_LIBSSL */
}

bool Csock::AllowWrite( unsigned long long & iNOW ) const
{
	if ( ( m_iMaxBytes > 0 ) && ( m_iMaxMilliSeconds > 0 ) )
	{
		if( iNOW == 0 )
			iNOW = millitime();

		if( m_iLastSend <  m_iMaxBytes )
			return( true ); // allow sending if our out buffer was less than what we can send
		if ( ( iNOW - m_iLastSendTime ) < m_iMaxMilliSeconds )
			return( false );
	}
	return( true );
}

bool Csock::Write( const char *data, size_t len )
{
	m_sSend.append( data, len );

	if ( m_sSend.empty() )
		return( true );

	if ( m_eConState != CST_OK )
		return( true );

	if ( m_bBLOCK )
	{
		if ( WriteSelect() != SEL_OK )
			return( false );

	}
	// rate shaping
	u_long iBytesToSend = 0;

#ifdef HAVE_LIBSSL
	if( m_bssl && m_sSSLBuffer.empty() && !m_bsslEstablished )
	{
		// to keep openssl from spinning, just initiate the connection with 1 byte so the connection establishes faster
		iBytesToSend = 1;
	}
	else
#endif /* HAVE_LIBSSL */
	if ( ( m_iMaxBytes > 0 ) && ( m_iMaxMilliSeconds > 0 ) )
	{
		unsigned long long iNOW = millitime();
		// figure out the shaping here
		// if NOW - m_iLastSendTime > m_iMaxMilliSeconds then send a full length of ( iBytesToSend )
		if ( ( iNOW - m_iLastSendTime ) > m_iMaxMilliSeconds )
		{
			m_iLastSendTime = iNOW;
			iBytesToSend = m_iMaxBytes;
			m_iLastSend = 0;

		} else // otherwise send m_iMaxBytes - m_iLastSend
			iBytesToSend = m_iMaxBytes - m_iLastSend;

		// take which ever is lesser
		if ( m_sSend.length() < iBytesToSend )
			iBytesToSend = 	m_sSend.length();

		// add up the bytes sent
		m_iLastSend += iBytesToSend;

		// so, are we ready to send anything ?
		if ( iBytesToSend == 0 )
			return( true );

	} else
		iBytesToSend = m_sSend.length();

#ifdef HAVE_LIBSSL
	if ( m_bssl )
	{

		if ( m_sSSLBuffer.empty() ) // on retrying to write data, ssl wants the data in the SAME spot and the SAME size
			m_sSSLBuffer.append( m_sSend.data(), iBytesToSend );

		int iErr = SSL_write( m_ssl, m_sSSLBuffer.data(), (int)m_sSSLBuffer.length() );

		if ( ( iErr < 0 ) && ( GetSockError() == ECONNREFUSED ) )
		{
			// If ret == -1, the underlying BIO reported an I/O error (man SSL_get_error)
			ConnectionRefused();
			return( false );
		}

		switch( SSL_get_error( m_ssl, iErr ) )
		{
			case SSL_ERROR_NONE:
			m_bsslEstablished = true;
			// all ok
			break;

			case SSL_ERROR_ZERO_RETURN:
			{
				// weird closer alert
				return( false );
			}

			case SSL_ERROR_WANT_READ:
			// retry
			break;

			case SSL_ERROR_WANT_WRITE:
			// retry
			break;

			case SSL_ERROR_SSL:
			{
				SSLErrors( __FILE__, __LINE__ );
				return( false );
			}
		}

		if ( iErr > 0 )
		{
			m_sSSLBuffer.clear();
			m_sSend.erase( 0, iErr );
			// reset the timer on successful write (we have to set it here because the write
			// bit might not always be set, so need to trigger)
			if ( TMO_WRITE & GetTimeoutType() )
				ResetTimer();

			m_iBytesWritten += (unsigned long long)iErr;
		}

		return( true );
	}
#endif /* HAVE_LIBSSL */
#ifdef _WIN32
	cs_ssize_t bytes = send( m_iWriteSock, m_sSend.data(), iBytesToSend, 0 );
#else
	cs_ssize_t bytes = write( m_iWriteSock, m_sSend.data(), iBytesToSend );
#endif /* _WIN32 */

	if ( ( bytes == -1 ) && ( GetSockError() == ECONNREFUSED ) )
	{
		ConnectionRefused();
		return( false );
	}

#ifdef _WIN32
	if ( ( bytes <= 0 ) && ( GetSockError() != WSAEWOULDBLOCK ) )
		return( false );
#else
	if ( ( bytes <= 0 ) && ( GetSockError() != EAGAIN ) )
		return( false );
#endif /* _WIN32 */

	// delete the bytes we sent
	if ( bytes > 0 )
	{
		m_sSend.erase( 0, bytes );
		if ( TMO_WRITE & GetTimeoutType() )
			ResetTimer();	// reset the timer on successful write
		m_iBytesWritten += (unsigned long long)bytes;
	}

	return( true );
}

bool Csock::Write( const CS_STRING & sData )
{
	return( Write( sData.c_str(), sData.length() ) );
}

cs_ssize_t Csock::Read( char *data, size_t len )
{
	cs_ssize_t bytes = 0;

	if ( IsReadPaused() && SslIsEstablished() )
		return( READ_EAGAIN ); // allow the handshake to complete first

	if ( m_bBLOCK )
	{
		switch( ReadSelect() )
		{
			case SEL_OK:
				break;
			case SEL_TIMEOUT:
				return( READ_TIMEDOUT );
			default:
				return( READ_ERR );
		}
	}

#ifdef HAVE_LIBSSL
	if ( m_bssl )
	{
		bytes = SSL_read( m_ssl, data, (int)len );
		if( bytes >= 0 )
			m_bsslEstablished = true; // this means all is good in the realm of ssl
	}
	else
#endif /* HAVE_LIBSSL */
#ifdef _WIN32
		bytes = recv( m_iReadSock, data, len, 0 );
#else
		bytes = read( m_iReadSock, data, len );
#endif /* _WIN32 */
	if ( bytes == -1 )
	{
		if ( GetSockError() == ECONNREFUSED )
			return( READ_CONNREFUSED );

		if ( GetSockError() == ETIMEDOUT )
			return( READ_TIMEDOUT );

		if ( ( GetSockError() == EINTR ) || ( GetSockError() == EAGAIN ) )
			return( READ_EAGAIN );

#ifdef _WIN32
		if ( GetSockError() == WSAEWOULDBLOCK )
			return( READ_EAGAIN );
#endif /* _WIN32 */

#ifdef HAVE_LIBSSL
		if ( m_bssl )
		{
			int iErr = SSL_get_error( m_ssl, (int)bytes );
			if ( ( iErr != SSL_ERROR_WANT_READ ) && ( iErr != SSL_ERROR_WANT_WRITE ) )
				return( READ_ERR );
			else
				return( READ_EAGAIN );
		}
#else
		return( READ_ERR );
#endif /* HAVE_LIBSSL */
	}

	if( bytes > 0 ) // becareful not to add negative bytes :P
		m_iBytesRead += (unsigned long long)bytes;

	return( bytes );
}

CS_STRING Csock::GetLocalIP()
{
	if ( !m_sLocalIP.empty() )
		return( m_sLocalIP );

	cs_sock_t iSock = GetSock();

	if ( iSock == CS_INVALID_SOCK )
		return( "" );

	if( !GetIPv6() )
	{
		char straddr[INET_ADDRSTRLEN];
		struct sockaddr_in mLocalAddr;
		socklen_t mLocalLen = sizeof( mLocalAddr );
		if ( ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
			&& ( inet_ntop( AF_INET, &mLocalAddr.sin_addr, straddr, sizeof(straddr) ) ) )
		{
			m_sLocalIP = straddr;
		}
	}
#ifdef HAVE_IPV6
	else
	{
		char straddr[INET6_ADDRSTRLEN];
		struct sockaddr_in6 mLocalAddr;
		socklen_t mLocalLen = sizeof( mLocalAddr );
		if ( ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
			&& ( inet_ntop( AF_INET6, &mLocalAddr.sin6_addr, straddr, sizeof(straddr) ) ) )
		{
			m_sLocalIP = straddr;
		}
	}
#endif /* HAVE_IPV6 */

	return( m_sLocalIP );
}

CS_STRING Csock::GetRemoteIP()
{
	if ( !m_sRemoteIP.empty() )
		return( m_sRemoteIP );

	cs_sock_t iSock = GetSock();

	if ( iSock == CS_INVALID_SOCK )
		return( "" );

	if( !GetIPv6() )
	{
		struct sockaddr_in mRemoteAddr;
		socklen_t mRemoteLen = sizeof( mRemoteAddr );
		if ( getpeername( iSock, (struct sockaddr *) &mRemoteAddr, &mRemoteLen ) == 0 )
			m_sRemoteIP = ConvertAddress( &mRemoteAddr.sin_addr, false );
	}
#ifdef HAVE_IPV6
	else
	{
		struct sockaddr_in6 mRemoteAddr;
		socklen_t mRemoteLen = sizeof( mRemoteAddr );
		if ( getpeername( iSock, (struct sockaddr *) &mRemoteAddr, &mRemoteLen ) == 0 )
			m_sRemoteIP = ConvertAddress( &mRemoteAddr.sin6_addr, true );
	}
#endif /* HAVE_IPV6 */

	return( m_sRemoteIP );
}

CS_STRING Csock::ConvertAddress( void *addr, bool bIPv6 )
{
	CS_STRING sRet;

	if( !bIPv6 ) 
	{
		in_addr *p = (in_addr*) addr;
		sRet = inet_ntoa(*p);
	} 
	else 
	{
		char straddr[INET6_ADDRSTRLEN];
		if( inet_ntop( AF_INET6, addr, straddr, sizeof(straddr) ) > 0 )
			sRet = straddr;
	}

	return( sRet );
}

bool Csock::IsConnected() const { return( m_bIsConnected ); }
void Csock::SetIsConnected( bool b ) { m_bIsConnected = b; }

cs_sock_t & Csock::GetRSock() { return( m_iReadSock ); }
void Csock::SetRSock( cs_sock_t iSock ) { m_iReadSock = iSock; }
cs_sock_t & Csock::GetWSock() { return( m_iWriteSock ); }
void Csock::SetWSock( cs_sock_t iSock ) { m_iWriteSock = iSock; }
void Csock::SetSock( cs_sock_t iSock ) { m_iWriteSock = iSock; m_iReadSock = iSock; }
cs_sock_t & Csock::GetSock() { return( m_iReadSock ); }
void Csock::ResetTimer() { m_iLastCheckTimeoutTime = 0; m_iTcount = 0; }
void Csock::PauseRead() { m_bPauseRead = true; }
bool Csock::IsReadPaused() { return( m_bPauseRead ); }

void Csock::UnPauseRead()
{
	m_bPauseRead = false;
	ResetTimer();
	PushBuff( "", 0, true );
}

void Csock::SetTimeout( int iTimeout, u_int iTimeoutType )
{
	m_iTimeoutType = iTimeoutType;
	m_itimeout = iTimeout;
}

void Csock::SetTimeoutType( u_int iTimeoutType ) { m_iTimeoutType = iTimeoutType; }
int Csock::GetTimeout() const { return m_itimeout; }
u_int Csock::GetTimeoutType() const { return( m_iTimeoutType ); }

bool Csock::CheckTimeout( time_t iNow )
{
	if( m_iLastCheckTimeoutTime == 0 )
	{
		m_iLastCheckTimeoutTime = iNow;
		return( false );
	}

	if ( IsReadPaused() )
		return( false );

	time_t iDiff = 0;
	if( iNow > m_iLastCheckTimeoutTime )
		iDiff = iNow - m_iLastCheckTimeoutTime;
	else
	{
		// this is weird, but its possible if someone changes a clock and it went back in time, this essentially has to reset the last check
		// the worst case scenario is the timeout is about to it and the clock changes, it would then cause
		// this to pass over the last half the time
		m_iLastCheckTimeoutTime = iNow;
	}

	if ( m_itimeout > 0 )
	{
		// this is basically to help stop a clock adjust ahead, stuff could reset immediatly on a clock jump
		// otherwise
		time_t iRealTimeout = m_itimeout;
		if( iRealTimeout <= 1 )
			m_iTcount++;
		else if( m_iTcount == 0 )
			iRealTimeout /= 2;
		if( iDiff >= iRealTimeout )
		{
			if( m_iTcount == 0 )
				m_iLastCheckTimeoutTime = iNow - iRealTimeout;
			if( m_iTcount++ >= 1 )
			{
				Timeout();
				return( true );
			}
		}
	}
	return( false );
}

void Csock::PushBuff( const char *data, size_t len, bool bStartAtZero )
{
	if ( !m_bEnableReadLine )
		return;	// If the ReadLine event is disabled, just ditch here

	size_t iStartPos = ( m_sbuffer.empty() || bStartAtZero ? 0 : m_sbuffer.length() - 1 );

	if ( data )
		m_sbuffer.append( data, len );

	while( !m_bPauseRead && GetCloseType() == CLT_DONT  )
	{
		CS_STRING::size_type iFind = m_sbuffer.find( "\n", iStartPos );

		if ( iFind != CS_STRING::npos )
		{
			CS_STRING sBuff = m_sbuffer.substr( 0, iFind + 1 );	// read up to(including) the newline
			m_sbuffer.erase( 0, iFind + 1 );					// erase past the newline
			ReadLine( sBuff );
			iStartPos = 0; // reset this back to 0, since we need to look for the next newline here.

		} else
			break;
	}

	if ( ( m_iMaxStoredBufferLength > 0 ) && ( m_sbuffer.length() > m_iMaxStoredBufferLength ) )
		ReachedMaxBuffer(); // call the max read buffer event

}

CS_STRING & Csock::GetInternalReadBuffer() { return( m_sbuffer ); }
CS_STRING & Csock::GetInternalWriteBuffer() { return( m_sSend ); }
void Csock::SetMaxBufferThreshold( u_int iThreshold ) { m_iMaxStoredBufferLength = iThreshold; }
u_int Csock::GetMaxBufferThreshold() const { return( m_iMaxStoredBufferLength ); }
int Csock::GetType() const { return( m_iConnType ); }
void Csock::SetType( int iType ) { m_iConnType = iType; }
const CS_STRING & Csock::GetSockName() const { return( m_sSockName ); }
void Csock::SetSockName( const CS_STRING & sName ) { m_sSockName = sName; }
const CS_STRING & Csock::GetHostName() const { return( m_shostname ); }
void Csock::SetHostName( const CS_STRING & sHostname ) { m_shostname = sHostname; }
unsigned long long Csock::GetStartTime() const { return( m_iStartTime ); }
void Csock::ResetStartTime() { m_iStartTime = 0; }
unsigned long long Csock::GetBytesRead() const { return( m_iBytesRead ); }
void Csock::ResetBytesRead() { m_iBytesRead = 0; }
unsigned long long Csock::GetBytesWritten() const { return( m_iBytesWritten ); }
void Csock::ResetBytesWritten() { m_iBytesWritten = 0; }

double Csock::GetAvgRead( unsigned long long iSample )
{
	unsigned long long iDifference = ( millitime() - m_iStartTime );

	if ( ( m_iBytesRead == 0 ) || ( iSample > iDifference ) )
		return( (double)m_iBytesRead );

	return( ( (double)m_iBytesRead / ( (double)iDifference / (double)iSample ) ) );
}

double Csock::GetAvgWrite( unsigned long long iSample )
{
	unsigned long long iDifference = ( millitime() - m_iStartTime );

	if ( ( m_iBytesWritten == 0 ) || ( iSample > iDifference ) )
		return( (double)m_iBytesWritten );

	return( ( (double)m_iBytesWritten / ( (double)iDifference / (double)iSample ) ) );
}

u_short Csock::GetRemotePort()
{
	if ( m_iRemotePort > 0 )
		return( m_iRemotePort );

	cs_sock_t iSock = GetSock();

	if ( iSock != CS_INVALID_SOCK )
	{
		if( !GetIPv6() )
		{
			struct sockaddr_in mAddr;
			socklen_t mLen = sizeof( mAddr );
			if ( getpeername( iSock, (struct sockaddr *) &mAddr, &mLen ) == 0 )
				m_iRemotePort = ntohs( mAddr.sin_port );
		}
#ifdef HAVE_IPV6
		else
		{
			struct sockaddr_in6 mAddr;
			socklen_t mLen = sizeof( mAddr );
			if ( getpeername( iSock, (struct sockaddr *) &mAddr, &mLen ) == 0 )
				m_iRemotePort = ntohs( mAddr.sin6_port );
		}
#endif /* HAVE_IPV6 */
	}

	return( m_iRemotePort );
}

u_short Csock::GetLocalPort()
{
	if ( m_iLocalPort > 0 )
		return( m_iLocalPort );

	cs_sock_t iSock = GetSock();

	if ( iSock != CS_INVALID_SOCK )
	{
		if( !GetIPv6() )
		{
			struct sockaddr_in mLocalAddr;
			socklen_t mLocalLen = sizeof( mLocalAddr );
			if ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
				m_iLocalPort = ntohs( mLocalAddr.sin_port );
		}
#ifdef HAVE_IPV6
		else
		{
			struct sockaddr_in6 mLocalAddr;
			socklen_t mLocalLen = sizeof( mLocalAddr );
			if ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
				m_iLocalPort = ntohs( mLocalAddr.sin6_port );
		}
#endif /* HAVE_IPV6 */
	}

	return( m_iLocalPort );
}

u_short Csock::GetPort() { return( m_uPort ); }
void Csock::SetPort( u_short iPort ) { m_uPort = iPort; }
void Csock::Close( ECloseType eCloseType )
{
	m_eCloseType = eCloseType;
}
void Csock::BlockIO( bool bBLOCK ) { m_bBLOCK = bBLOCK; }

void Csock::NonBlockingIO()
{
	set_non_blocking( m_iReadSock );

	if ( m_iReadSock != m_iWriteSock )
	{
		set_non_blocking( m_iWriteSock );
	}

	BlockIO( false );
}

bool Csock::GetSSL() { return( m_bssl ); }
void Csock::SetSSL( bool b ) { m_bssl = b; }

#ifdef HAVE_LIBSSL
void Csock::SetCipher( const CS_STRING & sCipher ) { m_sCipherType = sCipher; }
const CS_STRING & Csock::GetCipher() { return( m_sCipherType ); }
void Csock::SetPemLocation( const CS_STRING & sPemFile ) { m_sPemFile = sPemFile; }
const CS_STRING & Csock::GetPemLocation() { return( m_sPemFile ); }
void Csock::SetPemPass( const CS_STRING & sPassword ) { m_sPemPass = sPassword; }
const CS_STRING & Csock::GetPemPass() const { return( m_sPemPass ); }

int Csock::PemPassCB( char *buf, int size, int rwflag, void *pcSocket )
{
	Csock *pSock = (Csock *)pcSocket;
	const CS_STRING & sPassword = pSock->GetPemPass();
	memset( buf, '\0', size );
	strncpy( buf, sPassword.c_str(), size );
	buf[size-1] = '\0';
	return( (int)strlen( buf ) );
}

int Csock::CertVerifyCB( int preverify_ok, X509_STORE_CTX *x509_ctx )
{
	/*
	 * A small quick example on how to get ahold of the Csock in the data portion of x509_ctx
	Csock *pSock = GetCsockFromCTX( x509_ctx );
	assert( pSock );
	cerr << pSock->GetRemoteIP() << endl;
	 */

	/* return 1 always for now, probably want to add some code for cert verification */
	return( 1 );
}

void Csock::SetSSLMethod( int iMethod ) { m_iMethod = iMethod; }
int Csock::GetSSLMethod() { return( m_iMethod ); }
void Csock::SetSSLObject( SSL *ssl ) { m_ssl = ssl; }
void Csock::SetCTXObject( SSL_CTX *sslCtx ) { m_ssl_ctx = sslCtx; }

SSL_SESSION * Csock::GetSSLSession()
{
	if ( m_ssl )
		return( SSL_get_session( m_ssl ) );

	return( NULL );
}
#endif /* HAVE_LIBSSL */

const CS_STRING & Csock::GetWriteBuffer() { return( m_sSend ); }
void Csock::ClearWriteBuffer() { m_sSend.clear(); }
bool Csock::SslIsEstablished() { return ( m_bsslEstablished ); }

bool Csock::ConnectInetd( bool bIsSSL, const CS_STRING & sHostname )
{
	if ( !sHostname.empty() )
		m_sSockName = sHostname;

	// set our hostname
	if ( m_sSockName.empty() )
	{
		struct sockaddr_in client;
		socklen_t clen = sizeof( client );
		if ( getpeername( 0, (struct sockaddr *)&client, &clen ) < 0 )
			m_sSockName = "0.0.0.0:0";
		else
		{
			stringstream s;
			s << inet_ntoa( client.sin_addr ) << ":" << ntohs( client.sin_port );
			m_sSockName = s.str();
		}
	}

	return( ConnectFD( 0, 1, m_sSockName, bIsSSL, INBOUND ) );
}

bool Csock::ConnectFD( int iReadFD, int iWriteFD, const CS_STRING & sName, bool bIsSSL, ETConn eDirection )
{
	if ( eDirection == LISTENER )
	{
		CS_DEBUG( "You can not use a LISTENER type here!" );
		return( false );
	}

	// set our socket type
	SetType( eDirection );

	// set the hostname
	m_sSockName = sName;

	// set the file descriptors
	SetRSock( iReadFD );
	SetWSock( iWriteFD );

	// set it up as non-blocking io
	NonBlockingIO();

	if ( bIsSSL )
	{
		if ( ( eDirection == INBOUND ) && ( !AcceptSSL() ) )
			return( false );
		else if ( ( eDirection == OUTBOUND ) && ( !ConnectSSL() ) )
			return( false );
	}

	return( true );
}

#ifdef HAVE_LIBSSL
X509 *Csock::getX509()
{
	if ( m_ssl )
		return( SSL_get_peer_certificate( m_ssl ) );

	return( NULL );
}

CS_STRING Csock::GetPeerPubKey()
{
	CS_STRING sKey;

	SSL_SESSION *pSession = GetSSLSession();

	if ( ( pSession ) && ( pSession->peer ) )
	{
		EVP_PKEY *pKey = X509_get_pubkey( pSession->peer );
		if ( pKey )
		{
			char *hxKey = NULL;
			switch( pKey->type )
			{
				case EVP_PKEY_RSA:
				{
					hxKey = BN_bn2hex( pKey->pkey.rsa->n );
					break;
				}
				case EVP_PKEY_DSA:
				{
					hxKey = BN_bn2hex( pKey->pkey.dsa->pub_key );
					break;
				}
				default:
				{
					CS_DEBUG( "Not Prepared for Public Key Type [" << pKey->type << "]" );
					break;
				}
			}
			if ( hxKey )
			{
				sKey = hxKey;
				OPENSSL_free( hxKey );
			}
			EVP_PKEY_free( pKey );
		}
	}
	return( sKey );
}
int Csock::GetPeerFingerprint( CS_STRING & sFP )
{
	sFP.clear();

	if ( !GetSSL() )
		return 0;

	X509* pCert = getX509();

	// Inspired by charybdis
	if ( pCert )
	{
		for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		{
			char buf[3];
			snprintf(buf, 3, "%02x", pCert->sha1_hash[i]);
			sFP += buf;
		}
		X509_free(pCert);
	}

	return SSL_get_verify_result(m_ssl);
}
unsigned int Csock::GetRequireClientCertFlags() { return( m_iRequireClientCertFlags ); }
void Csock::SetRequiresClientCert( bool bRequiresCert ) { m_iRequireClientCertFlags = ( bRequiresCert ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_PEER : 0 ); }

#endif /* HAVE_LIBSSL */

void Csock::SetParentSockName( const CS_STRING & sParentName ) { m_sParentName = sParentName; }
const CS_STRING & Csock::GetParentSockName() { return( m_sParentName ); }

void Csock::SetRate( u_int iBytes, unsigned long long iMilliseconds )
{
	m_iMaxBytes = iBytes;
	m_iMaxMilliSeconds = iMilliseconds;
}

u_int Csock::GetRateBytes() { return( m_iMaxBytes ); }
unsigned long long Csock::GetRateTime() { return( m_iMaxMilliSeconds ); }

void Csock::Cron()
{
	time_t iNow = 0;

	for( vector<CCron *>::size_type a = 0; a < m_vcCrons.size(); a++ )
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

void Csock::AddCron( CCron * pcCron )
{
	m_vcCrons.push_back( pcCron );
}

void Csock::DelCron( const CS_STRING & sName, bool bDeleteAll, bool bCaseSensitive )
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

void Csock::DelCron( u_int iPos )
{
	if ( iPos < m_vcCrons.size() )
	{
		m_vcCrons[iPos]->Stop();
		CS_Delete( m_vcCrons[iPos] );
		m_vcCrons.erase( m_vcCrons.begin() + iPos );
	}
}

void Csock::DelCronByAddr( CCron *pcCron )
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

void Csock::EnableReadLine() { m_bEnableReadLine = true; }
void Csock::DisableReadLine() {
	m_bEnableReadLine = false;
	m_sbuffer.clear();
}

void Csock::ReachedMaxBuffer()
{
	std::cerr << "Warning, Max Buffer length Warning Threshold has been hit" << endl;
	std::cerr << "If you don't care, then set SetMaxBufferThreshold to 0" << endl;
}

int Csock::GetPending()
{
#ifdef HAVE_LIBSSL
	if( m_ssl )
	{
		// in v23 method, the pending function is initialized to ssl_undefined_const_function
		// which throws SSL_UNDEFINED_CONST_FUNCTION on to the error stack
		// this is one of the more stupid things in openssl, it seems bizarre that even though SSL_pending
		// returns an int, they don't bother returning in error to notify us, so basically
		// we have to always clear errors here generated by SSL_pending, otherwise the stack could
		// have a lame error on it causing SSL_write to fail in certain instances.
#if defined( OPENSSL_VERSION_NUMBER ) && OPENSSL_VERSION_NUMBER >= 0x00908000
		ERR_set_mark();
		int iBytes = SSL_pending( m_ssl );
		ERR_pop_to_mark();
		return( iBytes );
#else 
		int iBytes = SSL_pending( m_ssl );
		ERR_clear_error(); // to get safer handling, upgrade your openssl version!
		return( iBytes );
#endif /* OPENSSL_VERSION_NUMBER */
	}
	else
		return( 0 );
#else
	return( 0 );
#endif /* HAVE_LIBSSL */
}

int Csock::GetAddrInfo( const CS_STRING & sHostname, CSSockAddr & csSockAddr )
{
#ifdef HAVE_IPV6
	if( csSockAddr.GetAFRequire() != AF_INET && inet_pton( AF_INET6, sHostname.c_str(), csSockAddr.GetAddr6() ) > 0 )
	{
		SetIPv6( true );
		return( 0 );
	}
#endif /* HAVE_IPV6 */
	if( inet_pton( AF_INET, sHostname.c_str(), csSockAddr.GetAddr() ) > 0 )
	{
#ifdef HAVE_IPV6
		SetIPv6( false );
#endif /* HAVE_IPV6 */
		return( 0 );
	}

#ifdef HAVE_C_ARES
	if( GetType() != LISTENER )
	{ // right now the current function in Listen() is it blocks, the easy way around this at the moment is to use ip
		// need to compute this up here
		if( !m_pARESChannel )
		{
			if( ares_init( &m_pARESChannel ) != ARES_SUCCESS )
			{ // TODO throw some debug?
				FreeAres();
				return( ETIMEDOUT );
			}
			m_pCurrAddr = &csSockAddr; // flag its starting

			int iFamily = AF_INET;
#ifdef HAVE_IPV6
			// as of ares 1.6.0 if it fails on af_inet6, it falls back to af_inet, this code was here in the previous Csocket version, just adding the comment as a reminder
			iFamily = csSockAddr.GetAFRequire() == CSSockAddr::RAF_ANY ? AF_INET6 : csSockAddr.GetAFRequire();
#endif /* HAVE_IPV6 */
			ares_gethostbyname( m_pARESChannel, sHostname.c_str(), iFamily, AresHostCallback, this );
		}
		if( !m_pCurrAddr )
		{ // this means its finished
			FreeAres();
#ifdef HAVE_IPV6
			if( m_iARESStatus == ARES_SUCCESS && csSockAddr.GetAFRequire() == CSSockAddr::RAF_ANY && GetIPv6() )
			{
				// this means that ares_host returned an ipv6 host, so try a connect right away
				if( CreateSocksFD() && Connect( GetBindHost(), true ) )
				{
					SetSkipConnect( true );
				}
				else if( GetSockError() == ENETUNREACH )
				{
					// the Connect() failed, so throw a retry back in with ipv4, and let it process normally
					CS_DEBUG( "Failed ipv6 connection with PF_UNSPEC, falling back to ipv4" );
					m_iARESStatus = -1;
					CloseSocksFD();
					SetAFRequire( CSSockAddr::RAF_INET );
					return( GetAddrInfo( sHostname, csSockAddr ) );
				}
			}
#if ARES_VERSION < CREATE_ARES_VER( 1, 5, 3 )
			if( m_iARESStatus != ARES_SUCCESS && csSockAddr.GetAFRequire() == CSSockAddr::RAF_ANY )
			{ // this is a workaround for ares < 1.5.3 where the builtin retry on failed AF_INET6 isn't there yet
				CS_DEBUG( "Retry for older version of c-ares with AF_INET only" );
				// this means we tried previously with AF_INET6 and failed, so force AF_INET and retry
				SetAFRequire( CSSockAddr::RAF_INET );
				return( GetAddrInfo( sHostname, csSockAddr ) );
			}
#endif /* ARES_VERSION < CREATE_ARES_VER( 1, 5, 3 ) */
#endif /* HAVE_IPV6 */
			return( m_iARESStatus == ARES_SUCCESS ? 0 : ETIMEDOUT );
		}
		return( EAGAIN );
	}
#endif /* HAVE_C_ARES */

	return( ::GetAddrInfo( sHostname, this, csSockAddr ) );
}

int Csock::DNSLookup( EDNSLType eDNSLType )
{
	if ( eDNSLType == DNS_VHOST )
	{
		if ( m_sBindHost.empty() )
		{
			if ( m_eConState != CST_OK )
				m_eConState = CST_DESTDNS; // skip binding, there is no vhost
			return( 0 );
		}

		m_bindhost.SinFamily();
		m_bindhost.SinPort( 0 );
	}

	int iRet = ETIMEDOUT;
	if ( eDNSLType == DNS_VHOST )
	{
		iRet = GetAddrInfo( m_sBindHost, m_bindhost );
#ifdef HAVE_IPV6
		if( m_bindhost.GetIPv6() )
		{
			SetAFRequire( CSSockAddr::RAF_INET6 );
		}
		else
		{
			SetAFRequire( CSSockAddr::RAF_INET );
		}
#endif /* HAVE_IPV6 */
	}
	else
	{
		iRet = GetAddrInfo( m_shostname, m_address );
	}

	if ( iRet == 0 )
	{
		if( !CreateSocksFD() )
		{
			m_iDNSTryCount = 0;
			return( ETIMEDOUT );
		}
		if ( m_eConState != CST_OK )
			m_eConState = ( ( eDNSLType == DNS_VHOST ) ? CST_BINDVHOST : CST_CONNECT );
		m_iDNSTryCount = 0;
		return( 0 );
	}
	else if ( iRet == EAGAIN )
	{
#ifndef HAVE_C_ARES
		m_iDNSTryCount++;
		if ( m_iDNSTryCount > 20 )
		{
			m_iDNSTryCount = 0;
			return( ETIMEDOUT );
		}
#endif /* HAVE_C_ARES */
		return( EAGAIN );
	}
	m_iDNSTryCount = 0;
	return( ETIMEDOUT );
}

bool Csock::SetupVHost()
{
	if ( m_sBindHost.empty() )
	{
		if ( m_eConState != CST_OK )
			m_eConState = CST_DESTDNS;
		return( true );
	}
	int iRet = -1;
	if( !GetIPv6() )
		iRet = bind( m_iReadSock, (struct sockaddr *) m_bindhost.GetSockAddr(), m_bindhost.GetSockAddrLen() );
#ifdef HAVE_IPV6
	else
		iRet = bind( m_iReadSock, (struct sockaddr *) m_bindhost.GetSockAddr6(), m_bindhost.GetSockAddrLen6() );
#endif /* HAVE_IPV6 */

	if ( iRet == 0 )
	{
		if ( m_eConState != CST_OK )
			m_eConState = CST_DESTDNS;
		return( true );
	}
	m_iCurBindCount++;
	if ( m_iCurBindCount > 3 )
	{
		CS_DEBUG( "Failure to bind to " << m_sBindHost );
		return( false );
	}

	return( true );
}

#ifdef HAVE_LIBSSL
void Csock::FREE_SSL()
{
	if ( m_ssl )
	{
		SSL_shutdown( m_ssl );
		SSL_free( m_ssl );
	}
	m_ssl = NULL;
}

void Csock::FREE_CTX()
{
	if ( m_ssl_ctx )
		SSL_CTX_free( m_ssl_ctx );

	m_ssl_ctx = NULL;
}

#endif /* HAVE_LIBSSL */

cs_sock_t Csock::CreateSocket( bool bListen )
{
#ifdef HAVE_IPV6
	cs_sock_t iRet = socket( ( GetIPv6() ? PF_INET6 : PF_INET ), SOCK_STREAM, IPPROTO_TCP );
#else
	cs_sock_t iRet = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
#endif /* HAVE_IPV6 */

	if ( iRet != CS_INVALID_SOCK ) 
	{
		set_close_on_exec( iRet );

		if ( bListen ) 
		{
			const int on = 1;

			if ( setsockopt( iRet, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof( on ) ) != 0 )
				PERROR( "SO_REUSEADDR" );
		}
	} 
	else
		PERROR( "socket" );

	return( iRet );
}

void Csock::Init( const CS_STRING & sHostname, u_short uPort, int itimeout )
{
#ifdef HAVE_LIBSSL
	m_ssl = NULL;
	m_ssl_ctx = NULL;
	m_iRequireClientCertFlags = 0;
#endif /* HAVE_LIBSSL */
	m_iTcount = 0;
	m_iReadSock = CS_INVALID_SOCK;
	m_iWriteSock = CS_INVALID_SOCK;
	m_itimeout = itimeout;
	m_bssl = false;
	m_bIsConnected = false;
	m_uPort = uPort;
	m_shostname = sHostname;
	m_sbuffer.clear();
	m_eCloseType = CLT_DONT;
	m_bBLOCK = true;
	m_iMethod = SSL23;
	m_sCipherType = "ALL";
	m_iMaxBytes = 0;
	m_iMaxMilliSeconds = 0;
	m_iLastSendTime = 0;
	m_iLastSend = 0;
	m_bsslEstablished = false;
	m_bEnableReadLine = false;
	m_iMaxStoredBufferLength = 1024;
	m_iConnType = INBOUND;
	m_iRemotePort = 0;
	m_iLocalPort = 0;
	m_iBytesRead = 0;
	m_iBytesWritten = 0;
	m_iStartTime = millitime();
	m_bPauseRead = false;
	m_iTimeoutType = TMO_ALL;
	m_eConState = CST_OK;	// default should be ok
	m_iDNSTryCount = 0;
	m_iCurBindCount = 0;
	m_bIsIPv6 = false;
	m_bSkipConnect = false;
	m_iLastCheckTimeoutTime = 0;
#ifdef HAVE_C_ARES
	m_pARESChannel = NULL;
	m_pCurrAddr = NULL;
	m_iARESStatus = -1;
#endif /* HAVE_C_ARES */
}

