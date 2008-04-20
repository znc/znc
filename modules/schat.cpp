/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Secure chat system
 * Author: imaginos <imaginos@imaginos.net>
 */

#define REQUIRESSL

#include "User.h"
#include "znc.h"
#include <sstream>

using std::pair;
using std::stringstream;

class CSChat;

class CRemMarkerJob : public CTimer
{
public:
	CRemMarkerJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel,
			const CString& sDescription )
		: CTimer( pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CRemMarkerJob() {}
	void SetNick( const CString & sNick )
	{
		m_sNick = sNick;
	}

protected:
	virtual void RunJob();
	CString		m_sNick;
};

class CSChatSock : public Csock
{
public:
	CSChatSock( CSChat *pMod ) : Csock()
	{
		m_pModule = pMod;
	}
	CSChatSock( int itimeout = 60 ) : Csock( itimeout )
	{
		m_pModule = NULL;
		EnableReadLine();
	}
	CSChatSock( const CS_STRING & sHost, u_short iPort, int iTimeout = 60 )
		: Csock( sHost, iPort, iTimeout )
	{
		m_pModule = NULL;
		EnableReadLine();
	}

	virtual Csock *GetSockObj( const CS_STRING & sHostname, u_short iPort )
	{
		CSChatSock *p = new CSChatSock( sHostname, iPort );
		p->SetModule( m_pModule );
		p->SetChatNick( m_sChatNick );
		p->SetSockName( GetSockName() + "::" + m_sChatNick );
		return( p );
	}

	virtual bool ConnectionFrom( const CS_STRING & sHost, u_short iPort )
	{
		Close();	// close the listener after the first connection
		return( true );
	}

	virtual void Connected();
	virtual void Timeout();

	void SetModule( CSChat *p )
	{
		m_pModule = p;
	}
	void SetChatNick( const CString & sNick )
	{
		m_sChatNick = sNick;
	}

	const CString & GetChatNick() const { return( m_sChatNick ); }

	virtual void ReadLine( const CS_STRING & sLine );
	virtual void Disconnected();

	virtual void AddLine( const CString & sLine )
	{
		m_vBuffer.insert( m_vBuffer.begin(), sLine );
		if ( m_vBuffer.size() > 200 )
			m_vBuffer.pop_back();
	}

	virtual void DumpBuffer()
	{
		for( vector<CS_STRING>::reverse_iterator it = m_vBuffer.rbegin(); it != m_vBuffer.rend(); it++ )
			ReadLine( *it );

		m_vBuffer.clear();
	}

private:
	CSChat 					*m_pModule;
	CString					m_sChatNick;
	vector<CS_STRING>		m_vBuffer;
};

class CSChat : public CModule
{
public:
	MODCONSTRUCTOR(CSChat) {}
	virtual ~CSChat() { CleanSocks(); }

	virtual bool OnLoad( const CString & sArgs, CString & sMessage )
	{
		m_sPemFile = sArgs;

		if ( m_sPemFile.empty() )
		{
			m_sPemFile = CZNC::Get().GetPemLocation();
		}

		if (!CFile::Exists(m_sPemFile)) {
			sMessage = "Unable to load pem file [" + m_sPemFile + "]";
			return false;
		}

		return true;
	}

	virtual void OnUserAttached()
	{
		CString sName = "SCHAT::" + m_pUser->GetUserName();
		for( u_int a = 0; a < m_pManager->size(); a++ )
		{
			if ((*m_pManager)[a]->GetSockName() != sName.c_str() ||
					((*m_pManager)[a]->GetType() == CSChatSock::LISTENER ))
				continue;

			CSChatSock *p = (CSChatSock *)(*m_pManager)[a];
			p->DumpBuffer();
		}
	}

	void CleanSocks()
	{
		CString sName = "SCHAT::" + m_pUser->GetUserName();
		for( u_int a= 0; a < m_pManager->size(); a++ )
		{
			if ((*m_pManager)[a]->GetSockName() == sName)
				m_pManager->DelSock(a--);
		}
	}

	virtual EModRet OnUserRaw( CString & sLine )
	{
		if ( sLine.CaseCmp( "schat ", 6 ) == 0 )
		{
			OnModCommand( "chat " + sLine.substr(6) );
			return( HALT );

		} else if ( sLine.CaseCmp( "schat" ) == 0 )
		{
			PutModule( "SChat User Area ..." );
			OnModCommand( "help" );
			return( HALT );

		}

		return( CONTINUE );
	}

