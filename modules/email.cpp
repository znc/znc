#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include "FileUtils.h"
#include "md5.h"
#include <pwd.h>
#include <sstream>


/*
 * Email Monitor / Retrieval
 * Author: imaginos <imaginos@imaginos.net>
 *
 * $Log$
 * Revision 1.4  2005/05/05 18:11:04  prozacx
 * Changed all references to std::string over to CString
 *
 * Revision 1.3  2005/05/02 22:34:52  prozacx
 * Get CFile from FileUtils.h now
 *
 * Revision 1.2  2005/04/04 06:35:19  imaginos
 * fixed int32's that test against npos to CString::size_type
 *
 * Revision 1.1  2005/03/30 18:46:35  imaginos
 * moving to standard makefile system, and cpp only extension
 *
 * Revision 1.1.1.1  2004/08/24 00:08:52  prozacx
 *
 *
 *
 */

struct EmailST
{
	CString	sFrom;
	CString	sSubject;
	CString	sUidl;
	u_int	iSize;
};

class CEmailJob : public CTimer 
{
public:
	CEmailJob( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription ) 
		: CTimer( pModule, uInterval, uCycles, sLabel, sDescription) {}

	virtual ~CEmailJob() {}

protected:
	virtual void RunJob();
};

class CEmail : public CModule 
{
public:
	MODCONSTRUCTOR(CEmail)
	{
		m_iLastCheck = 0;
		m_bInitialized = false;
	}
	virtual ~CEmail() 
	{
		vector<Csock*> vSocks = m_pManager->FindSocksByName( "EMAIL::" + m_pUser->GetUserName() );	
		for( u_int a = 0; a < vSocks.size(); a++ )
			m_pManager->DelSockByAddr( vSocks[a] );
	}

	virtual bool OnLoad(const CString & sArgs) {
		m_sMailPath = sArgs;

		StartParser();
		if ( m_pUser->IsUserAttached() )
			StartTimer();

		return true;
	}

	virtual void OnUserAttached()
	{
		stringstream s;
		s << "You have " << m_ssUidls.size() << " emails.";
		PutModule( s.str() );
		StartTimer();
	}
	virtual void OnUserDetached()
	{
		RemTimer( "EMAIL::" + m_pUser->GetUserName()  );
	}

	void StartTimer()
	{
		if ( !FindTimer( "EMAIL::" + m_pUser->GetUserName() ) )
		{
			CEmailJob *p = new CEmailJob( this, 60, 0, "EmailMonitor", "Monitors email activity" );
			AddTimer( p );
		}
	}

	virtual CString GetDescription() 
	{
		return ( "Monitors Email activity on local disk /var/mail/user" );
	}
	
	virtual void OnModCommand( const CString& sCommand );
	void StartParser();

	void ParseEmails( const vector<EmailST> & vEmails )
	{
		if ( !m_bInitialized )
		{
			m_bInitialized = true;
			for( u_int a = 0; a < vEmails.size(); a++ )
				m_ssUidls.insert( vEmails[a].sUidl );

			stringstream s;
			s << "You have " << vEmails.size() << " emails.";
			PutModule( s.str() );
		} else
		{
			set<CString> ssUidls;

			CTable Table;
			Table.AddColumn("From");
			Table.AddColumn("Size");
			Table.AddColumn("Subject");

			for( u_int a = 0; a < vEmails.size(); a++ )
			{
				if ( m_ssUidls.find( vEmails[a].sUidl ) == m_ssUidls.end() )
				{
					//PutModule( "------------------- New Email -------------------" );
					Table.AddRow();
					Table.SetCell( "From", CUtils::Ellipsize( vEmails[a].sFrom, 32 ) );
					Table.SetCell( "Size", CUtils::ToString( vEmails[a].iSize ) );
					Table.SetCell( "Subject", CUtils::Ellipsize( vEmails[a].sSubject, 64 ) );
				}
				ssUidls.insert( vEmails[a].sUidl );
			}

			m_ssUidls = ssUidls;	// keep the list in synch
		
			if (Table.size()) {
				unsigned int uTableIdx = 0;
				CString sLine;
				while ( Table.GetLine( uTableIdx++, sLine ) ) {
					PutModule( sLine );
				}

				stringstream s;
				s << "You have " << vEmails.size() << " emails.";
				PutModule( s.str() );
			}
		}
	}

	
private:
	CString			m_sMailPath;
	u_int			m_iLastCheck;
	set<CString>		m_ssUidls;
	bool			m_bInitialized;
};

