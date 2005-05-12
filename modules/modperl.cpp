#ifdef HAVE_PERL
#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

// perl stuff
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

class CModPerl;
static CModPerl *g_ModPerl = NULL;

class CModPerlTimer : public CTimer 
{
public:
	CModPerlTimer( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription ) 
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CModPerlTimer() {}

protected:
	virtual void RunJob();

};

class CModPerl : public CModule 
{
public:
	MODCONSTRUCTOR( CModPerl ) 
	{
		g_ModPerl = this;
		m_pPerl = perl_alloc();
		perl_construct( m_pPerl );
		AddZNCHook( "Init" );
		AddZNCHook( "Shutdown" );
	}

	virtual ~CModPerl() 
	{
		ModPerlDestroy();
		perl_destruct( m_pPerl );
		perl_free( m_pPerl );
		g_ModPerl = NULL;
	}

	virtual bool OnLoad( const CString & sArgs );

	virtual bool OnChanMsg( const CNick& Nick, const CChan & Channel, CString & sMessage ) 
	{
		int iRet = CallBack( "OnChanMsg", Nick.GetNickMask().c_str(), Channel.GetName().c_str(), sMessage.c_str(), NULL );
		if ( iRet > 0 )
			return( true );

		return( false );
	}
	
	void AddZNCHook( const string & sHookName ) { m_mssHookNames.insert( sHookName ); }
	void DelZNCHook( const string & sHookName )
	{
		set< string >::iterator it = m_mssHookNames.find( sHookName );
		if ( it != m_mssHookNames.end() )
			m_mssHookNames.erase( it );
	}

	int CallBack( const char *pszHookName, ... );

private:

	PerlInterpreter	*m_pPerl;
	set< string >	m_mssHookNames;

	void ModPerlInit() 
	{
		CallBack( "Init", NULL );
	}

	void ModPerlDestroy() 
	{
		CallBack( "Shutdown", NULL );
	}

};

MODULEDEFS( CModPerl )



//////////////////////////////// PERL GUTS //////////////////////////////

XS(XS_AddZNCHook)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: AddZNCHook( sFuncName )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
			g_ModPerl->AddZNCHook( (char *)SvPV(ST(0),PL_na) );

		PUTBACK;
		return;
	}
}

XS(XS_AddZNCTimer)
{
	dXSARGS;
	if ( items != 4 )
		Perl_croak( aTHX_ "Usage: AddZNCTimer( sFuncName, iInterval, iCycles, sDesc )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			string sLabel = (char *)SvPV(ST(0),PL_na);
			u_int iInterval = (u_int)SvIV(ST(1));
			u_int iCycles = (u_int)SvIV(ST(2));
			string sDesc = (char *)SvPV(ST(3),PL_na);

			CModPerlTimer *pTimer = new CModPerlTimer( g_ModPerl, iInterval, iCycles, sLabel, sDesc ); 
			g_ModPerl->AddZNCHook( sLabel );
			g_ModPerl->AddTimer( pTimer );
		}
		PUTBACK;
		return;
	}
}

XS(XS_KillZNCTimer)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: KillZNCTimer( sLabel )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			string sLabel = (char *)SvPV(ST(0),PL_na);

			CTimer *pTimer = g_ModPerl->FindTimer( sLabel );
			if ( pTimer )
				g_ModPerl->UnlinkTimer( pTimer );
		}
		PUTBACK;
		return;
	}
}


/////////// supporting functions from within module
bool CModPerl::OnLoad( const CString & sArgs ) 
{
	const char *pArgv[] = 
	{
		sArgs.c_str(),
		sArgs.c_str(),
		NULL
	};

	perl_parse( m_pPerl, NULL, 2, (char **)pArgv, (char **)NULL );
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

	newXS( "AddZNCHook", XS_AddZNCHook, (char *)__FILE__ );
	newXS( "AddZNCTimer", XS_AddZNCTimer, (char *)__FILE__ );
	newXS( "KillZNCTimer", XS_KillZNCTimer, (char *)__FILE__ );

	ModPerlInit();
	return( true );
}

int CModPerl::CallBack( const char *pszHookName, ... )
{
	set< string >::iterator it = m_mssHookNames.find( pszHookName );

	if ( it == m_mssHookNames.end() )
		return( -1 );
	
	va_list ap;
	va_start( ap, pszHookName );

	char *pTmp;

	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );
	while( ( pTmp = va_arg( ap, char * ) ) )
	{
		XPUSHs( sv_2mortal( newSVpv( pTmp, strlen( pTmp ) ) ) );
	}
	PUTBACK;

	int iCount = call_pv( it->c_str(), G_SCALAR );

	SPAGAIN;
	int iRet = 0;

	if ( iCount == 1 )
		iRet = POPi;	

	PUTBACK;
	FREETMPS;
	LEAVE;

	va_end( ap );
	return( iRet );
}

void CModPerlTimer::RunJob() 
{
	((CModPerl *)m_pModule)->CallBack( GetName().c_str(), NULL );
}

#endif /* HAVE_PERL */
