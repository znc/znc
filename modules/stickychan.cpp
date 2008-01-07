/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"

class CStickyChan : public CModule 
{
public:
	MODCONSTRUCTOR(CStickyChan) {}
	virtual ~CStickyChan() 
	{
	}

	virtual bool OnLoad( const CString& sArgs, CString& sMessage );

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
		else if ( ( sCmdName == "list" ) && ( sChannel.empty() ) )
		{
			int i = 1;
			for( MCString::iterator it = BeginNV(); it != EndNV(); it++, i++ )
			{
				if (it->second.empty())
					PutModule(CString( i ) + ": " + it->first);
				else
					PutModule(CString( i ) + ": " + it->first + " (" + it->second + ")");
			}
			PutModule(" -- End of List");
		}
		else
		{
			PutModule( "USAGE: [un]stick #channel [key], list" );
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

bool CStickyChan::OnLoad(const CString& sArgs, CString& sMessage)
{
	AddTimer( RunTimer, "StickyChanTimer", 15 );
	return( true );
}

MODULEDEFS(CStickyChan, "configless sticky chans, keeps you there very stickily even")
