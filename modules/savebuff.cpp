#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>

/* TODO list
 * store timestamp to be displayed
 * store OnJoin, OnQuit, OnPart, etc send down as messages
 */

#ifndef HAVE_LIBSSL
#error This plugin only works with OpenSSL
#endif /* HAVE_LIBSSL */

#define CRYPT_VERIFICATION_TOKEN "::__:SAVEBUFF:__::"

/*
 * Buffer Saving thing, incase your shit goes out while your out
 * Author: imaginos <imaginos@imaginos.net>
 *
 * Its only as secure as your shell, the encryption only offers a slightly 
 * better solution then plain text.
 * 
 * $Log$
 * Revision 1.2  2005/04/04 06:35:19  imaginos
 * fixed int32's that test against npos to string::size_type
 *
 * Revision 1.1  2005/03/30 19:36:20  imaginos
 * rename files
 *
 * Revision 1.3  2005/01/28 04:37:47  imaginos
 * force requirements on main class to so savebuff can be sure to save all data needed. added todo list
 *
 * Revision 1.2  2004/11/07 02:53:32  imaginos
 * added replay to savebuff so one can 'replay' a channel
 *
 * Revision 1.1.1.1  2004/08/24 00:08:52  prozacx
 *
 *
 *
 */

class CSaveBuff;

class CSaveBuffJob : public CTimer 
{
public:
	CSaveBuffJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const string& sLabel, const string& sDescription ) 
		: CTimer( pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CSaveBuffJob() {}

protected:
	virtual void RunJob();
};

class CSaveBuff : public CModule 
{
public:
	MODCONSTRUCTOR(CSaveBuff)
	{
		// m_sPassword = CBlowfish::MD5( "" );
		AddTimer( new CSaveBuffJob( this, 60, 0, "SaveBuff", "Saves the current buffer to disk every 1 minute" ) );
	}
	virtual ~CSaveBuff() 
	{
		SaveBufferToDisk();
	}

	virtual bool OnBoot()
	{
		if ( m_sPassword.empty() )
		{
			char *pTmp = getpass( "Enter Encryption Key for savebuff.so: " );

			if ( pTmp )
				m_sPassword = CBlowfish::MD5( pTmp );

			*pTmp = 0;
		}
		
		const vector<CChan *>& vChans = m_pUser->GetChans();
		for( u_int a = 0; a < vChans.size(); a++ )
		{
			if ( !BootStrap( vChans[a] ) )
				return( false );
		}

		return true;
	}

	bool BootStrap( CChan *pChan )
	{
		string sFile;
		if ( DecryptChannel( pChan->GetName(), sFile ) )
		{
			string sLine;
			u_int iPos = 0;
			while( ReadLine( sFile, sLine, iPos ) )
			{
				CUtils::Trim( sLine );
				pChan->AddBuffer( sLine );
			}
		} else
		{
			cerr << "Failed to Decrypt [" << pChan->GetName() << "]" << endl;
			return( false );
		}

		return( true );
	}
	
	void SaveBufferToDisk()
	{
		if ( !m_sPassword.empty() )
		{
			const vector<CChan *>& vChans = m_pUser->GetChans();
			for( u_int a = 0; a < vChans.size(); a++ )
			{
				const vector<string> & vBuffer = vChans[a]->GetBuffer();
				vChans[a]->SetKeepBuffer( true );
				vChans[a]->SetBufferCount( 500 );

				if ( vBuffer.empty() )
				{
					if ( !m_sPassword.empty() )
						BootStrap( vChans[a] );
					
					continue;
				}

				string sFile = CRYPT_VERIFICATION_TOKEN;
			
				for( u_int b = 0; b < vBuffer.size(); b++ )
						sFile += vBuffer[b] + "\n";

				CBlowfish c( m_sPassword, BF_ENCRYPT );
				sFile = c.Crypt( sFile );
				string sPath = GetPath( vChans[a]->GetName() );
				if ( !sPath.empty() )
				{
					WriteFile( sPath, sFile );
					chmod( sPath.c_str(), 0600 );
				}
			}
		}
	}

	virtual string GetDescription() 
	{
		return ( "Stores channel buffers to disk, encrypted." );
	}
	
	virtual void OnModCommand( const string& sCommand )
	{
		string::size_type iPos = sCommand.find( " " );
		string sCom, sArgs;
		if ( iPos == string::npos )
			sCom = sCommand;
		else
		{
			sCom = sCommand.substr( 0, iPos );
			sArgs = sCommand.substr( iPos + 1, string::npos );
		}

		if ( strcasecmp( sCom.c_str(), "setpass" ) == 0 )
		{
			PutModule( "Password set to [" + sArgs + "]" );
			m_sPassword = CBlowfish::MD5( sArgs );
		
		} else if ( strcasecmp( sCom.c_str(), "dumpbuff" ) == 0 )
		{
			string sFile;
			if ( DecryptChannel( sArgs, sFile ) )
			{
				string sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					CUtils::Trim( sLine );
					PutModule( "[" + sLine + "]" );
				}
			}
			PutModule( "//!-- EOF " + sArgs );
		} else if ( strcasecmp( sCom.c_str(), "replay" ) == 0 )
		{
			string sFile;
			PutUser( ":***!znc@znc.com PRIVMSG " + sArgs + " :Replaying ..." );
			if ( DecryptChannel( sArgs, sFile ) )
			{
				string sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					CUtils::Trim( sLine );
					PutUser( sLine );
				}
			}
			PutModule( "Replayed " + sArgs );
			PutUser( ":***!znc@znc.com PRIVMSG " + sArgs + " :Done!" );

		} else if ( strcasecmp( sCom.c_str(), "save" ) == 0 )
		{
			SaveBufferToDisk();
			PutModule( "Done." );
		} else
			PutModule( "Unknown command [" + sCommand + "]" );
	}

	string GetPath( const string & sChannel )
	{
		string sBuffer = m_pUser->GetUserName() + Lower( sChannel );
		string sRet = m_pUser->GetHomePath();
		sRet += "/.znc-savebuff-" + CBlowfish::MD5( sBuffer, true );
		return( sRet );
	}

private:
	string	m_sPassword;
	bool DecryptChannel( const string & sChan, string & sBuffer )
	{
		string sChannel = GetPath( sChan );
		string sFile;
		sBuffer = "";
	
		if ( ( sChannel.empty() ) || ( !ReadFile( sChannel, sFile ) ) )
		{
			 PutModule( "Unable to find buffer for that channel" );
			 return( true ); // gonna be successful here
		}

		if ( !sFile.empty() )
		{
			CBlowfish c( m_sPassword, BF_DECRYPT );
			sBuffer = c.Crypt( sFile );

			if ( sBuffer.substr( 0, strlen( CRYPT_VERIFICATION_TOKEN ) ) != CRYPT_VERIFICATION_TOKEN )
			{
				// failed to decode :(
				PutModule( "Unable to decode Encrypted file [" + sChannel + "]" );
				return( false );
			}
			sBuffer.erase( 0, strlen( CRYPT_VERIFICATION_TOKEN ) );
		}
		return( true );
	}
};


void CSaveBuffJob::RunJob()
{
	CSaveBuff *p = (CSaveBuff *)m_pModule;
	p->SaveBufferToDisk();
}

MODULEDEFS(CSaveBuff)

