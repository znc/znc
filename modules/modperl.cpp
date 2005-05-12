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

#define NICK( a ) a.GetNickMask().c_str()
#define CHAN( a ) a.GetName().c_str()
#define NUM( a ) CString::ToString( a ).c_str()
#define BOOL( a ) ( a ? "1" : "0" )
#define CBNONE( a ) CallBack( a, NULL )
#define CBSINGLE( a, b ) CallBack( a, b, NULL )
#define CBDOUBLE( a, b, c ) CallBack( a, b, c, NULL )

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
	virtual bool OnBoot() { return( CBNONE( "OnLoad" ) ); }
	virtual void OnUserAttached() {  CBNONE( "OnUserAttached" ); }
	virtual void OnUserDetached() {  CBNONE( "OnUserDetached" ); }
	virtual void OnIRCDisconnected() {  CBNONE( "OnIRCDisconnected" ); }
	virtual void OnIRCConnected() {  CBNONE( "OnIRCConnected" ); }

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, 
		const CString& sFile, unsigned long uFileSize)
	{
		return( CallBack( "OnDCCUserSend", NICK( RemoteNick ), NUM( uLongIP ), NUM( uPort ), sFile.c_str(), NUM( uFileSize ), NULL ) );
	}

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CallBack( "OnOp", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), BOOL( bNoChange ), NULL );
	}
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CallBack( "OnDeop", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), BOOL( bNoChange ), NULL );
	}
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CallBack( "OnVoice", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), BOOL( bNoChange ), NULL );
	}
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CallBack( "OnDevoice", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), BOOL( bNoChange ), NULL );
	}
	virtual void OnRawMode(const CNick& Nick, const CChan& Channel, const CString& sModes, const CString& sArgs)
	{
		CallBack( "OnRawMode", NICK( Nick ), CHAN( Channel ), sModes.c_str(), sArgs.c_str(), NULL );
	}
	virtual bool OnUserRaw(CString& sLine) { return( CBSINGLE( "OnUserRaw", sLine.c_str() ) ); }
	virtual bool OnRaw(CString& sLine) { return( CBSINGLE( "OnRaw", sLine.c_str() ) ); }
	virtual bool OnStatusCommand(const CString& sCommand) { return( CBSINGLE( "OnStatusCommand", sCommand.c_str() ) ); }
	virtual void OnModCommand(const CString& sCommand) { CBSINGLE( "OnModCommand", sCommand.c_str() ); }
	virtual void OnModNotice(const CString& sMessage) { CBSINGLE( "OnModNotice", sMessage.c_str() ); }
	virtual void OnModCTCP(const CString& sMessage) { CBSINGLE( "OnModCTCP", sMessage.c_str() ); }

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		vector< CString > vsArgs;
		vsArgs.push_back( Nick.GetNickMask() );
		vsArgs.push_back( sMessage );
		for( vector<CChan*>::size_type a = 0; a < vChans.size(); a++ )
			vsArgs.push_back( vChans[a]->GetName() );

		CallBackVec( "OnQuit", vsArgs );
	}

	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		vector< CString > vsArgs;
		vsArgs.push_back( Nick.GetNickMask() );
		vsArgs.push_back( sNewNick );
		for( vector<CChan*>::size_type a = 0; a < vChans.size(); a++ )
			vsArgs.push_back( vChans[a]->GetName() );

		CallBackVec( "OnNick", vsArgs );
	}

	virtual void OnKick(const CNick& Nick, const CString& sOpNick, const CChan& Channel, const CString& sMessage)
	{
		CallBack( "OnKick", NICK( Nick ), sOpNick.c_str(), CHAN( Channel ), sMessage.c_str(), NULL );
	}

	virtual void OnJoin(const CNick& Nick, const CChan& Channel) { CBDOUBLE( "OnJoin", NICK( Nick ), CHAN( Channel ) ); }
	virtual void OnPart(const CNick& Nick, const CChan& Channel) { CBDOUBLE( "OnPart", NICK( Nick ), CHAN( Channel ) ); }

	virtual bool OnUserCTCPReply(const CNick& Nick, CString& sMessage) 
	{ 
		return CBDOUBLE( "OnUserCTCPReply", NICK( Nick ), sMessage.c_str() ); 
	}
	virtual bool OnCTCPReply(const CNick& Nick, CString& sMessage)
	{
		return CBDOUBLE( "OnCTCPReply", NICK( Nick ), sMessage.c_str() ); 
	}
	virtual bool OnUserCTCP(const CString& sTarget, CString& sMessage)
	{
		return CBDOUBLE( "OnUserCTCP", sTarget.c_str(), sMessage.c_str() ); 
	}
	virtual bool OnPrivCTCP(const CNick& Nick, CString& sMessage)
	{
		return CBDOUBLE( "OnPrivCTCP", NICK( Nick ), sMessage.c_str() ); 
	}
	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return CallBack( NICK( Nick ), CHAN( Channel ), sMessage.c_str(), NULL );
	}
	virtual bool OnUserMsg(const CString& sTarget, CString& sMessage)
	{
		return CBDOUBLE( "OnUserMsg", sTarget.c_str(), sMessage.c_str() );
	}
	virtual bool OnPrivMsg(const CNick& Nick, CString& sMessage)
	{
		return CBDOUBLE( "OnPrivMsg", NICK( Nick ), sMessage.c_str() );
	}

	virtual bool OnChanMsg( const CNick& Nick, const CChan & Channel, CString & sMessage )
	{
		return( CallBack( "OnChanMsg", NICK( Nick ), CHAN( Channel ), sMessage.c_str(), NULL ) );
	}
	virtual bool OnUserNotice(const CString& sTarget, CString& sMessage)
	{
		return CBDOUBLE( "OnUserNotice", sTarget.c_str(), sMessage.c_str() );
	}
	virtual bool OnPrivNotice(const CNick& Nick, CString& sMessage)
	{
		return CBDOUBLE( "OnPrivNotice", NICK( Nick ), sMessage.c_str() );
	}
	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return( CallBack( "OnChanNotice", NICK( Nick ), CHAN( Channel ), sMessage.c_str(), NULL ) );
	}

	void AddZNCHook( const CString & sHookName ) { m_mssHookNames.insert( sHookName ); }
	void DelZNCHook( const CString & sHookName )
	{
		set< CString >::iterator it = m_mssHookNames.find( sHookName );
		if ( it != m_mssHookNames.end() )
			m_mssHookNames.erase( it );
	}

	int CallBack( const char *pszHookName, ... );
	int CallBackVec( const CString & sHookName, const vector< CString > & vsArgs );

