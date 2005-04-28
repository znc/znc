/**
*
*    Copyright (c) 1999-2004 Jim Hull <imaginos@imaginos.net>
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

#include "main.h"
#include "Csocket.h"

#ifdef HAVE_LIBSSL
bool InitSSL( ECompType eCompressionType )
{
	SSL_load_error_strings();
	if ( SSL_library_init() != 1 )
	{
		CS_DEBUG( "SSL_library_init() failed!" );
		return( false );
	}

	if ( access( "/dev/urandom", R_OK ) == 0 )
		RAND_load_file( "/dev/urandom", 1024 );
	else if( access( "/dev/random", R_OK ) == 0 )
		RAND_load_file( "/dev/random", 1024 );
	else
	{
		CS_DEBUG( "Unable to locate entropy location! Tried /dev/urandom and /dev/random" );
		return( false );
	}

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
		if ( strlen( szError ) > 0 )
			CS_DEBUG( szError );
	}
}
#endif /* HAVE_LIBSSL */

void __Perror( const CS_STRING & s )
{
#ifdef __sun
	CS_DEBUG( s << ": " << strerror( errno ) );
#else
	char buff[512];
	memset( (char *)buff, '\0', 512 );
	if ( strerror_r( errno, buff, 511 ) == 0 )
		CS_DEBUG( s << ": " << buff );
	else
		CS_DEBUG( s << ": Unknown Error Occured" );
#endif /* __sun */
}

unsigned long long millitime()
{
	struct timeval tv;
	unsigned long long iTime = 0;
	gettimeofday( &tv, NULL );
	iTime = (unsigned long long )tv.tv_sec * 1000;
	iTime += ( (unsigned long long)tv.tv_usec / 1000 );
	return( iTime );
}

bool GetHostByName( const CS_STRING & sHostName, struct in_addr *paddr )
{
	bool bRet = false;
	struct hostent *hent = NULL;
#ifdef __linux__
	char hbuff[2048];
	struct hostent hentbuff;

	int err;
	for( u_int a = 0; a < 20; a++ )
	{
		memset( (char *)hbuff, '\0', 2048 );
		int iRet = gethostbyname_r( sHostName.c_str(), &hentbuff, hbuff, 2048, &hent, &err );

		if ( iRet == 0 )
		{
			bRet = true;
			break;
		}

		if ( iRet != TRY_AGAIN )
			break;

		PERROR( "gethostbyname_r" );
	}

	if ( !hent )
		bRet = false;
#else
	hent = gethostbyname( sHostName.c_str() );
	PERROR( "gethostbyname" );

	if ( hent )
		bRet = true;

#endif /* __linux__ */

	if ( bRet )
		memcpy( &paddr->s_addr, hent->h_addr_list[0], 4 );

	return( bRet );
}

CCron::CCron()
{
	m_iCycles = 0;
	m_iMaxCycles = 0;
	m_bActive = true;
	m_iTime	= 0;
	m_iTimeSequence = 60;
	m_bPause = false;
}

//! This is used by the Job Manager, and not you directly
void CCron::run()
{
	if ( m_bPause )
		return;

	if ( ( m_bActive ) && ( time( NULL ) >= m_iTime ) )
	{
		RunJob();

		if ( ( m_iMaxCycles > 0 ) && ( ++m_iCycles >= m_iMaxCycles  ) )
			m_bActive = false;
		else
			m_iTime = time( NULL ) + m_iTimeSequence;
	}
}

/**
 * @TimeSequence	how often to run in seconds
 * @iMaxCycles		how many times to run, 0 makes it run forever
 */
void CCron::StartMaxCycles( int TimeSequence, u_int iMaxCycles )
{
	m_iTimeSequence = TimeSequence;
	m_iTime = time( NULL ) + m_iTimeSequence;
	m_iMaxCycles = iMaxCycles;
}

