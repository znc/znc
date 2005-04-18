#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>
#include <map>
#include <vector>

#ifndef HAVE_LIBSSL
#error This plugin only works with OpenSSL
#endif /* HAVE_LIBSSL */

#define CRYPT_VERIFICATION_TOKEN "::__:AWAY:__::"

/*
 * Quiet Away and message logger
 * Author: imaginos <imaginos@imaginos.net>
 *
 * 
 * $Log$
 * Revision 1.5  2005/04/18 00:18:46  prozacx
 * Upgraded output msgs and changed path of file
 *
 * Revision 1.4  2005/04/02 22:22:24  imaginos
 * ability to change pass
 *
 * Revision 1.3  2005/04/01 08:55:41  imaginos
 * keep things in synch
 *
 * Revision 1.2  2005/04/01 08:49:46  imaginos
 * woops actually delete the message
 *
 * Revision 1.1  2005/04/01 08:30:47  imaginos
 * simple away script
 *
 *
 */

class CAway;

class CAwayJob : public CTimer 
{
public:
	CAwayJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const string& sLabel, const string& sDescription ) 
		: CTimer( pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CAwayJob() {}

protected:
	virtual void RunJob();
};

class CAway : public CModule 
{
public:
	MODCONSTRUCTOR(CAway)
	{
		Ping();	
		m_bIsAway = false;
		AddTimer( new CAwayJob( this, 60, 0, "AwayJob", "Checks for idle and saves messages every 1 minute" ) );
	}
	virtual ~CAway() 
	{
		SaveBufferToDisk();
	}

	virtual bool OnBoot()
	{
		if ( m_sPassword.empty() )
		{
			char *pTmp = CUtils::GetPass( "Enter Encryption Key for away.so: " );

			if ( pTmp )
				m_sPassword = CBlowfish::MD5( pTmp );

			*pTmp = 0;
		}
		
		if ( !BootStrap() )
			return( false );

		return( true );
	}

	virtual void OnIRCConnected()
	{
		if( m_bIsAway )
			Away( true ); // reset away if we are reconnected
		else
			Back();	// ircd seems to remember your away if you killed the client and came back
	}

	bool BootStrap()
	{
		string sFile;
		if ( DecryptMessages( sFile ) )
		{
			string sLine;
			u_int iPos = 0;
			while( ReadLine( sFile, sLine, iPos ) )
			{
				CUtils::Trim( sLine );
				AddMessage( sLine );
			}
		} else
		{
			CUtils::PrintError("Failed to Decrypt Messages");
			return( false );
		}

		return( true );
	}
	
	void SaveBufferToDisk()
	{
		if ( !m_sPassword.empty() )
		{
			string sFile = CRYPT_VERIFICATION_TOKEN;
			
			for( u_int b = 0; b < m_vMessages.size(); b++ )
				sFile += m_vMessages[b] + "\n";

			CBlowfish c( m_sPassword, BF_ENCRYPT );
			sFile = c.Crypt( sFile );
			string sPath = GetPath();
			if ( !sPath.empty() )
			{
				WriteFile( sPath, sFile );
				chmod( sPath.c_str(), 0600 );
			}
		}
	}

	virtual void OnUserAttached()
	{
		Back( true );
	}
	virtual void OnUserDetached()
	{
		Away();
	}

	virtual string GetDescription() 
	{
		return ( "Stores messages while away, also auto away" );
	}
	
	virtual void OnModCommand( const string& sCommand )
	{
		string sCmdName = CUtils::Token(sCommand, 0);
		if ( sCmdName == "away" )
		{
			Away();
			PutModNotice( "You have been marked as away", "away" );
		}	
		else if ( sCmdName == "back" )
		{
			if ( m_vMessages.empty() )
				PutModNotice( "Welcome Back!", "away" );
			Back();
		}
		else if ( sCmdName == "messages" )
		{
			for( u_int a = 0; a < m_vMessages.size(); a++ )
				PutModule( m_vMessages[a], "away" );
		} 
		else if ( sCmdName == "delete" )
		{
			string sWhich = CUtils::Token(sCommand, 1);
			if ( sWhich == "all" )
			{
				PutModNotice( "Deleted " + CUtils::ToString( m_vMessages.size() ) + " Messages.", "away" );
				for( u_int a = 0; a < m_vMessages.size(); a++ )
					m_vMessages.erase( m_vMessages.begin() + a-- );

			} 
			else if ( sWhich.empty() )
			{
				PutModNotice( "USAGE: delete <num|all>", "away" );
				return;
			} else
			{
				u_int iNum = atoi( sWhich.c_str() );
				if ( iNum >= m_vMessages.size() )
				{
					PutModNotice( "Illegal Message # Requested", "away" );
					return;
				}
				else
				{
					m_vMessages.erase( m_vMessages.begin() + iNum );
					PutModNotice( "Message Erased.", "away" );
				}
				SaveBufferToDisk();
			}
		}
		else if ( sCmdName == "save" )
		{
			SaveBufferToDisk();
			PutModNotice( "Messages saved to disk.", "away" );
		}
		else if ( sCmdName == "ping" )
		{
			Ping();
			if ( m_bIsAway )
				Back();
		}
		else if ( sCmdName == "pass" )
		{
			m_sPassword = CUtils::Token( sCommand, 1 );
			PutModNotice( "Password Updated to [" + m_sPassword + "]" );
		}
		else if ( sCmdName == "show" )
		{
			map< string, vector< string> > msvOutput;
			for( u_int a = 0; a < m_vMessages.size(); a++ )
			{
				string sTime = CUtils::Token( m_vMessages[a], 0, false, ':' );
				string sWhom = CUtils::Token( m_vMessages[a], 1, false, ':' );
				string sMessage = CUtils::Token( m_vMessages[a], 2, true, ':' );
				
				if ( ( sTime.empty() ) || ( sWhom.empty() ) || ( sMessage.empty() ) )
				{
					// illegal format
					PutModule( "Corrupt message! [" + m_vMessages[a] + "]", "away" );
					m_vMessages.erase( m_vMessages.begin() + a-- );
					continue;
				}
				time_t iTime = strtol( sTime.c_str(), NULL, 10 );
				char szFormat[64];
				struct tm t;
				localtime_r( &iTime, &t );
				size_t iCount = strftime( szFormat, 64, "%F %T", &t );
				if ( iCount <= 0 )
				{
					PutModule( "Corrupt time stamp! [" + m_vMessages[a] + "]", "away" );
					m_vMessages.erase( m_vMessages.begin() + a-- );
					continue;
				}
				string sTmp = "    " + CUtils::ToString( a ) + ") [";
				sTmp.append( szFormat, iCount );
				sTmp += "] ";
				sTmp += sMessage;
				msvOutput[sWhom].push_back( sTmp );
			}
			for( map< string, vector< string> >::iterator it = msvOutput.begin(); it != msvOutput.end(); it++ )
			{
				PutModule( it->first, "away" );
				for( u_int a = 0; a < it->second.size(); a++ )
					PutModule( it->second[a] );
			}
			PutModule( "#--- End Messages", "away" );
					
		} else
		{
			PutModule( "Commands: away, back, delete <num|all>, ping, show, save", "away" );
		}
	}