	virtual void OnModCommand( const CString& sCommand )
	{
		CString sCom, sArgs;

		sCom = sCommand.Token(0);
		sArgs = sCommand.Token(1, true);

		if ( (sCom.CaseCmp("chat") == 0 ) && ( !sArgs.empty()) ) {
			CString sSockName = "SCHAT::" + m_pUser->GetUserName();
			CString sNick = "(s)" + sArgs;
			for( u_int a= 0; a < m_pManager->size(); a++ )
			{
				if ( (*m_pManager)[a]->GetSockName() != sSockName )
					continue;

				CSChatSock *pSock = (CSChatSock *)(*m_pManager)[a];
				if ( pSock->GetChatNick().CaseCmp(sNick.c_str()) == 0 )
				{
					PutModule( "Already Connected to [" + sArgs + "]" );
					return;
				}
			}

			CSChatSock *pSock = new CSChatSock;
			pSock->SetCipher( "HIGH" );
			pSock->SetPemLocation( m_sPemFile );
			pSock->SetModule( this );
			pSock->SetChatNick( sNick );

			u_short iPort = m_pManager->ListenRand( sSockName, m_pUser->GetLocalIP(), true,
					SOMAXCONN, pSock, 60 );

			if ( iPort == 0 )
			{
				PutModule( "Failed to start chat!" );
				return;
			}

			stringstream s;
			s << "PRIVMSG " << sArgs << " :\001";
			s << "DCC SCHAT chat ";
			s << CUtils::GetLongIP( m_pUser->GetLocalIP() );
			s << " " << iPort << "\001";

			PutIRC( s.str() );

		} else if ( sCom.CaseCmp( "list" ) == 0 )
		{
			CString sName = "SCHAT::" + m_pUser->GetUserName();
			CTable Table;
			Table.AddColumn( "Nick" );
			Table.AddColumn( "Created" );
			Table.AddColumn( "Host" );
			Table.AddColumn( "Port" );
			Table.AddColumn( "Status" );
			Table.AddColumn( "Cipher" );
			for( u_int a= 0; a < m_pManager->size(); a++ )
			{
				if ( (*m_pManager)[a]->GetSockName() != sName )
					continue;

				Table.AddRow();

				CSChatSock *pSock = (CSChatSock *)(*m_pManager)[a];
				Table.SetCell( "Nick", pSock->GetChatNick() );
				unsigned long long iStartTime = pSock->GetStartTime();
				time_t iTime = iStartTime / 1000;
				char *pTime = ctime( &iTime );
				if ( pTime )
				{
					CString sTime = pTime;
					sTime.Trim();
					Table.SetCell( "Created", sTime );
				}

				if ( pSock->GetType() != CSChatSock::LISTENER )
				{
					Table.SetCell( "Status", "Established" );
					Table.SetCell( "Host", pSock->GetRemoteIP() );
					Table.SetCell( "Port", CString( pSock->GetRemotePort() ) );
					SSL_SESSION *pSession = pSock->GetSSLSession();
					if ( ( pSession ) && ( pSession->cipher ) && ( pSession->cipher->name ) )
						Table.SetCell( "Cipher", pSession->cipher->name );

				} else
				{
					Table.SetCell( "Status", "Waiting" );
					Table.SetCell( "Port", CString( pSock->GetLocalPort() ) );
				}
			}
			if ( Table.size() )
			{
				unsigned int uTableIdx = 0;
				CString sLine;
				while ( Table.GetLine( uTableIdx++, sLine ) )
					PutModule( sLine );
			} else
				PutModule( "No SDCCs currently in session" );

		} else if ( sCom.CaseCmp( "close" ) == 0 )
		{
			CString sName = "SCHAT::" + m_pUser->GetUserName();
			for( u_int a = 0; a < m_pManager->size(); a++ )
			{
				if ( (*m_pManager)[a]->GetSockName() != sName )
					continue;

				CSChatSock *pSock = (CSChatSock *)(*m_pManager)[a];
				if ( sArgs.CaseCmp( "(s)", 3 ) != 0 )
					sArgs = "(s)" + sArgs;

				if ( sArgs.CaseCmp(pSock->GetChatNick()) == 0 )
				{
					pSock->Close();
					return;
				}
			}
			PutModule( "No Such Chat [" + sArgs + "]" );
		} else if ( sCom.CaseCmp( "showsocks" ) == 0 )
		{
			CTable Table;
			Table.AddColumn( "SockName" );
			Table.AddColumn( "Created" );
			Table.AddColumn( "LocalIP:Port" );
			Table.AddColumn( "RemoteIP:Port" );
			Table.AddColumn( "Type" );
			Table.AddColumn( "Cipher" );
			for( u_int a = 0; a < m_pManager->size(); a++ )
			{
				Table.AddRow();
				Csock *pSock = (*m_pManager)[a];
				Table.SetCell( "SockName", pSock->GetSockName() );
				unsigned long long iStartTime = pSock->GetStartTime();
				time_t iTime = iStartTime / 1000;
				char *pTime = ctime( &iTime );
				if ( pTime )
				{
					CString sTime = pTime;
					sTime.Trim();
					Table.SetCell( "Created", sTime );
				}

				if ( pSock->GetType() != Csock::LISTENER )
				{
					if ( pSock->GetType() == Csock::OUTBOUND )
						Table.SetCell( "Type", "Outbound" );
					else
						Table.SetCell( "Type", "Inbound" );
					Table.SetCell( "LocalIP:Port", pSock->GetLocalIP() + ":" +
							CString( pSock->GetLocalPort() ) );
					Table.SetCell( "RemoteIP:Port", pSock->GetRemoteIP() + ":" +
							CString( pSock->GetRemotePort() ) );
					SSL_SESSION *pSession = pSock->GetSSLSession();
					if ( ( pSession ) && ( pSession->cipher ) && ( pSession->cipher->name ) )
						Table.SetCell( "Cipher", pSession->cipher->name );
					else
						Table.SetCell( "Cipher", "None" );

				} else
				{
					Table.SetCell( "Type", "Listener" );
					Table.SetCell( "LocalIP:Port", pSock->GetLocalIP() +
							":" + CString( pSock->GetLocalPort() ) );
					Table.SetCell( "RemoteIP:Port", "0.0.0.0:0" );
				}
			}
			if ( Table.size() )
			{
				unsigned int uTableIdx = 0;
				CString sLine;
				while ( Table.GetLine( uTableIdx++, sLine ) )
					PutModule( sLine );
			} else
				PutModule( "Error Finding Sockets" );

		} else if ( sCom.CaseCmp( "help" ) == 0 )
		{
			PutModule( "Commands are: " );
			PutModule( "    help           - This text." );
			PutModule( "    chat <nick>    - Chat a nick." );
			PutModule( "    list           - List current chats." );
			PutModule( "    close <nick>   - Close a chat to a nick." );
			PutModule( "    timers         - Shows related timers." );
			PutModule( "    showsocks      - Shows all socket connections." );
		} else if ( sCom.CaseCmp( "timers" ) == 0 )
			ListTimers();
		else
			PutModule( "Unknown command [" + sCom + "] [" + sArgs + "]" );
	}