//! starts and runs infinity amount of times
void CCron::Start( int TimeSequence )
{
	m_iTimeSequence = TimeSequence;
	m_iTime = time( NULL ) + m_iTimeSequence;
	m_iMaxCycles = 0;
}

//! call this to turn off your cron, it will be removed
void CCron::Stop()
{
	m_bActive = false;
}

//! pauses excution of your code in RunJob
void CCron::Pause()
{
	m_bPause = true;
}

//! removes the pause on RunJon
void CCron::UnPause()
{
	m_bPause = false;
}

int CCron::GetInterval() const { return( m_iTimeSequence ); }
u_int CCron::GetMaxCycles() const { return( m_iMaxCycles ); }
u_int CCron::GetCyclesLeft() const { return( ( m_iMaxCycles > m_iCycles ? ( m_iMaxCycles - m_iCycles ) : 0 ) ); }

//! returns true if cron is active
bool CCron::isValid() { return( m_bActive ); }
const CS_STRING & CCron::GetName() const { return( m_sName ); }
void CCron::SetName( const CS_STRING & sName ) { m_sName = sName; }
void CCron::RunJob() { CS_DEBUG( "This should be overriden" ); }

Csock::Csock( int itimeout )
{
	Init( "", 0, itimeout );
}

Csock::Csock( const CS_STRING & sHostname, int iport, int itimeout )
{
	Init( sHostname, iport, itimeout );
}

// override this for accept sockets
Csock *Csock::GetSockObj( const CS_STRING & sHostname, int iPort )
{
	return( NULL );
}

Csock::~Csock()
{
	if ( m_iReadSock != m_iWriteSock )
	{
		close( m_iReadSock );
		close( m_iWriteSock );
	} else
		close( m_iReadSock );

	m_iReadSock = -1;
	m_iWriteSock = -1;

#ifdef HAVE_LIBSSL
	FREE_SSL();
	FREE_CTX();

#endif /* HAVE_LIBSSL */
	// delete any left over crons
	for( vector<CCron *>::size_type i = 0; i < m_vcCrons.size(); i++ )
		CS_Delete( m_vcCrons[i] );
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

bool Csock::Connect( const CS_STRING & sBindHost )
{
	// create the socket
	m_iReadSock = m_iWriteSock = SOCKET();

	if ( m_iReadSock == -1 )
		return( false );

	m_address.sin_family = PF_INET;
	m_address.sin_port = htons( m_iport );

	if ( !GetHostByName( m_shostname, &(m_address.sin_addr) ) )
		return( false );

	// bind to a hostname if requested
	if ( !sBindHost.empty() )
	{
		struct sockaddr_in vh;

		vh.sin_family = PF_INET;
		vh.sin_port = htons( 0 );

		if ( !GetHostByName( sBindHost, &(vh.sin_addr) ) )
			return( false );

		// try to bind 3 times, otherwise exit failure
		bool bBound = false;
		for( int a = 0; a < 3; a++ )
		{
			if ( bind( m_iReadSock, (struct sockaddr *) &vh, sizeof( vh ) ) == 0 )
			{
				bBound = true;
				break;
			}
			usleep( 5000 );	// quick pause, common lets BIND!)(!*!
		}

		if ( !bBound )
		{
			CS_DEBUG( "Failure to bind to " << sBindHost );
			return( false );
		}
	}

	// set it none blocking
	int fdflags = fcntl (m_iReadSock, F_GETFL, 0);
	fcntl( m_iReadSock, F_SETFL, fdflags|O_NONBLOCK );
	m_iConnType = OUTBOUND;

	// connect
	int ret = connect( m_iReadSock, (struct sockaddr *)&m_address, sizeof( m_address ) );
	if ( ( ret == -1 ) && ( errno != EINPROGRESS ) )
	{
		CS_DEBUG( "Connect Failed. ERRNO [" << errno << "] FD [" << m_iReadSock << "]" );
		return( false );
	}

	if ( m_bBLOCK )
	{
		// unset the flags afterwords, rather than have connect block
		int fdflags = fcntl (m_iReadSock, F_GETFL, 0);
		fdflags &= ~O_NONBLOCK;
		fcntl( m_iReadSock, F_SETFL, fdflags );
	}

	return( true );
}

int Csock::WriteSelect()
{
	if ( m_iWriteSock < 0 )
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
		if ( errno == EINTR )
			return( SEL_EAGAIN );
		else
			return( SEL_ERR );
	}

	return( SEL_OK );
}

