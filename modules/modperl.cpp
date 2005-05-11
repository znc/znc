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
	
	void AddZNCHook( const string & sHookName )
	{
		m_mssHookNames.insert( sHookName );
	}


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

	int CallBack( const char *pszHookName, ... )
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
};

MODULEDEFS( CModPerl )



//////////////////////////////// PERL GUTS //////////////////////////////

XS(XS_AddZNCHook)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak(aTHX_ "Usage: AddZNCHook( HookName )");

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
			g_ModPerl->AddZNCHook( (char *)SvPV(ST(0),PL_na) );

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

	ModPerlInit();
	return( true );
}

