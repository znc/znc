#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>

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
 * Revision 1.1  2004/08/24 00:08:52  prozacx
 * Initial revision
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
			string sFile;
			string sPath = GetPath( vChans[a]->GetName() );

			if ( ( sPath.empty() ) || ( !ReadFile( sPath, sFile ) ) )
				continue;
			if ( !sFile.empty() )
			{
				CBlowfish c( m_sPassword, BF_DECRYPT );
				sFile = c.Crypt( sFile );

				if ( sFile.substr( 0, strlen( CRYPT_VERIFICATION_TOKEN ) ) != CRYPT_VERIFICATION_TOKEN )
				{
					// failed to decode :(
					cerr << "Unable to decode Encrypted file [" << sPath << "]" << endl;
					continue;
				}
				sFile.erase( 0, strlen( CRYPT_VERIFICATION_TOKEN ) );

				string sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					CUtils::Trim( sLine );
					vChans[a]->AddBuffer( sLine );
				}
			}
		}

		return true;
	}
	void SaveBufferToDisk()
	{
		if ( !m_sPassword.empty() )
		{
			const vector<CChan *>& vChans = m_pUser->GetChans();
			for( u_int a = 0; a < vChans.size(); a++ )
			{
				const vector<string> & vBuffer = vChans[a]->GetBuffer();
				if ( vBuffer.empty() )
					continue;

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
		u_int iPos = sCommand.find( " " );
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
			string sChannel = GetPath( sArgs );
			string sFile;
		
			if ( ( sChannel.empty() ) || ( !ReadFile( sChannel, sFile ) ) )
			{
				 PutModule( "Unable to find buffer for that channel" );
				 return;
			}

			if ( !sFile.empty() )
			{
				CBlowfish c( m_sPassword, BF_DECRYPT );
				sFile = c.Crypt( sFile );

				if ( sFile.substr( 0, strlen( CRYPT_VERIFICATION_TOKEN ) ) != CRYPT_VERIFICATION_TOKEN )
				{
					// failed to decode :(
					PutModule( "Unable to decode Encrypted file [" + sChannel + "]" );
					return;
				}
				sFile.erase( 0, strlen( CRYPT_VERIFICATION_TOKEN ) );

				string sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					CUtils::Trim( sLine );
					PutModule( "[" + sLine + "]" );
				}
			}
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
	string			m_sPassword;
};


void CSaveBuffJob::RunJob()
{
	CSaveBuff *p = (CSaveBuff *)m_pModule;
	p->SaveBufferToDisk();
}

MODULEDEFS(CSaveBuff)