int Csock::ReadSelect()
{
	if ( m_iReadSock < 0 )
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
		if ( errno == EINTR )
			return( SEL_EAGAIN );
		else
			return( SEL_ERR );
	}

	return( SEL_OK );
}

bool Csock::Listen( int iPort, int iMaxConns, const CS_STRING & sBindHost, u_int iTimeout )
{
	m_iReadSock = m_iWriteSock = SOCKET( true );
	m_iConnType = LISTENER;
	m_itimeout = iTimeout;

	if ( m_iReadSock == -1 )
		return( false );

	m_address.sin_family = PF_INET;
	if ( sBindHost.empty() )
		m_address.sin_addr.s_addr = htonl( INADDR_ANY );
	else
	{
		if ( !GetHostByName( sBindHost, &(m_address.sin_addr) ) )
			return( false );
	}
	m_address.sin_port = htons( iPort );
	bzero(&(m_address.sin_zero), 8);

	if ( bind( m_iReadSock, (struct sockaddr *) &m_address, sizeof( m_address ) ) == -1 )
		return( false );

	if ( listen( m_iReadSock, iMaxConns ) == -1 )
		return( false );

	if ( !m_bBLOCK )
	{
		// set it none blocking
		int fdflags = fcntl ( m_iReadSock, F_GETFL, 0);
		fcntl( m_iReadSock, F_SETFL, fdflags|O_NONBLOCK );
	}

	return( true );
}

