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

#ifndef _HAS_CSOCKET_
#define _HAS_CSOCKET_
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>

#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
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


#ifndef CS_STRING
#	ifdef _HAS_CSTRING_
#		define CS_STRING Cstring
#	else
#		define CS_STRING string
#	endif	/* _HAS_CSTRING_ */
#endif /* CS_STRING */

#ifndef CS_DEBUG
#ifdef __DEBUG__
#	define CS_DEBUG( f ) cerr << __FILE__ << ":" << __LINE__ << " " << f << endl
#else
#	define CS_DEBUG(f)	(void)0
#endif /* __DEBUG__ */
#endif /* CS_DEBUG */

#ifdef __DEBUG__
#	define PERROR( f ) __Perror( f )
#else
#	define PERROR( f )	(void)0
#endif /* __DEBUG__ */

using namespace std;

#ifndef _NO_CSOCKET_NS // some people may not want to use a namespace
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */
	const u_int CS_BLOCKSIZE = 4096;
	template <class T> inline void CS_Delete( T * & p ) { if( p ) { delete p; p = NULL; } }

	// wrappers for FD_SET and such to work in templates
	inline void TFD_ZERO( fd_set *set )
	{
		FD_ZERO( set );
	}

	inline void TFD_SET( int iSock, fd_set *set )
	{
		FD_SET( iSock, set );
	}

	inline bool TFD_ISSET( int iSock, fd_set *set )
	{
		if ( FD_ISSET( iSock, set ) )
		return( true );
		
		return( false );
	}

	inline void TFD_CLR( int iSock, fd_set *set )
	{
		FD_CLR( iSock, set );
	}

	inline void __Perror( const CS_STRING & s )
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
	inline unsigned long long millitime()
	{
		struct timeval tv;
		unsigned long long iTime = 0;
		gettimeofday( &tv, NULL );
		iTime = (unsigned long long )tv.tv_sec * 1000;
		iTime += ( (unsigned long long)tv.tv_usec / 1000 );		
		return( iTime );
	}

#ifdef HAVE_LIBSSL
	inline void SSLErrors()
	{
		unsigned long iSSLError = 0;
		while( ( iSSLError = ERR_get_error() ) != 0 )
		{
			char szError[512];
			memset( (char *) szError, '\0', 512 );
			ERR_error_string_n( iSSLError, szError, 511 );
			if ( strlen( szError ) > 0 )
				CS_DEBUG( szError );
		}
	}
