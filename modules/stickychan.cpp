#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include "FileUtils.h"
#include <pwd.h>
#include <map>
#include <vector>


class CStickyChan : public CModule 
{
public:
	MODCONSTRUCTOR(CStickyChan) {}
	virtual ~CStickyChan() 
	{
	}

	virtual bool OnLoad( const CString& sArgs );

	virtual EModRet OnUserPart( CString& sChannel, CString& sMessage )
	{
		for ( MCString::iterator it = BeginNV(); it != EndNV(); it++ )
		{
			if ( sChannel.CaseCmp( it->first ) == 0 )
			{
				CChan* pChan = m_pUser->FindChan( sChannel );

				if ( pChan )
				{
					pChan->JoinUser( true, "", m_pClient );
					return HALT;
				}
			}
		}

		return CONTINUE;
	}

	virtual void OnModCommand( const CString& sCommand )
	{
		CString sCmdName = sCommand.Token(0);
		CString sChannel = sCommand.Token(1);
		sChannel.MakeLower();
		if ( ( sCmdName == "stick" ) && ( !sChannel.empty() ) )
		{
			SetNV( sChannel, sCommand.Token(2) );
			PutModule( "Stuck " + sChannel );
		}
		else if ( ( sCmdName == "unstick" ) && ( !sChannel.empty() ) )
		{
			MCString::iterator it = FindNV( sChannel );
			if ( it != EndNV() )
				DelNV( it );

			PutModule( "UnStuck " + sChannel );
		}	
		else
		{
			PutModule( "USAGE: [un]stick #channel [key]" );
		}
	}

	virtual void RunJob()
	{
		for( MCString::iterator it = BeginNV(); it != EndNV(); it++ )
		{
			if ( !m_pUser->FindChan( it->first ) )
			{
				CChan *pChan = new CChan( it->first, m_pUser, true );
				if ( !it->second.empty() )
					pChan->SetKey( it->second );
				m_pUser->AddChan( pChan );
				PutModule( "Joining [" + it->first + "]" );
				PutIRC( "JOIN " + it->first + ( it->second.empty() ? "" : it->second ) );
			}
		}
	}
private:

};


static void RunTimer( CModule * pModule, CFPTimer *pTimer )
{
	((CStickyChan *)pModule)->RunJob();
}

bool CStickyChan::OnLoad(const CString& sArgs)
{
	AddTimer( RunTimer, "StickyChanTimer", 15 );
	return( true );
}

MODULEDEFS(CStickyChan, "configless sticky chans, keeps you there very stickily even")