int Csock::Accept( CS_STRING & sHost, int & iRPort )
{
	struct sockaddr_in client;
	socklen_t clen = sizeof(struct sockaddr);

	int iSock = accept( m_iReadSock , (struct sockaddr *) &client, &clen );
	if ( iSock != -1 )
	{
		if ( !m_bBLOCK )
		{
			// make it none blocking
			int fdflags = fcntl (iSock, F_GETFL, 0);
			fcntl( iSock, F_SETFL, fdflags|O_NONBLOCK );
		}

		getpeername( iSock, (struct sockaddr *) &client, &clen );

		sHost = inet_ntoa( client.sin_addr );
		iRPort = ntohs( client.sin_port );

		if ( !ConnectionFrom( sHost, iRPort ) )
		{
			close( iSock );
			iSock = -1;
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
		m_bFullsslAccept = true;
		return( true );
	}

	m_bFullsslAccept = false;

	int sslErr = SSL_get_error( m_ssl, err );

	if ( ( sslErr == SSL_ERROR_WANT_READ ) || ( sslErr == SSL_ERROR_WANT_WRITE ) )
		return( true );

	SSLErrors( __FILE__, __LINE__ );

#endif /* HAVE_LIBSSL */

	return( false );
}

//! This sets up the SSL Client, this is used internally
bool Csock::SSLClientSetup()
{
#ifdef HAVE_LIBSSL
	m_bssl = true;
	FREE_SSL();
	FREE_CTX();

	switch( m_iMethod )
	{
		case SSL2:
			m_ssl_method = SSLv2_client_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv2_client_method failed!" );
				return( false );
			}
			break;

		case SSL3:
			m_ssl_method = SSLv3_client_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv3_client_method failed!" );
				return( false );
			}
			break;

		case SSL23:
		default:
			m_ssl_method = SSLv23_client_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv23_client_method failed!" );
				return( false );
			}
			break;
	}

	// wrap some warnings in here
	m_ssl_ctx = SSL_CTX_new ( m_ssl_method );
	if ( !m_ssl_ctx )
		return( false );

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

	SSL_set_rfd( m_ssl, m_iReadSock );
	SSL_set_wfd( m_ssl, m_iWriteSock );
	SSL_set_verify( m_ssl, SSL_VERIFY_PEER, CertVerifyCB );

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

	switch( m_iMethod )
	{
		case SSL2:
			m_ssl_method = SSLv2_server_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv2_server_method failed!" );
				return( false );
			}
			break;

		case SSL3:
			m_ssl_method = SSLv3_server_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv3_server_method failed!" );
				return( false );
			}
			break;

		case SSL23:
		default:
			m_ssl_method = SSLv23_server_method();
			if ( !m_ssl_method )
			{
				CS_DEBUG( "WARNING: MakeConnection .... SSLv23_server_method failed!" );
				return( false );
			}
			break;
	}

	// wrap some warnings in here
	m_ssl_ctx = SSL_CTX_new ( m_ssl_method );
	if ( !m_ssl_ctx )
		return( false );

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
	if ( SSL_CTX_use_certificate_file( m_ssl_ctx, m_sPemFile.c_str() , SSL_FILETYPE_PEM ) <= 0 )
	{
		CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
		SSLErrors( __FILE__, __LINE__ );
		return( false );
	}

	if ( SSL_CTX_use_PrivateKey_file( m_ssl_ctx, m_sPemFile.c_str(), SSL_FILETYPE_PEM ) <= 0 )
	{
		CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
		SSLErrors( __FILE__, __LINE__ );
		return( false );
	}

	if ( SSL_CTX_set_cipher_list( m_ssl_ctx, m_sCipherType.c_str() ) <= 0 )
	{
		CS_DEBUG( "Could not assign cipher [" << m_sCipherType << "]" );
		return( false );
	}

	//
	// setup the SSL
	m_ssl = SSL_new ( m_ssl_ctx );
	if ( !m_ssl )
		return( false );

	// Call for client Verification
	SSL_set_rfd( m_ssl, m_iReadSock );
	SSL_set_wfd( m_ssl, m_iWriteSock );
	SSL_set_accept_state( m_ssl );

	if ( m_bRequireClientCert )
		SSL_set_verify( m_ssl, SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_PEER, CertVerifyCB );

	return( true );
#else
	return( false );
#endif /* HAVE_LIBSSL */
}

bool Csock::ConnectSSL( const CS_STRING & sBindhost )
{
#ifdef HAVE_LIBSSL
	if ( m_iReadSock == -1 )
		if ( !Connect( sBindhost ) )
			return( false );

	if ( !m_ssl )
		if ( !SSLClientSetup() )
			return( false );

	bool bPass = true;

	if ( m_bBLOCK )
	{
		int fdflags = fcntl ( m_iReadSock, F_GETFL, 0);
		fcntl( m_iReadSock, F_SETFL, fdflags|O_NONBLOCK );
	}

	int iErr = SSL_connect( m_ssl );
	if ( iErr != 1 )
	{
		int sslErr = SSL_get_error( m_ssl, iErr );

		if ( ( sslErr != SSL_ERROR_WANT_READ ) && ( sslErr != SSL_ERROR_WANT_WRITE ) )
			bPass = false;
	} else
		bPass = true;

	if ( m_bBLOCK )
	{
		// unset the flags afterwords, rather then have connect block
		int fdflags = fcntl (m_iReadSock, F_GETFL, 0);
		fdflags &= ~O_NONBLOCK;
		fcntl( m_iReadSock, F_SETFL, fdflags );

	}

	return( bPass );
#else
	return( false );
#endif /* HAVE_LIBSSL */
}