private:

	PerlInterpreter	*m_pPerl;
	set< CString >	m_mssHookNames;

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
			CString sLabel = (char *)SvPV(ST(0),PL_na);
			u_int iInterval = (u_int)SvIV(ST(1));
			u_int iCycles = (u_int)SvIV(ST(2));
			CString sDesc = (char *)SvPV(ST(3),PL_na);

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
			CString sLabel = (char *)SvPV(ST(0),PL_na);

			CTimer *pTimer = g_ModPerl->FindTimer( sLabel );
			if ( pTimer )
				g_ModPerl->UnlinkTimer( pTimer );
		}
		PUTBACK;
		return;
	}
}


/////////// supporting functions from within module

int CModPerl::CallBack( const char *pszHookName, ... )
{
	set< CString >::iterator it = m_mssHookNames.find( pszHookName );

	if ( it == m_mssHookNames.end() )
		return( 1 );
	
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
	int iRet = 1;

	if ( iCount == 1 )
		iRet = POPi;	

	PUTBACK;
	FREETMPS;
	LEAVE;

	va_end( ap );
	return( iRet );
}

int CModPerl::CallBackVec( const CString & sHookName, const vector< CString > & vsArgs )
{
	set< CString >::iterator it = m_mssHookNames.find( sHookName );

	if ( it == m_mssHookNames.end() )
		return( 1 );
	
	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );
	for( vector< CString >::size_type a = 0; a < vsArgs.size(); a++ )
	{
		XPUSHs( sv_2mortal( newSVpv( vsArgs[a].c_str(), vsArgs[a].length() ) ) );
	}
	PUTBACK;

	int iCount = call_pv( it->c_str(), G_SCALAR );

	SPAGAIN;
	int iRet = 1;

	if ( iCount == 1 )
		iRet = POPi;	

	PUTBACK;
	FREETMPS;
	LEAVE;

	return( iRet );
}

////////////////////// Events ///////////////////
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

/////////////// CModPerlTimer //////////////
void CModPerlTimer::RunJob() 
{
	((CModPerl *)m_pModule)->CallBack( GetName().c_str(), NULL );
}

#endif /* HAVE_PERL */