	string GetPath()
	{
		string sBuffer = m_pUser->GetUserName();
		string sRet = m_pUser->GetDataPath() + "/away";
		CUtils::MakeDir(sRet);
		sRet += "/.znc-away-" + CBlowfish::MD5( sBuffer, true );
		return( sRet );
	}

	virtual void Away( bool bForce = false, const string & sReason = "" )
	{
		if ( ( !m_bIsAway ) || ( bForce ) )
		{
			if ( !bForce )
				m_sReason = sReason;
			else if ( !sReason.empty() )
				m_sReason = sReason;

			time_t iTime = time( NULL );
			char *pTime = ctime( &iTime );
			string sTime;
			if ( pTime )
			{
				sTime = pTime;
				CUtils::Trim( sTime );
			}
			if ( m_sReason.empty() )
				m_sReason = "away :Auto Away at " + sTime;
			PutIRC( m_sReason );
			m_bIsAway = true;
		}
	}

	virtual void Back( bool bUsePrivMessage = false )
	{
		PutIRC( "away" );
		m_bIsAway = false;
		if ( !m_vMessages.empty() )
		{
			if ( bUsePrivMessage )
			{
				PutModule( "Welcome Back!", "away" );
				PutModule( "You have " + CUtils::ToString( m_vMessages.size() ) + " messages!", "away" );
			}
			else
			{
				PutModNotice( "Welcome Back!", "away" );
				PutModNotice( "You have " + CUtils::ToString( m_vMessages.size() ) + " messages!", "away" );
			}
		}
		m_sReason = "";
	}

	virtual bool OnPrivMsg(const CNick& Nick, string& sMessage)
	{
		if ( m_bIsAway )
			AddMessage( time( NULL ), Nick, sMessage );
		return( false );	
	}
	
	virtual bool OnUserNotice(const string& sTarget, string& sMessage)
	{
		Ping();
		if( m_bIsAway )
			Back();
		
		return( false );	
	}
	virtual bool OnUserMsg(const string& sTarget, string& sMessage)
	{
		Ping();
		if( m_bIsAway )
			Back();
		
		return( false );	
	}

	time_t GetTimeStamp() const { return( m_iLastSentData ); }
	void Ping() { m_iLastSentData = time( NULL ); }

	bool IsAway() { return( m_bIsAway ); }

private:
	string	m_sPassword;
	bool DecryptMessages( string & sBuffer )
	{
		string sMessages = GetPath();
		string sFile;
		sBuffer = "";
	
		if ( ( sMessages.empty() ) || ( !ReadFile( sMessages, sFile ) ) )
		{
			 PutModule( "Unable to find buffer" );
			 return( true ); // gonna be successful here
		}

		if ( !sFile.empty() )
		{
			CBlowfish c( m_sPassword, BF_DECRYPT );
			sBuffer = c.Crypt( sFile );

			if ( sBuffer.substr( 0, strlen( CRYPT_VERIFICATION_TOKEN ) ) != CRYPT_VERIFICATION_TOKEN )
			{
				// failed to decode :(
				PutModule( "Unable to decode Encrypted messages" );
				return( false );
			}
			sBuffer.erase( 0, strlen( CRYPT_VERIFICATION_TOKEN ) );
		}
		return( true );
	}

	void AddMessage( time_t iTime, const CNick & Nick, string & sMessage )
	{
		AddMessage( CUtils::ToString( iTime ) + ":" + Nick.GetNickMask() + ":" + sMessage );
	}

	void AddMessage( const string & sText )
	{
		m_vMessages.push_back( sText );
	}

	time_t			m_iLastSentData;
	bool			m_bIsAway;
	vector<string>	m_vMessages;
	string			m_sReason;
};


void CAwayJob::RunJob()
{
	CAway *p = (CAway *)m_pModule;
	p->SaveBufferToDisk();
	
	if ( !p->IsAway() )
	{
		time_t iNow = time( NULL );

		if ( ( iNow - p->GetTimeStamp() ) > 300 )
			p->Away();
	}
}

MODULEDEFS(CAway)