bool Csock::Write( const char *data, int len )
{
	m_sSend.append( data, len );

	if ( m_sSend.empty() )
		return( true );

	if ( m_bBLOCK )
	{
		if ( WriteSelect() != SEL_OK )
			return( false );

	}
	// rate shaping
	u_int iBytesToSend = 0;

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

		int iErr = SSL_write( m_ssl, m_sSSLBuffer.data(), m_sSSLBuffer.length() );

		if ( ( iErr < 0 ) && ( errno == ECONNREFUSED ) )
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
	int bytes = write( m_iWriteSock, m_sSend.data(), iBytesToSend );

	if ( ( bytes == -1 ) && ( errno == ECONNREFUSED ) )
	{
		ConnectionRefused();
		return( false );
	}

	if ( ( bytes <= 0 ) && ( errno != EAGAIN ) )
		return( false );

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

int Csock::Read( char *data, int len )
{
	int bytes = 0;
	memset( (char *)data, '\0', len );

	if ( ( IsReadPaused() ) && ( SslIsEstablished() ) )
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
		bytes = SSL_read( m_ssl, data, len );
	else
#endif /* HAVE_LIBSSL */
		bytes = read( m_iReadSock, data, len );

	if ( bytes == -1 )
	{
		if ( errno == ECONNREFUSED )
			return( READ_CONNREFUSED );

		if ( errno == ETIMEDOUT )
			return( READ_TIMEDOUT );

		if ( ( errno == EINTR ) || ( errno == EAGAIN ) )
			return( READ_EAGAIN );
#ifdef HAVE_LIBSSL
		if ( m_bssl )
		{
			int iErr = SSL_get_error( m_ssl, bytes );
			if ( ( iErr != SSL_ERROR_WANT_READ ) && ( iErr != SSL_ERROR_WANT_WRITE ) )
				return( READ_ERR );
			else
				return( READ_EAGAIN );
		}
#else
		return( READ_ERR );
#endif /* HAVE_LIBSSL */
	}

	m_iBytesRead += (unsigned long long)bytes;

	return( bytes );
}

CS_STRING Csock::GetLocalIP()
{
	if ( !m_sLocalIP.empty() )
		return( m_sLocalIP );

	int iSock = GetSock();

	if ( iSock < 0 )
		return( "" );

	struct sockaddr_in mLocalAddr;
	socklen_t mLocalLen = sizeof(struct sockaddr);
	if ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
		m_sLocalIP = inet_ntoa( mLocalAddr.sin_addr );

	return( m_sLocalIP );
}

CS_STRING Csock::GetRemoteIP()
{
	if ( !m_sRemoteIP.empty() )
		return( m_sRemoteIP );

	int iSock = GetSock();

	if ( iSock <= 0 )
	{
		cerr << "What the hell is wrong with my fd!?" << endl;
		return( "" );
	}

	struct sockaddr_in mRemoteAddr;
	socklen_t mRemoteLen = sizeof(struct sockaddr);
	if ( getpeername( iSock, (struct sockaddr *) &mRemoteAddr, &mRemoteLen ) == 0 )
		m_sRemoteIP = inet_ntoa( mRemoteAddr.sin_addr );

	return( m_sRemoteIP );
}

bool Csock::IsConnected() { return( m_bIsConnected ); }
void Csock::SetIsConnected( bool b ) { m_bIsConnected = b; }

int & Csock::GetRSock() { return( m_iReadSock ); }
void Csock::SetRSock( int iSock ) { m_iReadSock = iSock; }
int & Csock::GetWSock() { return( m_iWriteSock ); }
void Csock::SetWSock( int iSock ) { m_iWriteSock = iSock; }
void Csock::SetSock( int iSock ) { m_iWriteSock = iSock; m_iReadSock = iSock; }
int & Csock::GetSock() { return( m_iReadSock ); }
void Csock::ResetTimer() { m_iTcount = 0; }
void Csock::PauseRead() { m_bPauseRead = true; }
bool Csock::IsReadPaused() { return( m_bPauseRead ); }

void Csock::UnPauseRead()
{
	m_bPauseRead = false;
	ResetTimer();
}

void Csock::SetTimeout( int iTimeout, u_int iTimeoutType )
{
	m_iTimeoutType = iTimeoutType;
	m_itimeout = iTimeout;
}

void Csock::SetTimeoutType( u_int iTimeoutType ) { m_iTimeoutType = iTimeoutType; }
int Csock::GetTimeout() const { return m_itimeout; }
u_int Csock::GetTimeoutType() const { return( m_iTimeoutType ); }

//! returns true if the socket has timed out
bool Csock::CheckTimeout()
{
	if ( IsReadPaused() )
		return( false );

	if ( m_itimeout > 0 )
	{
		if ( m_iTcount >= m_itimeout )
		{
			Timeout();
			return( true );
		}

		m_iTcount++;
	}
	return( false );
}

void Csock::PushBuff( const char *data, int len )
{
	if ( !m_bEnableReadLine )
		return;	// If the ReadLine event is disabled, just ditch here

	u_int iStartPos = ( m_sbuffer.empty() ? 0 : m_sbuffer.length() - 1 );

	if ( data )
		m_sbuffer.append( data, len );

	while( true )
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

CS_STRING & Csock::GetInternalBuffer() { return( m_sbuffer ); }
void Csock::SetMaxBufferThreshold( u_int iThreshold ) { m_iMaxStoredBufferLength = iThreshold; }
u_int Csock::GetMaxBufferThreshold() { return( m_iMaxStoredBufferLength ); }
int Csock::GetType() { return( m_iConnType ); }
void Csock::SetType( int iType ) { m_iConnType = iType; }
const CS_STRING & Csock::GetSockName() { return( m_sSockName ); }
void Csock::SetSockName( const CS_STRING & sName ) { m_sSockName = sName; }
const CS_STRING & Csock::GetHostName() { return( m_shostname ); }
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

int Csock::GetRemotePort()
{
	if ( m_iRemotePort > 0 )
		return( m_iRemotePort );

	int iSock = GetSock();

	if ( iSock >= 0 )
	{
		struct sockaddr_in mAddr;
		socklen_t mLen = sizeof(struct sockaddr);
		if ( getpeername( iSock, (struct sockaddr *) &mAddr, &mLen ) == 0 )
			m_iRemotePort = ntohs( mAddr.sin_port );
	}

	return( m_iRemotePort );
}

int Csock::GetLocalPort()
{
	if ( m_iLocalPort > 0 )
		return( m_iLocalPort );

	int iSock = GetSock();

	if ( iSock >= 0 )
	{
		struct sockaddr_in mLocalAddr;
		socklen_t mLocalLen = sizeof(struct sockaddr);
		if ( getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen ) == 0 )
			m_iLocalPort = ntohs( mLocalAddr.sin_port );
	}

	return( m_iLocalPort );
}

int Csock::GetPort() { return( m_iport ); }
void Csock::SetPort( int iPort ) { m_iport = iPort; }
void Csock::Close() { m_bClosed = true; }
bool Csock::isClosed() { return( m_bClosed ); }
void Csock::BlockIO( bool bBLOCK ) { m_bBLOCK = bBLOCK; }

void Csock::NonBlockingIO()
{
	int fdflags = fcntl ( m_iReadSock, F_GETFL, 0);
	fcntl( m_iReadSock, F_SETFL, fdflags|O_NONBLOCK );

	if ( m_iReadSock != m_iWriteSock )
	{
		fdflags = fcntl ( m_iWriteSock, F_GETFL, 0);
		fcntl( m_iWriteSock, F_SETFL, fdflags|O_NONBLOCK );
	}

	BlockIO( false );
}

//! if this connection type is ssl or not
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
	return( strlen( buf ) );
}