	virtual EModRet OnPrivCTCP( CNick& Nick, CString& sMessage )
	{
		if ( sMessage.CaseCmp("DCC SCHAT ", 10 ) == 0 )
		{
			// chat ip port
			unsigned long iIP = sMessage.Token( 3 ).ToULong();
			unsigned short iPort = sMessage.Token( 4 ).ToUShort();

			if ( ( iIP > 0 ) && ( iPort > 0 ) )
			{
				pair<u_long, u_short> pTmp;
				CString sMask;

				pTmp.first = iIP;
				pTmp.second = iPort;
				sMask = "(s)" + Nick.GetNick() + "!" + "(s)" +
					Nick.GetNick() + "@" + CUtils::GetIP(iIP);

				m_siiWaitingChats["(s)" + Nick.GetNick()] = pTmp;
				SendToUser(sMask, "*** Incoming DCC SCHAT, Accept ? (yes/no)");
				CRemMarkerJob *p = new CRemMarkerJob( this, 60, 1,
						"Remove (s)" + Nick.GetNick(),
						"Removes this nicks entry for waiting DCC." );
				p->SetNick( "(s)" + Nick.GetNick() );
				AddTimer( p );
				return( HALT );
			}
		}

		return( CONTINUE );
	}

	void AcceptSDCC( const CString & sNick, u_long iIP, u_short iPort )
	{
		CSChatSock *p = new CSChatSock( CUtils::GetIP( iIP ), iPort, 60 );
		p->SetModule( this );
		p->SetChatNick( sNick );
		CString sSockName = "SCHAT::" + m_pUser->GetUserName() +  "::" + sNick;
		m_pManager->Connect( CUtils::GetIP( iIP ), iPort, sSockName, 60, true, m_pUser->GetLocalIP(), p );
		RemTimer( "Remove " + sNick ); // delete any associated timer to this nick
	}
	virtual EModRet OnUserMsg( CString& sTarget, CString& sMessage )
	{
		if ( sTarget.Left(3) == "(s)")
		{
			CString sSockName = "SCHAT::" + m_pUser->GetUserName() + "::" + sTarget;
			CSChatSock *p = (CSChatSock *)m_pManager->FindSockByName( sSockName );
			if ( !p )
			{
				map< CString,pair< u_long,u_short > >::iterator it;
				it = m_siiWaitingChats.find( sTarget );

				if ( it != m_siiWaitingChats.end() )
				{
					if ( sMessage.CaseCmp( "yes" ) != 0 )
						SendToUser( sTarget + "!" + sTarget + "@" +
								CUtils::GetIP( it->second.first ),
								"Refusing to accept DCC SCHAT!" );
					else
						AcceptSDCC( sTarget, it->second.first, it->second.second );

					m_siiWaitingChats.erase( it );
					return( HALT );
				}
				PutModule( "No such SCHAT to [" + sTarget + "]" );
			} else
				p->Write( sMessage + "\n" );

			return( HALT );
		}
		return( CONTINUE );
	}