#endif /* HAVE_LIBSSL */

	inline bool GetHostByName( const CS_STRING & sHostName, struct in_addr *paddr )
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
			bcopy( hent->h_addr_list[0], &paddr->s_addr, 4 );

		return( bRet );
	}

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
		CCron() 
		{
			m_iCycles = 0;
			m_iMaxCycles = 0;
			m_bActive = true;
			m_iTime	= 0;
			m_iTimeSequence = 60;
			m_bPause = false;
		}
		virtual ~CCron() {}
		
		//! This is used by the Job Manager, and not you directly
		void run() 
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
		void StartMaxCycles( int TimeSequence, u_int iMaxCycles )
		{
			m_iTimeSequence = TimeSequence;
			m_iTime = time( NULL ) + m_iTimeSequence;
			m_iMaxCycles = iMaxCycles;
		}

		//! starts and runs infinity amount of times
		void Start( int TimeSequence )
		{
			m_iTimeSequence = TimeSequence;
			m_iTime = time( NULL ) + m_iTimeSequence;
			m_iMaxCycles = 0;
		}

		//! call this to turn off your cron, it will be removed
		void Stop()
		{
			m_bActive = false;
		}

		//! pauses excution of your code in RunJob
		void Pause()
		{
			m_bPause = true;
		}

		//! removes the pause on RunJon
		void UnPause()
		{
			m_bPause = false;
		}

		int GetInterval() const { return( m_iTimeSequence ); }
		u_int GetMaxCycles() const { return( m_iMaxCycles ); }
		u_int GetCyclesLeft() const { return( ( m_iMaxCycles > m_iCycles ? ( m_iMaxCycles - m_iCycles ) : 0 ) ); }

		//! returns true if cron is active
		bool isValid() { return( m_bActive ); }

		const CS_STRING & GetName() const { return( m_sName ); }
		void SetName( const CS_STRING & sName ) { m_sName = sName; }
		
	protected:

		//! this is the method you should override
		virtual void RunJob() { CS_DEBUG( "This should be overriden" ); }
		
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
		Csock( int itimeout = 60 ) 
		{ 
			Init( "", 0, itimeout ); 
		}
		/**
		* Advanced constructor, for creating a simple connection
		*
		* sHostname the hostname your are connecting to
		* iport the port you are connectint to
		* itimeout how long to wait before ditching the connection, default is 60 seconds
		*/
		Csock( const CS_STRING & sHostname, int iport, int itimeout = 60 ) 
		{
			Init( sHostname, iport, itimeout );
		}
		
		// override this for accept sockets
		virtual Csock *GetSockObj( const CS_STRING & sHostname, int iPort ) 
		{ 
			return( NULL ); 
		}
		
		virtual ~Csock()
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
			for( unsigned int i = 0; i < m_vcCrons.size(); i++ )
				CS_Delete( m_vcCrons[i] );
		}

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

		Csock & operator<<( const CS_STRING & s ) 
		{
			Write( s );
			return( *this );
		}

		Csock & operator<<( ostream & ( *io )( ostream & ) )
		{
			Write( "\r\n" );
			return( *this );
		}

		Csock & operator<<( int i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( unsigned int i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( long i ) 
		{ 
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( unsigned long i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( unsigned long long i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( float i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}
		Csock & operator<<( double i ) 
		{
			stringstream s;
			s << i;
			Write( s.str() );
			return( *this );
		}		

		/**
		* Create the connection
		*
		* \param sBindHost the ip you want to bind to locally
		* \return true on success
		*/
		virtual bool Connect( const CS_STRING & sBindHost = "" )
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
		
		/**
		* WriteSelect on this socket
		* Only good if JUST using this socket, otherwise use the TSocketManager
		*/
		virtual int WriteSelect()
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

		/**
		* ReadSelect on this socket
		* Only good if JUST using this socket, otherwise use the TSocketManager
		*/
		virtual int ReadSelect()
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
				
		/**
		* Listens for connections
		*
		* \param iPort the port to listen on
		* \param iMaxConns the maximum amount of connections to allow
		*/
		virtual bool Listen( int iPort, int iMaxConns = SOMAXCONN, const CS_STRING & sBindHost = "", u_int iTimeout = 0 )
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
		
		//! Accept an inbound connection, this is used internally
		virtual int Accept( CS_STRING & sHost, int & iRPort )
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
		
		//! Accept an inbound SSL connection, this is used internally and called after Accept
		virtual bool AcceptSSL()
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

			SSLErrors();

#endif /* HAVE_LIBSSL */
			
			return( false );	
		}

		//! This sets up the SSL Client, this is used internally
		virtual bool SSLClientSetup()
		{
#ifdef HAVE_LIBSSL		
			m_bssl = true;
			FREE_SSL();
			FREE_CTX();

			SSLeay_add_ssl_algorithms();
		
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

			SSL_load_error_strings ();
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
					SSLErrors();
				}
			
				if ( SSL_CTX_use_PrivateKey_file( m_ssl_ctx, m_sPemFile.c_str(), SSL_FILETYPE_PEM ) <= 0 )
				{
					CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
					SSLErrors();
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

		//! This sets up the SSL Server, this is used internally
		virtual bool SSLServerSetup()
		{
#ifdef HAVE_LIBSSL
			m_bssl = true;
			FREE_SSL();			
			FREE_CTX();
					
			SSLeay_add_ssl_algorithms();
		
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

			SSL_load_error_strings ();
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
				SSLErrors();
				return( false );
			}
			
			if ( SSL_CTX_use_PrivateKey_file( m_ssl_ctx, m_sPemFile.c_str(), SSL_FILETYPE_PEM ) <= 0 )
			{
				CS_DEBUG( "Error with PEM file [" << m_sPemFile << "]" );
				SSLErrors();
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

		/**
		* Create the SSL connection
		*
		* \param sBindhost the ip you want to bind to locally
		* \return true on success
		*/		
		virtual bool ConnectSSL( const CS_STRING & sBindhost = "" )
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


		/**
		* Write data to the socket
		* if not all of the data is sent, it will be stored on
		* an internal buffer, and tried again with next call to Write
		* if the socket is blocking, it will send everything, its ok to check ernno after this (nothing else is processed)
		*
		* \param data the data to send
		* \param len the length of data
		* 
		*/
		virtual bool Write( const char *data, int len )
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
						SSLErrors();
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
		
		/**
		* convience function
		* @see Write( const char *, int )
		*/
		virtual bool Write( const CS_STRING & sData )
		{
			return( Write( sData.c_str(), sData.length() ) );
		}

		/**
		* Read from the socket
		* Just pass in a pointer, big enough to hold len bytes
		*
		* \param data the buffer to read into
		* \param len the size of the buffer
		*
		* \return
		* Returns READ_EOF for EOF
		* Returns READ_ERR for ERROR
		* Returns READ_EAGAIN for Try Again ( EAGAIN )
		* Returns READ_CONNREFUSED for connection refused
		* Returns READ_TIMEDOUT for a connection that timed out at the TCP level
		* Otherwise returns the bytes read into data
		*/
		virtual int Read( char *data, int len )
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

		CS_STRING GetLocalIP()
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

		CS_STRING GetRemoteIP()
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

		//! Tells you if the socket is connected
		virtual bool IsConnected() { return( m_bIsConnected ); }
		//! Sets the sock, telling it its connected (internal use only)
		virtual void SetIsConnected( bool b ) { m_bIsConnected = b; }	

		//! returns a reference to the sock
		int & GetRSock() { return( m_iReadSock ); }
		void SetRSock( int iSock ) { m_iReadSock = iSock; }
		int & GetWSock() { return( m_iWriteSock ); }
		void SetWSock( int iSock ) { m_iWriteSock = iSock; }
		
		void SetSock( int iSock ) { m_iWriteSock = iSock; m_iReadSock = iSock; }
		int & GetSock() { return( m_iReadSock ); }

		//! resets the time counter
		void ResetTimer() { m_iTcount = 0; }
		
		//! will pause/unpause reading on this socket
		void PauseRead() { m_bPauseRead = true; }
		void UnPauseRead() 
		{ 
			m_bPauseRead = false; 
			ResetTimer();
		}
		bool IsReadPaused() { return( m_bPauseRead ); }
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
		void SetTimeout( int iTimeout, u_int iTimeoutType = TMO_ALL ) 
		{ 
			m_iTimeoutType = iTimeoutType;
			m_itimeout = iTimeout; 
		}
		void SetTimeoutType( u_int iTimeoutType ) { m_iTimeoutType = iTimeoutType; }
		int GetTimeout() const { return m_itimeout; }
		u_int GetTimeoutType() const { return( m_iTimeoutType ); }
		
		//! returns true if the socket has timed out
		virtual bool CheckTimeout()
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
		
		/**
		* pushes data up on the buffer, if a line is ready
		* it calls the ReadLine event
		*/
		virtual void PushBuff( const char *data, int len )
		{
			if ( !m_bEnableReadLine )
				return;	// If the ReadLine event is disabled, just ditch here

			if ( data )
				m_sbuffer.append( data, len );
			
			while( true )
			{
				u_int iFind = m_sbuffer.find( "\n" );
			
				if ( iFind != CS_STRING::npos )
				{
					CS_STRING sBuff = m_sbuffer.substr( 0, iFind + 1 );	// read up to(including) the newline
					m_sbuffer.erase( 0, iFind + 1 );					// erase past the newline
					ReadLine( sBuff );

				} else
					break;
			}

			if ( ( m_iMaxStoredBufferLength > 0 ) && ( m_sbuffer.length() > m_iMaxStoredBufferLength ) )
				ReachedMaxBuffer(); // call the max read buffer event

		}

		//! This gives access to the internal buffer, if your
		//! not going to use GetLine(), then you may want to clear this out
		//! (if its binary data and not many '\n'
		CS_STRING & GetInternalBuffer() { return( m_sbuffer ); }	
		//! sets the max buffered threshold when enablereadline() is enabled
		void SetMaxBufferThreshold( u_int iThreshold ) { m_iMaxStoredBufferLength = iThreshold; }
		u_int GetMaxBufferThreshold() { return( m_iMaxStoredBufferLength ); }

		//! Returns the connection type from enum eConnType
		int GetType() { return( m_iConnType ); }
		void SetType( int iType ) { m_iConnType = iType; }
		
		//! Returns a reference to the socket name
		const CS_STRING & GetSockName() { return( m_sSockName ); }
		void SetSockName( const CS_STRING & sName ) { m_sSockName = sName; }
		
		//! Returns a reference to the host name
		const CS_STRING & GetHostName() { return( m_shostname ); }
		void SetHostName( const CS_STRING & sHostname ) { m_shostname = sHostname; }


		//! Gets the starting time of this socket
		unsigned long long GetStartTime() const { return( m_iStartTime ); } 
		//! Resets the start time
		void ResetStartTime() { m_iStartTime = 0; }

		//! Gets the amount of data read during the existence of the socket
		unsigned long long GetBytesRead() const { return( m_iBytesRead ); }
		void ResetBytesRead() { m_iBytesRead = 0; }
		
		//! Gets the amount of data written during the existence of the socket
		unsigned long long GetBytesWritten() const { return( m_iBytesWritten ); }
		void ResetBytesWritten() { m_iBytesWritten = 0; }


		//! Get Avg Read Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
		double GetAvgRead( unsigned long long iSample = 1000 )
		{
			unsigned long long iDifference = ( millitime() - m_iStartTime );
			
			if ( ( m_iBytesRead == 0 ) || ( iSample > iDifference ) )
				return( (double)m_iBytesRead );

			return( ( (double)m_iBytesRead / ( (double)iDifference / (double)iSample ) ) );
		}

		//! Get Avg Write Speed in sample milliseconds (default is 1000 milliseconds or 1 second)
		double GetAvgWrite( unsigned long long iSample = 1000 )
		{
			unsigned long long iDifference = ( millitime() - m_iStartTime );
			
			if ( ( m_iBytesWritten == 0 ) || ( iSample > iDifference ) )
				return( (double)m_iBytesWritten );

			return( ( (double)m_iBytesWritten / ( (double)iDifference / (double)iSample ) ) );
		}
			

		//! Returns the remote port
		int GetRemotePort() 
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

		//! Returns the local port
		int GetLocalPort() 
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

		//! Returns the port
		int GetPort() { return( m_iport ); }
		void SetPort( int iPort ) { m_iport = iPort; }
		
		//! just mark us as closed, the parent can pick it up
		void Close() { m_bClosed = true; }
		//! returns true if the socket is closed
		bool isClosed() { return( m_bClosed ); }
		
		//! Set rather to NON Blocking IO on this socket, default is true
		void BlockIO( bool bBLOCK ) { m_bBLOCK = bBLOCK; }
		
		//! Use this to change your fd's to blocking or none blocking
		void NonBlockingIO()
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
		bool GetSSL() { return( m_bssl ); }
		void SetSSL( bool b ) { m_bssl = b; }
		
#ifdef HAVE_LIBSSL
		//! Set the cipher type ( openssl cipher [to see ciphers available] )
		void SetCipher( const CS_STRING & sCipher ) { m_sCipherType = sCipher; }
		const CS_STRING & GetCipher() { return( m_sCipherType ); }
		
		//! Set the pem file location
		void SetPemLocation( const CS_STRING & sPemFile ) { m_sPemFile = sPemFile; }
		const CS_STRING & GetPemLocation() { return( m_sPemFile ); }
		void SetPemPass( const CS_STRING & sPassword ) { m_sPemPass = sPassword; }
		const CS_STRING & GetPemPass() const { return( m_sPemPass ); }
		static int PemPassCB( char *buf, int size, int rwflag, void *pcSocket )
		{
			Csock *pSock = (Csock *)pcSocket;
			const CS_STRING & sPassword = pSock->GetPemPass();
			memset( buf, '\0', size );
			strncpy( buf, sPassword.c_str(), size );
			buf[size-1] = '\0';
			return( strlen( buf ) );
		}
		static int CertVerifyCB( int preverify_ok, X509_STORE_CTX *x509_ctx )
		{
			/* return 1 always for now, probably want to add some code for cert verification */
			return( 1 );
		}
		
		//! Set the SSL method type
		void SetSSLMethod( int iMethod ) { m_iMethod = iMethod; }
		int GetSSLMethod() { return( m_iMethod ); }
		
		void SetSSLObject( SSL *ssl ) { m_ssl = ssl; }
		void SetCTXObject( SSL_CTX *sslCtx ) { m_ssl_ctx = sslCtx; }
		void SetFullSSLAccept() { m_bFullsslAccept = true; }
		SSL_SESSION * GetSSLSession() { return( SSL_get_session( m_ssl ) ); }
#endif /* HAVE_LIBSSL */
		
		//! Get the send buffer
		const CS_STRING & GetWriteBuffer() { return( m_sSend ); }
		void ClearWriteBuffer() { m_sSend.clear(); }

		//! is SSL_accept finished ?
		bool FullSSLAccept() { return ( m_bFullsslAccept ); }
		//! is the ssl properly finished (from write no error)
		bool SslIsEstablished() { return ( m_bsslEstablished ); }

		//! Use this to bind this socket to inetd
		bool ConnectInetd( bool bIsSSL = false, const CS_STRING & sHostname = "" )
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

		//! Tie this guy to an existing real file descriptor
		bool ConnectFD( int iReadFD, int iWriteFD, const CS_STRING & sName, bool bIsSSL = false, ETConn eDirection = INBOUND )
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

		//! Get the peer's X509 cert
#ifdef HAVE_LIBSSL
		X509 *getX509()
		{
			if ( m_ssl )
				return( SSL_get_peer_certificate( m_ssl ) );

			return( NULL );
		}

		//!
		//! Returns The Peers Public Key
		//!
		CS_STRING GetPeerPubKey()
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
		bool RequiresClientCert() { return( m_bRequireClientCert ); }
		void SetRequiresClientCert( bool bRequiresCert ) { m_bRequireClientCert = bRequiresCert; }

#endif /* HAVE_LIBSSL */

		//! Set The INBOUND Parent sockname
		virtual void SetParentSockName( const CS_STRING & sParentName ) { m_sParentName = sParentName; }
		const CS_STRING & GetParentSockName() { return( m_sParentName ); }
		
		/*
		* sets the rate at which we can send data
		* \param iBytes the amount of bytes we can write
		* \param iMilliseconds the amount of time we have to rate to iBytes
		*/
		virtual void SetRate( u_int iBytes, unsigned long long iMilliseconds )
		{
			m_iMaxBytes = iBytes;
			m_iMaxMilliSeconds = iMilliseconds;
		}
		
		u_int GetRateBytes() { return( m_iMaxBytes ); }
		unsigned long long GetRateTime() { return( m_iMaxMilliSeconds ); }
		
		
		//! This has a garbage collecter, and is used internall to call the jobs
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
		
		//! insert a newly created cron
		virtual void AddCron( CCron * pcCron )
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
		void EnableReadLine() { m_bEnableReadLine = true; }
		void DisableReadLine() { m_bEnableReadLine = false; }

		/**
		 * Override these functions for an easy interface when using the Socket Manager
		 * Don't bother using these callbacks if you are using this class directly (without Socket Manager)
		 * as the Socket Manager calls most of these callbacks
		 * This WARNING event is called when your buffer for readline exceeds the warning threshold
		 * and triggers this event. Either Override it and do nothing, or @SetMaxBufferThreshold( int )
		 * This event will only get called if m_bEnableReadLine is enabled
		 */
		virtual void ReachedMaxBuffer()
		{
			cerr << "Warning, Max Buffer length Warning Threshold has been hit" << endl;
			cerr << "If you don't care, then set SetMaxBufferThreshold to 0" << endl;
		}
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
		virtual bool ConnectionFrom( const CS_STRING & sHost, int iPort ) { return( true ); }

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

		//! return the data imediatly ready for read
		virtual int GetPending()
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

		//////////////////////////////////////////////////	
			
	private:
		int			m_iReadSock, m_iWriteSock, m_itimeout, m_iport, m_iConnType, m_iTcount, m_iMethod, m_iRemotePort, m_iLocalPort;
		bool		m_bssl, m_bIsConnected, m_bClosed, m_bBLOCK, m_bFullsslAccept;
		bool		m_bsslEstablished, m_bEnableReadLine, m_bRequireClientCert, m_bPauseRead;
		CS_STRING	m_shostname, m_sbuffer, m_sSockName, m_sPemFile, m_sCipherType, m_sParentName;
		CS_STRING	m_sSend, m_sSSLBuffer, m_sPemPass, m_sLocalIP, m_sRemoteIP;

		unsigned long long	m_iMaxMilliSeconds, m_iLastSendTime, m_iBytesRead, m_iBytesWritten, m_iStartTime;
		unsigned int		m_iMaxBytes, m_iLastSend, m_iMaxStoredBufferLength, m_iTimeoutType;
		
		struct sockaddr_in 	m_address;
				
#ifdef HAVE_LIBSSL
		SSL 				*m_ssl;
		SSL_CTX				*m_ssl_ctx;
		SSL_METHOD			*m_ssl_method;

		virtual void FREE_SSL()
		{
			if ( m_ssl )
			{
				SSL_shutdown( m_ssl );
				SSL_free( m_ssl );
			}
			m_ssl = NULL;
		}

		virtual void FREE_CTX()
		{
			if ( m_ssl_ctx )
				SSL_CTX_free( m_ssl_ctx );

			m_ssl_ctx = NULL;
		}

#endif /* HAVE_LIBSSL */

		vector<CCron *>		m_vcCrons;

		//! Create the socket
		virtual int SOCKET( bool bListen = false )
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

		virtual void Init( const CS_STRING & sHostname, int iport, int itimeout = 60 )
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
	};

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
	class TSocketManager : public vector<T *>
	{
	public:
		TSocketManager() : vector<T *>()
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
			for( unsigned int i = 0; i < this->size(); i++ )
				CS_Delete( (*this)[i] );

			vector<T *>::clear();
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
		* \return true on success
		*/
		virtual bool Connect( const CS_STRING & sHostname, int iPort , const CS_STRING & sSockName, int iTimeout = 60, bool isSSL = false, const CS_STRING & sBindHost = "", T *pcSock = NULL )
		{
			// create the new object
			if ( !pcSock )
				pcSock = new T( sHostname, iPort, iTimeout );
			else
			{
				pcSock->SetHostName( sHostname );
				pcSock->SetPort( iPort );
				pcSock->SetTimeout( iTimeout );
			}
			
			// make it NON-Blocking IO
			pcSock->BlockIO( false );
			
			if ( !pcSock->Connect( sBindHost ) )
			{
				if ( errno == ECONNREFUSED )
					pcSock->ConnectionRefused();
				
				CS_Delete( pcSock );
				return( false );
			}
		
#ifdef HAVE_LIBSSL
			if ( isSSL )
			{
				if ( !pcSock->ConnectSSL() )
				{
					if ( errno == ECONNREFUSED )
						pcSock->ConnectionRefused();

					CS_Delete( pcSock );
					return( false );
				}
			}
#endif /* HAVE_LIBSSL */
			
			AddSock( pcSock, sSockName );
			return( true );
		}

		/**
		* Create a listening socket
		* 
		* \param iPort the port to listen on
		* \param sSockName the name of the socket
		* \param isSSL if the sockets created require an ssl layer
		* \param iMaxConns the maximum amount of connections to accept
		* \return true on success
		*/
		virtual T * ListenHost( int iPort, const CS_STRING & sSockName, const CS_STRING & sBindHost, int isSSL = false, int iMaxConns = SOMAXCONN, T *pcSock = NULL, u_int iTimeout = 0 )
		{
			if ( !pcSock )
				pcSock = new T();

			pcSock->BlockIO( false );

			pcSock->SetSSL( isSSL );

			if ( pcSock->Listen( iPort, iMaxConns, sBindHost, iTimeout ) )
			{
				AddSock( pcSock, sSockName );
				return( pcSock );
			}
			CS_Delete( pcSock );
			return( NULL );
		}
		
		virtual bool ListenAll( int iPort, const CS_STRING & sSockName, int isSSL = false, int iMaxConns = SOMAXCONN, T *pcSock = NULL, u_int iTimeout = 0 )
		{
			return( ListenHost( iPort, sSockName, "", isSSL, iMaxConns, pcSock, iTimeout ) );
		}

		/*
		 * @return the port number being listened on
		 */
		virtual u_short ListenRand( const CS_STRING & sSockName, const CS_STRING & sBindHost, int isSSL = false, int iMaxConns = SOMAXCONN, T *pcSock = NULL, u_int iTimeout = 0 )
		{
			u_short iPort = 0;
			T *pNewSock = ListenHost( 0,  sSockName, sBindHost, isSSL, iMaxConns, pcSock, iTimeout );
			if ( pNewSock )
			{
				int iSock = pNewSock->GetSock();	

				if ( iSock < 0 )
				{
					CS_DEBUG( "Failed to attain a valid file descriptor" );
					pNewSock->Close();
					return( 0 );
				}

				struct sockaddr_in mLocalAddr;
				socklen_t mLocalLen = sizeof(struct sockaddr);
				getsockname( iSock, (struct sockaddr *) &mLocalAddr, &mLocalLen );
			
				iPort = ntohs( mLocalAddr.sin_port );
			}

			return( iPort );
		}
		virtual u_short ListenAllRand( const CS_STRING & sSockName, int isSSL = false, int iMaxConns = SOMAXCONN, T *pcSock = NULL, u_int iTimeout = 0 )
		{
			return( ListenRand( sSockName, "", isSSL, iMaxConns, pcSock, iTimeout ) );
		}

		/*
		* Best place to call this class for running, all the call backs are called
		* You should through this in your main while loop (long as its not blocking)
		* all the events are called as needed
		*/ 
		virtual void Loop ()
		{
			map<T *, EMessages> mpeSocks;
			Select( mpeSocks );
			set<T *> spReadySocks;
			
			switch( m_errno )
			{
				case SUCCESS:
				{
					for( typename map<T *, EMessages>::iterator itSock = mpeSocks.begin(); itSock != mpeSocks.end(); itSock++ )
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
									pcSock->SockError( errno );
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

									pcSock->PushBuff( buff, bytes );
									pcSock->ReadData( buff, bytes );
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
		virtual T * FindSockByRemotePort( int iPort )
		{
			for( unsigned int i = 0; i < this->size(); i++ )
			{
				if ( (*this)[i]->GetRemotePort() == iPort )
					return( (*this)[i] );
			}

			return( NULL );
		}

		//! returns a pointer to the FIRST sock found by port or NULL on no match
		virtual T * FindSockByLocalPort( int iPort )
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

		virtual vector<T *> FindSocksByName( const CS_STRING & sName )
		{
			vector<T *> vpSocks;
			
			for( unsigned int i = 0; i < this->size(); i++ )
				if ( (*this)[i]->GetSockName() == sName )
					vpSocks.push_back( (*this)[i] );

			return( vpSocks );
		}
		
		//! returns a vector of pointers to socks with sHostname as being connected
		virtual vector<T *> FindSocksByRemoteHost( const CS_STRING & sHostname )
		{
			vector<T *> vpSocks;
			
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

		vector<CCron *> & GetCrons() { return( m_vcCrons ); }
		
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
			if ( (*this)[iPos]->IsConnected() )
				(*this)[iPos]->Disconnected(); // only call disconnected event if connected event was called (IE IsConnected was set)
					
			CS_Delete( (*this)[iPos] );
			this->erase( this->begin() + iPos );
		}

	private:
		/**
		* fills a map of socks to a message for check
		* map is empty if none are ready, check GetErrno() for the error, if not SUCCESS Select() failed
		* each struct contains the socks error
		* @see GetErrno()
		*/
		virtual void Select( map<T *, EMessages> & mpeSocks )
		{		
			mpeSocks.clear();
			struct timeval tv;
			fd_set rfds, wfds;
			
			tv.tv_sec = 0;
			tv.tv_usec = m_iSelectWait;
			
			u_int iQuickReset = 1000;
			if ( m_iSelectWait == 0 )
				iQuickReset = 0;

			TFD_ZERO( &rfds );						
			TFD_ZERO( &wfds );

			// before we go any further, Process work needing to be done on the job
			for( unsigned int i = 0; i < this->size(); i++ )
			{
				if ( (*this)[i]->isClosed() )
					DelSock( i-- ); // close any socks that have requested it
				else
					(*this)[i]->Cron(); // call the Cron handler here
			}

			bool bHasWriteable = false;

			for( unsigned int i = 0; i < this->size(); i++ )
			{

				T *pcSock = (*this)[i];

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
		
				if ( ( pcSock->GetSSL() ) && ( pcSock->GetType() != Csock::LISTENER ) )
				{
					if ( ( pcSock->GetPending() > 0 ) && ( !pcSock->IsReadPaused() ) )
						SelectSock( mpeSocks, SUCCESS, pcSock );
				}
			}

			// old fashion select, go fer it
			int iSel;

			if ( !mpeSocks.empty() )
				tv.tv_usec = iQuickReset;	// this won't be a timeout, 1 ms pause to see if anything else is ready (IE if there is SSL data pending, don't wait too long)
				
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
						int port;
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
									stringstream s;
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
		virtual void SelectSock( map<T *, EMessages> & mpeSocks, EMessages eErrno, T * pcSock )
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

		EMessages				m_errno;
		vector<CCron *>			m_vcCrons;
		unsigned long long		m_iCallTimeouts;	
		u_int					m_iSelectWait;
	};

	//! basic socket class
	typedef TSocketManager<Csock> CSocketManager;

#ifndef _NO_CSOCKET_NS
};
#endif /* _NO_CSOCKET_NS */

#endif /* _HAS_CSOCKET_ */