int Csock::CertVerifyCB( int preverify_ok, X509_STORE_CTX *x509_ctx )
{
	/* return 1 always for now, probably want to add some code for cert verification */
	return( 1 );
}

//! Set the SSL method type
void Csock::SetSSLMethod( int iMethod ) { m_iMethod = iMethod; }
int Csock::GetSSLMethod() { return( m_iMethod ); }
void Csock::SetSSLObject( SSL *ssl ) { m_ssl = ssl; }
void Csock::SetCTXObject( SSL_CTX *sslCtx ) { m_ssl_ctx = sslCtx; }
void Csock::SetFullSSLAccept() { m_bFullsslAccept = true; }

SSL_SESSION * Csock::GetSSLSession()
{
	if ( m_ssl )
		return( SSL_get_session( m_ssl ) );

	return( NULL );
}
#endif /* HAVE_LIBSSL */

const CS_STRING & Csock::GetWriteBuffer() { return( m_sSend ); }
void Csock::ClearWriteBuffer() { m_sSend.clear(); }
bool Csock::FullSSLAccept() { return ( m_bFullsslAccept ); }
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
bool Csock::RequiresClientCert() { return( m_bRequireClientCert ); }
void Csock::SetRequiresClientCert( bool bRequiresCert ) { m_bRequireClientCert = bRequiresCert; }

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
	for( vector<CCron *>::size_type a = 0; a < m_vcCrons.size(); a++ )
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