	virtual void RemoveMarker( const CString & sNick )
	{
		map< CString,pair< u_long,u_short > >::iterator it = m_siiWaitingChats.find( sNick );
		if ( it != m_siiWaitingChats.end() )
			m_siiWaitingChats.erase( it );
	}

	void SendToUser( const CString & sFrom, const CString & sText )
	{
		//:*schat!znc@znc.com PRIVMSG Jim :
		CString sSend = ":" + sFrom + " PRIVMSG " + m_pUser->GetCurNick() + " :" + sText;
		PutUser( sSend );
	}

	bool IsAttached()
	{
		return( m_pUser->IsUserAttached() );
	}

private:
	map< CString,pair< u_long,u_short > >		m_siiWaitingChats;
	CString		m_sPemFile;
};


//////////////////// methods ////////////////

void CSChatSock::ReadLine( const CS_STRING & sLine )
{
	if ( m_pModule )
	{
		CString sText = sLine;
		if ( sText[sText.length()-1] == '\n' )
			sText.erase( sText.length()-1, 1 );

		if ( sText[sText.length()-1] == '\r' )
			sText.erase( sText.length()-1, 1 );

		if ( m_pModule->IsAttached() )
			m_pModule->SendToUser( m_sChatNick + "!" + m_sChatNick + "@" + GetRemoteIP(), sText );
		else
			AddLine( sText );
	}
}

void CSChatSock::Disconnected()
{
	if ( m_pModule )
		m_pModule->SendToUser( m_sChatNick + "!" + m_sChatNick + "@" + GetRemoteIP(),
				"*** Disconnected." );
}

void CSChatSock::Connected()
{
	SetTimeout( 0 );
	if ( m_pModule )
		m_pModule->SendToUser( m_sChatNick + "!" + m_sChatNick + "@" + GetRemoteIP(), "*** Connected." );
}

void CSChatSock::Timeout()
{
	if ( m_pModule )
	{
		if ( GetType() == LISTENER )
			m_pModule->PutModule( "Timeout while waiting for [" + m_sChatNick + "]" );
		else
			m_pModule->SendToUser( m_sChatNick + "!" + m_sChatNick + "@" + GetRemoteIP(),
					"*** Connection Timed out." );
	}
}

void CRemMarkerJob::RunJob()
{
	CSChat *p = (CSChat *)m_pModule;
	p->RemoveMarker( m_sNick );

	// store buffer
}
MODULEDEFS(CSChat, "Secure cross platform (:P) chat system")

