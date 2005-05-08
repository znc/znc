#include "main.h"
#include "znc.h"
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
 * Revision 1.21  2005/05/08 06:42:02  prozacx
 * Moved CUtils::ToString() into CString class
 *
 * Revision 1.20  2005/05/08 04:30:14  prozacx
 * Moved CUtils::Trim() into CString class
 *
 * Revision 1.19  2005/05/05 18:11:04  prozacx
 * Changed all references to std::string over to CString
 *
 * Revision 1.18  2005/04/24 08:05:41  prozacx
 * Fixed literal CStrings of 'savebuff' to now use GetModName() and removed redundant prefix from filenames
 *
 * Revision 1.17  2005/04/23 18:24:38  imaginos
 * only work on chans where keepbuffer is true
 *
 * Revision 1.16  2005/04/23 08:10:34  prozacx
 * Changed vChans to a reference in OnNick and OnQuit (oops)
 *
 * Revision 1.15  2005/04/23 07:24:58  prozacx
 * Changed OnNick() and OnQuit() to take a vector<CChan*> of common channels
 *
 * Revision 1.14  2005/04/23 06:44:19  prozacx
 * Changed buffer playback messages to mirror engine behavior
 *
 * Revision 1.13  2005/04/22 03:35:48  imaginos
 * start logging certain events
 *
 * Revision 1.12  2005/04/19 20:54:38  imaginos
 * cut&paste madness
 *
 * Revision 1.11  2005/04/19 20:50:24  imaginos
 * only fill the buffer if its empty
 *
 * Revision 1.10  2005/04/18 22:56:42  prozacx
 * Call OnBoot() in OnLoad() again since closing stdout bug is now fixed
 *
 * Revision 1.9  2005/04/18 22:32:24  imaginos
 * move password reset into BootStrap
 *
 * Revision 1.8  2005/04/18 17:26:23  imaginos
 * ditch warning message
 *
 * Revision 1.7  2005/04/18 05:39:19  prozacx
 * Don't call OnBoot() in OnLoad() and print modname in error msg
 *
 * Revision 1.6  2005/04/18 04:44:40  imaginos
 * fixed bug where attempting to set a bad pass trashes existing buffer
 *
 * Revision 1.5  2005/04/17 23:58:26  prozacx
 * Added OnLoad() for setting pass from config
 *
 * Revision 1.4  2005/04/17 23:46:06  prozacx
 * Upgraded output print msgs to new schema
 *
 * Revision 1.3  2005/04/12 07:33:45  prozacx
 * Changed path to DataPath
 *
 * Revision 1.2  2005/04/04 06:35:19  imaginos
 * fixed int32's that test against npos to CString::size_type
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
	CSaveBuffJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription ) 
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
		m_bBootError = false;
		// m_sPassword = CBlowfish::MD5( "" );
		AddTimer( new CSaveBuffJob( this, 60, 0, "SaveBuff", "Saves the current buffer to disk every 1 minute" ) );
	}
	virtual ~CSaveBuff() 
	{
		if ( !m_bBootError )
		{
			SaveBufferToDisk();
		}
	}

	virtual bool OnLoad(const CString& sArgs)
	{
		if (!sArgs.empty())
		{
			m_sPassword = CBlowfish::MD5( sArgs );
			return( OnBoot() );
		}

		return true;
	}

	virtual bool OnBoot()
	{
		if ( m_sPassword.empty() )
		{
			char *pTmp = CUtils::GetPass( "Enter Encryption Key for " + GetModName() + ".so" );

			if ( pTmp )
				m_sPassword = CBlowfish::MD5( pTmp );

			*pTmp = 0;
		}
		
		const vector<CChan *>& vChans = m_pUser->GetChans();
		for( u_int a = 0; a < vChans.size(); a++ )
		{
			if ( !vChans[a]->KeepBuffer() )
				continue;

			if ( !BootStrap( vChans[a] ) )
			{
				m_bBootError = true;
				return( false );
			}
		}

		return true;
	}

	bool BootStrap( CChan *pChan )
	{
		CString sFile;
		if ( DecryptChannel( pChan->GetName(), sFile ) )
		{
			if ( !pChan->GetBuffer().empty() )
				return( true ); // reloaded a module probably in this case, so just verify we can decrypt the file

			CString sLine;
			u_int iPos = 0;
			while( ReadLine( sFile, sLine, iPos ) )
			{
				sLine.Trim();
				pChan->AddBuffer( sLine );
			}
		} else
		{
			m_sPassword = "";
			CUtils::PrintError("[" + GetModName() + ".so] Failed to Decrypt [" + pChan->GetName() + "]");
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
				if ( !vChans[a]->KeepBuffer() )
					continue;

				const vector<CString> & vBuffer = vChans[a]->GetBuffer();

				if ( vBuffer.empty() )
				{
					if ( !m_sPassword.empty() )
						BootStrap( vChans[a] );
					
					continue;
				}

				CString sFile = CRYPT_VERIFICATION_TOKEN;
			
				for( u_int b = 0; b < vBuffer.size(); b++ )
						sFile += vBuffer[b] + "\n";

				CBlowfish c( m_sPassword, BF_ENCRYPT );
				sFile = c.Crypt( sFile );
				CString sPath = GetPath( vChans[a]->GetName() );
				if ( !sPath.empty() )
				{
					WriteFile( sPath, sFile );
					chmod( sPath.c_str(), 0600 );
				}
			}
		}
	}

	virtual CString GetDescription() 
	{
		return ( "Stores channel buffers to disk, encrypted." );
	}
	
	virtual void OnModCommand( const CString& sCommand )
	{
		CString::size_type iPos = sCommand.find( " " );
		CString sCom, sArgs;
		if ( iPos == CString::npos )
			sCom = sCommand;
		else
		{
			sCom = sCommand.substr( 0, iPos );
			sArgs = sCommand.substr( iPos + 1, CString::npos );
		}

		if ( strcasecmp( sCom.c_str(), "setpass" ) == 0 )
		{
			PutModule( "Password set to [" + sArgs + "]" );
			m_sPassword = CBlowfish::MD5( sArgs );
		
		} else if ( strcasecmp( sCom.c_str(), "dumpbuff" ) == 0 )
		{
			CString sFile;
			if ( DecryptChannel( sArgs, sFile ) )
			{
				CString sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					sLine.Trim();
					PutModule( "[" + sLine + "]" );
				}
			}
			PutModule( "//!-- EOF " + sArgs );
		} else if ( strcasecmp( sCom.c_str(), "replay" ) == 0 )
		{
			CString sFile;
			PutUser( ":***!znc@znc.com PRIVMSG " + sArgs + " :Buffer Playback..." );
			if ( DecryptChannel( sArgs, sFile ) )
			{
				CString sLine;
				u_int iPos = 0;
				while( ReadLine( sFile, sLine, iPos ) )
				{
					sLine.Trim();
					PutUser( sLine );
				}
			}
			PutModule( "Replayed " + sArgs );
			PutUser( ":***!znc@znc.com PRIVMSG " + sArgs + " :Playback Complete." );

		} else if ( strcasecmp( sCom.c_str(), "save" ) == 0 )
		{
			SaveBufferToDisk();
			PutModule( "Done." );
		} else
			PutModule( "Unknown command [" + sCommand + "]" );
	}

	CString GetPath( const CString & sChannel )
	{
		CString sBuffer = m_pUser->GetUserName() + Lower( sChannel );
		CString sRet = m_pUser->GetDataPath() + "/" + GetModName();
		CUtils::MakeDir(sRet);
		sRet += "/" + CBlowfish::MD5( sBuffer, true );
		return( sRet );
	}

	CString SpoofChanMsg( const CString & sChannel, const CString & sMesg )
	{
		CString sReturn = ":*" + GetModName() + "!znc@znc.com PRIVMSG " + sChannel + " :" + CString::ToString( time( NULL ) ) + " " + sMesg;
		return( sReturn );
	}

	virtual void OnRawMode(const CNick& cOpNick, const CChan& cChannel, const CString& sModes, const CString& sArgs)
	{
		if ( !cChannel.KeepBuffer() )
			return;
		
		((CChan &)cChannel).AddBuffer( SpoofChanMsg( cChannel.GetName(), cOpNick.GetNickMask() + " MODE " + sModes + " " + sArgs ) );
	}
	virtual void OnQuit(const CNick& cNick, const CString& sMessage, const vector<CChan*>& vChans)
	{ 
		for( u_int a = 0; a < vChans.size(); a++ )
		{
			if ( !vChans[a]->KeepBuffer() )
				continue;
			vChans[a]->AddBuffer( SpoofChanMsg( vChans[a]->GetName(), cNick.GetNickMask() + " QUIT " + sMessage ) ); 
		}
	}

	virtual void OnNick(const CNick& cNick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		for( u_int a = 0; a < vChans.size(); a++ )
		{
			if ( !vChans[a]->KeepBuffer() )
				continue;
			vChans[a]->AddBuffer( SpoofChanMsg( vChans[a]->GetName(), cNick.GetNickMask() + " NICK " + sNewNick ) ); 
		}
	}
	virtual void OnKick(const CNick& cNick, const CString& sOpNick, const CChan& cChannel, const CString& sMessage)
	{
		if ( !cChannel.KeepBuffer() )
			return;
		((CChan &)cChannel).AddBuffer( SpoofChanMsg( cChannel.GetName(), sOpNick + " KICK " + cNick.GetNickMask() + " " + sMessage ) );
	}
	virtual void OnJoin(const CNick& cNick, const CChan& cChannel)
	{
		if ( !cChannel.KeepBuffer() )
			return;
		((CChan &)cChannel).AddBuffer( SpoofChanMsg( cChannel.GetName(), cNick.GetNickMask() + " JOIN" ) );
	}
	virtual void OnPart(const CNick& cNick, const CChan& cChannel)
	{
		if ( !cChannel.KeepBuffer() )
			return;
		((CChan &)cChannel).AddBuffer( SpoofChanMsg( cChannel.GetName(), cNick.GetNickMask() + " PART" ) );
	}

private:
	bool	m_bBootError;
	CString	m_sPassword;
	bool DecryptChannel( const CString & sChan, CString & sBuffer )
	{
		CString sChannel = GetPath( sChan );
		CString sFile;
		sBuffer = "";
	
		if ( ( sChannel.empty() ) || ( !ReadFile( sChannel, sFile ) ) )
			 return( true ); // gonna be successful here

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