//! insert a newly created cron
void Csock::AddCron( CCron * pcCron )
{
	m_vcCrons.push_back( pcCron );
}

//! delete cron(s) by name
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
		}
	}
}

//! delete cron by idx
void Csock::DelCron( u_int iPos )
{
	if ( iPos < m_vcCrons.size() )
	{
		m_vcCrons[iPos]->Stop();
		CS_Delete( m_vcCrons[iPos] );
		m_vcCrons.erase( m_vcCrons.begin() + iPos );
	}
}
//! delete cron by address
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
void Csock::DisableReadLine() { m_bEnableReadLine = false; }

void Csock::ReachedMaxBuffer()
{
	cerr << "Warning, Max Buffer length Warning Threshold has been hit" << endl;
	cerr << "If you don't care, then set SetMaxBufferThreshold to 0" << endl;
}

int Csock::GetPending()
{
#ifdef HAVE_LIBSSL
	if( m_ssl )
		return( SSL_pending( m_ssl ) );
	else
		return( 0 );
#else
	return( 0 );
#endif /* HAVE_LIBSSL */
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

//! Create the socket
int Csock::SOCKET( bool bListen )
{
	int iRet = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );

	if ( ( iRet > -1 ) && ( bListen ) )
	{
		const int on = 1;

		if ( setsockopt( iRet, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) ) != 0 )
			PERROR( "setsockopt" );

	} else if ( iRet == -1 )
		PERROR( "socket" );

	return( iRet );
}

void Csock::Init( const CS_STRING & sHostname, int iport, int itimeout )
{
#ifdef HAVE_LIBSSL
	m_ssl = NULL;
	m_ssl_ctx = NULL;
#endif /* HAVE_LIBSSL */
	m_iReadSock = -1;
	m_iWriteSock = -1;
	m_itimeout = itimeout;
	m_bssl = false;
	m_bIsConnected = false;
	m_iport = iport;
	m_shostname = sHostname;
	m_iTcount = 0;
	m_sbuffer.clear();
	m_bClosed = false;
	m_bBLOCK = true;
	m_iMethod = SSL23;
	m_sCipherType = "ALL";
	m_iMaxBytes = 0;
	m_iMaxMilliSeconds = 0;
	m_iLastSendTime = 0;
	m_iLastSend = 0;
	m_bFullsslAccept = false;
	m_bsslEstablished = false;
	m_bEnableReadLine = false;
	m_bRequireClientCert = false;
	m_iMaxStoredBufferLength = 1024;
	m_iConnType = INBOUND;
	m_iRemotePort = 0;
	m_iLocalPort = 0;
	m_iBytesRead = 0;
	m_iBytesWritten = 0;
	m_iStartTime = millitime();
	m_bPauseRead = false;
	m_iTimeoutType = TMO_ALL;
}