class CEmailFolder : public Csock
{
public:
	CEmailFolder( CEmail *pModule, const CString & sMailbox ) : Csock()
	{
		m_pModule = pModule;
		m_sMailbox = sMailbox;
		EnableReadLine();
	}	

	virtual ~CEmailFolder()
	{
		if ( !m_sMailBuffer.empty() )
			ProcessMail();	// get the last one

		if ( !m_vEmails.empty() )
			m_pModule->ParseEmails( m_vEmails );
	}
	
	virtual void ReadLine( const CS_STRING & sLine )
	{
		if ( sLine.substr( 0, 5 ) == "From " )
		{
			if ( !m_sMailBuffer.empty() )
			{
				ProcessMail();
				m_sMailBuffer.clear();
			}
		}
		m_sMailBuffer += sLine;
	}

	void ProcessMail()
	{
		EmailST tmp;
		tmp.sUidl = (char *)CMD5( m_sMailBuffer.substr( 0, 255 ) );
		CString sLine;
		u_int iPos = 0;
		while( ::ReadLine( m_sMailBuffer, sLine, iPos ) )
		{
			CUtils::Trim( sLine );
			if ( sLine.empty() )
				break;	// out of the headers

			if ( strncasecmp( sLine.c_str(), "From: ", 6 ) == 0 )
				tmp.sFrom = sLine.substr( 6, CString::npos );
			else if ( strncasecmp( sLine.c_str(), "Subject: ", 9 ) == 0 )
				tmp.sSubject = sLine.substr( 9, CString::npos );

			if ( ( !tmp.sFrom.empty() ) && ( !tmp.sSubject.empty() ) )
				break;
		}
		tmp.iSize = m_sMailBuffer.length();
		m_vEmails.push_back( tmp );
	}
private:
	CEmail				*m_pModule;
	CString				m_sMailbox;
	CString				m_sMailBuffer;
	vector<EmailST>		m_vEmails;	
};

void CEmail::OnModCommand( const CString& sCommand )
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
	
	if ( sCom == "timers" )
	{
		ListTimers();
	} else
		PutModule( "Error, no such command [" + sCom + "]" );
}

void CEmail::StartParser()
{
	CString sParserName = "EMAIL::" + m_pUser->GetUserName();

	if ( m_pManager->FindSockByName( sParserName ) )
		return;	// one at a time sucker

	CFile cFile( m_sMailPath );
	if ( ( !cFile.Exists() ) || ( cFile.GetSize() == 0 ) )
	{
		m_bInitialized = true;
		return;	// der
	}

	if ( cFile.GetMTime() <= m_iLastCheck )
		return;	// only check if modified

	int iFD = open( m_sMailPath.c_str(), O_RDONLY );
	if ( iFD >= 0 )
	{
		m_iLastCheck = time( NULL );
		CEmailFolder *p = new CEmailFolder( this, m_sMailPath );
		p->SetRSock( iFD );
		p->SetWSock( iFD );
		m_pManager->AddSock( (Csock *)p, "EMAIL::" + m_pUser->GetUserName() );
	}
}

void CEmailJob::RunJob()
{
	CEmail *p = (CEmail *)m_pModule;
	p->StartParser();
}
MODULEDEFS(CEmail)

