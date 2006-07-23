#define REQUIRESSL

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>
#include <map>
#include <vector>

#define CRYPT_VERIFICATION_TOKEN "::__:AWAY:__::"

/*
 * Quiet Away and message logger
 * Author: imaginos <imaginos@imaginos.net>
 *
 * 
 * $Log$
 * Revision 1.23  2006/07/23 04:01:44  imaginos
 * add back functionality to give an away reason
 *
 * Revision 1.22  2006/03/28 16:48:23  imaginos
 * added a quiet flag
 *
 * Revision 1.21  2006/02/25 09:43:35  prozacx
 * Migrated away from CString::ToString() in favor of explicit constructors
 *
 * Revision 1.20  2006/02/11 11:55:55  imaginos
 * fixed wrong type being used on 64bit
 *
 * Revision 1.19  2005/09/26 23:09:05  prozacx
 * Removed const from args in a bunch of hooks
 *
 * Revision 1.18  2005/09/26 08:23:30  prozacx
 * Removed const from CNick in priv/chan hooks
 *
 * Revision 1.17  2005/09/06 22:43:02  prozacx
 * Added REQUIRESSL
 *
 * Revision 1.16  2005/06/30 21:51:06  prozacx
 * Changed CString::Token() to split on a string rather than char
 *
 * Revision 1.15  2005/06/12 09:04:39  prozacx
 * Changed to new GetSavePath()
 *
 * Revision 1.14  2005/05/26 20:42:13  prozacx
 * Moved GetDescription() into second argument of MODULEDEFS()
 *
 * Revision 1.13  2005/05/15 08:27:27  prozacx
 * Changed return value from bool to EModRet on most hooks
 *
 * Revision 1.12  2005/05/08 06:42:01  prozacx
 * Moved CUtils::ToString() into CString class
 *
 * Revision 1.11  2005/05/08 04:30:13  prozacx
 * Moved CUtils::Trim() into CString class
 *
 * Revision 1.10  2005/05/07 09:18:49  prozacx
 * Moved CUtils::Token() into CString class
 *
 * Revision 1.9  2005/05/05 18:11:03  prozacx
 * Changed all references to std::string over to CString
 *
 * Revision 1.8  2005/04/18 22:32:24  imaginos
 * move password reset into BootStrap
 *
 * Revision 1.7  2005/04/18 05:41:43  prozacx
 * Added OnLoad() and print modname in error msg
 *
 * Revision 1.6  2005/04/18 04:44:40  imaginos
 * fixed bug where attempting to set a bad pass trashes existing buffer
 *
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
	CAwayJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription ) 
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
		m_bBootError = false;
		AddTimer( new CAwayJob( this, 60, 0, "AwayJob", "Checks for idle and saves messages every 1 minute" ) );
	}
	virtual ~CAway() 
	{
		if ( !m_bBootError )
			SaveBufferToDisk();
	}

	virtual bool OnLoad(const CString& sArgs)
	{
		if (!sArgs.empty())
		{
			m_sPassword = CBlowfish::MD5( sArgs );
		}

		return true;
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
		{
			m_bBootError = true;
			return( false );
		}

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
		CString sFile;
		if ( DecryptMessages( sFile ) )
		{
			CString sLine;
			CString::size_type iPos = 0;
			while( ReadLine( sFile, sLine, iPos ) )
			{
				sLine.Trim();
				AddMessage( sLine );
			}
		} else
		{
			m_sPassword = "";
			CUtils::PrintError("[" + GetModName() + ".so] Failed to Decrypt Messages");
			return( false );
		}

		return( true );
	}
	
	void SaveBufferToDisk()
	{
		if ( !m_sPassword.empty() )
		{
			CString sFile = CRYPT_VERIFICATION_TOKEN;
			
			for( u_int b = 0; b < m_vMessages.size(); b++ )
				sFile += m_vMessages[b] + "\n";

			CBlowfish c( m_sPassword, BF_ENCRYPT );
			sFile = c.Crypt( sFile );
			CString sPath = GetPath();
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

	virtual void OnModCommand( const CString& sCommand )
	{
		CString sCmdName = sCommand.Token(0);
		if ( sCmdName == "away" )
		{
			CString sReason;
			if( sCommand.Token( 1 ) != "-quiet" ) 
			{
				sReason = sCommand.Token( 1, true );
				PutModNotice( "You have been marked as away", "away" );
			}
			else
				sReason = sCommand.Token( 2, true );
			Away( true, sReason );
		}	
		else if ( sCmdName == "back" )
		{
			if ( ( m_vMessages.empty() ) && ( sCommand.Token( 1 ) != "-quiet" ) )
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
			CString sWhich = sCommand.Token(1);
			if ( sWhich == "all" )
			{
				PutModNotice( "Deleted " + CString( m_vMessages.size() ) + " Messages.", "away" );
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
			m_sPassword = sCommand.Token( 1 );
			PutModNotice( "Password Updated to [" + m_sPassword + "]" );
		}
		else if ( sCmdName == "show" )
		{
			map< CString, vector< CString> > msvOutput;
			for( u_int a = 0; a < m_vMessages.size(); a++ )
			{
				CString sTime = m_vMessages[a].Token( 0, false, ":" );
				CString sWhom = m_vMessages[a].Token( 1, false, ":" );
				CString sMessage = m_vMessages[a].Token( 2, true, ":" );
				
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
				CString sTmp = "    " + CString( a ) + ") [";
				sTmp.append( szFormat, iCount );
				sTmp += "] ";
				sTmp += sMessage;
				msvOutput[sWhom].push_back( sTmp );
			}
			for( map< CString, vector< CString> >::iterator it = msvOutput.begin(); it != msvOutput.end(); it++ )
			{
				PutModule( it->first, "away" );
				for( u_int a = 0; a < it->second.size(); a++ )
					PutModule( it->second[a] );
			}
			PutModule( "#--- End Messages", "away" );
					
		} else
		{
			PutModule( "Commands: away [-quiet], back [-quiet], delete <num|all>, ping, show, save", "away" );
		}
	}

	CString GetPath()
	{
		CString sBuffer = m_pUser->GetUserName();
		CString sRet = GetSavePath();
		sRet += "/.znc-away-" + CBlowfish::MD5( sBuffer, true );
		return( sRet );
	}

	virtual void Away( bool bForce = false, const CString & sReason = "" )
	{
		if ( ( !m_bIsAway ) || ( bForce ) )
		{
			if ( !bForce )
				m_sReason = sReason;
			else if ( !sReason.empty() )
				m_sReason = sReason;

			time_t iTime = time( NULL );
			char *pTime = ctime( &iTime );
			CString sTime;
			if ( pTime )
			{
				sTime = pTime;
				sTime.Trim();
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
				PutModule( "You have " + CString( m_vMessages.size() ) + " messages!", "away" );
			}
			else
			{
				PutModNotice( "Welcome Back!", "away" );
				PutModNotice( "You have " + CString( m_vMessages.size() ) + " messages!", "away" );
			}
		}
		m_sReason = "";
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage)
	{
		if ( m_bIsAway )
			AddMessage( time( NULL ), Nick, sMessage );
		return( CONTINUE );	
	}
	
	virtual EModRet OnUserNotice(const CString& sTarget, CString& sMessage)
	{
		Ping();
		if( m_bIsAway )
			Back();
		
		return( CONTINUE );	
	}
	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage)
	{
		Ping();
		if( m_bIsAway )
			Back();
		
		return( CONTINUE );	
	}

	time_t GetTimeStamp() const { return( m_iLastSentData ); }
	void Ping() { m_iLastSentData = time( NULL ); }

	bool IsAway() { return( m_bIsAway ); }

private:
	CString	m_sPassword;
	bool	m_bBootError;
	bool DecryptMessages( CString & sBuffer )
	{
		CString sMessages = GetPath();
		CString sFile;
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

	void AddMessage( time_t iTime, const CNick & Nick, CString & sMessage )
	{
		AddMessage( CString( iTime ) + ":" + Nick.GetNickMask() + ":" + sMessage );
	}

	void AddMessage( const CString & sText )
	{
		m_vMessages.push_back( sText );
	}

	time_t			m_iLastSentData;
	bool			m_bIsAway;
	vector<CString>	m_vMessages;
	CString			m_sReason;
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

MODULEDEFS(CAway, "Stores messages while away, also auto away")

